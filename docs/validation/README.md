# Recorded evidence

All results are local and dated 2026-11-23. No remote GitHub Actions run is claimed.

- Windows GCC 15.2 Debug and Release: 37/37 core/CLI checks.
- Windows Qt 6.8.3 with matching MinGW 13.1: 39/39 CTest checks, including a QtTest
  controller suite and offscreen QML navigation/theme/compact/RTL smoke test.
- Linux Ubuntu 24.04 WSL / Clang 18.1.3 / CMake 3.28.3 / Ninja 1.11.1: 37/37 core/CLI
  checks under ASan+UBSan. Windows tools: CMake 4.4.5, Ninja 1.13.2.
- JUnit evidence: `build/dev/final-results.xml`, `build/release/final-results.xml`,
  `build/gui-qt/final-results.xml`, `build/linux-sanitize/final-results.xml` (build
  directories remain local and outside Git).
- Five fuzz targets completed 20000 runs each with seed 1337 and valid initial
  engineering seeds. Corpus mutations are local under tmp/fuzz-final. Logs are
  `fuzz-Pcap.txt`, `fuzz-Dns.txt`, `fuzz-Ipv4.txt`, `fuzz-Tcp.txt`, `fuzz-Dispatcher.txt`.
  No failure was reported in this bounded campaign; exhaustive coverage is not claimed.
- Clang-Tidy 18 analyzed ten core source files. Five optional-access warnings in the
  report-writer API led to an explicit missing-DNS guard. The focused follow-up
  `clang-tidy-reportwriter.txt` reports no project warning. The original scan log is
  preserved; large suppressed warning counts come from non-project headers.
- Clang-Format 23.1.5 dry-run passed on all owned C++ files.
- Unicode input path verified through the native Windows CLI.
- Deployed CLI version and Qt offscreen smoke tests passed with only Windows system
  directories on PATH. This is local dependency isolation, not a clean-machine test.
- Screenshots are actual Qt renders using the generated engineering demo, not artwork
  or product-embedded fake data. Desktop, light, compact and Persian were inspected.
- Synthetic summary/evaluation files describe the 155-packet construction only.
  Four constructed intervals and five rule groups yielded TP=4, FP=0, FN=0 in that
  engineering check. These are not real assignment accuracy estimates.
- Benchmarks and process sampling are described in ../performance.md.

macOS, Android, iOS, native live capture, a full accessibility audit, vulnerability
scanning and real student-dataset evaluation are unverified or not implemented.
