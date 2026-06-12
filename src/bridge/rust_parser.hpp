// rust_parser.hpp — Stub for build compatibility
#pragma once
#include <string>
#include <vector>

namespace rawrxd {
namespace ast {

struct Position {
    size_t line = 0;
    size_t column = 0;
};

struct Range {
    Position start;
    Position end;
};

struct Meta {
    std::string visibility = "pub";
    std::string doc;
};

enum class NodeType {
    FunctionDecl,
    VariableDecl,
    ClassDecl,
    StructDecl,
    EnumDecl,
    NamespaceDecl,
    Other
};

struct Symbol {
    std::string name;
    std::string kind;
    std::string file;
    size_t line = 0;
    size_t column = 0;
    bool is_public = true;
    Range range;
    Meta meta;
    NodeType type = NodeType::Other;
};

class SymbolTable {
public:
    std::vector<Symbol> symbols;
    void add(const Symbol& s) { symbols.push_back(s); }
    const std::vector<Symbol>& all() const { return symbols; }
};

namespace rust {
    class RustParser {
    public:
        struct Result {
            bool success = false;
        };
        Result parse(const std::string&, const std::string&, SymbolTable*) {
            return Result{false};
        }
    };
}

} // namespace ast

namespace parser {
    using ast::Symbol;
    struct ParseResult {
        std::vector<Symbol> symbols;
        bool success = false;
    };
    inline ParseResult parseRust(const std::string& /*source*/) {
        return ParseResult{};
    }
} // namespace parser
} // namespace rawrxd

// Compatibility alias for legacy code
namespace RawrXD { namespace AST {
#ifndef RAWRXD_AST_NODETYPE_DEFINED
#define RAWRXD_AST_NODETYPE_DEFINED
    using NodeType = rawrxd::ast::NodeType;
#endif
}}

// ============================================================================
// MASM Binary Bridge — POD Symbol Layout for SymbolProcessor.asm
// ============================================================================
// The assembly scanner expects a contiguous array of 48-byte POD symbols.
// This struct MUST stay in sync with SymbolProcessor.asm offsets.
// ============================================================================

#include <cstddef>
#include <cstdint>

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

extern "C" {
    // Returns count of public symbols
    size_t FilterPublicSymbols(const void* symbolBuffer, size_t count);
    
    // Returns index or -1
    int64_t FindSymbolByName(const void* symbolBuffer, size_t count, const char* nameToFind);
    
    // Returns match count
    size_t CountSymbolsByKind(const void* symbolBuffer, size_t count, const char* kindToFind);
}
