// ============================================================================
// ast_arena.h — ZCCF AST Arena (Bump Allocator with Handle Export)
// ============================================================================
// Provides a generation-tracked bump allocator for AST nodes. Agents receive
// 32-bit handles instead of raw pointers — the LLM context plane carries
// handles, and resolves content on demand rather than serialising whole trees.
//
// Memory layout:
//   ┌────────────────────────────────────────────────────────┐
//   │                     AstArena                            │
//   │                                                         │
//   │  Chunk 0 [64MB]  Chunk 1 [64MB]  ...                   │
//   │  ┌──────────┐    ┌──────────┐                           │
//   │  │ AstNode  │    │ AstNode  │  ← bump ptr moves right   │
//   │  │ AstNode  │    │  ...     │                           │
//   │  └──────────┘    └──────────┘                           │
//   │                                                         │
//   │  Handle = [ chunk_id:8 | slot:16 | generation:8 ]      │
//   └────────────────────────────────────────────────────────┘
//
// Lifetime:
//   - Generation counter increments on each Reset().
//   - Stale handle detection: handle.generation != arena.currentGeneration.
//   - Agent request pinning: Pin() prevents Reset() from invalidating handles
//     while the agent round-trip is in-flight.
//
// Thread Safety:
//   Alloc is serialised per-chunk with an atomic bump pointer.
//   Reads are lock-free (generation check is atomic).
//
// No VirtualAlloc large pages in P0 — standard VirtualAlloc committed pages.
// Large-page path is a P1 MASM hotpath upgrade (flagged below).
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <expected>
#include <atomic>

namespace RawrXD {
namespace ZCCF {

// ============================================================================
// Handle type
// ============================================================================

struct AstHandle {
    uint32_t raw = 0;

    constexpr uint8_t  chunkId()    const noexcept { return (raw >> 24) & 0xFF; }
    constexpr uint16_t slot()       const noexcept { return (raw >>  8) & 0xFFFF; }
    constexpr uint8_t  generation() const noexcept { return  raw        & 0xFF; }

    static AstHandle Make(uint8_t chunk, uint16_t slot, uint8_t gen) noexcept {
        return AstHandle{ (uint32_t(chunk) << 24) |
                          (uint32_t(slot)  <<  8) |
                           uint32_t(gen) };
    }

    bool IsNull() const noexcept { return raw == 0; }
    static AstHandle Null() noexcept { return {}; }
};

// ============================================================================
// AST node kinds
// ============================================================================

enum class AstNodeKind : uint16_t {
    Unknown       = 0,
    TranslationUnit,
    FunctionDecl,
    VarDecl,
    TypeDecl,
    ParmDecl,
    CompoundStmt,
    CallExpr,
    BinaryExpr,
    UnaryExpr,
    DeclRefExpr,
    Literal,
    Namespace,
    ClassDecl,
    FieldDecl,
    // Extend as needed — existing values must never change
};

// ============================================================================
// AST node — fixed-size for arena slot packing
// ============================================================================

struct AstNode {
    AstNodeKind  kind         = AstNodeKind::Unknown;
    uint16_t     childCount   = 0;
    uint32_t     symbolHandle = 0;    // ZCCF SymbolTable handle (0 = none)
    uint32_t     fileHandle   = 0;    // ZCCF FileCache handle (0 = none)
    uint32_t     lineBegin    = 0;
    uint32_t     lineEnd      = 0;
    uint32_t     colBegin     = 0;
    uint32_t     colEnd       = 0;
    AstHandle    firstChild;
    AstHandle    nextSibling;
    // Name stored in a small inline buffer; longer names truncated to 63 chars
    char         name[64]     = {};
};

static_assert(sizeof(AstNode) == 104, "AstNode size changed — update slot packing");

// ============================================================================
// Arena error type
// ============================================================================

enum class ArenaError : uint32_t {
    None         = 0,
    OutOfMemory  = 1,
    StaleHandle  = 2,
    PinDepthExceeded = 3,
};

template<typename T>
using ArenaResult = std::expected<T, ArenaError>;

// ============================================================================
// AstArena
// ============================================================================

class AstArena {
public:
    static constexpr size_t kChunkBytes   = 64 * 1024 * 1024;  // 64 MB
    static constexpr size_t kMaxChunks    = 8;
    static constexpr size_t kSlotsPerChunk = kChunkBytes / sizeof(AstNode);
    static constexpr uint8_t kMaxPinDepth = 16;

    AstArena();
    ~AstArena();

    // Not copyable
    AstArena(const AstArena&)            = delete;
    AstArena& operator=(const AstArena&) = delete;

    // -------------------------------------------------------------------------
    // Allocation
    // -------------------------------------------------------------------------

    /// Allocate one AstNode, fill from node, return its handle.
    ArenaResult<AstHandle> Alloc(const AstNode& node);

    // -------------------------------------------------------------------------
    // Resolution
    // -------------------------------------------------------------------------

    /// Resolve a handle to a pointer. Returns nullptr on stale handle.
    const AstNode* Resolve(AstHandle h) const noexcept;
    AstNode*       ResolveMut(AstHandle h) noexcept;

    // -------------------------------------------------------------------------
    // Generation / Reset
    // -------------------------------------------------------------------------

    /// Increment generation and reset all chunks. Invalidates all handles.
    /// Returns false and does nothing if any pin is active.
    bool Reset();

    uint8_t CurrentGeneration() const noexcept;
    size_t  NodeCount() const noexcept;

    // -------------------------------------------------------------------------
    // Pinning — prevents Reset() from invalidating handles mid-request
    // -------------------------------------------------------------------------

    ArenaResult<void> Pin();
    void              Unpin() noexcept;
    bool              IsPinned() const noexcept;

private:
    struct Chunk {
        void*                 memory    = nullptr;
        std::atomic<uint32_t> nextSlot  = 0;
    };

    Chunk                 m_chunks[kMaxChunks] = {};
    uint8_t               m_chunkCount   = 0;
    std::atomic<uint8_t>  m_generation   = { 1 };
    std::atomic<uint8_t>  m_pinDepth     = { 0 };

    bool AllocChunk();
};

} // namespace ZCCF
} // namespace RawrXD
