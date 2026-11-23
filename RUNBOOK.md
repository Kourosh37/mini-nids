# Mini-NIDS runbook

## Environment

Use CMake >=3.25, Ninja and GCC/Clang with C++20 or a recent MSVC toolset.
From a compiler-enabled terminal run `cmake --preset dev`, `cmake --build --preset dev`
and `ctest --preset dev`. Windows executables are under `build/dev/apps/cli`.
`mininids --version` verifies the foundation CLI starts.

Verified foundation environment: Windows, GCC 15.2, CMake 4.4.5, Ninja 1.13.2;
12/12 GoogleTest checks passed on 2026-11-23. Qt is being installed separately.

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

PCAP analysis, reports, evaluation and the GUI are not present in the foundation build.
Live capture and mobile deployment require separate platform tooling and permissions.
The four required student captures and independently annotated ground truth have not
been supplied; final academic evaluation remains blocked by those inputs.
