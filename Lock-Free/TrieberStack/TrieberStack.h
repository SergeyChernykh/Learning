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
        Node *retired_next_;
    };
public:

    TStack() = default;
    TStack(const TStack<T>&) = delete;
    TStack& operator=(const TStack<T>&) = delete;
    TStack(TStack<T>&& other) = delete;
    TStack& operator=(TStack<T>&& other) = delete;
    ~TStack() {
        Node *head = head_;
        while (head != nullptr) {
            auto t = head;
            head = head->next_;
            delete t;
        }        
        retire_list(retired_list_);
    }

    void push(T value) {
        Node *node = new Node {std::move(value), nullptr, nullptr};
        Node *head = head_.load(std::memory_order_relaxed);
        do {
            node->next_ = head;
        } while (!head_.compare_exchange_weak(head, node, std::memory_order_release));
        return;
    }

    std::optional<T> try_pop() {
        active_counter_.fetch_add(1);
        Node *head = nullptr;
        Node *next = nullptr;
        do {
            head = head_.load(std::memory_order_acquire);
            if (head == nullptr) {
                retire(nullptr);
                return std::nullopt;
            }
            next = head->next_;
        } while (!head_.compare_exchange_weak(head, next, std::memory_order_relaxed));
        T val = head->val;
        retire(head);
        return val;
    }
private:
    void retire_list(Node* head) {
        while (head != nullptr) {
            auto t = head;
            head = head->retired_next_;
            delete t;
        }
    }

    void retire(Node* node) {
        if (node != nullptr) {
            Node *rhead = retired_list_.load();
            do {
                node->retired_next_ = rhead;
            } while (!retired_list_.compare_exchange_weak(rhead, node));
        }
        if (active_counter_.load() == 1) {
            Node *rhead = retired_list_.exchange(nullptr);
            int old = active_counter_.fetch_sub(1);
            if (rhead == nullptr) {
                return;
            }
            if (old == 1) {
                retire_list(rhead);
                return;
            }
            Node* retire_it = rhead;
            while (retire_it->retired_next_ != nullptr) {
                retire_it = retire_it->retired_next_;
            }
            Node* new_retire_list = retired_list_.load();
            do {
                retire_it->retired_next_ = new_retire_list;
            } while (!retired_list_.compare_exchange_weak(new_retire_list, rhead));
        } else {
            active_counter_.fetch_sub(1);
        }
    }

private:
    std::atomic<Node*> head_ {nullptr};
    std::atomic<int> active_counter_ {0};
    std::atomic<Node*> retired_list_ {nullptr};
};
