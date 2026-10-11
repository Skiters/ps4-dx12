"""Passive Windows Vulkan monitor. Standard library only; never controls the game."""
import argparse, collections, ctypes as C, datetime as D, hashlib, json, os
import pathlib, statistics, subprocess, time, uuid
from ctypes import wintypes as W

K = C.WinDLL('kernel32', use_last_error=True)
P = C.WinDLL('psapi', use_last_error=True)
H = C.WinDLL('pdh', use_last_error=True)
K.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]; K.OpenProcess.restype = W.HANDLE
K.OpenThread.argtypes = [W.DWORD, W.BOOL, W.DWORD]; K.OpenThread.restype = W.HANDLE
K.CloseHandle.argtypes = [W.HANDLE]
K.GetProcessTimes.argtypes = [W.HANDLE] + [C.POINTER(W.FILETIME)] * 4
K.GetThreadTimes.argtypes = K.GetProcessTimes.argtypes
K.QueryFullProcessImageNameW.argtypes = [W.HANDLE, W.DWORD, W.LPWSTR, C.POINTER(W.DWORD)]
K.GetThreadDescription.argtypes = [W.HANDLE, C.POINTER(C.c_void_p)]
K.LocalFree.argtypes = [C.c_void_p]
K.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]; K.CreateToolhelp32Snapshot.restype = W.HANDLE
K.GetThreadPriority.argtypes = [W.HANDLE]; K.GetThreadPriority.restype = C.c_int

class Memory(C.Structure):
    _fields_ = [('cb', W.DWORD), ('faults', W.DWORD)] + [(n,C.c_size_t) for n in
        ['peak_ws','ws','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private']]
class IO(C.Structure):
    _fields_ = [(n,C.c_ulonglong) for n in ['read_ops','write_ops','other_ops','read_bytes','write_bytes','other_bytes']]
class ThreadEntry(C.Structure):
    _fields_ = [('size',W.DWORD),('usage',W.DWORD),('tid',W.DWORD),('pid',W.DWORD),
                ('base_priority',C.c_long),('delta_priority',C.c_long),('flags',W.DWORD)]
K.Thread32First.argtypes = [W.HANDLE,C.POINTER(ThreadEntry)]
K.Thread32Next.argtypes = K.Thread32First.argtypes
K.GetExitCodeProcess.argtypes = [W.HANDLE,C.POINTER(W.DWORD)]
class ValueUnion(C.Union):
    _fields_ = [('long',C.c_long),('double',C.c_double),('large',C.c_longlong),('str',W.LPWSTR)]
class Value(C.Structure):
    _fields_ = [('status',W.DWORD),('value',ValueUnion)]
class Item(C.Structure):
    _fields_ = [('name',W.LPWSTR),('formatted',Value)]
P.GetProcessMemoryInfo.argtypes = [W.HANDLE,C.POINTER(Memory),W.DWORD]
K.GetProcessIoCounters.argtypes = [W.HANDLE,C.POINTER(IO)]
H.PdhOpenQueryW.argtypes = [W.LPCWSTR,C.c_size_t,C.POINTER(W.HANDLE)]
H.PdhAddEnglishCounterW.argtypes = [W.HANDLE,W.LPCWSTR,C.c_size_t,C.POINTER(W.HANDLE)]
H.PdhCollectQueryData.argtypes = [W.HANDLE]
H.PdhCloseQuery.argtypes = [W.HANDLE]
H.PdhGetFormattedCounterArrayW.argtypes = [W.HANDLE,W.DWORD,C.POINTER(W.DWORD),C.POINTER(W.DWORD),C.c_void_p]

def utc(): return D.datetime.now(D.timezone.utc).isoformat()
def ft(x): return (x.dwHighDateTime << 32) | x.dwLowDateTime
def times(handle, thread=False):
    a,b,c,d = (W.FILETIME() for _ in range(4))
    fn = K.GetThreadTimes if thread else K.GetProcessTimes
    if not fn(handle,C.byref(a),C.byref(b),C.byref(c),C.byref(d)): raise C.WinError(C.get_last_error())
    return ft(a),ft(c)/1e7,ft(d)/1e7
def write_json(path, data):
    tmp=path.with_suffix('.tmp'); tmp.write_text(json.dumps(data,indent=2),encoding='utf-8');tmp.replace(path)
def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    return h.hexdigest()
def identity(pid):
    h=K.OpenProcess(0x1000|0x10,False,pid)
    if not h: raise C.WinError(C.get_last_error())
    try:
        buf=C.create_unicode_buffer(32768);size=W.DWORD(len(buf))
        if not K.QueryFullProcessImageNameW(h,0,buf,C.byref(size)):raise C.WinError(C.get_last_error())
        start,_,_=times(h)
        return h,{'pid':pid,'executable':buf.value,'process_start_filetime_100ns':start,'executable_sha256':sha(buf.value)}
    except: K.CloseHandle(h);raise
def snapshot(handle):
    m=Memory();m.cb=C.sizeof(m)
    if not P.GetProcessMemoryInfo(handle,C.byref(m),m.cb):raise C.WinError(C.get_last_error())
    io=IO();K.GetProcessIoCounters(handle,C.byref(io));_,kernel,user=times(handle)
    return {'working_set_bytes':m.ws,'private_bytes':m.private,'page_faults':m.faults,
            'cpu_kernel_s':kernel,'cpu_user_s':user,'io':{n:getattr(io,n) for n,_ in IO._fields_}}
def threads(pid, previous, dt):
    result=[];now={};s=K.CreateToolhelp32Snapshot(4,0)
    try:
        e=ThreadEntry();e.size=C.sizeof(e);ok=K.Thread32First(s,C.byref(e))
        while ok:
            if e.pid==pid:
                h=K.OpenThread(0x800,False,e.tid)
                if h:
                    try:
                        start,k,u=times(h,True);key=(e.tid,start);now[key]=k+u
                        name=C.c_void_p();label=None
                        hresult=K.GetThreadDescription(h,C.byref(name))
                        if hresult>=0 and name.value:label=C.wstring_at(name.value)
                        if name.value:K.LocalFree(name)
                        result.append({'tid':e.tid,'start_filetime':start,'name':label,
                            'priority':K.GetThreadPriority(h),'cpu_total_s':k+u,
                            'cpu_core_equivalents':(k+u-previous[key])/dt if key in previous and dt>0 else None})
                    except OSError:pass
                    finally:K.CloseHandle(h)
            ok=K.Thread32Next(s,C.byref(e))
    finally:K.CloseHandle(s)
    return result,now
class Counters:
    def __init__(self):
        self.query=W.HANDLE();self.handles={};self.errors={}
        rc=H.PdhOpenQueryW(None,0,C.byref(self.query))
        if rc:raise RuntimeError('PdhOpenQuery '+hex(rc&0xffffffff))
        for name,path in [('engines',r'\GPU Engine(*)\Utilization Percentage'),
            ('dedicated',r'\GPU Process Memory(*)\Dedicated Usage'),('shared',r'\GPU Process Memory(*)\Shared Usage'),
            ('committed',r'\GPU Process Memory(*)\Total Committed')]:
            handle=W.HANDLE();rc=H.PdhAddEnglishCounterW(self.query,path,0,C.byref(handle))
            if rc:self.errors[name]=hex(rc&0xffffffff)
            else:self.handles[name]=handle
        H.PdhCollectQueryData(self.query)
    def read(self,pid):
        H.PdhCollectQueryData(self.query);out={}
        for name,h in self.handles.items():
            size=W.DWORD();count=W.DWORD();H.PdhGetFormattedCounterArrayW(h,0x200,C.byref(size),C.byref(count),None)
            if not size.value:continue
            buf=C.create_string_buffer(size.value)
            rc=H.PdhGetFormattedCounterArrayW(h,0x200,C.byref(size),C.byref(count),buf)
            if rc:out[name]={'error':hex(rc&0xffffffff)};continue
            items=C.cast(buf,C.POINTER(Item))
            out[name]=[{'instance':items[i].name,'value':items[i].formatted.value.double,
                        'status':items[i].formatted.status} for i in range(count.value)
                       if items[i].name.startswith('pid_'+str(pid)+'_')]
        return out
    def close(self):H.PdhCloseQuery(self.query)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--pid',type=int,required=True);ap.add_argument('--output',type=pathlib.Path,required=True)
    ap.add_argument('--expected-sha',required=True);ap.add_argument('--seconds',type=float,default=28800)
    ap.add_argument('--interval',type=float,default=1);ap.add_argument('--max-mb',type=int,default=1024)
    ap.add_argument('--private-growth-mb',type=float,default=512);ap.add_argument('--fault-rate',type=float,default=10000)
    ap.add_argument('--sensor-test',action='store_true')
    args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    if args.interval<=0 or args.seconds<=0 or args.max_mb<=0:raise ValueError('Positive interval, duration and storage cap required')
    root=args.output;handle,ident=identity(args.pid)
    if ident['executable_sha256']!=args.expected_sha:K.CloseHandle(handle);raise RuntimeError('Executable identity mismatch')
    session=uuid.uuid4().hex;start=time.perf_counter();self_handle=K.OpenProcess(0x1000|0x10,False,os.getpid())
    counters=Counters();meta={'mode':'A','session_id':session,'segment_id':ident['process_start_filetime_100ns'],
        'started_utc':utc(),'identity':ident,'collector_pid':os.getpid(),'counter_errors':counters.errors,'monitor_sha256':sha(__file__),
        'interval_s':args.interval,'clock':'perf_counter (Windows QPC), UTC paired per sample',
        'frame_id':None,'frame_id_reason':'External counters cannot identify a new guest frame',
        'dropped_etw_events':None,'dropped_etw_reason':'No ETW event stream captured by this counter collector'}
    for name in ['config.json','custom_configs/CUSA03173.json']:
        path=pathlib.Path(os.environ['APPDATA'])/'shadPS4'/name
        if path.exists():meta[name]={'sha256':sha(path)}
    cache=pathlib.Path(os.environ['APPDATA'])/'shadPS4/cache'
    # Directory identity metadata is deliberately not a content checksum of mutable cache files.
    meta['shader_cache_identity']={'root':str(cache),'method':'path only; mutable contents not hashed during gameplay','content_hash':None}
    meta['revision']=None if args.sensor_test else 'c7e065d1b415be16c23e260a21f1dd8bbfc4cb57';meta['title_id']=None if args.sensor_test else 'CUSA03173';meta['sensor_test']=args.sensor_test
    write_json(root/'manifest.json',meta)
    previous=None;thread_previous={};ring=collections.deque(maxlen=150);pending=[];events=0;count=0;reason='duration_limit';skipped=0;slow_samples=0;interval=args.interval
    logpath=pathlib.Path(os.environ['APPDATA'])/'shadPS4/log/shad_log.txt';logoffset=logpath.stat().st_size if logpath.exists() else 0
    try:
        with (root/'samples.jsonl').open('w',encoding='utf-8',buffering=1) as writer,(root/'anomalies.jsonl').open('w',encoding='utf-8',buffering=1) as anomalies:
            next_tick=start
            while time.perf_counter()-start<args.seconds:
                if (root/'STOP').exists():reason='stop_requested';break
                tick=time.perf_counter();r={'utc':utc(),'qpc_s':tick,'elapsed_s':tick-start,'session_id':session,'pid':args.pid,'errors':[]}
                try:
                    code=W.DWORD();K.GetExitCodeProcess(handle,C.byref(code))
                    if code.value!=259:reason='process_exited';break
                    if times(handle)[0]!=ident['process_start_filetime_100ns']:reason='pid_reused';break
                    r.update(snapshot(handle))
                except OSError:reason='process_exited_or_unreadable';break
                dt=tick-previous['qpc_s'] if previous else 0
                r['cpu_core_equivalents']=(r['cpu_user_s']+r['cpu_kernel_s']-previous['cpu_user_s']-previous['cpu_kernel_s'])/dt if dt else None
                r['cpu_machine_percent']=100*r['cpu_core_equivalents']/os.cpu_count() if dt else None
                r['threads'],thread_previous=threads(args.pid,thread_previous,dt)
                r['gpu']=counters.read(args.pid);r['collector']=snapshot(self_handle)
                r['sampling_jitter_s']=max(0,tick-next_tick)
                if count%5==0:
                    try:
                        cmd=[r'C:\Windows\System32\nvidia-smi.exe','--query-gpu=index,name,driver_version,utilization.gpu,utilization.memory,memory.used,memory.total,power.draw,temperature.gpu,clocks.current.graphics,clocks.current.memory','--format=csv,noheader,nounits']
                        child=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=0x08000000,text=True)
                        try:
                            out,err=child.communicate(timeout=2)
                            _,ck,cu=times(int(child._handle))
                            r['nvidia_child_cpu_s']=ck+cu
                            if child.returncode:raise RuntimeError(err or 'nvidia exit '+str(child.returncode))
                            r['nvidia_adapter_csv']=out.strip()
                        except subprocess.TimeoutExpired:child.kill();child.communicate();raise
                    except Exception as e:r['errors'].append('nvidia: '+str(e))
                if count%5==0 and logpath.exists():
                    with logpath.open('rb') as f:
                        size=logpath.stat().st_size
                        if size<logoffset:r['errors'].append('emulator log rotated');logoffset=0
                        f.seek(logoffset);data=f.read(65536);logoffset=f.tell()
                        r['log_tail']=data.decode('utf-8',errors='replace').splitlines()[-30:]
                labels=[]
                if previous and dt:
                    if r['private_bytes']-previous['private_bytes']>args.private_growth_mb*2**20:labels.append('private_growth')
                    if (r['page_faults']-previous['page_faults'])/dt>args.fault_rate:labels.append('page_fault_spike_not_necessarily_hard_faults')
                    if r['cpu_kernel_s']-previous['cpu_kernel_s']>dt:labels.append('kernel_cpu_over_one_core')
                if any('device lost' in l.lower() or 'unhandled exception' in l.lower() for l in r.get('log_tail',[])):labels.append('emulator_log_error_candidate')
                r['anomaly_candidates']=labels;r['collection_wall_s']=time.perf_counter()-tick;r['effective_interval_s']=interval
                writer.write(json.dumps(r,separators=(',',':'))+'\n');ring.append(r)
                while ring and ring[0]['qpc_s']<tick-30:ring.popleft()
                for event in list(pending):
                    event['after'].append(r)
                    if tick>=event['end_qpc']:
                        anomalies.write(json.dumps(event,separators=(',',':'))+'\n');pending.remove(event)
                if labels and events<100 and len(pending)<4:
                    pending.append({'event_id':events,'classification':'candidate; cause not established','labels':labels,
                                    'before':list(ring),'after':[],'end_qpc':tick+15});events+=1
                count+=1;previous=r
                write_json(root/'status.json',{'status':'recording','collector_pid':os.getpid(),'target_pid':args.pid,'samples':count,'last_utc':r['utc'],'skipped_intervals':skipped})
                if writer.tell()>args.max_mb*2**20:reason='storage_cap';break
                slow_samples=slow_samples+1 if r['collection_wall_s']>interval*.5 else 0
                if slow_samples>=3:
                    interval=min(max(args.interval,5),interval*2);slow_samples=0
                next_tick+=interval
                if time.perf_counter()>next_tick+interval:
                    missed=int((time.perf_counter()-next_tick)/interval);skipped+=missed;next_tick+=missed*interval
                time.sleep(max(0,next_tick-time.perf_counter()))
            for event in pending:event['post_window_complete']=False;anomalies.write(json.dumps(event,separators=(',',':'))+'\n')
    except Exception as e:reason='collector_error: '+repr(e)
    finally:
        counters.close();K.CloseHandle(handle);K.CloseHandle(self_handle)
        write_json(root/'status.json',{'status':'stopped','reason':reason,'samples':count,'ended_utc':utc(),'skipped_intervals':skipped})
        from report import generate
        generate(root)

if __name__=='__main__':main()
