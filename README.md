# x[n]y[n]

Minimalist, performance-focused C++20 DSP building blocks.

**xnyn** is header-only and has no dependencies beyond the standard library.

## Blocks

| Block | Header | Description |
|---|---|---|
| `Phasor` | `xnyn/osc/phasor.hpp` | Normalized phase accumulator, branchless wrap, supports negative and through-zero frequencies |

## Usage

```cpp
#include <xnyn/osc/phasor.hpp>

xnyn::Phasor phasor;
phasor.prepare(sampleRate);
phasor.setFrequency(440.0);

for (auto& sample : buffer) {
  const double phase = phasor.tick();  // [0, 1)
  // ...
}
```

Include individual headers, or `<xnyn/xnyn.hpp>` for every block.

## Integration

xnyn exposes the CMake target `xnyn::xnyn`.

**Git submodule**

```cmake
add_subdirectory(path/to/xnyn)
target_link_libraries(MyTarget PRIVATE xnyn::xnyn)
```

**FetchContent**

```cmake
include(FetchContent)
FetchContent_Declare(xnyn
    GIT_REPOSITORY https://github.com/inrd/xnyn.git
    GIT_TAG main)
FetchContent_MakeAvailable(xnyn)
target_link_libraries(MyTarget PRIVATE xnyn::xnyn)
```

Tests and benchmarks are only built when xnyn is the top-level project.

## Development

```sh
# tests
cmake --preset debug && cmake --build --preset debug && ctest --preset debug

# benchmarks
cmake --preset release && cmake --build --preset release
./build/release/benchmarks/xnyn_benchmarks
```

Catch2 v3 is used if installed, otherwise fetched automatically.
See [CONVENTIONS.md](CONVENTIONS.md) for the rules every block follows.

## License

[MIT](LICENSE)
