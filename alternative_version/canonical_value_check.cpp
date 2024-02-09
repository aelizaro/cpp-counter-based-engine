// clang++ -std=gnu++2a -Wall  test.cpp

#include <iostream>
#include "philox_engine.hpp"

int main() {
    philox4x32 engine;

    for (int i = 0; i < 9999; i++) {
        std::cout << engine() << std::endl;
    }

    std::cout << "10000th element is " << engine() << std::endl;

    std::cout << "with a default seed " << philox4x32::default_seed << std::endl;
    return 0;
}