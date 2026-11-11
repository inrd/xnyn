include(Catch)

add_executable(xnyn_tests
        filter/dc_blocker_tests.cpp
        math/bilerp_tests.cpp        math/decibels_tests.cpp
        math/equal_power_crossfade_tests.cpp        math/fast_pow_tests.cpp
        math/fast_sin_tests.cpp
        math/fast_tanh_tests.cpp
        math/lerp_tests.cpp
        osc/phasor_tests.cpp
        util/sanitize_tests.cpp
        util/sanitize_fast_math_tests.cpp)

target_link_libraries(xnyn_tests PRIVATE xnyn::xnyn Catch2::Catch2WithMain)

target_compile_options(xnyn_tests PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)

# Proves sanitize survives the optimizer assuming NaN / inf never occur.
set_source_files_properties(util/sanitize_fast_math_tests.cpp PROPERTIES COMPILE_OPTIONS
        "$<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-ffast-math;-O2>;$<$<CXX_COMPILER_ID:MSVC>:/fp:fast;/O2>")

catch_discover_tests(xnyn_tests)
