"""Refresh source excerpts/study tables in PRESENTATION.md and export browser HTML.

PDF printing is a separate explicit step: use a browser with Persian text shaping.
This command never downloads tools or invokes a browser.
"""
import argparse
import base64
import csv
import html
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[2]

def browser_document(text, public_links=False):
    # Embedded web fonts also work when a browser restricts file:// font requests.
    for font in ('Vazirmatn-Regular.woff2','Vazirmatn-Bold.woff2'):
        relative='docs/academic/fonts/'+font
        encoded=base64.b64encode((ROOT/relative).read_bytes()).decode('ascii')
        text=text.replace(relative,'data:font/woff2;base64,'+encoded)
    def embed_figure(match):
        relative=match.group(2)
        encoded=base64.b64encode((ROOT/relative).read_bytes()).decode('ascii')
        return match.group(1)+'data:image/png;base64,'+encoded+match.group(3)
    text=re.sub(r'(<img\b[^>]*\bsrc=")([^"#]+\.png)(")',embed_figure,text)
    if public_links:
        text=re.sub(r'<a href="(?!https?:|#)([^"]+)"',lambda m:'<a href="https://github.com/Kourosh37/mini-nids/blob/main/'+m.group(1)+'"',text)
    return text

def table(rows, columns):
    output='<table><thead><tr>'+''.join('<th>'+html.escape(label)+'</th>' for _,label in columns)+'</tr></thead><tbody>'
    for row in rows:
        output+='<tr>'
        for key,_ in columns:
            value=row.get(key,'')
            if isinstance(value,float): value=f'{value:.6g}'
            direction='rtl' if re.search(r'[\u0600-\u06ff]',str(value)) else 'ltr'
            output+='<td><bdi dir="'+direction+'">'+html.escape(str(value))+'</bdi></td>'
        output+='</tr>'
    return output+'</tbody></table>'

def read_csv(name):
    with (ROOT/'docs/academic'/name).open(newline='',encoding='utf-8') as f: return list(csv.DictReader(f))

def build(output):
    source=ROOT/'PRESENTATION.md'; text=source.read_text(encoding='utf-8')
    def excerpt(match):
        path,anchor,count=match.group(1),match.group(2),int(match.group(3))
        lines=(ROOT/path).read_text(encoding='utf-8').splitlines()
        found=[i for i,line in enumerate(lines) if anchor in line]
        if not found: raise ValueError(f'Missing source anchor {path}: {anchor}')
        first=found[0]; chosen=lines[first:first+count]
        snippet='\n'.join(f'{first+i+1:4}  {line}' for i,line in enumerate(chosen))
        return (match.group(0).split('-->')[0]+'-->\n'
                +f'<p class="source">کد واقعی: <a href="{path}#L{first+1}"><bdi dir="ltr">{path}</bdi>، سطر {first+1}</a>؛ اعداد سمت چپ شماره سطرند.</p>\n'
                +'<pre dir="ltr"><code>'+html.escape(snippet)+'</code></pre>\n<!-- END CODE -->')
    text=re.sub(r'<!-- CODE:([^|]+)\|([^|]+)\|(\d+) -->.*?<!-- END CODE -->',excerpt,text,flags=re.S)
    configs={
        'matrix':('rule-file-matrix.csv',[(k,k) for k in ['capture','ARP-01','ARP-02','SCAN-01','SCAN-02','SYN-01','DNS-01','DNS-02','DNS-03','SLOW-01','TP','FP','FN']]),
        'baseline':('baselines.csv',[("capture","فایل"),("seconds","بازه پایه (s)"),("mean_packets_per_second","میانگین packet/s"),("max_packets_per_second","بیشینه packet/s"),("mean_syn_per_second","میانگین SYN/s"),("max_syn_per_second","بیشینه SYN/s")]),
        'sensitivity':('sensitivity.csv',[("scan_ports","آستانه پورت"),("TP","TP کل"),("FP","FP کل"),("FN","FN کل"),("precision","Precision کل"),("recall","Recall کل"),("scan_FP","FP اسکن"),("scan_recall","Recall اسکن"),("mean_detected_scan_delay_s","میانگین delay اسکن‌های کشف‌شده (s)")]),
        'performance':('performance.csv',[("capture","فایل"),("packets","بسته"),("engine_seconds","پردازش (s)"),("packets_per_second","packet/s"),("peak_memory_bytes","بیشینه حافظه (byte)"),("state_drops","افت state")]),
        'delays':('detection-delays.csv',[("capture","فایل"),("event","بازه مرجع"),("delay_display","تأخیر کشف")]),
    }
    observations=json.loads((ROOT/'docs/academic/reviewed-observations.json').read_text(encoding='utf-8'))
    from decimal import Decimal
    events=[]
    for capture in observations['captures']:
        for event in capture['events']:
            events.append({'capture':capture['name'],'event':event['id'],
                           'start':event['start'],'end':event['end'],
                           'duration_s':str(Decimal(event['end'])-Decimal(event['start'])),
                           'packets':str(event['first_packet'])+' / '+str(event['last_packet'])})
    text=re.sub(r'<!-- TABLE:events -->.*?<!-- END TABLE -->',lambda _: '<!-- TABLE:events -->\n'+table(events,[('capture','فایل'),('event','بازه'),('start','آغاز Unix s'),('end','پایان Unix s'),('duration_s','طول بازه (s)'),('packets','بسته اول / آخر')])+'\n<!-- END TABLE -->',text,flags=re.S)
    for key,(filename,columns) in configs.items():
        rows=read_csv(filename)
        for row in rows:
            for field in ('engine_seconds','packets_per_second','mean_packets_per_second','mean_syn_per_second','precision','recall','scan_recall','mean_detected_scan_delay_s'):
                if field in row and row[field]: row[field]=float(row[field])
        pattern=r'<!-- TABLE:'+re.escape(key)+r' -->.*?<!-- END TABLE -->'
        text=re.sub(pattern,lambda _:f'<!-- TABLE:{key} -->\n'+table(rows,columns)+'\n<!-- END TABLE -->',text,flags=re.S)
    if '<!-- NEXT:' in text: raise ValueError('Unfinished chapter marker remains')
    ids=set(re.findall(r'\bid="([^"]+)"',text))
    for target in re.findall(r'href="#([^"]+)"',text):
        if target not in ids: raise ValueError('Broken chapter link: '+target)
    for target in re.findall(r'(?:href|src)="([^"]+)"',text):
        if target.startswith(('http:','https:','#','mailto:')): continue
        if not (ROOT/target.split('#')[0]).exists(): raise ValueError('Missing linked artifact: '+target)
    source.write_text(text,encoding='utf-8')
    output.parent.mkdir(parents=True,exist_ok=True)
    # Use a separate exported copy. Repository Markdown retains portable relative links.
    # Embedding assets avoids a <base> element, which would also redirect #chapter links.
    exported=browser_document(text,public_links=True)
    output.write_text(exported,encoding='utf-8')
    print(json.dumps({'source_excerpts':text.count('<!-- CODE:'),'chapters':text.count('class="chapter"'),
                      'utf8_bytes':len(text.encode()),'html':str(output)},ensure_ascii=True))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'tmp/report/PRESENTATION.html')
    a=p.parse_args();build(a.output)
