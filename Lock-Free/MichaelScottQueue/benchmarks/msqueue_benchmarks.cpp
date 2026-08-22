#include "../MSQueue.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    std::string benchmark;
    std::uint64_t iterations = 250'000;
    unsigned int producers = 4;
    unsigned int consumers = 4;
    bool csv = false;
};

struct MemorySample {
    std::uint64_t rss_kib = 0;
    std::uint64_t peak_rss_kib = 0;
    bool available = false;
};

struct Result {
    std::string benchmark;
    std::uint64_t items = 0;
    std::uint64_t operations = 0;
    unsigned int producers = 0;
    unsigned int consumers = 0;
    double seconds = 0.0;
    MemorySample before;
    MemorySample after;
};

struct alignas(64) ConsumerStats {
    std::uint64_t consumed = 0;
    std::uint64_t sum = 0;
    std::uint64_t values_xor = 0;
};

[[noreturn]] void fail(const std::string& message)
{
    throw std::runtime_error(message);
}

std::uint64_t parse_positive(const char* text, const char* option)
{
    std::size_t parsed = 0;
    const std::string value(text);
    const auto number = std::stoull(value, &parsed);
    if (parsed != value.size() || number == 0) {
        fail(std::string(option) + " requires a positive integer");
    }
    return number;
}

Options parse_options(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        auto require_value = [&](const char* option) -> const char* {
            if (++index == argc) {
                fail(std::string(option) + " requires a value");
            }
            return argv[index];
        };

        if (argument == "--benchmark") {
            options.benchmark = require_value("--benchmark");
        } else if (argument == "--iterations") {
            options.iterations = parse_positive(
                require_value("--iterations"), "--iterations");
        } else if (argument == "--producers") {
            const auto count = parse_positive(
                require_value("--producers"), "--producers");
            if (count > std::numeric_limits<unsigned int>::max()) {
                fail("--producers is too large");
            }
            options.producers = static_cast<unsigned int>(count);
        } else if (argument == "--consumers") {
            const auto count = parse_positive(
                require_value("--consumers"), "--consumers");
            if (count > std::numeric_limits<unsigned int>::max()) {
                fail("--consumers is too large");
            }
            options.consumers = static_cast<unsigned int>(count);
        } else if (argument == "--csv") {
            options.csv = true;
        } else if (argument == "--help") {
            std::cout
                << "usage: msqueue_benchmarks --benchmark NAME [options]\n"
                << "\n"
                << "benchmarks: sequential, spsc, mpmc\n"
                << "options:\n"
                << "  --iterations N  items per producer (default: 250000)\n"
                << "  --producers N   MPMC producer count (default: 4)\n"
                << "  --consumers N   MPMC consumer count (default: 4)\n"
                << "  --csv           print a CSV row\n";
            std::exit(EXIT_SUCCESS);
        } else {
            fail("unknown option: " + argument);
        }
    }

    if (options.benchmark.empty()) {
        fail("--benchmark is required");
    }
    return options;
}

std::optional<std::uint64_t> read_status_value(
    const std::string& key,
    const std::string& line)
{
    if (line.compare(0, key.size(), key) != 0) {
        return std::nullopt;
    }

    const auto first_digit = line.find_first_of("0123456789", key.size());
    if (first_digit == std::string::npos) {
        return std::nullopt;
    }
    return std::stoull(line.substr(first_digit));
}

MemorySample sample_memory()
{
    std::ifstream status("/proc/self/status");
    MemorySample sample;
    if (!status) {
        return sample;
    }

    std::string line;
    bool found_rss = false;
    bool found_peak = false;
    while (std::getline(status, line)) {
        if (const auto value = read_status_value("VmRSS:", line)) {
            sample.rss_kib = *value;
            found_rss = true;
        } else if (const auto value = read_status_value("VmHWM:", line)) {
            sample.peak_rss_kib = *value;
            found_peak = true;
        }
    }
    sample.available = found_rss && found_peak;
    return sample;
}

std::uint64_t xor_zero_to(std::uint64_t value)
{
    switch (value & 3U) {
    case 0:
        return value;
    case 1:
        return 1;
    case 2:
        return value + 1;
    default:
        return 0;
    }
}

std::uint64_t expected_sum(std::uint64_t item_count)
{
    if ((item_count & 1U) == 0) {
        return (item_count / 2) * (item_count - 1);
    }
    return item_count * ((item_count - 1) / 2);
}

Result run_sequential(const Options& options)
{
    MSQueue<std::uint64_t> queue;
    Result result {"sequential", options.iterations,
                   options.iterations * 2, 1, 1, 0.0, {}, {}};
    result.before = sample_memory();

    const auto started = Clock::now();
    for (std::uint64_t value = 0; value < options.iterations; ++value) {
        queue.push(value);
    }

    std::uint64_t sum = 0;
    std::uint64_t values_xor = 0;
    for (std::uint64_t expected = 0; expected < options.iterations; ++expected) {
        const auto value = queue.try_pop();
        if (!value || *value != expected) {
            fail("sequential benchmark observed broken FIFO order");
        }
        sum += *value;
        values_xor ^= *value;
    }
    const auto finished = Clock::now();

    if (sum != expected_sum(options.iterations) ||
        values_xor != xor_zero_to(options.iterations - 1)) {
        fail("sequential benchmark checksum failed");
    }
    result.seconds = std::chrono::duration<double>(finished - started).count();
    result.after = sample_memory();
    return result;
}

Result run_spsc(const Options& options)
{
    MSQueue<std::uint64_t> queue;
    Result result {"spsc", options.iterations,
                   options.iterations * 2, 1, 1, 0.0, {}, {}};
    std::atomic<unsigned int> ready {0};
    std::atomic<bool> start {false};
    std::atomic<bool> valid {true};

    std::thread producer([&] {
        ready.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (std::uint64_t value = 0; value < options.iterations; ++value) {
            queue.push(value);
        }
    });

    std::thread consumer([&] {
        ready.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        std::uint64_t expected = 0;
        while (expected < options.iterations) {
            if (const auto value = queue.try_pop()) {
                if (*value != expected) {
                    valid.store(false, std::memory_order_relaxed);
                }
                ++expected;
            } else {
                std::this_thread::yield();
            }
        }
    });

    while (ready.load(std::memory_order_acquire) != 2) {
        std::this_thread::yield();
    }
    result.before = sample_memory();
    const auto started = Clock::now();
    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();
    const auto finished = Clock::now();

    if (!valid.load(std::memory_order_relaxed)) {
        fail("SPSC benchmark observed broken FIFO order");
    }
    result.seconds = std::chrono::duration<double>(finished - started).count();
    result.after = sample_memory();
    return result;
}

Result run_mpmc(const Options& options)
{
    if (options.iterations >
        std::numeric_limits<std::uint64_t>::max() / options.producers) {
        fail("total MPMC item count overflows uint64_t");
    }
    const auto total = options.iterations * options.producers;
    if (total > std::numeric_limits<std::uint64_t>::max() / 2) {
        fail("total MPMC operation count overflows uint64_t");
    }
    MSQueue<std::uint64_t> queue;
    Result result {"mpmc", total, total * 2,
                   options.producers, options.consumers, 0.0, {}, {}};

    std::atomic<unsigned int> ready {0};
    std::atomic<bool> start {false};
    std::atomic<unsigned int> producers_done {0};
    std::vector<ConsumerStats> consumer_stats(options.consumers);
    std::vector<std::thread> threads;
    threads.reserve(options.producers + options.consumers);

    for (unsigned int producer = 0; producer < options.producers; ++producer) {
        threads.emplace_back([&, producer] {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            const auto begin = options.iterations * producer;
            const auto end = begin + options.iterations;
            for (std::uint64_t value = begin; value < end; ++value) {
                queue.push(value);
            }
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    for (unsigned int consumer = 0; consumer < options.consumers; ++consumer) {
        threads.emplace_back([&, consumer] {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            auto& stats = consumer_stats[consumer];
            for (;;) {
                if (const auto value = queue.try_pop()) {
                    stats.sum += *value;
                    stats.values_xor ^= *value;
                    ++stats.consumed;
                    continue;
                }
                if (producers_done.load(std::memory_order_acquire) ==
                    options.producers) {
                    break;
                }
                std::this_thread::yield();
            }
        });
    }

    const auto worker_count = options.producers + options.consumers;
    while (ready.load(std::memory_order_acquire) != worker_count) {
        std::this_thread::yield();
    }
    result.before = sample_memory();
    const auto started = Clock::now();
    start.store(true, std::memory_order_release);
    for (auto& thread : threads) {
        thread.join();
    }
    const auto finished = Clock::now();

    std::uint64_t consumed = 0;
    std::uint64_t sum = 0;
    std::uint64_t values_xor = 0;
    for (const auto& stats : consumer_stats) {
        consumed += stats.consumed;
        sum += stats.sum;
        values_xor ^= stats.values_xor;
    }
    if (consumed != total || sum != expected_sum(total) ||
        values_xor != xor_zero_to(total - 1)) {
        fail("MPMC benchmark checksum failed");
    }
    result.seconds = std::chrono::duration<double>(finished - started).count();
    result.after = sample_memory();
    return result;
}

std::uint64_t nonnegative_delta(std::uint64_t after, std::uint64_t before)
{
    return after > before ? after - before : 0;
}

void print_result(const Result& result, bool csv)
{
    const double operations_per_second = result.operations / result.seconds;
    const double nanoseconds_per_operation =
        result.seconds * 1'000'000'000.0 / result.operations;

    if (csv) {
        std::cout
            << result.benchmark << ','
            << result.items << ','
            << result.operations << ','
            << result.producers << ','
            << result.consumers << ','
            << std::fixed << std::setprecision(6) << result.seconds << ','
            << std::setprecision(0) << operations_per_second << ','
            << std::setprecision(2) << nanoseconds_per_operation << ',';
        if (result.before.available && result.after.available) {
            std::cout
                << result.before.rss_kib << ','
                << result.after.rss_kib << ','
                << nonnegative_delta(result.after.rss_kib,
                                     result.before.rss_kib) << ','
                << result.after.peak_rss_kib << ','
                << nonnegative_delta(result.after.peak_rss_kib,
                                     result.before.peak_rss_kib);
        } else {
            std::cout << "NA,NA,NA,NA,NA";
        }
        std::cout << '\n';
        return;
    }

    std::cout
        << "benchmark: " << result.benchmark << '\n'
        << "items: " << result.items << '\n'
        << "operations: " << result.operations << '\n'
        << "producers/consumers: " << result.producers << '/'
        << result.consumers << '\n'
        << std::fixed << std::setprecision(6)
        << "elapsed: " << result.seconds << " s\n"
        << std::setprecision(0)
        << "throughput: " << operations_per_second << " ops/s\n"
        << std::setprecision(2)
        << "average time: " << nanoseconds_per_operation << " ns/op\n";
    if (result.before.available && result.after.available) {
        std::cout
            << "RSS before/after/delta: "
            << result.before.rss_kib << '/' << result.after.rss_kib << '/'
            << nonnegative_delta(result.after.rss_kib, result.before.rss_kib)
            << " KiB\n"
            << "peak RSS after/delta: " << result.after.peak_rss_kib << '/'
            << nonnegative_delta(result.after.peak_rss_kib,
                                 result.before.peak_rss_kib)
            << " KiB\n";
    } else {
        std::cout << "memory: unavailable (/proc/self/status is required)\n";
    }
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parse_options(argc, argv);
        Result result;
        if (options.benchmark == "sequential") {
            result = run_sequential(options);
        } else if (options.benchmark == "spsc") {
            result = run_spsc(options);
        } else if (options.benchmark == "mpmc") {
            result = run_mpmc(options);
        } else {
            fail("unknown benchmark: " + options.benchmark);
        }
        print_result(result, options.csv);
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
