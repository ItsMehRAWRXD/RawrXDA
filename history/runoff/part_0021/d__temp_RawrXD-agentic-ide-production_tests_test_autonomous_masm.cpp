// ============================================================================
// test_autonomous_masm.cpp - Test harness for pure MASM autonomous features
// ============================================================================
// Verifies that MASM autonomous widgets integrate correctly with Win32
// ============================================================================

#include <windows.h>
#include <stdio.h>
#include <string.h>

// External MASM function declarations
extern "C" {
    // Autonomous Suggestion API
    void* AutonomousSuggestion_Create(
        const char* suggestionId,
        const char* type,
        const char* filePath,
        int lineNumber
    );
    
    void AutonomousSuggestion_Destroy(void* pSuggestion);
    
    int AutonomousSuggestion_SetCode(
        void* pSuggestion,
        const char* originalCode,
        const char* suggestedCode,
        const char* explanation
    );
    
    void AutonomousSuggestion_SetConfidence(
        void* pSuggestion,
        double confidence
    );
    
    void AutonomousSuggestion_Accept(void* pSuggestion);
    void AutonomousSuggestion_Reject(void* pSuggestion);
    
    // Security Issue API
    void* SecurityIssue_Create(
        const char* issueId,
        const char* severity,
        const char* issueType,
        const char* filePath,
        int lineNumber
    );
    
    void SecurityIssue_Destroy(void* pIssue);
    
    int SecurityIssue_SetDetails(
        void* pIssue,
        const char* vulnerableCode,
        const char* description,
        const char* suggestedFix
    );
    
    void SecurityIssue_SetRiskScore(void* pIssue, double riskScore);
    
    // Performance Optimization API
    void* PerformanceOptimization_Create(
        const char* optimizationId,
        const char* optimizationType,
        const char* filePath,
        int lineNumber
    );
    
    void PerformanceOptimization_Destroy(void* pOptimization);
    
    int PerformanceOptimization_SetImplementations(
        void* pOptimization,
        const char* currentImplementation,
        const char* optimizedImplementation,
        const char* reasoning
    );
    
    void PerformanceOptimization_SetMetrics(
        void* pOptimization,
        double expectedSpeedup,
        long long expectedMemorySaving,
        double confidence
    );
    
    // Widget API
    void* SuggestionWidget_Create(
        HWND hParentWnd,
        void* pSuggestion,
        int x,
        int y,
        int width,
        int height
    );
    
    void SuggestionWidget_Destroy(void* pWidget);
    void SuggestionWidget_OnAccept(void* pWidget);
    void SuggestionWidget_OnReject(void* pWidget);
    
    // Collection API
    void* StringList_Create();
    void StringList_Destroy(void* pList);
    void* KeyValueMap_Create();
    void KeyValueMap_Destroy(void* pMap);
}

// Test counters
int g_testsPassed = 0;
int g_testsFailed = 0;

// Test macros
#define TEST(name) \
    printf("\n[TEST] %s\n", name); \
    bool testResult = true;

#define ASSERT(condition) \
    if (!(condition)) { \
        printf("  FAIL: %s (line %d)\n", #condition, __LINE__); \
        testResult = false; \
        g_testsFailed++; \
    }

#define END_TEST() \
    if (testResult) { \
        printf("  PASS\n"); \
        g_testsPassed++; \
    }

// Test functions
void TestAutonomousSuggestionLifecycle() {
    TEST("Autonomous Suggestion Lifecycle");
    
    // Create suggestion
    void* suggestion = AutonomousSuggestion_Create(
        "SUG-001",
        "optimization",
        "src/main.cpp",
        42
    );
    ASSERT(suggestion != nullptr);
    
    // Set code details
    int result = AutonomousSuggestion_SetCode(
        suggestion,
        "for (int i = 0; i < n; i++)",
        "std::for_each(begin, end, lambda)",
        "Use STL algorithm for better performance"
    );
    ASSERT(result == 1);  // TRUE
    
    // Set confidence
    AutonomousSuggestion_SetConfidence(suggestion, 0.85);
    
    // Accept suggestion
    AutonomousSuggestion_Accept(suggestion);
    
    // Clean up
    AutonomousSuggestion_Destroy(suggestion);
    
    END_TEST();
}

void TestSecurityIssueLifecycle() {
    TEST("Security Issue Lifecycle");
    
    // Create security issue
    void* issue = SecurityIssue_Create(
        "SEC-001",
        "critical",
        "sql_injection",
        "src/database.cpp",
        127
    );
    ASSERT(issue != nullptr);
    
    // Set details
    int result = SecurityIssue_SetDetails(
        issue,
        "query = \"SELECT * FROM users WHERE id = \" + userId;",
        "SQL injection vulnerability - user input concatenated directly",
        "Use prepared statements with parameter binding"
    );
    ASSERT(result == 1);
    
    // Set risk score
    SecurityIssue_SetRiskScore(issue, 9.5);
    
    // Clean up
    SecurityIssue_Destroy(issue);
    
    END_TEST();
}

void TestPerformanceOptimizationLifecycle() {
    TEST("Performance Optimization Lifecycle");
    
    // Create optimization
    void* optimization = PerformanceOptimization_Create(
        "OPT-001",
        "caching",
        "src/renderer.cpp",
        256
    );
    ASSERT(optimization != nullptr);
    
    // Set implementations
    int result = PerformanceOptimization_SetImplementations(
        optimization,
        "texture = loadTexture(path);",
        "texture = textureCache.get(path);",
        "Cache loaded textures to avoid redundant I/O"
    );
    ASSERT(result == 1);
    
    // Set metrics
    PerformanceOptimization_SetMetrics(
        optimization,
        3.5,        // 3.5x speedup
        1048576,    // 1MB memory saved
        0.92        // 92% confidence
    );
    
    // Clean up
    PerformanceOptimization_Destroy(optimization);
    
    END_TEST();
}

void TestCollectionLifecycle() {
    TEST("Collection Management");
    
    // Create string list
    void* stringList = StringList_Create();
    ASSERT(stringList != nullptr);
    
    // Clean up string list
    StringList_Destroy(stringList);
    
    // Create key-value map
    void* kvMap = KeyValueMap_Create();
    ASSERT(kvMap != nullptr);
    
    // Clean up map
    KeyValueMap_Destroy(kvMap);
    
    END_TEST();
}

void TestMultipleSuggestions() {
    TEST("Multiple Suggestions Stress Test");
    
    const int COUNT = 10;
    void* suggestions[COUNT];
    
    // Create multiple suggestions
    for (int i = 0; i < COUNT; i++) {
        char id[32];
        sprintf_s(id, sizeof(id), "SUG-%03d", i);
        
        suggestions[i] = AutonomousSuggestion_Create(
            id,
            "refactoring",
            "test.cpp",
            i * 10
        );
        ASSERT(suggestions[i] != nullptr);
    }
    
    // Clean up all suggestions
    for (int i = 0; i < COUNT; i++) {
        AutonomousSuggestion_Destroy(suggestions[i]);
    }
    
    END_TEST();
}

void TestNullPointerSafety() {
    TEST("Null Pointer Safety");
    
    // Should not crash on null pointers
    AutonomousSuggestion_Destroy(nullptr);
    SecurityIssue_Destroy(nullptr);
    PerformanceOptimization_Destroy(nullptr);
    StringList_Destroy(nullptr);
    KeyValueMap_Destroy(nullptr);
    
    // Widget destroy with null (stub implementation)
    SuggestionWidget_Destroy(nullptr);
    
    END_TEST();
}

void TestLongStrings() {
    TEST("Long String Handling");
    
    void* suggestion = AutonomousSuggestion_Create(
        "SUG-LONG",
        "test",
        "very/long/path/to/file/that/exceeds/normal/length.cpp",
        999
    );
    ASSERT(suggestion != nullptr);
    
    // Very long code strings (will be truncated to 511 chars)
    char longCode[1024];
    memset(longCode, 'A', sizeof(longCode) - 1);
    longCode[sizeof(longCode) - 1] = '\0';
    
    int result = AutonomousSuggestion_SetCode(
        suggestion,
        longCode,
        longCode,
        "Test truncation handling"
    );
    ASSERT(result == 1);
    
    AutonomousSuggestion_Destroy(suggestion);
    
    END_TEST();
}

// Main test runner
int main(int argc, char* argv[]) {
    printf("==============================================\n");
    printf("MASM Autonomous Features Test Suite\n");
    printf("==============================================\n");
    
    // Run all tests
    TestAutonomousSuggestionLifecycle();
    TestSecurityIssueLifecycle();
    TestPerformanceOptimizationLifecycle();
    TestCollectionLifecycle();
    TestMultipleSuggestions();
    TestNullPointerSafety();
    TestLongStrings();
    
    // Print summary
    printf("\n==============================================\n");
    printf("Test Summary:\n");
    printf("  Passed: %d\n", g_testsPassed);
    printf("  Failed: %d\n", g_testsFailed);
    printf("==============================================\n");
    
    if (g_testsFailed > 0) {
        printf("\n❌ TESTS FAILED\n");
        return 1;
    } else {
        printf("\n✅ ALL TESTS PASSED\n");
        return 0;
    }
}
