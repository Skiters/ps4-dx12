"""Create an immutable complete-line snapshot while a passive collector continues."""
import argparse,datetime,hashlib,json,pathlib,shutil
from report import generate
def main():
    ap=argparse.ArgumentParser();ap.add_argument('session',type=pathlib.Path);ap.add_argument('output',type=pathlib.Path);a=ap.parse_args()
    a.output.mkdir(parents=True,exist_ok=False)
    if (a.session/'collector-source').is_dir():shutil.copytree(a.session/'collector-source',a.output/'collector-source')
    for name in ['manifest.json','status.json','measurement-validation.json','monitor-source.py','frame-source-status.json','loaded-modules.json','process-exit.json','presentmon.stderr.txt','presentmon-command.json','collector-error.txt']:
        if (a.session/name).exists():shutil.copyfile(a.session/name,a.output/name)
    for name in ['samples.jsonl','anomalies.jsonl','markers.jsonl','presentmon.csv']:
        path=a.session/name
        if path.exists():
            data=path.read_bytes();end=data.rfind(b'\n')+1
            (a.output/name).write_bytes(data[:end])
    (a.output/'snapshot.json').write_text(json.dumps({'source_session':str(a.session.resolve()),'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'method':'complete newline-terminated prefix; copied status can be a few samples ahead','collector_continues':True},indent=2),encoding='utf-8')
    generate(a.output)
if __name__=='__main__':main()
