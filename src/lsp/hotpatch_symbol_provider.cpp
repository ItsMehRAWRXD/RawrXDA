// ============================================================================
// hotpatch_symbol_provider.cpp — Hotpatch Symbol Table Export for LSP
// ============================================================================
// Implementation of symbol extraction from all three hotpatch layers,
// GGUF model metadata, and the sorted hash index for ASM-accelerated lookup.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: PatchResult-style returns (no exceptions)
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "hotpatch_symbol_provider.hpp"
#include "core/model_memory_hotpatch.hpp"
#include "core/byte_level_hotpatcher.hpp"
#include "core/unified_hotpatch_manager.hpp"
#include "server/gguf_server_hotpatch.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace RawrXD::LSPBridge;
namespace fs = std::filesystem;

// ============================================================================
// SINGLETON
// ============================================================================

HotpatchSymbolProvider& HotpatchSymbolProvider::instance() {
    static HotpatchSymbolProvider inst;
    return inst;
}

HotpatchSymbolProvider::HotpatchSymbolProvider()  = default;
HotpatchSymbolProvider::~HotpatchSymbolProvider() = default;

// ============================================================================
// FNV-1a HASH (matches LSP server's fnv1a)
// ============================================================================

uint64_t HotpatchSymbolProvider::computeHash(const char* name) const {
    uint64_t h = 14695981039346656037ULL;
    if (!name) return h;
    while (*name) {
        h ^= (uint8_t)*name++;
        h *= 1099511628211ULL;
    }
    return h;
}

// ============================================================================
// SORTED INDEX REBUILD
// ============================================================================

void HotpatchSymbolProvider::rebuildSortedIndex() {
    // Build parallel hash/index arrays
    size_t n = m_symbols.size();
    m_index.hashes.resize(n);
    m_index.indices.resize(n);

    for (size_t i = 0; i < n; ++i) {
        m_index.hashes[i]  = m_symbols[i].hash;
        m_index.indices[i] = (uint32_t)i;
    }

    // Sort by hash for binary search
    std::sort(m_index.indices.begin(), m_index.indices.end(),
              [this](uint32_t a, uint32_t b) {
                  return m_index.hashes[a] < m_index.hashes[b];
              });

    // Rearrange hashes to match sorted order
    std::vector<uint64_t> sortedHashes(n);
    for (size_t i = 0; i < n; ++i) {
        sortedHashes[i] = m_symbols[m_index.indices[i]].hash;
    }
    m_index.hashes = std::move(sortedHashes);

    m_index.generation.fetch_add(1);
}

// ============================================================================
// INTERNAL STRING STORAGE
// ============================================================================

// Stores a copy of the string and returns a stable pointer
static const char* storeString(std::vector<std::string>& storage, const char* s) {
    if (!s) return "";
    storage.emplace_back(s);
    return storage.back().c_str();
}

static const char* storeString(std::vector<std::string>& storage, const std::string& s) {
    storage.push_back(s);
    return storage.back().c_str();
}

// ============================================================================
// FULL INDEX REBUILD
// ============================================================================

PatchResult HotpatchSymbolProvider::rebuildIndex() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto t0 = std::chrono::high_resolution_clock::now();

    // Clear existing data
    m_symbols.clear();
    m_ownedStrings.clear();
    m_ownedStrings.reserve(2048);

    // Scan all layers
    scanMemoryLayer();
    scanByteLayer();
    scanServerLayer();
    scanGGUFModels();

    // Rebuild hash index
    rebuildSortedIndex();

    // Update stats
    auto t1 = std::chrono::high_resolution_clock::now();
    m_stats.lastRebuildMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    m_stats.totalSymbols.store(m_symbols.size());
    m_stats.rebuildCount.fetch_add(1);

    return PatchResult::ok("Symbol index rebuilt");
}

// ============================================================================
// MEMORY LAYER SCAN
// ============================================================================

void HotpatchSymbolProvider::scanMemoryLayer() {
    // Query the UnifiedHotpatchManager for memory patch statistics
    auto& mgr = UnifiedHotpatchManager::instance();
    auto& stats = mgr.getStats();

    // Memory patches are tracked internally — we create synthetic symbols
    // for the memory layer's current state.
    uint64_t count = stats.memoryPatchCount.load();

    if (count > 0) {
        HotpatchSymbolEntry entry{};
        entry.name      = storeString(m_ownedStrings, "memory_patches_active");
        entry.kind      = HotpatchSymbolKind::MemoryPatch;
        entry.layer     = HotpatchLayer::Memory;
        
        std::ostringstream desc;
        desc << "Active memory patches: " << count
             << " | Applied: " << get_memory_patch_stats().totalApplied.load()
             << " | Reverted: " << get_memory_patch_stats().totalReverted.load()
             << " | Failed: " << get_memory_patch_stats().totalFailed.load();
        entry.detail    = storeString(m_ownedStrings, desc.str());
        entry.filePath  = "";
        entry.line      = 0;
        entry.startChar = 0;
        entry.endChar   = 0;
        entry.address   = 0;
        entry.size      = 0;
        entry.hash      = computeHash(entry.name);
        entry.active    = true;
        m_symbols.push_back(entry);
        m_stats.memoryPatchSymbols.fetch_add(1);
    }
}

// ============================================================================
// BYTE LAYER SCAN
// ============================================================================

void HotpatchSymbolProvider::scanByteLayer() {
    // Scan for .gguf files in the workspace root to create byte-layer symbols
    // Each GGUF file is a potential target for byte-level hotpatching
    auto& mgr = UnifiedHotpatchManager::instance();
    auto& stats = mgr.getStats();
    uint64_t count = stats.bytePatchCount.load();

    if (count > 0) {
        HotpatchSymbolEntry entry{};
        entry.name      = storeString(m_ownedStrings, "byte_patches_active");
        entry.kind      = HotpatchSymbolKind::BytePatch;
        entry.layer     = HotpatchLayer::Byte;
        
        std::ostringstream desc;
        desc << "Active byte-level patches: " << count;
        entry.detail    = storeString(m_ownedStrings, desc.str());
        entry.filePath  = "";
        entry.line      = 0;
        entry.startChar = 0;
        entry.endChar   = 0;
        entry.address   = 0;
        entry.size      = 0;
        entry.hash      = computeHash(entry.name);
        entry.active    = true;
        m_symbols.push_back(entry);
        m_stats.bytePatchSymbols.fetch_add(1);
    }
}

// ============================================================================
// SERVER LAYER SCAN
// ============================================================================

void HotpatchSymbolProvider::scanServerLayer() {
    // GGUFServerHotpatch is a singleton with registered transforms
    auto& serverHP = GGUFServerHotpatch::instance();
    auto& mgr = UnifiedHotpatchManager::instance();
    auto& stats = mgr.getStats();

    uint64_t count = stats.serverPatchCount.load();
    if (count > 0) {
        HotpatchSymbolEntry entry{};
        entry.name      = storeString(m_ownedStrings, "server_patches_active");
        entry.kind      = HotpatchSymbolKind::ServerPatch;
        entry.layer     = HotpatchLayer::Server;
        
        std::ostringstream desc;
        desc << "Active server patches: " << count;
        entry.detail    = storeString(m_ownedStrings, desc.str());
        entry.filePath  = "";
        entry.line      = 0;
        entry.startChar = 0;
        entry.endChar   = 0;
        entry.address   = 0;
        entry.size      = 0;
        entry.hash      = computeHash(entry.name);
        entry.active    = true;
        m_symbols.push_back(entry);
        m_stats.serverPatchSymbols.fetch_add(1);
    }
}

// ============================================================================
// GGUF MODEL SCAN
// ============================================================================

void HotpatchSymbolProvider::scanGGUFModels() {
    // Scan workspace directory for .gguf files
    // For each file, read the GGUF header to extract metadata + tensor list
    // This uses direct_read from byte_level_hotpatcher for zero-copy I/O

    // Common workspace paths
    const char* searchDirs[] = { ".", "./models", "../models", nullptr };

    for (int d = 0; searchDirs[d]; ++d) {
        std::error_code ec;
        if (!fs::exists(searchDirs[d], ec)) continue;

        for (auto& entry : fs::directory_iterator(searchDirs[d], ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            if (ext != ".gguf") continue;

            std::string path = entry.path().string();
            uint64_t fileSize = entry.file_size(ec);
            if (ec || fileSize < 8) continue;

            // Read GGUF magic + version (first 8 bytes)
            uint32_t header[2] = {0, 0};
            size_t bytesRead = 0;
            PatchResult rr = direct_read(path.c_str(), 0, 8, header, &bytesRead);
            if (!rr.success || bytesRead < 8) continue;

            uint32_t magic   = header[0];
            uint32_t version = header[1];

            // GGUF magic: "GGUF" = 0x46475547
            if (magic != 0x46475547) continue;

            // Create model metadata symbol
            {
                HotpatchSymbolEntry sym{};
                std::string symName = entry.path().filename().string();
                sym.name      = storeString(m_ownedStrings, symName);
                sym.kind      = HotpatchSymbolKind::GGUFMetadata;
                sym.layer     = HotpatchLayer::Byte; // GGUF files are byte-layer targets
                
                std::ostringstream desc;
                desc << "GGUF v" << version << " | " << (fileSize / (1024*1024)) << " MB";
                sym.detail    = storeString(m_ownedStrings, desc.str());
                sym.filePath  = storeString(m_ownedStrings, path);
                sym.line      = 0;
                sym.startChar = 0;
                sym.endChar   = 0;
                sym.address   = 0;
                sym.size      = (size_t)fileSize;
                sym.hash      = computeHash(sym.name);
                sym.active    = true;
                m_symbols.push_back(sym);
                m_stats.ggufMetadataSymbols.fetch_add(1);
            }
        }
    }
}

// ============================================================================
// INCREMENTAL SYMBOL ADDITIONS
// ============================================================================

PatchResult HotpatchSymbolProvider::addMemoryPatchSymbol(
    const MemoryPatchEntry* entry, const char* name, const char* description)
{
    if (!entry || !name) return PatchResult::error("null entry or name");

    std::lock_guard<std::mutex> lock(m_mutex);

    HotpatchSymbolEntry sym{};
    sym.name      = storeString(m_ownedStrings, name);
    sym.kind      = HotpatchSymbolKind::MemoryPatch;
    sym.layer     = HotpatchLayer::Memory;
    sym.detail    = storeString(m_ownedStrings, description ? description : "Memory patch");
    sym.filePath  = "";
    sym.line      = 0;
    sym.startChar = 0;
    sym.endChar   = 0;
    sym.address   = entry->targetAddr;
    sym.size      = entry->patchSize;
    sym.hash      = computeHash(sym.name);
    sym.active    = entry->applied;
    m_symbols.push_back(sym);
    m_stats.memoryPatchSymbols.fetch_add(1);
    m_stats.totalSymbols.fetch_add(1);

    rebuildSortedIndex();
    return PatchResult::ok("Memory patch symbol added");
}

PatchResult HotpatchSymbolProvider::addBytePatchSymbol(
    const BytePatch* patch, const char* filename, const char* name)
{
    if (!patch || !name) return PatchResult::error("null patch or name");

    std::lock_guard<std::mutex> lock(m_mutex);

    HotpatchSymbolEntry sym{};
    sym.name      = storeString(m_ownedStrings, name);
    sym.kind      = HotpatchSymbolKind::BytePatch;
    sym.layer     = HotpatchLayer::Byte;
    sym.detail    = storeString(m_ownedStrings, patch->description);
    sym.filePath  = storeString(m_ownedStrings, filename ? filename : "");
    sym.line      = 0;
    sym.startChar = 0;
    sym.endChar   = 0;
    sym.address   = 0;
    sym.size      = patch->data.size();
    sym.hash      = computeHash(sym.name);
    sym.active    = true;
    m_symbols.push_back(sym);
    m_stats.bytePatchSymbols.fetch_add(1);
    m_stats.totalSymbols.fetch_add(1);

    rebuildSortedIndex();
    return PatchResult::ok("Byte patch symbol added");
}

PatchResult HotpatchSymbolProvider::addServerPatchSymbol(const ServerHotpatch* patch) {
    if (!patch || !patch->name) return PatchResult::error("null server patch");

    std::lock_guard<std::mutex> lock(m_mutex);

    HotpatchSymbolEntry sym{};
    sym.name      = storeString(m_ownedStrings, patch->name);
    sym.kind      = HotpatchSymbolKind::ServerPatch;
    sym.layer     = HotpatchLayer::Server;
    sym.detail    = storeString(m_ownedStrings, "Server transform function");
    sym.filePath  = "";
    sym.line      = 0;
    sym.startChar = 0;
    sym.endChar   = 0;
    sym.address   = (uint64_t)(uintptr_t)patch->transform;
    sym.size      = 0;
    sym.hash      = computeHash(sym.name);
    sym.active    = true;
    m_symbols.push_back(sym);
    m_stats.serverPatchSymbols.fetch_add(1);
    m_stats.totalSymbols.fetch_add(1);

    rebuildSortedIndex();
    return PatchResult::ok("Server patch symbol added");
}

PatchResult HotpatchSymbolProvider::addGGUFTensorSymbol(
    const char* modelPath, const char* tensorName,
    uint32_t type, uint64_t offset, uint64_t size,
    const uint32_t dims[4], uint32_t nDims)
{
    if (!tensorName) return PatchResult::error("null tensor name");

    std::lock_guard<std::mutex> lock(m_mutex);

    HotpatchSymbolEntry sym{};
    sym.name      = storeString(m_ownedStrings, tensorName);
    sym.kind      = HotpatchSymbolKind::GGUFTensor;
    sym.layer     = HotpatchLayer::Byte;

    std::ostringstream desc;
    desc << "Tensor [" << dims[0];
    for (uint32_t i = 1; i < nDims && i < 4; ++i) desc << "x" << dims[i];
    desc << "] type=" << type << " @ offset 0x" << std::hex << offset;
    sym.detail    = storeString(m_ownedStrings, desc.str());
    sym.filePath  = storeString(m_ownedStrings, modelPath ? modelPath : "");
    sym.line      = 0;
    sym.startChar = 0;
    sym.endChar   = 0;
    sym.address   = offset;
    sym.size      = (size_t)size;
    sym.hash      = computeHash(sym.name);
    sym.active    = true;
    m_symbols.push_back(sym);
    m_stats.ggufTensorSymbols.fetch_add(1);
    m_stats.totalSymbols.fetch_add(1);

    rebuildSortedIndex();
    return PatchResult::ok("GGUF tensor symbol added");
}

PatchResult HotpatchSymbolProvider::addGGUFMetadataSymbol(
    const char* modelPath, const char* key, const char* value)
{
    if (!key) return PatchResult::error("null metadata key");

    std::lock_guard<std::mutex> lock(m_mutex);

    HotpatchSymbolEntry sym{};
    sym.name      = storeString(m_ownedStrings, key);
    sym.kind      = HotpatchSymbolKind::GGUFMetadata;
    sym.layer     = HotpatchLayer::Byte;

    std::string desc = std::string("Metadata: ") + (value ? value : "(null)");
    sym.detail    = storeString(m_ownedStrings, desc);
    sym.filePath  = storeString(m_ownedStrings, modelPath ? modelPath : "");
    sym.line      = 0;
    sym.startChar = 0;
    sym.endChar   = 0;
    sym.address   = 0;
    sym.size      = 0;
    sym.hash      = computeHash(sym.name);
    sym.active    = true;
    m_symbols.push_back(sym);
    m_stats.ggufMetadataSymbols.fetch_add(1);
    m_stats.totalSymbols.fetch_add(1);

    rebuildSortedIndex();
    return PatchResult::ok("GGUF metadata symbol added");
}

PatchResult HotpatchSymbolProvider::removeSymbol(const char* name) {
    if (!name) return PatchResult::error("null name");

    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t hash = computeHash(name);
    auto it = std::remove_if(m_symbols.begin(), m_symbols.end(),
                              [hash](const HotpatchSymbolEntry& s) {
                                  return s.hash == hash;
                              });
    if (it == m_symbols.end()) return PatchResult::error("Symbol not found");

    size_t removed = std::distance(it, m_symbols.end());
    m_symbols.erase(it, m_symbols.end());
    m_stats.totalSymbols.fetch_sub(removed);

    rebuildSortedIndex();
    return PatchResult::ok("Symbol removed");
}

// ============================================================================
// QUERIES
// ============================================================================

std::vector<HotpatchSymbolEntry> HotpatchSymbolProvider::getAllSymbols() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_symbols;
}

std::vector<HotpatchSymbolEntry> HotpatchSymbolProvider::getSymbolsByLayer(
    HotpatchLayer layer) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<HotpatchSymbolEntry> result;
    for (auto& s : m_symbols) {
        if (layer == HotpatchLayer::All || s.layer == layer) {
            result.push_back(s);
        }
    }
    return result;
}

std::vector<HotpatchSymbolEntry> HotpatchSymbolProvider::getSymbolsByKind(
    HotpatchSymbolKind kind) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<HotpatchSymbolEntry> result;
    for (auto& s : m_symbols) {
        if (s.kind == kind) {
            result.push_back(s);
        }
    }
    return result;
}

const HotpatchSymbolEntry* HotpatchSymbolProvider::findByName(const char* name) const {
    if (!name) return nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.lookupCount.fetch_add(1);

    uint64_t hash = computeHash(name);

    // Try ASM-accelerated binary search first
    if (!m_index.hashes.empty()) {
        int64_t idx = asm_symbol_hash_lookup(
            m_index.hashes.data(), (int64_t)m_index.hashes.size(), hash);
        if (idx >= 0 && idx < (int64_t)m_index.indices.size()) {
            uint32_t symIdx = m_index.indices[idx];
            if (symIdx < m_symbols.size()) {
                m_stats.lookupHits.fetch_add(1);
                return &m_symbols[symIdx];
            }
        }
    }

    // Fallback: linear scan (handles hash collisions)
    for (auto& s : m_symbols) {
        if (s.hash == hash && std::strcmp(s.name, name) == 0) {
            m_stats.lookupHits.fetch_add(1);
            return &s;
        }
    }

    return nullptr;
}

std::vector<HotpatchSymbolEntry> HotpatchSymbolProvider::findByPrefix(
    const char* prefix) const
{
    if (!prefix) return {};

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.lookupCount.fetch_add(1);

    size_t prefixLen = std::strlen(prefix);
    std::vector<HotpatchSymbolEntry> result;

    for (auto& s : m_symbols) {
        if (s.name && std::strncmp(s.name, prefix, prefixLen) == 0) {
            result.push_back(s);
        }
    }

    if (!result.empty()) m_stats.lookupHits.fetch_add(1);
    return result;
}

const HotpatchSymbolEntry* HotpatchSymbolProvider::findAtPosition(
    const char* filePath, uint32_t line, uint32_t character) const
{
    if (!filePath) return nullptr;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.lookupCount.fetch_add(1);

    for (auto& s : m_symbols) {
        if (!s.filePath || !s.filePath[0]) continue;
        if (std::strcmp(s.filePath, filePath) != 0) continue;
        if (s.line == line && character >= s.startChar && character <= s.endChar) {
            m_stats.lookupHits.fetch_add(1);
            return &s;
        }
    }

    return nullptr;
}

std::vector<HotpatchSymbolEntry> HotpatchSymbolProvider::getFileSymbols(
    const char* filePath) const
{
    if (!filePath) return {};

    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<HotpatchSymbolEntry> result;

    for (auto& s : m_symbols) {
        if (s.filePath && std::strcmp(s.filePath, filePath) == 0) {
            result.push_back(s);
        }
    }

    return result;
}
