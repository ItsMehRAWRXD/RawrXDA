#include <iostream>
#include <chrono>
#include <thread>

int main() {


    // Run indefinitely
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
