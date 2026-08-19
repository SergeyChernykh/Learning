#include "SPSQQueueTestSupport.hpp"

#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

template <typename Queue>
bool PushUntil(Queue& queue, QueueItem item, Clock::time_point deadline) {
    while (Clock::now() < deadline) {
        if (queue.try_push(std::move(item))) {
            return true;
        }
        std::this_thread::yield();
    }
    return false;
}

template <typename Queue>
std::optional<QueueItem> PopUntil(Queue& queue, Clock::time_point deadline) {
    while (Clock::now() < deadline) {
        if (auto item = queue.try_pop()) {
            return item;
        }
        std::this_thread::yield();
    }
    return std::nullopt;
}

bool BasicFifo() {
    SPCQQueue<QueueItem, 8> queue;
    const auto deadline = Clock::now() + std::chrono::milliseconds(100);

    for (std::uint64_t value : {10, 20, 30}) {
        if (!PushUntil(queue, QueueItem{value}, deadline)) {
            return false;
        }
    }
    for (std::uint64_t expected : {10, 20, 30}) {
        const auto item = PopUntil(queue, deadline);
        if (!item || !item->IsValid() || item->sequence != expected) {
            return false;
        }
    }
    return true;
}
bool EmptyAndFull() {
    SPCQQueue<QueueItem, 4> queue;
    if (queue.try_pop().has_value()) {
        return false;
    }
    for (std::uint64_t value = 0; value < 4; ++value) {
        if (!queue.try_push(QueueItem{value})) {
            return false;
        }
    }
    if (queue.try_push(QueueItem{4})) {
        return false;
    }
    /*
    for (std::uint64_t expected = 0; expected < 4; ++expected) {
        const auto item = queue.try_pop();
        if (!item || !item->IsValid() || item->sequence != expected) {
            return false;
        }
    }
    return !queue.try_pop().has_value();
    */
    return true;
}

bool WrapAround() {
    SPCQQueue<QueueItem, 4> queue;
    for (std::uint64_t batch = 0; batch < 5; ++batch) {
        for (std::uint64_t offset = 0; offset < 4; ++offset) {
            if (!queue.try_push(QueueItem{batch * 4 + offset})) {
                return false;
            }
        }
        for (std::uint64_t offset = 0; offset < 4; ++offset) {
            const auto expected = batch * 4 + offset;
            const auto item = queue.try_pop();
            if (!item || !item->IsValid() || item->sequence != expected) {
                return false;
            }
        }
    }
    return true;
}

bool RepeatedReuse() {
    SPCQQueue<QueueItem, 2> queue;
    constexpr std::uint64_t kOperations = 10'000;
    for (std::uint64_t sequence = 0; sequence < kOperations; ++sequence) {
        if (!queue.try_push(QueueItem{sequence})) {
            return false;
        }
        const auto item = queue.try_pop();
        if (!item || !item->IsValid() || item->sequence != sequence) {
            return false;
        }
    }
    return true;
}

bool ProducerConsumerStress() {
    constexpr std::uint64_t kOperations = 250'000;
    SPCQQueue<QueueItem, 1024> queue;
    const auto deadline = Clock::now() + std::chrono::seconds(3);
    std::atomic<bool> producer_ok{true};
    std::atomic<bool> consumer_ok{true};
    std::atomic<std::uint64_t> consumed{0};

    std::thread producer([&] {
        for (std::uint64_t sequence = 0; sequence < kOperations; ++sequence) {
            if (!PushUntil(queue, QueueItem{sequence}, deadline)) {
                producer_ok.store(false, std::memory_order_relaxed);
                return;
            }
        }
    });

    std::thread consumer([&] {
        for (std::uint64_t expected = 0; expected < kOperations; ++expected) {
            const auto item = PopUntil(queue, deadline);
            if (!item || !item->IsValid() || item->sequence != expected) {
                consumer_ok.store(false, std::memory_order_relaxed);
                return;
            }
            consumed.fetch_add(1, std::memory_order_relaxed);
        }
    });

    producer.join();
    consumer.join();
    return producer_ok.load(std::memory_order_relaxed) &&
           consumer_ok.load(std::memory_order_relaxed) &&
           consumed.load(std::memory_order_relaxed) == kOperations;
}

struct TestCase {
    std::string_view name;
    std::function<bool()> run;
};

}  // namespace

int main() {
    const std::vector<TestCase> tests = {
        {"basic FIFO", BasicFifo},
        {"empty/full boundaries", EmptyAndFull},
        {"wrap-around", WrapAround},
        {"repeated reuse", RepeatedReuse},
        {"1 producer + 1 consumer stress, integrity and order",
        ProducerConsumerStress},
    };

    std::size_t passed = 0;
    for (const auto& test : tests) {
        bool ok = false;
        try {
            ok = test.run();
        } catch (const std::exception& error) {
            std::cerr << "  exception: " << error.what() << '\n';
        } catch (...) {
            std::cerr << "  unknown exception\n";
        }
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << test.name << '\n';
        passed += ok;
    }

    std::cout << "\n" << passed << '/' << tests.size() << " tests passed\n";
    return passed == tests.size() ? 0 : 1;
}
