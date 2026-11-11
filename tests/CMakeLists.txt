include(Catch)

add_executable(xnyn_tests
        osc/phasor_tests.cpp)

target_link_libraries(xnyn_tests PRIVATE xnyn::xnyn Catch2::Catch2WithMain)

target_compile_options(xnyn_tests PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)

catch_discover_tests(xnyn_tests)
