#include <iostream>
#include <memory>
#include "FixedArray.h"

#define TEST(name) \
    if (!name()) { \
        std::cerr << #name << " failed!" << std::endl; \
    }

bool OverflowCheck() {
    FixedArray<int, 2> a;
    a.push_back(1);
    a.push_back(2);
    try {
        a.push_back(3);
    } catch(...) {
        return true;
    }
    return false;
}

bool test1() {
    FixedArray<int, 2> a;
    a.push_back(1);
    a.push_back(2);
    if (a[0] != 1) {
        return false;
    }
    if (a[1] != 2) {
        return false;
    }
    a.erase_back();
    if (a[0] != 1) {
        return false;
    }
    a.push_back(3);
    if (a[0] != 1) {
        return false;
    }
    if (a[1] != 3) {
        return false;
    }
    return true;
}

bool uniqueptr_test() {
    FixedArray<std::unique_ptr<int>, 2> a;
    a.push_back(std::make_unique<int>(1));
    if (*a[0] != 1) {
        return false;
    }
    auto p = std::make_unique<int>(2);
    a.push_back(std::move(p));
    if (*a[1] != 2) {
        return false;
    }
    return true;
}
int main(int argc, char* argv[]) {
    TEST(OverflowCheck)
    TEST(test1)
    return 0;
}