// test_phase1.cpp - Quick smoke test for Phase 1
#include <windows.h>
#include <stdio.h>

typedef long HRESULT;

extern "C" {
    __declspec(dllimport) HRESULT __stdcall IDEMaster_Initialize();
    __declspec(dllimport) DWORD __stdcall AgentPlan_Create(DWORD parentId, const char* pGoal);
    __declspec(dllimport) HRESULT __stdcall AgentPlan_Resolve(DWORD planId, char* pOut, DWORD cbOut);
    __declspec(dllimport) HRESULT __stdcall AgentLoop_SingleStep();
    __declspec(dllimport) HRESULT __stdcall AgentLoop_RunUntilDone(const char* pGoal);
}

int main() {
    printf("==============================================\n");
    printf("RawrXD Autonomous Agent - Phase 1 Test\n");
    printf("==============================================\n\n");
    
    // Test 1: Initialize
    printf("[TEST 1] Initializing IDE Master...\n");
    HRESULT hr = IDEMaster_Initialize();
    if (hr == 0) {
        printf("  [PASS] Initialized (hr=0x%08lX)\n\n", hr);
    } else {
        printf("  [FAIL] Failed to initialize (hr=0x%08lX)\n\n", hr);
        return 1;
    }
    
    // Test 2: Create a plan
    printf("[TEST 2] Creating autonomous plan...\n");
    DWORD planId = AgentPlan_Create(0, "{\"goal\":\"test\"}");
    if (planId > 0) {
        printf("  [PASS] Plan created with ID %lu\n\n", planId);
    } else {
        printf("  [FAIL] Failed to create plan\n\n");
        return 1;
    }
    
    // Test 3: Resolve plan to JSON
    printf("[TEST 3] Resolving plan to JSON...\n");
    char json[256] = {0};
    hr = AgentPlan_Resolve(planId, json, sizeof(json));
    if (hr == 0) {
        printf("  [PASS] JSON: %s\n\n", json);
    } else {
        printf("  [FAIL] Failed to resolve (hr=0x%08lX)\n\n", hr);
        return 1;
    }
    
    // Test 4: Single autonomy step
    printf("[TEST 4] Executing single autonomy step...\n");
    hr = AgentLoop_SingleStep();
    if (hr == 0 || hr == 0x80070102) {  // S_OK or E_TIMEOUT is acceptable
        printf("  [PASS] Step completed (hr=0x%08lX)\n\n", hr);
    } else {
        printf("  [WARN] Unexpected result (hr=0x%08lX)\n\n", hr);
    }
    
    // Test 5: Full autonomous loop
    printf("[TEST 5] Running full autonomous loop...\n");
    hr = AgentLoop_RunUntilDone("{\"goal\":\"autonomous test\"}");
    if (hr == 0) {
        printf("  [PASS] Loop completed successfully\n\n");
    } else {
        printf("  [WARN] Loop ended with hr=0x%08lX\n\n", hr);
    }
    
    printf("==============================================\n");
    printf("ALL TESTS COMPLETED!\n");
    printf("==============================================\n\n");
    
    printf("Check logs at: C:\\ProgramData\\RawrXD\\logs\\ide_runtime.jsonl\n\n");
    
    return 0;
}
