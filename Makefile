# Shortcuts for the CMake presets. Optional filter: make test T="[fast_sin]"
# (Catch2 test spec: tag, test name or wildcard).

T ?=
FORMAT_FILES = $(shell find include tests benchmarks -name '*.hpp' -o -name '*.cpp')

.PHONY: all build release test test-release bench format clean help

all: test

build:
	cmake --preset debug
	cmake --build --preset debug

release:
	cmake --preset release
	cmake --build --preset release

test: build
ifeq ($(T),)
	ctest --preset debug
else
	./build/debug/tests/xnyn_tests "$(T)"
endif

test-release: release
ifeq ($(T),)
	ctest --preset release
else
	./build/release/tests/xnyn_tests "$(T)"
endif

bench: release
	./build/release/benchmarks/xnyn_benchmarks $(if $(T),"$(T)")

format:
	clang-format -i $(FORMAT_FILES)

clean:
	rm -rf build

help:
	@echo "make test           build (debug) and run all tests"
	@echo "make test-release   build (release) and run all tests, faster"
	@echo "make bench          build (release) and run the benchmarks"
	@echo "make format         clang-format every source file"
	@echo "make clean          remove build/"
	@echo 'Filter with T, e.g. make test T="[fast_sin]", make bench T="[math]"'
