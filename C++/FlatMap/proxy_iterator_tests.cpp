#include "FlatMap.h"

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

// Proxy access through dereference, indexing, and arrow.
using Map = FlatMap<int, int>;
using MutableView = decltype(*std::declval<const Map::iterator&>());
using ConstView = decltype(*std::declval<const Map::const_iterator&>());
static_assert(!std::is_reference_v<MutableView>);
static_assert(!std::is_reference_v<ConstView>);
static_assert(std::is_same_v<decltype(std::declval<MutableView>().first), const int&>);
static_assert(std::is_same_v<decltype(std::declval<MutableView>().second), int&>);
static_assert(std::is_same_v<decltype(std::declval<ConstView>().first), const int&>);
static_assert(std::is_same_v<decltype(std::declval<ConstView>().second), const int&>);
static_assert(std::is_same_v<Map::iterator::value_type, std::pair<int, int>>);
static_assert(std::is_same_v<Map::const_iterator::value_type, std::pair<int, int>>);
static_assert(!std::is_assignable_v<decltype((std::declval<Map::iterator>()->first)), int>);
static_assert(std::is_assignable_v<decltype((std::declval<const Map::iterator&>()->second)), int>);
static_assert(!std::is_assignable_v<decltype((std::declval<Map::const_iterator>()->second)), int>);

int main() {
    Map map;
    assert(map.insert(2, 20));
    assert(map.insert(5, 50));
    const auto it = map.begin(); // Const iterator object still permits value mutation.
    auto entry = *it;
    assert(entry.first == 2);
    assert(std::addressof(entry.second) == map.find(2));
    auto copy = entry;
    copy.second = 42;
    assert(entry.second == 42);
    assert(*map.find(2) == 42);
    auto&& temporary_view = *it;
    temporary_view.second = 43;
    assert(*map.find(2) == 43);

    const Map& read = map;
    auto read_entry = *read.begin();
    assert(std::addressof(read_entry.second) == read.find(2));
    entry.second = 44;
    assert(read_entry.second == 44);
    for (auto&& element : map) element.second += 1;
    assert(*read.find(2) == 45);
    assert(*read.find(5) == 51);

    // Indexing returns the same kind of view, including valid negative offsets.
    static_assert(std::is_same_v<decltype(it[0]), MutableView>);
    static_assert(std::is_same_v<decltype(read.begin()[0]), ConstView>);
    auto last = map.end()[-1];
    assert(last.first == 5);
    assert(std::addressof(last.second) == map.find(5));
    last.second = 99;
    assert(*read.find(5) == 99);
    assert(read.end()[-2].first == 2);
    assert(std::addressof(read.begin()[1].second) == read.find(5));

    it->second = 100;
    assert(*read.find(2) == 100);
    assert(read.begin()->first == 2);
    assert(std::addressof(read.begin()->second) == read.find(2));
    // A saved arrow wrapper owns its view, not a pointer to a dead local view.
    auto arrow = it.operator->();
    auto arrow_copy = arrow;
    arrow_copy->second = 101;
    assert(arrow->second == 101);
    assert(*read.find(2) == 101);
    map.rbegin()->second = 102;
    assert(read.crbegin()->second == 102);
    assert(*read.find(5) == 102);
}
