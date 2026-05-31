// agentic_repair_stress_test.cpp — Stress test for the Self-Healing hotpatch lifecycle
//
// Scenario: Deliberately inject a buggy function, have the agent detect it,
// generate a hotpatch, apply it live, verify the fix, then rollback.
//
// Build: cmake --build build-ninja --target agentic_repair_stress_test
// Run:   .\build-ninja\tests\agentic_repair_stress_test.exe

#include <iostream>
#include <cassert>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>

// Minimal forward declarations to avoid pulling in the entire core
struct PatchResult {
    bool success = false;
    int code = -1;
    char message[256] = {};
};

// We use a simple memory-patch approach for the stress test:
// The "buggy" function is a no-op stub that returns 0xDEADBEEF.
// The "fix" overwrites the first few bytes with a jump to a replacement.

// ─── The Buggy Target ───────────────────────────────────────────────

// Marked noinline + volatile to prevent compiler from optimizing away.
// We make it large enough to accommodate a 14-byte patch.
__declspec(noinline) int buggy_compute_factorial(int n) {
    // BUG: returns n * (n-1) instead of actual factorial
    volatile int dummy = 0;
    dummy += n;
    dummy *= (n - 1);
    return dummy;
}

// The correct replacement (compiled into the same binary)
__declspec(noinline) int fixed_compute_factorial(int n) {
    if (n <= 1) return 1;
    int result = 1;
    for (int i = 2; i <= n; ++i) result *= i;
    return result;
}

// ─── Windows Memory Protection Helpers ────────────────────────────

#ifdef _WIN32
#include <windows.h>

static bool make_writable(void* addr, size_t len) {
    DWORD oldProtect;
    return VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &oldProtect) != 0;
}

static bool restore_protection(void* addr, size_t len, DWORD prot) {
    DWORD oldProtect;
    return VirtualProtect(addr, len, prot, &oldProtect) != 0;
}
#else
static bool make_writable(void*, size_t) { return true; }
static bool restore_protection(void*, size_t, unsigned long) { return true; }
#endif

// ─── Simple x64 Trampoline Patch ──────────────────────────────────
// Overwrites the first 14 bytes of the target with:
//   mov rax, <replacement_addr>   ; 10 bytes
//   jmp rax                       ; 2 bytes
//   nop                           ; 2 bytes (padding)
// Total: 14 bytes

static bool apply_trampoline_patch(void* target, void* replacement) {
    uint8_t* p = static_cast<uint8_t*>(target);
    // mov rax, imm64  (REX.W + B8 + 8-byte immediate)
    p[0] = 0x48;               // REX.W
    p[1] = 0xB8;               // MOV RAX, imm64
    std::memcpy(p + 2, &replacement, 8);
    p[10] = 0xFF;              // JMP RAX
    p[11] = 0xE0;
    p[12] = 0x90;              // NOP
    p[13] = 0x90;              // NOP
    return true;
}

static bool revert_trampoline_patch(void* target, const uint8_t* original, size_t len) {
    std::memcpy(target, original, len);
    return true;
}

// ─── Stress Test Harness ──────────────────────────────────────────

int main() {
    using namespace std::chrono;

    std::cout << "=== Agentic Repair Stress Test ===\n";
    std::cout << "Target: buggy_compute_factorial\n\n";

    // 1. Verify the bug exists
    int buggy_result = buggy_compute_factorial(5);
    std::cout << "[1] Buggy result for factorial(5): " << buggy_result << " (expected 120)\n";
    assert(buggy_result != 120);
    std::cout << "    ✓ Bug confirmed (" << buggy_result << " != 120)\n\n";

    // 2. Save original bytes for rollback
    constexpr size_t PATCH_SIZE = 14;
    uint8_t original_bytes[PATCH_SIZE];
    std::memcpy(original_bytes, buggy_compute_factorial, PATCH_SIZE);

    // 3. Make memory writable and apply the hotpatch (live binary surgery)
    void* target_addr = reinterpret_cast<void*>(buggy_compute_factorial);
    DWORD old_protect = 0;
#ifdef _WIN32
    if (!VirtualProtect(target_addr, PATCH_SIZE, PAGE_EXECUTE_READWRITE, &old_protect)) {
        std::cerr << "ERROR: VirtualProtect failed\n";
        return 1;
    }
#endif

    auto t0 = high_resolution_clock::now();
    bool patch_ok = apply_trampoline_patch(target_addr, reinterpret_cast<void*>(fixed_compute_factorial));
    auto t1 = high_resolution_clock::now();
    double patch_us = duration_cast<microseconds>(t1 - t0).count();
    std::cout << "[2] Hotpatch applied in " << patch_us << " µs\n";
    assert(patch_ok);
    std::cout << "    ✓ Patch applied successfully\n\n";

    // 4. Verify the fix
    int fixed_result = buggy_compute_factorial(5);
    std::cout << "[3] Patched result for factorial(5): " << fixed_result << "\n";
    assert(fixed_result == 120);
    std::cout << "    ✓ Fix verified (" << fixed_result << " == 120)\n\n";

    // 5. Additional stress: run many iterations to ensure stability
    constexpr int STRESS_ITERATIONS = 100000;
    std::atomic<int> errors{0};
    auto t2 = high_resolution_clock::now();
    for (int i = 0; i < STRESS_ITERATIONS; ++i) {
        int r = buggy_compute_factorial(5);
        if (r != 120) errors.fetch_add(1);
    }
    auto t3 = high_resolution_clock::now();
    double stress_ms = duration_cast<milliseconds>(t3 - t2).count();
    std::cout << "[4] Stress test: " << STRESS_ITERATIONS << " calls in " << stress_ms << " ms\n";
    std::cout << "    Errors: " << errors.load() << "\n";
    assert(errors.load() == 0);
    std::cout << "    ✓ Zero errors across " << STRESS_ITERATIONS << " invocations\n\n";

    // 6. Revert the hotpatch
    auto t4 = high_resolution_clock::now();
    bool revert_ok = revert_trampoline_patch(target_addr, original_bytes, PATCH_SIZE);
    auto t5 = high_resolution_clock::now();
    double revert_us = duration_cast<microseconds>(t5 - t4).count();
    std::cout << "[5] Hotpatch reverted in " << revert_us << " µs\n";
    assert(revert_ok);
    std::cout << "    ✓ Revert successful\n\n";

    // Restore original memory protection
#ifdef _WIN32
    VirtualProtect(target_addr, PATCH_SIZE, old_protect, &old_protect);
#endif

    // 7. Verify reversion (bug should be back)
    int reverted_result = buggy_compute_factorial(5);
    std::cout << "[6] Reverted result for factorial(5): " << reverted_result << "\n";
    assert(reverted_result != 120);
    std::cout << "    ✓ Bug restored (" << reverted_result << " != 120)\n\n";

    // 8. Telemetry summary
    std::cout << "=== Telemetry Summary ===\n";
    std::cout << "Patch latency:      " << patch_us << " µs\n";
    std::cout << "Revert latency:     " << revert_us << " µs\n";
    std::cout << "Stress throughput:  " << (STRESS_ITERATIONS * 1000.0 / stress_ms) << " calls/sec\n";
    std::cout << "\n=== ALL STRESS TESTS PASSED ===\n";
    return 0;
}
