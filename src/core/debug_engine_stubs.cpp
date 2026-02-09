// =============================================================================
// debug_engine_stubs.cpp — Stub implementations for MASM debug engine symbols
// =============================================================================
// These stubs allow the build to link without the actual MASM64 object file
// (RawrXD_Debug_Engine.asm → .obj). When the real ASM is assembled and linked,
// these stubs are superseded by the .obj definitions (strong symbols).
//
// Pattern: matches swarm_network_stubs.cpp
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include <cstdint>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ── INT3 Software Breakpoint ─────────────────────────────────────────────────

// Stub: inject INT3 (0xCC) at target address, saving original byte
// Returns 0 on success, nonzero on failure
uint32_t Dbg_InjectINT3(uint64_t targetAddress, uint8_t* outOriginalByte) {
#ifdef _WIN32
    HANDLE hProcess = GetCurrentProcess();
    SIZE_T bytesRead = 0;
    uint8_t origByte = 0;

    // Read original byte
    if (!ReadProcessMemory(hProcess, (LPCVOID)(uintptr_t)targetAddress,
                           &origByte, 1, &bytesRead)) {
        return 1;
    }
    if (outOriginalByte) {
        *outOriginalByte = origByte;
    }

    // Change protection
    DWORD oldProtect = 0;
    if (!VirtualProtect((LPVOID)(uintptr_t)targetAddress, 1,
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return 1;
    }

    // Write INT3
    uint8_t int3 = 0xCC;
    SIZE_T bytesWritten = 0;
    BOOL ok = WriteProcessMemory(hProcess, (LPVOID)(uintptr_t)targetAddress,
                                  &int3, 1, &bytesWritten);

    // Flush instruction cache
    FlushInstructionCache(hProcess, (LPCVOID)(uintptr_t)targetAddress, 1);

    // Restore protection
    DWORD dummy = 0;
    VirtualProtect((LPVOID)(uintptr_t)targetAddress, 1, oldProtect, &dummy);

    return ok ? 0 : 1;
#else
    (void)targetAddress; (void)outOriginalByte;
    return 1; // Not supported on non-Windows
#endif
}

// Stub: restore original byte at address (remove INT3)
uint32_t Dbg_RestoreINT3(uint64_t targetAddress, uint8_t originalByte) {
#ifdef _WIN32
    HANDLE hProcess = GetCurrentProcess();
    DWORD oldProtect = 0;

    if (!VirtualProtect((LPVOID)(uintptr_t)targetAddress, 1,
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return 1;
    }

    SIZE_T bytesWritten = 0;
    BOOL ok = WriteProcessMemory(hProcess, (LPVOID)(uintptr_t)targetAddress,
                                  &originalByte, 1, &bytesWritten);

    FlushInstructionCache(hProcess, (LPCVOID)(uintptr_t)targetAddress, 1);

    DWORD dummy = 0;
    VirtualProtect((LPVOID)(uintptr_t)targetAddress, 1, oldProtect, &dummy);

    return ok ? 0 : 1;
#else
    (void)targetAddress; (void)originalByte;
    return 1;
#endif
}

// ── Hardware Breakpoints ─────────────────────────────────────────────────────

// Stub: set hardware breakpoint via DR0–DR3
uint32_t Dbg_SetHardwareBreakpoint(uint64_t threadHandle, uint32_t slotIndex,
                                    uint64_t address, uint32_t condition,
                                    uint32_t sizeBytes) {
#ifdef _WIN32
    if (slotIndex > 3) return 2;

    HANDLE hThread = (HANDLE)(uintptr_t)threadHandle;

    SuspendThread(hThread);

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(hThread, &ctx)) {
        ResumeThread(hThread);
        return 1;
    }

    // Set DRn
    switch (slotIndex) {
        case 0: ctx.Dr0 = address; break;
        case 1: ctx.Dr1 = address; break;
        case 2: ctx.Dr2 = address; break;
        case 3: ctx.Dr3 = address; break;
    }

    // Enable local enable bit in DR7
    uint64_t dr7 = ctx.Dr7;
    dr7 |= (1ULL << (slotIndex * 2));           // Local enable

    // Clear condition+size bits (4 bits at 16 + 4*slot)
    uint32_t bitPos = 16 + slotIndex * 4;
    dr7 &= ~(0xFULL << bitPos);

    // Set condition (2 bits)
    dr7 |= ((uint64_t)(condition & 3) << bitPos);

    // Encode size: 1→0, 2→1, 4→3, 8→2
    uint32_t sizeEnc = 0;
    switch (sizeBytes) {
        case 1: sizeEnc = 0; break;
        case 2: sizeEnc = 1; break;
        case 4: sizeEnc = 3; break;
        case 8: sizeEnc = 2; break;
        default: sizeEnc = 0; break;
    }
    dr7 |= ((uint64_t)sizeEnc << (bitPos + 2));

    ctx.Dr7 = dr7;

    BOOL ok = SetThreadContext(hThread, &ctx);
    ResumeThread(hThread);

    return ok ? 0 : 1;
#else
    (void)threadHandle; (void)slotIndex; (void)address;
    (void)condition; (void)sizeBytes;
    return 1;
#endif
}

// Stub: clear a hardware breakpoint slot
uint32_t Dbg_ClearHardwareBreakpoint(uint64_t threadHandle, uint32_t slotIndex) {
#ifdef _WIN32
    if (slotIndex > 3) return 2;

    HANDLE hThread = (HANDLE)(uintptr_t)threadHandle;

    SuspendThread(hThread);

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(hThread, &ctx)) {
        ResumeThread(hThread);
        return 1;
    }

    // Clear DRn
    switch (slotIndex) {
        case 0: ctx.Dr0 = 0; break;
        case 1: ctx.Dr1 = 0; break;
        case 2: ctx.Dr2 = 0; break;
        case 3: ctx.Dr3 = 0; break;
    }

    // Clear enable bits
    ctx.Dr7 &= ~(3ULL << (slotIndex * 2));

    // Clear condition+size bits
    uint32_t bitPos = 16 + slotIndex * 4;
    ctx.Dr7 &= ~(0xFULL << bitPos);

    BOOL ok = SetThreadContext(hThread, &ctx);
    ResumeThread(hThread);

    return ok ? 0 : 1;
#else
    (void)threadHandle; (void)slotIndex;
    return 1;
#endif
}

// ── Single-Step ──────────────────────────────────────────────────────────────

// Stub: enable single-step via TF flag
uint32_t Dbg_EnableSingleStep(uint64_t threadHandle) {
#ifdef _WIN32
    HANDLE hThread = (HANDLE)(uintptr_t)threadHandle;
    SuspendThread(hThread);

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(hThread, &ctx)) {
        ResumeThread(hThread);
        return 1;
    }

    ctx.EFlags |= 0x100; // TF bit
    BOOL ok = SetThreadContext(hThread, &ctx);
    ResumeThread(hThread);
    return ok ? 0 : 1;
#else
    (void)threadHandle;
    return 1;
#endif
}

// Stub: disable single-step
uint32_t Dbg_DisableSingleStep(uint64_t threadHandle) {
#ifdef _WIN32
    HANDLE hThread = (HANDLE)(uintptr_t)threadHandle;
    SuspendThread(hThread);

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (!GetThreadContext(hThread, &ctx)) {
        ResumeThread(hThread);
        return 1;
    }

    ctx.EFlags &= ~0x100; // Clear TF
    BOOL ok = SetThreadContext(hThread, &ctx);
    ResumeThread(hThread);
    return ok ? 0 : 1;
#else
    (void)threadHandle;
    return 1;
#endif
}

// ── Context Capture ──────────────────────────────────────────────────────────

// Stub: capture full thread context
uint32_t Dbg_CaptureContext(uint64_t threadHandle, void* outContextBuffer,
                             uint32_t bufferSize) {
#ifdef _WIN32
    if (bufferSize < sizeof(CONTEXT)) return 2;
    if (!outContextBuffer) return 2;

    HANDLE hThread = (HANDLE)(uintptr_t)threadHandle;

    memset(outContextBuffer, 0, bufferSize);
    CONTEXT* ctx = (CONTEXT*)outContextBuffer;
    ctx->ContextFlags = CONTEXT_ALL;

    SuspendThread(hThread);
    BOOL ok = GetThreadContext(hThread, ctx);
    ResumeThread(hThread);

    return ok ? 0 : 1;
#else
    (void)threadHandle; (void)outContextBuffer; (void)bufferSize;
    return 1;
#endif
}

// ── Register Set ─────────────────────────────────────────────────────────────

// Stub: set a single GPR by index
uint32_t Dbg_SetRegister(uint64_t threadHandle, uint32_t registerIndex,
                           uint64_t value) {
#ifdef _WIN32
    HANDLE hThread = (HANDLE)(uintptr_t)threadHandle;
    SuspendThread(hThread);

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(hThread, &ctx)) {
        ResumeThread(hThread);
        return 1;
    }

    // Map index to register
    switch (registerIndex) {
        case 0:  ctx.Rax = value; break;
        case 1:  ctx.Rcx = value; break;
        case 2:  ctx.Rdx = value; break;
        case 3:  ctx.Rbx = value; break;
        case 4:  ctx.Rsp = value; break;
        case 5:  ctx.Rbp = value; break;
        case 6:  ctx.Rsi = value; break;
        case 7:  ctx.Rdi = value; break;
        case 8:  ctx.R8  = value; break;
        case 9:  ctx.R9  = value; break;
        case 10: ctx.R10 = value; break;
        case 11: ctx.R11 = value; break;
        case 12: ctx.R12 = value; break;
        case 13: ctx.R13 = value; break;
        case 14: ctx.R14 = value; break;
        case 15: ctx.R15 = value; break;
        case 16: ctx.Rip = value; break;
        case 17: ctx.EFlags = (DWORD)value; break;
        default: ResumeThread(hThread); return 2;
    }

    BOOL ok = SetThreadContext(hThread, &ctx);
    ResumeThread(hThread);
    return ok ? 0 : 1;
#else
    (void)threadHandle; (void)registerIndex; (void)value;
    return 1;
#endif
}

// ── Stack Walk ───────────────────────────────────────────────────────────────

// Stub: walk stack via RBP chain
uint32_t Dbg_WalkStack(uint64_t processHandle, uint64_t threadHandle,
                         uint64_t* outFrames, uint32_t maxFrames,
                         uint32_t* outFrameCount) {
#ifdef _WIN32
    if (!outFrames || maxFrames == 0) return 2;

    HANDLE hThread = (HANDLE)(uintptr_t)threadHandle;
    HANDLE hProcess = (HANDLE)(uintptr_t)processHandle;

    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (!GetThreadContext(hThread, &ctx)) {
        if (outFrameCount) *outFrameCount = 0;
        return 1;
    }

    outFrames[0] = ctx.Rip;
    uint32_t count = 1;

    uint64_t rbp = ctx.Rbp;
    while (count < maxFrames && rbp != 0) {
        uint64_t frameData[2] = {}; // [0]=saved_rbp, [1]=return_addr
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(hProcess, (LPCVOID)(uintptr_t)rbp,
                               frameData, 16, &bytesRead)) {
            break;
        }
        if (frameData[1] == 0) break;
        outFrames[count++] = frameData[1];
        rbp = frameData[0];
    }

    if (outFrameCount) *outFrameCount = count;
    return 0;
#else
    (void)processHandle; (void)threadHandle;
    (void)outFrames; (void)maxFrames; (void)outFrameCount;
    return 1;
#endif
}

// ── Memory Read/Write ────────────────────────────────────────────────────────

uint32_t Dbg_ReadMemory(uint64_t processHandle, uint64_t address,
                          void* outBuffer, uint64_t size,
                          uint64_t* outBytesRead) {
#ifdef _WIN32
    SIZE_T br = 0;
    BOOL ok = ReadProcessMemory((HANDLE)(uintptr_t)processHandle,
                                 (LPCVOID)(uintptr_t)address,
                                 outBuffer, (SIZE_T)size, &br);
    if (outBytesRead) *outBytesRead = br;
    return ok ? 0 : 1;
#else
    (void)processHandle; (void)address; (void)outBuffer;
    (void)size; (void)outBytesRead;
    return 1;
#endif
}

uint32_t Dbg_WriteMemory(uint64_t processHandle, uint64_t address,
                           const void* buffer, uint64_t size,
                           uint64_t* outBytesWritten) {
#ifdef _WIN32
    HANDLE hProcess = (HANDLE)(uintptr_t)processHandle;

    // Try with VirtualProtect
    DWORD oldProtect = 0;
    VirtualProtectEx(hProcess, (LPVOID)(uintptr_t)address,
                     (SIZE_T)size, PAGE_EXECUTE_READWRITE, &oldProtect);

    SIZE_T bw = 0;
    BOOL ok = WriteProcessMemory(hProcess, (LPVOID)(uintptr_t)address,
                                  buffer, (SIZE_T)size, &bw);
    if (outBytesWritten) *outBytesWritten = bw;

    // Restore protection
    DWORD dummy = 0;
    VirtualProtectEx(hProcess, (LPVOID)(uintptr_t)address,
                     (SIZE_T)size, oldProtect, &dummy);

    return ok ? 0 : 1;
#else
    (void)processHandle; (void)address; (void)buffer;
    (void)size; (void)outBytesWritten;
    return 1;
#endif
}

// ── Memory Scan ──────────────────────────────────────────────────────────────

uint32_t Dbg_MemoryScan(uint64_t processHandle, uint64_t startAddress,
                          uint64_t regionSize, const void* pattern,
                          uint32_t patternLen, uint64_t* outFoundAddress) {
#ifdef _WIN32
    if (!pattern || patternLen == 0 || !outFoundAddress) return 1;

    HANDLE hProcess = (HANDLE)(uintptr_t)processHandle;
    const uint8_t* pat = (const uint8_t*)pattern;

    uint8_t chunk[0x10000]; // 64KB
    uint64_t pos = startAddress;
    uint64_t remaining = regionSize;

    while (remaining > 0) {
        SIZE_T toRead = (remaining < sizeof(chunk)) ? (SIZE_T)remaining : sizeof(chunk);
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(hProcess, (LPCVOID)(uintptr_t)pos,
                               chunk, toRead, &bytesRead)) {
            break;
        }
        if (bytesRead < patternLen) break;

        // Linear scan
        for (SIZE_T i = 0; i <= bytesRead - patternLen; i++) {
            if (memcmp(chunk + i, pat, patternLen) == 0) {
                *outFoundAddress = pos + i;
                return 0; // Found
            }
        }

        SIZE_T advance = bytesRead - patternLen + 1;
        pos += advance;
        remaining -= advance;
    }

    *outFoundAddress = 0;
    return 1; // Not found
#else
    (void)processHandle; (void)startAddress; (void)regionSize;
    (void)pattern; (void)patternLen; (void)outFoundAddress;
    return 1;
#endif
}

// ── CRC-32 ───────────────────────────────────────────────────────────────────

uint32_t Dbg_MemoryCRC32(uint64_t processHandle, uint64_t address,
                           uint64_t size, uint32_t* outCRC) {
#ifdef _WIN32
    if (!outCRC) return 2;

    HANDLE hProcess = (HANDLE)(uintptr_t)processHandle;
    uint32_t crc = 0xFFFFFFFF;

    // CRC-32 table (standard IEEE 802.3)
    static const uint32_t crc32tab[256] = {
        0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
        0x0EDB8832,0x79DCB8A4,0xE0D5E91B,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D09,0x90BF1D9F,
        0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
        0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
        0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
        0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
        0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCF0BA899,0xB805980F,
        0x28D12D17,0x5FD6E381,0xC6DF0B3B,0xB1D0BBFD,0x2F6F7C87,0x584ED011,0xC1E3D0AB,0xB6E4E03D,
        0x4ADF5355,0x3DD863C3,0xA4D13279,0xD3D602EF,0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,
        0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,
        0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,
        0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
        0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,
        0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,
        0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
        0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,
        0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,
        0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,
        0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,
        0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,0x9B64C2B0,0xEC63F226,0x7562639C,0x026557A5,
        0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,
        0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,
        0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,0xFF0F6B70,0x66063BCA,0x11010B5C,
        0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,
        0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,
        0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,
        0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
        0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
    };

    uint8_t chunk[0x10000]; // 64KB
    uint64_t pos = address;
    uint64_t remaining = size;

    while (remaining > 0) {
        SIZE_T toRead = (remaining < sizeof(chunk)) ? (SIZE_T)remaining : sizeof(chunk);
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(hProcess, (LPCVOID)(uintptr_t)pos,
                               chunk, toRead, &bytesRead)) {
            *outCRC = 0;
            return 1;
        }

        for (SIZE_T i = 0; i < bytesRead; i++) {
            uint8_t idx = (uint8_t)((crc ^ chunk[i]) & 0xFF);
            crc = (crc >> 8) ^ crc32tab[idx];
        }

        pos += bytesRead;
        remaining -= bytesRead;
    }

    *outCRC = crc ^ 0xFFFFFFFF;
    return 0;
#else
    (void)processHandle; (void)address; (void)size; (void)outCRC;
    return 1;
#endif
}

// ── RDTSC ────────────────────────────────────────────────────────────────────

uint64_t Dbg_RDTSC(void) {
#if defined(_MSC_VER)
    return __rdtsc();
#elif defined(__GNUC__)
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    return 0;
#endif
}

#ifdef __cplusplus
}
#endif
