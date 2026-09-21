#ifndef FLATMAP_HEADER
#define FLATMAP_HEADER "FlatMap.h"
#endif
#include FLATMAP_HEADER
#include <cassert>

struct Key { int id; };
struct KeyLess {
    bool operator()(const Key& a, const Key& b) const { return a.id < b.id; }
};

int main() {
    FlatMap<int, int> map;
    assert(map.empty());
    assert(map.size() == 0);
    assert(map.find(1) == nullptr);
    assert(!map.erase(1));

    assert(map.insert(5, 50));
    assert(map.insert(2, 20));
    assert(map.insert(9, 90));
    assert(!map.insert(5, 500));
    assert(map.size() == 3);
    const auto& read = map;
    for (int key : {2, 5, 9}) {
        const auto* value = read.find(key);
        assert(value && *value == key * 10);
    }
    for (int key : {1, 6, 12}) assert(read.find(key) == nullptr);
    auto* value = map.find(5);
    assert(value);
    *value = 51;
    assert(*read.find(5) == 51);

    assert(map.erase(5));
    assert(read.find(5) == nullptr);
    assert(!map.erase(5));
    assert(map.erase(2));
    assert(map.erase(9));
    assert(map.empty());

    FlatMap<int, int, std::greater<int>> reverse;
    for (int key : {2, 9, 5}) assert(reverse.insert(key, key));
    for (int key : {2, 9, 5}) {
        auto* result = reverse.find(key);
        assert(result && *result == key);
    }
    for (int key : {1, 6, 12}) assert(reverse.find(key) == nullptr);
    assert(!reverse.insert(5, 100));
    assert(reverse.erase(5));
    assert(reverse.find(5) == nullptr);

    FlatMap<Key, int, KeyLess> custom;
    assert(custom.insert(Key{3}, 30));
    assert(!custom.insert(Key{3}, 99));
    auto* result = custom.find(Key{3});
    assert(result && *result == 30);
    assert(custom.find(Key{4}) == nullptr);
    assert(custom.erase(Key{3}));
    assert(custom.empty());

    // Repeated front insertions and erasures must preserve key/value alignment.
    FlatMap<int, int> growing;
    for (int key = 255; key >= 0; --key) assert(growing.insert(key, key + 1000));
    for (int key = 0; key < 256; key += 2) assert(growing.erase(key));
    assert(growing.size() == 128);
    for (int key = 0; key < 256; ++key) {
        auto* result = growing.find(key);
        if (key % 2 == 0) assert(result == nullptr);
        else assert(result && *result == key + 1000);
    }
}
