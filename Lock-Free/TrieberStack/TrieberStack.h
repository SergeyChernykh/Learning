#include <atomic>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <utility>

template <class T>
class TStack {
    struct Node {
        T val;
        Node *next_;
        Node *retired_next_;
    };

    class RetireRAII {
        public:
        RetireRAII(std::atomic<int>& active_counter, std::atomic<Node*>& retired_list) :
            active_counter_(active_counter),
            retired_list_(retired_list) {
            active_counter_.fetch_add(1, std::memory_order_acquire);
            }
        RetireRAII(const RetireRAII&) = delete;
        RetireRAII(RetireRAII&&) = delete;
        RetireRAII& operator=(const RetireRAII&) = delete;
        RetireRAII& operator=(RetireRAII&&) = delete;
        void SetNode(Node* node) {
            node_ = node;
        }

        ~RetireRAII() {
            if (node_ != nullptr) {
                Node *rhead = retired_list_.load(std::memory_order_relaxed);
                do {
                    node_->retired_next_ = rhead;
                } while (!retired_list_.compare_exchange_weak(rhead, node_, std::memory_order_release, std::memory_order_relaxed));
            }
            if (active_counter_.load(std::memory_order_relaxed) == 1) {
                Node *rhead = retired_list_.exchange(nullptr, std::memory_order_acquire);
                int old = active_counter_.fetch_sub(1, std::memory_order_acq_rel);
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
                Node* new_retire_list = retired_list_.load(std::memory_order_acquire);
                do {
                    retire_it->retired_next_ = new_retire_list;
                } while (!retired_list_.compare_exchange_weak(new_retire_list, rhead, std::memory_order_release, std::memory_order_relaxed));
            } else {
                active_counter_.fetch_sub(1, std::memory_order_release);
            }
        }
        private:
            void retire_list(Node* head) {
                while (head != nullptr) {
                    auto t = head;
                    head = head->retired_next_;
                    delete t;
                }
            }

        private:
            Node* node_ {nullptr};
            std::atomic<int>& active_counter_;
            std::atomic<Node*>& retired_list_;
    };
public:

    TStack() = default;
    TStack(const TStack<T>&) = delete;
    TStack& operator=(const TStack<T>&) = delete;
    TStack(TStack<T>&& other) = delete;
    TStack& operator=(TStack<T>&& other) = delete;
    ~TStack() {

        {
            Node *head = head_;
            while (head != nullptr) {
                auto t = head;
                head = head->next_;
                delete t;
            }
        }
        {
            Node *head = retired_list_;
            while (head != nullptr) {
                auto t = head;
                head = head->retired_next_;
                delete t;
            }
        }
    }

    void push(T value) {
        Node *node = new Node {std::move(value), nullptr, nullptr};
        Node *head = head_.load(std::memory_order_relaxed);
        do {
            node->next_ = head;
        } while (!head_.compare_exchange_weak(head, node, std::memory_order_release, std::memory_order_relaxed));
        return;
    }

    std::optional<T> try_pop() {
        RetireRAII retireRAII(active_counter_, retired_list_);
        Node *head = nullptr;
        Node *next = nullptr;
        do {
            head = head_.load(std::memory_order_acquire);
            if (head == nullptr) {
                return std::nullopt;
            }
            next = head->next_;
        } while (!head_.compare_exchange_weak(head, next, std::memory_order_relaxed, std::memory_order_relaxed));
        retireRAII.SetNode(head);
        return std::move(head->val);
    }

private:
    std::atomic<Node*> head_ {nullptr};
    std::atomic<int> active_counter_ {0};
    std::atomic<Node*> retired_list_ {nullptr};
};
