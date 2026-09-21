#include "FlatMap.h"

#include <cassert>
#include <memory>
#include <type_traits>

using Map = FlatMap<int, int>;
using I = Map::iterator;
using C = Map::const_iterator;
static_assert(std::is_convertible_v<I, C>);
static_assert(!std::is_constructible_v<I, C>);
static_assert(!std::is_assignable_v<I&, C>);
static_assert(std::is_copy_constructible_v<I>);
static_assert(std::is_copy_constructible_v<C>);

int main() {
    Map map;
    assert(map.insert(2, 20));
    assert(map.insert(5, 50));
    auto source = map.begin() + 1;
    C converted = source;
    assert(converted == map.cbegin() + 1);
    assert(std::addressof(converted->second) == map.find(5));
    source->second = 51;
    assert(converted->second == 51);
    --source;
    assert(source->first == 2);
    assert(converted->first == 5);
    C assigned;
    assigned = source;
    assert(assigned == map.cbegin());
    C end = map.end();
    assert(end == map.cend());
    C unbound = I{};
    assert(unbound == C{});
    Map empty;
    C empty_begin = empty.begin();
    assert(empty_begin == empty.cend());
    Map::const_reverse_iterator reverse = map.rbegin();
    assert(reverse->first == 5);
}
