.PHONY: all configure build test run asan tsan clean help

all: test

configure:
	cmake -S . -B build -DENABLE_ASAN=OFF -DENABLE_TSAN=OFF

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

asan:
	cmake -S . -B build-asan -DENABLE_ASAN=ON -DENABLE_TSAN=OFF
	cmake --build build-asan -j
	ctest --test-dir build-asan --output-on-failure

tsan:
	cmake -S . -B build-tsan -DENABLE_ASAN=OFF -DENABLE_TSAN=ON
	cmake --build build-tsan -j
	ctest --test-dir build-tsan --output-on-failure

clean:
	cmake -E remove_directory "$(CURDIR)/build"
	cmake -E remove_directory "$(CURDIR)/build-asan"
	cmake -E remove_directory "$(CURDIR)/build-tsan"

help:
	@echo "make test                       build and run all tests"
	@echo "make run TEST=sequential_fifo  run one scenario"
	@echo "make asan                      run with ASAN and UBSan"
	@echo "make tsan                      run with TSAN"
	@echo "make clean                     remove generated build directories"
