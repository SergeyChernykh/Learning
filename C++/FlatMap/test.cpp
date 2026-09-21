#include "FlatMap.h"

int main() {
    FlatMap<int, int> fmap;
    fmap.find(10);
    const_cast<const FlatMap<int, int>*>(&fmap)->find(10);
}