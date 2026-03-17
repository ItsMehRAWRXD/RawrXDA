// ═════════════════════════════════════════════════════════════════════════════
// Win32_HotpatchManager.hpp - Pure Win32 Replacement for Qt UnifiedHotpatchManager
// Zero Qt Dependencies - 100% Pure C++20 + Win32 API
// ═════════════════════════════════════════════════════════════════════════════

#pragma once

#ifndef RAWRXD_WIN32_HOTPATCH_MANAGER_HPP
#define RAWRXD_WIN32_HOTPATCH_MANAGER_HPP

#include "agent_kernel_main.hpp"
#include <functional>
#include <memory>
#include <chrono>

namespace RawrXD {
namespace Win32 {

// ═════════════════════════════════════════════════════════════════════════════
// Patch Result
// ═════════════════════════════════════════════════════════════════════════════

struct PatchResult {
    bool success = false;
    String message;
    std::chrono::system_clock::time_point timestamp;
    uint32_t bytesPatched = 0;
    
    static PatchResult Ok(const String& msg = L"") {
        PatchResult r;
        r.success = true;
        r.message = msg;
        r.timestamp = std::chrono::system_clock::now();
        return r;
    }
    
    static PatchResult Error(const String& msg) {
        PatchResult r;
        r.success = false;
        r.message = msg;
        r.timestamp = std::chrono::system_clock::now();
        return r;
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Patch Target Info
// ═════════════════════════════════════════════════════════════════════════════

struct PatchTarget {
    DWORD_PTR address = 0;
    Vector<uint8_t> originalBytes;
    Vector<uint8_t> patchedBytes;
    String targetName;
    bool applied = false;
};

// ═════════════════════════════════════════════════════════════════════════════
// Callbacks
// ═════════════════════════════════════════════════════════════════════════════

using OnLogMessageCallback = std::function<void(const String& message)>;
using OnPatchProgressCallback = std::function<void(int current, int total)>;
using OnPatchCompleteCallback = std::function<void(const PatchResult& result)>;
using OnPatchErrorCallback = std::function<void(const String& error)>;

// ═════════════════════════════════════════════════════════════════════════════
// Hotpatch Manager - Pure Win32 Implementation
// ═════════════════════════════════════════════════════════════════════════════

class HotpatchManager {
public:
    explicit HotpatchManager() : isPatching_(false) {}
    
    virtual ~HotpatchManager() = default;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Core Hotpatch Operations
    // ─────────────────────────────────────────────────────────────────────────
    
    PatchResult PerformHotpatch() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (isPatching_) {
            return PatchResult::Error(L"Hotpatch already in progress");
        }
        
        isPatching_ = true;
        
        try {
            // Register default patches if none exist
            if (patchTargets_.empty()) {
                RegisterDefaultPatches();
            }
            
            EmitLogMessage(L"Starting hotpatch process (" + std::to_wstring(patchTargets_.size()) + L" targets)");
            
            PatchResult overallResult = PatchResult::Ok();
            int successCount = 0;
            
            for (size_t i = 0; i < patchTargets_.size(); ++i) {
                if (onPatchProgress_) {
                    onPatchProgress_(static_cast<int>(i), static_cast<int>(patchTargets_.size()));
                }
                
                auto& target = patchTargets_[i];
                PatchResult result = ApplyPatch(target);
                
                if (result.success) {
                    successCount++;
                    overallResult.bytesPatched += result.bytesPatched;
                } else {
                    overallResult.success = false;
                    if (overallResult.message.empty()) {
                        overallResult.message = result.message;
                    }
                }
            }
            
            EmitLogMessage(L"Hotpatch complete: " + std::to_wstring(successCount) + L"/" + 
                          std::to_wstring(patchTargets_.size()) + L" targets patched");
            
            if (onPatchComplete_) {
                onPatchComplete_(overallResult);
            }
            
            isPatching_ = false;
            return overallResult;
        }
        catch (const std::exception& e) {
            PatchResult error = PatchResult::Error(StringUtils::FromUtf8(e.what()));
            if (onPatchError_) {
                onPatchError_(error.message);
            }
            isPatching_ = false;
            return error;
        }
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Patch Target Management
    // ─────────────────────────────────────────────────────────────────────────
    
    void RegisterPatchTarget(const PatchTarget& target) {
        std::lock_guard<std::mutex> lock(mutex_);
        patchTargets_.push_back(target);
    }
    
    void ClearPatchTargets() {
        std::lock_guard<std::mutex> lock(mutex_);
        patchTargets_.clear();
    }
    
    size_t GetPatchTargetCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return patchTargets_.size();
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Rollback Support
    // ─────────────────────────────────────────────────────────────────────────
    
    PatchResult RollbackPatches() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (patchTargets_.empty()) {
            return PatchResult::Error(L"No patches to rollback");
        }
        
        EmitLogMessage(L"Rolling back " + std::to_wstring(patchTargets_.size()) + L" patches");
        
        int rollbackCount = 0;
        for (auto& target : patchTargets_) {
            if (target.applied) {
                try {
                    DWORD oldProtect;
                    if (VirtualProtect(reinterpret_cast<void*>(target.address), 
                                      target.originalBytes.size(), 
                                      PAGE_EXECUTE_READWRITE, &oldProtect)) {
                        memcpy(reinterpret_cast<void*>(target.address), 
                               target.originalBytes.data(), 
                               target.originalBytes.size());
                        
                        VirtualProtect(reinterpret_cast<void*>(target.address), 
                                      target.originalBytes.size(), 
                                      oldProtect, &oldProtect);
                        
                        target.applied = false;
                        rollbackCount++;
                    }
                }
                catch (...) {
                    // Continue with other patches
                }
            }
        }
        
        EmitLogMessage(L"Rolled back " + std::to_wstring(rollbackCount) + L" patches");
        return PatchResult::Ok(L"Rolled back " + std::to_wstring(rollbackCount) + L" patches");
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Status & Diagnostics
    // ─────────────────────────────────────────────────────────────────────────
    
    bool IsPatching() const {
        return isPatching_.load();
    }
    
    uint32_t GetTotalBytesPatched() const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t total = 0;
        for (const auto& target : patchTargets_) {
            if (target.applied) {
                total += static_cast<uint32_t>(target.patchedBytes.size());
            }
        }
        return total;
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Callback Registration
    // ─────────────────────────────────────────────────────────────────────────
    
    void SetOnLogMessage(OnLogMessageCallback callback) {
        onLogMessage_ = callback;
    }
    
    void SetOnPatchProgress(OnPatchProgressCallback callback) {
        onPatchProgress_ = callback;
    }
    
    void SetOnPatchComplete(OnPatchCompleteCallback callback) {
        onPatchComplete_ = callback;
    }
    
    void SetOnPatchError(OnPatchErrorCallback callback) {
        onPatchError_ = callback;
    }

protected:
    // ─────────────────────────────────────────────────────────────────────────
    // Protected Methods for Subclassing
    // ─────────────────────────────────────────────────────────────────────────
    
    virtual void RegisterDefaultPatches() {
        // Subclasses can override to register specific patches
    }
    
    virtual PatchResult ApplyPatch(PatchTarget& target) {
        if (target.address == 0) {
            return PatchResult::Error(L"Invalid patch target address");
        }
        
        if (target.originalBytes.empty() || target.patchedBytes.empty()) {
            return PatchResult::Error(L"Empty patch data");
        }
        
        try {
            DWORD oldProtect;
            HANDLE hProcess = GetCurrentProcess();
            
            // Backup original bytes if not already done
            if (target.originalBytes.size() != target.patchedBytes.size()) {
                return PatchResult::Error(L"Patch data size mismatch");
            }
            
            // Make memory writable
            if (!VirtualProtect(reinterpret_cast<void*>(target.address),
                               target.patchedBytes.size(),
                               PAGE_EXECUTE_READWRITE, &oldProtect)) {
                return PatchResult::Error(L"Failed to change memory protection");
            }
            
            // Apply patch
            memcpy(reinterpret_cast<void*>(target.address),
                   target.patchedBytes.data(),
                   target.patchedBytes.size());
            
            // Restore protection
            DWORD newProtect;
            VirtualProtect(reinterpret_cast<void*>(target.address),
                          target.patchedBytes.size(),
                          oldProtect, &newProtect);
            
            // Flush instruction cache
            FlushInstructionCache(hProcess,
                                 reinterpret_cast<void*>(target.address),
                                 target.patchedBytes.size());
            
            target.applied = true;
            
            PatchResult result = PatchResult::Ok(L"Patched: " + target.targetName);
            result.bytesPatched = static_cast<uint32_t>(target.patchedBytes.size());
            
            EmitLogMessage(L"✓ " + target.targetName + L" (" + 
                          std::to_wstring(target.patchedBytes.size()) + L" bytes)");
            
            return result;
        }
        catch (const std::exception& e) {
            return PatchResult::Error(L"Exception applying patch: " + StringUtils::FromUtf8(e.what()));
        }
    }
    
    void EmitLogMessage(const String& message) {
        if (onLogMessage_) {
            std::thread([this, message]() {
                onLogMessage_(message);
            }).detach();
        }
    }

private:
    mutable std::mutex mutex_;
    std::atomic<bool> isPatching_;
    Vector<PatchTarget> patchTargets_;
    
    OnLogMessageCallback onLogMessage_;
    OnPatchProgressCallback onPatchProgress_;
    OnPatchCompleteCallback onPatchComplete_;
    OnPatchErrorCallback onPatchError_;
};

} // namespace Win32
} // namespace RawrXD

#endif // RAWRXD_WIN32_HOTPATCH_MANAGER_HPP
