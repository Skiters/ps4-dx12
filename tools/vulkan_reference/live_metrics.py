"""External frame sources. Reported integration rates are never guest FPS."""
import collections, csv, ctypes as C, json, pathlib, re, subprocess, threading, time
from ctypes import wintypes as W

class MemoryStatus(C.Structure):
    _fields_=[('length',W.DWORD),('load',W.DWORD)]+[(k,C.c_ulonglong) for k in
        ['total_physical','available_physical','total_pagefile','available_pagefile','total_virtual','available_virtual','available_extended']]
def system_memory():
    m=MemoryStatus();m.length=C.sizeof(m)
    if not C.windll.kernel32.GlobalMemoryStatusEx(C.byref(m)):return {'error':'GlobalMemoryStatusEx failed'}
    return {k:getattr(m,k) for k,_ in m._fields_[1:]}

def metric(value=None,source='unavailable',kind='unknown',updated=None,age=None,confidence='unavailable',loss=None):
    return dict(value=value,source=source,kind=kind,last_utc=updated,age_s=age,
                confidence=confidence,loss=loss,status='NOT MEASURED' if value is None else ('STALE' if age is not None and age>12 else 'MEASURED'))

class LiveMetrics:
    coverage_definition=['process_cpu','user_cpu','kernel_cpu','thread_cpu','thread_names','working_set','private_bytes',
        'process_io','page_faults','system_available_ram','memory_pressure','gpu_engines','process_dedicated_vram',
        'process_shared_gpu_memory','process_gpu_commit','adapter_telemetry','collector_cpu','sampling_jitter',
        'guest_frame_ids','host_present_rate','display_rate','generated_frame_ids','cpu_stacks','gpu_stage_timestamps',
        'pm4_counts','buffer_transfer_bytes','texture_detile_time','frame_latency']
    def coverage(self,r):
        yes=set(self.coverage_definition[:11]+['collector_cpu','sampling_jitter'])
        if r.get('cpu_core_equivalents') is None:yes.discard('process_cpu')
        if not any(t.get('cpu_core_equivalents') is not None for t in r.get('threads',[])):yes.discard('thread_cpu')
        if not any(t.get('name') for t in r.get('threads',[])):yes.discard('thread_names')
        if 'available_physical' not in r.get('system_memory',{}):yes.discard('system_available_ram');yes.discard('memory_pressure')
        for key,name in [('engines','gpu_engines'),('dedicated','process_dedicated_vram'),('shared','process_shared_gpu_memory'),('committed','process_gpu_commit')]:
            items=r.get('gpu',{}).get(key,[])
            if isinstance(items,list) and any(x.get('status') in (0,1) for x in items):yes.add(name)
        if 'nvidia_adapter_csv' in r:self.adapter_tick=r['qpc_s']
        if r['qpc_s']-getattr(self,'adapter_tick',-1000)<15:yes.add('adapter_telemetry')
        for key,name in [('host_fps','host_present_rate'),('display_fps','display_rate')]:
            if r['frames'][key]['status']=='MEASURED':yes.add(name)
        return {'available':len(yes),'total':len(self.coverage_definition),'measured':sorted(yes),'unavailable':sorted(set(self.coverage_definition)-yes)}
    @staticmethod
    def selected_gpu(gpu,adapters):
        # Join OS process instance LUID to DXGI vendor identity, not adapter index.
        nvidia={str(a.get('luid_pdh','')).lower().removeprefix('luid_') for a in adapters if a.get('vendor_id')==0x10de}
        out={}
        for category,items in gpu.items():
            if not isinstance(items,list):continue
            out[category]=[]
            for i in items:
                m=re.search(r'luid_(0x[0-9a-f]+_0x[0-9a-f]+)',i['instance'],re.I)
                if m and m[1].lower() in nvidia and i['status'] in (0,1):out[category].append(i)
        out['identity']='PDH process LUID joined to DXGI NVIDIA vendor';return out
    def __init__(self,root,pid,profile,presentmon):
        self.root=root;self.pid=pid;self.lock=threading.Lock();self.events=collections.deque(maxlen=8192)
        self.child=None;self.reader=None;self.failure=None;self.total=0;self.retention_drops=0
        self.integration=None;self.fg='requested; active unverified';self.partial='';self.cooldowns={}
        self.previous_host_rate=None
        self.log_counts=collections.Counter();self.log_last={}
        ini=profile/'dlss.ini'
        if ini.exists():
            vals=dict(re.findall(r'^([\w]+)=(.*)$',ini.read_text(encoding='utf-8'),re.M))
            self.fg='x'+vals.get('fg_multiplier','?')+' requested ('+vals.get('fg_backend','unknown').strip()+')'
        self.sources={'guest_fps':'unavailable: no guest frame identity',
            'integration':'existing FRAME-GEN log: emulator EndPresent and Streamline reported shown, rounded 5-window updates; not guest simulation',
            'presentmon':str(presentmon) if presentmon else 'not configured','fg_requested':self.fg}
        if presentmon and pathlib.Path(presentmon).is_file():
            args=[presentmon,'--process_id',str(pid),'--session_name','VulkanResearch-'+root.name,
                  '--output_stdout','--no_console_stats','--v2_metrics','--terminate_on_proc_exit','--no_track_input']
            (root/'presentmon-command.json').write_text(json.dumps(args,indent=2))
            err=(root/'presentmon.stderr.txt').open('w')
            try:
                self.child=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=err,text=True,creationflags=0x08000000)
                self.reader=threading.Thread(target=self._read,daemon=True);self.reader.start()
            except Exception as e:self.failure=repr(e)
            finally:err.close()
        else:self.failure='PresentMon executable not configured/available'
    def _read(self):
        try:
            with (self.root/'presentmon.csv').open('w',encoding='utf-8',newline='') as f:
                header=None
                for line in self.child.stdout:
                    if f.tell()>128*2**20:
                        self.failure='PresentMon 128MiB storage cap';self.child.terminate();break
                    f.write(line);f.flush()
                    vals=next(csv.reader([line]))
                    if 'ProcessID' in vals and 'SwapChainAddress' in vals:header=vals;continue
                    if not header or len(vals)!=len(header):continue
                    row=dict(zip(header,vals))
                    if row.get('ProcessID')!=str(self.pid):continue
                    with self.lock:
                        if len(self.events)==self.events.maxlen:self.retention_drops+=1
                        self.events.append((time.perf_counter(),row));self.total+=1
            code=self.child.wait()
            if code:self.failure='PresentMon exit '+str(code)+'; see stdout/CSV and stderr (no elevation requested)'
        except Exception as e:self.failure=repr(e)
    def log(self,text,utc,tick):
        text=self.partial+text;lines=text.split('\n');self.partial=lines.pop()
        for line in lines:
            for label,pattern in [('resident','EnsureResident'),('sparse','sparse'),('arena_migration','migration'),
                 ('pipeline','pipeline'),('shader','shader'),('warning','<Warning>'),('error','<Error>')]:
                if pattern.lower() in line.lower():self.log_counts[label]+=1
            m=re.search(r'\[FRAME-GEN\] base ([\d.]+) fps, shown ([\d.]+) fps',line)
            if m:self.integration=(float(m[1]),float(m[2]),utc,tick)
            if '[FRAME-GEN] On' in line:self.fg=self.fg.split(';')[0]+'; generation enabled in log'
            if '[FRAME-GEN] Off' in line:self.fg=self.fg.split(';')[0]+'; generation off in log'
    def read(self,tick):
        frames={'guest_fps':metric(kind='guest simulation'), 'new_emulator_fps':metric(kind='unique prepared frames'),
            'host_fps':metric(kind='host present cadence'),'display_fps':metric(kind='display cadence'),
            'generated_fps':metric(kind='generated-only frames'),'latency_ms':metric(kind='display latency'),
            'frame_time_ms':metric(kind='guest frame time'),'fg_mode':self.fg,'deep_profiling':'OFF',
            'etw_event_losses':None,'presentmon_error':self.failure,'observed_log_records':dict(self.log_counts)}
        if self.integration:
            b,o,u,t=self.integration;age=tick-t
            frames['integration_present_fps']=metric(b,'existing FRAME-GEN log','emulator EndPresent calls',u,age,'rounded application-reported; source dirty',None)
            frames['integration_shown_fps']=metric(o,'existing FRAME-GEN log','Streamline shown count reported by integration; fallback can count 1',u,age,'rounded application-reported; not independent display measurement',None)
        with self.lock:events=list(self.events);total=self.total;drops=self.retention_drops
        frames['presentmon_rows']=total;frames['presentmon_retention_evictions']=drops
        # Independent cadence by swapchain, never combine distinct swapchains.
        recent=[(t,r) for t,r in events if tick-t<4]
        chains=collections.Counter(r.get('SwapChainAddress') for t,r in recent)
        if chains:
            chain=chains.most_common(1)[0][0];rs=[r for t,r in recent if r.get('SwapChainAddress')==chain]
            frames['swapchain']=chain;frames['active_swapchains']=dict(chains)
            for field,out,kind in [('CPUFrameTime','host_fps','host CPU present cadence'),('DisplayedTime','display_fps','display duration cadence')]:
                values=[]
                for r in rs:
                    try:
                        n=float(r[field])
                        if n>0 and n<10000:values.append(n)
                    except (KeyError,ValueError):pass
                if len(values)>=2:
                    age=tick-events[-1][0]
                    frames[out]=metric(1000/(sum(values)/len(values)),'PresentMon 2.6 CSV '+field,kind,None,age,'OS presentation measurement; frame class unknown',None)
            frames['present_cpu_intervals_ms']=[float(r['CPUFrameTime']) for r in rs if r.get('CPUFrameTime','').replace('.','',1).isdigit()][-256:]
        elif total:frames['presentmon_stale']=True
        return frames
    def anomalies(self,r,prev,dt):
        labels=[];mem=r['system_memory'];gpu=r['gpu'].get('engines',[])
        valid=[g['value'] for g in gpu if isinstance(g,dict) and g.get('status') in (0,1) and 'engtype_3D' in g['instance']]
        if mem.get('load',0)>90:labels.append('system_memory_pressure')
        if dt and prev:
            if sum(r['io'][k]-prev['io'][k] for k in ['read_bytes','write_bytes'])/dt>256*2**20:labels.append('io_burst_candidate')
            if valid and max(valid)<5 and (r.get('cpu_core_equivalents') or 0)>2:labels.append('gpu_low_cpu_busy_candidate')
        intervals=r['frames'].get('present_cpu_intervals_ms',[])
        if intervals and max(intervals)>100:labels.append('long_host_present_interval_candidate')
        host=r['frames'].get('host_fps',{})
        rate=host.get('value') if host.get('status')=='MEASURED' else None
        if rate and self.previous_host_rate and rate<self.previous_host_rate*.6:labels.append('host_output_rate_drop_candidate')
        if rate:self.previous_host_rate=rate
        if r['frames'].get('presentmon_stale') and valid and max(valid)>90:labels.append('gpu_busy_present_stream_stale_candidate')
        if 'nvidia_adapter_csv' in r:
            for line in r['nvidia_adapter_csv'].splitlines():
                fields=next(csv.reader([line],skipinitialspace=True))
                try:
                    if float(fields[6])>0 and float(fields[5])/float(fields[6])>.9:labels.append('adapter_vram_occupancy_high_candidate_not_budget')
                except (IndexError,ValueError):pass
        if r['frames'].get('presentmon_error') and not self.cooldowns.get('capture_unavailable'):labels.append('capture_unavailable')
        if r.get('sampling_jitter_s',0)>3:labels.append('collector_late')
        out=[];tick=r['qpc_s']
        for x in labels:
            if tick-self.cooldowns.get(x,-1000)>30:self.cooldowns[x]=tick;out.append(x)
        return out
    def close(self,reason):
        if self.child and self.child.poll() is None:
            self.child.terminate()
            try:self.child.wait(timeout=3)
            except subprocess.TimeoutExpired:self.child.kill();self.child.wait()
        if self.reader:self.reader.join(timeout=3)
        (self.root/'frame-source-status.json').write_text(json.dumps({'sources':self.sources,'rows':self.total,'error':self.failure,
             'retention_evictions':self.retention_drops,'event_loss':None,'stop_reason':reason,'observed_log_records':dict(self.log_counts)},indent=2))
