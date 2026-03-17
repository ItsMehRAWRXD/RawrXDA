#include "hot_patcher.h"
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
        std::cerr << "[PATCH] Error: Patch '" << patch_name << "' already exists.\n";
        return false;
    }

    DWORD old_protect;
    size_t size = new_opcodes.size();

    // 1. UNLOCK MEMORY
    // We must change the page protection to Read-Write-Execute
    if (!VirtualProtect(target_address, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        std::cerr << "[PATCH] Critical: Failed to unlock memory at " << target_address << "\n";
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

    std::cout << "[PATCH] Successfully applied '" << patch_name << "' at " << target_address << "\n";
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
    std::cout << "[PATCH] Reverted '" << patch_name << "'\n";
    return true;
}

void* HotPatcher::GetFunctionAddress(const std::string& module_name, const std::string& function_name) {
    HMODULE hModule = GetModuleHandleA(module_name.empty() ? NULL : module_name.c_str());
    if (!hModule) return nullptr;
    return (void*)GetProcAddress(hModule, function_name.c_str());
}

void HotPatcher::ListPatches() {
    std::cout << "=== Active HotPatches ===\n";
    for (const auto& [name, record] : patches_) {
        std::cout << (record.active ? "[ON]  " : "[OFF] ") 
                  << name << " @ " << record.target_address << "\n";
    }
}

bool HotPatcher::ScanAndPatch(const std::string& patch_name, const std::vector<unsigned char>& signature, const std::vector<unsigned char>& replacement) {
    if (signature.empty()) {
        std::cerr << "[PATCH] Error: Empty signature for scan-and-patch\n";
        return false;
    }

    // Get module info for current process
    HMODULE hModule = GetModuleHandleA(NULL);
    if (!hModule) return false;

    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) {
        std::cerr << "[PATCH] Failed to get module information\n";
        return false;
    }

    unsigned char* baseAddr = static_cast<unsigned char*>(modInfo.lpBaseOfDll);
    size_t moduleSize = modInfo.SizeOfImage;

    // Scan for signature (Boyer-Moore would be ideal, using naive scan for simplicity)
    for (size_t i = 0; i < moduleSize - signature.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < signature.size(); ++j) {
            if (baseAddr[i + j] != signature[j]) {
                match = false;
                break;
            }
        }

        if (match) {
            void* target = baseAddr + i;
            std::cout << "[PATCH] Signature found at " << target << "\n";
            return ApplyPatch(patch_name, target, replacement);
        }
    }

    std::cerr << "[PATCH] Signature not found in memory\n";
    return false;
}

HotPatcher g_hot_patcher;

// Helper to parse hex address string to void*
void* ParseHexAddress(const std::string& addrStr) {
    if (addrStr.empty()) return nullptr;
    unsigned long long addr = 0;
    try {
        addr = std::stoull(addrStr, nullptr, 16);
    } catch(...) { return nullptr; }
    return reinterpret_cast<void*>(addr);
}
