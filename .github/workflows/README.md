# .github/workflows/

GitHub Actions workflows. `stress.yml` runs the offline DSP stress suite on Linux
(ASan/UBSan/LSan, cppcheck, clang-tidy, libFuzzer) — manual-only
(`workflow_dispatch`), not on every push. See `tests/stress/`.
