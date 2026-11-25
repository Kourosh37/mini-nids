"""Independent TShark observations for manual annotation, never detector-derived truth."""
import argparse
import collections
import csv
import json
from pathlib import Path
import subprocess

FIELDS = ["frame.number", "frame.time_epoch", "frame.len", "frame.cap_len", "eth.src", "eth.dst",
          "ip.src", "ip.dst", "ip.proto", "ip.ttl", "tcp.srcport", "tcp.dstport", "tcp.flags",
          "tcp.seq_raw", "tcp.ack_raw", "tcp.len", "tcp.stream", "udp.srcport", "udp.dstport",
          "icmp.type", "icmp.code", "arp.opcode", "arp.src.proto_ipv4", "arp.dst.proto_ipv4",
          "arp.src.hw_mac", "dns.flags.response", "dns.id", "dns.qry.name", "dns.a", "dns.cname",
          "dns.response_to", "http.request.method", "http.request.uri"]

def inspect(tshark, capture, output):
    output.mkdir(parents=True, exist_ok=False)
    command = [str(tshark), "-n", "-r", str(capture), "-o", "tcp.desegment_tcp_streams:FALSE",
               "-o", "tcp.analyze_sequence_numbers:FALSE", "-T", "fields", "-E", "separator=/t",
               "-E", "occurrence=f"]
    for field in FIELDS:
        command += ["-e", field]
    seconds = collections.defaultdict(collections.Counter)
    sources, destinations, protocols = (collections.Counter() for _ in range(3))
    arps, syns, web = {}, {}, {}
    count = truncated = 0
    first = last = None
    with (output / "tshark-errors.txt").open("w") as errors, (output / "dns.tsv").open("w", newline="", encoding="utf-8") as dns_file:
        dns_writer = csv.writer(dns_file, delimiter="\t")
        dns_writer.writerow(FIELDS)
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=errors,
                                   text=True, encoding="utf-8", errors="strict")
        for values in csv.reader(process.stdout, delimiter="\t", quoting=csv.QUOTE_NONE):
            values += [""] * (len(FIELDS) - len(values))
            row = dict(zip(FIELDS, values))
            number = int(row["frame.number"])
            stamp = row["frame.time_epoch"]
            sec = int(stamp.split(".")[0])
            first = stamp if first is None else first
            last = stamp
            count += 1
            truncated += int(row["frame.cap_len"]) < int(row["frame.len"])
            bucket = seconds[sec]
            bucket["packets"] += 1
            if row["ip.src"]:
                sources[row["ip.src"]] += 1
                destinations[row["ip.dst"]] += 1
            protocol = "ARP" if row["arp.opcode"] else "DNS" if row["dns.id"] else {"6":"TCP","17":"UDP","1":"ICMP"}.get(row["ip.proto"], row["ip.proto"] or "Other")
            protocols[protocol] += 1
            if row["arp.opcode"]:
                key = "/".join(row[k] for k in ("arp.opcode","arp.src.proto_ipv4","arp.src.hw_mac","arp.dst.proto_ipv4"))
                item = arps.setdefault(key, {"count":0,"first":stamp,"first_packet":number,"samples":[]})
                item["count"] += 1; item["last"] = stamp; item["last_packet"] = number
                if len(item["samples"]) < 5: item["samples"].append(number)
                bucket["arp_reply"] += row["arp.opcode"] == "2"
            if row["tcp.flags"]:
                flags = int(row["tcp.flags"], 16)
                if flags & 2 and not flags & 16:
                    key = row["ip.src"] + "/" + row["ip.dst"]
                    item = syns.setdefault(key, {"count":0,"ports":set(),"first":stamp,"first_packet":number,"seconds":collections.Counter()})
                    item["count"] += 1; item["ports"].add(row["tcp.dstport"])
                    item["last"] = stamp; item["last_packet"] = number; item["seconds"][sec] += 1
                    bucket["syn"] += 1
                bucket["syn_ack"] += bool(flags & 2 and flags & 16)
                bucket["rst"] += bool(flags & 4)
                if row["tcp.dstport"] == "80" and int(row["tcp.len"] or 0):
                    key = row["ip.src"] + "/" + row["ip.dst"]
                    item = web.setdefault(key, {"packets":0,"bytes":0,"first":stamp,"first_packet":number,"streams":set(),"methods":collections.Counter()})
                    item["packets"] += 1; item["bytes"] += int(row["tcp.len"])
                    item["last"] = stamp; item["last_packet"] = number; item["streams"].add(row["tcp.stream"])
                    if row["http.request.method"]: item["methods"][row["http.request.method"]] += 1
            if row["dns.id"]:
                dns_writer.writerow(values)
                bucket["dns_query" if row["dns.flags.response"].lower() in ("0", "false") else "dns_response"] += 1
        process.stdout.close()
        if process.wait() != 0:
            raise RuntimeError("TShark failed; inspect its error log")
    for item in syns.values():
        item["distinct_ports"] = len(item["ports"])
        item["ports"] = sorted(int(p) for p in item["ports"])
        item["peak_syn_per_second"] = max(item["seconds"].values())
        item["seconds"] = dict(sorted(item["seconds"].items()))
    for item in web.values():
        item["distinct_streams"] = len(item.pop("streams"))
    result = {"tool":"TShark independent dissection; not attack labels", "capture":str(capture),
              "packets":count,"truncated":truncated,"first":first,"last":last,
              "sources":sources.most_common(25),"destinations":destinations.most_common(25),
              "protocols":protocols,"arp_observations":arps,
              "tcp_syn_pairs":dict(sorted(syns.items(),key=lambda pair:pair[1]["count"],reverse=True)),
              "web_clients":web}
    (output / "observations.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    with (output / "seconds.csv").open("w",newline="",encoding="utf-8") as f:
        writer=csv.writer(f); keys=["packets","syn","syn_ack","rst","arp_reply","dns_query","dns_response"]
        writer.writerow(["unix_second"]+keys)
        for sec, counts in sorted(seconds.items()): writer.writerow([sec]+[counts[k] for k in keys])
    web_summary = sorted(web.items(), key=lambda pair: pair[1]["distinct_streams"], reverse=True)[:5]
    print(json.dumps({"file":capture.name,"packets":count,"protocols":protocols,
                      "top_syn_pairs":[(k,v["count"],v["distinct_ports"]) for k,v in list(result["tcp_syn_pairs"].items())[:5]],
                      "top_web_clients":web_summary}), flush=True)

if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tshark",type=Path,required=True)
    parser.add_argument("--input",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    inspect(args.tshark,args.input,args.output)
