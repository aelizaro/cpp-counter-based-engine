// To check how hard it is to set the full state of generator with 'transparent' seed sequence

#include <iostream>
#include <vector>
#include <algorithm>
#include "philox_engine.hpp"

class custom_seed_seq {
public:
    using result_type = std::uint_least32_t;
    custom_seed_seq() noexcept {}
    custom_seed_seq(const custom_seed_seq&) = delete;

    template <class InputIt>
    custom_seed_seq(InputIt begin, InputIt end) : seed_seq_(begin, end) {}

    template <class T>
    custom_seed_seq(std::initializer_list<T> il) {
        for (auto a : il) {
            seed_seq_.push_back(a);
        }
    }

    custom_seed_seq& operator=(const custom_seed_seq&) = delete;

    template <class RandomIt>
    void generate(RandomIt begin, RandomIt end) {
        if (begin == end) {
            return;
        }

        const int length = end - begin;
        for (int i = 0; i < length; i++) {
            if (i < seed_seq_.size()) {
                begin[i] = seed_seq_[i];
            }
            else {
                begin[i] = 0;
            }
        }
    }

    std::size_t size() const noexcept {
        return seed_seq_.size();
    }

    template <class OutputIt>
    void param(OutputIt dest) const {
        // TODO ?
    }

private:
    std::vector<result_type> seed_seq_;
};

int main() {
    constexpr size_t n = 20;

    constexpr size_t discard = 8;

    philox4x32 engine(7777);

    std::array<typename philox4x32::result_type, n> out1;
    std::array<typename philox4x32::result_type, n - discard> out2;

    std::cout << "Reference output:" << std::endl;
    for (int i = 0; i < n; i++) {
        out1[i] = engine();
        std::cout << out1[i] << " ";
    }

    std::cout << "\nSeed reset:" << std::endl;
    int counter = discard / philox4x32::word_count;
    custom_seed_seq seq{ counter, 0, 0, 0, 7777 };
    engine.seed(seq); // NB: Counters are set to 0!

    for (int i = 0; i < n - discard; i++) {
        out2[i] = engine();
        std::cout << out2[i] << " ";
        if (out1[i + discard] != out2[i]) {
            std::cout << "\nresults mismatch: " << out1[i] << " " << out2[i] << std::endl;
        }
    }

    return 0;
}