#include <iostream>
#include <compare>
#include <thread>
#include <windows.h>
#include <syncstream>

int main() {
#ifdef _GLIBCXX_HAS_GTHREADS
    std::cout << "_GLIBCXX_HAS_GTHREADS is defined!" << std::endl;
#else
    std::cout << "_GLIBCXX_HAS_GTHREADS is NOT defined!" << std::endl;
#endif
    std::jthread t([]{
        // Use syncstream to prevent interleaving, though in a single thread example it matters less
        // But verifies another C++20 feature
        std::osyncstream(std::cout) << "Hello jthread\n";
    });
    return 0;
}
