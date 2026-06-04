# tests/stress/

Headless C++ DSP stress harness (issue #17). Drives every voice's per-sample
`process()` against the Rack SDK headers only (no libRack, no GUI) and checks:
finite/bounded output, denormal flush, reset determinism, zero audio-thread
allocation, performance, and libFuzzer fuzzing.

- `make stress` — robustness + RT-safety + perf (ASan/UBSan)
- `make fuzz` — libFuzzer campaign (needs LLVM clang)

Report: `STRESS-REPORT.md`. CI: `../../.github/workflows/stress.yml`.
