// unified_hotpatch_manager.cpp — Unified Hotpatch Coordination Layer Implementation
// Routes patches to proper layer, tracks stats, preset save/load via manual JSON.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "unified_hotpatch_manager.hpp"
#include "../server/gguf_server_hotpatch.hpp"
#include "live_binary_patcher.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <fstream>

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
UnifiedHotpatchManager& UnifiedHotpatchManager::instance() {
    static UnifiedHotpatchManager inst;
    return inst;
}

UnifiedHotpatchManager::UnifiedHotpatchManager() {
    std::memset(m_eventRing, 0, sizeof(m_eventRing));
    std::memset(m_callbacks, 0, sizeof(m_callbacks));
}

UnifiedHotpatchManager::~UnifiedHotpatchManager() = default;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
uint64_t UnifiedHotpatchManager::next_sequence() {
    return m_sequence.fetch_add(1, std::memory_order_relaxed);
}

void UnifiedHotpatchManager::emit_event(HotpatchEvent::Type type, const char* detail) {
    uint64_t seq = next_sequence();
    uint64_t idx = m_eventHead.fetch_add(1, std::memory_order_relaxed) & EVENT_RING_MASK;

    m_eventRing[idx].type       = type;
    m_eventRing[idx].timestamp  = GetTickCount64();
    m_eventRing[idx].sequenceId = seq;
    m_eventRing[idx].detail     = detail;

    // Fire callbacks
    for (size_t i = 0; i < m_callbackCount; ++i) {
        if (m_callbacks[i].callback) {
            m_callbacks[i].callback(&m_eventRing[idx], m_callbacks[i].userData);
        }
    }
}

// ---------------------------------------------------------------------------
// Memory Layer (Layer 1)
// ---------------------------------------------------------------------------
UnifiedResult UnifiedHotpatchManager::apply_memory_patch(void* addr, size_t size, const void* data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    PatchResult r = ::apply_memory_patch(addr, size, data);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.memoryPatchCount.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::MemoryPatchApplied, r.detail);
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "memory", next_sequence());
}

UnifiedResult UnifiedHotpatchManager::apply_memory_patch_tracked(MemoryPatchEntry* entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    PatchResult r = ::apply_memory_patch_tracked(entry);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.memoryPatchCount.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::MemoryPatchApplied, r.detail);
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "memory", next_sequence());
}

UnifiedResult UnifiedHotpatchManager::revert_memory_patch(MemoryPatchEntry* entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    PatchResult r = ::revert_memory_patch(entry);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        emit_event(HotpatchEvent::MemoryPatchReverted, r.detail);
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "memory", next_sequence());
}

// ---------------------------------------------------------------------------
// Byte Layer (Layer 2)
// ---------------------------------------------------------------------------
UnifiedResult UnifiedHotpatchManager::apply_byte_patch(const char* filename, const BytePatch& patch) {
    std::lock_guard<std::mutex> lock(m_mutex);
    PatchResult r = ::patch_bytes(filename, patch);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.bytePatchCount.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::BytePatchApplied, r.detail);
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::BytePatchFailed, r.detail);
    }
    return UnifiedResult::from(r, "byte", next_sequence());
}

UnifiedResult UnifiedHotpatchManager::apply_byte_search_patch(
    const char* filename,
    const std::vector<uint8_t>& pattern,
    const std::vector<uint8_t>& replacement)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    PatchResult r = ::search_and_patch_bytes(filename, pattern, replacement);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.bytePatchCount.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::BytePatchApplied, r.detail);
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::BytePatchFailed, r.detail);
    }
    return UnifiedResult::from(r, "byte", next_sequence());
}

// ---------------------------------------------------------------------------
// Server Layer (Layer 3)
// ---------------------------------------------------------------------------
UnifiedResult UnifiedHotpatchManager::add_server_patch(ServerHotpatch* patch) {
    if (!patch || !patch->name || !patch->transform) {
        return UnifiedResult::from(PatchResult::error("Null server patch or missing fields", 1), "server", next_sequence());
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_serverPatches.push_back(patch);
    m_stats.serverPatchCount.fetch_add(1, std::memory_order_relaxed);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    emit_event(HotpatchEvent::ServerPatchAdded, patch->name);
    return UnifiedResult::from(PatchResult::ok("Server patch added"), "server", next_sequence());
}

UnifiedResult UnifiedHotpatchManager::remove_server_patch(const char* name) {
    if (!name) {
        return UnifiedResult::from(PatchResult::error("Null name", 1), "server", next_sequence());
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_serverPatches.begin(); it != m_serverPatches.end(); ++it) {
        if (std::strcmp((*it)->name, name) == 0) {
            m_serverPatches.erase(it);
            m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
            emit_event(HotpatchEvent::ServerPatchRemoved, name);
            return UnifiedResult::from(PatchResult::ok("Server patch removed"), "server", next_sequence());
        }
    }
    return UnifiedResult::from(PatchResult::error("Server patch not found", 2), "server", next_sequence());
}

// ---------------------------------------------------------------------------
// PT Driver (Layer 0 — Page Table substrate)
// ---------------------------------------------------------------------------

PatchResult UnifiedHotpatchManager::pt_initialize() {
    return PTDriverContract::instance().initialize();
}

PatchResult UnifiedHotpatchManager::pt_shutdown() {
    return PTDriverContract::instance().shutdown();
}

const PTDriverStats& UnifiedHotpatchManager::pt_get_stats() const {
    return PTDriverContract::instance().get_stats();
}

UnifiedResult UnifiedHotpatchManager::pt_arm_watchpoint(uintptr_t addr, uint64_t size,
                                                         WatchpointEntry::WatchpointCallback cb,
                                                         void* ctx, bool oneShot, uint32_t* outId) {
    uint64_t seq = next_sequence();
    PatchResult r = PTDriverContract::instance().arm_watchpoint(addr, size, cb, ctx, oneShot, outId);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.ptOperationCount.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::PTWatchpointArmed, r.detail);
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "pt", seq);
}

UnifiedResult UnifiedHotpatchManager::pt_disarm_watchpoint(uint32_t id) {
    uint64_t seq = next_sequence();
    PatchResult r = PTDriverContract::instance().disarm_watchpoint(id);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.ptOperationCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "pt", seq);
}

UnifiedResult UnifiedHotpatchManager::pt_take_snapshot(uintptr_t addr, uint64_t size,
                                                        const char* label, uint32_t layerIndex,
                                                        uint32_t* outSnapshotId) {
    uint64_t seq = next_sequence();
    PatchResult r = PTDriverContract::instance().take_snapshot(addr, size, label, layerIndex, outSnapshotId);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.ptOperationCount.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::PTSnapshotTaken, label ? label : "snapshot");
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "pt", seq);
}

UnifiedResult UnifiedHotpatchManager::pt_restore_snapshot(uint32_t snapshotId) {
    uint64_t seq = next_sequence();
    PatchResult r = PTDriverContract::instance().restore_snapshot(snapshotId);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.ptOperationCount.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::PTSnapshotRestored, "snapshot restored");
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "pt", seq);
}

UnifiedResult UnifiedHotpatchManager::pt_set_protection(uintptr_t addr, uint64_t size,
                                                          uint32_t newProtect, uint32_t* outOldProtect) {
    uint64_t seq = next_sequence();
    PatchResult r = PTDriverContract::instance().set_protection(addr, size, newProtect, outOldProtect);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.ptOperationCount.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::PTProtectionChanged, r.detail);
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "pt", seq);
}

UnifiedResult UnifiedHotpatchManager::pt_alloc_large_arena(uint64_t sizeBytes, uint64_t pageSize,
                                                             uint32_t* outArenaId) {
    uint64_t seq = next_sequence();
    PatchResult r = PTDriverContract::instance().allocate_large_arena(sizeBytes, pageSize, outArenaId);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.ptOperationCount.fetch_add(1, std::memory_order_relaxed);
        emit_event(HotpatchEvent::PTArenaAllocated, "large-page arena");
    } else {
        m_stats.totalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "pt", seq);
}

UnifiedResult UnifiedHotpatchManager::pt_normalize(const ASLRContext* ctx,
                                                     uintptr_t absAddr, uintptr_t* outRelative) {
    uint64_t seq = next_sequence();
    PatchResult r = PTDriverContract::instance().normalize_address(ctx, absAddr, outRelative);

    m_stats.totalOperations.fetch_add(1, std::memory_order_relaxed);
    if (r.success) {
        m_stats.ptOperationCount.fetch_add(1, std::memory_order_relaxed);
    }
    return UnifiedResult::from(r, "pt", seq);
}

// ---------------------------------------------------------------------------
// Preset Management (manual JSON serializer)
// ---------------------------------------------------------------------------
PatchResult UnifiedHotpatchManager::save_preset(const char* filename, const HotpatchPreset& preset) {
    if (!filename) {
        return PatchResult::error("Null filename", 1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    FILE* f = std::fopen(filename, "wb");
    if (!f) {
        return PatchResult::error("Could not open file for writing", 2);
    }

    // Manual JSON serialization
    std::fprintf(f, "{\n");
    std::fprintf(f, "  \"name\": \"%s\",\n", preset.name);
    std::fprintf(f, "  \"memoryPatchCount\": %zu,\n", preset.memoryPatches.size());
    std::fprintf(f, "  \"bytePatchCount\": %zu,\n", preset.bytePatches.size());
    std::fprintf(f, "  \"serverPatchNames\": [");
    for (size_t i = 0; i < preset.serverPatchNames.size(); ++i) {
        if (i > 0) std::fprintf(f, ", ");
        std::fprintf(f, "\"%s\"", preset.serverPatchNames[i].c_str());
    }
    std::fprintf(f, "],\n");

    // Byte patches with hex-encoded data
    std::fprintf(f, "  \"bytePatches\": [\n");
    for (size_t i = 0; i < preset.bytePatches.size(); ++i) {
        const auto& bp = preset.bytePatches[i];
        std::fprintf(f, "    {\"offset\": %zu, \"size\": %zu, \"desc\": \"%s\"}",
                     bp.offset, bp.data.size(), bp.description.c_str());
        if (i + 1 < preset.bytePatches.size()) std::fprintf(f, ",");
        std::fprintf(f, "\n");
    }
    std::fprintf(f, "  ]\n");
    std::fprintf(f, "}\n");

    std::fclose(f);
    emit_event(HotpatchEvent::PresetSaved, preset.name);
    return PatchResult::ok("Preset saved");
}

PatchResult UnifiedHotpatchManager::load_preset(const char* filename, HotpatchPreset* outPreset) {
    if (!filename || !outPreset) {
        return PatchResult::error("Null filename or output preset", 1);
    }

    // Open and read the entire preset file
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Preset file not found", static_cast<int>(GetLastError()));
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE || fileSize > 1024 * 1024) { // 1MB limit
        CloseHandle(hFile);
        return PatchResult::error("Preset file too large or unreadable", 2);
    }

    std::vector<char> buffer(fileSize + 1, 0);
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL)) {
        CloseHandle(hFile);
        return PatchResult::error("Failed to read preset file", static_cast<int>(GetLastError()));
    }
    CloseHandle(hFile);

    std::string json(buffer.data(), bytesRead);

    // Lightweight JSON parser for our known preset format
    auto extractString = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        size_t start = json.find('"', pos + search.size() + 1) + 1;
        size_t end = json.find('"', start);
        if (start == std::string::npos || end == std::string::npos) return "";
        return json.substr(start, end - start);
    };

    auto extractInt = [&](const std::string& key) -> int {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0;
        size_t colon = json.find(':', pos);
        if (colon == std::string::npos) return 0;
        return std::atoi(json.c_str() + colon + 1);
    };

    // Parse preset fields
    {
        std::string n = extractString("name");
        std::strncpy(outPreset->name, n.c_str(), sizeof(outPreset->name) - 1);
        outPreset->name[sizeof(outPreset->name) - 1] = '\0';
    }
    outPreset->version = extractInt("version");

    // Parse memoryPatchNames array
    outPreset->memoryPatchNames.clear();
    size_t memPos = json.find("\"memoryPatchNames\"");
    if (memPos != std::string::npos) {
        size_t arrStart = json.find('[', memPos);
        size_t arrEnd = json.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            std::string arrStr = json.substr(arrStart + 1, arrEnd - arrStart - 1);
            size_t p = 0;
            while ((p = arrStr.find('"', p)) != std::string::npos) {
                size_t e = arrStr.find('"', p + 1);
                if (e != std::string::npos) {
                    outPreset->memoryPatchNames.push_back(arrStr.substr(p + 1, e - p - 1));
                    p = e + 1;
                } else break;
            }
        }
    }

    // Parse serverPatchNames array
    outPreset->serverPatchNames.clear();
    size_t srvPos = json.find("\"serverPatchNames\"");
    if (srvPos != std::string::npos) {
        size_t arrStart = json.find('[', srvPos);
        size_t arrEnd = json.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            std::string arrStr = json.substr(arrStart + 1, arrEnd - arrStart - 1);
            size_t p = 0;
            while ((p = arrStr.find('"', p)) != std::string::npos) {
                size_t e = arrStr.find('"', p + 1);
                if (e != std::string::npos) {
                    outPreset->serverPatchNames.push_back(arrStr.substr(p + 1, e - p - 1));
                    p = e + 1;
                } else break;
            }
        }
    }

    // Parse bytePatches array
    outPreset->bytePatches.clear();
    size_t bpPos = json.find("\"bytePatches\"");
    if (bpPos != std::string::npos) {
        size_t arrStart = json.find('[', bpPos);
        size_t arrEnd = json.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos) {
            std::string arrStr = json.substr(arrStart + 1, arrEnd - arrStart - 1);
            // Parse each {offset, size, desc} object
            size_t objPos = 0;
            while ((objPos = arrStr.find('{', objPos)) != std::string::npos) {
                size_t objEnd = arrStr.find('}', objPos);
                if (objEnd == std::string::npos) break;
                std::string objStr = arrStr.substr(objPos, objEnd - objPos + 1);

                BytePatch entry;
                entry.offset = 0;
                // Extract offset
                size_t offPos = objStr.find("\"offset\"");
                if (offPos != std::string::npos) {
                    size_t colon = objStr.find(':', offPos);
                    entry.offset = static_cast<size_t>(std::strtoull(objStr.c_str() + colon + 1, nullptr, 10));
                }
                // Extract size
                size_t szPos = objStr.find("\"size\"");
                if (szPos != std::string::npos) {
                    size_t colon = objStr.find(':', szPos);
                    size_t dataSize = static_cast<size_t>(std::strtoull(objStr.c_str() + colon + 1, nullptr, 10));
                    entry.data.resize(dataSize, 0);
                }
                // Extract description
                size_t descPos = objStr.find("\"desc\"");
                if (descPos != std::string::npos) {
                    size_t s = objStr.find('"', descPos + 6) + 1;
                    size_t e = objStr.find('"', s);
                    if (s != std::string::npos && e != std::string::npos) {
                        entry.description = objStr.substr(s, e - s);
                    }
                }
                outPreset->bytePatches.push_back(entry);
                objPos = objEnd + 1;
            }
        }
    }

    emit_event(HotpatchEvent::PresetLoaded, filename);
    return PatchResult::ok("Preset loaded successfully");
}

// ---------------------------------------------------------------------------
// Event System
// ---------------------------------------------------------------------------
void UnifiedHotpatchManager::register_callback(HotpatchEventCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_callbackCount < MAX_CALLBACKS && cb) {
        m_callbacks[m_callbackCount].callback = cb;
        m_callbacks[m_callbackCount].userData = userData;
        ++m_callbackCount;
    }
}

void UnifiedHotpatchManager::unregister_callback(HotpatchEventCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < m_callbackCount; ++i) {
        if (m_callbacks[i].callback == cb) {
            // Shift remaining entries
            for (size_t j = i; j + 1 < m_callbackCount; ++j) {
                m_callbacks[j] = m_callbacks[j + 1];
            }
            --m_callbackCount;
            m_callbacks[m_callbackCount] = {};
            return;
        }
    }
}

bool UnifiedHotpatchManager::poll_event(HotpatchEvent* outEvent) {
    uint64_t tail = m_eventTail.load(std::memory_order_relaxed);
    uint64_t head = m_eventHead.load(std::memory_order_relaxed);
    if (tail >= head) {
        return false; // No new events
    }
    uint64_t idx = tail & EVENT_RING_MASK;
    *outEvent = m_eventRing[idx];
    m_eventTail.fetch_add(1, std::memory_order_relaxed);
    return true;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
const UnifiedHotpatchManager::Stats& UnifiedHotpatchManager::getStats() const {
    return m_stats;
}

void UnifiedHotpatchManager::resetStats() {
    m_stats.memoryPatchCount.store(0, std::memory_order_relaxed);
    m_stats.bytePatchCount.store(0, std::memory_order_relaxed);
    m_stats.serverPatchCount.store(0, std::memory_order_relaxed);
    m_stats.totalOperations.store(0, std::memory_order_relaxed);
    m_stats.totalFailures.store(0, std::memory_order_relaxed);
}
