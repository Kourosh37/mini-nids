"""Package current sources and completed academic reports with a verified SHA-256 manifest."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[2]

def digest_file(path):
    digest=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''): digest.update(block)
    return digest.hexdigest()

def package(study,output):
    if output.exists(): raise ValueError('Refusing to overwrite an existing submission')
    required=['presentation.md','docs/report/Mini-NIDS-report-fa.pdf','docs/reference/Mini-NIDS.pdf',
              'docs/academic/alert-review.csv','docs/academic/packet-evidence/false-alert-triggers.csv']
    for name in required:
        if not (ROOT/name).is_file(): raise ValueError('Missing required deliverable: '+name)
    results=json.loads((study/'results.json').read_text(encoding='utf-8'))
    if len(results['results'])!=28: raise ValueError('The complete four-threshold, seven-file study is required')
    for path in study.rglob('session_status.json'):
        if json.loads(path.read_text())['status']!='complete': raise ValueError('Incomplete session: '+str(path))
    listing=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT)
    files={}
    for name in listing.decode('utf-8').split('\0'):
        path=ROOT/name
        if name and path.is_file() and path.suffix.lower() not in ('.pcap','.pcapng','.zip','.log','.pyc'):
            files[name]=path
    for path in study.rglob('*'):
        if path.is_file() and path.suffix in ('.csv','.json'):
            files['reports/'+path.relative_to(study).as_posix()]=path
    output.parent.mkdir(parents=True,exist_ok=True)
    partial=output.with_suffix('.partial.zip');manifest={};prefix='Mini-NIDS/'
    try:
        with zipfile.ZipFile(partial,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
            for name,path in sorted(files.items()):
                archive.write(path,prefix+name)
                manifest[name]={'bytes':path.stat().st_size,'sha256':digest_file(path)}
            presentation=(ROOT/'presentation.md').read_bytes()
            archive.writestr(prefix+'presentation.html',presentation)
            manifest['presentation.html']={'bytes':len(presentation),'sha256':hashlib.sha256(presentation).hexdigest()}
            note=('بسته تحویل Mini-NIDS — ۲۵ نوامبر ۲۰۲۶\n'
                  'presentation.html را با مرورگر و docs/report/Mini-NIDS-report-fa.pdf را با PDF reader باز کنید.\n'
                  'سورس، README/RUNBOOK، صورت مسئله اصلی، مرجع مشاهده‌شده و گزارش کامل ۲۸ اجرا همراه‌اند.\n'
                  'گزارش‌های پیش‌فرض در reports/default و حساسیت در reports/scan-N هستند.\n'
                  'captureهای ورودی، SDK، build، Git و لاگ موقت همراه نیستند؛ لینک دانلود در README است.\n'
                  'مرجع، مشاهده مستقل TShark است و برچسب رسمی/داوری دوم انسانی نیست.\n'
                  'آزمایش واقعی driver کپچر زنده محلی انجام نشده؛ اسکریپت و workflow آن موجود است.\n'
                  'manifest.json اندازه و SHA-256 همه محتوای بسته جز خود manifest را دارد.\n').encode('utf-8')
            archive.writestr(prefix+'SUBMISSION-README.txt',note)
            manifest['SUBMISSION-README.txt']={'bytes':len(note),'sha256':hashlib.sha256(note).hexdigest()}
            archive.writestr(prefix+'manifest.json',json.dumps({'method':'SHA-256 per payload; manifest excludes itself',
                              'files':manifest},indent=2)+'\n')
        with zipfile.ZipFile(partial) as archive:
            if len(archive.namelist())!=len(manifest)+1: raise ValueError('Duplicate/unexpected archive member')
            for name,metadata in manifest.items():
                digest=hashlib.sha256();size=0
                with archive.open(prefix+name) as f:
                    for block in iter(lambda:f.read(1024*1024),b''):
                        digest.update(block);size+=len(block)
                if size!=metadata['bytes'] or digest.hexdigest()!=metadata['sha256']:
                    raise ValueError('Archive verification failed: '+name)
        partial.rename(output)
    except BaseException:
        partial.unlink(missing_ok=True)
        raise
    print(json.dumps({'archive':str(output),'files':len(manifest)+1,'bytes':output.stat().st_size,
                      'sha256':digest_file(output),'verified':'All payload hashes and ZIP CRCs'},indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--study',type=Path,required=True)
    p.add_argument('--output',type=Path,default=ROOT/'output/Mini-NIDS-submission.zip')
    a=p.parse_args();package(a.study,a.output)
