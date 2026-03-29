#include "Engine.hpp"
#include <vector>
#include <algorithm>
#include <memory>
#include <cstdint>
#include <limits>

namespace {
double Clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

void* AllocateExecutableNear(void* target, size_t size) {
    const uintptr_t targetAddr = reinterpret_cast<uintptr_t>(target);
    const uintptr_t window = 0x7fff0000ULL; // ~2GB range for rel32

    uintptr_t minAddr = (targetAddr > window) ? (targetAddr - window) : 0;
    uintptr_t maxAddr = (targetAddr < (std::numeric_limits<uintptr_t>::max() - window))
        ? (targetAddr + window)
        : std::numeric_limits<uintptr_t>::max();

    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const uintptr_t gran = static_cast<uintptr_t>(si.dwAllocationGranularity);

    if (gran == 0) {
        return nullptr;
    }

    minAddr = (minAddr + gran - 1) & ~(gran - 1);

    for (uintptr_t addr = minAddr; addr < maxAddr; addr += gran) {
        void* mem = VirtualAlloc(reinterpret_cast<void*>(addr), size,
                                 MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (mem) {
            return mem;
        }
    }

    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
}
}

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
    FlushInstructionCache(GetCurrentProcess(), target_, originalCode_.size());
    installed_ = false;
    return true;
}

bool Detour::createTrampoline() {
    // Allocate trampoline close to target to keep rel32 return jump valid.
    trampoline_ = AllocateExecutableNear(target_, 64);
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
    const auto relative64 = static_cast<int64_t>(
        reinterpret_cast<char*>(to) - reinterpret_cast<char*>(from) - 5);
    if (relative64 < static_cast<int64_t>(std::numeric_limits<int32_t>::min()) ||
        relative64 > static_cast<int64_t>(std::numeric_limits<int32_t>::max())) {
        return false;
    }
    int32_t relative = static_cast<int32_t>(relative64);
    memcpy(&code[1], &relative, 4);
    FlushInstructionCache(GetCurrentProcess(), from, 5);
    
    return true;
}

// Engine implementation
Engine& Engine::instance() {
    static Engine instance;
    return instance;
}

bool Engine::installHook(const std::string& name, HookType type, void* target, void* replacement) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (hooks_.find(name) != hooks_.end() && unrestrictiveDial_ < 0.80) {
        return false; // Already exists
    }

    if (hooks_.find(name) != hooks_.end() && unrestrictiveDial_ >= 0.80) {
        // Unrestricted mode: allow replacing an existing hook definition.
        removeHook(name);
    }
    
    if (!validateTarget(target)) {
        return false;
    }
    
    HookConfig config;
    config.name = name;
    config.type = type;
    config.target = target;
    config.replacement = replacement;
    config.trampoline = nullptr;
    config.runtimeHandle = nullptr;
    config.patchSize = 0;
    config.enabled = false;
    
    // Create appropriate hook based on type
    switch (type) {
        case HookType::DETOUR:
            break;
        case HookType::PATCH:
            // Simple patch - save original code
            if (hotness_ < 0.34) {
                config.patchSize = 8;
            } else if (hotness_ < 0.67) {
                config.patchSize = 16;
            } else {
                config.patchSize = 32;
            }
            config.originalCode.resize(config.patchSize);
            memcpy(config.originalCode.data(), target, config.patchSize);
            break;
        default:
            return false; // Unsupported type
    }
    
    hooks_[name] = config;

    if (hotpatchingEnabled_) {
        return applyHook(hooks_[name]);
    }
    return true;
}

bool Engine::removeHook(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

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
    std::lock_guard<std::mutex> lock(mutex_);

    if (!hotpatchingEnabled_) {
        return false;
    }

    auto config = findHook(name);
    if (!config || config->enabled) {
        return false;
    }
    
    return applyHook(*config);
}

bool Engine::disableHook(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto config = findHook(name);
    if (!config || !config->enabled) {
        return false;
    }
    
    return removeHook(*config);
}

bool Engine::setHotpatchingEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (hotpatchingEnabled_ == enabled) {
        return true;
    }

    hotpatchingEnabled_ = enabled;
    bool allOk = true;

    for (auto& [name, config] : hooks_) {
        (void)name;
        if (enabled) {
            if (!config.enabled) {
                allOk = applyHook(config) && allOk;
            }
        } else {
            if (config.enabled) {
                allOk = removeHook(config) && allOk;
            }
        }
    }

    return allOk;
}

bool Engine::isHotpatchingEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hotpatchingEnabled_;
}

bool Engine::setModelTemperature(double temperature01) {
    std::lock_guard<std::mutex> lock(mutex_);
    modelTemperature_ = Clamp01(temperature01);
    hotness_ = modelTemperature_;
    applyTemperaturePolicyLocked();
    return true;
}

bool Engine::setUnrestrictiveDial(double dial01) {
    std::lock_guard<std::mutex> lock(mutex_);
    unrestrictiveDial_ = Clamp01(dial01);
    return true;
}

double Engine::getUnrestrictiveDial() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return unrestrictiveDial_;
}

double Engine::getModelTemperature() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return modelTemperature_;
}

double Engine::getHotness() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hotness_;
}

void Engine::applyTemperaturePolicyLocked() {
    const bool shouldEnable = modelTemperature_ > 0.10;
    if (hotpatchingEnabled_ == shouldEnable) {
        return;
    }

    hotpatchingEnabled_ = shouldEnable;
    for (auto& [name, config] : hooks_) {
        (void)name;
        if (hotpatchingEnabled_) {
            if (!config.enabled) {
                applyHook(config);
            }
        } else {
            if (config.enabled) {
                removeHook(config);
            }
        }
    }
}

bool Engine::registerHotkey(UINT vkCode, std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (hotkeys_.find(vkCode) != hotkeys_.end()) {
        return false; // Already registered
    }
    
    hotkeys_[vkCode] = callback;
    return true;
}

bool Engine::unregisterHotkey(UINT vkCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    return hotkeys_.erase(vkCode) > 0;
}

bool Engine::isHotkey(UINT vkCode) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hotkeys_.find(vkCode) != hotkeys_.end();
}

bool Engine::execute(UINT vkCode) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!hotpatchingEnabled_) {
        return false;
    }

    auto it = hotkeys_.find(vkCode);
    if (it == hotkeys_.end()) {
        return false;
    }
    
    it->second();
    return true;
}

bool Engine::installModuleHooks(HMODULE module) {
    if (!hotpatchingEnabled_) {
        return true;
    }

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
    std::lock_guard<std::mutex> lock(mutex_);

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

    // High dial means broad, context-agnostic patch acceptance.
    if (unrestrictiveDial_ >= 0.50) {
        return true;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(target, &mbi, sizeof(mbi))) {
        return false;
    }

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
    if (!hotpatchingEnabled_) {
        return false;
    }

    if (config.enabled) {
        return true; // Already applied
    }
    
    switch (config.type) {
        case HookType::DETOUR: {
            auto* detour = new Detour(config.target, config.replacement);
            if (!detour->install()) {
                delete detour;
                return false;
            }
            config.trampoline = reinterpret_cast<void*>(detour->getTrampoline<void()>());
            config.runtimeHandle = detour;
            break;
        }
        case HookType::PATCH: {
            MemoryProtection prot(config.target, config.patchSize, PAGE_EXECUTE_READWRITE);
            if (!prot.isValid()) {
                return false;
            }
            // Save original bytes before patching
            config.originalCode.resize(config.patchSize);
            memcpy(config.originalCode.data(), config.target, config.patchSize);
            // Apply the patch data over the target memory
            memcpy(config.target, config.patchData.data(),
                   std::min(config.patchSize, static_cast<size_t>(config.patchData.size())));
            FlushInstructionCache(GetCurrentProcess(), config.target, config.patchSize);
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
            auto* detour = static_cast<Detour*>(config.runtimeHandle);
            if (detour) {
                if (!detour->remove()) {
                    return false;
                }
                delete detour;
                config.runtimeHandle = nullptr;
                config.trampoline = nullptr;
            } else {
                // Best-effort fallback if runtime handle is missing.
                MemoryProtection prot(config.target, 5, PAGE_EXECUTE_READWRITE);
                if (!prot.isValid()) {
                    return false;
                }
                if (config.originalCode.size() >= 5) {
                    memcpy(config.target, config.originalCode.data(), 5);
                }
            }
            break;
        }
        case HookType::PATCH: {
            MemoryProtection prot(config.target, config.patchSize, PAGE_EXECUTE_READWRITE);
            if (!prot.isValid()) {
                return false;
            }
            memcpy(config.target, config.originalCode.data(), config.patchSize);
            FlushInstructionCache(GetCurrentProcess(), config.target, config.patchSize);
            break;
        }
        default:
            return false;
    }
    
    config.enabled = false;
    return true;
}

} // namespace RawrXD::Agentic::Hotpatch