#include "threefry_prf.hpp"
#include "philox_prf.hpp"
#include "siphash_prf.hpp"
#include "counter_based_engine.hpp"
#include "timeit.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ranges>
#include <map>
#include <functional>
#include <algorithm>

using namespace std;
volatile int check = 0;

template <typename T, size_t N>
std::ostream& operator<<(std::ostream& os, const array<T, N>& a){
    for(const auto& v : a)
        os << v << " ";
    return os;
}

template<typename PRF>
void doit(string name){
    using prf_in_type = typename PRF::in_type;
    prf_in_type c = {99};
    static constexpr size_t prf_result_N = PRF::result_N;
    using prf_value_type = detail::uint_least<PRF::result_bits>;
    using prf_result_type = array<prf_value_type, prf_result_N> ;
    using engine_result_type = prf_value_type;
    static constexpr size_t engine_result_bits = PRF::result_bits;
    timeit_result perf;
    counter_based_engine<engine_result_type, PRF, 64/PRF::in_bits> engine;
    engine_result_type r = 0;
    static const size_t bulkN = 1024;
    static const size_t bits_per_byte = 8;
    float Gbytes_per_iter = 0.;

    // single (result_N) generation with prf
    perf = timeit(std::chrono::seconds(5),
                       [&r, &c](){
                           prf_result_type rv;
                           PRF{}(ranges::single_view(c), begin(rv));
                           c.back()++;
                           r = accumulate(begin(rv), end(rv), r, bit_xor<decltype(r)>{});
                       });
    cout << "calling " << name << " directly (" << prf_result_N << " at a time): " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << " Miters/sec";
    Gbytes_per_iter = 1.e-9 * (PRF::result_bits*prf_result_N)/bits_per_byte;
    cout << " approx " << perf.iter_per_sec() *  Gbytes_per_iter <<  " GB/s\n";

    // bulk generation directly with prf
    array<typename PRF::in_type, bulkN/prf_result_N> bulkin;
    for(auto& a : bulkin)
        a = prf_in_type{};
    perf = timeit(std::chrono::seconds(5),
                       [&r, &bulkin](){
                           array<engine_result_type, bulkN> bulk;
                           PRF{}(bulkin, begin(bulk));
                           for(auto& a : bulkin)
                               a.back()++;
                           r = accumulate(begin(bulk), end(bulk), r, bit_xor<decltype(r)>{});
                       });
    cout << "calling " << name << " directly (" << bulkN << " at a time): " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << " Miters/sec";
    Gbytes_per_iter = 1.e-9*(bulkN*PRF::result_bits)/bits_per_byte;
    cout << " approx " << perf.iter_per_sec() *  Gbytes_per_iter <<  " GB/s\n";

    perf = timeit(chrono::seconds(5),
                           [&](){
                               r ^= engine();
                           });
    cout << "calling " << name << " through engine (1 at a time): " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << " Miters/sec";
    Gbytes_per_iter = 1.e-9 * engine_result_bits/bits_per_byte;
    cout << " approx " << perf.iter_per_sec() * Gbytes_per_iter <<  " GB/s\n";

    perf = timeit(chrono::seconds(5),
                           [&](){
                               array<engine_result_type, bulkN> bulk;
                               engine(begin(bulk), end(bulk));
                               r = accumulate(begin(bulk), end(bulk), r, bit_xor<engine_result_type>{});
                           });
    cout << "calling " << name << " through engine (" << bulkN << " at a  time): " << (r==0?" (zero?!) ":"");
    cout << perf.iter_per_sec()/1e6 << " Miters/sec";
    Gbytes_per_iter = 1.e-9 * bulkN*engine_result_bits/bits_per_byte;
    cout << " approx " << perf.iter_per_sec() * Gbytes_per_iter <<  " GB/s\n";
}

// a minimal prf that copies inputs to outputs - useful for estimating
// function call and related overheads
class null_prf{
public:
    static constexpr size_t in_bits=64;
    static constexpr size_t in_N = 4;
    static constexpr size_t result_bits=64;
    static constexpr size_t result_N = 4;
    using result_value_type = uint64_t;
    using in_value_type = uint64_t;
    using result_type = array<result_value_type, result_N>;
    template <detail::integral_input_range InRange, weakly_incrementable O>
    void operator()(InRange&& in, O result) const{
        ranges::copy(in, result);
    }
};        

#define MAPPED(prf) {string(#prf), function<void(string)>(&doit<prf>)}
#define _ ,
map<string, function<void(string)>> dispatch_map = {
                                                    //    MAPPED(uint64_t, null_prf),
    MAPPED(threefry4x64_prf<>),
    MAPPED(threefry2x64_prf<>),
    MAPPED(threefry4x32_prf<>),
    MAPPED(threefry2x32_prf<>),
    MAPPED(philox4x64_prf<>),
    MAPPED(philox2x64_prf<>),
    MAPPED(philox4x32_prf<>),
    MAPPED(philox2x32_prf<>),
    MAPPED(siphash_prf<4>),
    MAPPED(siphash_prf<16>),
};
    

int main(int argc, char**argv){
    cout << setprecision(2);
    if(argc == 1){
        for(auto& p : dispatch_map)
            p.second(p.first);
        return 0;
    }
    for(auto p = argv+1; *p; p++){
        auto found = dispatch_map.find(string(*p));
        if(found != dispatch_map.end())
            found->second(found->first);
        else
            cout << *p << " not found in dispatch map\n";
    }        
    return 0;
}
