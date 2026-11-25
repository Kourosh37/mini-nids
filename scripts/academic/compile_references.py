"""Compile reviewed decimal capture-time observations into exact evaluator intervals."""
import argparse
from decimal import Decimal
import json
from pathlib import Path

def compile_references(source, output):
    data=json.loads(source.read_text(encoding="utf-8"))
    output.mkdir(parents=True,exist_ok=True)
    for capture in data["captures"]:
        events=[]
        for event in capture["events"]:
            row={"id":event["id"],"attack":event["attack"],
                 "start_ns":int(Decimal(event["start"])*1_000_000_000),
                 "end_ns":int(Decimal(event["end"])*1_000_000_000)}
            for key in ("src","dst","src_mac"):
                if event.get(key): row[key]=event[key]
            if row["end_ns"]<row["start_ns"]: raise ValueError("Reference interval ends before it starts")
            events.append(row)
        (output/(capture["name"]+".json")).write_text(json.dumps(events,indent=2)+"\n",encoding="utf-8")

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path,default=Path("docs/academic/reviewed-observations.json"))
    parser.add_argument("--output",type=Path,default=Path("data/ground-truth"))
    args=parser.parse_args()
    compile_references(args.source,args.output)
