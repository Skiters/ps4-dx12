"""Summaries with explicit unavailable metrics; never substitutes utilization for time."""
import collections,csv,json,pathlib,statistics,hashlib,datetime
def stats(values):
    v=sorted(float(x) for x in values if x is not None)
    if not v:return None
    return dict(count=len(v),mean=statistics.mean(v),min=v[0],max=v[-1],median=statistics.median(v),
                p95=v[min(len(v)-1,int(.95*len(v)))],p99=v[min(len(v)-1,int(.99*len(v)))])
def table(path,rows):
    if not rows:return
    with path.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def generate(root):
    root=pathlib.Path(root);rows=[];bad=0
    for line in (root/'samples.jsonl').read_text(encoding='utf-8').splitlines() if (root/'samples.jsonl').exists() else []:
        try:rows.append(json.loads(line))
        except ValueError:bad+=1
    meta=json.loads((root/'manifest.json').read_text());metrics={}
    supervisor_exit=None;exit_code=meta.get('exit_code')
    exit_path=root/'process-exit.json'
    if exit_path.exists():
        evidence=json.loads(exit_path.read_text(encoding='utf-8-sig'))
        seconds,ticks=divmod(meta['identity']['process_start_filetime_100ns'],10000000)
        start=datetime.datetime(1601,1,1,tzinfo=datetime.timezone.utc)+datetime.timedelta(seconds=seconds)
        expected_start=start.strftime('%Y-%m-%dT%H:%M:%S')+'.'+f'{ticks:07d}'+'Z'
        if evidence.get('pid')==meta['identity']['pid'] and evidence.get('start_utc')==expected_start:
            supervisor_exit=evidence
            if evidence.get('exit_code') is not None:exit_code=evidence['exit_code'] & 0xffffffff
    for key in ['cpu_core_equivalents','cpu_machine_percent','working_set_bytes','private_bytes','commit_bytes','collection_wall_s','sampling_jitter_s']:
        metrics[key]=stats(r.get(key) for r in rows)
    cpu=collections.defaultdict(list);gpu=collections.defaultdict(list);memory=collections.defaultdict(list)
    for r in rows:
        for t in r.get('threads',[]):cpu[(t['tid'],t['start_filetime'],t['name'] or 'unnamed')].append(t.get('cpu_core_equivalents'))
        for category,items in r.get('gpu',{}).items():
            if not isinstance(items,list):continue
            for i in items:
                if i['status'] not in (0,1):continue
                (gpu if category=='engines' else memory)[(category,i['instance'])].append(i['value'])
    def records(groups):
        return [dict(category=k[0],instance=k[1],**stats(v)) for k,v in groups.items() if stats(v)]
    table(root/'CPU_PROFILE_SUMMARY.csv',[dict(tid=k[0],start_filetime=k[1],name=k[2],**stats(v)) for k,v in cpu.items() if stats(v)])
    table(root/'GPU_PROFILE_SUMMARY.csv',records(gpu));table(root/'MEMORY_RESIDENCY_SUMMARY.csv',records(memory))
    unavailable={
        'FRAME_TIMELINE.csv':'Guest frame IDs and GPU completion require matching instrumented Vulkan. External present stream, when available, is separate in presentmon.csv.',
        'SYNC_STALLS.csv':'Counters do not measure wait durations, lock owners or causal dependencies. ETW ready/context-switch stacks require a separately authorized trace.',
        'CACHE_OPERATIONS.csv':'Internal cache calls and exact upload/download bytes require Mode B.',
    }
    anomalies=[]
    for r in rows:
        for label in r.get('anomaly_candidates',[]):anomalies.append({'utc':r['utc'],'qpc_s':r['qpc_s'],'candidate':label,'cause':'unverified'})
    if anomalies:table(root/'PERFORMANCE_ANOMALIES.csv',anomalies)
    else:unavailable['PERFORMANCE_ANOMALIES.csv']='No configured anomaly candidates observed; not proof that no stall occurred.'
    overhead={}
    if len(rows)>1:
        seconds=rows[-1]['qpc_s']-rows[0]['qpc_s'];a,b=rows[0]['collector'],rows[-1]['collector']
        overhead={'duration_s':seconds,'collector_cpu_core_equivalents':(b['cpu_user_s']+b['cpu_kernel_s']-a['cpu_user_s']-a['cpu_kernel_s'])/seconds,
                  'collector_working_set_peak_bytes':max(r['collector']['working_set_bytes'] for r in rows),
                  'collector_io_write_bytes_delta':b['io']['write_bytes']-a['io']['write_bytes'],
                  'nvidia_child_cpu_s':sum(r.get('nvidia_child_cpu_s',0) for r in rows),
                  'child_tool_cpu_available':any('nvidia_child_cpu_s' in r for r in rows),'collection_wall_time_is_not_cpu_time':True}
    errors=collections.Counter(e for r in rows for e in r.get('errors',[]))
    status=json.loads((root/'status.json').read_text()) if (root/'status.json').exists() else {'status':'unknown'}
    frame_status=json.loads((root/'frame-source-status.json').read_text()) if (root/'frame-source-status.json').exists() else {}
    markers=[json.loads(l) for l in (root/'markers.jsonl').read_text(encoding='utf-8-sig').splitlines()] if (root/'markers.jsonl').exists() else []
    validation=json.loads((root/'measurement-validation.json').read_text()) if (root/'measurement-validation.json').exists() else {}
    nvidia=[]
    for r in rows:
        for line in r.get('nvidia_adapter_csv','').splitlines():
            values=next(csv.reader([line],skipinitialspace=True))
            if len(values)!=11:continue
            item={'utc':r['utc'],'index':values[0],'name':values[1],'driver':values[2]}
            for col,key in enumerate(['utilization_gpu_percent','utilization_memory_percent','memory_used_mib','memory_total_mib','power_w','temperature_c','clock_graphics_mhz','clock_memory_mhz'],3):
                try:item[key]=float(values[col])
                except ValueError:item[key]=None
            nvidia.append(item)
    table(root/'NVIDIA_ADAPTER_SUMMARY.csv',nvidia)
    summary={'mode':'A','session_id':meta['session_id'],'samples':len(rows),'malformed_rows':bad,'collector_status':status,
        'report_generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
        'first_utc':rows[0]['utc'] if rows else None,'last_utc':rows[-1]['utc'] if rows else None,'metrics':metrics,
        'collector_overhead':overhead,'unavailable_outputs':unavailable,'errors':dict(errors),'measurement_validation':validation,
        'thread_count':stats(len(r.get('threads',[])) for r in rows),
        'named_thread_samples':sum(bool(t.get('name')) for r in rows for t in r.get('threads',[])),
        'nvidia_adapter_samples':len(nvidia),
        'gpu_engine_statistics':records(gpu),'gpu_memory_statistics':records(memory),
        'page_faults_delta':rows[-1]['page_faults']-rows[0]['page_faults'] if rows else None,
        'observed_duration_s':rows[-1]['qpc_s']-rows[0]['qpc_s'] if len(rows)>1 else None,
        'io_deltas':{k:rows[-1]['io'][k]-rows[0]['io'][k] for k in rows[0]['io']} if rows else {},
        'guest_fps':None,'gpu_operation_timing':None,'scenes':'See manual markers; free-play and HUD acceptance are not stationary benchmarks',
        'identity':meta['identity'],'revision':meta.get('revision'),'build_label':meta.get('build_label'),
        'frame_source_status':frame_status,'markers':markers,'exit_code':exit_code,
        'supervisor_exit':supervisor_exit,'collector_exit_code':meta.get('exit_code'),
        'exit_evidence_disagrees':supervisor_exit is not None and meta.get('exit_code') is not None and exit_code!=meta['exit_code'],
        'exit_classification':'nonzero process exit; cause unknown' if exit_code not in (None,0) else ('zero process exit' if exit_code==0 else 'not observed'),
        'rate_statistics_method':'sample-held rounded integration log values; not independent frame windows',
        'system_memory_pressure_percent':stats(r.get('system_memory',{}).get('load') for r in rows),
        'system_available_ram_bytes':stats(r.get('system_memory',{}).get('available_physical') for r in rows),
        'coverage_definition':meta.get('coverage_definition'),
        'frame_rate_metrics':{k:stats(r.get('frames',{}).get(k,{}).get('value') for r in rows if r.get('frames',{}).get(k,{}).get('status')=='MEASURED') for k in ['host_fps','display_fps','integration_present_fps','integration_shown_fps']}}
    hud=[r['hud'] for r in rows if 'hud' in r]
    if len(hud)>1:
        seconds=rows[-1]['qpc_s']-rows[0]['qpc_s']
        groups=collections.defaultdict(list)
        for h in hud:groups[h['pid']].append(h)
        summary['hud_overhead']={'cpu_core_equivalents':sum(g[-1]['cpu_total_s']-g[0]['cpu_total_s'] for g in groups.values())/seconds,
             'ram_peak_bytes':max(h['ram_bytes'] for h in hud),'refreshes':sum(g[-1]['refreshes']-g[0]['refreshes'] for g in groups.values())}
    (root/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    (root/'report-generator-source.py').write_bytes(pathlib.Path(__file__).read_bytes())
    cpu_status='MEASURED' if len(rows)>1 else 'INVALID_MEASUREMENT'
    thread_status='PARTIALLY_MEASURED' if cpu else 'NOT_MEASURED'
    gpu_status='PARTIALLY_MEASURED' if gpu else 'NOT_MEASURED'
    nvidia_status='MEASURED' if nvidia else 'NOT_MEASURED'
    coverage=f'''# Telemetry coverage — Mode A

| Metric | Status | Source | Timestamp/frame | Stack/loss/limit |
|---|---|---|---|---|
| Process CPU/kernel/user, RAM/private, I/O/page faults | {cpu_status} | Win32 process counters | UTC + QPC / no frame | No stack; page faults include soft faults |
| Thread names/CPU/priority | {thread_status} | Toolhelp + GetThreadTimes/Description | UTC + QPC / no frame | Named observations: {summary['named_thread_samples']}; no ready/wait duration, affinity or migration; vanished/inaccessible threads omitted |
| Process GPU engines/memory/LUID | {gpu_status} | English PDH GPU counters | UTC + QPC / no frame | Engine accounting interval, not operation time; only valid PDH samples |
| NVIDIA telemetry | {nvidia_status} | nvidia-smi, every fifth sample | UTC + QPC / no frame | Valid observations: {len(nvidia)}; whole adapter, includes other apps; failures preserved |
| Collector load/jitter/I/O | PARTIALLY_MEASURED | Collector process + QPC | UTC + QPC / no frame | Child CPU availability in summary; child RAM not sampled; skipped intervals in status |
| Present/display cadence | {'PARTIALLY_MEASURED' if summary['frame_rate_metrics']['host_fps'] or summary['frame_rate_metrics']['display_fps'] else 'NOT_MEASURED'} | Configured PresentMon external stream | QPC / no guest frame ID | Current source status: {json.dumps(frame_status)}; no guessed FPS |
| CPU stacks/ready/context switches/WDDM edges | NOT_MEASURED | WPR installed | unavailable | No administrative token; no elevation or kernel session requested |
| Guest/HLE/PM4/cache/dirty/detile/PSO/VideoOut IDs | REQUIRES_INSTRUMENTED_BUILD | Mode B hooks | pending | External counters cannot observe these calls |
| GPU stage timestamps and completion | REQUIRES_INSTRUMENTED_BUILD | Scheduler timestamp instrumentation | pending | Utilization is not a timestamp |
| Device errors | PARTIALLY_MEASURED | Incremental existing emulator log | sample UTC / no frame | Capped tail; duplicate suppression; full log retained by emulator |

No completeness claim. Counter errors are in manifest and summary. No ETW stream was recorded,
so dropped ETW events are unavailable rather than zero. Snapshot sampling misses short episodes.

Measurement validation: {json.dumps(validation,ensure_ascii=False)}

Live coverage denominator (fixed): {json.dumps(meta.get('coverage_definition'))}.
System available RAM/pressure: GlobalMemoryStatusEx, once per system sample.
Guest FPS, generated-only IDs, GPU stages, wait stacks and latency remain unavailable.
'''
    (root/'TELEMETRY_COVERAGE.md').write_text(coverage,encoding='utf-8')
    report='# Vulkan passive session\n\n'+json.dumps(summary,indent=2)+'\n\nThis free-play session is not a matched DX12 benchmark. GPU engine activity is not guest FPS.\n'
    (root/'SESSION_SUMMARY.md').write_text(report,encoding='utf-8')
    files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir() if p.is_file() and p.name not in ('integrity.json','status.json','STOP')}
    (root/'integrity.json').write_text(json.dumps(files,indent=2),encoding='utf-8')
if __name__=='__main__':
    import sys
    generate(sys.argv[1])
