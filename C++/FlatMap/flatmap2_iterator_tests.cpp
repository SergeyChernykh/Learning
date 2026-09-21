#include "FlatMap2.h"

#include <cassert>
#include <concepts>
#include <iostream>
#include <iterator>
#include <memory>
#include <utility>

// Build with -std=c++20 or -std=c++23. Check both declarations and bodies:
// concept checks alone need not instantiate the bodies of iterator operations.
using Map = FlatMap<int, int>;
static_assert(std::random_access_iterator<Map::iterator>);
static_assert(std::random_access_iterator<Map::const_iterator>);
static_assert(std::convertible_to<Map::iterator, Map::const_iterator>);
static_assert(!std::convertible_to<Map::const_iterator, Map::iterator>);

template<class I, class C>
constexpr bool mixed_operations = requires(I i, C c) {
    { i == c } -> std::same_as<bool>;
    { c == i } -> std::same_as<bool>;
    { i < c } -> std::same_as<bool>;
    { c < i } -> std::same_as<bool>;
    { i - c } -> std::same_as<std::iter_difference_t<I>>;
    { c - i } -> std::same_as<std::iter_difference_t<I>>;
};

template<class M>
void check_positions(M& map) {
    const auto first = map.begin();
    const auto last = map.end();
    using D = std::iter_difference_t<decltype(first)>;
    const auto size = static_cast<D>(map.size());
    assert(last - first == size);
    assert(first - last == -size);
    for (D a = 0; a <= size; ++a) {
        for (D b = 0; b <= size; ++b) {
            const auto left = first + a;
            const auto right = first + b;
            const D n = b - a;
            assert(left + n == right);
            assert(n + left == right);
            assert(left - (-n) == right);
            assert(right - left == n);
            assert((left < right) == (a < b));
            assert((left > right) == (a > b));
            assert((left <= right) == (a <= b));
            assert((left >= right) == (a >= b));
            auto moved = left;
            assert(std::addressof(moved += n) == std::addressof(moved));
            assert(moved == right);
            assert(std::addressof(moved -= n) == std::addressof(moved));
            assert(moved == left);
            if (b < size) {
                assert(std::addressof(left[n].first) == std::addressof((*right).first));
                assert(std::addressof(left[n].second) == std::addressof(right->second));
            }
        }
    }
}

int main() {
    // These diagnostics separate int-only success from generic type support.
    using MoveOnly = FlatMap<int, std::unique_ptr<int>>;
    std::cout << std::boolalpha
              << "move-only iterator random access: "
              << std::random_access_iterator<MoveOnly::iterator> << '\n'
              << "move-only const_iterator random access: "
              << std::random_access_iterator<MoveOnly::const_iterator> << '\n'
              << "mixed iterator/const_iterator operations: "
              << mixed_operations<Map::iterator, Map::const_iterator> << '\n';

    assert(Map::iterator{} == Map::iterator{});
    assert(Map::const_iterator{} == Map::const_iterator{});
    Map map;
    check_positions(map);
    check_positions(std::as_const(map));
    for (int key : {3, 1, 2}) assert(map.insert(key, key * 10));
    check_positions(map);
    check_positions(std::as_const(map));
    auto it = map.begin();
    const auto old = it++;
    assert(old == map.begin() && it == map.begin() + 1);
    assert(it-- == map.begin() + 1 && it == map.begin());
    assert(std::addressof(++it) == std::addressof(it));
    assert(std::addressof(--it) == std::addressof(it));
    auto view = *it;
    view.second = 42;
    assert(*map.find(1) == 42);
    assert(map.cend()[-1].first == 3);
    assert(map.rbegin()->first == 3);
    assert(map.crbegin()[1].first == 2);
    assert(map.rend() - map.rbegin() == 3);
    std::cout << "Runtime iterator checks passed\n";
}
