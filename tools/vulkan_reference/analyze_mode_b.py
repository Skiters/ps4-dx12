"""Analyze internal events without treating frame hints as causal frame attribution."""
import argparse,collections,csv,json,pathlib,statistics
from report import stats,table
def main():
    ap=argparse.ArgumentParser();ap.add_argument('directory',type=pathlib.Path);a=ap.parse_args();root=a.directory
    with (root/'internal-events.tsv').open(encoding='utf-8') as f:
        events=list(csv.DictReader(f,delimiter='\t'))
    for e in events:
        for k in list(e):
            if k!='name':e[k]=int(e[k])
    scopes=collections.defaultdict(list);gpu=collections.defaultdict(list);frames=collections.defaultdict(dict);shaders={};mem=[];waits=[]
    for e in events:
        name=e['name']
        if e['end_ns']>e['start_ns']:scopes[name].append(e)
        if name=='compute.shader':shaders[e['parent']]=e['a']
        if name.startswith('gpu.') and name not in ('gpu.timestamp.skipped_capacity','gpu.timestamp.unsupported','gpu.timestamp.setup_failed','gpu.timestamp.not_ready','gpu.timestamp.read_failed'):
            gpu[(name,shaders.get(e['parent']))].append(e['a'])
        if name in ('frame.ready','frame.gpu_completion_observed','frame.return'):
            frames[e['frame_hint']][name]=e
        if name=='Presenter::Present.frame_identity' and e['a']:frames[e['a']].setdefault('presents',[]).append(e)
        if name in ('buffer.upload.bytes','buffer.download.bytes','buffer.physical.allocate'):mem.append(dict(event_id=e['id'],utc='unavailable',qpc_ns=e['start_ns'],frame_hint=e['frame_hint'],operation=name,bytes=e['a'],address_or_blocks=e['b'],size=e['c']))
        if '::Wait' in name or '::Finish' in name or 'Mutex::Lock' in name:
            waits.append(dict(name=name,event_id=e['id'],parent=e['parent'],frame_hint=e['frame_hint'],tid=e['tid'],wall_ns=e['end_ns']-e['start_ns'],cpu_ns=e['cpu_ns'],dependency_token=e['a'],necessity='unverified'))
    cpu=[]
    for name,group in scopes.items():
        cpu.append(dict(name=name,calls=len(group),inclusive_wall_ns=sum(e['end_ns']-e['start_ns'] for e in group),exclusive_wall_ns=sum(e['exclusive_wall_ns'] for e in group),inclusive_cpu_ns=sum(e['cpu_ns'] for e in group),exclusive_cpu_ns=sum(e['exclusive_cpu_ns'] for e in group),p95_wall_ns=stats(e['end_ns']-e['start_ns'] for e in group)['p95'],causal_frames_affected='unavailable'))
    cpu.sort(key=lambda r:r['exclusive_cpu_ns'],reverse=True);table(root/'CPU_PROFILE_SUMMARY.csv',cpu)
    table(root/'GPU_PROFILE_SUMMARY.csv',[dict(operation=k[0],shader_hash=k[1] if k[1] is not None else 'unavailable',calls=len(v),total_gpu_span_ns=sum(v),**stats(v)) for k,v in gpu.items()])
    table(root/'SYNC_STALLS.csv',waits);table(root/'CACHE_OPERATIONS.csv',mem)
    timeline=[]
    for frame,record in sorted(frames.items()):
        if not frame:continue
        ready=record.get('frame.ready');done=record.get('frame.gpu_completion_observed');presents=record.get('presents',[])
        first=min((e['start_ns'] for e in presents),default=None)
        timeline.append(dict(frame_id=frame,ready_cpu_ns=ready['start_ns'] if ready else None,timeline_tick=ready['a'] if ready else None,gpu_completion_observed_cpu_ns=done['start_ns'] if done else None,first_present_call_ns=first,repeated_present_calls=sum(bool(e['b']) for e in presents),host_displayed_time='unavailable',cpu_observed_completion_latency_ns=done['start_ns']-ready['start_ns'] if ready and done else None,pm4_cpu_attribution='unavailable'))
    table(root/'FRAME_TIMELINE.csv',timeline)
    status=json.loads((root/'trace-status.json').read_text())
    result={'mode':'B','trace_events':len(events),'trace_status':status,'prepared_frames':len(timeline),
        'limitations':['Scope wall includes blocking; exclusive subtracts instrumented children only.','Thread CPU time is coarse; no stack samples.','frame_hint does not establish causal PM4 attribution.','GPU query spans include dependencies; GPU-to-CPU clocks not calibrated.','Completion is observed after an existing timeline tick, not exact GPU timestamp.','Dropped events invalidate exhaustive per-event rankings; aggregate-counters.csv retains totals when aggregate_overflow=0.'],
        'reference_revision':'c7e065d1b415be16c23e260a21f1dd8bbfc4cb57'}
    unavailable={}
    for filename,data in [('GPU_PROFILE_SUMMARY.csv',gpu),('SYNC_STALLS.csv',waits),('CACHE_OPERATIONS.csv',mem),('FRAME_TIMELINE.csv',timeline)]:
        if not data:unavailable[filename]='No corresponding retained events; does not prove zero calls. Check trace losses, capture configuration and aggregate counters.'
    unavailable['MEMORY_RESIDENCY_SUMMARY.csv']='Use paired process-specific Mode A PDH memory and Mode B allocation counters. Instrumentation does not reconstruct complete live heap residency.'
    unavailable['PERFORMANCE_ANOMALIES.csv']='Internal anomaly threshold evaluation is pending; no empty success file generated.'
    result['unavailable_outputs']=unavailable
    result['event_rankings_complete']=status.get('dropped_events',0)==0
    by_id={e['id']:e for e in events}
    table(root/'CALL_TREE.csv',[{'event_id':e['id'],'parent_id':e['parent'],'name':e['name'],'parent_name':by_id[e['parent']]['name'] if e['parent'] in by_id else 'unavailable_or_root','tid':e['tid']} for e in events if e['end_ns']>e['start_ns']])
    (root/'summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    (root/'SESSION_SUMMARY.md').write_text('# Instrumented reference trace\n\n'+json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
