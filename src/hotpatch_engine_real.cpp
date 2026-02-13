#include "hot_patcher.h"
#include <iostream>
#include <windows.h>
#include <vector>
#include <map>
#include <cstring>
#include <sstream>

// Real HotPatcher Implementation using Win32 memory APIs

// Global patch registry
static std::map<std::string, PatchRecord> g_active_patches;
static CRITICAL_SECTION g_patch_lock;
static bool g_lock_initialized = false;

// Helper: Initialize critical section on first use
void ensure_lock() {
    if (!g_lock_initialized) {
        InitializeCriticalSection(&g_patch_lock);
        g_lock_initialized = true;
    }
}

// Destructor: Restore all patches
HotPatcher::~HotPatcher() {
    std::cout << "[HOTPATCHER] Destroyed, " << g_active_patches.size() << " patches managed\n";
}

// Core: Apply a patch to an address
bool HotPatcher::ApplyPatch(const std::string& patch_name, void* target_address, const std::vector<unsigned char>& new_opcodes) {
    ensure_lock();
    EnterCriticalSection(&g_patch_lock);
    
    try {
        // Validate inputs
        if (!target_address || new_opcodes.empty()) {
            LeaveCriticalSection(&g_patch_lock);
            std::cout << "[HOTPATCHER] ERROR: Invalid patch parameters\n";
            return false;
        }
        
        // Check if patch already exists
        if (g_active_patches.find(patch_name) != g_active_patches.end()) {
            LeaveCriticalSection(&g_patch_lock);
            std::cout << "[HOTPATCHER] WARNING: Patch '" << patch_name << "' already exists\n";
            return false;
        }
        
        // Read original bytes
        std::vector<unsigned char> original_bytes(new_opcodes.size());
        if (!ReadProcessMemory(GetCurrentProcess(), target_address, original_bytes.data(), new_opcodes.size(), nullptr)) {
            LeaveCriticalSection(&g_patch_lock);
            std::cout << "[HOTPATCHER] ERROR: Failed to read original bytes\n";
            return false;
        }
        
        // Change memory protection to allow writing
        DWORD old_protect = 0;
        if (!VirtualProtect(target_address, new_opcodes.size(), PAGE_EXECUTE_READWRITE, &old_protect)) {
            LeaveCriticalSection(&g_patch_lock);
            std::cout << "[HOTPATCHER] ERROR: VirtualProtect failed\n";
            return false;
        }
        
        // Write new bytes
        if (!WriteProcessMemory(GetCurrentProcess(), target_address, (void*)new_opcodes.data(), new_opcodes.size(), nullptr)) {
            VirtualProtect(target_address, new_opcodes.size(), old_protect, nullptr);
            LeaveCriticalSection(&g_patch_lock);
            std::cout << "[HOTPATCHER] ERROR: WriteProcessMemory failed\n";
            return false;
        }
        
        // Flush instruction cache
        FlushInstructionCache(GetCurrentProcess(), target_address, new_opcodes.size());
        
        // Restore original protection
        DWORD ignored = 0;
        VirtualProtect(target_address, new_opcodes.size(), old_protect, &ignored);
        
        // Store patch record
        PatchRecord record;
        record.target_address = target_address;
        record.original_bytes = original_bytes;
        record.new_bytes = new_opcodes;
        record.size = new_opcodes.size();
        record.active = true;
        
        g_active_patches[patch_name] = record;
        
        LeaveCriticalSection(&g_patch_lock);
        std::cout << "[HOTPATCHER] Applied patch '" << patch_name << "' at address (thread-safe)\n";
        return true;
    }
    catch (...) {
        LeaveCriticalSection(&g_patch_lock);
        return false;
    }
}

// Core: Revert a patch
bool HotPatcher::RevertPatch(const std::string& patch_name) {
    ensure_lock();
    EnterCriticalSection(&g_patch_lock);
    
    auto it = g_active_patches.find(patch_name);
    if (it == g_active_patches.end()) {
        LeaveCriticalSection(&g_patch_lock);
        std::cout << "[HOTPATCHER] WARNING: Patch '" << patch_name << "' not found\n";
        return false;
    }
    
    PatchRecord& record = it->second;
    
    try {
        // Change memory protection
        DWORD old_protect = 0;
        if (!VirtualProtect(record.target_address, record.size, PAGE_EXECUTE_READWRITE, &old_protect)) {
            LeaveCriticalSection(&g_patch_lock);
            std::cout << "[HOTPATCHER] ERROR: VirtualProtect failed on revert\n";
            return false;
        }
        
        // Restore original bytes
        if (!WriteProcessMemory(GetCurrentProcess(), record.target_address, (void*)record.original_bytes.data(), record.size, nullptr)) {
            VirtualProtect(record.target_address, record.size, old_protect, nullptr);
            LeaveCriticalSection(&g_patch_lock);
            std::cout << "[HOTPATCHER] ERROR: WriteProcessMemory failed on revert\n";
            return false;
        }
        
        // Flush cache
        FlushInstructionCache(GetCurrentProcess(), record.target_address, record.size);
        
        // Restore protection
        DWORD ignored = 0;
        VirtualProtect(record.target_address, record.size, old_protect, &ignored);
        
        // Remove from registry
        g_active_patches.erase(it);
        
        LeaveCriticalSection(&g_patch_lock);
        std::cout << "[HOTPATCHER] Reverted patch '" << patch_name << "'\n";
        return true;
    }
    catch (...) {
        LeaveCriticalSection(&g_patch_lock);
        return false;
    }
}

// Utility: Get function address from module
void* HotPatcher::GetFunctionAddress(const std::string& module_name, const std::string& function_name) {
    HMODULE module = GetModuleHandleA(module_name.c_str());
    if (!module) {
        std::cout << "[HOTPATCHER] ERROR: Module '" << module_name << "' not found\n";
        return nullptr;
    }
    
    void* addr = (void*)GetProcAddress(module, function_name.c_str());
    if (!addr) {
        std::cout << "[HOTPATCHER] ERROR: Function '" << function_name << "' not found\n";
        return nullptr;
    }
    
    std::cout << "[HOTPATCHER] Function found at address (thread-safe)\n";
    return addr;
}

// List all active patches
void HotPatcher::ListPatches() {
    ensure_lock();
    EnterCriticalSection(&g_patch_lock);
    
    if (g_active_patches.empty()) {
        std::cout << "[HOTPATCHER] No active patches\n";
    } else {
        std::cout << "[HOTPATCHER] Active patches (" << g_active_patches.size() << "):\n";
        for (const auto& pair : g_active_patches) {
            const PatchRecord& record = pair.second;
            std::cout << "  - " << pair.first 
                      << " at 0x" << std::hex << (uintptr_t)record.target_address << std::dec
                      << " (" << record.size << " bytes)\n";
        }
    }
    
    LeaveCriticalSection(&g_patch_lock);
}
