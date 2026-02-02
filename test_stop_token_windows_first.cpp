#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <compare>
#include <atomic>
#include <thread>
#include <stop_token>
#include <iostream>

int main() {
    std::stop_source src;
    std::stop_token token = src.get_token();
    if(token.stop_possible()) std::cout << "Stop possible\n";
    return 0;
}
