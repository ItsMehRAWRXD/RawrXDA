#include <iostream>
#include <thread>
#include <stop_token>

void worker(std::stop_token st) {
    while(!st.stop_requested()) {
        std::cout << "Working...\n";
        break; 
    }
}

int main() {
    std::jthread t(worker);
    t.request_stop();
    return 0;
}
