"""Non-game integration test: hidden named Python fixture, bounded monitor, reports."""
import argparse,ctypes,hashlib,json,pathlib,subprocess,sys,time
def main():
    ap=argparse.ArgumentParser();ap.add_argument('output',type=pathlib.Path);a=ap.parse_args()
    if a.output.exists():raise RuntimeError('Use a fresh test directory')
    fixture="import ctypes,time; k=ctypes.windll.kernel32; k.SetThreadDescription.argtypes=[ctypes.c_void_p,ctypes.c_wchar_p]; k.SetThreadDescription(ctypes.c_void_p(-2),'ReferenceMonitorSensorFixture'); time.sleep(12)"
    child=subprocess.Popen([sys.executable,'-c',fixture],creationflags=0x08000000)
    try:
        time.sleep(.2)
        sha=hashlib.sha256(pathlib.Path(sys.executable).read_bytes()).hexdigest()
        subprocess.run([sys.executable,str(pathlib.Path(__file__).with_name('mode_a.py')),'--sensor-test','--pid',str(child.pid),'--expected-sha',sha,'--output',str(a.output),'--seconds','7'],check=True,creationflags=0x08000000)
        rows=[json.loads(l) for l in (a.output/'samples.jsonl').read_text().splitlines()]
        assert len(rows)>=6 and not any(r['errors'] for r in rows)
        assert any(t['name']=='ReferenceMonitorSensorFixture' for r in rows for t in r['threads'])
        assert any('nvidia_adapter_csv' in r and 'nvidia_child_cpu_s' in r for r in rows)
        assert json.loads((a.output/'summary.json').read_text())['nvidia_adapter_samples']>=1
        assert json.loads((a.output/'status.json').read_text())['reason']=='duration_limit'
        print('PASS: collector identity, named threads, NVIDIA/child CPU, report parsing, graceful stop. GPU workload timing not tested.')
    finally:child.wait()
if __name__=='__main__':main()
