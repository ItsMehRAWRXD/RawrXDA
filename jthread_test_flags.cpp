#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <thread>
#include <stop_token>

void worker() {}

int main() {
    std::jthread t(worker);
    return 0;
}
