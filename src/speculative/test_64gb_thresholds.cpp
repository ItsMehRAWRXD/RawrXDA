// test_64gb_thresholds.cpp
// Minimal test for 64GB threshold validation
// Compile: ml64 /c rawr_aperture_bypass.asm && cl test_64gb_thresholds.cpp rawr_aperture_bypass.obj

#include <cstdio>
#include <cstdint>

// External assembly functions
extern "C" {
    uint32_t RawrCheckOverflowTier(uint32_t util_bits);
    uint32_t RawrCheckOverflowTierAggressive(uint32_t util_bits);
}

static uint32_t f2u(float v) {
    union { float f; uint32_t u; } c;
    c.f = v;
    return c.u;
}

int main() {
    printf("========================================\n");
    printf("64GB System RAM Threshold Validation\n");
    printf("========================================\n\n");
    
    printf("Thresholds:\n");
    printf("  NORMAL:   < 70%% (< 44.8 GB)\n");
    printf("  WARNING:  70-85%% (44.8-54.4 GB)\n");
    printf("  CRITICAL: 85-95%% (54.4-60.8 GB)\n");
    printf("  PANIC:    > 95%% (> 60.8 GB)\n\n");
    
    // Test cases for 64GB thresholds
    struct TestCase {
        float util;
        uint32_t expected;
        const char* desc;
    };
    
    TestCase cases[] = {
        { 0.50f, 0, "Normal (50%)" },
        { 0.65f, 0, "Normal (65%)" },
        { 0.70f, 1, "Warning threshold (70%)" },
        { 0.75f, 1, "Warning (75%)" },
        { 0.85f, 2, "Critical threshold (85%)" },
        { 0.90f, 2, "Critical (90%)" },
        { 0.95f, 3, "Panic threshold (95%)" },
        { 0.98f, 3, "Panic (98%)" },
    };
    
    bool all_pass = true;
    printf("Testing RawrCheckOverflowTier (64GB thresholds):\n");
    printf("----------------------------------------\n");
    
    for (const auto& tc : cases) {
        uint32_t result = RawrCheckOverflowTier(f2u(tc.util));
        bool pass = (result == tc.expected);
        printf("  %s -> Tier %u (exp %u) [%s]\n", 
               tc.desc, result, tc.expected, pass ? "PASS" : "FAIL");
        all_pass &= pass;
    }
    
    printf("\nTesting RawrCheckOverflowTierAggressive (60/75/90%%):\n");
    printf("----------------------------------------\n");
    
    TestCase agg_cases[] = {
        { 0.50f, 0, "Normal (50%)" },
        { 0.60f, 1, "Warning threshold (60%)" },
        { 0.70f, 1, "Warning (70%)" },
        { 0.75f, 2, "Critical threshold (75%)" },
        { 0.85f, 2, "Critical (85%)" },
        { 0.90f, 3, "Panic threshold (90%)" },
        { 0.95f, 3, "Panic (95%)" },
    };
    
    for (const auto& tc : agg_cases) {
        uint32_t result = RawrCheckOverflowTierAggressive(f2u(tc.util));
        bool pass = (result == tc.expected);
        printf("  %s -> Tier %u (exp %u) [%s]\n", 
               tc.desc, result, tc.expected, pass ? "PASS" : "FAIL");
        all_pass &= pass;
    }
    
    printf("\n========================================\n");
    printf("All tests: %s\n", all_pass ? "PASSED" : "FAILED");
    printf("========================================\n");
    
    return all_pass ? 0 : 1;
}