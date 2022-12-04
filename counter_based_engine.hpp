#pragma once
#include "detail.hpp"

#include <limits>
#include <array>
#include <random>
#include <iosfwd>
#include <algorithm>
#include <cstring>
#include <vector>
#include "threefry_prf.hpp"
#include "philox_prf.hpp"

namespace std {

// What happens if we want operator() of Prf to be templated
// template<class Prf, class InputRange, class OutputIterator>
// concept pseudo_random_function = std::input_range<InputRange> &&
// std::sized_range<InputRange> && std::output_iterator<OutputIterator> && requires(Prf prf, InputRange range, OutputIterator o) {
//     typename Prf::result_type;
//     Prf::word_size;
//     Prf::input_count;
//     Prf::output_count;
//     { Prf::min() } -> same_as<typename Prf::result_type>;
//     { Prf::max() } -> same_as<typename Prf::result_type>;
//     { prf(range, o) } -> std::output_iterator;
// };
//
// It pollutes counter_based_engine template parameters:
// template< std::sized_range InputRange, std::output_iterator OutputIterator, pseudo_random_function<InputRange, OutputIterator>, size_t c>
// class counter_based_engine;
//

template<class Prf>
concept pseudo_random_function = requires(Prf prf, span<typename Prf::input_value_type, Prf::input_count> in, span<typename Prf::output_value_type, Prf::output_count> out) {
    typename Prf::input_value_type;
    typename Prf::output_value_type;
    Prf::input_word_size;
    Prf::output_word_size;
    Prf::input_count;
    Prf::output_count;
    { Prf::min() } -> same_as<typename Prf::output_value_type>;
    { Prf::max() } -> same_as<typename Prf::output_value_type>;
    prf(in, out);
};

template<pseudo_random_function prf, size_t c>
class counter_based_engine {
    static_assert(numeric_limits<typename prf::output_value_type>::max() >= prf::output_count);
    static_assert(c > 0);
    static_assert(prf::input_word_size > 0);
    static_assert(prf::output_word_size > 0);
    static_assert(prf::output_count > 0);

public:
    using result_type = typename prf::output_value_type;
    using seed_value_type = typename prf::input_value_type;
    static constexpr size_t word_size = prf::output_word_size;
    static constexpr size_t counter_count = c;
    static constexpr size_t counter_word_size = prf::input_word_size;
    static constexpr size_t seed_count = prf::input_count - counter_count;
    static constexpr size_t seed_word_size = prf::input_word_size;
private:
    using prf_result_type = array<result_type, prf::output_count>;

    static constexpr size_t result_count = prf::output_count;
    static constexpr size_t input_count = prf::input_count;
    static constexpr size_t input_word_size = prf::input_word_size;

    using input_value_type = typename prf::input_value_type;
    using in_type = array<input_value_type, input_count>;

    static_assert(integral<input_value_type>);

    static constexpr auto in_mask = detail::fffmask<input_value_type, prf::input_word_size>;

    in_type in; // state
    prf_result_type results;

    // To save space, store the index of the next result to be returned in the 0th results slot.
    const auto& ridxref() const {
        return results[0];
    }
    auto& ridxref() {
        return results[0];
    }


    using counter_type = detail::uint_fast<c*prf::input_word_size>;

    counter_type get_counter() const{
        uint64_t ret = 0;
        for(size_t i=0; i<counter_count; ++i)
            ret |= uint64_t(in[i])<<(input_word_size * i);
        return ret;
    }

    static void set_counter(in_type& inn, counter_type newctr){
        static_assert(input_word_size * counter_count <= numeric_limits<counter_type>::digits);
        for(size_t i=0; i<counter_count; ++i)
            inn[i] = (newctr >> (input_word_size*i)) & in_mask;
    }

    void incr_counter(){
        in[0] = (in[0] + 1) & in_mask;
        for(size_t i=1; i<counter_count; ++i){
            if(in[i-1])
                [[likely]]return;
            in[i] = (in[i] + 1) & in_mask;
        }
    }

public:

    static constexpr result_type min(){ return prf::min(); }
    static constexpr result_type max(){ return prf::max(); };
    static constexpr result_type default_seed = 20111115u; // AE: Default seed should be moved to prf?


    result_type operator()(){
        result_type ret;
        (*this)(&ret, &ret+1);
        return ret;   
    }


private:

    template <output_iterator<const result_type&> O, sized_sentinel_for<O> S>
    O operator()(O out, S sen) {
        auto n = sen - out;
        
        // Deliver any saved results
        auto ri = ridxref();
        if(ri && n){
            while(ri < result_count && n){
                *out++ = results[ri++];
                --n;
            }
            if(ri == result_count)
                ri = 0;
        }
            
        // Call the bulk generator
        auto nprf = n/result_count;
        // lazily construct the input range.  No need
        // to allocate and fill a big chunk of memory
        using namespace std::ranges;
        auto c0 = get_counter();
        in_type inn;
        out = prf{}.generate(views::iota(c0, c0+nprf) |
                             views::transform([&](auto ctr){
                                                  inn = in;
                                                  set_counter(inn, ctr);
                                                  return ranges::begin(inn);
                                              }),
                             out); // TODO: rework it to span api

        n -= nprf*result_count;
        set_counter(in, c0 + nprf);

        // Restock the results array
        if(ri == 0 && n){
            prf{}(in, results);
            incr_counter();
        }
            
        // Finish off any stragglers.
        while(n--)
            *out++ = results[ri++];
        ridxref() = ri;
        return out;
    }
public:


    counter_based_engine() : counter_based_engine(default_seed){}

    explicit counter_based_engine(result_type s){ seed(s); }

    void seed(result_type value = default_seed){
        array<seed_value_type, seed_count> K = { input_value_type(value) & in_mask };
        seed_internal(K);
    }

    template <typename SeedSeq>
    explicit counter_based_engine(SeedSeq& q){ seed(q); }


    template <typename SeedSeq>
    void seed(SeedSeq& s){
        constexpr size_t N32_per_in = (input_word_size-1)/32 + 1;
        array<uint_fast32_t, N32_per_in * seed_count> k32;
        s.generate(k32.begin(), k32.end());

        auto k32p = begin(k32);
        array<seed_value_type, seed_count> iv;
        for(auto& v : iv){
            v = 0;
            for(size_t j=0; j<N32_per_in; ++j)
                v |= seed_value_type(*k32p++) << (32*j);
            v &= in_mask;
        }            

        seed_internal(iv);
    }

    // (in)equality operators
    bool operator==(const counter_based_engine& rhs) const { return in == rhs.in && ridxref() == rhs.ridxref(); }
    bool operator!=(const counter_based_engine& rhs) const { return !operator==(rhs); }

    // discard - N.B.  There are a lot of tricky corner cases here
    //  that have NOT been tested.  E.g., really large jumps and/or
    //  an input_value_type that's wider than w.
    void discard(unsigned long long jump) {
        auto oldridx = ridxref();
        unsigned newridx = (jump + oldridx) % result_count;
        unsigned long long jumpll = jump + oldridx - (!oldridx && newridx);
        jumpll /= result_count;
        jumpll += !oldridx;
        input_value_type jumpctr = jumpll & in_mask;
        input_value_type oldctr = get_counter();
        input_value_type newctr = (jumpctr-1 + oldctr) & in_mask;
        set_counter(in, newctr);
        if(newridx){
            if(jumpctr)
                prf{}(in, results);
            incr_counter();
        }else if(newctr == 0){
            newridx = result_count;
        }
                
        ridxref() = newridx;
    }

    // stream inserter and extractor
    template <typename CharT, typename Traits>
    friend basic_ostream<CharT, Traits>& operator<<(basic_ostream<CharT, Traits>& os, const counter_based_engine& p){
        // FIXME - save/restore os state
        ostream_iterator<input_value_type> osin(os, " ");
        ranges::copy(p.in, osin);
        return os << p.ridxref();
    }
    template<typename CharT, typename Traits>
    friend basic_istream<CharT, Traits>& operator>>(basic_istream<CharT, Traits>& is, counter_based_engine& p){
        // FIXME - save/restore is state
        istream_iterator<input_value_type> isiin(is);
        copy_n(isiin, input_count, begin(p.in));
        result_type ridx;
        is >> ridx;
        if(ridx)
            prf{}(p.in, p.results);
        p.ridxref() = ridx;
        return is;
    }

    // Extensions:

    // counter_based_engine has public methods and types that are not
    // required for a Uniform Random Number Engine:

    // - the type of the underlying prf and a reference to it.
    using prf_type = prf;

    // - how many values are consumed in the seed(InRange) member
    // and corresponding constructor?
    
    // Constructors and seed members from from a 'seed-range'

public:
    void set_counters(std::initializer_list<result_type> counters) {
        auto start = counters.begin();
        auto end = counters.end();
        for(size_t i = 0; i < result_count; i++) {
            in[i] = (start == end) ? 0 : (*start++) & in_mask; // all counters are set
        }
    }
 
private:
    template <detail::integral_input_range InRange>
    void seed_internal(InRange _in){
        // copy _in to in:
        auto inp = ranges::begin(_in);
        auto ine = ranges::end(_in);
        for(size_t i=counter_count; i<input_count; ++i)
            in[i] = (inp == ine) ? 0 : seed_value_type(*inp++) & in_mask; // ?? throw if *inp > in_mask??
        set_counter(in, 0);
        ridxref() = 0;
    }
};

using philox2x32 = counter_based_engine<philox2x32_prf, 2>;
using philox4x32 = counter_based_engine<philox4x32_prf, 2>;
using philox2x64 = counter_based_engine<philox2x64_prf, 1>;
using philox4x64 = counter_based_engine<philox4x64_prf, 1>;

// using threefry2x32 = counter_based_engine<threefry2x32_prf, 2>;
// using threefry4x32 = counter_based_engine<threefry4x32_prf, 2>;
// using threefry2x64 = counter_based_engine<threefry2x64_prf, 1>;
// using threefry4x64 = counter_based_engine<threefry4x64_prf, 1>;

} // namespace std
