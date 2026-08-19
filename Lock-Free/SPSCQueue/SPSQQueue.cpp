#include <atomic>
#include <array>
#include <optional>
#include <iostream>
template <class T, std::size_t capacity>
class SPCQQueue {
static_assert(capacity != 0);
public:
    bool try_push(T val) {
        std::size_t head = head_.load(std::memory_order_acquire);
        std::size_t i = tail_.load(std::memory_order_relaxed);
        if (i - head == capacity) {
            return false;
        }
        queue_[i % capacity] = std::move(val);
        tail_.store(i + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() {
        std::size_t tail = tail_.load(std::memory_order_acquire);
        std::size_t i = head_.load(std::memory_order_relaxed);
        if (i == tail) {
            return std::nullopt;
        }
        T val = std::move(queue_[i % capacity]);
        head_.store(i + 1, std::memory_order_release);
        return val;
    }
private:
    std::array<T, capacity> queue_;
    alignas(64) std::atomic<std::size_t> head_ {0};
    alignas(64) std::atomic<std::size_t> tail_ {0};
};

int main() {
    return 0;
}
