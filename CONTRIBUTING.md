# Contributing

Use C++20, the repository clang-format style and Conventional Commits. Keep the core
independent of Qt and platform UI APIs. Add meaningful parser/detector regression
checks and run CTest for behavior changes. Update README and RUNBOOK in the same
commit as operational changes. Keep captures, build output, credentials and private
network data outside Git. Describe tested environments and limitations accurately.
Do not use protocol-decoding libraries or train rules on hardcoded dataset identities.

Format with `clang-format -i` on changed C++ files. Use `cmake --preset sanitize`
with Clang on Linux/macOS for sanitizers, and `MININIDS_BUILD_FUZZERS=ON` for fuzzing.
