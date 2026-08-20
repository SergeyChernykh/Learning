#include "../TrieberStack.h"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kThreadCount = 4;
constexpr int kValuesPerThread = 2'000;

class LifetimeTracked {
public:
    explicit LifetimeTracked(int value = 0) : value_(value)
    {
        alive_.fetch_add(1, std::memory_order_relaxed);
    }

    LifetimeTracked(const LifetimeTracked &other) : value_(other.value_)
    {
        alive_.fetch_add(1, std::memory_order_relaxed);
    }

    LifetimeTracked(LifetimeTracked &&other) noexcept : value_(other.value_)
    {
        alive_.fetch_add(1, std::memory_order_relaxed);
    }

    LifetimeTracked &operator=(const LifetimeTracked &) = default;
    LifetimeTracked &operator=(LifetimeTracked &&) = default;

    ~LifetimeTracked()
    {
        alive_.fetch_sub(1, std::memory_order_relaxed);
    }

    int value() const
    {
        return value_;
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

class ThrowingMoveTracked {
public:
    explicit ThrowingMoveTracked(int value = 0) : value_(value)
    {
        ++alive_;
    }

    ThrowingMoveTracked(const ThrowingMoveTracked &) = delete;
    ThrowingMoveTracked &operator=(const ThrowingMoveTracked &) = delete;

    ThrowingMoveTracked(ThrowingMoveTracked &&other) : value_(other.value_)
    {
        if (throw_on_move_) {
            throw MoveFailure {};
        }
        ++alive_;
    }

    ThrowingMoveTracked &operator=(ThrowingMoveTracked &&) = delete;

    ~ThrowingMoveTracked()
    {
        --alive_;
    }

    static void set_throw_on_move(bool enabled)
    {
        throw_on_move_ = enabled;
    }

    static int alive()
    {
        return alive_;
    }

    struct MoveFailure {};

private:
    int value_;
    static int alive_;
    static bool throw_on_move_;
};

int ThrowingMoveTracked::alive_ = 0;
bool ThrowingMoveTracked::throw_on_move_ = false;

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

void test_move_only_value()
{
    TStack<std::unique_ptr<int>> stack;
    stack.push(std::make_unique<int>(42));

    auto popped = stack.try_pop();
    require(popped && **popped == 42,
            "try_pop() must move a move-only value out of the node");
    require(!stack.try_pop(),
            "the stack must be empty after popping a move-only value");
}

void test_throwing_move_reclaims_popped_node()
{
    require(ThrowingMoveTracked::alive() == 0,
            "the throwing-move lifetime counter must start at zero");

    TStack<ThrowingMoveTracked> stack;
    stack.push(ThrowingMoveTracked {42});
    require(ThrowingMoveTracked::alive() == 1,
            "the stack must own the value before try_pop()");

    ThrowingMoveTracked::set_throw_on_move(true);
    bool move_threw = false;
    try {
        (void)stack.try_pop();
    } catch (const ThrowingMoveTracked::MoveFailure &) {
        move_threw = true;
    }
    ThrowingMoveTracked::set_throw_on_move(false);

    require(move_threw, "try_pop() must propagate a move failure");
    require(ThrowingMoveTracked::alive() == 0,
            "a failed move must not leak the removed node");
    require(!stack.try_pop(),
            "the node is logically removed even when moving its value fails");
}

void test_destructor_reclaims_remaining_nodes()
{
    require(LifetimeTracked::alive() == 0,
            "the lifetime counter must start at zero");
    {
        TStack<LifetimeTracked> stack;
        stack.push(LifetimeTracked {1});
        stack.push(LifetimeTracked {2});
        stack.push(LifetimeTracked {3});
        require(LifetimeTracked::alive() == 3,
                "the stack must own three live values");
    }
    require(LifetimeTracked::alive() == 0,
            "destroying the stack must destroy all remaining values");
}

void test_pop_reclaims_retired_node_on_destruction()
{
    require(LifetimeTracked::alive() == 0,
            "the lifetime counter must start at zero");
    {
        TStack<LifetimeTracked> stack;
        stack.push(LifetimeTracked {42});

        {
            const auto popped = stack.try_pop();
            require(popped && popped->value() == 42,
                    "try_pop() must return the stored value");
            require(LifetimeTracked::alive() == 1,
                    "after pop only the returned value may remain alive");
        }
    }
    require(LifetimeTracked::alive() == 0,
            "stack destruction must reclaim a retired value");
}

void test_concurrent_pop_reclaims_nodes_on_destruction()
{
    require(LifetimeTracked::alive() == 0,
            "the lifetime counter must start at zero");

    {
        TStack<LifetimeTracked> stack;
        const int total = kThreadCount * kValuesPerThread;
        for (int value = 0; value < total; ++value) {
            stack.push(LifetimeTracked {value});
        }
        require(LifetimeTracked::alive() == total,
                "every pushed node must own one live value");

        std::vector<std::atomic<unsigned int>> seen(
            static_cast<std::size_t>(total));
        for (auto &counter : seen) {
            counter.store(0, std::memory_order_relaxed);
        }
        std::atomic<bool> invalid_value {false};
        std::vector<std::thread> threads;

        for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
            threads.emplace_back([&] {
                while (const auto value = stack.try_pop()) {
                    const int index = value->value();
                    if (index < 0 || index >= total) {
                        invalid_value.store(true, std::memory_order_relaxed);
                    } else {
                        seen[static_cast<std::size_t>(index)].fetch_add(
                            1, std::memory_order_relaxed);
                    }
                }
            });
        }
        for (auto &thread : threads) {
            thread.join();
        }

        require(!invalid_value.load(std::memory_order_relaxed),
                "concurrent pop returned an invalid value");
        for (int value = 0; value < total; ++value) {
            require(seen[static_cast<std::size_t>(value)].load(
                        std::memory_order_relaxed) == 1,
                    "concurrent reclamation lost or duplicated a value");
        }
    }

    require(LifetimeTracked::alive() == 0,
            "stack destruction must reclaim all retired values");
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
        } else if (scenario == "move_only_value") {
            test_move_only_value();
        } else if (scenario == "throwing_move_reclaims_popped_node") {
            test_throwing_move_reclaims_popped_node();
        } else if (scenario == "destructor_reclaims_remaining_nodes") {
            test_destructor_reclaims_remaining_nodes();
        } else if (scenario == "pop_reclaims_retired_node_on_destruction") {
            test_pop_reclaims_retired_node_on_destruction();
        } else if (scenario == "concurrent_pop_reclaims_nodes_on_destruction") {
            test_concurrent_pop_reclaims_nodes_on_destruction();
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
