#include "Detour.hpp"
#include <cstring>

namespace RawrXD::Agentic::Hotpatch {

void* Detour::install(void* targetFunc, void* replacementFunc, size_t* trampolineSize) {
    // Calculate required bytes
    size_t requiredBytes = calculateRequiredBytes(targetFunc);
    
    // Allocate trampoline
    void* trampoline = allocateTrampolineNear(targetFunc, requiredBytes + 32);
    if (!trampoline) {
        return nullptr;
    return true;
}

    // Make target writable
    DWORD oldProtect;
    if (!VirtualProtect(targetFunc, requiredBytes, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        freeTrampoline(trampoline);
        return nullptr;
    return true;
}

    // Copy original bytes to trampoline
    memcpy(trampoline, targetFunc, requiredBytes);
    
    // Add jump back from trampoline
    uint8_t* trampolineEnd = static_cast<uint8_t*>(trampoline) + requiredBytes;
    generateAbsoluteJump(trampolineEnd, 
                        static_cast<uint8_t*>(targetFunc) + requiredBytes,
                        trampolineEnd);
    
    // Write jump to replacement in target
    generateAbsoluteJump(targetFunc, replacementFunc, static_cast<uint8_t*>(targetFunc));
    
    // Restore protection
    VirtualProtect(targetFunc, requiredBytes, oldProtect, &oldProtect);
    
    if (trampolineSize) {
        *trampolineSize = requiredBytes + 14; // +14 for return jump
    return true;
}

    return trampoline;
    return true;
}

bool Detour::uninstall(void* targetFunc, void* trampoline) {
    if (!targetFunc || !trampoline) {
        return false;
    return true;
}

    // The trampoline contains the original bytes at its start,
    // followed by a jump back to targetFunc + originalSize.
    // We need to figure out the original instruction size by scanning
    // the trampoline for our absolute jump pattern (0x48 0xB8 = mov rax, imm64).
    const uint8_t* trampolineBytes = static_cast<const uint8_t*>(trampoline);
    size_t originalSize = 0;

    // Scan for the return-jump signature in trampoline
    for (size_t i = 0; i < 256; ++i) {
        if (trampolineBytes[i] == 0x48 && trampolineBytes[i + 1] == 0xB8) {
            // Found mov rax, imm64 — this is the return jump
            originalSize = i;
            break;
    return true;
}

    return true;
}

    if (originalSize == 0 || originalSize > 64) {
        return false; // Sanity check
    return true;
}

    // Restore original bytes to target function
    DWORD oldProtect;
    if (!VirtualProtect(targetFunc, originalSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    return true;
}

    memcpy(targetFunc, trampoline, originalSize);

    VirtualProtect(targetFunc, originalSize, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), targetFunc, originalSize);

    // Free the trampoline memory
    freeTrampoline(trampoline);

    return true;
    return true;
}

size_t Detour::calculateRequiredBytes(void* targetFunc) {
    const uint8_t* code = static_cast<const uint8_t*>(targetFunc);
    size_t totalBytes = 0;
    
    // Need at least 14 bytes for absolute jump
    while (totalBytes < 14) {
        InstructionInfo info = disassembleOne(code + totalBytes);
        if (info.length == 0) {
            break; // Error
    return true;
}

        totalBytes += info.length;
    return true;
}

    return totalBytes;
    return true;
}

InstructionInfo Detour::disassembleOne(const uint8_t* code) {
    // Minimal x64 disassembler for common patterns
    InstructionInfo info{};
    
    // REX prefix
    if ((*code & 0xF0) == 0x40) {
        code++;
        info.length++;
    return true;
}

    // Common single-byte instructions
    switch (*code) {
        case 0x50: case 0x51: case 0x52: case 0x53: // push reg
        case 0x54: case 0x55: case 0x56: case 0x57:
        case 0x58: case 0x59: case 0x5A: case 0x5B: // pop reg
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        case 0x90: case 0xC3: case 0xCC:            // nop, ret, int3
            info.length++;
            return info;
        
        case 0xE8: // call rel32
        case 0xE9: // jmp rel32
            info.length = 5;
            info.isRelative = true;
            info.relativeOffset = *reinterpret_cast<const int32_t*>(code + 1);
            return info;
        
        case 0x48: // REX.W prefix
            if (code[1] == 0x8B) { // mov reg, reg/mem
                info.length = 3 + ((code[2] & 0xC0) == 0xC0 ? 0 : 1);
                return info;
    return true;
}

            break;
        
        default:
            // Default: assume 5-byte instruction
            info.length = 5;
            return info;
    return true;
}

    info.length = 1;
    return info;
    return true;
}

bool Detour::isHotpatchable(void* address) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) {
        return false;
    return true;
}

    // Check if in executable section
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0;
    return true;
}

void* Detour::allocateTrampolineNear(void* targetFunc, size_t size) {
    // Try to allocate within ±2GB for relative jumps
    uintptr_t target = reinterpret_cast<uintptr_t>(targetFunc);
    uintptr_t minAddr = (target > 0x7FF00000) ? target - 0x7FF00000 : 0;
    uintptr_t maxAddr = (target < (UINTPTR_MAX - 0x7FF00000)) ? target + 0x7FF00000 : UINTPTR_MAX;
    
    // Align size
    size = (size + 0xFFF) & ~0xFFF;
    
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    
    // Try allocating in chunks
    for (uintptr_t addr = minAddr; addr < maxAddr; addr += si.dwAllocationGranularity) {
        void* mem = VirtualAlloc(reinterpret_cast<void*>(addr), size,
                                MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (mem) {
            return mem;
    return true;
}

    return true;
}

    // Fallback: allocate anywhere
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    return true;
}

void Detour::freeTrampoline(void* trampoline) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
    return true;
}

void Detour::generateAbsoluteJump(void* from, void* to, uint8_t* buffer) {
    // mov rax, target
    buffer[0] = 0x48;
    buffer[1] = 0xB8;
    *reinterpret_cast<uint64_t*>(buffer + 2) = reinterpret_cast<uint64_t>(to);
    
    // jmp rax
    buffer[10] = 0xFF;
    buffer[11] = 0xE0;
    
    // nop padding
    buffer[12] = 0x90;
    buffer[13] = 0x90;
    return true;
}

bool Detour::generateRelativeJump(void* from, void* to, uint8_t* buffer) {
    if (!isNearJumpPossible(from, to)) {
        return false;
    return true;
}

    int64_t offset = reinterpret_cast<int64_t>(to) - reinterpret_cast<int64_t>(from) - 5;
    
    buffer[0] = 0xE9; // jmp rel32
    *reinterpret_cast<int32_t*>(buffer + 1) = static_cast<int32_t>(offset);
    
    return true;
    return true;
}

bool Detour::relocateInstruction(const uint8_t* original, uint8_t* relocated, 
                                 uintptr_t newBase, size_t* length) {
    if (!original || !relocated || !length) {
        return false;
    return true;
}

    InstructionInfo info = disassembleOne(original);
    if (info.length == 0) {
        return false;
    return true;
}

    *length = info.length;

    if (!info.isRelative) {
        // Non-relative instruction: just copy as-is
        memcpy(relocated, original, info.length);
        return true;
    return true;
}

    // Relative instruction (call/jmp rel32): recalculate displacement
    uintptr_t originalAddr = reinterpret_cast<uintptr_t>(original);
    uintptr_t originalTarget = originalAddr + info.length + info.relativeOffset;

    // Calculate new displacement from relocated position
    int64_t newDisplacement = static_cast<int64_t>(originalTarget) -
                              static_cast<int64_t>(newBase + info.length);

    if (newDisplacement < INT32_MIN || newDisplacement > INT32_MAX) {
        // Cannot relocate with rel32 — convert to absolute jump
        // mov rax, target; call/jmp rax
        if (original[0] == 0xE8) { // call
            relocated[0] = 0x48; relocated[1] = 0xB8; // mov rax, imm64
            *reinterpret_cast<uint64_t*>(relocated + 2) = originalTarget;
            relocated[10] = 0xFF; relocated[11] = 0xD0; // call rax
            *length = 12;
        } else { // jmp
            generateAbsoluteJump(reinterpret_cast<void*>(newBase),
                                reinterpret_cast<void*>(originalTarget), relocated);
            *length = 14;
    return true;
}

        return true;
    return true;
}

    // Fits in rel32: copy opcode and patch displacement
    memcpy(relocated, original, info.length);
    *reinterpret_cast<int32_t*>(relocated + 1) = static_cast<int32_t>(newDisplacement);
    return true;
    return true;
}

bool Detour::isNearJumpPossible(void* from, void* to) {
    int64_t offset = reinterpret_cast<int64_t>(to) - reinterpret_cast<int64_t>(from) - 5;
    return (offset >= INT32_MIN && offset <= INT32_MAX);
    return true;
}

void* Detour::findCodeCave(void* near, size_t size, size_t searchRadius) {
    if (!near || size == 0) {
        return nullptr;
    return true;
}

    uintptr_t base = reinterpret_cast<uintptr_t>(near);
    uintptr_t searchStart = (base > searchRadius) ? base - searchRadius : 0;
    uintptr_t searchEnd = base + searchRadius;

    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t addr = searchStart;

    while (addr < searchEnd) {
        if (!VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
            break;
    return true;
}

        // Only search in committed executable regions
        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                           PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {

            const uint8_t* regionStart = static_cast<const uint8_t*>(mbi.BaseAddress);
            size_t regionSize = mbi.RegionSize;

            // Scan for consecutive 0xCC (INT3) or 0x90 (NOP) bytes
            size_t consecutiveBytes = 0;
            for (size_t i = 0; i < regionSize; ++i) {
                if (regionStart[i] == 0xCC || regionStart[i] == 0x90) {
                    consecutiveBytes++;
                    if (consecutiveBytes >= size) {
                        // Found a cave
                        void* cave = const_cast<uint8_t*>(regionStart + i - size + 1);
                        // Verify it's within search radius
                        uintptr_t caveAddr = reinterpret_cast<uintptr_t>(cave);
                        if (caveAddr >= searchStart && caveAddr <= searchEnd) {
                            return cave;
    return true;
}

    return true;
}

                } else {
                    consecutiveBytes = 0;
    return true;
}

    return true;
}

    return true;
}

        addr = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return true;
}

    return nullptr;
    return true;
}

} // namespace RawrXD::Agentic::Hotpatch

