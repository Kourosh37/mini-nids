"""Build reproducible study tables, an alert adjudication ledger and baseline summaries."""
import argparse
import collections
import csv
from decimal import Decimal
import json
from pathlib import Path
import shutil
import statistics

RULES = ["ARP-01","ARP-02","SCAN-01","SCAN-02","SYN-01","DNS-01","DNS-02","DNS-03","SLOW-01"]

def read_csv(path):
    with path.open(newline="",encoding="utf-8") as f: return list(csv.DictReader(f))

def write_csv(path, rows, fields=None):
    if fields is None: fields=list(rows[0])
    with path.open("w",newline="",encoding="utf-8") as f:
        writer=csv.DictWriter(f,fieldnames=fields); writer.writeheader(); writer.writerows(rows)

def summarize(study, independent, destination):
    destination.mkdir(parents=True,exist_ok=True)
    data=json.loads((study/"results.json").read_text(encoding="utf-8"))
    reviewed=json.loads((destination/"reviewed-observations.json").read_text(encoding="utf-8"))
    results=data["results"]; defaults=[r for r in results if r["scan_ports"]==20]
    shutil.copyfile(study/"results.json",destination/"results.json")
    matrix=[]; performance=[]; delays=[]; ledger=[]; baseline=[]
    for r in defaults:
        name=r["capture"]; report=study/"default"/name
        matrix.append({"capture":name,**{rule:r["alerts_by_rule"].get(rule,0) for rule in RULES},
                       "TP":r["metrics"]["tp"],"FP":r["metrics"]["fp"],"FN":r["metrics"]["fn"]})
        performance.append({"capture":name,**{k:r[k] for k in ("packets","flows","dns_rows","http_rows","engine_seconds","wall_seconds","packets_per_second","peak_memory_bytes","state_drops","clock_regressions","malformed","truncated","exact_seconds")}})
        delays.extend({"capture":name,**event} for event in r["metrics"]["delays"])
        references=json.loads((Path("data/ground-truth")/(name+".json")).read_text(encoding="utf-8"))
        events=read_csv(report/"alert_events.csv")
        extended={e["group_id"]:e for e in read_csv(report/"alerts_extended.csv")}
        seen=set()
        for a in events:
            stamp=int(Decimal(a["timestamp"])*1_000_000_000)
            matching=[e for e in references if e["attack"]==a["attack"] and e["start_ns"]<=stamp<=e["end_ns"]
                      and all(not e.get(k) or e[k]==a[v] for k,v in (("src","src"),("dst","dst"),("src_mac","source_mac")))]
            match=min(matching,key=lambda x:x["start_ns"]) if matching else None
            label="FP" if match is None else "duplicate" if match["id"] in seen else "TP"
            if match: seen.add(match["id"])
            reason="Matches independently reviewed category/time/entities; later groups are repeated campaign evidence."
            if label=="FP":
                if a["rule_id"]=="SCAN-02":
                    reason="Outside the reviewed scan campaigns. Outer UDP from .42 to external high destination ports meets the port-count heuristic; no corroborated scan intent. Conditional reference FP; possible background or unlabelled secondary traffic, application unknown."
                elif a["rule_id"]=="DNS-02":
                    reason="Observed original gateway MAC 3c:18:a0:41:c3:a0; learned ARP history has drifted/expired during poisoning. The cross-layer heuristic mistakes a legitimate gateway response for a mapping mismatch. Not credited merely for occurring inside a DNS attack interval."
                else:
                    reason="No independently reviewed interval matches all category/time/entities; inspect packet and initial evidence."
            e=extended[a["group_id"]]
            ledger.append({"capture":name,"group_id":a["group_id"],"classification":label,
                           "reference":match["id"] if match else "", "packet_index":e["packet_index"],
                           "timestamp":a["timestamp"],"rule_id":a["rule_id"],"src":a["src"],"dst":a["dst"],
                           "source_mac":a["source_mac"],"occurrences":e["occurrences"],
                           "initial_evidence":a["evidence"],"review_reason":reason})
        assert sum(a["classification"]=="FP" for a in ledger if a["capture"]==name)==r["metrics"]["fp"]
        annotation=next(c for c in reviewed["captures"] if c["name"]==name)
        folder=independent/("slowloris" if name=="DoS-slowloris-stu" else name)
        observation=json.loads((folder/"observations.json").read_text(encoding="utf-8"))
        seconds=read_csv(folder/"seconds.csv")
        onset=min((Decimal(e["start"]) for e in annotation["events"]),default=None)
        first=int(Decimal(observation["first"]))+1
        end=min(first+60,int(onset)) if onset is not None else first+60
        sampled={int(s["unix_second"]):s for s in seconds}
        count=max(0,end-first)
        buckets=[int(sampled.get(i,{}).get("packets",0)) for i in range(first,end)]
        syn=[int(sampled.get(i,{}).get("syn",0)) for i in range(first,end)]
        baseline.append({"capture":name,"start_unix_second":first,"end_exclusive":end,"seconds":count,
                         "mean_packets_per_second":statistics.mean(buckets) if buckets else None,
                         "max_packets_per_second":max(buckets,default=0),
                         "mean_syn_per_second":statistics.mean(syn) if syn else None,
                         "max_syn_per_second":max(syn,default=0),
                         "policy":"First up to 60 complete seconds strictly before earliest reviewed onset; absent buckets are zero. Descriptive baseline, not automatically learned trust."})
    sensitivity=[]
    for threshold in sorted({r["scan_ports"] for r in results}):
        rows=[r for r in results if r["scan_ports"]==threshold]
        tp=sum(r["metrics"]["tp"] for r in rows); fp=sum(r["metrics"]["fp"] for r in rows); fn=sum(r["metrics"]["fn"] for r in rows)
        scan=[r["metrics"]["per_attack"]["Port Scan"] for r in rows]
        stp=sum(r["tp"] for r in scan); sfp=sum(r["fp"] for r in scan); sfn=sum(r["fn"] for r in scan)
        scan_delays=[e["delay_s"] for r in rows for e in r["metrics"]["delays"] if "scan-campaign" in e["event"]]
        sensitivity.append({"scan_ports":threshold,"TP":tp,"FP":fp,"FN":fn,"precision":tp/(tp+fp) if tp+fp else None,
                            "recall":tp/(tp+fn) if tp+fn else None,"scan_TP":stp,"scan_FP":sfp,"scan_FN":sfn,
                            "scan_precision":stp/(stp+sfp) if stp+sfp else None,"scan_recall":stp/(stp+sfn) if stp+sfn else None,
                            "mean_detected_scan_delay_s":statistics.mean(scan_delays) if scan_delays else None})
    for filename,rows in (("rule-file-matrix.csv",matrix),("performance.csv",performance),("detection-delays.csv",delays),
                          ("alert-review.csv",ledger),("baselines.csv",baseline),("sensitivity.csv",sensitivity)):
        write_csv(destination/filename,rows)
    print(json.dumps({"groups_reviewed":len(ledger),"false_groups":sum(a["classification"]=="FP" for a in ledger),
                      "default":next(r for r in sensitivity if r["scan_ports"]==20)},indent=2))

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--study",type=Path,required=True)
    p.add_argument("--independent",type=Path,default=Path("tmp/independent"))
    p.add_argument("--destination",type=Path,default=Path("docs/academic"))
    a=p.parse_args(); summarize(a.study,a.independent,a.destination)
