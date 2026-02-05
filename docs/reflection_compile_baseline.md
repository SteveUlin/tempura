# Reflection Compile-Time Baseline

Baseline measurements for the P2996 reflection migration.
Run with `tools/measure_reflection_compile.sh` from the `nix develop .#reflection` shell.

## Context

- **Compiler**: Clang P2996 (Bloomberg fork, rev 2ea0a79fe7bb5)
- **libstdc++**: GCC 15.1.0
- **Build type**: Release with `-ftime-trace`
- **LTO**: ON

From `docs/compilation_optimization_guide.md`: 71% of compile time is **codegen**,
not template instantiation. Reflection targets the other 29%. Set expectations
accordingly.

## Baseline (Phase 0 — before any reflection changes)

| Target | Wall Time (s) | Frontend (ms) | Instantiation (ms) | Codegen (ms) |
|--------|---------------|---------------|---------------------|--------------|
| symbolic4_core_test | — | — | — | — |
| symbolic4_compressed_test | — | — | — | — |
| symbolic4_eval_test | — | — | — | — |
| symbolic4_wrappers_test | — | — | — | — |
| symbolic4_model_test | — | — | — | — |

> Fill in after running: `./tools/measure_reflection_compile.sh`

## Progress

| Phase | Description | Status | Notes |
|-------|-------------|--------|-------|
| 0 | Build infrastructure | 🔧 In progress | Toolchain, presets, Clang compat |
| 1 | meta/ TypeList → reflection | Pending | |
| 2 | core.h type traits → reflection | Pending | |
| 3 | SymbolicState lookup → reflection | Pending | |
| 4 | Distribution wrappers → CRTP | Pending | |
