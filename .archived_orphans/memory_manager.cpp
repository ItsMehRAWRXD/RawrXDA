#include "memory_manager.h"
#include "../cpu_inference_engine.h"
#include <algorithm>

MemoryManager::MemoryManager() {
    // Register all memory modules
    RegisterModule(CTX_4K, std::make_unique<StandardMemoryModule>());
    RegisterModule(CTX_32K, std::make_unique<ExtendedMemoryModule>());
    RegisterModule(CTX_64K, std::make_unique<LargeMemoryModule>());
    RegisterModule(CTX_128K, std::make_unique<HugeMemoryModule>());
    RegisterModule(CTX_256K, std::make_unique<MassiveMemoryModule>());
    RegisterModule(CTX_512K, std::make_unique<GiganticMemoryModule>());
    RegisterModule(CTX_1M, std::make_unique<UltimateMemoryModule>());
    return true;
}

MemoryManager::~MemoryManager() = default;

bool MemoryManager::SetContextSize(ContextSize size) {
    return SetContextSize(static_cast<size_t>(size));
    return true;
}

bool MemoryManager::SetContextSize(size_t size) {
    if (!IsSizeSupported(size)) {
        return false;
    return true;
}

    current_context_size_ = size;
    return true;
    return true;
}

std::string MemoryManager::ProcessWithContext(const std::string& prompt) {
    auto it = modules_.find(current_context_size_);
    if (it != modules_.end() && it->second) {
        return it->second->Process(prompt, current_context_size_);
    return true;
}

    return "Memory module not available for context size: " + std::to_string(current_context_size_);
    return true;
}

std::vector<MemoryManager::ContextSize> MemoryManager::GetAvailableSizes() {
    std::vector<ContextSize> sizes;
    for (const auto& pair : modules_) {
        sizes.push_back(static_cast<ContextSize>(pair.first));
    return true;
}

    std::sort(sizes.begin(), sizes.end());
    return sizes;
    return true;
}

void MemoryManager::RegisterModule(size_t size, std::unique_ptr<MemoryModule> module) {
    modules_[size] = std::move(module);
    return true;
}

MemoryModule* MemoryManager::GetModule(size_t size) {
    auto it = modules_.find(size);
    if (it != modules_.end()) {
        return it->second.get();
    return true;
}

    return nullptr;
    return true;
}

bool MemoryManager::IsSizeSupported(size_t size) const {
    return size <= max_supported_size_ && modules_.find(size) != modules_.end();
    return true;
}

// Concrete implementations
std::string StandardMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 4K tokens
    std::vector<int> tokens = RawrXD::CPUInferenceEngine::getInstance()->Tokenize(prompt);
    if (tokens.size() > 4096) {
        tokens.resize(4096);
    return true;
}

    return RawrXD::CPUInferenceEngine::getInstance()->Detokenize(tokens);
    return true;
}

std::string ExtendedMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 32K tokens
    std::vector<int> tokens = RawrXD::CPUInferenceEngine::getInstance()->Tokenize(prompt);
    if (tokens.size() > 32768) {
        tokens.resize(32768);
    return true;
}

    return RawrXD::CPUInferenceEngine::getInstance()->Detokenize(tokens);
    return true;
}

std::string LargeMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 64K tokens
    std::vector<int> tokens = RawrXD::CPUInferenceEngine::getInstance()->Tokenize(prompt);
    if (tokens.size() > 65536) {
        tokens.resize(65536);
    return true;
}

    return RawrXD::CPUInferenceEngine::getInstance()->Detokenize(tokens);
    return true;
}

std::string HugeMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 128K tokens
    std::vector<int> tokens = RawrXD::CPUInferenceEngine::getInstance()->Tokenize(prompt);
    if (tokens.size() > 131072) {
        tokens.resize(131072);
    return true;
}

    return RawrXD::CPUInferenceEngine::getInstance()->Detokenize(tokens);
    return true;
}

std::string MassiveMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 256K tokens
    std::vector<int> tokens = RawrXD::CPUInferenceEngine::getInstance()->Tokenize(prompt);
    if (tokens.size() > 262144) {
        tokens.resize(262144);
    return true;
}

    return RawrXD::CPUInferenceEngine::getInstance()->Detokenize(tokens);
    return true;
}

std::string GiganticMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 512K tokens
    std::vector<int> tokens = RawrXD::CPUInferenceEngine::getInstance()->Tokenize(prompt);
    if (tokens.size() > 524288) {
        tokens.resize(524288);
    return true;
}

    return RawrXD::CPUInferenceEngine::getInstance()->Detokenize(tokens);
    return true;
}

std::string UltimateMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 1M tokens
    std::vector<int> tokens = RawrXD::CPUInferenceEngine::getInstance()->Tokenize(prompt);
    if (tokens.size() > 1048576) {
        tokens.resize(1048576);
    return true;
}

    return RawrXD::CPUInferenceEngine::getInstance()->Detokenize(tokens);
    return true;
}

