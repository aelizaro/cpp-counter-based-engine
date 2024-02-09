#include <iostream>
#include "philox_engine.hpp"

int main() {
    philox4x32 engine;
    philox4x64 engine64;

    for (int i = 0; i < 9999; i++) {
        std::cout << engine() << std::endl;
        std::cout << engine64() << std::endl;
    }

    std::cout << "10000th element of philox4x32 is " << engine() << std::endl;

    std::cout << "with a default seed " << philox4x32::default_seed << std::endl;


    std::cout << "10000th element of philox4x64 is " << engine64() << std::endl;

    std::cout << "with a default seed " << philox4x64::default_seed << std::endl;
    return 0;
}