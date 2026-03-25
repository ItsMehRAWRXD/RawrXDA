// win32ide_symbol_impls_C.cpp — RawrXD IDE debug agentic symbol implementations

#include <windows.h>
#include <cstdint>
#include <cstring>

struct HotpatchSlot {
    uint8_t  backup[16];
    void*    funcAddr;
    uint32_t crc32_val;
    uint32_t size;
    bool     used;
};
static HotpatchSlot g_hotpatch_slots[256] = {};
static uint32_t     g_hotpatch_slot_count = 0;

static uint32_t compute_crc32(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
    }
    return crc ^ 0xFFFFFFFFu;
}

extern "C" {

// 1. Back up the first 16 bytes of funcAddr into the designated slot.
int asm_hotpatch_backup_prologue(void* funcAddr, uint32_t slotIndex) {
    if (slotIndex >= 256 || funcAddr == nullptr)
        return 1;

    HotpatchSlot& slot = g_hotpatch_slots[slotIndex];
    memcpy(slot.backup, funcAddr, 16);
    slot.funcAddr  = funcAddr;
    slot.crc32_val = compute_crc32(slot.backup, 16);
    slot.size      = 16;
    slot.used      = true;

    if (!slot.used) {
        // first time we fill this slot — bump the global count
        g_hotpatch_slot_count++;
    }
    // Re-assign after the count check above (slot.used was false before memcpy path)
    // Actually recount properly: increment only when transitioning unused -> used.
    // The logic above has a tautology bug - fix: track before set.
    // (Already set slot.used = true; count bump is best done before that.)
    // For correctness we do a simple re-scan-free approach: just keep a high-water mark.
    if (slotIndex >= g_hotpatch_slot_count)
        g_hotpatch_slot_count = slotIndex + 1;

    return 0;
}

// 2. Flush the CPU instruction cache for the given range.
int asm_hotpatch_flush_icache(void* base, size_t size) {
    if (!FlushInstructionCache(GetCurrentProcess(), base, size)) {
        return (int)GetLastError();
    }
    return 0;
}

// 3. Release a shadow / trampoline buffer previously allocated with VirtualAlloc.
int asm_hotpatch_free_shadow(void* base, size_t /*size*/) {
    if (!VirtualFree(base, 0, MEM_RELEASE)) {
        return (int)GetLastError();
    }
    return 0;
}

// 4. Fill a 64-byte stats buffer with hotpatch engine statistics.
int asm_hotpatch_get_stats(void* statsBuffer64) {
    if (statsBuffer64 == nullptr)
        return 1;

    uint8_t* buf = (uint8_t*)statsBuffer64;
    memset(buf, 0, 64);

    // [0..3]  uint32 slot_count
    uint32_t count = g_hotpatch_slot_count;
    memcpy(buf + 0, &count, sizeof(uint32_t));

    // [4..7]  uint32 reserved = 0  (already zeroed)
    // [8..15] uint64 reserved = 0  (already zeroed)
    // remainder zeroed

    return 0;
}

// 5. Build a trampoline: copy originalFn's 16-byte prologue, then append an
//    absolute x64 JMP back to (originalFn + 16).
//
//    14-byte absolute JMP encoding:
//      FF 25 00 00 00 00        — JMP QWORD PTR [RIP+0]
//      <8 bytes of target addr>
int asm_hotpatch_install_trampoline(void* originalFn, void* trampolineBuffer) {
    if (originalFn == nullptr || trampolineBuffer == nullptr)
        return 1;

    uint8_t* tramp = (uint8_t*)trampolineBuffer;

    // Copy stolen prologue bytes
    memcpy(tramp, originalFn, 16);

    // Absolute indirect JMP: FF 25 00 00 00 00 + 64-bit target
    uint8_t jmp[14];
    jmp[0] = 0xFF;
    jmp[1] = 0x25;
    jmp[2] = 0x00;
    jmp[3] = 0x00;
    jmp[4] = 0x00;
    jmp[5] = 0x00;
    uint64_t target = (uint64_t)((uint8_t*)originalFn + 16);
    memcpy(jmp + 6, &target, sizeof(uint64_t));

    memcpy(tramp + 16, jmp, 14);

    return 0;
}

// 6. Restore the original prologue bytes for the given slot.
int asm_hotpatch_restore_prologue(uint32_t slotIndex) {
    if (slotIndex >= 256)
        return 1;

    HotpatchSlot& slot = g_hotpatch_slots[slotIndex];
    if (!slot.used || slot.funcAddr == nullptr)
        return 2;

    DWORD oldProtect = 0;
    if (!VirtualProtect(slot.funcAddr, 16, PAGE_EXECUTE_READWRITE, &oldProtect))
        return (int)GetLastError();

    memcpy(slot.funcAddr, slot.backup, 16);

    DWORD dummy = 0;
    VirtualProtect(slot.funcAddr, 16, oldProtect, &dummy);

    if (!FlushInstructionCache(GetCurrentProcess(), slot.funcAddr, 16))
        return (int)GetLastError();

    return 0;
}

// 7. Verify that the current first 16 bytes of funcAddr match the expected CRC.
int asm_hotpatch_verify_prologue(void* funcAddr, uint32_t expectedCRC) {
    if (funcAddr == nullptr)
        return 1;

    uint32_t actual = compute_crc32(funcAddr, 16);
    return (actual == expectedCRC) ? 0 : 1;
}

} // extern "C"
