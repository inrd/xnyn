add_executable(xnyn_benchmarks
        osc/phasor_bench.cpp)

target_link_libraries(xnyn_benchmarks PRIVATE xnyn::xnyn Catch2::Catch2WithMain)

# Benchmarks are only meaningful with optimizations, whatever the build type.
target_compile_options(xnyn_benchmarks PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-O3>
        $<$<CXX_COMPILER_ID:MSVC>:/O2>)
