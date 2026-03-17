#include <windows.h>
#include <stdio.h>
#include <cstring>
#include <cstdint>

// ============================================================================
// Week 1 Integration Test: Inline Edit Pipeline Validation
// Tests: Hotkey → Context → Request → Stream → Validate → Commit
// ============================================================================

// Forward declarations from assembly modules
extern "C" {
    // Keybinding module
    int GlobalHotkey_Register(HWND hwnd);
    int InlineEdit_ContextCapture(const char* code, int cursorPos, char* outContext);
    
    // Streaming module
    int InlineEdit_RequestStreaming(const char* instruction, const char* context, HWND hwnd);
    int InlineEdit_StreamToken(const char* token, int tokenLen, HWND hwnd, int isDone);
    
    // Validator module  
    int DiffValidator_CompareASTs(const char* original, const char* generated, void* resultBuffer);
    int DiffValidator_ApproveEdit(void* resultBuffer);
    
    // Context extractor
    int ContextExtractor_GetRawContext(HWND hwnd, char* buffer, int maxBytes, int linesContext);
    int ContextExtractor_DetectLanguage(const char* buffer, int bufferSize);
}

// Test constants
#define TEST_PASS 0
#define TEST_HOTKEY_FAILED 1
#define TEST_CONTEXT_FAILED 2
#define TEST_REQUEST_FAILED 3
#define TEST_VALIDATE_FAILED 5

// Test data
const char TEST_CODE[] = 
    "mov rax, rbx\n"
    "mov rcx, rdx\n"
    "add rax, rcx\n"
    "ret\n";

const char TEST_INSTRUCTION[] = "Reorder the add before mov";

const char GENERATED_CODE[] = 
    "add rax, rcx\n"
    "mov rax, rbx\n"
    "mov rcx, rdx\n"
    "ret\n";

// ============================================================================
// Test Phase 1: Hotkey Registration
// ============================================================================
int TestPhase1_HotkeyRegistration() {
    printf("[TEST] Phase 1: Hotkey Registration\n");
    
    // Create test window (minimal Win32 window)
    HWND hwnd = GetDesktopWindow();
    
    if (!hwnd) {
        printf("  [FAIL] Could not get desktop window\n");
        return TEST_HOTKEY_FAILED;
    }
    
    int result = GlobalHotkey_Register(hwnd);
    if (result != 0) {
        printf("  [PASS] Hotkey registered successfully\n");
        return 0;
    }
    
    printf("  [FAIL] Hotkey registration returned %d\n", result);
    return TEST_HOTKEY_FAILED;
}

// ============================================================================
// Test Phase 2: Context Extraction
// ============================================================================
int TestPhase2_ContextExtraction() {
    printf("[TEST] Phase 2: Context Extraction\n");
    
    char contextBuffer[4096] = {0};
    int cursorPos = 19;  // Position in test code
    
    int bytesExtracted = InlineEdit_ContextCapture(
        TEST_CODE, 
        cursorPos, 
        contextBuffer
    );
    
    if (bytesExtracted > 0) {
        printf("  [PASS] Context extracted (%d bytes)\n", bytesExtracted);
        printf("  Context: %s\n", contextBuffer);
        return 0;
    }
    
    printf("  [FAIL] Context extraction failed\n");
    return TEST_CONTEXT_FAILED;
}

// ============================================================================
// Test Phase 3: LLM Request Streaming
// ============================================================================
int TestPhase3_RequestStreaming() {
    printf("[TEST] Phase 3: Streaming Request\n");
    
    HWND hwnd = GetDesktopWindow();
    
    int result = InlineEdit_RequestStreaming(
        TEST_INSTRUCTION,
        TEST_CODE,
        hwnd
    );
    
    if (result == 0) {
        printf("  [PASS] Request queued\n");
        return 0;
    }
    
    printf("  [FAIL] Request streaming failed (%d)\n", result);
    return TEST_REQUEST_FAILED;
}

// ============================================================================
// Test Phase 4: Token Streaming
// ============================================================================
int TestPhase4_TokenProcessing() {
    printf("[TEST] Phase 4: Token Processing\n");
    
    HWND hwnd = GetDesktopWindow();
    
    // Simulate token stream
    const char* tokens[] = {"add", " rax", ",", " rcx"};
    int tokenCount = sizeof(tokens) / sizeof(tokens[0]);
    
    for (int i = 0; i < tokenCount; i++) {
        int isDone = (i == tokenCount - 1) ? 1 : 0;
        int len = strlen(tokens[i]);
        
        int result = InlineEdit_StreamToken(tokens[i], len, hwnd, isDone);
        if (result != 0) {
            printf("  [FAIL] Token streaming failed at token %d\n", i);
            return TEST_REQUEST_FAILED;
        }
    }
    
    printf("  [PASS] Token stream processed (%d tokens)\n", tokenCount);
    return 0;
}

// ============================================================================
// Test Phase 5: Code Validation
// ============================================================================
int TestPhase5_CodeValidation() {
    printf("[TEST] Phase 5: Code Validation\n");
    
    // Allocate validator result structure (simplified)
    char resultBuffer[256] = {0};
    
    int compareResult = DiffValidator_CompareASTs(
        TEST_CODE,
        GENERATED_CODE,
        resultBuffer
    );
    
    if (compareResult == 0) {
        // Approve the edit
        int approveResult = DiffValidator_ApproveEdit(resultBuffer);
        if (approveResult == 0) {
            printf("  [PASS] Code validation passed and approved\n");
            return 0;
        }
    }
    
    printf("  [FAIL] Code validation failed\n");
    return TEST_VALIDATE_FAILED;
}

// ============================================================================
// Main Test Orchestrator
// ============================================================================
int main(int argc, char* argv[]) {
    printf("=================================================================\n");
    printf("RawrXD Inline Edit - Week 1 Integration Test\n");
    printf("=================================================================\n\n");
    
    int testResults = TEST_PASS;
    
    // Phase 1
    testResults |= TestPhase1_HotkeyRegistration();
    printf("\n");
    
    // Phase 2
    testResults |= TestPhase2_ContextExtraction();
    printf("\n");
    
    // Phase 3
    testResults |= TestPhase3_RequestStreaming();
    printf("\n");
    
    // Phase 4
    testResults |= TestPhase4_TokenProcessing();
    printf("\n");
    
    // Phase 5
    testResults |= TestPhase5_CodeValidation();
    printf("\n");
    
    // Summary
    printf("=================================================================\n");
    if (testResults == TEST_PASS) {
        printf("✓ ALL TESTS PASSED - Week 1 Pipeline Validation Complete\n");
    } else {
        printf("✗ TESTS FAILED - Error code: 0x%X\n", testResults);
    }
    printf("=================================================================\n");
    
    return testResults;
}
