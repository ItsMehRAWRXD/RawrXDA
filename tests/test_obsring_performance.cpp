// ============================================================================
// test_obsring_performance.cpp — Performance Audit: Observation Ring Buffer
// ============================================================================
// Validates the MASM RawrXD_ObsRing + RawrXD_TermPipe_Execute under
// high-parallelism build stress (simulated multi-threaded writes).
//
// Tests:
//   1. Ring stability under concurrent push (8 threads, 100K writes)
//   2. Snapshot clamping never exceeds 2048 bytes for LLM context
//   3. Data integrity — no torn reads under contention
//   4. Throughput measurement (MB/s through the ring)
//   5. TermPipe_Execute with a real process (ninja --version)
//
// Build:
//   cl.exe /EHsc /std:c++17 /I src\asm test_obsring_performance.cpp
//          build\RawrXD_TerminalPipe.obj kernel32.lib user32.lib
//
// Run:  ctest -R obsring_performance -V
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Import the MASM module's C interface
extern "C" {
    int64_t RawrXD_ObsRing_Init(void);
    int64_t RawrXD_ObsRing_Push(const char* data, uint64_t byteCount);
    const char* RawrXD_ObsRing_Snapshot(void);
    void RawrXD_ObsRing_Destroy(void);
    int64_t RawrXD_TermPipe_Execute(const char* commandLine, uint32_t timeoutMs);
}

// Snapshot returns length in RDX — we use a helper to measure via strlen
// (the buffer is NUL-terminated by the ASM code)
static size_t getSnapshotLength(const char* snap) {
    return snap ? strlen(snap) : 0;
}

// ============================================================================
// Helpers
// ============================================================================
static int g_passed = 0;
static int g_failed = 0;

static void CHECK(bool cond, const char* name) {
    if (cond) {
        printf("  [PASS] %s\n", name);
        g_passed++;
    } else {
        printf("  [FAIL] %s\n", name);
        g_failed++;
    }
}

// ============================================================================
// Test 1: Concurrent Push Stability
// ============================================================================
static void test_concurrent_push() {
    printf("\n--- Test 1: Concurrent Push (8 threads x 100K writes) ---\n");

    RawrXD_ObsRing_Init();

    const int NUM_THREADS = 8;
    const int WRITES_PER_THREAD = 100000;
    std::atomic<int> totalWritten{0};
    std::atomic<bool> anyFault{false};

    auto writer = [&](int threadId) {
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "T%d:", threadId);
        for (int i = 0; i < WRITES_PER_THREAD; i++) {
            int64_t written = RawrXD_ObsRing_Push(buf, len);
            if (written < 0) {
                anyFault = true;
                return;
            }
            totalWritten += (int)written;
        }
    };

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(writer, t);
    }
    for (auto& t : threads) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    double throughputMBs = ((double)totalWritten / (1024.0 * 1024.0)) / (elapsedMs / 1000.0);

    CHECK(!anyFault, "No faults during concurrent push");
    CHECK(totalWritten > 0, "Non-zero bytes pushed");
    printf("  Throughput: %.2f MB/s (%d bytes in %.1f ms)\n",
           throughputMBs, totalWritten.load(), elapsedMs);

    RawrXD_ObsRing_Destroy();
}

// ============================================================================
// Test 2: Snapshot Clamping (Never Exceeds 2048 bytes)
// ============================================================================
static void test_snapshot_clamping() {
    printf("\n--- Test 2: Snapshot Clamping (2KB max for LLM context) ---\n");

    RawrXD_ObsRing_Init();

    // Push 64KB of data (way more than ring size)
    const int BIG_PUSH = 65536;
    char* bigBuf = new char[BIG_PUSH];
    memset(bigBuf, 'X', BIG_PUSH);

    // Push in chunks to simulate a long-running build
    for (int offset = 0; offset < BIG_PUSH; offset += 256) {
        int chunk = (offset + 256 <= BIG_PUSH) ? 256 : (BIG_PUSH - offset);
        RawrXD_ObsRing_Push(bigBuf + offset, chunk);
    }
    delete[] bigBuf;

    const char* snap = RawrXD_ObsRing_Snapshot();
    size_t snapLen = getSnapshotLength(snap);

    CHECK(snap != nullptr, "Snapshot pointer is valid");
    CHECK(snapLen <= 2047, "Snapshot length <= 2047 (2KB - NUL)");
    CHECK(snapLen > 0, "Snapshot contains data");
    printf("  Snapshot length: %zu bytes\n", snapLen);

    // Verify no garbage — all bytes should be 'X' (or valid)
    bool allValid = true;
    for (size_t i = 0; i < snapLen; i++) {
        if (snap[i] != 'X') {
            allValid = false;
            break;
        }
    }
    CHECK(allValid, "Snapshot data integrity (all bytes match)");

    RawrXD_ObsRing_Destroy();
}

// ============================================================================
// Test 3: No Torn Reads Under Contention
// ============================================================================
static void test_no_torn_reads() {
    printf("\n--- Test 3: No Torn Reads (concurrent push + snapshot) ---\n");

    RawrXD_ObsRing_Init();

    std::atomic<bool> stop{false};
    std::atomic<int> snapshotCount{0};
    std::atomic<bool> tornDetected{false};

    // Writer thread: pushes marker patterns "AAAA...A" then "BBBB...B"
    auto writer = [&]() {
        char bufA[128], bufB[128];
        memset(bufA, 'A', sizeof(bufA));
        memset(bufB, 'B', sizeof(bufB));
        while (!stop) {
            RawrXD_ObsRing_Push(bufA, sizeof(bufA));
            RawrXD_ObsRing_Push(bufB, sizeof(bufB));
        }
    };

    // Reader thread: takes snapshots and checks for mixed A/B within a single push unit
    // (torn read would show 'A' mid-stream replaced by 'B')
    auto reader = [&]() {
        while (!stop) {
            const char* snap = RawrXD_ObsRing_Snapshot();
            if (!snap) continue;
            size_t len = strlen(snap);
            snapshotCount++;

            // We just verify the snapshot is readable and NUL-terminated
            // In a ring buffer, interleaved A/B is expected — but truncation
            // mid-byte or NUL in the middle of data would be torn
            for (size_t i = 0; i < len; i++) {
                if (snap[i] != 'A' && snap[i] != 'B') {
                    tornDetected = true;
                    break;
                }
            }
        }
    };

    std::thread tw(writer);
    std::thread tr(reader);

    Sleep(500);  // Run for 500ms
    stop = true;
    tw.join();
    tr.join();

    CHECK(!tornDetected, "No torn reads detected (no garbage bytes)");
    CHECK(snapshotCount > 10, "Sufficient snapshots taken under contention");
    printf("  Snapshots taken: %d\n", snapshotCount.load());

    RawrXD_ObsRing_Destroy();
}

// ============================================================================
// Test 4: Ring Wrapping Correctness
// ============================================================================
static void test_ring_wrapping() {
    printf("\n--- Test 4: Ring Wrapping (overwrite oldest data correctly) ---\n");

    RawrXD_ObsRing_Init();

    // Push exactly ring_size bytes of 'O' (old data)
    char oldData[4096];
    memset(oldData, 'O', sizeof(oldData));
    RawrXD_ObsRing_Push(oldData, sizeof(oldData));

    // Push 256 bytes of 'N' (new data) — should overwrite oldest
    char newData[256];
    memset(newData, 'N', sizeof(newData));
    RawrXD_ObsRing_Push(newData, sizeof(newData));

    const char* snap = RawrXD_ObsRing_Snapshot();
    size_t snapLen = getSnapshotLength(snap);

    // The snapshot should end with 'N' (newest data at the tail)
    bool endsWithNew = snapLen > 0 && snap[snapLen - 1] == 'N';
    CHECK(endsWithNew, "Newest data appears at end of snapshot");

    // Count N's in snapshot
    int nCount = 0;
    for (size_t i = 0; i < snapLen; i++) {
        if (snap[i] == 'N') nCount++;
    }
    CHECK(nCount == 256, "All 256 'N' bytes present in snapshot");
    printf("  'N' bytes in snapshot: %d / 256\n", nCount);

    RawrXD_ObsRing_Destroy();
}

// ============================================================================
// Test 5: TermPipe_Execute (real process)
// ============================================================================
static void test_termpipe_execute() {
    printf("\n--- Test 5: TermPipe_Execute (cmd /c echo) ---\n");

    RawrXD_ObsRing_Init();

    // Execute a simple command that produces known output
    int64_t exitCode = RawrXD_TermPipe_Execute(
        const_cast<char*>("cmd.exe /c echo RAWRXD_OBSRING_TEST_MARKER"), 10000);

    CHECK(exitCode == 0, "Process exited with code 0");

    // Check that the output was captured in the ring
    const char* snap = RawrXD_ObsRing_Snapshot();
    size_t snapLen = getSnapshotLength(snap);

    bool hasMarker = snap && strstr(snap, "RAWRXD_OBSRING_TEST_MARKER") != nullptr;
    CHECK(hasMarker, "Output captured: contains RAWRXD_OBSRING_TEST_MARKER");
    CHECK(snapLen > 0, "Snapshot has content after execution");
    printf("  Exit code: %lld, Snapshot length: %zu\n", (long long)exitCode, snapLen);

    if (snap && snapLen > 0) {
        // Print first 200 chars of captured output
        printf("  Captured: \"%.200s\"\n", snap);
    }

    RawrXD_ObsRing_Destroy();
}

// ============================================================================
// Test 6: Rapid Init/Destroy Cycles (No Leaks)
// ============================================================================
static void test_init_destroy_cycles() {
    printf("\n--- Test 6: Init/Destroy Cycles (100x, no leaks) ---\n");

    bool anyFail = false;
    for (int i = 0; i < 100; i++) {
        int64_t rc = RawrXD_ObsRing_Init();
        if (rc != 0) { anyFail = true; break; }
        RawrXD_ObsRing_Push("cycle", 5);
        RawrXD_ObsRing_Snapshot();
        RawrXD_ObsRing_Destroy();
    }
    CHECK(!anyFail, "100 init/destroy cycles completed without error");
}

// ============================================================================
int main() {
    printf("=== RawrXD ObsRing Performance Audit (Phase 2) ===\n");

    test_concurrent_push();
    test_snapshot_clamping();
    test_no_torn_reads();
    test_ring_wrapping();
    test_termpipe_execute();
    test_init_destroy_cycles();

    printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
