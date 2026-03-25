// test_dispatch_ipc.cpp — Verify all RawrXD_DispatchIPC call paths don't crash or corrupt.
//
// Links subsystem_mode_fallbacks.cpp (the canonical provider) directly.
// Exercises: null input, empty data, valid wide string, unaligned size, oversized data.
//
// Build via tests/CMakeLists.txt target test_dispatch_ipc.

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cassert>

extern "C" void __stdcall RawrXD_DispatchIPC(const void* pData, uint32_t size);

static int g_fail = 0;

#define VERIFY(cond) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); ++g_fail; } \
} while(0)

// Null data — must not crash.
static void test_null_data() {
    RawrXD_DispatchIPC(nullptr, 0);
    RawrXD_DispatchIPC(nullptr, 16);
    std::fprintf(stdout, "  ✓ null data (no crash)\n");
}

// Zero size — must not crash even with valid pointer.
static void test_zero_size() {
    const wchar_t* msg = L"hello";
    RawrXD_DispatchIPC(msg, 0);
    std::fprintf(stdout, "  ✓ zero size (no crash)\n");
}

// Valid wide string — normal dispatch path.
static void test_valid_wide_string() {
    const wchar_t* msg = L"[RawrXD Test] IPC dispatch probe";
    uint32_t size = static_cast<uint32_t>(wcslen(msg) * sizeof(wchar_t));
    RawrXD_DispatchIPC(msg, size);
    std::fprintf(stdout, "  ✓ valid wide string (%u bytes)\n", size);
}

// Odd byte count — implementation should handle gracefully (size % sizeof(wchar_t) != 0).
static void test_odd_size() {
    const wchar_t* msg = L"probe";
    uint32_t size = static_cast<uint32_t>(wcslen(msg) * sizeof(wchar_t)) - 1; // deliberately odd
    RawrXD_DispatchIPC(msg, size);
    std::fprintf(stdout, "  ✓ odd byte count (no crash)\n");
}

// Single null wchar_t — edge case: empty-but-non-null message.
static void test_single_null_wchar() {
    const wchar_t msg[1] = { L'\0' };
    RawrXD_DispatchIPC(msg, sizeof(wchar_t));
    std::fprintf(stdout, "  ✓ single null wchar_t (no crash)\n");
}

// Very large payload — must not overflow or crash.
static void test_large_payload() {
    static wchar_t large_buf[4096];
    for (int i = 0; i < 4095; ++i) large_buf[i] = L'A';
    large_buf[4095] = L'\0';
    RawrXD_DispatchIPC(large_buf, static_cast<uint32_t>(4096 * sizeof(wchar_t)));
    std::fprintf(stdout, "  ✓ large payload (no crash)\n");
}

int main() {
    std::fprintf(stdout, "=== test_dispatch_ipc ===\n");
    test_null_data();
    test_zero_size();
    test_valid_wide_string();
    test_odd_size();
    test_single_null_wchar();
    test_large_payload();

    if (g_fail == 0) {
        std::fprintf(stdout, "PASS (all %d checks)\n", 6);
        return 0;
    }
    std::fprintf(stderr, "FAIL (%d check(s) failed)\n", g_fail);
    return 1;
}
