#pragma once

#ifdef SMART_PTR_USE_STD

#include <memory>
#include <utility>

template <class T, class Deleter = std::default_delete<T>>
using UniquePtr = std::unique_ptr<T, Deleter>;

template <class T>
using SharedPtr = std::shared_ptr<T>;

template <class T>
using WeakPtr = std::weak_ptr<T>;

template <class T, class... Args>
UniquePtr<T> makeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

#else

#include "my_smart_ptr.hpp"

#endif
