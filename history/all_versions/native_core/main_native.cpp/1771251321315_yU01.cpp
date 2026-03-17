// native_core/main_native.cpp
#include "gguf_native_loader.hpp"
#include "win32_file_iterator.hpp"
#include "http_native_server.hpp"
#include <iostream>
#include <string>

extern "C" {
    void Q4_0_DequantBlock_AVX512(void* input, float* output);
    void Q4_0_DequantBlock_AVX2(void* input, float* output);
}

int main() {
    std::cout << "RawrXD Native Core Test" << std::endl;

    // Test file iterator
    RawrXD::Native::FileIterator iter(L".");
    std::cout << "Files in current directory:" << std::endl;
    iter.ForEach([](const WIN32_FIND_DATAW& data) {
        std::wcout << L"  " << data.cFileName << std::endl;
        return true;
    });

    // Test HTTP server
    RawrXD::Native::HttpNativeServer server(8080);
    if (server.Start([](const std::string& request) {
        return "Hello from RawrXD Native Core!\nRequest: " + request.substr(0, 100) + "...";
    })) {
        std::cout << "HTTP server started on port 8080" << std::endl;
        std::cout << "Press Enter to stop..." << std::endl;
        std::cin.get();
        server.Stop();
    } else {
        std::cout << "Failed to start HTTP server" << std::endl;
    }

    return 0;
}