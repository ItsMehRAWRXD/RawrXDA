#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <windows.h>
#include <memory>
#include <cstdint>
#include <mutex>

namespace RawrXD::Agentic::Hotpatch {

// Hotpatch hook types
enum class HookType {
    DETOUR,     // Function redirection
    PATCH,      // Code modification
    TRAMPOLINE, // Call redirection
    VTABLE      // Virtual table hook
};

// Hook configuration
struct HookConfig {
    std::string name;
    HookType type;
    void* target;
    void* replacement;
    void* trampoline;
    void* runtimeHandle;
    size_t patchSize;
    bool enabled;
    std::vector<uint8_t> patchData;
    std::vector<uint8_t> originalCode;
};

// Memory protection wrapper
class MemoryProtection {
public:
    MemoryProtection(void* address, size_t size, DWORD newProtection);
    ~MemoryProtection();
    
    bool isValid() const { return valid_; }
    
private:
    void* address_;
    size_t size_;
    DWORD oldProtection_;
    bool valid_;
};

// Shadow page for safe code modification
class ShadowPage {
public:
    ShadowPage(void* target, size_t size);
    ~ShadowPage();
    
    void* getShadowAddress() const { return shadowAddr_; }
    bool isValid() const { return valid_; }
    
    bool copyOriginalCode();
    bool applyPatch(const std::vector<uint8_t>& patch);
    
private:
    void* target_;
    void* shadowAddr_;
    size_t size_;
    bool valid_;
};

// Detour implementation
class Detour {
public:
    Detour(void* target, void* replacement);
    ~Detour();
    
    bool install();
    bool remove();
    bool isInstalled() const { return installed_; }
    
    template<typename Func>
    Func* getTrampoline() const {
        return reinterpret_cast<Func*>(trampoline_);
    }
    
private:
    void* target_;
    void* replacement_;
    void* trampoline_;
    bool installed_;
    std::vector<uint8_t> originalCode_;
    
    bool createTrampoline();
    bool writeJump(void* from, void* to);
};

// Main hotpatch engine
class Engine {
public:
    static Engine& instance();
    
    // Hook management
    bool installHook(const std::string& name, HookType type, void* target, void* replacement);
    bool removeHook(const std::string& name);
    bool enableHook(const std::string& name);
    bool disableHook(const std::string& name);

    // Global hotpatch toggle.
    bool setHotpatchingEnabled(bool enabled);
    bool isHotpatchingEnabled() const;

    // Temperature-driven mode: colder -> conservative/off, hotter -> aggressive.
    bool setModelTemperature(double temperature01);
    double getModelTemperature() const;
    double getHotness() const;

    // Unrestrictive dial: 0.0 = strict checks, 1.0 = unrestricted behavior.
    bool setUnrestrictiveDial(double dial01);
    double getUnrestrictiveDial() const;
    
    // Hotkey integration
    bool registerHotkey(UINT vkCode, std::function<void()> callback);
    bool unregisterHotkey(UINT vkCode);
    bool isHotkey(UINT vkCode) const;
    bool execute(UINT vkCode);
    
    // Module hooks
    bool installModuleHooks(HMODULE module);
    bool removeModuleHooks(HMODULE module);
    
    // Statistics
    size_t getHookCount() const { return hooks_.size(); }
    size_t getActiveHookCount() const;
    
    // Safety
    bool validateTarget(void* target) const;
    bool isAddressWritable(void* address) const;
    
private:
    Engine() = default;
    void applyTemperaturePolicyLocked();

    mutable std::mutex mutex_;
    bool hotpatchingEnabled_ = true;
    double modelTemperature_ = 0.5;
    double hotness_ = 0.5;
    double unrestrictiveDial_ = 1.0;
    std::map<std::string, HookConfig> hooks_;
    std::map<UINT, std::function<void()>> hotkeys_;
    
    HookConfig* findHook(const std::string& name);
    bool applyHook(HookConfig& config);
    bool removeHook(HookConfig& config);
};

} // namespace RawrXD::Agentic::Hotpatch