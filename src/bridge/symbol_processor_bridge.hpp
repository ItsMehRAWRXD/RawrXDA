// symbol_processor_bridge.hpp — C++ <-> MASM binary bridge for AST hot-path
// ============================================================================
// This header defines the POD layout that MASM expects. If you change any
// field order, size, or alignment, the assembly will silently corrupt memory.
// The static_assert at the bottom is your safety net.
// ============================================================================
#pragma once

#include <cstddef>
#include <cstdint>

namespace rawrxd {
namespace asm_bridge {

// POD symbol — exactly 48 bytes, 8-byte aligned.
// MASM sees this as a flat memory layout; no vtables, no SSO, no exceptions.
struct alignas(8) PODSymbol {
    const char* name;       // Offset 0   (QWORD)
    const char* kind;       // Offset 8   (QWORD)
    std::size_t line;       // Offset 16  (QWORD)
    std::size_t column;     // Offset 24  (QWORD)
    std::uint8_t is_public; // Offset 32  (BYTE)
    std::uint8_t pad0[7];   // Offset 33  (BYTE[7])  alignment
    std::uint32_t node_type;// Offset 40  (DWORD)
    std::uint8_t pad1[4];   // Offset 44  (BYTE[4])  alignment
};

static_assert(sizeof(PODSymbol) == 48, "ASM/C++ struct alignment mismatch!");
static_assert(offsetof(PODSymbol, name) == 0, "name offset mismatch");
static_assert(offsetof(PODSymbol, kind) == 8, "kind offset mismatch");
static_assert(offsetof(PODSymbol, line) == 16, "line offset mismatch");
static_assert(offsetof(PODSymbol, column) == 24, "column offset mismatch");
static_assert(offsetof(PODSymbol, is_public) == 32, "is_public offset mismatch");
static_assert(offsetof(PODSymbol, node_type) == 40, "node_type offset mismatch");

} // namespace asm_bridge
} // namespace rawrxd

// ============================================================================
// C ABI exports from SymbolProcessor.asm
// ============================================================================
extern "C" {

// FilterPublicSymbols(symbolBuffer, count) -> publicCount
// Scans an array of PODSymbol and returns how many have is_public == 1.
std::size_t FilterPublicSymbols(const void* symbolBuffer, std::size_t count);

// FindSymbolByName(symbolBuffer, count, nameToFind) -> index or -1
// Returns the index of the first symbol whose name matches (case-sensitive).
std::size_t FindSymbolByName(const void* symbolBuffer, std::size_t count, const char* nameToFind);

// CountSymbolsByKind(symbolBuffer, count, kindToFind) -> matchCount
// Returns the number of symbols whose kind string matches (case-sensitive).
std::size_t CountSymbolsByKind(const void* symbolBuffer, std::size_t count, const char* kindToFind);

} // extern "C"
