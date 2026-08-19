#include "../TrieberStack.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    std::size_t iterations = 100'000;
    std::size_t threads = 4;
    std::size_t rounds = 5;
    bool csv = false;
};

struct Sample {
    std::uint64_t operations;
    double seconds;
};

class StartGate {
public:
    void arrive_and_wait()
    {
        ready_.fetch_add(1, std::memory_order_release);
        while (!open_.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    void wait_until_ready(std::size_t worker_count) const
    {
        while (ready_.load(std::memory_order_acquire) < worker_count) {
            std::this_thread::yield();
        }
    }

    void open()
    {
        open_.store(true, std::memory_order_release);
    }

private:
    std::atomic<std::size_t> ready_ {0};
    std::atomic<bool> open_ {false};
};

[[noreturn]] void usage_error(const std::string &message)
{
    throw std::invalid_argument(
        message +
        "\nusage: tstack_benchmarks [--iterations N] [--threads N] "
        "[--rounds N] [--csv]");
}

std::size_t parse_positive(const char *text, const std::string &option)
{
    try {
        std::size_t consumed = 0;
        const auto value = std::stoull(text, &consumed);
        if (text[consumed] != '\0' || value == 0) {
            usage_error(option + " requires a positive integer");
        }
        return static_cast<std::size_t>(value);
    } catch (const std::exception &) {
        usage_error(option + " requires a positive integer");
    }
}

Options parse_options(int argc, char **argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--csv") {
            options.csv = true;
        } else if (argument == "--iterations" || argument == "--threads" ||
                   argument == "--rounds") {
            if (index + 1 >= argc) {
                usage_error("missing value for " + argument);
            }
            const auto value = parse_positive(argv[++index], argument);
            if (argument == "--iterations") {
                options.iterations = value;
            } else if (argument == "--threads") {
                options.threads = value;
            } else {
                options.rounds = value;
            }
        } else if (argument == "--help" || argument == "-h") {
            std::cout
                << "usage: tstack_benchmarks [--iterations N] [--threads N] "
                   "[--rounds N] [--csv]\n\n"
                << "iterations are per worker; mixed mode uses half of the "
                   "threads as producers\n";
            std::exit(EXIT_SUCCESS);
        } else {
            usage_error("unknown option: " + argument);
        }
    }

    if (options.threads < 2) {
        usage_error("--threads must be at least 2");
    }
    return options;
}

std::uint64_t expected_sum(std::uint64_t count)
{
    return count * (count - 1) / 2;
}

template <class Work>
Sample measure(std::uint64_t operations, Work &&work)
{
    const auto begin = Clock::now();
    work();
    const auto end = Clock::now();
    return {
        operations,
        std::chrono::duration<double>(end - begin).count(),
    };
}

Sample sequential_push(std::size_t iterations)
{
    TStack<std::uint64_t> stack;
    return measure(iterations, [&] {
        for (std::size_t value = 0; value < iterations; ++value) {
            stack.push(value);
        }
    });
}

Sample sequential_pop(std::size_t iterations)
{
    TStack<std::uint64_t> stack;
    for (std::size_t value = 0; value < iterations; ++value) {
        stack.push(value);
    }

    std::uint64_t sum = 0;
    const auto sample = measure(iterations, [&] {
        for (std::size_t index = 0; index < iterations; ++index) {
            const auto value = stack.try_pop();
            if (!value) {
                throw std::runtime_error("sequential_pop: stack became empty early");
            }
            sum += *value;
        }
    });
    if (sum != expected_sum(iterations) || stack.try_pop()) {
        throw std::runtime_error("sequential_pop: invalid result");
    }
    return sample;
}

Sample sequential_push_pop(std::size_t iterations)
{
    TStack<std::uint64_t> stack;
    std::uint64_t sum = 0;
    const auto sample = measure(2 * iterations, [&] {
        for (std::size_t value = 0; value < iterations; ++value) {
            stack.push(value);
            const auto popped = stack.try_pop();
            if (!popped) {
                throw std::runtime_error(
                    "sequential_push_pop: stack became empty early");
            }
            sum += *popped;
        }
    });
    if (sum != expected_sum(iterations) || stack.try_pop()) {
        throw std::runtime_error("sequential_push_pop: invalid result");
    }
    return sample;
}

Sample parallel_push(std::size_t iterations, std::size_t thread_count)
{
    TStack<std::uint64_t> stack;
    StartGate gate;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::size_t id = 0; id < thread_count; ++id) {
        threads.emplace_back([&, id] {
            gate.arrive_and_wait();
            const auto begin = id * iterations;
            const auto end = begin + iterations;
            for (auto value = begin; value < end; ++value) {
                stack.push(value);
            }
        });
    }

    gate.wait_until_ready(thread_count);
    const auto begin = Clock::now();
    gate.open();
    for (auto &thread : threads) {
        thread.join();
    }
    const auto end = Clock::now();

    const auto total = iterations * thread_count;
    std::uint64_t sum = 0;
    std::uint64_t count = 0;
    while (const auto value = stack.try_pop()) {
        sum += *value;
        ++count;
    }
    if (count != total || sum != expected_sum(total)) {
        throw std::runtime_error("parallel_push: invalid result");
    }

    return {total, std::chrono::duration<double>(end - begin).count()};
}

Sample parallel_pop(std::size_t iterations, std::size_t thread_count)
{
    TStack<std::uint64_t> stack;
    const auto total = iterations * thread_count;
    for (std::uint64_t value = 0; value < total; ++value) {
        stack.push(value);
    }

    StartGate gate;
    std::atomic<std::uint64_t> sum {0};
    std::atomic<bool> failed {false};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::size_t id = 0; id < thread_count; ++id) {
        threads.emplace_back([&] {
            gate.arrive_and_wait();
            std::uint64_t local_sum = 0;
            for (std::size_t index = 0; index < iterations; ++index) {
                const auto value = stack.try_pop();
                if (!value) {
                    failed.store(true, std::memory_order_relaxed);
                    break;
                }
                local_sum += *value;
            }
            sum.fetch_add(local_sum, std::memory_order_relaxed);
        });
    }

    gate.wait_until_ready(thread_count);
    const auto begin = Clock::now();
    gate.open();
    for (auto &thread : threads) {
        thread.join();
    }
    const auto end = Clock::now();

    if (failed.load(std::memory_order_relaxed) ||
        sum.load(std::memory_order_relaxed) != expected_sum(total) ||
        stack.try_pop()) {
        throw std::runtime_error("parallel_pop: invalid result");
    }

    return {total, std::chrono::duration<double>(end - begin).count()};
}

Sample parallel_push_pop(std::size_t iterations, std::size_t thread_count)
{
    const std::size_t producer_count = thread_count / 2;
    const std::size_t consumer_count = thread_count - producer_count;
    const std::uint64_t total_values = iterations * producer_count;

    TStack<std::uint64_t> stack;
    StartGate gate;
    std::atomic<std::size_t> producers_done {0};
    std::atomic<std::uint64_t> popped {0};
    std::atomic<std::uint64_t> sum {0};
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (std::size_t id = 0; id < producer_count; ++id) {
        threads.emplace_back([&, id] {
            gate.arrive_and_wait();
            const auto begin = id * iterations;
            const auto end = begin + iterations;
            for (auto value = begin; value < end; ++value) {
                stack.push(value);
            }
            producers_done.fetch_add(1, std::memory_order_release);
        });
    }

    for (std::size_t id = 0; id < consumer_count; ++id) {
        threads.emplace_back([&] {
            gate.arrive_and_wait();
            std::uint64_t local_count = 0;
            std::uint64_t local_sum = 0;
            for (;;) {
                if (const auto value = stack.try_pop()) {
                    ++local_count;
                    local_sum += *value;
                    continue;
                }
                if (producers_done.load(std::memory_order_acquire) ==
                    producer_count) {
                    break;
                }
                std::this_thread::yield();
            }
            popped.fetch_add(local_count, std::memory_order_relaxed);
            sum.fetch_add(local_sum, std::memory_order_relaxed);
        });
    }

    gate.wait_until_ready(thread_count);
    const auto begin = Clock::now();
    gate.open();
    for (auto &thread : threads) {
        thread.join();
    }
    const auto end = Clock::now();

    if (popped.load(std::memory_order_relaxed) != total_values ||
        sum.load(std::memory_order_relaxed) != expected_sum(total_values) ||
        stack.try_pop()) {
        throw std::runtime_error("parallel_push_pop: invalid result");
    }

    return {
        2 * total_values,
        std::chrono::duration<double>(end - begin).count(),
    };
}

struct Result {
    std::string name;
    std::uint64_t operations;
    std::vector<double> rates;
};

template <class Benchmark>
Result run_rounds(const std::string &name, std::size_t rounds,
                  Benchmark &&benchmark)
{
    Result result {name, 0, {}};
    result.rates.reserve(rounds);
    for (std::size_t round = 0; round < rounds; ++round) {
        const auto sample = benchmark();
        result.operations = sample.operations;
        result.rates.push_back(sample.operations / sample.seconds);
    }
    return result;
}

void print_results(std::vector<Result> results, const Options &options)
{
    if (options.csv) {
        std::cout << "benchmark,operations,median_ops_per_sec,min_ops_per_sec,"
                     "max_ops_per_sec\n";
    } else {
        std::cout << "iterations/worker=" << options.iterations
                  << " threads=" << options.threads
                  << " rounds=" << options.rounds << "\n\n";
        std::cout << std::left << std::setw(25) << "benchmark"
                  << std::right << std::setw(14) << "operations"
                  << std::setw(18) << "median ops/s"
                  << std::setw(18) << "min ops/s"
                  << std::setw(18) << "max ops/s" << '\n';
    }

    for (auto &result : results) {
        std::sort(result.rates.begin(), result.rates.end());
        const auto median = result.rates[result.rates.size() / 2];
        const auto minimum = result.rates.front();
        const auto maximum = result.rates.back();

        if (options.csv) {
            std::cout << result.name << ',' << result.operations << ','
                      << std::fixed << std::setprecision(0) << median << ','
                      << minimum << ',' << maximum << '\n';
        } else {
            std::cout << std::left << std::setw(25) << result.name
                      << std::right << std::setw(14) << result.operations
                      << std::setw(18) << std::fixed << std::setprecision(0)
                      << median << std::setw(18) << minimum << std::setw(18)
                      << maximum << '\n';
        }
    }
}

}  // namespace

int main(int argc, char **argv)
{
    try {
        const auto options = parse_options(argc, argv);
        std::vector<Result> results;
        results.push_back(run_rounds("sequential_push", options.rounds, [&] {
            return sequential_push(options.iterations);
        }));
        results.push_back(run_rounds("sequential_pop", options.rounds, [&] {
            return sequential_pop(options.iterations);
        }));
        results.push_back(
            run_rounds("sequential_push_pop", options.rounds, [&] {
                return sequential_push_pop(options.iterations);
            }));
        results.push_back(run_rounds("parallel_push", options.rounds, [&] {
            return parallel_push(options.iterations, options.threads);
        }));
        results.push_back(run_rounds("parallel_pop", options.rounds, [&] {
            return parallel_pop(options.iterations, options.threads);
        }));
        results.push_back(
            run_rounds("parallel_push_pop", options.rounds, [&] {
                return parallel_push_pop(options.iterations, options.threads);
            }));
        print_results(std::move(results), options);
    } catch (const std::exception &error) {
        std::cerr << "benchmark failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
