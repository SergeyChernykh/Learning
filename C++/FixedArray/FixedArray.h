#include <cstddef>
#include <utility>
#include <exception>
#include <new>
#include <stdexcept>
#include <iterator>

#pragma once

using size_t = std::size_t;
template <typename T, size_t N>
class FixedArray {
private:
    size_t capacity_{N};
    void *buffer_ {nullptr};
    size_t size_ {0};
private:
    T* Cast() const {
        return reinterpret_cast<T*>(buffer_);
    }

    T* GetElem(size_t i) const {
        return Cast() + i;
    }
public:
    FixedArray(): buffer_(::operator new (sizeof(T)*capacity_, std::align_val_t{alignof(T)})) {};
    FixedArray(const FixedArray<T, N>&) = delete;
    FixedArray(FixedArray<T, N>&&) = delete;
    FixedArray<T, N>& operator=(const FixedArray<T, N>&) = delete;
    FixedArray<T, N>& operator=(FixedArray<T, N>&&) = delete;

    ~FixedArray() {
        for (size_t i = 0; i < size_; ++i) {
            GetElem(i)->~T();
        }
        ::operator delete (buffer_, std::align_val_t{alignof(T)});
    }
    
    size_t size() const {
        return size_;
    }

    size_t capacity() const {
        return capacity_;
    }

    void push_back(T val) {
        if (size_ == capacity_) {
            throw std::runtime_error("FixedArray is full");
        }
        new (GetElem(size_)) T(std::move(val));
        size_++;
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            throw std::runtime_error("FixedArray is full");
        }
        new (GetElem(size_)) T(std::forward<Args>(args)...);
        size_++;
    }
    
    void erase_back() {
        if (size_ == 0) {
            throw std::runtime_error("FixedArray is empty");
        }
        GetElem(--size_)->~T();
    }

    T& operator[] (size_t i) {
        if (i >= size_) {
            throw std::runtime_error("Out of bound exception");
        }
        return *GetElem(i);
    }

    const T& operator[] (size_t i) const {
        if (i >= size_) {
            throw std::runtime_error("Out of bound exception");
        }
        return *GetElem(i);
    }

    T* begin() {
        return GetElem(0);
    }

    T* end() {
        return GetElem(size_);
    }

    
    const T* cbegin() const {
        return GetElem(0);
    }
    
    const T* cend() const {
        return GetElem(size_);
    }

    const T* begin() const {
        return cbegin();
    }

    const T* end() const {
        return cend();
    }

    using reverse_iterator = std::reverse_iterator<T*>;
    using const_reverse_iterator = std::reverse_iterator<const T*>;

    reverse_iterator rbegin() {
        return reverse_iterator{GetElem(size_)};
    }

    reverse_iterator rend() {
        return reverse_iterator{GetElem(0)};
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator{GetElem(size_)};
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator{GetElem(0)};
    }

    const_reverse_iterator crbegin() const {
        return const_reverse_iterator{GetElem(size_)};
    }

    const_reverse_iterator  crend() const {
        return const_reverse_iterator{GetElem(0)};
    }
};