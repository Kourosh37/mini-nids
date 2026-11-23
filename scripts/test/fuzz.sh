#!/usr/bin/env sh
set -eu
build_dir=${1:-build/linux-sanitize}
corpus_dir=${2:-tmp/fuzz-corpus}
log_dir=${3:-tmp/fuzz-logs}
mkdir -p "$corpus_dir"
mkdir -p "$log_dir"
for target in Pcap Dns Ipv4 Tcp Dispatcher; do
    mkdir -p "$corpus_dir/$target"
    "$build_dir/fuzz/fuzz_$target" "$corpus_dir/$target" -runs=20000 -seed=1337 -max_len=262144 > "$log_dir/fuzz-$target.txt" 2>&1
    tail -2 "$log_dir/fuzz-$target.txt"
done
