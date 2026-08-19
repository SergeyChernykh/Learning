#include "../TrieberStack.h"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kThreadCount = 4;
constexpr int kValuesPerThread = 2'000;

[[noreturn]] void fail(const std::string &message)
{
    throw std::runtime_error(message);
}

void require(bool condition, const std::string &message)
{
    if (!condition) {
        fail(message);
    }
}

void test_empty()
{
    TStack<int> stack;
    require(!stack.try_pop().has_value(),
            "try_pop() on an empty stack must return std::nullopt");
}

void test_lifo()
{
    TStack<int> stack;
    stack.push(10);
    stack.push(20);
    stack.push(30);

    const auto first = stack.try_pop();
    const auto second = stack.try_pop();
    const auto third = stack.try_pop();
    const auto empty = stack.try_pop();
    require(first && *first == 30, "the first value must be 30");
    require(second && *second == 20, "the second value must be 20");
    require(third && *third == 10, "the third value must be 10");
    require(!empty, "the stack must be empty after all values are popped");
}

void test_reuse_after_empty()
{
    TStack<int> stack;
    stack.push(1);
    require(stack.try_pop() == 1, "failed to pop the initial value");
    require(!stack.try_pop(), "stack did not become empty");

    stack.push(2);
    require(stack.try_pop() == 2, "failed to reuse the emptied stack");
    require(!stack.try_pop(), "reused stack did not become empty");
}

void test_concurrent_push()
{
    TStack<int> stack;
    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
        threads.emplace_back([&stack, thread_id] {
            const int begin = thread_id * kValuesPerThread;
            const int end = begin + kValuesPerThread;
            for (int value = begin; value < end; ++value) {
                stack.push(value);
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    const int total = kThreadCount * kValuesPerThread;
    std::vector<unsigned char> seen(static_cast<std::size_t>(total), 0);
    int popped = 0;
    while (const auto value = stack.try_pop()) {
        require(*value >= 0 && *value < total,
                "concurrent push produced an out-of-range value");
        require(seen[static_cast<std::size_t>(*value)] == 0,
                "concurrent push produced a duplicate value");
        seen[static_cast<std::size_t>(*value)] = 1;
        ++popped;
    }

    require(popped == total, "concurrent push lost one or more values");
}

void test_concurrent_pop()
{
    TStack<int> stack;
    const int total = kThreadCount * kValuesPerThread;
    for (int value = 0; value < total; ++value) {
        stack.push(value);
    }

    std::vector<std::atomic<unsigned int>> seen(static_cast<std::size_t>(total));
    for (auto &counter : seen) {
        counter.store(0, std::memory_order_relaxed);
    }
    std::atomic<int> popped {0};
    std::atomic<bool> invalid_value {false};
    std::vector<std::thread> threads;

    for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
        threads.emplace_back([&] {
            while (const auto value = stack.try_pop()) {
                if (*value < 0 || *value >= total) {
                    invalid_value.store(true, std::memory_order_relaxed);
                } else {
                    seen[static_cast<std::size_t>(*value)].fetch_add(
                        1, std::memory_order_relaxed);
                }
                popped.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    require(!invalid_value.load(std::memory_order_relaxed),
            "concurrent pop returned an out-of-range value");
    require(popped.load(std::memory_order_relaxed) == total,
            "concurrent pop lost or duplicated values");
    for (int value = 0; value < total; ++value) {
        require(seen[static_cast<std::size_t>(value)].load(
                    std::memory_order_relaxed) == 1,
                "a value was lost or returned more than once");
    }
    require(!stack.try_pop(), "stack must be empty after concurrent pop");
}

void test_concurrent_push_pop()
{
    TStack<int> stack;
    const int producer_count = kThreadCount / 2;
    const int consumer_count = kThreadCount / 2;
    const int total = producer_count * kValuesPerThread;

    std::vector<std::atomic<unsigned int>> seen(static_cast<std::size_t>(total));
    for (auto &counter : seen) {
        counter.store(0, std::memory_order_relaxed);
    }

    std::atomic<int> producers_done {0};
    std::atomic<int> popped {0};
    std::atomic<bool> invalid_value {false};
    std::vector<std::thread> threads;

    for (int producer_id = 0; producer_id < producer_count; ++producer_id) {
        threads.emplace_back([&, producer_id] {
            const int begin = producer_id * kValuesPerThread;
            const int end = begin + kValuesPerThread;
            for (int value = begin; value < end; ++value) {
                stack.push(value);
                if ((value & 63) == 0) {
                    std::this_thread::yield();
                }
            }
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    for (int consumer_id = 0; consumer_id < consumer_count; ++consumer_id) {
        threads.emplace_back([&] {
            for (;;) {
                if (const auto value = stack.try_pop()) {
                    if (*value < 0 || *value >= total) {
                        invalid_value.store(true, std::memory_order_relaxed);
                    } else {
                        seen[static_cast<std::size_t>(*value)].fetch_add(
                            1, std::memory_order_relaxed);
                    }
                    popped.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                if (producers_done.load(std::memory_order_acquire) ==
                    producer_count) {
                    break;
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    require(!invalid_value.load(std::memory_order_relaxed),
            "mixed push/pop returned an out-of-range value");
    require(popped.load(std::memory_order_relaxed) == total,
            "mixed push/pop lost or duplicated values");
    for (int value = 0; value < total; ++value) {
        require(seen[static_cast<std::size_t>(value)].load(
                    std::memory_order_relaxed) == 1,
                "mixed push/pop lost a value or returned it more than once");
    }
    require(!stack.try_pop(), "stack must be empty after mixed push/pop");
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "usage: tstack_tests <scenario>\n";
        return EXIT_FAILURE;
    }

    try {
        const std::string scenario = argv[1];
        if (scenario == "empty") {
            test_empty();
        } else if (scenario == "lifo") {
            test_lifo();
        } else if (scenario == "reuse_after_empty") {
            test_reuse_after_empty();
        } else if (scenario == "concurrent_push") {
            test_concurrent_push();
        } else if (scenario == "concurrent_pop") {
            test_concurrent_pop();
        } else if (scenario == "concurrent_push_pop") {
            test_concurrent_push_pop();
        } else {
            std::cerr << "unknown scenario: " << scenario << '\n';
            return EXIT_FAILURE;
        }
    } catch (const std::exception &error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "PASSED: " << argv[1] << '\n';
    return EXIT_SUCCESS;
}
