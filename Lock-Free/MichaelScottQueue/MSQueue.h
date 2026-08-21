#pragma once

#include <optional>
#include <utility>

// Этап 1: реализуйте Michael–Scott MPMC queue.
// Требования, инварианты и вопросы находятся в README.md.
template <class T>
class MSQueue {
public:
    MSQueue() = default;
    ~MSQueue() = default;

    MSQueue(const MSQueue&) = delete;
    MSQueue& operator=(const MSQueue&) = delete;
    MSQueue(MSQueue&&) = delete;
    MSQueue& operator=(MSQueue&&) = delete;

    void push(T value)
    {
        // TODO: присоединить новый узел и при необходимости помочь tail.
        (void)value;
    }

    std::optional<T> try_pop()
    {
        // TODO: отличить пустую очередь от отставшего tail.
        return std::nullopt;
    }
};
