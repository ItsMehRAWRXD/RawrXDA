#include <iostream>
#include <thread>
#include <vector>

void worker() {
    std::cout << "Thread working\n";
}

int main() {
    std::vector<std::thread> threads;
    threads.emplace_back(std::thread(worker));
    for(auto& t : threads) t.join();
    return 0;
}
