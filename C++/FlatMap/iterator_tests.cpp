#include "FlatMap.h"

#include <cassert>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

// Bidirectional operations with proxy references and separate mutable iterators.
using Map = FlatMap<int, int>;
using It = Map::const_iterator;
using Traits = std::iterator_traits<It>;
static_assert(std::is_default_constructible_v<It>);
// Range endpoints must be independent iterator values, not dangling references.
static_assert(std::is_same_v<decltype(std::declval<const Map&>().begin()), It>);
static_assert(std::is_same_v<decltype(std::declval<const Map&>().end()), It>);
static_assert(std::is_same_v<decltype(std::declval<const Map&>().cbegin()), It>);
static_assert(std::is_same_v<decltype(std::declval<const Map&>().cend()), It>);
static_assert(std::is_same_v<Traits::value_type, std::pair<int, int>>);
static_assert(!std::is_reference_v<Traits::reference>);
static_assert(std::is_same_v<decltype(std::declval<Traits::reference>().first), const int&>);
static_assert(std::is_same_v<decltype(std::declval<Traits::reference>().second), const int&>);
static_assert(std::is_same_v<decltype(std::declval<const It&>().operator->()), Traits::pointer>);
static_assert(std::is_same_v<Traits::difference_type, std::ptrdiff_t>);
static_assert(std::is_base_of_v<std::bidirectional_iterator_tag,
                               Traits::iterator_category>);
static_assert(std::is_same_v<decltype(*std::declval<const It&>()), Traits::reference>);
static_assert(std::is_same_v<decltype(++std::declval<It&>()), It&>);
static_assert(std::is_same_v<decltype(--std::declval<It&>()), It&>);
static_assert(std::is_same_v<decltype(std::declval<It&>()++), It>);
static_assert(std::is_same_v<decltype(std::declval<It&>()--), It>);
static_assert(!std::is_assignable_v<decltype((std::declval<It>()->first)), int>);
static_assert(!std::is_assignable_v<decltype((std::declval<It>()->second)), int>);

int main() {
    // Empty ranges and value-initialized iterators can be compared, not dereferenced.
    assert(It{} == It{});
    Map map;
    assert(map.begin() == map.end());
    assert(map.cbegin() == map.cend());
    const Map& read = map;
    assert(read.begin() == read.end());
    assert(read.rbegin() == read.rend());
    assert(read.crbegin() == read.crend());

    assert(map.insert(5, 50));
    assert(map.insert(2, 20));
    assert(map.insert(9, 90));
    // Compare after conversion; mixed-specialization operators are a separate contract.
    const It converted_begin = map.begin();
    assert(converted_begin == read.begin());
    assert(map.cbegin() == read.begin());
    assert(map.cend() == read.end());

    // Views are temporary objects, but their fields alias the stored elements.
    const It first = read.begin();
    assert(first->first == 2);
    auto view = *first;
    auto arrow = first.operator->();
    assert(std::addressof(view.first) == std::addressof(arrow->first));
    assert(std::addressof(view.second) == std::addressof(arrow->second));
    assert(std::addressof(first->second) == read.find(2));

    // Copies advance independently; postfix returns the previous position.
    auto it = first;
    auto old = it++;
    assert(old == first);
    assert(it != first);
    assert(it->first == 5);
    assert(std::addressof(++it) == std::addressof(it));
    assert(it->first == 9);
    assert(++it == read.end());
    assert(std::addressof(--it) == std::addressof(it));
    assert(it->first == 9);
    old = it--;
    assert(old->first == 9);
    assert(it->first == 5);
    assert(--it == first);

    // Assignment must copy position and return the destination object.
    auto assigned = read.end();
    assert(std::addressof(assigned = first) == std::addressof(assigned));
    assert(assigned == first);
    assert(std::addressof(assigned = read.end()) == std::addressof(assigned));
    assert(assigned == read.end());
    const auto same = first;
    assert(first == same);
    assert(!(first != same));

    // Standard algorithms and range-for must see the same sorted range.
    assert(std::distance(read.begin(), read.end()) == 3);
    assert(std::next(read.begin(), 3) == read.end());
    assert(std::prev(read.end())->first == 9);
    int expected[] = {2, 5, 9};
    int index = 0;
    for (const auto& entry : map) {
        assert(index < 3);
        assert(entry.first == expected[index++]);
        assert(entry.second == entry.first * 10);
    }
    assert(index == 3);

    // Reverse endpoints wrap forward endpoints; base() is one past the element.
    assert(read.rbegin().base() == read.end());
    assert(read.rend().base() == read.begin());
    assert(read.crbegin() == read.rbegin());
    assert(read.crend() == read.rend());
    index = 3;
    for (auto reverse = read.rbegin(); reverse != read.rend(); ++reverse) {
        assert(index > 0);
        assert(reverse->first == expected[--index]);
    }
    assert(index == 0);

    FlatMap<int, int, std::greater<int>> descending;
    for (int key : {2, 9, 5}) assert(descending.insert(key, key));
    index = 3;
    for (const auto& entry : descending) {
        assert(index > 0);
        assert(entry.first == expected[--index]);
    }
    assert(index == 0);

    Map single;
    assert(single.insert(7, 70));
    assert(std::next(single.begin()) == single.end());
    assert(std::prev(single.end()) == single.begin());
    assert(single.erase(7));
    // Obtain fresh iterators after mutation; never inspect invalidated ones.
    assert(single.begin() == single.end());
}
