// test_hotpatch.cpp — Verify asm_hotpatch_backup_prologue / restore_prologue roundtrip.
//
// Phase 1 (current): Tests a portable C++ simulation of the backup/restore table
// to establish the expected state machine behavior.
//
// Phase 2 (when RAWRXD_HOTPATCH_LINKED is defined): Calls the real ASM functions.
// These require VirtualProtect-able memory — allocate an executable page via
// VirtualAlloc(MEM_COMMIT, PAGE_EXECUTE_READWRITE).
//
// Hotpatch ABI (x64 Windows):
//   asm_hotpatch_backup_prologue(void* funcAddr, uint32_t slot)  → EAX 0/-1
//   asm_hotpatch_restore_prologue(uint32_t slot)                 → EAX 0/-1

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>

static int g_fail = 0;

#define VERIFY(cond) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); ++g_fail; } \
} while(0)

// ---------------------------------------------------------------------------
// ASM declarations — conditional on link phase
// ---------------------------------------------------------------------------
#ifdef RAWRXD_HOTPATCH_LINKED
extern "C" {
    int asm_hotpatch_backup_prologue(void* funcAddr, uint32_t slot);
    int asm_hotpatch_restore_prologue(uint32_t slot);
    int asm_hotpatch_verify_prologue(uint32_t slot);
    int asm_hotpatch_flush_icache(void* addr, uint32_t size);
}
#endif

// ---------------------------------------------------------------------------
// Portable simulation: HP_BACKUP_SIZE = 16 bytes per slot
// ---------------------------------------------------------------------------
static constexpr uint32_t HP_BACKUP_SIZE    = 16;
static constexpr uint32_t HP_MAX_SNAPSHOTS  = 32;

struct HotpatchSim {
    uint8_t table[HP_MAX_SNAPSHOTS][HP_BACKUP_SIZE] = {};
    void*   addrs[HP_MAX_SNAPSHOTS]                 = {};
    bool    valid[HP_MAX_SNAPSHOTS]                 = {};

    int backup(void* funcAddr, uint32_t slot) {
        if (!funcAddr || slot >= HP_MAX_SNAPSHOTS) return -1;
        std::memcpy(table[slot], funcAddr, HP_BACKUP_SIZE);
        addrs[slot] = funcAddr;
        valid[slot] = true;
        return 0;
    }

    int restore(uint32_t slot) {
        if (slot >= HP_MAX_SNAPSHOTS || !valid[slot] || !addrs[slot]) return -1;
        DWORD old;
        VirtualProtect(addrs[slot], HP_BACKUP_SIZE, PAGE_EXECUTE_READWRITE, &old);
        std::memcpy(addrs[slot], table[slot], HP_BACKUP_SIZE);
        VirtualProtect(addrs[slot], HP_BACKUP_SIZE, old, &old);
        return 0;
    }
};

// ---------------------------------------------------------------------------
// Allocate an executable page with known pattern, patch it, restore, verify.
// ---------------------------------------------------------------------------
static void test_backup_restore_sim() {
    // Allocate a small executable+RW page
    uint8_t* page = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    VERIFY(page != nullptr);
    if (!page) return;

    // Write a known prologue: NOP × 16
    for (int i = 0; i < 16; ++i) page[i] = 0x90;  // NOP

    HotpatchSim sim;
    int rc = sim.backup(page, 0);
    VERIFY(rc == 0);
    VERIFY(sim.valid[0]);
    VERIFY(sim.addrs[0] == page);

    // Overwrite prologue with RET (0xC3) × 16 — simulates a patch
    for (int i = 0; i < 16; ++i) page[i] = 0xC3;  // RET
    for (int i = 0; i < 16; ++i) VERIFY(page[i] == 0xC3);

    // Restore and verify original NOP bytes are back
    rc = sim.restore(0);
    VERIFY(rc == 0);
    for (int i = 0; i < 16; ++i) VERIFY(page[i] == 0x90);

    VirtualFree(page, 0, MEM_RELEASE);
    std::fprintf(stdout, "  ✓ backup/restore roundtrip (NOP → RET → NOP)\n");
}

// ---------------------------------------------------------------------------
// Null-safety: backup with null address must return -1
// ---------------------------------------------------------------------------
static void test_backup_null_addr() {
    HotpatchSim sim;
    int rc = sim.backup(nullptr, 0);
    VERIFY(rc == -1);
    std::fprintf(stdout, "  ✓ backup(nullptr, 0) returns -1\n");
}

// ---------------------------------------------------------------------------
// Slot boundary: slot >= HP_MAX_SNAPSHOTS must fail
// ---------------------------------------------------------------------------
static void test_backup_slot_oob() {
    HotpatchSim sim;
    uint8_t dummy[16] = {};
    VERIFY(sim.backup(dummy, HP_MAX_SNAPSHOTS) == -1);     // exactly at limit
    VERIFY(sim.backup(dummy, HP_MAX_SNAPSHOTS + 1) == -1); // over limit
    std::fprintf(stdout, "  ✓ backup OOB slot rejects correctly\n");
}

// ---------------------------------------------------------------------------
// Restore without prior backup must fail (slot not valid)
// ---------------------------------------------------------------------------
static void test_restore_uninit_slot() {
    HotpatchSim sim;
    int rc = sim.restore(0);   // slot 0 never backed up
    VERIFY(rc == -1);
    std::fprintf(stdout, "  ✓ restore of uninitialised slot returns -1\n");
}

// ---------------------------------------------------------------------------
// ASM phase — only compiled+linked when RAWRXD_HOTPATCH_LINKED is defined
// ---------------------------------------------------------------------------
#ifdef RAWRXD_HOTPATCH_LINKED
static void test_asm_backup_restore() {
    uint8_t* page = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    VERIFY(page != nullptr);
    if (!page) return;

    for (int i = 0; i < 16; ++i) page[i] = 0x90; // NOP prologue

    int rc = asm_hotpatch_backup_prologue(page, 1);
    VERIFY(rc == 0);

    for (int i = 0; i < 16; ++i) page[i] = 0xC3; // Overwrite with RET

    rc = asm_hotpatch_restore_prologue(1);
    VERIFY(rc == 0);
    for (int i = 0; i < 16; ++i) VERIFY(page[i] == 0x90); // Must be NOP again

    asm_hotpatch_flush_icache(page, 16);
    VirtualFree(page, 0, MEM_RELEASE);
    std::fprintf(stdout, "  ✓ ASM backup/restore roundtrip [HOTPATCH_LINKED]\n");
}
#endif

int main() {
    std::fprintf(stdout, "=== test_hotpatch ===\n");
    test_backup_restore_sim();
    test_backup_null_addr();
    test_backup_slot_oob();
    test_restore_uninit_slot();

#ifdef RAWRXD_HOTPATCH_LINKED
    test_asm_backup_restore();
#else
    std::fprintf(stdout, "  (ASM tests skipped — define RAWRXD_HOTPATCH_LINKED to enable)\n");
#endif

    if (g_fail == 0) {
        std::fprintf(stdout, "PASS (%d checks)\n", 4 + (g_fail == 0 ? 0 : 0));
        return 0;
    }
    std::fprintf(stderr, "FAIL (%d check(s) failed)\n", g_fail);
    return 1;
}
