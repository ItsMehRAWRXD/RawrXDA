#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <sstream>

// TITAN Bridging Headers
// (Headers removed to avoid extern "C" duplication with linked ASM stubs below)
/*
#define RAWRXD_NATIVE_SPEED_HPP
#define _NATIVE_SPEED_LAYER_HPP_
#define NATIVE_SPEED_LAYER_HPP
#define BYTE_LEVEL_HOTPATCHER_HPP
#define _BYTE_LEVEL_HOTPATCHER_HPP_
#define BYTE_LEVEL_HOTPATCHER_HPP_
#include "core/shadow_page_detour.hpp"
#include "core/native_speed_layer.hpp"
#include "core/byte_level_hotpatcher.hpp"
#include "core/model_memory_hotpatch.hpp"
*/

#ifndef RAWRXD_SUBSYS_MODES_D_INTEGRATED
#define RAWRXD_SUBSYS_MODES_D_INTEGRATED
#endif

namespace {
constexpr size_t kMaxSnapshotIdBytes = 256;
constexpr size_t kMaxSnapshotDataBytes = 2 * 1024 * 1024;

std::string SanitizeId(const std::string& id) {
    std::string out;
    out.reserve(id.size());
    for (char ch : id) {
        if ((ch >= '0' && ch <= '9') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            ch == '-' || ch == '_' || ch == '.') {
            out.push_back(ch);
        }
    }
    return out;
}

std::filesystem::path GetSnapshotRoot() {
    wchar_t tempPath[MAX_PATH] = {};
    DWORD n = GetTempPathW(static_cast<DWORD>(_countof(tempPath)), tempPath);
    if (n == 0 || n >= _countof(tempPath)) {
        return std::filesystem::path(L".") / L"RawrXD" / L"mnemosyne";
    }
    return std::filesystem::path(tempPath) / L"RawrXD" / L"mnemosyne";
}
}

namespace RawrXD::Memory {

/**
 * @brief Win32IDE_Mnemosyne: Persistent Workspace Memory
 * Manages the persistent memory 'checkpoints' for the agent.
 * Used for survival across reboots. High durability.
 */
class Mnemosyne {
public:
    static Mnemosyne& GetInstance() {
        static Mnemosyne instance;
        return instance;
    }

    // Resolves: Mnemosyne::Checkpoint
    bool Checkpoint(const std::string& snapshot_id, const std::string& data) {
        std::lock_guard<std::mutex> lock(m_mutex);

        const std::string cleanId = SanitizeId(snapshot_id);
        if (cleanId.empty() || cleanId.size() > kMaxSnapshotIdBytes) {
            LOG_ERROR("[Mnemosyne] Rejecting invalid snapshot id.");
            return false;
        }
        if (data.size() > kMaxSnapshotDataBytes) {
            LOG_ERROR("[Mnemosyne] Rejecting oversized snapshot payload.");
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(m_root, ec);
        if (ec) {
            LOG_ERROR("[Mnemosyne] Failed to create snapshot directory.");
            return false;
        }

        const std::filesystem::path target = m_root / (cleanId + ".json");
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            LOG_ERROR("[Mnemosyne] Failed to open snapshot file for write.");
            return false;
        }
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!out.good()) {
            LOG_ERROR("[Mnemosyne] Failed to write snapshot data.");
            return false;
        }

        m_cache[cleanId] = data;
        LOG_INFO("[Mnemosyne] Saved checkpoint: " + cleanId);
        return true;
    }

    // Resolves: Mnemosyne::Hydrate
    std::string Hydrate(const std::string& snapshot_id) {
        std::lock_guard<std::mutex> lock(m_mutex);

        const std::string cleanId = SanitizeId(snapshot_id);
        if (cleanId.empty() || cleanId.size() > kMaxSnapshotIdBytes) {
            return "{}";
        }

        auto it = m_cache.find(cleanId);
        if (it != m_cache.end()) {
            return it->second;
        }

        const std::filesystem::path target = m_root / (cleanId + ".json");
        std::ifstream in(target, std::ios::binary);
        if (!in.is_open()) {
            return "{}";
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        std::string hydrated = ss.str();
        if (hydrated.size() > kMaxSnapshotDataBytes) {
            return "{}";
        }
        m_cache[cleanId] = hydrated;
        LOG_INFO("[Mnemosyne] Hydrated checkpoint: " + cleanId);
        return hydrated;
    }

private:
    Mnemosyne() : m_root(GetSnapshotRoot()) {}
    std::mutex m_mutex;
    std::filesystem::path m_root;
    std::unordered_map<std::string, std::string> m_cache;
};


} // namespace RawrXD::Memory

extern "C" bool Mnemosyne_Checkpoint(const char* id, const char* data) {
    if (!id || !data) {
        return false;
    }
    return RawrXD::Memory::Mnemosyne::GetInstance().Checkpoint(id, data);
}

extern "C" const char* Mnemosyne_Hydrate(const char* id) {
    static thread_local std::string hydrated;
    if (!id) {
        hydrated = "{}";
        return hydrated.c_str();
    }
    hydrated = RawrXD::Memory::Mnemosyne::GetInstance().Hydrate(id);
    return hydrated.c_str();
}
#ifdef RAWRXD_MASM_CORE_NATIVE_BRIDGE
// Strict-lane bridge stubs. No-op implementations that satisfy the linker.
// Signatures match shadow_page_detour.hpp, native_speed_layer.hpp,
// byte_level_hotpatcher.hpp, and camellia256_bridge.hpp.
// NOTE: asm_apply_memory_patch is provided by RawrXD_SelfPatch_Agent.asm.

#endif // RAWRXD_MASM_CORE_NATIVE_BRIDGE
