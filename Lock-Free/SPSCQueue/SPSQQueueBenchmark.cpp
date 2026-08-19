#include "SPSQQueueTestSupport.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;
constexpr std::uint64_t kOperations = 20'000'000;

void PinCurrentThread(unsigned cpu) {
#ifdef __linux__
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu, &cpu_set);
    (void)pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
#else
    (void)cpu;
#endif
}

void PrintResult(const char* name, double seconds) {
    const double operations_per_second = kOperations / seconds;
    std::cout << std::left << std::setw(34) << name << std::right
              << std::fixed << std::setprecision(3) << std::setw(10) << seconds
              << std::setprecision(2) << std::setw(14)
              << operations_per_second / 1'000'000.0 << std::setprecision(1)
              << std::setw(14) << seconds * 1e9 / kOperations << "  OK\n";
}

bool SingleThreadRoundTrip() {
    PinCurrentThread(0);
    SPCQQueue<QueueItem, 1024> queue;
    const auto started = Clock::now();
    for (std::uint64_t sequence = 0; sequence < kOperations; ++sequence) {
        if (!queue.try_push(QueueItem{sequence})) {
            std::cout << std::left << std::setw(72)
                      << "single-thread push+pop" << "FAILED (no progress)\n";
            return false;
        }
        const auto item = queue.try_pop();
        if (!item || !item->IsValid() || item->sequence != sequence) {
            std::cout << std::left << std::setw(72)
                      << "single-thread push+pop" << "FAILED (validation)\n";
            return false;
        }
    }
    PrintResult("single-thread push+pop", std::chrono::duration<double>(Clock::now() - started).count());
    return true;
}

bool ProducerConsumerThroughput() {
    SPCQQueue<QueueItem, 1024> queue;
    std::atomic<bool> start{false};
    std::atomic<bool> failed{false};
    const auto deadline = Clock::now() + std::chrono::seconds(10);

    std::thread producer([&] {
        PinCurrentThread(0);
        while (!start.load(std::memory_order_acquire)) {
        }
        for (std::uint64_t sequence = 0; sequence < kOperations; ++sequence) {
            while (!queue.try_push(QueueItem{sequence})) {
                if (Clock::now() >= deadline) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        PinCurrentThread(2);
        while (!start.load(std::memory_order_acquire)) {
        }
        for (std::uint64_t expected = 0; expected < kOperations; ++expected) {
            std::optional<QueueItem> item;
            while (!(item = queue.try_pop())) {
                if (Clock::now() >= deadline) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
                std::this_thread::yield();
            }
            if (!item->IsValid() || item->sequence != expected) {
                failed.store(true, std::memory_order_relaxed);
                return;
            }
        }
    });

    const auto started = Clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    if (failed.load(std::memory_order_relaxed)) {
        std::cout << std::left << std::setw(72)
                  << "1 producer + 1 consumer transfer" << "FAILED (validation/progress)\n";
        return false;
    }
    PrintResult("1 producer + 1 consumer transfer",
                std::chrono::duration<double>(Clock::now() - started).count());
    return true;
}

}  // namespace

int main() {
    std::cout << "Operations per case: " << kOperations << "\n\n"
              << std::left << std::setw(34) << "case" << std::right
              << std::setw(10) << "seconds" << std::setw(14) << "M ops/s"
              << std::setw(14) << "ns/op" << "  status\n";
    const bool single_thread_ok = SingleThreadRoundTrip();
    const bool spsc_ok = ProducerConsumerThroughput();
    return single_thread_ok && spsc_ok ? 0 : 1;
}
