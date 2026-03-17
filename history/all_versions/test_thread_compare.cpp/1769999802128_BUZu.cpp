#include <windows.h>
#include <compare>
#include <thread>
#include <iostream>

int main() {
    std::jthread t([]{
        std::cout << "Hello jthread\n";
    });
    return 0;
}
