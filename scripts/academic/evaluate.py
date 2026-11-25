"""Reproduce per-file reports, reviewed-interval metrics and scan threshold sensitivity."""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import time

def rows(path):
    with path.open(newline="",encoding="utf-8") as f:
        return list(csv.DictReader(f))

def evaluate(executable,captures,references,output,observations,thresholds):
    output.mkdir(parents=True,exist_ok=False)
    reviewed=json.loads(observations.read_text(encoding="utf-8"))
    names=[c["name"] for c in reviewed["captures"]]
    for capture in reviewed["captures"]:
        path=captures/(capture["name"]+".pcap")
        digest=hashlib.sha256()
        with path.open("rb") as f:
            for chunk in iter(lambda:f.read(1024*1024),b""): digest.update(chunk)
        digest=digest.hexdigest()
        if digest!=capture["sha256"]: raise ValueError("Capture differs from reviewed reference: "+path.name)
    results=[]
    for threshold in thresholds:
        settings=output/f"scan-{threshold}.json"
        settings.write_text(json.dumps({"scan_ports":threshold})+"\n")
        for name in names:
            destination=output/("default" if threshold==20 else f"scan-{threshold}")/name
            destination.parent.mkdir(exist_ok=True)
            command=[str(executable.resolve()),"analyze","--input",str((captures/(name+".pcap")).resolve()),
                     "--output",str(destination.resolve()),"--ground-truth",str((references/(name+".json")).resolve()),
                     "--config",str(settings.resolve())]
            started=time.perf_counter()
            completed=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding="utf-8")
            wall=time.perf_counter()-started
            if completed.returncode: raise RuntimeError(f"Analysis failed: {name}: {completed.stderr} {completed.stdout}")
            data=json.loads(completed.stdout)
            summary=data["capture"]; metrics=data["evaluation"]
            assert summary["status"]=="complete" and metrics["status"]=="evaluated"
            alerts=rows(destination/"alerts.csv")
            with (destination/"flows.csv").open(newline="",encoding="utf-8") as f:
                count=0
                for flow in csv.DictReader(f):
                    count+=1
                    assert int(flow["forward_bytes"])+int(flow["reverse_bytes"])==int(flow["bytes"])
                    assert int(flow["forward_captured_bytes"])+int(flow["reverse_captured_bytes"])==int(flow["captured_bytes"])
            assert count==summary["flows"]
            seconds=rows(destination/"traffic_per_second.csv")
            assert sum(int(r["packets"]) for r in seconds)==summary["packets"]
            assert len(seconds)==summary["exact_timeline_buckets"]
            assert len(alerts)==summary["alerts"]
            by_rule=collections.Counter(a["rule_id"] for a in alerts)
            assert dict(by_rule)==(metrics["alerts_by_rule"] or {})
            journal=rows(destination/"alert_events.csv")
            assert len(journal)==len(alerts)
            result={"capture":name,"scan_ports":threshold,"packets":summary["packets"],"flows":count,
                    "dns_rows":len(rows(destination/"dns_log.csv")),"http_rows":len(rows(destination/"http_log.csv")),
                    "alerts":len(alerts),"alerts_by_rule":dict(by_rule),"metrics":metrics,
                    "state_drops":summary["state_drops"],"clock_regressions":summary["clock_regressions"],
                    "malformed":summary["malformed"],"truncated":summary["truncated"],
                    "engine_seconds":data["performance"]["elapsed_seconds"],"wall_seconds":wall,
                    "packets_per_second":data["performance"]["packets_per_second"],
                    "peak_memory_bytes":data["performance"]["peak_memory_bytes"],"exact_seconds":len(seconds)}
            results.append(result)
            (output/"results.json").write_text(json.dumps({"platform":platform.platform(),"method":"single warm-cache CLI runs with CSV enabled; reviewed observational campaign intervals; not external certified ground truth", "results":results},indent=2)+"\n",encoding="utf-8")
            print(json.dumps({"file":name,"scan_ports":threshold,"alerts":dict(by_rule),"tp":metrics["tp"],"fp":metrics["fp"],"fn":metrics["fn"],"state_drops":summary["state_drops"]}),flush=True)
    return results

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--executable",type=Path,required=True)
    p.add_argument("--captures",type=Path,default=Path("data/pcaps"))
    p.add_argument("--references",type=Path,default=Path("data/ground-truth"))
    p.add_argument("--observations",type=Path,default=Path("docs/academic/reviewed-observations.json"))
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--thresholds",nargs="+",type=int,default=[10,20,40,1000])
    a=p.parse_args()
    if len(set(a.thresholds))!=len(a.thresholds) or any(t<2 or t>65535 for t in a.thresholds): p.error("Use distinct thresholds in 2..65535")
    evaluate(a.executable,a.captures,a.references,a.output,a.observations,a.thresholds)
