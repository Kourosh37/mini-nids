# Mini-NIDS runbook

## Environment

Use CMake >=3.25, Ninja and GCC/Clang with C++20 or a recent MSVC toolset.
From a compiler-enabled terminal run `cmake --preset dev`, `cmake --build --preset dev`
and `ctest --preset dev`. Windows executables are under `build/dev/apps/cli`.
`mininids --version` verifies the foundation CLI starts.

Verified foundation environment: Windows, GCC 15.2, CMake 4.4.5, Ninja 1.13.2;
33/33 GoogleTest/CTest checks passed on 2026-11-23. Qt 6.8.3 is installed locally.

The source fallback needs HTTPS access to GitHub. Offline use requires installed
packages and `-DMININIDS_FETCH_DEPS=OFF`. A working vcpkg checkout is required for
the vcpkg preset. The preinstalled machine vcpkg has invalid Git metadata, so it
cannot currently provide a reproducible manifest build.

## Safety and recovery

Offline analysis requires no elevated privileges. Keep captures local and outside Git.
Only use authorized captures and isolated networks for optional future replay work.
Build failures do not modify captures. Remove the selected `build/<preset>` directory
and reconfigure to recover from stale compiler configuration.

## Capture policy

Only classic PCAP 2.4 with Ethernet link type 1 is accepted. Maximum snaplen is
262144 bytes. Invalid record structure stops analysis with an error; malformed
individual frames are counted and skipped. Unsupported EtherTypes are counted.
Non-initial IPv4 fragments are not transport decoded. Initial fragments may expose
transport headers, but fragmented DNS is not decoded without reassembly. Snaplen
truncation does not invalidate intact transport headers. TCP DNS reassembly and
PCAPNG are not implemented.

## Pending operations

State limits reject new entries when full and count those drops. Idle and active
expiration use one ordered timer per entry. Capture timestamps drive state;
regressing timestamps use a nondecreasing watermark for expiration and windows,
while original packet timestamps remain available in evidence. Count regressions
when assessing a capture's time ordering.

Run `build/dev/tests/mininids_fixtures tmp/demo` to generate explicit engineering
fixtures (create the `tmp` parent first). Then run:

```sh
build/dev/apps/cli/mininids analyze --input tmp/demo/engineering-demo.pcap --output results/demo-001 --ground-truth tmp/demo/ground-truth.json
```

Use a new output path for every run; existing directories are refused. The session
marker remains `incomplete` if reading or writing fails. Ctrl+C requests cancellation;
partial reports are marked cancelled and exit code is 130. Successful exit is 0,
input/output errors 1, usage/config errors 2. A report byte budget prevents disk
exhaustion. `redact: true` hides addresses, names and identity-bearing evidence in
exports. This reduces diagnostic detail and does not modify source captures.

The GUI is in progress. Missing live/mobile tooling and assignment datasets are
tracked below.
Live capture and mobile deployment require separate platform tooling and permissions.
The four required student captures and independently annotated ground truth have not
been supplied; final academic evaluation remains blocked by those inputs.
