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
    }
    
    // Make target writable
    DWORD oldProtect;
    if (!VirtualProtect(targetFunc, requiredBytes, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        freeTrampoline(trampoline);
        return nullptr;
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
    }
    
    return trampoline;
}

bool Detour::uninstall(void* targetFunc, void* trampoline) {
    // TODO: Restore original bytes from trampoline
    return false;
}

size_t Detour::calculateRequiredBytes(void* targetFunc) {
    const uint8_t* code = static_cast<const uint8_t*>(targetFunc);
    size_t totalBytes = 0;
    
    // Need at least 14 bytes for absolute jump
    while (totalBytes < 14) {
        InstructionInfo info = disassembleOne(code + totalBytes);
        if (info.length == 0) {
            break; // Error
        }
        totalBytes += info.length;
    }
    
    return totalBytes;
}

InstructionInfo Detour::disassembleOne(const uint8_t* code) {
    // Minimal x64 disassembler for common patterns
    InstructionInfo info{};
    
    // REX prefix
    if ((*code & 0xF0) == 0x40) {
        code++;
        info.length++;
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
            }
            break;
        
        default:
            // Default: assume 5-byte instruction
            info.length = 5;
            return info;
    }
    
    info.length = 1;
    return info;
}

bool Detour::isHotpatchable(void* address) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) {
        return false;
    }
    
    // Check if in executable section
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0;
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
        }
    }
    
    // Fallback: allocate anywhere
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}

void Detour::freeTrampoline(void* trampoline) {
    VirtualFree(trampoline, 0, MEM_RELEASE);
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
}

bool Detour::generateRelativeJump(void* from, void* to, uint8_t* buffer) {
    if (!isNearJumpPossible(from, to)) {
        return false;
    }
    
    int64_t offset = reinterpret_cast<int64_t>(to) - reinterpret_cast<int64_t>(from) - 5;
    
    buffer[0] = 0xE9; // jmp rel32
    *reinterpret_cast<int32_t*>(buffer + 1) = static_cast<int32_t>(offset);
    
    return true;
}

bool Detour::relocateInstruction(const uint8_t* original, uint8_t* relocated, 
                                 uintptr_t newBase, size_t* length) {
    // TODO: Implement instruction relocation
    return false;
}

bool Detour::isNearJumpPossible(void* from, void* to) {
    int64_t offset = reinterpret_cast<int64_t>(to) - reinterpret_cast<int64_t>(from) - 5;
    return (offset >= INT32_MIN && offset <= INT32_MAX);
}

void* Detour::findCodeCave(void* near, size_t size, size_t searchRadius) {
    // TODO: Implement code cave search
    return nullptr;
}

} // namespace RawrXD::Agentic::Hotpatch
