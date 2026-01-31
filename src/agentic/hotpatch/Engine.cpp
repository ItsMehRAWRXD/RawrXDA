#include "Engine.hpp"
#include <vector>
#include <algorithm>
#include <memory>
#include <cstdint>

namespace RawrXD::Agentic::Hotpatch {

// MemoryProtection implementation
MemoryProtection::MemoryProtection(void* address, size_t size, DWORD newProtection)
    : address_(address), size_(size), oldProtection_(0), valid_(false) {
    
    if (!VirtualProtect(address, size, newProtection, &oldProtection_)) {
        return;
    }
    
    valid_ = true;
}

MemoryProtection::~MemoryProtection() {
    if (valid_) {
        DWORD temp;
        VirtualProtect(address_, size_, oldProtection_, &temp);
    }
}

// ShadowPage implementation
ShadowPage::ShadowPage(void* target, size_t size)
    : target_(target), size_(size), shadowAddr_(nullptr), valid_(false) {
    
    shadowAddr_ = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!shadowAddr_) {
        return;
    }
    
    valid_ = true;
}

ShadowPage::~ShadowPage() {
    if (shadowAddr_) {
        VirtualFree(shadowAddr_, 0, MEM_RELEASE);
    }
}

bool ShadowPage::copyOriginalCode() {
    if (!valid_ || !target_) {
        return false;
    }
    
    MemoryProtection prot(target_, size_, PAGE_EXECUTE_READWRITE);
    if (!prot.isValid()) {
        return false;
    }
    
    memcpy(shadowAddr_, target_, size_);
    return true;
}

bool ShadowPage::applyPatch(const std::vector<uint8_t>& patch) {
    if (!valid_ || patch.size() > size_) {
        return false;
    }
    
    MemoryProtection prot(target_, size_, PAGE_EXECUTE_READWRITE);
    if (!prot.isValid()) {
        return false;
    }
    
    memcpy(target_, patch.data(), patch.size());
    return true;
}

// Detour implementation
Detour::Detour(void* target, void* replacement)
    : target_(target), replacement_(replacement), trampoline_(nullptr), installed_(false) {
}

Detour::~Detour() {
    if (installed_) {
        remove();
    }
}

bool Detour::install() {
    if (installed_) {
        return true;
    }
    
    if (!createTrampoline()) {
        return false;
    }
    
    // Save original code
    originalCode_.resize(5); // Size of jmp instruction
    memcpy(originalCode_.data(), target_, originalCode_.size());
    
    // Apply jump to replacement
    if (!writeJump(target_, replacement_)) {
        return false;
    }
    
    installed_ = true;
    return true;
}

bool Detour::remove() {
    if (!installed_) {
        return true;
    }
    
    // Restore original code
    MemoryProtection prot(target_, originalCode_.size(), PAGE_EXECUTE_READWRITE);
    if (!prot.isValid()) {
        return false;
    }
    
    memcpy(target_, originalCode_.data(), originalCode_.size());
    installed_ = false;
    return true;
}

bool Detour::createTrampoline() {
    // Allocate trampoline memory
    trampoline_ = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline_) {
        return false;
    }
    
    // Copy original instructions
    memcpy(trampoline_, target_, 5);
    
    // Add jump back to original function + 5
    void* returnAddr = static_cast<char*>(target_) + 5;
    if (!writeJump(static_cast<char*>(trampoline_) + 5, returnAddr)) {
        VirtualFree(trampoline_, 0, MEM_RELEASE);
        trampoline_ = nullptr;
        return false;
    }
    
    return true;
}

bool Detour::writeJump(void* from, void* to) {
    MemoryProtection prot(from, 5, PAGE_EXECUTE_READWRITE);
    if (!prot.isValid()) {
        return false;
    }
    
    // Write jmp instruction
    uint8_t* code = static_cast<uint8_t*>(from);
    code[0] = 0xE9; // jmp
    int32_t relative = static_cast<char*>(to) - static_cast<char*>(from) - 5;
    memcpy(&code[1], &relative, 4);
    
    return true;
}

// Engine implementation
Engine& Engine::instance() {
    static Engine instance;
    return instance;
}

bool Engine::installHook(const std::string& name, HookType type, void* target, void* replacement) {
    if (hooks_.find(name) != hooks_.end()) {
        return false; // Already exists
    }
    
    if (!validateTarget(target)) {
        return false;
    }
    
    HookConfig config;
    config.name = name;
    config.type = type;
    config.target = target;
    config.replacement = replacement;
    config.enabled = true;
    
    // Create appropriate hook based on type
    switch (type) {
        case HookType::DETOUR: {
            auto detour = std::make_unique<Detour>(target, replacement);
            if (!detour->install()) {
                return false;
            }
            config.trampoline = reinterpret_cast<void*>(detour->getTrampoline<void()>());
            break;
        }
        case HookType::PATCH:
            // Simple patch - save original code
            config.patchSize = 16; // Default patch size
            config.originalCode.resize(config.patchSize);
            memcpy(config.originalCode.data(), target, config.patchSize);
            break;
        default:
            return false; // Unsupported type
    }
    
    hooks_[name] = config;
    return true;
}

bool Engine::removeHook(const std::string& name) {
    auto it = hooks_.find(name);
    if (it == hooks_.end()) {
        return false;
    }
    
    if (it->second.enabled) {
        removeHook(it->second);
    }
    
    hooks_.erase(it);
    return true;
}

bool Engine::enableHook(const std::string& name) {
    auto config = findHook(name);
    if (!config || config->enabled) {
        return false;
    }
    
    return applyHook(*config);
}

bool Engine::disableHook(const std::string& name) {
    auto config = findHook(name);
    if (!config || !config->enabled) {
        return false;
    }
    
    return removeHook(*config);
}

bool Engine::registerHotkey(UINT vkCode, std::function<void()> callback) {
    if (hotkeys_.find(vkCode) != hotkeys_.end()) {
        return false; // Already registered
    }
    
    hotkeys_[vkCode] = callback;
    return true;
}

bool Engine::unregisterHotkey(UINT vkCode) {
    return hotkeys_.erase(vkCode) > 0;
}

bool Engine::isHotkey(UINT vkCode) const {
    return hotkeys_.find(vkCode) != hotkeys_.end();
}

bool Engine::execute(UINT vkCode) {
    auto it = hotkeys_.find(vkCode);
    if (it == hotkeys_.end()) {
        return false;
    }
    
    it->second();
    return true;
}

bool Engine::installModuleHooks(HMODULE module) {
    // Module-wide hook installation
    // This would scan module exports and install hooks automatically
    // For now, just track the module
    return true;
}

bool Engine::removeModuleHooks(HMODULE module) {
    // Remove all hooks for this module
    return true;
}

size_t Engine::getActiveHookCount() const {
    size_t count = 0;
    for (const auto& [name, config] : hooks_) {
        if (config.enabled) {
            count++;
        }
    }
    return count;
}

bool Engine::validateTarget(void* target) const {
    if (!target) {
        return false;
    }
    
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(target, &mbi, sizeof(mbi))) {
        return false;
    }
    
    // Check if executable
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0;
}

bool Engine::isAddressWritable(void* address) const {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) {
        return false;
    }
    
    return (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;
}

HookConfig* Engine::findHook(const std::string& name) {
    auto it = hooks_.find(name);
    return it != hooks_.end() ? &it->second : nullptr;
}

bool Engine::applyHook(HookConfig& config) {
    if (config.enabled) {
        return true; // Already applied
    }
    
    switch (config.type) {
        case HookType::DETOUR: {
            auto detour = std::make_unique<Detour>(config.target, config.replacement);
            if (!detour->install()) {
                return false;
            }
            config.trampoline = reinterpret_cast<void*>(detour->getTrampoline<void()>());
            break;
        }
        case HookType::PATCH: {
            MemoryProtection prot(config.target, config.patchSize, PAGE_EXECUTE_READWRITE);
            if (!prot.isValid()) {
                return false;
            }
            // Apply patch (placeholder)
            break;
        }
        default:
            return false;
    }
    
    config.enabled = true;
    return true;
}

bool Engine::removeHook(HookConfig& config) {
    if (!config.enabled) {
        return true; // Already removed
    }
    
    switch (config.type) {
        case HookType::DETOUR: {
            // Restore original code
            MemoryProtection prot(config.target, 5, PAGE_EXECUTE_READWRITE);
            if (!prot.isValid()) {
                return false;
            }
            memcpy(config.target, config.originalCode.data(), 5);
            break;
        }
        case HookType::PATCH: {
            MemoryProtection prot(config.target, config.patchSize, PAGE_EXECUTE_READWRITE);
            if (!prot.isValid()) {
                return false;
            }
            memcpy(config.target, config.originalCode.data(), config.patchSize);
            break;
        }
        default:
            return false;
    }
    
    config.enabled = false;
    return true;
}

} // namespace RawrXD::Agentic::Hotpatch