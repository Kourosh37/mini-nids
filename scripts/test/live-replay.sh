#!/usr/bin/env bash
set -euo pipefail
# Both veth ends stay in one isolated namespace. No route/interface reaches a real LAN.
[[ $(uname -s) == Linux && $EUID == 0 ]] || { echo 'Run as root on Linux for an isolated veth test' >&2; exit 2; }
build=$(cd "${1:-build/release}" && pwd)
output=${2:-tmp/live-replay}
[[ ! -e $output ]] || { echo 'Choose a new output directory' >&2; exit 2; }
mkdir -p "$output"
output=$(cd "$output" && pwd)
namespace="mini-nids-live-$$"
capture_pid=
cleanup() {
  if [[ -n $capture_pid ]]; then kill "$capture_pid" 2>/dev/null || true; wait "$capture_pid" 2>/dev/null || true; fi
  ip netns del "$namespace" 2>/dev/null || true
}
trap cleanup EXIT INT TERM
ip netns add "$namespace"
ip netns exec "$namespace" sysctl -qw net.ipv6.conf.all.disable_ipv6=1
ip -n "$namespace" link add nids-tx type veth peer name nids-rx
ip -n "$namespace" link set nids-tx address 02:00:00:00:00:01 up
ip -n "$namespace" link set nids-rx address 02:00:00:00:00:02 up
"$build/tests/mininids_fixtures" "$output/fixtures"
for profile in slow fast overload; do
  seconds=5
  input="$output/fixtures/engineering-demo.pcap"
  replay=(--pps=100)
  if [[ $profile == fast ]]; then replay=(--pps=10000); fi
  if [[ $profile == overload ]]; then input="$output/fixtures/engineering-load.pcap"; replay=(--topspeed); fi
  ip netns exec "$namespace" "$build/apps/cli/mininids" live --interface nids-rx \
    --duration "$seconds" --output "$output/$profile" >"$output/$profile-summary.json" 2>"$output/$profile-alerts.txt" &
  capture_pid=$!
  for attempt in $(seq 1 100); do
    [[ -f $output/$profile/alert_events.csv ]] && break
    kill -0 "$capture_pid"
    sleep 0.05
  done
  [[ -f $output/$profile/alert_events.csv ]]
  date -u +%s.%N > "$output/$profile-replay-start.txt"
  ip netns exec "$namespace" tcpreplay --quiet -i nids-tx "${replay[@]}" "$input" > "$output/$profile-replay.txt"
  wait "$capture_pid"
  capture_pid=
done
python3 - "$output" <<'PY'
import csv,json,pathlib,statistics,sys
root=pathlib.Path(sys.argv[1]);results=[]
for profile in ('slow','fast','overload'):
    data=json.loads((root/(profile+'-summary.json')).read_text())
    capture=data['capture'];stats=capture['source_statistics']
    assert capture['status']=='complete' and stats['live'] and stats['available']
    assert capture['packets']>0
    with (root/profile/'alert_events.csv').open(newline='') as f: alerts=list(csv.DictReader(f))
    ids={a['rule_id'] for a in alerts}
    if profile!='overload': assert {'ARP-01','SCAN-01','SYN-01','DNS-01'}<=ids,ids
    latency=sorted(int(a['delivery_latency_ns']) for a in alerts if a['delivery_latency_ns'])
    results.append({'profile':profile,'packets':capture['packets'],'driver_statistics':stats,
        'engine_state_drops':capture['state_drops'],'packets_per_second':data['performance']['packets_per_second'],
        'median_capture_to_alert_ns':statistics.median(latency) if latency else None,
        'p95_capture_to_alert_ns':latency[min(len(latency)-1,int(len(latency)*.95))] if latency else None,
        'rules':sorted(ids)})
(root/'results.json').write_text(json.dumps({'workload':'synthetic isolated veth; not academic dataset accuracy',
    'note':'Capture loss, engine state pressure and discovery delay are different; pcap counters vary by platform.',
    'results':results},indent=2)+'\n')
print(json.dumps(results,indent=2))
PY
