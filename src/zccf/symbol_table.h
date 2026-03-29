// ============================================================================
// symbol_table.h — ZCCF Symbol Table (Lock-Free Reads, 32-bit Handles)
// ============================================================================
// Provides a versioned, handle-based symbol table. Agents receive 32-bit
// symbol handles instead of string names. The LLM context payload carries
// handles; symbol resolution happens only when agent logic needs the full
// descriptor — zero serialisation in the hot path.
//
// Architecture:
//   ┌──────────────────────────────────────────────────────────┐
//   │                    SymbolTable                            │
//   │                                                           │
//   │  Slot array (flat, cache-friendly)                        │
//   │  ┌───┬───┬───┬───┬── ... ──────────────────────────────┐ │
//   │  │ 0 │ 1 │ 2 │ 3 │ ...   (SymbolEntry × kMaxSymbols)   │ │
//   │  └───┴───┴───┴───┴──────────────────────────────────────┘ │
//   │                                                           │
//   │  Hash index: name → slot (open-addressing, power-of-2)   │
//   │  Write epoch: atomic counter; readers take snapshot       │
//   └──────────────────────────────────────────────────────────┘
//
// Concurrency:
//   - Reads: lock-free via epoch-based snapshot. Safe to call on any thread.
//   - Writes: serialised by the caller (single writer expected per session).
//
// Handle encoding:
//   [ scope:8 | slot:16 | version:8 ]
//   version  — incremented when a slot is reused; stale handle → null resolve.
//
// No exceptions.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <expected>
#include <atomic>
#include <memory>

namespace RawrXD {
namespace ZCCF {

// ============================================================================
// Handle type
// ============================================================================

struct SymbolHandle {
    uint32_t raw = 0;

    constexpr uint8_t  scope()   const noexcept { return (raw >> 24) & 0xFF; }
    constexpr uint16_t slot()    const noexcept { return (raw >>  8) & 0xFFFF; }
    constexpr uint8_t  version() const noexcept { return  raw        & 0xFF; }

    static SymbolHandle Make(uint8_t scope, uint16_t slot, uint8_t ver) noexcept {
        return SymbolHandle{ (uint32_t(scope) << 24) |
                             (uint32_t(slot)  <<  8) |
                              uint32_t(ver) };
    }

    bool IsNull() const noexcept { return raw == 0; }
    static SymbolHandle Null() noexcept { return {}; }
};

// ============================================================================
// Symbol kinds
// ============================================================================

enum class SymbolKind : uint16_t {
    Unknown     = 0,
    Function    = 1,
    Variable    = 2,
    Type        = 3,
    Namespace   = 4,
    Field       = 5,
    Parameter   = 6,
    Label       = 7,
    Macro       = 8,
};

// ============================================================================
// SymbolEntry — fixed-size descriptor
// ============================================================================

struct SymbolEntry {
    SymbolHandle handle;
    SymbolKind   kind           = SymbolKind::Unknown;
    uint8_t      version        = 0;     // Must match handle.version to be valid
    bool         isDefinition   = false; // false = declaration only
    uint32_t     fileHandle     = 0;     // ZCCF FileCache handle
    uint32_t     astHandle      = 0;     // ZCCF AstArena handle (raw u32)
    uint32_t     lineBegin      = 0;
    uint32_t     lineEnd        = 0;
    char         name[128]      = {};    // Qualified name, null-terminated
    char         type[64]       = {};    // Type string, null-terminated
};

static_assert(sizeof(SymbolEntry) == 224, "SymbolEntry size changed");

// ============================================================================
// Error type
// ============================================================================

enum class SymbolError : uint32_t {
    None         = 0,
    NotFound     = 1,
    TableFull    = 2,
    StaleHandle  = 3,
    DuplicateName= 4,
};

template<typename T>
using SymbolResult = std::expected<T, SymbolError>;

// ============================================================================
// SymbolTable
// ============================================================================

class SymbolTable {
public:
    static constexpr uint32_t kMaxSymbols = 65536;

    SymbolTable();
    ~SymbolTable();

    SymbolTable(const SymbolTable&)            = delete;
    SymbolTable& operator=(const SymbolTable&) = delete;

    // -------------------------------------------------------------------------
    // Write (single-writer; call from index-rebuild thread only)
    // -------------------------------------------------------------------------

    /// Insert a new symbol. Returns a fresh handle.
    SymbolResult<SymbolHandle> Insert(SymbolEntry entry);

    /// Update an existing entry in-place (by handle).
    SymbolResult<void> Update(SymbolHandle h, const SymbolEntry& updated);

    /// Remove a symbol; slot version is incremented (old handles become stale).
    SymbolResult<void> Remove(SymbolHandle h);

    /// Clear all entries and reset the table.
    void Clear();

    // -------------------------------------------------------------------------
    // Read (lock-free, any thread)
    // -------------------------------------------------------------------------

    /// Resolve a handle. Returns nullptr on stale/null handle.
    const SymbolEntry* Resolve(SymbolHandle h) const noexcept;

    /// Lookup by qualified name. Returns null handle if not found.
    SymbolHandle FindByName(std::string_view qualifiedName) const noexcept;

    // -------------------------------------------------------------------------
    // Snapshot — for agent payload construction
    // -------------------------------------------------------------------------

    /// Advance the write epoch, making the current state visible to readers.
    void PublishEpoch() noexcept;

    uint64_t CurrentEpoch() const noexcept;
    uint32_t SymbolCount()  const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ZCCF
} // namespace RawrXD
