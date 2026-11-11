cmake_minimum_required(VERSION 3.21)

project(xnyn VERSION 0.1.0 LANGUAGES CXX)

add_library(xnyn INTERFACE)
add_library(xnyn::xnyn ALIAS xnyn)

target_include_directories(xnyn INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(xnyn INTERFACE cxx_std_20)

# Tests and benchmarks are only built when xnyn is the top-level project,
# so consumers (add_subdirectory / FetchContent) never pull in Catch2.
if (PROJECT_IS_TOP_LEVEL)
    option(XNYN_BUILD_TESTS "Build the xnyn unit tests" ON)
    option(XNYN_BUILD_BENCHMARKS "Build the xnyn benchmarks" ON)

    if (XNYN_BUILD_TESTS OR XNYN_BUILD_BENCHMARKS)
        find_package(Catch2 3 QUIET)
        if (NOT Catch2_FOUND)
            include(FetchContent)
            FetchContent_Declare(Catch2
                    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
                    GIT_TAG v3.8.1
                    GIT_SHALLOW TRUE)
            FetchContent_MakeAvailable(Catch2)
        endif ()
    endif ()

    if (XNYN_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif ()

    if (XNYN_BUILD_BENCHMARKS)
        add_subdirectory(benchmarks)
    endif ()
endif ()
