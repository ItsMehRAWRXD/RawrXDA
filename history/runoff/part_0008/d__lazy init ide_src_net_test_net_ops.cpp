// test_net_ops.cpp
// Regression testing for MASM networking routines

#include "net_masm_bridge.h"
#include <iostream>
#include <cstring>
#include <cassert>

// Test: TcpConnect (stub)
bool TestTcpConnect() {
    std::cout << "Testing TcpConnect..." << std::endl;
    void* handle = TcpConnect("localhost", 8080);
    // Since stubs return null/0, just verify no crash
    std::cout << "TcpConnect: PASS (stub)" << std::endl;
    return true;
}

// Test: HttpGet (stub)
bool TestHttpGet() {
    std::cout << "Testing HttpGet..." << std::endl;
    char buffer[1024];
    long long bytes = HttpGet("http://localhost", buffer, sizeof(buffer));
    // Since stubs return 0, just verify no crash
    std::cout << "HttpGet: PASS (stub)" << std::endl;
    return true;
}

// Test: HttpPost (stub)
bool TestHttpPost() {
    std::cout << "Testing HttpPost..." << std::endl;
    char buffer[1024];
    const char* data = "{\"test\": \"data\"}";
    long long bytes = HttpPost("http://localhost", data, strlen(data), buffer);
    // Since stubs return 0, just verify no crash
    std::cout << "HttpPost: PASS (stub)" << std::endl;
    return true;
}

// Test: WebSocket operations (stub)
bool TestWebSocket() {
    std::cout << "Testing WebSocket..." << std::endl;
    void* handle = TcpConnect("localhost", 8080);
    char buffer[1024];
    long long sent = WebSocketSend(handle, "test", 4);
    long long recv = WebSocketRecv(handle, buffer, sizeof(buffer));
    // Since stubs return 0, just verify no crash
    std::cout << "WebSocket: PASS (stub)" << std::endl;
    return true;
}

int main() {
    std::cout << "=== MASM Networking Op Regression Tests ===" << std::endl;
    
    bool all_pass = true;
    all_pass &= TestTcpConnect();
    all_pass &= TestHttpGet();
    all_pass &= TestHttpPost();
    all_pass &= TestWebSocket();

    std::cout << std::endl;
    if (all_pass) {
        std::cout << "ALL TESTS PASSED (stubs)" << std::endl;
        return 0;
    } else {
        std::cout << "SOME TESTS FAILED" << std::endl;
        return 1;
    }
}
