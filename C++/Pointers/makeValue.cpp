#include <iostream>
#include <memory>

std::unique_ptr<int> makeValue(int x) {
    return std::make_unique<int>(x);
}

int main() {
    std::cout << *makeValue(5) << std::endl;
}