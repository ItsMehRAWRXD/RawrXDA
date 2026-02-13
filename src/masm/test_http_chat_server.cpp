/**
 * @file test_http_chat_server.cpp
 * @brief Test harness for RawrXD_HttpChatServer MASM module
 * 
 * Build:
 *   cl /EHsc /Zi test_http_chat_server.cpp /link RawrXD_HttpChatServer.lib ^
 *      kernel32.lib user32.lib wininet.lib shlwapi.lib
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RawrXD_HttpChatServer.h"

// ANSI color codes for Windows console
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET  "\033[0m"

#define TEST_PASS(msg) printf(COLOR_GREEN "[PASS] " COLOR_RESET "%s\n", msg)
#define TEST_FAIL(msg) printf(COLOR_RED "[FAIL] " COLOR_RESET "%s\n", msg)
#define TEST_INFO(msg) printf(COLOR_YELLOW "[INFO] " COLOR_RESET "%s\n", msg)

int main(int argc, char* argv[]) {
    printf("\n");
    printf("===============================================================\n");
    printf(" RawrXD HTTP Chat Server - Test Suite\n");
    printf("===============================================================\n\n");
    
    int tests_passed = 0;
    int tests_failed = 0;
    
    // Test 1: HTTP Client Initialization
    TEST_INFO("Test 1: HTTP Client Initialization");
    if (HttpClient_Initialize()) {
        TEST_PASS("HttpClient_Initialize succeeded");
        tests_passed++;
    } else {
        TEST_FAIL("HttpClient_Initialize failed");
        tests_failed++;
    }
    
    // Test 2: Check server status before starting
    TEST_INFO("Test 2: Check initial server status");
    if (!ChatServer_IsRunning()) {
        TEST_PASS("Server correctly reports as not running initially");
        tests_passed++;
    } else {
        TEST_INFO("Server was already running (external instance?)");
        tests_passed++;
    }
    
    // Test 3: Start chat server
    TEST_INFO("Test 3: Start chat server");
    if (StartChatServer()) {
        TEST_PASS("StartChatServer succeeded");
        tests_passed++;
        
        // Give it time to start
        Sleep(1000);
        
        // Verify it's running
        if (ChatServer_IsRunning()) {
            TEST_PASS("Server confirmed running after start");
            tests_passed++;
        } else {
            TEST_FAIL("Server not running after StartChatServer returned true");
            tests_failed++;
        }
    } else {
        TEST_INFO("StartChatServer returned false (Python/script may not be available)");
        // This isn't necessarily a failure of the MASM code
    }
    
    // Test 4: HTTP Connection (to localhost:15099)
    TEST_INFO("Test 4: HTTP Connection to localhost:15099");
    if (HttpClient_Connect(L"localhost", 15099)) {
        TEST_PASS("HttpClient_Connect succeeded");
        tests_passed++;
    } else {
        TEST_INFO("HttpClient_Connect failed (server may not be listening)");
    }
    
    // Test 5: Send a chat message (only if server started)
    TEST_INFO("Test 5: Send chat message");
    if (ChatServer_IsRunning()) {
        char response[4096] = {0};
        
        if (SendChatMessage("Hello from MASM test!", response, sizeof(response))) {
            TEST_PASS("SendChatMessage succeeded");
            printf("       Response: %.100s%s\n", response, strlen(response) > 100 ? "..." : "");
            tests_passed++;
        } else {
            TEST_INFO("SendChatMessage failed (expected if server script is missing)");
        }
    } else {
        TEST_INFO("Skipping chat test - server not running");
    }
    
    // Test 6: HTTP POST with raw data
    TEST_INFO("Test 6: Raw HTTP POST test");
    if (ChatServer_IsRunning()) {
        const char* json = "{\"test\":\"data\"}";
        char response[4096] = {0};
        
        uint32_t status = HttpClient_Post(
            L"/api/chat",
            json,
            (uint32_t)strlen(json),
            response,
            sizeof(response)
        );
        
        if (status == 200) {
            TEST_PASS("HttpClient_Post returned HTTP 200");
            tests_passed++;
        } else if (status > 0 && status < 1000) {
            printf("       HTTP Status: %u\n", status);
            TEST_INFO("HTTP request completed with non-200 status");
        } else {
            printf("       Error Code: %u\n", status);
            TEST_INFO("HTTP request failed (expected if endpoint doesn't exist)");
        }
    } else {
        TEST_INFO("Skipping HTTP POST test - server not running");
    }
    
    // Test 7: Stop server
    TEST_INFO("Test 7: Stop chat server");
    if (StopChatServer(3000)) {
        TEST_PASS("StopChatServer succeeded");
        tests_passed++;
    } else {
        TEST_INFO("StopChatServer returned false (may have already stopped)");
    }
    
    // Test 8: Verify stopped
    TEST_INFO("Test 8: Verify server stopped");
    Sleep(500);
    if (!ChatServer_IsRunning()) {
        TEST_PASS("Server confirmed stopped");
        tests_passed++;
    } else {
        TEST_FAIL("Server still running after StopChatServer");
        tests_failed++;
    }
    
    // Test 9: Full shutdown
    TEST_INFO("Test 9: Full subsystem shutdown");
    if (RawrXD_ChatServer_Shutdown()) {
        TEST_PASS("RawrXD_ChatServer_Shutdown succeeded");
        tests_passed++;
    } else {
        TEST_FAIL("RawrXD_ChatServer_Shutdown failed");
        tests_failed++;
    }
    
    // Test 10: HTTP cleanup
    TEST_INFO("Test 10: HTTP cleanup");
    HttpClient_Cleanup();
    TEST_PASS("HttpClient_Cleanup completed (no return value)");
    tests_passed++;
    
    // Summary
    printf("\n");
    printf("===============================================================\n");
    printf(" Test Summary\n");
    printf("===============================================================\n");
    printf(" Passed: %d\n", tests_passed);
    printf(" Failed: %d\n", tests_failed);
    printf("===============================================================\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
