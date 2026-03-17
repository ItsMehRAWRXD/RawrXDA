// ============================================================================
// test_agent.cpp - Smoke test for autonomous agent DLL
// ============================================================================
// Compile: cl /Fe:test_agent.exe test_agent.cpp
// Run:     .\test_agent.exe

#include <windows.h>
#include <stdio.h>

// Function prototypes
extern "C" {
    typedef long HRESULT;
    
    // Core autonomous functions
    __declspec(dllimport) HRESULT __stdcall IDEMaster_Initialize();
    __declspec(dllimport) DWORD __stdcall AgentPlan_Create(DWORD parentId, const char* pGoalJson);
    __declspec(dllimport) HRESULT __stdcall AgentPlan_Resolve(DWORD planId, char* pOutJson, DWORD cbOut);
    __declspec(dllimport) HRESULT __stdcall AgentLoop_SingleStep();
    __declspec(dllimport) HRESULT __stdcall AgentLoop_RunUntilDone(const char* pGoalJson);
    
    __declspec(dllimport) HRESULT __stdcall AgentMemory_Store(const char* pKey, const char* pValue, void* pVector);
    __declspec(dllimport) DWORD __stdcall AgentMemory_Recall(const char* pQuery, char* pResults, DWORD maxResults);
    
    __declspec(dllimport) HRESULT __stdcall AgentTool_Dispatch(DWORD toolId, const char* pInput, char* pOutput, DWORD cbOutput);
    __declspec(dllimport) HRESULT __stdcall AgentSelfReflect(DWORD planId, DWORD* pScore);
    __declspec(dllimport) HRESULT __stdcall AgentCrit_SelfHeal(DWORD toolId, HRESULT lastHr);
    
    __declspec(dllimport) HRESULT __stdcall AgentPolicy_CheckSafety(DWORD toolId, const char* pToken);
    __declspec(dllimport) HRESULT __stdcall AgentComm_SendA2A(DWORD receiverId, DWORD msgType, const char* pPayload, DWORD payloadLen);
    __declspec(dllimport) HRESULT __stdcall AgentTelemetry_Step(DWORD tick, DWORD duration_us);
    
    __declspec(dllimport) HRESULT __stdcall KVCache_Init();
}

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, hr) printf("[FAIL] %s (hr=0x%08lX)\n", name, hr)
#define S_OK 0

int main() {
    printf("========================================\n");
    printf("RawrXD Autonomous Agent - Smoke Test\n");
    printf("========================================\n\n");
    
    HRESULT hr;
    int passed = 0, failed = 0;
    
    // Test 1: Initialize IDE Master
    printf("[TEST 1] IDEMaster_Initialize...\n");
    hr = IDEMaster_Initialize();
    if (hr == S_OK) {
        TEST_PASS("IDEMaster_Initialize");
        passed++;
    } else {
        TEST_FAIL("IDEMaster_Initialize", hr);
        failed++;
    }
    
    // Test 2: Initialize KV Cache
    printf("[TEST 2] KVCache_Init...\n");
    hr = KVCache_Init();
    if (hr == S_OK) {
        TEST_PASS("KVCache_Init");
        passed++;
    } else {
        TEST_FAIL("KVCache_Init", hr);
        failed++;
    }
    
    // Test 3: Create a plan
    printf("[TEST 3] AgentPlan_Create...\n");
    DWORD planId = AgentPlan_Create(0, "{\"goal\":\"test autonomous loop\"}");
    if (planId > 0) {
        TEST_PASS("AgentPlan_Create");
        passed++;
        
        // Test 4: Resolve plan to JSON
        printf("[TEST 4] AgentPlan_Resolve...\n");
        char jsonBuf[1024] = {0};
        hr = AgentPlan_Resolve(planId, jsonBuf, sizeof(jsonBuf));
        if (hr == S_OK && strlen(jsonBuf) > 0) {
            TEST_PASS("AgentPlan_Resolve");
            printf("         JSON: %s\n", jsonBuf);
            passed++;
        } else {
            TEST_FAIL("AgentPlan_Resolve", hr);
            failed++;
        }
    } else {
        TEST_FAIL("AgentPlan_Create", 0);
        failed++;
    }
    
    // Test 5: Store data in agent memory
    printf("[TEST 5] AgentMemory_Store...\n");
    hr = AgentMemory_Store("test_key", "test_value", nullptr);
    if (hr == S_OK) {
        TEST_PASS("AgentMemory_Store");
        passed++;
    } else {
        TEST_FAIL("AgentMemory_Store", hr);
        failed++;
    }
    
    // Test 6: Self-reflect
    printf("[TEST 6] AgentSelfReflect...\n");
    DWORD score = 0;
    hr = AgentSelfReflect(planId, &score);
    if (hr == S_OK) {
        TEST_PASS("AgentSelfReflect");
        printf("         Score: %lu/100\n", score);
        passed++;
    } else {
        TEST_FAIL("AgentSelfReflect", hr);
        failed++;
    }
    
    // Test 7: Check safety policy
    printf("[TEST 7] AgentPolicy_CheckSafety...\n");
    hr = AgentPolicy_CheckSafety(0, nullptr);
    if (hr == S_OK) {
        TEST_PASS("AgentPolicy_CheckSafety");
        passed++;
    } else {
        TEST_FAIL("AgentPolicy_CheckSafety", hr);
        failed++;
    }
    
    // Test 8: Single autonomy step
    printf("[TEST 8] AgentLoop_SingleStep...\n");
    hr = AgentLoop_SingleStep();
    if (hr == S_OK || hr == 0x80070102) { // S_OK or E_TIMEOUT acceptable
        TEST_PASS("AgentLoop_SingleStep");
        passed++;
    } else {
        TEST_FAIL("AgentLoop_SingleStep", hr);
        failed++;
    }
    
    // Test 9: Telemetry
    printf("[TEST 9] AgentTelemetry_Step...\n");
    hr = AgentTelemetry_Step(1, 5000);
    if (hr >= 0) {
        TEST_PASS("AgentTelemetry_Step");
        passed++;
    } else {
        TEST_FAIL("AgentTelemetry_Step", hr);
        failed++;
    }
    
    // Test 10: Send A2A message
    printf("[TEST 10] AgentComm_SendA2A...\n");
    hr = AgentComm_SendA2A(1, 0, "test_payload", 12);
    if (hr == S_OK) {
        TEST_PASS("AgentComm_SendA2A");
        passed++;
    } else {
        TEST_FAIL("AgentComm_SendA2A", hr);
        failed++;
    }
    
    // Summary
    printf("\n========================================\n");
    printf("Test Results: %d passed, %d failed\n", passed, failed);
    printf("========================================\n");
    
    if (failed == 0) {
        printf("\n✓ ALL TESTS PASSED - Agent core is operational!\n\n");
        printf("Check logs at: C:\\ProgramData\\RawrXD\\logs\\ide_runtime.jsonl\n\n");
        return 0;
    } else {
        printf("\n✗ SOME TESTS FAILED - Review output above\n\n");
        return 1;
    }
}
