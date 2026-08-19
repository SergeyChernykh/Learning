#include <iostream>
#include <memory>

void printIfAlive(std::weak_ptr<int> w) {
    if (auto ptr = w.lock()) {
        std::cout << "Alive: " << *ptr << std::endl;
        std::cout << "Use count: " << ptr.use_count() << std::endl;
    } else {
        std::cout << "Dead" << std::endl;
    }
}

int main() {
    std::weak_ptr<int> w;
    {
        auto ptr = std::make_shared<int>(5);
        std::cout << "Use count: " << ptr.use_count() << std::endl;
        w = ptr;
        std::cout << "Use count: " << ptr.use_count() << std::endl;
        printIfAlive(w);
        std::cout << "Use count: " << ptr.use_count() << std::endl;
    }
    printIfAlive(w);
}