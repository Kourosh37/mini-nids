"""Independently re-dissect every FP trigger; alert indices locate evidence, not truth."""
import argparse
import csv
from pathlib import Path
from packet_evidence import extract, FIELDS

def verify(tshark,captures,root):
    with (root/'alert-review.csv').open(newline='',encoding='utf-8') as f:
        rows=[r for r in csv.DictReader(f) if r['classification']=='FP']
    output=[]
    for name in sorted({r['capture'] for r in rows}):
        selected=[r for r in rows if r['capture']==name]
        expression=' || '.join('frame.number == '+r['packet_index'] for r in selected)
        packets={p['frame.number']:p for p in extract(tshark,captures/(name+'.pcap'),expression)}
        for alert in selected:
            packet=packets[alert['packet_index']]
            assert packet['eth.src']==alert['source_mac']
            if alert['rule_id']=='DNS-02':
                assert packet['ip.src']==alert['src'] and packet['ip.dst']==alert['dst']
                assert packet['dns.qry.name'] and packet['eth.src']=='3c:18:a0:41:c3:a0'
            elif alert['rule_id']=='SCAN-02':
                forward=packet['ip.src']==alert['src'] and packet['ip.dst']==alert['dst']
                quoted=packet['icmp.type']=='3' and packet['icmp.code']=='3'
                reverse=packet['ip.src']==alert['dst'] and packet['ip.dst']==alert['src']
                assert forward or quoted or reverse, (alert['packet_index'],packet['ip.src'],packet['ip.dst'])
                assert packet['udp.dstport'] or quoted
                assert not packet['dns.qry.name']
            output.append({'capture':name,'group_id':alert['group_id'],'rule_id':alert['rule_id'],**packet})
    with (root/'packet-evidence/false-alert-triggers.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=['capture','group_id','rule_id']+FIELDS)
        writer.writeheader();writer.writerows(output)
    print('Independently verified FP trigger packets:',len(output))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tshark',type=Path,required=True)
    p.add_argument('--captures',type=Path,default=Path('data/pcaps'));p.add_argument('--root',type=Path,default=Path('docs/academic'))
    a=p.parse_args();verify(a.tshark,a.captures,a.root)
