#pragma once

// Это стартовый режим: тесты подключены к стандартной библиотеке и должны
// проходить сразу. Заменяйте alias-шаблоны своими классами по одному.
//
// Имена и публичный интерфейс должны быть совместимы с соответствующим
// подмножеством std::unique_ptr, std::shared_ptr и std::weak_ptr, которое
// используется в tests/smart_ptr_tests.cpp.

#include <memory>
#include <type_traits>
#include <iostream>
// template <class T, class D = std::default_delete<T>>
// using UniquePtr = std::unique_ptr<T, D>;

template <class T>
using SharedPtr = std::shared_ptr<T>;

template <class T>
using WeakPtr = std::weak_ptr<T>;

template <class T, class D = std::default_delete<T>>
class UniquePtr {
    void deleter_call() {
        if (ptr_ != nullptr) {
            deleter_(ptr_);
        }
    }
public:
    // constructor
    UniquePtr(T* ptr = nullptr, const D& deleter = D()) : ptr_{ptr}, deleter_ {deleter} {
    };
    // copy constructor - deleted
    UniquePtr(const UniquePtr<T, D>& other) = delete;
    // move constructor
    template <class U, class Del>
    requires requires (T* t, U* u) { t = u; }
    UniquePtr(UniquePtr<U, Del>&& other): 
        ptr_ {other.release()}, 
        deleter_{std::move(other.get_deleter())}{}

    // assigment - deleted
    UniquePtr<T, D>& operator=(const UniquePtr<T, D>& other) = delete;
    // move assigment
    UniquePtr<T, D>& operator=(UniquePtr<T, D>&& other) {
        if (&other == this) {
            return *this;
        }
        deleter_call();
        ptr_ = other.ptr_;
        deleter_ = other.deleter_;
        other.ptr_ = nullptr;
        return *this;
    }
    // destructor
    ~UniquePtr() {
        deleter_call();
    }

    // methods
    const T* get() const {
        return ptr_;
    }

    T* get() {
        return ptr_;
    }

    const D& get_deleter() const {
        return deleter_;
    }

    D& get_deleter() {
        return deleter_;
    }

    T* release() {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }

    void reset(T* other = nullptr) {
        deleter_call();
        ptr_ = other;
    }

    void swap(UniquePtr<T, D>& other) {
        std::swap(ptr_, other.ptr_);
        std::swap(deleter_, other.deleter_);
    }

    // operators
    const T& operator*() const {
        return *ptr_;
    }

    T& operator*() {
        return *ptr_;
    }

    const T* operator->() const {
        return ptr_;
    }

    T* operator->() {
        return ptr_;
    }

    // operators
    operator bool() const {
        return ptr_ != nullptr;
    }
private:
    T* ptr_ {nullptr};
    D deleter_;
};

template <class T, class D>
class UniquePtr<T[], D> {
    void deleter_call() {
        if (ptr_ != nullptr) {
            deleter_(ptr_);
        }
    }
public:
    // constructor
    UniquePtr(T* ptr = nullptr, const D& deleter = D()) : deleter_ {deleter}, ptr_{ptr} {};
    UniquePtr(T* ptr, D&& deleter) : deleter_ {deleter},  ptr_{ptr} {};

    // copy constructor - deleted
    UniquePtr(const UniquePtr<T[], D>& other) = delete;
    // move constructor
    template <class U, class Del>
    requires requires (T* t, U* u) { t = u; }
    UniquePtr(UniquePtr<U[], Del>&& other): 

    
        deleter_{std::move(other.get_deleter())}, 
        ptr_ {other.release()} {}

    // assigment - deleted
    UniquePtr<T[], D>& operator=(const UniquePtr<T[], D>& other) = delete;
    // move assigment
    UniquePtr<T[], D>& operator=(UniquePtr<T[], D>&& other) {
        if (&other == this) {
            return *this;
        }
        deleter_call();
        ptr_ = other.ptr_;
        deleter_ = other.deleter_;
        other.ptr_ = nullptr;
        return *this;
    }
    // destructor
    ~UniquePtr() {
        deleter_call();
    }

    // methods
    const T* get() const {
        return ptr_;
    }

    T* get() {
        return ptr_;
    }

    const D& get_deleter() const {
        return deleter_;
    }

    D& get_deleter_() {
        return deleter_;
    }

    T* release() {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }

    void reset(T* other = nullptr) {
        deleter_call();
        ptr_ = other;
    }

    void swap(UniquePtr<T[], D>& other) {
        std::swap(ptr_, other.ptr_);
        std::swap(deleter_, other.deleter_);
    }

    // operators
    const T& operator*() const {
        return *ptr_;
    }

    T& operator*() {
        return *ptr_;
    }

    const T* operator->() const {
        return ptr_;
    }

    T* operator->() {
        return ptr_;
    }

    // operators
    operator bool() const {
        return ptr_ != nullptr;
    }

    T& operator[](size_t i) {
        return ptr_[i];
    }

    const T& operator[](size_t i) const {
        return ptr_[i];
    }

private:
    D deleter_;
    T* ptr_ {nullptr};
};

template<class T, class... Args>
UniquePtr<T> makeUnique(Args&&... argv) {
    return UniquePtr<T>(new T(std::forward<Args>(argv)...));
}
