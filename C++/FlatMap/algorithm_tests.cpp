#include "FlatMap.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <numeric>
#include <vector>

// Behavioral smoke tests, not proof of all standard iterator requirements.
int main() {
    FlatMap<int, int> map;
    for (int key : {5, 2, 9}) assert(map.insert(key, key * 10));
    const auto& read = map;
    auto found = std::find_if(read.begin(), read.end(),
        [](const auto& entry) { return entry.first == 5; });
    assert(found == read.begin() + 1);
    assert(std::count_if(read.begin(), read.end(),
        [](const auto& entry) { return entry.second >= 50; }) == 2);
    assert(std::accumulate(read.begin(), read.end(), 0,
        [](int sum, const auto& entry) { return sum + entry.second; }) == 160);
    std::vector<int> values;
    std::transform(read.begin(), read.end(), std::back_inserter(values),
        [](const auto& entry) { return entry.second; });
    assert((values == std::vector<int>{20, 50, 90}));
    std::for_each(map.begin(), map.end(), [](auto entry) { ++entry.second; });
    assert(*read.find(2) == 21);
    assert(*read.find(5) == 51);
    assert(*read.find(9) == 91);
}
