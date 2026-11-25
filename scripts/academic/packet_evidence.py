"""Export small, reproducible independent TShark samples and aggregate TCP evidence."""
import argparse
import collections
import csv
import json
from pathlib import Path
import subprocess

FIELDS = ["frame.number", "frame.time_epoch", "eth.src", "eth.dst", "ip.src", "ip.dst",
          "tcp.stream", "tcp.srcport", "tcp.dstport", "tcp.flags", "tcp.seq_raw",
          "tcp.ack_raw", "tcp.len", "tcp.payload", "udp.srcport", "udp.dstport",
          "dns.id", "dns.qry.name", "dns.a", "arp.opcode", "arp.src.proto_ipv4",
          "arp.dst.proto_ipv4", "arp.src.hw_mac", "frame.len", "icmp.type", "icmp.code"]

def extract(tshark, capture, display_filter):
    command = [str(tshark), "-n", "-r", str(capture), "-o", "tcp.desegment_tcp_streams:FALSE",
               "-Y", display_filter, "-T", "fields", "-E", "separator=/t", "-E", "occurrence=f"]
    for name in FIELDS:
        command += ["-e", name]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, encoding="utf-8")
    for row in csv.reader(process.stdout, delimiter="\t", quoting=csv.QUOTE_NONE):
        yield dict(zip(FIELDS, row))
    process.stdout.close()
    errors = process.stderr.read()
    if process.wait():
        raise RuntimeError(errors)

def collect(tshark, captures, output):
    output.mkdir(parents=True, exist_ok=True)
    samples = {
        "benign-start": ("benign1", "frame.number <= 180"),
        "benign-connection": ("benign1", "tcp.stream == 8 || (arp && (arp.src.proto_ipv4 == 192.168.137.41 || arp.dst.proto_ipv4 == 192.168.137.41))"),
        "benign-walkthrough": ("benign1", "tcp.stream == 49 || frame.number == 1586 || frame.number == 1589 || frame.number == 1650 || frame.number == 1658 || frame.number == 1958 || frame.number == 1960"),
        "dns-chain": ("DNSspoofing-stu", "frame.number == 1660 || frame.number == 5395 || frame.number == 5497 || (ip.src == 192.168.137.49 && ip.dst == 192.168.137.35 && tcp.flags.syn == 1)"),
        "slow-stream": ("DoS-slowloris-stu", "tcp.stream == 2754"),
        "slow-trickle": ("DoS-slowloris-stu", "tcp.port == 54018 && ip.addr == 192.168.10.50"),
        "arp-context": ("ARPspoofing-stu", "arp && frame.number <= 3400"),
        "syn-attack": ("DoS-SYN-stu", "tcp && frame.time_epoch >= 1665163502.374618 && ((ip.src == 192.168.137.182 && ip.dst == 192.168.137.187) || (ip.src == 192.168.137.187 && ip.dst == 192.168.137.182))"),
        "scan-tcp": ("portScan-stu", "tcp && ((ip.src == 172.16.0.1 && ip.dst == 192.168.10.50) || (ip.src == 192.168.10.50 && ip.dst == 172.16.0.1))"),
    }
    summary = {}
    for name, (capture, expression) in samples.items():
        source = extract(tshark, captures/(capture+".pcap"), expression)
        rows = []
        if name in ("syn-attack", "scan-tcp"):
            flags, sizes, directions, payloads = (collections.Counter() for _ in range(4))
            streams = collections.defaultdict(list)
            count = 0
            for r in source:
                count += 1
                flags[r["tcp.flags"]] += 1; sizes[r["frame.len"]] += 1
                directions[r["ip.src"]] += 1; payloads[r["tcp.len"]] += 1
                if len(rows) < 24: rows.append(r)
                if name == "scan-tcp" and len(streams[r["tcp.stream"]]) < 12:
                    streams[r["tcp.stream"]].append(r)
            details = {"packets":count, "flags":dict(flags), "wire_sizes":dict(sizes),
                       "directions":dict(directions), "payload_lengths":dict(payloads)}
            if name == "scan-tcp":
                outcomes = collections.Counter()
                representatives = {}
                for stream, packets in streams.items():
                    initiated = any(r["ip.src"] == "172.16.0.1" and int(r["tcp.flags"],16)&0x12 == 2 for r in packets)
                    if not initiated:
                        continue
                    synack = next((i for i,r in enumerate(packets) if r["ip.src"] == "192.168.10.50" and int(r["tcp.flags"],16)&0x12 == 0x12), None)
                    mode = "closed-or-no-open-response"
                    if synack is not None:
                        mode = "open-partial"
                        for r in packets[synack+1:]:
                            if r["ip.src"] != "172.16.0.1":
                                continue
                            flags_value = int(r["tcp.flags"],16)
                            if flags_value&4:
                                mode="SYN-open-reset"; break
                            if flags_value&16 and not flags_value&2:
                                mode="Connect-open-ack"; break
                    outcomes[mode] += 1
                    representatives.setdefault(mode, packets[:12])
                details["observed_stream_outcomes"] = dict(outcomes)
                details["representatives"] = representatives
            summary[name] = details
            rows = rows[:24]
        else:
            rows = list(source)
            summary[name] = {"packets":len(rows)}
        with (output/(name+".csv")).open("w",newline="",encoding="utf-8") as f:
            writer=csv.DictWriter(f,fieldnames=FIELDS); writer.writeheader(); writer.writerows(rows)
        print(name, summary[name]["packets"], flush=True)
    (output/"packet-evidence.json").write_text(json.dumps(summary,indent=2)+"\n",encoding="utf-8")

if __name__ == "__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--tshark",type=Path,required=True)
    p.add_argument("--captures",type=Path,default=Path("data/pcaps"))
    p.add_argument("--output",type=Path,default=Path("docs/academic/packet-evidence"))
    a=p.parse_args(); collect(a.tshark,a.captures,a.output)
