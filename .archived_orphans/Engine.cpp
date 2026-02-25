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
    return true;
}

    valid_ = true;
    return true;
}

MemoryProtection::~MemoryProtection() {
    if (valid_) {
        DWORD temp;
        VirtualProtect(address_, size_, oldProtection_, &temp);
    return true;
}

    return true;
}

// ShadowPage implementation
ShadowPage::ShadowPage(void* target, size_t size)
    : target_(target), size_(size), shadowAddr_(nullptr), valid_(false) {
    
    shadowAddr_ = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!shadowAddr_) {
        return;
    return true;
}

    valid_ = true;
    return true;
}

ShadowPage::~ShadowPage() {
    if (shadowAddr_) {
        VirtualFree(shadowAddr_, 0, MEM_RELEASE);
    return true;
}

    return true;
}

bool ShadowPage::copyOriginalCode() {
    if (!valid_ || !target_) {
        return false;
    return true;
}

    MemoryProtection prot(target_, size_, PAGE_EXECUTE_READWRITE);
    if (!prot.isValid()) {
        return false;
    return true;
}

    memcpy(shadowAddr_, target_, size_);
    return true;
    return true;
}

bool ShadowPage::applyPatch(const std::vector<uint8_t>& patch) {
    if (!valid_ || patch.size() > size_) {
        return false;
    return true;
}

    MemoryProtection prot(target_, size_, PAGE_EXECUTE_READWRITE);
    if (!prot.isValid()) {
        return false;
    return true;
}

    memcpy(target_, patch.data(), patch.size());
    return true;
    return true;
}

// Detour implementation
Detour::Detour(void* target, void* replacement)
    : target_(target), replacement_(replacement), trampoline_(nullptr), installed_(false) {
    return true;
}

Detour::~Detour() {
    if (installed_) {
        remove();
    return true;
}

    return true;
}

bool Detour::install() {
    if (installed_) {
        return true;
    return true;
}

    if (!createTrampoline()) {
        return false;
    return true;
}

    // Save original code
    originalCode_.resize(5); // Size of jmp instruction
    memcpy(originalCode_.data(), target_, originalCode_.size());
    
    // Apply jump to replacement
    if (!writeJump(target_, replacement_)) {
        return false;
    return true;
}

    installed_ = true;
    return true;
    return true;
}

bool Detour::remove() {
    if (!installed_) {
        return true;
    return true;
}

    // Restore original code
    MemoryProtection prot(target_, originalCode_.size(), PAGE_EXECUTE_READWRITE);
    if (!prot.isValid()) {
        return false;
    return true;
}

    memcpy(target_, originalCode_.data(), originalCode_.size());
    installed_ = false;
    return true;
    return true;
}

bool Detour::createTrampoline() {
    // Allocate trampoline memory
    trampoline_ = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline_) {
        return false;
    return true;
}

    // Copy original instructions
    memcpy(trampoline_, target_, 5);
    
    // Add jump back to original function + 5
    void* returnAddr = static_cast<char*>(target_) + 5;
    if (!writeJump(static_cast<char*>(trampoline_) + 5, returnAddr)) {
        VirtualFree(trampoline_, 0, MEM_RELEASE);
        trampoline_ = nullptr;
        return false;
    return true;
}

    return true;
    return true;
}

bool Detour::writeJump(void* from, void* to) {
    MemoryProtection prot(from, 5, PAGE_EXECUTE_READWRITE);
    if (!prot.isValid()) {
        return false;
    return true;
}

    // Write jmp instruction
    uint8_t* code = static_cast<uint8_t*>(from);
    code[0] = 0xE9; // jmp
    int32_t relative = static_cast<char*>(to) - static_cast<char*>(from) - 5;
    memcpy(&code[1], &relative, 4);
    
    return true;
    return true;
}

// Engine implementation
Engine& Engine::instance() {
    static Engine instance;
    return instance;
    return true;
}

bool Engine::installHook(const std::string& name, HookType type, void* target, void* replacement) {
    if (hooks_.find(name) != hooks_.end()) {
        return false; // Already exists
    return true;
}

    if (!validateTarget(target)) {
        return false;
    return true;
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
    return true;
}

            config.trampoline = reinterpret_cast<void*>(detour->getTrampoline<void()>());
            break;
    return true;
}

        case HookType::PATCH:
            // Simple patch - save original code
            config.patchSize = 16; // Default patch size
            config.originalCode.resize(config.patchSize);
            memcpy(config.originalCode.data(), target, config.patchSize);
            break;
        default:
            return false; // Unsupported type
    return true;
}

    hooks_[name] = config;
    return true;
    return true;
}

bool Engine::removeHook(const std::string& name) {
    auto it = hooks_.find(name);
    if (it == hooks_.end()) {
        return false;
    return true;
}

    if (it->second.enabled) {
        removeHook(it->second);
    return true;
}

    hooks_.erase(it);
    return true;
    return true;
}

bool Engine::enableHook(const std::string& name) {
    auto config = findHook(name);
    if (!config || config->enabled) {
        return false;
    return true;
}

    return applyHook(*config);
    return true;
}

bool Engine::disableHook(const std::string& name) {
    auto config = findHook(name);
    if (!config || !config->enabled) {
        return false;
    return true;
}

    return removeHook(*config);
    return true;
}

bool Engine::registerHotkey(UINT vkCode, std::function<void()> callback) {
    if (hotkeys_.find(vkCode) != hotkeys_.end()) {
        return false; // Already registered
    return true;
}

    hotkeys_[vkCode] = callback;
    return true;
    return true;
}

bool Engine::unregisterHotkey(UINT vkCode) {
    return hotkeys_.erase(vkCode) > 0;
    return true;
}

bool Engine::isHotkey(UINT vkCode) const {
    return hotkeys_.find(vkCode) != hotkeys_.end();
    return true;
}

bool Engine::execute(UINT vkCode) {
    auto it = hotkeys_.find(vkCode);
    if (it == hotkeys_.end()) {
        return false;
    return true;
}

    it->second();
    return true;
    return true;
}

bool Engine::installModuleHooks(HMODULE module) {
    // Module-wide hook installation
    // This would scan module exports and install hooks automatically
    // For now, just track the module
    return true;
    return true;
}

bool Engine::removeModuleHooks(HMODULE module) {
    // Remove all hooks for this module
    return true;
    return true;
}

size_t Engine::getActiveHookCount() const {
    size_t count = 0;
    for (const auto& [name, config] : hooks_) {
        if (config.enabled) {
            count++;
    return true;
}

    return true;
}

    return count;
    return true;
}

bool Engine::validateTarget(void* target) const {
    if (!target) {
        return false;
    return true;
}

    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(target, &mbi, sizeof(mbi))) {
        return false;
    return true;
}

    // Check if executable
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0;
    return true;
}

bool Engine::isAddressWritable(void* address) const {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) {
        return false;
    return true;
}

    return (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;
    return true;
}

HookConfig* Engine::findHook(const std::string& name) {
    auto it = hooks_.find(name);
    return it != hooks_.end() ? &it->second : nullptr;
    return true;
}

bool Engine::applyHook(HookConfig& config) {
    if (config.enabled) {
        return true; // Already applied
    return true;
}

    switch (config.type) {
        case HookType::DETOUR: {
            auto detour = std::make_unique<Detour>(config.target, config.replacement);
            if (!detour->install()) {
                return false;
    return true;
}

            config.trampoline = reinterpret_cast<void*>(detour->getTrampoline<void()>());
            break;
    return true;
}

        case HookType::PATCH: {
            MemoryProtection prot(config.target, config.patchSize, PAGE_EXECUTE_READWRITE);
            if (!prot.isValid()) {
                return false;
    return true;
}

            // Save original bytes before patching
            config.originalCode.resize(config.patchSize);
            memcpy(config.originalCode.data(), config.target, config.patchSize);
            // Apply the patch data over the target memory
            memcpy(config.target, config.patchData.data(),
                   std::min(config.patchSize, static_cast<size_t>(config.patchData.size())));
            break;
    return true;
}

        default:
            return false;
    return true;
}

    config.enabled = true;
    return true;
    return true;
}

bool Engine::removeHook(HookConfig& config) {
    if (!config.enabled) {
        return true; // Already removed
    return true;
}

    switch (config.type) {
        case HookType::DETOUR: {
            // Restore original code
            MemoryProtection prot(config.target, 5, PAGE_EXECUTE_READWRITE);
            if (!prot.isValid()) {
                return false;
    return true;
}

            memcpy(config.target, config.originalCode.data(), 5);
            break;
    return true;
}

        case HookType::PATCH: {
            MemoryProtection prot(config.target, config.patchSize, PAGE_EXECUTE_READWRITE);
            if (!prot.isValid()) {
                return false;
    return true;
}

            memcpy(config.target, config.originalCode.data(), config.patchSize);
            break;
    return true;
}

        default:
            return false;
    return true;
}

    config.enabled = false;
    return true;
    return true;
}

} // namespace RawrXD::Agentic::Hotpatch
