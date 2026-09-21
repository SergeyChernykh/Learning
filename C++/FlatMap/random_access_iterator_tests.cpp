#include "FlatMap.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

// Random-access operations with proxy references.
using Map = FlatMap<int, int>;
using It = Map::const_iterator;
using D = std::iterator_traits<It>::difference_type;
static_assert(std::is_same_v<std::iterator_traits<It>::iterator_category,
                             std::random_access_iterator_tag>);
static_assert(std::is_same_v<decltype(std::declval<It&>() += D{}), It&>);
static_assert(std::is_same_v<decltype(std::declval<It&>() -= D{}), It&>);
static_assert(std::is_same_v<decltype(std::declval<const It&>() + D{}), It>);
static_assert(std::is_same_v<decltype(D{} + std::declval<const It&>()), It>);
static_assert(std::is_same_v<decltype(std::declval<const It&>() - D{}), It>);
static_assert(std::is_same_v<decltype(std::declval<const It&>() -
                                    std::declval<const It&>()), D>);
static_assert(std::is_same_v<decltype(std::declval<const It&>()[D{}]),
                             std::iterator_traits<It>::reference>);
static_assert(!std::is_reference_v<std::iterator_traits<It>::reference>);
static_assert(std::is_same_v<decltype(std::declval<const It&>()[D{}].first), const int&>);
static_assert(std::is_same_v<decltype(std::declval<const It&>()[D{}].second), const int&>);
static_assert(std::is_same_v<decltype(std::declval<const It&>() <
                                    std::declval<const It&>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const It&>() >
                                    std::declval<const It&>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const It&>() <=
                                    std::declval<const It&>()), bool>);
static_assert(std::is_same_v<decltype(std::declval<const It&>() >=
                                    std::declval<const It&>()), bool>);

template<class M>
void check_positions(M& map) {
    using Iter = decltype(map.begin());
    using Diff = typename std::iterator_traits<Iter>::difference_type;
    static_assert(std::is_same_v<decltype(std::declval<Iter&>() += Diff{}), Iter&>);
    static_assert(std::is_same_v<decltype(std::declval<Iter&>() -= Diff{}), Iter&>);
    const Diff count = static_cast<Diff>(map.size());
    const auto begin = map.begin();
    const auto end = map.end();
    assert(end - begin == count);
    assert(begin - end == -count);
    assert(std::distance(end, begin) == -count);

    // Include end() as a position, but never dereference it.
    // Ordering is positional, independent of the map's key comparator.
    for (Diff a = 0; a <= count; ++a) {
        const auto left = begin + a;
        assert(a + begin == left);
        assert(end - (count - a) == left);
        assert(left + 0 == left);
        assert(left - 0 == left);
        for (Diff b = 0; b <= count; ++b) {
            const auto right = begin + b;
            const Diff offset = b - a;
            assert(right - left == offset);
            assert((left < right) == (a < b));
            assert((left > right) == (a > b));
            assert((left <= right) == (a <= b));
            assert((left >= right) == (a >= b));
            assert(left + offset == right);
            assert(offset + left == right);
            assert(left - (-offset) == right);

            auto moved = left;
            assert(std::addressof(moved += offset) == std::addressof(moved));
            assert(moved == right);
            assert(std::addressof(moved -= offset) == std::addressof(moved));
            assert(moved == left);
            std::advance(moved, offset);
            assert(moved == right);

            // Negative indexing is valid when the target is an actual element.
            if (b < count) {
                auto indexed = left[offset];
                auto dereferenced = *right;
                assert(std::addressof(indexed.first) == std::addressof(dereferenced.first));
                assert(std::addressof(indexed.second) == std::addressof(dereferenced.second));
                assert(std::addressof(left[offset].second) == map.find(right->first));
            }
        }
        assert(left == begin + a); // Arithmetic must not modify its operands.
    }
    assert(begin == map.begin());
    assert(end == map.end());
}

int main() {
    Map empty;
    check_positions(empty);
    check_positions(std::as_const(empty));
    Map single;
    assert(single.insert(7, 70));
    check_positions(single);
    check_positions(std::as_const(single));

    Map map;
    for (int key : {9, 2, 12, 5, 7}) assert(map.insert(key, key * 10));
    check_positions(map);
    check_positions(std::as_const(map));
    const auto begin = map.cbegin();
    const auto end = map.cend();
    assert(begin[0].first == 2);
    assert(begin[3].first == 9);
    assert(end[-1].first == 12);
    assert(end[-5].first == 2);
    assert(std::next(begin, 5) == end);
    assert(std::prev(end, 5) == begin);

    // Exercise a standard algorithm and a random-access iterator adapter.
    const auto found = std::lower_bound(begin, end, 7,
        [](const auto& entry, int key) { return entry.first < key; });
    assert(found == begin + 2);
    assert(std::lower_bound(begin, end, 99,
        [](const auto& entry, int key) { return entry.first < key; }) == end);
    const auto reverse_begin = std::make_reverse_iterator(end);
    const auto reverse_end = std::make_reverse_iterator(begin);
    assert(reverse_end - reverse_begin == 5);
    assert(reverse_begin[0].first == 12);
    assert(reverse_begin[4].first == 2);
    assert((reverse_begin + 2)->first == 7);

    FlatMap<int, int, std::greater<int>> descending;
    for (int key : {9, 2, 12, 5, 7}) assert(descending.insert(key, key));
    check_positions(descending);
    check_positions(std::as_const(descending));
    assert(descending.begin()[0].first == 12);
    assert(descending.begin()[4].first == 2);

    // No arithmetic/ordering across containers, no invalidated iterators,
    // and no positions outside [begin, end]. Complexity needs code review.
}
