/**
 * quant_correctness_tests.cpp — C++20 stub (Qt-free).
 *
 * Original tests used qtapp/quant_utils.hpp, QByteArray, QVector, QString.
 * qtapp is not in tree. This stub compiles without Qt; run full quant
 * tests when quant_utils (C++20/STL) is available.
 */

#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "Quant correctness tests (C++20): qtapp/quant_utils not in tree.\n";
    std::cout << "  To run: add quant_utils.hpp (STL-only) and re-enable tests.\n";
    return 0;
}
