// Simple IDE instantiation test
#include <windows.h>
#include <iostream>
#include <fstream>

int main() {
    std::ofstream log("simple_test_log.txt");
    log << "Test started\n";
    log.flush();
    
    std::cout << "Simple IDE Test\n";
    std::cout << "===============\n";
    
    log << "About to load library\n";
    log.flush();
    
    // Just test if we can reach main
    std::cout << "Success: Program executed\n";
    log << "Success\n";
    log.close();
    
    return 0;
}
