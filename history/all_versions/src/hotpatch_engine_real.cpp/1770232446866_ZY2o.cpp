#include "hot_patcher.h"
#include <iostream>
#include <windows.h>
#include <vector>
#include <map>
#include <cstring>
#include <sstream>
#include <iomanip>

// Real HotPatcher Implementation with actual memory patching
class HotPatcherImpl {
private:
    struct PatchInfo {
        std::string name;
        void* original_address;
        std::vector<unsigned char> original_bytes;
        std::vector<unsigned char> patch_bytes;
        bool is_active;
        DWORD original_protection;
    };
    
    std::map<std::string, PatchInfo> m_patches;
    CRITICAL_SECTION m_cs;
    
public:
    HotPatcherImpl() {
        InitializeCriticalSection(&m_cs);
        std::cout << "[HOTPATCHER] Engine initialized with real memory patching\n";
    }
    
    ~HotPatcherImpl() {
        // Restore all patches before shutdown
        for (auto& patch : m_patches) {
            if (patch.second.is_active) {
                RestorePatch(patch.first);
            }
        }
        DeleteCriticalSection(&m_cs);
    }
    
    bool ApplyPatch(const std::string& name, void* address, const std::vector<unsigned char>& bytes) {
        EnterCriticalSection(&m_cs);
        
        if (m_patches.find(name) != m_patches.end() && m_patches[name].is_active) {
            LeaveCriticalSection(&m_cs);
            return false; // Patch already applied
        }
        
        try {
            // Save original bytes for restoration
            PatchInfo info;
            info.name = name;
            info.original_address = address;
            info.patch_bytes = bytes;
            info.original_bytes.resize(bytes.size());
            info.is_active = false;
            
            // Read original bytes
            if (!ReadProcessMemory(GetCurrentProcess(), address, 
                                   info.original_bytes.data(), bytes.size(), nullptr)) {
                LeaveCriticalSection(&m_cs);
                std::cerr << "[HOTPATCHER] Failed to read original bytes at " << std::hex << address << "\n";
                return false;
            }
            
            // Change memory protection to writable
            DWORD old_protect;
            if (!VirtualProtect(address, bytes.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
                LeaveCriticalSection(&m_cs);
                std::cerr << "[HOTPATCHER] Failed to change protection\n";
                return false;
            }
            
            info.original_protection = old_protect;
            
            // Write patch bytes
            if (!WriteProcessMemory(GetCurrentProcess(), address, 
                                    bytes.data(), bytes.size(), nullptr)) {
                VirtualProtect(address, bytes.size(), old_protect, &old_protect);
                LeaveCriticalSection(&m_cs);
                std::cerr << "[HOTPATCHER] Failed to write patch bytes\n";
                return false;
            }
            
            // Restore original protection
            DWORD dummy;
            VirtualProtect(address, bytes.size(), old_protect, &dummy);
            
            // Flush instruction cache
            FlushInstructionCache(GetCurrentProcess(), address, bytes.size());
            
            info.is_active = true;
            m_patches[name] = info;
            
            std::cout << "[HOTPATCHER] ✓ Applied patch '" << name << "' at " 
                     << std::hex << address << " (" << std::dec << bytes.size() << " bytes)\n";
            
            LeaveCriticalSection(&m_cs);
            return true;
        } catch (...) {
            LeaveCriticalSection(&m_cs);
            return false;
        }
    }
    
    bool RestorePatch(const std::string& name) {
        EnterCriticalSection(&m_cs);
        
        auto it = m_patches.find(name);
        if (it == m_patches.end() || !it->second.is_active) {
            LeaveCriticalSection(&m_cs);
            return false;
        }
        
        PatchInfo& patch = it->second;
        
        try {
            // Change protection
            DWORD old_protect;
            if (!VirtualProtect(patch.original_address, patch.original_bytes.size(), 
                               PAGE_EXECUTE_READWRITE, &old_protect)) {
                LeaveCriticalSection(&m_cs);
                return false;
            }
            
            // Restore original bytes
            if (!WriteProcessMemory(GetCurrentProcess(), patch.original_address,
                                   patch.original_bytes.data(), patch.original_bytes.size(), nullptr)) {
                VirtualProtect(patch.original_address, patch.original_bytes.size(), old_protect, &old_protect);
                LeaveCriticalSection(&m_cs);
                return false;
            }
            
            // Restore protection
            DWORD dummy;
            VirtualProtect(patch.original_address, patch.original_bytes.size(), 
                          patch.original_protection, &dummy);
            
            FlushInstructionCache(GetCurrentProcess(), patch.original_address, patch.original_bytes.size());
            
            patch.is_active = false;
            std::cout << "[HOTPATCHER] ✓ Restored patch '" << name << "'\n";
            
            LeaveCriticalSection(&m_cs);
            return true;
        } catch (...) {
            LeaveCriticalSection(&m_cs);
            return false;
        }
    }
    
    std::string GetPatchStatus(const std::string& name) {
        EnterCriticalSection(&m_cs);
        
        std::stringstream ss;
        if (name.empty()) {
            // List all patches
            ss << "Total patches: " << m_patches.size() << "\n";
            for (const auto& patch : m_patches) {
                ss << "  " << patch.first << ": " 
                   << (patch.second.is_active ? "ACTIVE" : "INACTIVE") << "\n";
            }
        } else {
            auto it = m_patches.find(name);
            if (it != m_patches.end()) {
                ss << "Patch: " << it->first << "\n"
                   << "Status: " << (it->second.is_active ? "ACTIVE" : "INACTIVE") << "\n"
                   << "Address: 0x" << std::hex << (uintptr_t)it->second.original_address << "\n"
                   << "Size: " << std::dec << it->second.patch_bytes.size() << " bytes\n";
            } else {
                ss << "Patch not found: " << name;
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return ss.str();
    }
};

// Global instance
static HotPatcherImpl* g_impl = nullptr;

// Public interface
bool HotPatcher::ApplyPatch(const std::string& name, void* address, const std::vector<unsigned char>& bytes) {
    if (!g_impl) g_impl = new HotPatcherImpl();
    return g_impl->ApplyPatch(name, address, bytes);
}

bool HotPatcher::RestorePatch(const std::string& name) {
    if (!g_impl) return false;
    return g_impl->RestorePatch(name);
}

std::string HotPatcher::GetStatus() {
    if (!g_impl) g_impl = new HotPatcherImpl();
    return g_impl->GetPatchStatus("");
}

std::string HotPatcher::GetPatchStatus(const std::string& name) {
    if (!g_impl) return "Not initialized";
    return g_impl->GetPatchStatus(name);
}
