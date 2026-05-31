#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>

namespace RawrXD::Agentic::Hotpatch {

/// x64 instruction info for detour calculation
struct InstructionInfo {
    uint8_t length = 0;
    bool isRelative = false;
    int32_t relativeOffset = 0;
    uintptr_t absoluteTarget = 0;
};

/// Detour trampoline (original code + jmp back)
struct Trampoline {
    void* address = nullptr;
    size_t size = 0;
    std::vector<uint8_t> originalBytes;
    bool isActive = false;
};

/// x64 detour implementation (minimal inline hooking)
class Detour {
public:
    /// Install detour: replace target function with jump to replacement
    /// Returns trampoline address (original function relocated)
    static void* install(void* targetFunc, void* replacementFunc, size_t* trampolineSize = nullptr);
    
    /// Uninstall detour: restore original bytes
    static bool uninstall(void* targetFunc, void* trampoline);
    
    /// Calculate minimum bytes needed for detour (at least 14 bytes for x64 absolute jmp)
    static size_t calculateRequiredBytes(void* targetFunc);
    
    /// Disassemble instruction at address (returns length)
    static InstructionInfo disassembleOne(const uint8_t* code);
    
    /// Check if address is within hotpatchable region (.text section)
    static bool isHotpatchable(void* address);
    
    /// Allocate trampoline near target (for relative jumps)
    static void* allocateTrampolineNear(void* targetFunc, size_t size);
    
    /// Free trampoline
    static void freeTrampoline(void* trampoline);
    
    /// Generate absolute jump (14 bytes: mov rax, imm64; jmp rax)
    static void generateAbsoluteJump(void* from, void* to, uint8_t* buffer);
    
    /// Generate relative jump (5 bytes: jmp rel32)
    static bool generateRelativeJump(void* from, void* to, uint8_t* buffer);
    
    /// Relocate instruction with updated addresses
    static bool relocateInstruction(const uint8_t* original, uint8_t* relocated, uintptr_t newBase, size_t* length);
    
private:
    /// Check if two addresses are within ±2GB (for rel32)
    static bool isNearJumpPossible(void* from, void* to);
    
    /// Find code cave near target
    static void* findCodeCave(void* near, size_t size, size_t searchRadius = 0x7FF00000);
};

} // namespace RawrXD::Agentic::Hotpatch
