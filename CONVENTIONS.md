# Conventions

Rules every block in xnyn follows. Performance comes first; readability is still
required, but never at the cost of per-sample speed.

## Scope

- Plain C++20 and the standard library only. No framework dependencies (JUCE, etc.).
- Header-only. Everything lives under `include/xnyn/`, in namespace `xnyn`.
- Building blocks only: framework glue (buffers, parameter plumbing, host
  integration) belongs in consumer code, not here.

## Layout

- One block per header: `include/xnyn/<category>/<block>.hpp`
  (e.g. `osc/phasor.hpp`). Categories: `osc`, `filter`, `delay`, `env`, `math`, `util`
  (add new ones only when a block clearly fits none of these).
- Every header has a matching test file: `tests/<category>/<block>_tests.cpp`.
- Performance-sensitive blocks have a benchmark: `benchmarks/<category>/<block>_bench.cpp`.
- Every header has a matching doc page: `docs/<category>/<block>.md`.
- New headers are added to `include/xnyn/xnyn.hpp`, new test and benchmark files to the
  matching `CMakeLists.txt`.

## API shape

- Blocks are minimal `class`es: private state with default member initializers
  (`trailing_` underscore), so invariants such as derived coefficients or
  filter kernel variables can't be broken from outside. Add `const` getters only
  for values callers actually need. No inheritance, no virtuals.
- Lifecycle:
  - `prepare(sampleRate)` stores the sample rate and caches what derives from it
    (e.g. `1 / sampleRate`, so per-sample code multiplies instead of divides).
    Parameters keep their physical values and are re-derived. It then calls
    `reset()`, so a prepared block is always in a known state (like a plugin's
    `prepareToPlay`, it is not meant to be called mid-stream).
  - `prepare` is the only place a block may allocate, and only blocks that need memory
    whose size depends on the sample rate (e.g. delay lines). Such a `prepare` takes
    its size in physical units (e.g. `maxDelaySeconds`), may throw `std::bad_alloc`
    (so it is not `noexcept`), reuses its memory when the size is unchanged, and is
    documented as not real-time safe. Copying is deleted; moving is allowed.
  - `reset(...)` restores a known state; it never allocates.
  - A default-constructed block is safe to use before `prepare`: it produces
    finite, neutral output (silence or pass-through), including after setters
    are called. Achieve this through default member values (e.g. identity
    coefficients, `invSampleRate_ = 0`), never through branches in `tick`.
- Parameters are set in physical units: Hz, seconds or milliseconds (state which in
  the name, e.g. `setTimeMs`), cycles for phase. Setters never take the sample rate.
- Setters apply immediately: blocks never smooth parameters internally. Smoothing
  is a separate block the caller chains in. Setters must be cheap enough to call
  every sample (for modulation); if one can't be, document its cost.
- Per-sample processing:
  - generators: `tick()` returns the current output, then advances;
  - processors: `tick(x)` returns `y`.
- Block overloads (`std::span`) only when they measurably beat a loop over the
  per-sample call.
- Names: `PascalCase` types, `camelCase` functions and members, `kPascalCase`
  constants, `snake_case` files.

## Performance

- Real-time safe: no allocation, exceptions, locks, I/O or virtual calls in
  `prepare`, `reset`, setters or processing functions. Mark them `noexcept`. The one
  exception is an allocating `prepare` (see Lifecycle).
- Processors (`tick(x)`) are templated on the sample type:
  `template <std::floating_point T = float>`, so `xnyn::Block b;` is `float`.
  Internal state may use higher precision where needed; say why in a comment.
- Generators are concrete and use the precision their state requires (e.g.
  `double` for phase accumulators); say why in a comment.
- Sample rates are always `double`.
- No branches on per-sample data when a branchless form exists (selects, `floor`,
  `min`/`max`, `copysign`).
- No `fmod`, `pow`, `exp`, `tan`, `tanh`, `sin` in per-sample paths when an
  approximation from `xnyn/math` with an adequate, documented error bound exists.
- Keep state small and contiguous, so blocks stay friendly to future multi-voice
  SIMD processing. No SIMD intrinsics for now.
- Denormals are the caller's responsibility (enable FTZ/DAZ on the audio thread).
  Blocks don't add per-sample anti-denormal tricks.
- Performance claims are backed by a benchmark, ideally against the naive baseline.

## Approximations

- Every approximation documents its valid input range and its maximum error
  (absolute or relative, stated explicitly).
- Its tests enforce that error bound over the full documented range.

## Tests

Each block's tests cover, as relevant:

1. Reference values: exact where possible (pick sample rates and frequencies that
   give exactly representable values, e.g. 1024 Hz sample rate).
2. Invariants: output ranges, stability, no drift over long runs.
3. Edge cases: zero, negative and extreme parameters; first sample after `prepare` / `reset`;
   neutral, finite output when used without `prepare`.
4. Error bounds for approximations.

Tests use Catch2 v3, tagged `[<category>][<block>]`. Benchmarks are tagged
`[benchmark][<category>][<block>]` and live in the separate benchmark executable.

## Comments

- Explain *why* (numerical traps, performance choices), not *what*.
- Each block opens with a short comment stating what it does, its units and ranges.

## Documentation

User docs are plain Markdown in `docs/`, no generator. Header comments explain the
implementation; doc pages explain usage. Keep them in sync in the same commit.

- Adding a block: create `docs/<category>/<block>.md` and add a row to the block
  tables in both `docs/README.md` and the root `README.md`.
- Changing a block's API, behaviour, ranges, error bounds or performance
  characteristics: update its doc page.
- Removing or renaming a block: update or remove its page and table rows.
- Each page follows the same structure (see existing pages): title, include line, a
  one-line purpose, then `API` (signatures with units and ranges), `Usage` (a short,
  realistic example), behaviour / accuracy / trade-off sections as relevant, and
  `Performance` (what makes it fast and which benchmark backs it).
- Figures (error bounds, ranges, resolutions) must match the header comments and
  what the tests enforce. Don't quote benchmark timings: they are machine-dependent.

## Formatting

- `.clang-format` at the repo root is the reference. Run it on every changed file.
