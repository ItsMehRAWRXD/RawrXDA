/**
 * @file test_http_chat_integration.cpp
 * @brief Simple test to verify HTTP Chat Server MASM module is linked correctly
 */

#include <windows.h>
#include "RawrXD_HttpChatServer.h"
#include <iostream>
#include <cstring>

int main() {
    std::cout << "==============================================\n";
    std::cout << " HTTP Chat Server Integration Test\n";
    std::cout << "==============================================\n\n";

    // Test 1: Initialize HTTP client
    std::cout << "[TEST 1] Initializing HTTP client...\n";
    if (HttpClient_Initialize()) {
        std::cout << "  ✓ HTTP client initialized successfully\n";
    } else {
        std::cout << "  ✗ HTTP client initialization failed\n";
        return 1;
    }

    // Test 2: Check server status
    std::cout << "\n[TEST 2] Checking chat server status...\n";
    if (ChatServer_IsRunning()) {
        std::cout << "  ✓ Chat server is running\n";
    } else {
        std::cout << "  ℹ Chat server is not running (expected)\n";
    }

    // Test 3: Attempt to start server
    std::cout << "\n[TEST 3] Attempting to start chat server...\n";
    if (StartChatServer()) {
        std::cout << "  ✓ Server start command succeeded\n";
        
        // Wait a moment for server to initialize
        Sleep(500);
        
        if (ChatServer_IsRunning()) {
            std::cout << "  ✓ Server confirmed running\n";
        } else {
            std::cout << "  ℹ Server start requested but not yet running\n";
        }
    } else {
        std::cout << "  ℹ Server could not be started (Python may not be installed)\n";
    }

    // Test 4: Cleanup
    std::cout << "\n[TEST 4] Cleaning up resources...\n";
    HttpClient_Cleanup();
    std::cout << "  ✓ HTTP client cleaned up\n";

    RawrXD_ChatServer_Shutdown();
    std::cout << "  ✓ Chat server shutdown complete\n";

    std::cout << "\n==============================================\n";
    std::cout << " Integration Test Complete\n";
    std::cout << "==============================================\n";

    return 0;
}
