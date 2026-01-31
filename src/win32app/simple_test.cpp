// Simple IDE instantiation test
#include <windows.h>
#include <iostream>
#include <fstream>

int main() {
    std::ofstream log("simple_test_log.txt");
    log << "Test started\n";
    log.flush();


    log << "About to load library\n";
    log.flush();
    
    // Just test if we can reach main
    
    log << "Success\n";
    log.close();
    
    return 0;
}
