#include <functional>
#include <algorithm>
#include <vector>
#include <utility>

#pragma once

template <typename Key, typename Value, typename Comp = std::less<Key>>
class FlatMap {
private:
    std::vector<Key> keyStorage_;
    std::vector<Value> valueStorage_;
    auto findIt(const Key& key) const {
        const auto& it = std::lower_bound(keyStorage_.begin(), keyStorage_.end(), key, [](const Key &a, const Key &key) {
            return Comp()(a, key);
        });
        return it;
    }
public:
    const Value* find(const Key& key) const {
        const auto &it = findIt(key);
        if (it == keyStorage_.end() || Comp()(key, *it)) {
            return nullptr;
        }
        return &valueStorage_[std::distance(keyStorage_.cbegin(), it)];
    }
    Value* find(const Key& key) {
        const auto &t = *this;
        const Value* v = t.find(key);
        return const_cast<Value*>(v);
    }

    bool insert(Key key, Value value) { 
        const auto& it = findIt(key);
        if (it != keyStorage_.end() && !Comp()(key, *it)) {
            return false;
        }
        valueStorage_.emplace(valueStorage_.begin() + std::distance(keyStorage_.cbegin(), it), std::move(value));
        keyStorage_.emplace(it, std::move(key));
        return true;
    }

    bool erase(const Key& key) {
        const auto& it = findIt(key);
        if (it == keyStorage_.end() || Comp()(key, *it)) {
            return false;
        }
        valueStorage_.erase(valueStorage_.begin() + std::distance(keyStorage_.cbegin(), it));
        keyStorage_.erase(it);
        return true;
    }

    std::size_t size() const noexcept {
        return keyStorage_.size();
    }

    bool empty() const noexcept {
        return keyStorage_.empty();
    }
    template<bool IsConst>
    class base_iter {
    template<bool> friend class base_iter;
    private:
        using Proxy = std::pair<const Key&, std::conditional_t<IsConst, const Value&, Value&>>;
        struct ProxyBox {
            Proxy proxy;
            Proxy *operator->() {
                return &proxy;
            }
        };
    private:
        using map_type = std::conditional_t<IsConst, const FlatMap<Key, Value, Comp>*, FlatMap<Key, Value, Comp>*>;
        std::size_t i {0};
        map_type fmap_{nullptr};
    public:
        using value_type = std::pair<Key, Value>;
        using reference = Proxy;
        using pointer = ProxyBox;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::random_access_iterator_tag;
        base_iter() {};
        base_iter(map_type fmap) : fmap_(fmap) {}
        base_iter(map_type fmap, std::size_t idx) : i(idx), fmap_(fmap) {}
        template<bool OtherConst,
            std::enable_if_t<IsConst && !OtherConst, int> = 0>
        base_iter(const base_iter<OtherConst>& other) : i(other.i), fmap_(other.fmap_) {}
        base_iter operator++(int) {
            auto tmp = *this;
            i++;
            return tmp;
        }
        base_iter operator--(int) {
            auto tmp = *this;
            i--;
            return tmp;
        }
        base_iter& operator++() {
            i++;
            return *this;
        }
        base_iter& operator--() {
            i--;
            return *this;
        }
        base_iter operator+(difference_type n) const {
            auto tmp = *this;
            tmp.i += n;
            return tmp;
        }
        base_iter operator-(difference_type n) const {
            auto tmp = *this;
            tmp.i -= n;
            return tmp;
        }
        friend base_iter operator+(difference_type n, const base_iter &it) {
            return it + n;
        }
        difference_type operator-(const base_iter& it) const {
            return static_cast<difference_type>(i) - static_cast<difference_type>(it.i);
        }
        base_iter& operator+=(difference_type n) {
            i += n;
            return *this;
        }
        base_iter& operator-=(difference_type n) {
            i -= n;
            return *this;
        }
        
        bool operator==(const base_iter& it) const {
            return fmap_ == it.fmap_ && i == it.i;
        }
        bool operator!=(const base_iter& it) const {
            return !(*this == it);
        }
        bool operator>(const base_iter& it) const {
            return fmap_ == it.fmap_ && i > it.i;
        }
        bool operator>=(const base_iter& it) const {
            return fmap_ == it.fmap_ && i >= it.i;
        }
        bool operator<(const base_iter& it) const {
            return fmap_ == it.fmap_ && i < it.i;
        }
        bool operator<=(const base_iter& it) const {
            return fmap_ == it.fmap_ && i <= it.i;
        }
        reference operator*() const {
            return {fmap_->keyStorage_[i], fmap_->valueStorage_[i]};
        }
        reference operator[](difference_type n) const {
            return {fmap_->keyStorage_[i+n], fmap_->valueStorage_[i+n]};
        }
        pointer operator->() const {
            return ProxyBox {{fmap_->keyStorage_[i], fmap_->valueStorage_[i]}};
        }
    };
    using iterator = base_iter<false>;
    using const_iterator = base_iter<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() {
        return iterator(this);
    }
    const_iterator begin() const {
        return const_iterator(this);
    }
    const_iterator cbegin() const {
        return const_iterator(this);
    }
    iterator end() {
        return iterator(this, keyStorage_.size());
    }
    const_iterator end() const {
        return const_iterator(this, keyStorage_.size());
    }
    const_iterator cend() const {
        return const_iterator(this, keyStorage_.size());
    }
    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }
    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }
    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(cend());
    }
    reverse_iterator rend() {
        return reverse_iterator(begin());
    }
    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }
    const_reverse_iterator crend() const {
        return const_reverse_iterator(cbegin());
    }
};
