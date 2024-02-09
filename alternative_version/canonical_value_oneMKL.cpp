#include <iostream>
#include <vector>

#include "mkl.h"

int main() {
    MKL_INT n = 10000;
    std::vector<unsigned int> r(n);

    int err = 0;
    VSLStreamStatePtr stream;

    err += vslNewStream(&stream, VSL_BRNG_PHILOX4X32X10, 20111115u);
    
    err+= viRngUniformBits(0, stream, n, r.data());

    for (int i = 0; i < 9999; i++) {
        std::cout << r[i] << std::endl;
    }

    std::cout << "oneMKL 10000th element is " << r[9999] << std::endl;

    err += vslDeleteStream(&stream);
    return err;
}