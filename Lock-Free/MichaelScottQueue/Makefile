.PHONY: all configure build test run benchmark benchmark-one asan tsan clean help

BENCHMARK_ITERATIONS ?= 250000
BENCHMARK_PRODUCERS ?= 4
BENCHMARK_CONSUMERS ?= 4

all: test

configure:
	cmake -S . -B build -DENABLE_ASAN=OFF -DENABLE_TSAN=OFF \
		-DBUILD_BENCHMARKS=ON

build: configure
	cmake --build build -j

test: build
	ctest --test-dir build --output-on-failure

run: build
	@test -n "$(TEST)" || { \
		echo "usage: make run TEST=sequential_fifo"; \
		exit 2; \
	}
	./build/msqueue_tests "$(TEST)"

benchmark:
	cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
		-DENABLE_ASAN=OFF -DENABLE_TSAN=OFF -DBUILD_BENCHMARKS=ON
	cmake --build build-release -j --target msqueue_benchmarks
	./build-release/msqueue_benchmarks --benchmark sequential \
		--iterations $(BENCHMARK_ITERATIONS)
	./build-release/msqueue_benchmarks --benchmark spsc \
		--iterations $(BENCHMARK_ITERATIONS)
	./build-release/msqueue_benchmarks --benchmark mpmc \
		--iterations $(BENCHMARK_ITERATIONS) \
		--producers $(BENCHMARK_PRODUCERS) \
		--consumers $(BENCHMARK_CONSUMERS)

benchmark-one:
	@test -n "$(BENCH)" || { \
		echo "usage: make benchmark-one BENCH=mpmc [BENCHMARK_ITERATIONS=...]"; \
		exit 2; \
	}
	cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
		-DENABLE_ASAN=OFF -DENABLE_TSAN=OFF -DBUILD_BENCHMARKS=ON
	cmake --build build-release -j --target msqueue_benchmarks
	./build-release/msqueue_benchmarks --benchmark "$(BENCH)" \
		--iterations $(BENCHMARK_ITERATIONS) \
		--producers $(BENCHMARK_PRODUCERS) \
		--consumers $(BENCHMARK_CONSUMERS)

asan:
	cmake -S . -B build-asan -DENABLE_ASAN=ON -DENABLE_TSAN=OFF \
		-DBUILD_BENCHMARKS=OFF
	cmake --build build-asan -j
	ctest --test-dir build-asan --output-on-failure

tsan:
	cmake -S . -B build-tsan -DENABLE_ASAN=OFF -DENABLE_TSAN=ON \
		-DBUILD_BENCHMARKS=OFF
	cmake --build build-tsan -j
	ctest --test-dir build-tsan --output-on-failure

clean:
	cmake -E remove_directory "$(CURDIR)/build"
	cmake -E remove_directory "$(CURDIR)/build-asan"
	cmake -E remove_directory "$(CURDIR)/build-tsan"
	cmake -E remove_directory "$(CURDIR)/build-release"

help:
	@echo "make test                       build and run all tests"
	@echo "make run TEST=sequential_fifo  run one scenario"
	@echo "make asan                      run with ASAN and UBSan"
	@echo "make tsan                      run with TSAN"
	@echo "make benchmark                 benchmark sequential, SPSC and MPMC"
	@echo "make benchmark-one BENCH=mpmc run one benchmark"
	@echo "make clean                     remove generated build directories"
