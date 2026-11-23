# Milestone trace

| Milestone | Commit | Implemented / files | Executed checks and result | Security / performance / limitation | Next milestone |
|---|---|---|---|---|---|
| Foundation | 7fcf75b | CMake, core types, CLI version, README/RUNBOOK | Windows GCC 15.2 build; 2 tests pass | Checked byte reads; no parsing yet | Streaming parsing |
| Protocols | 1af9eb9 | PcapReader, manual dispatcher/DNS, parser tests/docs | 12 tests pass | Bounded buffers, snaplen handling and DNS pointer checks; no reassembly | State |
| Stateful engine | 5a6dad7 | FlowTracker, StateTable, TrafficWindow, config; README/RUNBOOK | 18 tests pass | One expiration node per entry; bounded attempts and capacity drops | Detection |
| Detection | 33e973d | ARP/scan/SYN/DNS rules, protocol context, rule docs | 26 tests pass | Multiple evidence signals, benign cases, bounded alert aggregation | Reports/CLI |
| Reporting | 78b3760 | CLI analysis, ReportWriter, Evaluator, synthetic constructors | 33 tests pass; 155-packet end-to-end run | CSV escaping, redaction, output budgets, deterministic evidence; academic data absent | State hardening/GUI |
| State fixes | 69bd662 | Tuple reuse, JSON depth/limits, native Unicode arguments | 35 tests pass | New-ISN handling and safe timestamp range; no new dependency | Qt workspace |
| Qt workspace | 022bcbc | Controller/thread, bounded models, nine QML sections, Persian catalog/screenshots | Matching Qt/MinGW kit passes 37 checks; renders inspected | Batched snapshots, bounded on-demand hex; full localization pending | Hardening/metrics |
| Metrics and contracts | 2063854 | Size/TTL/flag/endpoint distributions, parser/API defenses, formatted C++ | Windows 37 core / 39 Qt checks; Linux 37 sanitizer checks | Bounded top endpoints and time buckets; actual gaps preserved | Fuzz/benchmark/CI |
| Hardening | 2490868 | Five fuzz targets, benchmark suite, CI, security/dependency docs | 100000 seeded fuzz runs; static and format checks; measured benchmarks | SHA-256 dependency locks; short fuzz campaign only, no dataset accuracy claim | Package validation |
| Package fix | see git log | Deployment helper and RUNBOOK | Script and system-PATH CLI/Qt smoke tests pass | Native warnings retained; exit code determines failure | Engineering delivery |

README and RUNBOOK were updated with the relevant implementation commits. Validation
outputs document actual environments and results. The remaining academic milestone
requires real captures/independent annotations; optional native capture/reassembly and
unverified platform integrations are explicitly listed instead of empty implementations.

Follow-up: at the user's request, removed the language catalog, translator, language
setting and direction switching. The current UI is English-only, with embedded Inter
and JetBrains Mono fonts. Windows controller/QML checks remain 39/39 passing; updated
desktop, settings, packet and compact renders were inspected. Build commands for each
desktop platform and mobile limitations are documented in docs/building.md.
