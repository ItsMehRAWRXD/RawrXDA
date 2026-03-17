#include <compare>
#include <atomic>
#include <thread> // includes stop_token usually, or
#include <stop_token>
#include <iostream>

#include <windows.h>

int main() {
    std::stop_source src;
    std::stop_token token = src.get_token();
    if(token.stop_possible()) std::cout << "Stop possible\n";
    return 0;
}
