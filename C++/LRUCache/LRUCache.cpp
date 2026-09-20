#include <cstddef>
#include <unordered_map>
template <typename K, typename V>
class LRUCache {
private:
    struct Node {
        K key_;
        Node *prev_ {nullptr};
        Node *next_ {nullptr};
    };
private:
    size_t size_ {0};
    size_t capacity_ {0};
    std::unordered_map<K, std::pair<V, Node*>> cache_;
    Node *head_ {nullptr};
    Node *tail_ {nullptr};

private:
    void Erase() {
        if (head_ == nullptr) {
            return;
        }
        Node *node = head_;
        head_ = head_->next_;
        if (head_ == nullptr) {
            tail_ = nullptr;
        } else {
            head_->prev_ = nullptr;
        }
        cache_.erase(node->key_);
        delete node;
        size_--;
    }
    void UpdateUsage(Node *node) {
        Node *prev = node->prev_;
        Node *next = node->next_;
        if (next == nullptr) {
            return;
        }
        if (prev == nullptr) {
            head_ = head_->next_;
            head_->prev_ = nullptr;
            tail_->next_ = node;
            node->prev_ = tail_;
            node->next_ = nullptr;
            tail_ = node;
            return;
        }
        prev->next_ =  next;
        next->prev_ = prev;
        node->next_ = nullptr;
        node->prev_ = nullptr;
        tail_->next_ = node;
        node->prev_ = tail_;
        tail_ = node;
    }
public:
    LRUCache (size_t capacity) : capacity_(capacity) {};
    ~LRUCache () {
        cache_.clear();
        while(head_!= nullptr) {
            Node *t = head_;
            head_ = head_->next_;
            delete t;
        }
    }
    
    void Set(const K& key, V&& value) {
        if (Exist(key)) {
            auto &[_, node] = cache_[key];
            UpdateUsage(node);
            cache_[key] = {std::move(value), node};
            return;
        }
        if (capacity_ == size_) {
            if (capacity_ == 0) {
                return;
            }
            Erase();
        }
        size_++;
        Node *node = new Node();
        node->key_ = key;
        if (tail_ == nullptr) {
            tail_ = node;
            head_ = node;
        } else {
            tail_->next_ = node;
            node->prev_ = tail_;
            tail_ = node;
        }
        cache_[key] = {std::move(value), node};
    };

    V const* Get(const K& key) {
        if (!Exist(key)) {
            return nullptr;
        }
        auto& [_, node] = cache_[key];
        UpdateUsage(node);
        return &cache_[key].first;
    };

    bool Exist(const K& key) {
        return cache_.contains(key);
    }
};

int main() {return 0;}