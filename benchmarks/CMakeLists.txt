add_executable(xnyn_benchmarks
        filter/dc_blocker_bench.cpp
        math/bilerp_bench.cpp        math/equal_power_crossfade_bench.cpp        math/fast_sin_bench.cpp
        math/fast_tanh_bench.cpp
        math/lerp_bench.cpp
        osc/phasor_bench.cpp
        util/sanitize_bench.cpp)

target_link_libraries(xnyn_benchmarks PRIVATE xnyn::xnyn Catch2::Catch2WithMain)

# Benchmarks are only meaningful with optimizations, whatever the build type.
target_compile_options(xnyn_benchmarks PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-O3>
        $<$<CXX_COMPILER_ID:MSVC>:/O2>)
