#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
    // Returns count of public symbols
    size_t FilterPublicSymbols(const void* symbolBuffer, size_t count);
    
    // Returns index or -1
    int64_t FindSymbolByName(const void* symbolBuffer, size_t count, const char* nameToFind);
    
    // Returns match count
    size_t CountSymbolsByKind(const void* symbolBuffer, size_t count, const char* kindToFind);
}

// POD Symbol struct — must stay in sync with SymbolProcessor.asm layout
struct alignas(8) POD_Symbol {
    const char* pName;       // 8 bytes  (offset 0)
    const char* pKind;       // 8 bytes  (offset 8)
    size_t      line;        // 8 bytes  (offset 16)
    size_t      column;      // 8 bytes  (offset 24)
    uint8_t     is_public;   // 1 byte   (offset 32)
    uint8_t     pad0[7];     // 7 bytes  (offset 33)
    uint32_t    node_type;   // 4 bytes  (offset 40)
    uint8_t     pad1[4];     // 4 bytes  (offset 44)
};
static_assert(sizeof(POD_Symbol) == 48, "Symbol struct size mismatch between ASM and C++!");
static_assert(offsetof(POD_Symbol, pName)     == 0,  "pName offset mismatch");
static_assert(offsetof(POD_Symbol, pKind)     == 8,  "pKind offset mismatch");
static_assert(offsetof(POD_Symbol, line)      == 16, "line offset mismatch");
static_assert(offsetof(POD_Symbol, column)    == 24, "column offset mismatch");
static_assert(offsetof(POD_Symbol, is_public) == 32, "is_public offset mismatch");
static_assert(offsetof(POD_Symbol, node_type) == 40, "node_type offset mismatch");
