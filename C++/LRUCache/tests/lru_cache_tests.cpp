#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

// Exercise the original source without changing its standalone entry point.
#define main lru_exercise_main
#include "../LRUCache.cpp"
#undef main

void Check(bool condition) {
    if (!condition) throw std::runtime_error("LRU expectation failed");
}

void Expect(LRUCache<int, std::string>& cache, int key, const char* value) {
    const auto* actual = cache.Get(key);
    Check(actual != nullptr);
    Check(*actual == value);
}

template <typename Cache>
void CheckCopyOwnership() {
    // Either copying is disabled or each copy must own independent nodes.
    if constexpr (std::is_copy_constructible_v<Cache>) {
        Cache original(2);
        original.Set(1, std::string("one"));
        {
            Cache copy(original);
            Expect(copy, 1, "one");
        }
        Expect(original, 1, "one");
    }
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string_view test = argv[1];
    if (test == "basic") {
        LRUCache<int, std::string> cache(2);
        Check(!cache.Exist(1));
        Check(cache.Get(1) == nullptr);
        cache.Set(1, std::string("one"));
        Expect(cache, 1, "one");
    } else if (test == "update") {
        // Updating a key must preserve other entries and the new value.
        LRUCache<int, std::string> cache(3);
        cache.Set(1, std::string("one"));
        cache.Set(2, std::string("two"));
        cache.Set(2, std::string("updated"));
        cache.Set(3, std::string("three"));
        Check(cache.Exist(1));
        Expect(cache, 2, "updated");
    } else if (test == "get_head") {
        // Reading the oldest entry makes it the newest.
        LRUCache<int, std::string> cache(2);
        cache.Set(1, std::string("one"));
        cache.Set(2, std::string("two"));
        Expect(cache, 1, "one");
        cache.Set(3, std::string("three"));
        Check(cache.Exist(1));
        Check(!cache.Exist(2));
        Check(cache.Exist(3));
    } else if (test == "get_head_three") {
        // Three entries expose lost forward links hidden by a two-node list.
        LRUCache<int, std::string> cache(3);
        cache.Set(1, std::string("one"));
        cache.Set(2, std::string("two"));
        cache.Set(3, std::string("three"));
        Expect(cache, 1, "one");
        // Destruction must still reach and free all three nodes.
    } else if (test == "get_middle") {
        // Moving an interior entry must preserve all remaining links.
        LRUCache<int, std::string> cache(3);
        cache.Set(1, std::string("one"));
        cache.Set(2, std::string("two"));
        cache.Set(3, std::string("three"));
        Expect(cache, 2, "two");
        cache.Set(4, std::string("four"));
        cache.Set(5, std::string("five"));
        Check(!cache.Exist(1));
        Check(!cache.Exist(3));
        Expect(cache, 2, "two");
    } else if (test == "one") {
        LRUCache<int, std::string> cache(1);
        cache.Set(1, std::string("one"));
        cache.Set(1, std::string("updated"));
        Expect(cache, 1, "updated");
        cache.Set(2, std::string("two"));
        Check(!cache.Exist(1));
        Expect(cache, 2, "two");
    } else if (test == "zero") {
        // Contract assumed here: capacity zero stores no entries.
        LRUCache<int, std::string> cache(0);
        cache.Set(1, std::string("one"));
        Check(!cache.Exist(1));
        Check(cache.Get(1) == nullptr);
    } else if (test == "copy") {
        CheckCopyOwnership<LRUCache<int, std::string>>();
    } else if (test == "lifetime") {
        // LeakSanitizer should report no live allocations after destruction.
        for (int i = 0; i < 10; ++i) {
            LRUCache<int, std::string> cache(2);
            cache.Set(1, std::string("one"));
            cache.Set(2, std::string("two"));
        }
    } else {
        return 2;
    }
}
