// Standalone regression tests for ownership, construction failure and forwarding.
#include "FixedArray.h"
#include "FixedArray.h" // The header must also support repeated inclusion.
#include <cassert>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

using Array = FixedArray<int, 2>;
static_assert(!std::is_copy_constructible_v<Array>);
static_assert(!std::is_copy_assignable_v<Array>);
static_assert(!std::is_move_constructible_v<Array>);
static_assert(!std::is_move_assignable_v<Array>);

struct Tracked {
    inline static int live = 0;
    inline static bool fail_move = false;
    int value;

    explicit Tracked(int v) : value(v) {
        if (v < 0) throw std::runtime_error("construction failed");
        ++live;
    }
    Tracked(const Tracked&) = delete;
    Tracked(Tracked&& other) : value(other.value) {
        if (fail_move) throw std::runtime_error("move failed");
        ++live;
    }
    ~Tracked() { --live; }
};

struct Category {
    bool from_lvalue;
    explicit Category(int&) : from_lvalue(true) {}
    explicit Category(int&&) : from_lvalue(false) {}
};

struct alignas(64) AlignedItem {
    int value;
    explicit AlignedItem(int v) : value(v) {}
};

int main() {
    {
        FixedArray<Tracked, 2> a;
        bool threw = false;
        try { a.erase_back(); }
        catch (const std::runtime_error&) { threw = true; }
        assert(threw && a.size() == 0 && Tracked::live == 0);
        a.emplace_back(42);
        a.erase_back();
        assert(a.size() == 0 && Tracked::live == 0);
        threw = false;
        try { a.erase_back(); }
        catch (const std::runtime_error&) { threw = true; }
        assert(threw && a.size() == 0 && Tracked::live == 0);
        a.emplace_back(7);
        assert(a.size() == 1 && a[0].value == 7);
        assert(std::as_const(a)[0].value == 7);
        threw = false;
        try { (void)a[a.size()]; }
        catch (const std::runtime_error&) { threw = true; }
        assert(threw && a.size() == 1 && Tracked::live == 1);
        threw = false;
        try { (void)std::as_const(a)[a.size()]; }
        catch (const std::runtime_error&) { threw = true; }
        assert(threw && a.size() == 1 && Tracked::live == 1);
    }
    assert(Tracked::live == 0);
    {
        FixedArray<AlignedItem, 4> a;
        for (int i = 0; i < 4; ++i) {
            a.emplace_back(i);
            assert(reinterpret_cast<std::uintptr_t>(&a[i]) % alignof(AlignedItem) == 0);
            assert(a[i].value == i);
        }
        // ASan also checks that allocation and deallocation match at scope exit.
    }
    {
        FixedArray<Tracked, 2> a;
        a.emplace_back(7);
        bool threw = false;
        try { a.emplace_back(-1); }
        catch (const std::runtime_error&) { threw = true; }
        assert(threw && a.size() == 1 && Tracked::live == 1);
        assert(a[0].value == 7);

        Tracked::fail_move = true;
        threw = false;
        try { a.push_back(Tracked(8)); }
        catch (const std::runtime_error&) { threw = true; }
        Tracked::fail_move = false;
        assert(threw && a.size() == 1 && Tracked::live == 1);
        assert(a[0].value == 7);

        // A failed insertion must leave the slot available for a retry.
        a.push_back(Tracked(9));
        assert(a.size() == 2 && Tracked::live == 2);
        assert(a[1].value == 9);
    }
    assert(Tracked::live == 0);

    FixedArray<Category, 2> categories;
    int value = 42;
    categories.emplace_back(value);
    categories.emplace_back(std::move(value));
    assert(categories[0].from_lvalue);
    assert(!categories[1].from_lvalue);
}
