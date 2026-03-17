#include "hot_patcher.h"
#include "win32app/IDELogger.h"
#include <iomanip>
#include <psapi.h>

HotPatcher::~HotPatcher() {
    for (auto& [name, record] : patches_) {
        if (record.active) {
            RevertPatch(name);
        }
    }
}

bool HotPatcher::ApplyPatch(const std::string& patch_name, void* target_address, const std::vector<unsigned char>& new_opcodes) {
    if (patches_.find(patch_name) != patches_.end()) {
        LOG_ERROR("Patch '" + patch_name + "' already exists");
        return false;
    }

    DWORD old_protect;
    size_t size = new_opcodes.size();

    // 1. UNLOCK MEMORY
    // We must change the page protection to Read-Write-Execute
    if (!VirtualProtect(target_address, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        LOG_CRITICAL("Failed to unlock memory for patch");
        return false;
    }

    // 2. BACKUP ORIGINAL BYTES
    std::vector<unsigned char> original(size);
    memcpy(original.data(), target_address, size);

    // 3. INJECT NEW OPCODES
    memcpy(target_address, new_opcodes.data(), size);

    // 4. RELOCK MEMORY
    // Restore the old protection (usually PAGE_EXECUTE_READ)
    VirtualProtect(target_address, size, old_protect, &old_protect);

    // 5. RECORD STATE
    PatchRecord record;
    record.target_address = target_address;
    record.original_bytes = original;
    record.new_bytes = new_opcodes;
    record.size = size;
    record.active = true;
    patches_[patch_name] = record;

    LOG_INFO("Successfully applied '" + patch_name + "'");
    return true;
}

bool HotPatcher::RevertPatch(const std::string& patch_name) {
    auto it = patches_.find(patch_name);
    if (it == patches_.end()) return false;

    PatchRecord& record = it->second;
    if (!record.active) return true;

    DWORD old_protect;
    
    // Unlock
    if (!VirtualProtect(record.target_address, record.size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return false;
    }

    // Restore Original
    memcpy(record.target_address, record.original_bytes.data(), record.size);

    // Relock
    VirtualProtect(record.target_address, record.size, old_protect, &old_protect);

    record.active = false;
    LOG_INFO("Reverted '" + patch_name + "'");
    return true;
}

void* HotPatcher::GetFunctionAddress(const std::string& module_name, const std::string& function_name) {
    HMODULE hModule = GetModuleHandleA(module_name.empty() ? NULL : module_name.c_str());
    if (!hModule) return nullptr;
    return (void*)GetProcAddress(hModule, function_name.c_str());
}

void HotPatcher::ListPatches() {
    LOG_INFO("=== Active HotPatches ===");
    for (const auto& [name, record] : patches_) {
        LOG_INFO(std::string(record.active ? "[ON]  " : "[OFF] ") + name);
    }
}

HotPatcher g_hot_patcher;

// ============================================================================
// ScanAndPatch — Deterministic signature scan + byte replacement
// Scans the memory of the current process for an exact byte signature
// and applies a replacement patch. No heuristics, no fuzzy matching.
// ============================================================================
bool HotPatcher::ScanAndPatch(const std::string& patch_name, 
                               const std::vector<unsigned char>& signature, 
                               const std::vector<unsigned char>& replacement) {
    if (signature.empty()) {
        LOG_ERROR("ScanAndPatch: empty signature for '" + patch_name + "'");
        return false;
    }
    if (signature.size() != replacement.size()) {
        LOG_ERROR("ScanAndPatch: signature and replacement size mismatch for '" + patch_name + "'");
        return false;
    }
    if (patches_.find(patch_name) != patches_.end()) {
        LOG_ERROR("ScanAndPatch: patch '" + patch_name + "' already exists");
        return false;
    }

    // Enumerate all memory regions in the current process
    HANDLE hProcess = GetCurrentProcess();
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    unsigned char* addr = static_cast<unsigned char*>(sysInfo.lpMinimumApplicationAddress);
    unsigned char* maxAddr = static_cast<unsigned char*>(sysInfo.lpMaximumApplicationAddress);
    
    MEMORY_BASIC_INFORMATION mbi;
    size_t scanCount = 0;

    while (addr < maxAddr) {
        if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) {
            break;
        }

        // Only scan committed, executable/readable regions
        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect == PAGE_EXECUTE_READ || 
             mbi.Protect == PAGE_EXECUTE_READWRITE ||
             mbi.Protect == PAGE_EXECUTE_WRITECOPY ||
             mbi.Protect == PAGE_READONLY ||
             mbi.Protect == PAGE_READWRITE)) {

            unsigned char* regionBase = static_cast<unsigned char*>(mbi.BaseAddress);
            size_t regionSize = mbi.RegionSize;
            scanCount += regionSize;

            // Linear scan for exact signature match
            if (regionSize >= signature.size()) {
                for (size_t i = 0; i <= regionSize - signature.size(); ++i) {
                    if (memcmp(regionBase + i, signature.data(), signature.size()) == 0) {
                        // Found exact match — apply patch via ApplyPatch
                        void* target = regionBase + i;
                        LOG_INFO("ScanAndPatch: found signature for '" + patch_name + "'");
                        return ApplyPatch(patch_name, target, replacement);
                    }
                }
            }
        }

        addr = static_cast<unsigned char*>(mbi.BaseAddress) + mbi.RegionSize;
    }

    LOG_WARNING("ScanAndPatch: signature not found for '" + patch_name + "'");
    return false;
}

// Helper to parse hex address string to void*
void* ParseHexAddress(const std::string& addrStr) {
    if (addrStr.empty()) return nullptr;
    unsigned long long addr = 0;
    try {
        addr = std::stoull(addrStr, nullptr, 16);
    } catch(...) { return nullptr; }
    return reinterpret_cast<void*>(addr);
}
