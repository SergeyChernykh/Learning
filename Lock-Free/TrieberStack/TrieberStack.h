#include <atomic>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <iostream>
template <class T>
class TStack {
    struct Node {
        T val;
        Node *next_;
    };

public:
    void push(T value) {
        Node *node = new Node {std::move(value), nullptr};
        if (node == nullptr) {
            throw std::runtime_error("OOO");
        }
        Node *head = head_.load(std::memory_order_relaxed);;
        do {
            node->next_ = head;
        } while (!head_.compare_exchange_weak(head, node, std::memory_order_release));
        return;
    }

    std::optional<T> try_pop() {
        Node *head = nullptr;
        Node *next = nullptr;
        do {
            head = head_.load(std::memory_order_acquire);
            if (head == nullptr) {
                return std::nullopt;
            }
            next = head->next_;
        } while (!head_.compare_exchange_weak(head, next, std::memory_order_relaxed));
        return head->val;
    }
    
private:
    std::atomic<Node*> head_ {nullptr};
};