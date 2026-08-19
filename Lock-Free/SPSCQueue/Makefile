CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -g -Wall -Wextra -Wpedantic -pthread
TSANFLAGS ?= -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -pthread \
	-fsanitize=thread -fno-omit-frame-pointer

.PHONY: all test benchmark benchmark-alignment tsan clean

all: spsc-queue-tests spsc-queue-benchmark

spsc-queue-tests: SPSQQueueTests.cpp SPSQQueueTestSupport.hpp SPSQQueue.testable.inc
	$(CXX) $(CXXFLAGS) SPSQQueueTests.cpp -o $@

spsc-queue-benchmark: SPSQQueueBenchmark.cpp SPSQQueueTestSupport.hpp SPSQQueue.testable.inc
	$(CXX) $(CXXFLAGS) SPSQQueueBenchmark.cpp -o $@

spsc-queue-benchmark-no-align: SPSQQueueBenchmark.cpp SPSQQueueTestSupport.hpp SPSQQueue.no-align.testable.inc
	$(CXX) $(CXXFLAGS) \
		-DSPSC_QUEUE_TESTABLE_INCLUDE='"SPSQQueue.no-align.testable.inc"' \
		SPSQQueueBenchmark.cpp -o $@

spsc-queue-tests-tsan: SPSQQueueTests.cpp SPSQQueueTestSupport.hpp SPSQQueue.testable.inc
	$(CXX) $(TSANFLAGS) SPSQQueueTests.cpp -o $@

SPSQQueue.testable.inc: SPSQQueue.cpp
	sed -e 's/^class SPCQQueue/struct SPCQQueue/' \
	    -e 's/^int main()/int spsc_queue_exercise_main()/' $< > $@

SPSQQueue.no-align.testable.inc: SPSQQueue.cpp
	sed -e 's/^class SPCQQueue/struct SPCQQueue/' \
	    -e 's/alignas(64) //g' \
	    -e 's/^int main()/int spsc_queue_exercise_main()/' $< > $@

test: spsc-queue-tests
	./spsc-queue-tests

benchmark: spsc-queue-benchmark
	./spsc-queue-benchmark

benchmark-alignment: spsc-queue-benchmark spsc-queue-benchmark-no-align
	@printf '\nWith alignas(64):\n'
	@./spsc-queue-benchmark
	@printf '\nWithout alignas(64):\n'
	@./spsc-queue-benchmark-no-align

tsan: spsc-queue-tests-tsan
	./spsc-queue-tests-tsan

clean:
	rm -f spsc-queue-tests spsc-queue-benchmark spsc-queue-tests-tsan \
		spsc-queue-benchmark-no-align SPSQQueue.testable.inc \
		SPSQQueue.no-align.testable.inc
