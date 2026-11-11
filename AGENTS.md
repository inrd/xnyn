# Agent instructions

xnyn is a header-only, dependency-free C++20 DSP library focused on per-sample
performance.

- Read and follow `CONVENTIONS.md` before adding or changing any block.
- Every new or changed block needs tests; performance-sensitive blocks need a benchmark.
- Every new or changed block needs its doc page in `docs/` updated, plus the block
  tables in `docs/README.md` and `README.md` (see "Documentation" in `CONVENTIONS.md`).
- Before finishing, build and run the tests:
  ```sh
  cmake --preset debug && cmake --build --preset debug && ctest --preset debug
  ```
- Run benchmarks from the release build when performance is affected:
  ```sh
  cmake --preset release && cmake --build --preset release && ./build/release/benchmarks/xnyn_benchmarks
  ```
- Format changed files with `clang-format -i`.
- Never add dependencies.
