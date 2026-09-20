// Build with C++20. Tests specify iterator behavior without implementing it.
#include "FixedArray.h"
#include <algorithm>
#include <iostream>
#include <type_traits>
#include <utility>

int failures = 0;
void check(bool ok, const char* requirement) {
    if (!ok) {
        std::cerr << "FAIL: " << requirement << '\n';
        ++failures;
    }
}

template<class A>
void check_const_iteration(const A& a) {
    if constexpr (requires { a.begin(); a.end(); }) {
        check((std::is_same_v<decltype(*a.begin()), const int&>),
              "const begin dereferences to const int&");
        int sum = 0;
        for (const auto& value : a) sum += value;
        check(sum == 6, "range-for works on a const container");
    } else {
        check(false, "begin/end are callable on a const container");
    }
    if constexpr (requires { a.crbegin(); a.crend(); }) {
        check((std::is_same_v<decltype(*a.crbegin()), const int&>),
              "crbegin dereferences to const int&");
        check(a.crbegin().base() == a.cend(), "crbegin base equals cend");
        check(a.crend().base() == a.cbegin(), "crend base equals cbegin");
    } else {
        check(false, "standard names crbegin/crend exist on a const container");
    }
    if constexpr (requires { a.rbegin(); a.rend(); }) {
        check((std::is_same_v<decltype(*a.rbegin()), const int&>),
              "const rbegin dereferences to const int&");
        check(a.rbegin() == a.crbegin() && a.rend() == a.crend(),
              "const reverse boundaries agree");
    } else {
        check(false, "rbegin/rend are callable on a const container");
    }
}

template<class A>
void check_reverse(A& a) {
    check(a.rbegin() != a.rend(), "one element gives a nonempty reverse range");
    // Avoid stepping an ordinary pointer out of the allocation in broken code.
    if constexpr (std::is_pointer_v<decltype(a.rbegin())>) {
        check(false, "reverse iterator ++ must move toward the preceding element");
    } else {
        a.emplace_back(20);
        a.emplace_back(30);
        auto it = a.rbegin();
        for (int expected : {30, 20, 10}) {
            if (it == a.rend()) {
                check(false, "reverse iteration visits every live element");
                return;
            }
            check(*it == expected, "reverse iteration order");
            ++it;
        }
        check(it == a.rend(), "reverse iteration ends after size elements");
    }
}

int main() {
    FixedArray<int, 5> a;
    check(a.begin() == a.end(), "empty forward range");
    check(a.rbegin() == a.rend(), "empty reverse range");
    a.emplace_back(3);
    a.emplace_back(1);
    a.emplace_back(2);
    check(a.end() - a.begin() == 3, "range length uses size, not capacity");
    std::sort(a.begin(), a.end());
    check(a[0] == 1 && a[1] == 2 && a[2] == 3, "std::sort on a partial array");
    *a.begin() = 4;
    check(a[0] == 4, "mutable iterator changes the element");
    *a.begin() = 1;
    check_const_iteration(std::as_const(a));
    check((std::is_same_v<decltype(*a.cbegin()), const int&>), "cbegin is read-only");
    a.erase_back();
    check(a.end() - a.begin() == 2, "end follows erase_back");
    FixedArray<int, 5> singleton;
    singleton.emplace_back(10);
    check_reverse(singleton);
    std::cout << failures << " failed checks\n";
    return failures ? 1 : 0;
}
