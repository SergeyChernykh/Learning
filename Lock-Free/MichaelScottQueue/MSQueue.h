#pragma once

#include <optional>
#include <utility>
#include <atomic>
// Этап 1: реализуйте Michael–Scott MPMC queue.
// Требования, инварианты и вопросы находятся в README.md.
template <class T>
class MSQueue {
    struct Node {
        T val;
        std::atomic<Node*> next = nullptr;
    };
    std::atomic<Node*> head_ {nullptr};
    std::atomic<Node*> tail_ {nullptr};
    Node *ownership_head_ {nullptr};

public:
    MSQueue() :
        head_(new Node()),
        tail_(head_.load(std::memory_order_relaxed)),
        ownership_head_(head_.load(std::memory_order_relaxed)) {}
    ~MSQueue() {
        Node* head = ownership_head_;
        while (head != nullptr) {
            Node *n = head;
            head = head->next.load(std::memory_order_relaxed);
            delete n;
        }
    }

    MSQueue(const MSQueue&) = delete;
    MSQueue& operator=(const MSQueue&) = delete;
    MSQueue(MSQueue&&) = delete;
    MSQueue& operator=(MSQueue&&) = delete;

    void push(T value)
    {
        Node *node = new Node{std::move(value), nullptr};
        // мы читаем tail, и сразу его разыминовываем на 66 строке, нода tail должна быть полностью проинициализирована
        // acquire
        Node *t = tail_.load(std::memory_order_acquire);
        Node *tn = nullptr;
        do {
            for (;;) {
                // можем разиминовать tn, после присваивания в t на 54 строке - acquire
                tn = t->next.load(std::memory_order_acquire);
                if (tn == nullptr) {
                    break;
                }
                // успех - обновляем tail_ - release
                // неудача -  t обновляется, мы его разыменовываем, должны видеть проинициализированную ноду - acquire
                if (tail_.compare_exchange_weak(t, tn, std::memory_order_release, std::memory_order_acquire)) {
                    t = tn;
                }
            }
        // после этой точки, все должны увидеть проинициализированную новую ноду
        // поэтому на успехе - release
        // при неудаче tn обновляется, но это значение мы игнорируем - relaxed
        } while (!t->next.compare_exchange_weak(tn, node, std::memory_order_release, std::memory_order_relaxed));
        // успех - публикация tail - release
        // неудача - ничего не публиуется, новое значение игнорируется - relaxed
        tail_.compare_exchange_strong(t, node, std::memory_order_release, std::memory_order_relaxed);
    }

    std::optional<T> try_pop()
    {
        // мы читаем head, и сразу его разыминовываем на 66 строке, нода head должна быть полностью проинициализирована
        // acquire
        Node *h = head_.load(std::memory_order_acquire);
        Node *n = nullptr;
        do {
            // если мы прочитали не нулевую ноду, то она должны быть проинициализирована
            // load - acquire
            n = h->next.load(std::memory_order_acquire);
            if (n == nullptr) {
                return std::nullopt;
            }
        }
        // успех - обновляем head_ - release
        // неудача -  h обновляется, мы его разыменовываем, должны видеть проинициализированную ноду - acquire
        while (!head_.compare_exchange_weak(h, n, std::memory_order_release, std::memory_order_acquire));
        return std::move(n->val);
    }
};
