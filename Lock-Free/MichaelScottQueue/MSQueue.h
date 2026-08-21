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
    MSQueue() : head_(new Node()), tail_(head_.load()), ownership_head_(head_.load()) {}
    ~MSQueue() {
        Node* head = ownership_head_;
        while (head != nullptr) {
            Node *n = head;
            head = head->next;
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
        Node *t = tail_.load();
        Node *tn = nullptr;
        do {
            for (;;) {
                tn = t->next.load();
                if (tn == nullptr) {
                    break;
                }
                if (tail_.compare_exchange_weak(t, tn)) {
                    t = tn;
                }
            }
        } while (!t->next.compare_exchange_weak(tn, node));
        tail_.compare_exchange_strong(t, node);
    }

    std::optional<T> try_pop()
    {
        Node *h = head_.load();
        Node *n = nullptr;
        do {
            n = h->next;
            if (n == nullptr) {
                return std::nullopt;
            }
        }
        while (!head_.compare_exchange_weak(h, n));
        return std::move(n->val);
    }
};
