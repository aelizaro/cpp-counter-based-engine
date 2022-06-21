#include <iostream>
#include "philox_engine.hpp"

int main() {

    constexpr size_t n = 10;

    constexpr size_t discard = 4;

    philox4x32 engine(7777);

    std::array<typename philox4x32::result_type, n> out1;
    std::array<typename philox4x32::result_type, n> out2;
    std::array<typename philox4x32::result_type, n - discard> out3;
    std::array<typename philox4x32::result_type, n - discard> out4;

    std::cout << "Reference output:" << std::endl;
    for(int i = 0; i < n; i++) {
        out1[i] = engine();
        std::cout << out1[i] << " ";
    }

    std::cout << "\nSeed reset check:" << std::endl;
    engine.seed(7777); // NB: Counters are set to 0!

    for(int i = 0; i < n; i++) {
        out2[i] = engine();
        std::cout << out2[i] << " ";
        if(out1[i] != out2[i]) {
            std::cout << "\nresults mismatch: " << out1[i] << " " << out2[i] << std::endl;
        }
    }

    std::cout << "\nDiscard check: " << std::endl;
    engine.seed(7777);
    engine.discard(discard);

    for(int i = 0; i < n - discard; i++) {
        out3[i] = engine();
        std::cout << out3[i] << " ";
        if(out2[i + discard] != out3[i]) {
            std::cout << "\nresults mismatch: " << out2[i] << " " << out3[i] << std::endl;
        }
    }

    std::cout << "\nSet counters check: " << std::endl;
    engine.seed(7777);
    engine.set_counters({1, 0, 0, 0});

    for(int i = 0; i < n - discard; i++) {
        out4[i] = engine();
        std::cout << out4[i] << " ";
        if(out3[i] != out4[i]) {
            std::cout << "\nresults mismatch: " << out3[i] << " " << out4[i] << std::endl;
        }
    }

    return 0;
}