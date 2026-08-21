#include "../MSQueue.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
constexpr int kItemsPerProducer = 4'000;
constexpr int kProducerCount = 4;
constexpr int kConsumerCount = 4;

[[noreturn]] void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

void require(bool condition, const std::string& message)
{
    if (!condition) {
        fail(message);
    }
}

struct Item {
    int producer;
    int sequence;
};

class LifetimeTracked {
public:
    explicit LifetimeTracked(int value = 0) : value_(value)
    {
        alive_.fetch_add(1, std::memory_order_relaxed);
    }

    LifetimeTracked(const LifetimeTracked&) = delete;
    LifetimeTracked& operator=(const LifetimeTracked&) = delete;

    LifetimeTracked(LifetimeTracked&& other) noexcept : value_(other.value_)
    {
        alive_.fetch_add(1, std::memory_order_relaxed);
    }

    LifetimeTracked& operator=(LifetimeTracked&&) = delete;

    ~LifetimeTracked()
    {
        alive_.fetch_sub(1, std::memory_order_relaxed);
    }

    static int alive()
    {
        return alive_.load(std::memory_order_relaxed);
    }

private:
    int value_;
    static std::atomic<int> alive_;
};

std::atomic<int> LifetimeTracked::alive_ {0};

void test_empty()
{
    MSQueue<int> queue;
    require(!queue.try_pop(), "new queue must be empty");
    require(!queue.try_pop(), "repeated pop from empty queue must stay empty");
}

void test_sequential_fifo()
{
    MSQueue<int> queue;
    for (int value = 0; value < 100; ++value) {
        queue.push(value);
    }
    for (int expected = 0; expected < 100; ++expected) {
        const auto value = queue.try_pop();
        require(value && *value == expected, "sequential FIFO order is broken");
    }
    require(!queue.try_pop(), "queue must be empty after draining");
}

void test_reuse_after_empty()
{
    MSQueue<int> queue;
    for (int round = 0; round < 1'000; ++round) {
        queue.push(round * 2);
        queue.push(round * 2 + 1);
        require(queue.try_pop() == round * 2, "first value after reuse is wrong");
        require(queue.try_pop() == round * 2 + 1,
                "second value after reuse is wrong");
        require(!queue.try_pop(), "queue did not become empty between rounds");
    }
}

void test_move_only()
{
    MSQueue<std::unique_ptr<int>> queue;
    queue.push(std::make_unique<int>(42));
    auto value = queue.try_pop();
    require(value && *value && **value == 42, "move-only value was not preserved");
    require(!queue.try_pop(), "queue must be empty after move-only pop");
}

void test_destructor_lifetime()
{
    require(LifetimeTracked::alive() == 0, "lifetime counter must start at zero");
    {
        MSQueue<LifetimeTracked> queue;
        for (int value = 0; value < 100; ++value) {
            queue.push(LifetimeTracked {value});
        }
        for (int value = 0; value < 40; ++value) {
            require(queue.try_pop().has_value(), "failed to pop tracked value");
        }
    }
    require(LifetimeTracked::alive() == 0,
            "destructor must reclaim both queued and retired nodes");
}

void test_multiple_producers_fifo()
{
    MSQueue<Item> queue;
    std::vector<std::thread> producers;
    for (int producer = 0; producer < kProducerCount; ++producer) {
        producers.emplace_back([&, producer] {
            for (int sequence = 0; sequence < kItemsPerProducer; ++sequence) {
                queue.push(Item {producer, sequence});
                if ((sequence & 127) == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto& producer : producers) {
        producer.join();
    }

    std::vector<int> next_sequence(kProducerCount, 0);
    const int total = kProducerCount * kItemsPerProducer;
    for (int count = 0; count < total; ++count) {
        const auto item = queue.try_pop();
        require(item.has_value(), "multiple producers lost an item");
        require(item->producer >= 0 && item->producer < kProducerCount,
                "producer id is out of range");
        require(item->sequence == next_sequence[item->producer],
                "FIFO order of one producer was broken");
        ++next_sequence[item->producer];
    }
    require(!queue.try_pop(), "unexpected duplicate after producer drain");
}

void test_multiple_consumers()
{
    constexpr int total = kConsumerCount * kItemsPerProducer;
    MSQueue<int> queue;
    for (int value = 0; value < total; ++value) {
        queue.push(value);
    }

    std::vector<std::atomic<unsigned int>> seen(static_cast<std::size_t>(total));
    for (auto& count : seen) {
        count.store(0, std::memory_order_relaxed);
    }
    std::atomic<int> consumed {0};
    std::atomic<bool> invalid {false};
    std::vector<std::thread> consumers;

    for (int id = 0; id < kConsumerCount; ++id) {
        consumers.emplace_back([&] {
            while (const auto value = queue.try_pop()) {
                if (*value < 0 || *value >= total) {
                    invalid.store(true, std::memory_order_relaxed);
                } else {
                    seen[static_cast<std::size_t>(*value)].fetch_add(
                        1, std::memory_order_relaxed);
                }
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& consumer : consumers) {
        consumer.join();
    }

    require(!invalid.load(std::memory_order_relaxed), "consumer returned bad id");
    require(consumed.load(std::memory_order_relaxed) == total,
            "multiple consumers lost or duplicated an item");
    for (const auto& count : seen) {
        require(count.load(std::memory_order_relaxed) == 1,
                "an item was returned other than exactly once");
    }
}

void test_mixed_mpmc()
{
    constexpr int total = kProducerCount * kItemsPerProducer;
    MSQueue<int> queue;
    std::vector<std::atomic<unsigned int>> seen(static_cast<std::size_t>(total));
    for (auto& count : seen) {
        count.store(0, std::memory_order_relaxed);
    }
    std::atomic<int> producers_done {0};
    std::atomic<int> consumed {0};
    std::atomic<bool> invalid {false};
    std::atomic<bool> timed_out {false};
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    std::vector<std::thread> threads;

    for (int producer = 0; producer < kProducerCount; ++producer) {
        threads.emplace_back([&, producer] {
            const int begin = producer * kItemsPerProducer;
            for (int offset = 0; offset < kItemsPerProducer; ++offset) {
                queue.push(begin + offset);
                if ((offset & 127) == 0) {
                    std::this_thread::yield();
                }
            }
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    for (int id = 0; id < kConsumerCount; ++id) {
        threads.emplace_back([&] {
            for (;;) {
                if (const auto value = queue.try_pop()) {
                    if (*value < 0 || *value >= total) {
                        invalid.store(true, std::memory_order_relaxed);
                    } else {
                        seen[static_cast<std::size_t>(*value)].fetch_add(
                            1, std::memory_order_relaxed);
                    }
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                if (producers_done.load(std::memory_order_acquire) ==
                        kProducerCount &&
                    consumed.load(std::memory_order_relaxed) == total) {
                    break;
                }
                if (Clock::now() >= deadline) {
                    timed_out.store(true, std::memory_order_relaxed);
                    break;
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    require(!timed_out.load(std::memory_order_relaxed), "MPMC test timed out");
    require(!invalid.load(std::memory_order_relaxed), "MPMC returned bad id");
    require(consumed.load(std::memory_order_relaxed) == total,
            "MPMC lost or duplicated an item");
    for (const auto& count : seen) {
        require(count.load(std::memory_order_relaxed) == 1,
                "MPMC item was returned other than exactly once");
    }
    require(!queue.try_pop(), "queue must be empty after MPMC drain");
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: msqueue_tests <scenario>\n";
        return EXIT_FAILURE;
    }

    try {
        const std::string scenario = argv[1];
        if (scenario == "empty") {
            test_empty();
        } else if (scenario == "sequential_fifo") {
            test_sequential_fifo();
        } else if (scenario == "reuse_after_empty") {
            test_reuse_after_empty();
        } else if (scenario == "move_only") {
            test_move_only();
        } else if (scenario == "destructor_lifetime") {
            test_destructor_lifetime();
        } else if (scenario == "multiple_producers_fifo") {
            test_multiple_producers_fifo();
        } else if (scenario == "multiple_consumers") {
            test_multiple_consumers();
        } else if (scenario == "mixed_mpmc") {
            test_mixed_mpmc();
        } else {
            fail("unknown scenario: " + scenario);
        }
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "PASSED: " << argv[1] << '\n';
    return EXIT_SUCCESS;
}
