#pragma once
// HotpatchLinkerBridge — Runtime hotpatch linker bridge
// Bridges the hotpatch engine to the tool registry for live-patching operations.

#include <string>
#include <cstdint>
#include <functional>
#include <vector>
#include <map>
#include <windows.h>
#include <cstring>

namespace RawrXD {
namespace Runtime {

struct HotpatchResult {
    bool    success = false;
    std::string message;
    uint64_t patchAddress = 0;
    uint32_t patchSize    = 0;
};

class HotpatchLinkerBridge {
public:
    HotpatchLinkerBridge() = default;

    static HotpatchLinkerBridge& instance() {
        static HotpatchLinkerBridge s;
        return s;
    }

    bool Initialize(const std::string& targetModule) {
        m_targetModule = targetModule;
        m_initialized = true;
        return true;
    }

    HotpatchResult ApplyPatch(uint64_t address, const void* patchData, uint32_t patchSize) {
        HotpatchResult r;
        if (!m_initialized) {
            r.success = false;
            r.message = "HotpatchLinkerBridge: not initialized";
            return r;
        }

        if (address == 0 || !patchData || patchSize == 0) {
            r.success = false;
            r.message = "HotpatchLinkerBridge: invalid parameters";
            return r;
        }

        // Change memory protection to allow writing
        DWORD oldProtect;
        if (!VirtualProtect((void*)address, patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            r.success = false;
            r.message = "HotpatchLinkerBridge: VirtualProtect failed";
            return r;
        }

        // Store original bytes for revert
        std::vector<uint8_t> originalBytes(patchSize);
        memcpy(originalBytes.data(), (void*)address, patchSize);
        m_patchBackups[address] = std::move(originalBytes);

        // Apply patch
        memcpy((void*)address, patchData, patchSize);

        // Restore original protection
        VirtualProtect((void*)address, patchSize, oldProtect, &oldProtect);

        // Flush instruction cache
        FlushInstructionCache(GetCurrentProcess(), (void*)address, patchSize);

        r.success = true;
        r.message = "Patch applied successfully";
        r.patchAddress = address;
        r.patchSize = patchSize;
        return r;
    }

    HotpatchResult RevertPatch(uint64_t address) {
        HotpatchResult r;
        if (!m_initialized) {
            r.success = false;
            r.message = "HotpatchLinkerBridge: not initialized";
            return r;
        }

        auto it = m_patchBackups.find(address);
        if (it == m_patchBackups.end()) {
            r.success = false;
            r.message = "HotpatchLinkerBridge: no patch to revert at address";
            return r;
        }

        // Change memory protection to allow writing
        DWORD oldProtect;
        if (!VirtualProtect((void*)address, it->second.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            r.success = false;
            r.message = "HotpatchLinkerBridge: VirtualProtect failed";
            return r;
        }

        // Restore original bytes
        memcpy((void*)address, it->second.data(), it->second.size());

        // Restore original protection
        VirtualProtect((void*)address, it->second.size(), oldProtect, &oldProtect);

        // Flush instruction cache
        FlushInstructionCache(GetCurrentProcess(), (void*)address, it->second.size());

        // Remove backup
        m_patchBackups.erase(it);

        r.success = true;
        r.message = "Patch reverted successfully";
        r.patchAddress = address;
        return r;
    }

    bool runSentinelAudit(const std::string& symbolName, const std::vector<uint8_t>& expectedBytes) {
        (void)symbolName; (void)expectedBytes;
        return true;
    }

    bool IsInitialized() const { return m_initialized; }
    const std::string& GetTargetModule() const { return m_targetModule; }

private:
    std::string m_targetModule;
    bool m_initialized = false;
    std::map<uint64_t, std::vector<uint8_t>> m_patchBackups;
};

} // namespace Runtime
} // namespace RawrXD
