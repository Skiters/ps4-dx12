"""Explicit manual XInput-compatible game control through an installed ViGEm client.

Never injects, installs drivers, modifies a physical pad, or starts with telemetry.
ABI: https://github.com/nefarius/ViGEmClient (Client.h / Common.h).
"""
import argparse, ctypes as C, datetime as D, hashlib, json, os, pathlib, time, uuid

OK=0x20000000
BUTTONS=dict(up=1,down=2,left=4,right=8,start=16,back=32,ls=64,rs=128,
             lb=256,rb=512,a=4096,b=8192,x=16384,y=32768)
class Report(C.Structure):
    _fields_=[('buttons',C.c_ushort),('lt',C.c_ubyte),('rt',C.c_ubyte)]+[(n,C.c_short) for n in ('lx','ly','rx','ry')]
class State(C.Structure):
    _fields_=[('packet',C.c_uint32),('gamepad',Report)]

def wait_report(get,index,report,timeout=.25):
    """Wait for XInput's cached state to reflect an accepted driver update."""
    deadline=time.monotonic()+timeout;attempts=0
    while True:
        state=State();rc=get(index,C.byref(state));attempts+=1
        if not rc and bytes(state.gamepad)==bytes(report):return rc,state,attempts
        if time.monotonic()>=deadline:return rc,state,attempts
        time.sleep(.005)
def utc():return D.datetime.now(D.timezone.utc).isoformat()
def sha(path):return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
def save(path,value):
    tmp=path.with_suffix('.tmp');tmp.write_text(json.dumps(value,indent=2),encoding='utf-8')
    for attempt in range(20):
        try:tmp.replace(path);return
        except PermissionError:
            if attempt==19:raise
            time.sleep(.01)
def checked(code,operation):
    if code!=OK:raise RuntimeError(operation+' failed: '+hex(code))
def bind(dll,name,result,args):
    fn=getattr(dll,name);fn.restype=result;fn.argtypes=args;return fn
def serve(a):
    # Reuse verified process identity/handle; a recycled PID is never accepted.
    from mode_a import identity,times,exited,K,W
    handle,ident=identity(a.target_pid)
    K.CreateMutexW.argtypes=[C.c_void_p,W.BOOL,W.LPCWSTR];K.CreateMutexW.restype=W.HANDLE
    C.set_last_error(0)
    mutex=K.CreateMutexW(None,False,'Local\\VulkanXInput-'+str(a.target_pid))
    if not mutex:K.CloseHandle(handle);raise C.WinError(C.get_last_error())
    if C.get_last_error()==183:
        K.CloseHandle(mutex);K.CloseHandle(handle);raise RuntimeError('A feeder for this game already exists')
    if ident['executable_sha256']!=a.expected_sha:K.CloseHandle(handle);raise RuntimeError('Wrong game executable')
    if sha(a.client_dll)!=a.expected_client_sha:K.CloseHandle(handle);raise RuntimeError('Client DLL hash mismatch')
    a.root.mkdir(parents=True,exist_ok=False);(a.root/'commands').mkdir()
    sdk=C.CDLL(str(a.client_dll))  # Published cdecl ViGEm ABI; user-mode process only.
    alloc=bind(sdk,'vigem_alloc',C.c_void_p,[])
    connect=bind(sdk,'vigem_connect',C.c_uint32,[C.c_void_p])
    allocate_pad=bind(sdk,'vigem_target_x360_alloc',C.c_void_p,[])
    add=bind(sdk,'vigem_target_add',C.c_uint32,[C.c_void_p,C.c_void_p])
    update=bind(sdk,'vigem_target_x360_update',C.c_uint32,[C.c_void_p,C.c_void_p,Report])
    user_index=bind(sdk,'vigem_target_x360_get_user_index',C.c_uint32,[C.c_void_p,C.c_void_p,C.POINTER(C.c_uint32)])
    remove=bind(sdk,'vigem_target_remove',C.c_uint32,[C.c_void_p,C.c_void_p])
    free_pad=bind(sdk,'vigem_target_free',None,[C.c_void_p])
    disconnect=bind(sdk,'vigem_disconnect',None,[C.c_void_p])
    free=bind(sdk,'vigem_free',None,[C.c_void_p])
    xi=C.WinDLL('xinput1_4.dll');get=bind(xi,'XInputGetState',C.c_uint32,[C.c_uint32,C.POINTER(State)])
    user=C.windll.user32;user.GetForegroundWindow.restype=C.c_void_p
    user.GetWindowThreadProcessId.argtypes=[C.c_void_p,C.POINTER(C.c_uint32)]
    client=alloc();pad=None;attached=False;index=None;reason='duration_limit'
    meta=dict(session_id=uuid.uuid4().hex,identity=ident,client_dll=str(a.client_dll),client_sha256=a.expected_client_sha,
              started_utc=utc(),feeder_pid=os.getpid(),driver_install=False,injection=False,automatic_gameplay=False)
    source=pathlib.Path(__file__);meta['feeder_source_sha256']=sha(source)
    (a.root/'feeder-source.py').write_bytes(source.read_bytes())
    active=a.root.parent/'active-session.json'
    if active.exists():
        capture=json.loads(active.read_text(encoding='utf-8-sig'))
        if capture.get('target_pid')==a.target_pid:meta['telemetry_segment']=capture['session']
    save(a.root/'manifest.json',meta)
    def alive():
        return exited(handle) is None and times(handle)[0]==ident['process_start_filetime_100ns']
    def foreground():
        p=C.c_uint32();user.GetWindowThreadProcessId(user.GetForegroundWindow(),C.byref(p));return p.value==a.target_pid
    try:
        if not client:raise RuntimeError('Client allocation failed')
        checked(connect(client),'bus connect/version handshake')
        pad=allocate_pad()
        if not pad:raise RuntimeError('Pad allocation failed')
        checked(add(client,pad),'new virtual pad attach');attached=True
        checked(update(client,pad,Report()),'initial neutral report')
        ix=C.c_uint32();checked(user_index(client,pad,C.byref(ix)),'XInput index');index=ix.value
        if index>3:raise RuntimeError('Invalid XInput index')
        state=State();rc=get(index,C.byref(state))
        if rc:raise RuntimeError('Virtual device not readable by XInput: '+str(rc))
        meta['xinput_index']=index;meta['initial_packet']=state.packet;save(a.root/'manifest.json',meta)
        start=time.monotonic();next_status=0
        with (a.root/'actions.jsonl').open('a',encoding='utf-8',buffering=1) as log:
            while time.monotonic()-start<a.seconds and alive():
                if (a.root/'STOP').exists():reason='stop_requested';break
                if time.monotonic()>=next_status:
                    save(a.root/'status.json',dict(status='ready',utc=utc(),pid=os.getpid(),target_pid=a.target_pid,xinput_index=index))
                    next_status=time.monotonic()+1
                for path in sorted((a.root/'commands').glob('*.json')):
                    command=json.loads(path.read_text(encoding='utf-8'))
                    event=dict(command=command,utc=utc(),qpc_s=time.perf_counter(),identity=ident,xinput_index=index,session_id=meta['session_id'])
                    try:
                        if (D.datetime.now(D.timezone.utc)-D.datetime.fromisoformat(command['utc'])).total_seconds()>3:
                            raise RuntimeError('Queued command is stale; input refused')
                        if not foreground():raise RuntimeError('Game is not foreground; input refused')
                        hold=int(command.get('hold_ms',200))
                        if not 20<=hold<=5000:raise ValueError('Hold must be 20..5000ms')
                        buttons=0
                        for key in command.get('buttons',[]):buttons|=BUTTONS[key.lower()]
                        axes={n:int(command.get(n,0)) for n in ('lx','ly','rx','ry','lt','rt')}
                        for n,v in axes.items():
                            if not ((0<=v<=255) if n in ('lt','rt') else (-32768<=v<=32767)):raise ValueError('Axis out of range')
                        report=Report(buttons,axes['lt'],axes['rt'],axes['lx'],axes['ly'],axes['rx'],axes['ry'])
                        checked(update(client,pad,report),'input report')
                        rc,state,attempts=wait_report(get,index,report)
                        event['input_readback_attempts']=attempts
                        event['xinput_readback']=dict(result=rc,packet=state.packet,**{n:getattr(state.gamepad,n) for n,_ in Report._fields_})
                        if rc or bytes(state.gamepad)!=bytes(report):raise RuntimeError('XInput readback differs from requested report')
                        end=time.monotonic()+hold/1000
                        while time.monotonic()<end:
                            if not alive() or not foreground() or (a.root/'STOP').exists():break
                            time.sleep(.02)
                        event['result']='sent; game reaction must be observed separately'
                    except Exception as e:event['error']=repr(e)
                    finally:
                        checked(update(client,pad,Report()),'release to neutral')
                        rc,neutral,attempts=wait_report(get,index,Report())
                        event['neutral_readback_attempts']=attempts
                        event['neutral_readback']=dict(result=rc,packet=neutral.packet,**{n:getattr(neutral.gamepad,n) for n,_ in Report._fields_})
                        if rc or bytes(neutral.gamepad)!=bytes(Report()):
                            event['release_error']='Neutral release verification failed'
                            log.write(json.dumps(event)+'\n')
                            raise RuntimeError(event['release_error'])
                    event['released_utc']=utc();event['released_qpc_s']=time.perf_counter();log.write(json.dumps(event)+'\n')
                    path.rename(path.with_suffix('.handled'))
                time.sleep(.05)
            if not alive():reason='game_exited'
    except Exception as e:
        reason='error: '+repr(e);raise
    finally:
        removal=None
        if attached:
            update(client,pad,Report());removal=remove(client,pad)
        if pad:free_pad(pad)
        if client:disconnect(client);free(client)
        K.CloseHandle(handle)
        K.CloseHandle(mutex)
        save(a.root/'status.json',dict(status='stopped',utc=utc(),reason=reason,xinput_index=index,
             virtual_device_remove_result=removal,virtual_device_removed=removal==OK))
def main():
    p=argparse.ArgumentParser();sub=p.add_subparsers(dest='mode',required=True)
    s=sub.add_parser('serve');s.add_argument('--root',type=pathlib.Path,required=True)
    s.add_argument('--target-pid',type=int,required=True);s.add_argument('--expected-sha',required=True)
    s.add_argument('--client-dll',type=pathlib.Path,required=True);s.add_argument('--expected-client-sha',required=True)
    s.add_argument('--seconds',type=int,default=1200)
    c=sub.add_parser('send');c.add_argument('--root',type=pathlib.Path,required=True)
    c.add_argument('--buttons',nargs='*',default=[]);c.add_argument('--hold-ms',type=int,default=200)
    for n in ('lx','ly','rx','ry','lt','rt'):c.add_argument('--'+n,type=int,default=0)
    stop=sub.add_parser('stop');stop.add_argument('--root',type=pathlib.Path,required=True)
    a=p.parse_args()
    if a.mode=='serve' and not 1<=a.seconds<=3600:p.error('Duration must be 1..3600 seconds')
    if a.mode=='serve':serve(a)
    elif a.mode=='stop':(a.root/'STOP').write_text('explicit stop',encoding='utf-8')
    else:
        status=json.loads((a.root/'status.json').read_text())
        if status['status']!='ready':raise RuntimeError('Feeder is not ready')
        if (D.datetime.now(D.timezone.utc)-D.datetime.fromisoformat(status['utc'])).total_seconds()>3:
            raise RuntimeError('Feeder status is stale')
        payload={n:getattr(a,n) for n in ('buttons','hold_ms','lx','ly','rx','ry','lt','rt')};payload['utc']=utc()
        save(a.root/'commands'/(uuid.uuid4().hex+'.json'),payload)
if __name__=='__main__':main()
