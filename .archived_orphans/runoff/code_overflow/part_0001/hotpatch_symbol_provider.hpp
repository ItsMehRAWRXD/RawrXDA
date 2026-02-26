// ============================================================================
// hotpatch_symbol_provider.hpp — Hotpatch Symbol Table Export for LSP
// ============================================================================
// Extracts symbols from all three hotpatch layers (Memory, Byte, Server)
// plus GGUF model metadata and presents them as navigable LSP symbols.
//
// Used by the LSP bridge to serve:
//   textDocument/documentSymbol
//   workspace/symbol
//   textDocument/definition
//   textDocument/hover
//   textDocument/references
//
// Thread model: All access through mutex-protected public API.
// Memory: Symbols are stored in a flat vector; hashes in a parallel array
//         for ASM-accelerated binary search (see RawrXD_LSP_SymbolIndex.asm).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: PatchResult-style returns (no exceptions)
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
#pragma once

#include "lsp_bridge_protocol.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

// Forward declarations
struct MemoryPatchEntry;
struct BytePatch;
struct ServerHotpatch;
struct PatchResult;
class  UnifiedHotpatchManager;

// ---------------------------------------------------------------------------
// External ASM entry points (RawrXD_LSP_SymbolIndex.asm)
// ---------------------------------------------------------------------------
extern "C" {
    // Binary search for a hash in a sorted uint64_t array.
    // Returns index (0-based) or -1 if not found.
    int64_t asm_symbol_hash_lookup(const uint64_t* hashArray, int64_t count,
                                    uint64_t targetHash);

    // Batch FNV-1a hash computation for an array of C strings.
    // Writes results into outHashes. Returns number of hashes computed.
    int64_t asm_batch_fnv1a(const char* const* strings, int64_t count,
                             uint64_t* outHashes);

    // SIMD-accelerated prefix match scan across symbol name pointers.
    // Returns index of first match or -1.
    int64_t asm_symbol_prefix_scan(const char* const* names, int64_t count,
                                    const char* prefix, int64_t prefixLen);
}

// ---------------------------------------------------------------------------
// SymbolIndex — Sorted parallel arrays for ASM-accelerated lookup
// ---------------------------------------------------------------------------
struct SymbolIndex {
    std::vector<uint64_t>           hashes;     // Sorted FNV-1a hashes
    std::vector<uint32_t>           indices;    // Map hash position → symbol vector index
    std::atomic<uint64_t>           generation{0}; // Incremented on rebuild
};

// ---------------------------------------------------------------------------
// HotpatchSymbolProvider — Extracts and indexes symbols for LSP
// ---------------------------------------------------------------------------
class HotpatchSymbolProvider {
public:
    static HotpatchSymbolProvider& instance();

    // ---- Full Rebuild ----
    // Scan all hotpatch layers and GGUF metadata, rebuild the symbol table.
    PatchResult rebuildIndex();

    // ---- Incremental Updates ----
    PatchResult addMemoryPatchSymbol(const MemoryPatchEntry* entry,
                                     const char* name, const char* description);
    PatchResult addBytePatchSymbol(const BytePatch* patch,
                                   const char* filename, const char* name);
    PatchResult addServerPatchSymbol(const ServerHotpatch* patch);
    PatchResult addGGUFTensorSymbol(const char* modelPath, const char* tensorName,
                                    uint32_t type, uint64_t offset, uint64_t size,
                                    const uint32_t dims[4], uint32_t nDims);
    PatchResult addGGUFMetadataSymbol(const char* modelPath, const char* key,
                                      const char* value);
    PatchResult removeSymbol(const char* name);

    // ---- Queries ----

    // Get all symbols (thread-safe snapshot)
    std::vector<RawrXD::LSPBridge::HotpatchSymbolEntry> getAllSymbols() const;

    // Get symbols filtered by layer
    std::vector<RawrXD::LSPBridge::HotpatchSymbolEntry> getSymbolsByLayer(
        RawrXD::LSPBridge::HotpatchLayer layer) const;

    // Get symbols filtered by kind
    std::vector<RawrXD::LSPBridge::HotpatchSymbolEntry> getSymbolsByKind(
        RawrXD::LSPBridge::HotpatchSymbolKind kind) const;

    // Find symbol by exact name (uses ASM hash lookup)
    const RawrXD::LSPBridge::HotpatchSymbolEntry* findByName(const char* name) const;

    // Find symbols matching a prefix (for completion)
    std::vector<RawrXD::LSPBridge::HotpatchSymbolEntry> findByPrefix(
        const char* prefix) const;

    // Find symbol at a file position (for hover/definition)
    const RawrXD::LSPBridge::HotpatchSymbolEntry* findAtPosition(
        const char* filePath, uint32_t line, uint32_t character) const;

    // Get symbols for a specific file (for documentSymbol)
    std::vector<RawrXD::LSPBridge::HotpatchSymbolEntry> getFileSymbols(
        const char* filePath) const;

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> totalSymbols{0};
        std::atomic<uint64_t> memoryPatchSymbols{0};
        std::atomic<uint64_t> bytePatchSymbols{0};
        std::atomic<uint64_t> serverPatchSymbols{0};
        std::atomic<uint64_t> ggufTensorSymbols{0};
        std::atomic<uint64_t> ggufMetadataSymbols{0};
        std::atomic<uint64_t> lookupCount{0};
        std::atomic<uint64_t> lookupHits{0};
        std::atomic<uint64_t> rebuildCount{0};
        double                lastRebuildMs{0.0};
    };

    const Stats& getStats() const { return m_stats; }

    // ---- Index Generation ----
    uint64_t getGeneration() const { return m_index.generation.load(); }

private:
    HotpatchSymbolProvider();
    ~HotpatchSymbolProvider();
    HotpatchSymbolProvider(const HotpatchSymbolProvider&) = delete;
    HotpatchSymbolProvider& operator=(const HotpatchSymbolProvider&) = delete;

    // Internal helpers
    uint64_t computeHash(const char* name) const;
    void rebuildSortedIndex();
    void scanMemoryLayer();
    void scanByteLayer();
    void scanServerLayer();
    void scanGGUFModels();

    mutable std::mutex                                      m_mutex;
    std::vector<RawrXD::LSPBridge::HotpatchSymbolEntry>     m_symbols;
    std::vector<std::string>                                m_ownedStrings; // Storage for symbol strings
    SymbolIndex                                             m_index;
    mutable Stats                                           m_stats;
};
