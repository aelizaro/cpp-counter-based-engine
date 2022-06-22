#pragma once
#include <array>
#include <ranges>
#include "detail.hpp"

template <typename UIntType, std::size_t w, std::size_t n, std::size_t r, std::size_t c,
          UIntType... consts>
class philox_engine {
    static_assert((c > 0) && (c <= n));
    static_assert(n > 0);
    static_assert(sizeof...(consts) == n);

    // Exposition only
    static constexpr std::size_t s_box_count = n / 2; // keys_count

public:
    // types
    using result_type = UIntType;

    // engine characteristics
    static constexpr std::size_t word_size = w;
    static constexpr std::size_t word_count = n;
    static constexpr std::size_t round_count = r;
    static constexpr size_t counter_count = c;

    static constexpr std::array<result_type, s_box_count> multipliers =
        detail::get_even_array_from_tuple<UIntType>(std::make_tuple(consts...),
                                                    std::make_index_sequence<s_box_count>{});
    static constexpr std::array<result_type, s_box_count> round_consts =
        detail::get_odd_array_from_tuple<UIntType>(std::make_tuple(consts...),
                                                   std::make_index_sequence<s_box_count>{});
    static constexpr result_type default_seed = 20111115u;

private:
    static constexpr size_t state_size = 3 * n / 2; // counters + keys

    using state = std::array<result_type, state_size>;
    state state_; // state: [counter_0,..., counter_n, key_0, ..., key_n/2-1];
    using results = std::array<result_type, word_count>;
    results results_;
    static constexpr auto in_mask = detail::fffmask<result_type, word_size>;
    static constexpr auto result_mask =
        detail::fffmask<result_type,
                        std::min<size_t>(std::numeric_limits<result_type>::digits, word_size)>;

public:
    // constructors and seeding functions
    philox_engine() : philox_engine(default_seed) {}

    explicit philox_engine(result_type s) {
        seed(s);
    }

    template <class Sseq>
    explicit philox_engine(Sseq& s) {
        seed(s);
    }

    void seed(result_type seed = default_seed) {
        seed_internal({ seed & in_mask });
    }

    template <class Sseq>
    void seed(Sseq& seed) {
        // TODO
    }

    // generating functions
    result_type operator()() {
        result_type ret;
        (*this)(&ret, &ret + 1);
        return ret;
    }

    void discard(unsigned long long jump) {
        auto oldridx = ridxref();
        unsigned newridx = (jump + oldridx) % word_count;
        unsigned long long jumpll = jump + oldridx - (!oldridx && newridx);
        jumpll /= word_count;
        jumpll += !oldridx;
        result_type jumpctr = jumpll & in_mask;
        result_type oldctr = get_counter_internal();
        result_type newctr = (jumpctr - 1 + oldctr) & in_mask;
        set_counter_internal(state_, newctr);
        if (newridx) {
            if (jumpctr)
                (*this)(std::begin(state_), std::begin(results_));
            increase_counter_internal();
        }
        else if (newctr == 0) {
            newridx = word_count;
        }

        ridxref() = newridx;
    }

    void set_counters(std::initializer_list<result_type> counters) {
        auto start = counters.begin();
        auto end = counters.end();
        for (size_t i = 0; i < word_count; i++) {
            state_[i] = (start == end) ? 0 : (*start++) & in_mask; // all counters are set
        }
    }

    static constexpr result_type min() {
        return 0;
    }
    static constexpr result_type max() {
        return result_mask;
    }

private:
    // methods to manipulate counters
    using counter_type =
        detail::uint_fast<counter_count * word_size>; // WARNING: doesn't scale for word_count > 4

    counter_type get_counter_internal() const { // need to check
        std::uint64_t ret = 0;
        for (size_t i = 0; i < counter_count; ++i) {
            ret |= std::uint64_t(state_[i]) << (word_size * i);
        }
        return ret;
    }
    void set_counter_internal(state& s, counter_type newctr) { // need to check
        static_assert(word_size * counter_count <= std::numeric_limits<counter_type>::digits);
        for (size_t i = 0; i < counter_count; ++i)
            s[i] = (newctr >> (word_size * i)) & in_mask;
    }
    void increase_counter_internal() { // need to check
        state_[0] = (state_[0] + 1) & in_mask;
        for (size_t i = 1; i < counter_count; ++i) {
            if (state_[i - 1]) {
                [[likely]] return;
            }
            state_[i] = (state_[i] + 1) & in_mask;
        }
    }

    void seed_internal(std::initializer_list<result_type> seed) {
        auto start = seed.begin();
        auto end = seed.end();
        size_t i = 0;
        for (i = 0; i < word_count; i++) {
            state_[i] = 0; // all counters are set to zero
            // WARNING: do we need to do it if counters were set before?
        }
        for (; i < state_size; i++) { // keys are set as seed
            state_[i] = (start == end) ? 0 : (*start++) & in_mask;
        }
        ridxref() = 0;
    }

    const auto& ridxref() const {
        return results_[0];
    }
    auto& ridxref() {
        return results_[0];
    }

    template <typename InputIterator, typename OutputIterator>
    OutputIterator operator()(InputIterator input, OutputIterator output) {
        return generate(std::ranges::single_view(input), output);
    }

    template <std::ranges::input_range InRange, std::weakly_incrementable Output>
    requires std::ranges::sized_range<InRange> &&
        std::integral<std::iter_value_t<std::ranges::range_value_t<InRange>>> &&
        std::integral<std::iter_value_t<Output>> &&
        std::indirectly_writable<Output, std::iter_value_t<Output>>
            Output generate(InRange&& inrange, Output output) {
        for (auto initer : inrange) {
            if constexpr (n == 2) {
                result_type R0 = (*initer++) & in_mask;
                result_type L0 = (*initer++) & in_mask;
                result_type K0 = (*initer++) & in_mask;
                for (size_t i = 0; i < round_count; ++i) {
                    auto [hi, lo] = detail::mulhilo<word_size>(R0, multipliers[0]);
                    R0 = hi ^ K0 ^ L0;
                    L0 = lo;
                    K0 = (K0 + round_consts[0]) & in_mask;
                }
                *output++ = R0;
                *output++ = L0;
            }
            else if constexpr (n == 4) {
                result_type R0 = (*initer++) & in_mask;
                result_type L0 = (*initer++) & in_mask;
                result_type R1 = (*initer++) & in_mask;
                result_type L1 = (*initer++) & in_mask;
                result_type K0 = (*initer++) & in_mask;
                result_type K1 = (*initer++) & in_mask;
                for (size_t i = 0; i < r; ++i) {
                    auto [hi0, lo0] = detail::mulhilo<word_size>(R0, multipliers[0]);
                    auto [hi1, lo1] = detail::mulhilo<word_size>(R1, multipliers[1]);
                    R0 = hi1 ^ L0 ^ K0;
                    L0 = lo1;
                    R1 = hi0 ^ L1 ^ K1;
                    L1 = lo0;
                    K0 = (K0 + round_consts[0]) & in_mask;
                    K1 = (K1 + round_consts[1]) & in_mask;
                }
                *output++ = R0;
                *output++ = L0;
                *output++ = R1;
                *output++ = L1;
            }
            // No more cases.  See the static_assert(n==2 || n==4) at the top of the class
        }
        return output;
    }

    template <std::output_iterator<const result_type&> Output,
              std::sized_sentinel_for<Output> Sentinel>
    Output operator()(Output out, Sentinel sen) {
        auto len = sen - out;

        // Deliver any saved results
        auto ri = ridxref();
        if (ri && len) {
            while (ri < word_count && len) {
                *out++ = results_[ri++];
                --len;
            }
            if (ri == word_count)
                ri = 0;
        }

        // Call the bulk generator
        auto nprf = len / word_count;
        // lazily construct the input range.  No need
        // to allocate and fill a big chunk of memory
        auto c0 = get_counter_internal();
        state state_tmp;
        out = generate(
            std::ranges::views::iota(c0, c0 + nprf) | std::ranges::views::transform([&](auto ctr) {
                state_tmp = state_;
                set_counter_internal(state_tmp, ctr);
                return std::ranges::begin(state_tmp);
            }),
            out);
        len -= nprf * word_count;
        set_counter_internal(state_, c0 + nprf);

        // Restock the results array
        if (ri == 0 && len) {
            (*this)(std::begin(state_), std::begin(results_));
            increase_counter_internal();
        }

        // Finish off any stragglers.
        while (len--)
            *out++ = results_[ri++];
        ridxref() = ri;
        return out;
    }
};

template <size_t r, size_t c>
using philox4x32_rc =
    philox_engine<std::uint_fast32_t, 32, 4, r, c, 0xD2511F53, 0x9E3779B9, 0xCD9E8D57, 0xBB67AE85>;

template <size_t r, size_t c>
using philox4x64_rc = philox_engine<std::uint_fast64_t, 64, 4, r, c, 0xD2E7470EE14C6C93,
                                    0x9E3779B97F4A7C15, 0xCA5A826395121157, 0xBB67AE8584CAA73B>;

using philox4x32 =
    philox4x32_rc<10, 1>; // oops int128 is not working, need algotithm redo with reinterpret casts
using philox4x64 = philox4x64_rc<10, 1>;