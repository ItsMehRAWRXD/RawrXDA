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
}

MemoryManager::~MemoryManager() = default;

bool MemoryManager::SetContextSize(ContextSize size) {
    return SetContextSize(static_cast<size_t>(size));
}

bool MemoryManager::SetContextSize(size_t size) {
    if (!IsSizeSupported(size)) {
        return false;
    }
    current_context_size_ = size;
    return true;
}

std::string MemoryManager::ProcessWithContext(const std::string& prompt) {
    auto it = modules_.find(current_context_size_);
    if (it != modules_.end() && it->second) {
        return it->second->Process(prompt, current_context_size_);
    }
    return "Memory module not available for context size: " + std::to_string(current_context_size_);
}

std::vector<MemoryManager::ContextSize> MemoryManager::GetAvailableSizes() {
    std::vector<ContextSize> sizes;
    for (const auto& pair : modules_) {
        sizes.push_back(static_cast<ContextSize>(pair.first));
    }
    std::sort(sizes.begin(), sizes.end());
    return sizes;
}

void MemoryManager::RegisterModule(size_t size, std::unique_ptr<MemoryModule> module) {
    modules_[size] = std::move(module);
}

MemoryModule* MemoryManager::GetModule(size_t size) {
    auto it = modules_.find(size);
    if (it != modules_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool MemoryManager::IsSizeSupported(size_t size) const {
    return size <= max_supported_size_ && modules_.find(size) != modules_.end();
}

// Concrete implementations
std::string StandardMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 4K tokens (approximate as characters/4)
    size_t max_chars = 4096 * 4;
    if (prompt.size() > max_chars) {
        return prompt.substr(0, max_chars);
    }
    return prompt;
}

std::string ExtendedMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 32K tokens (approximate as characters/4)
    size_t max_chars = 32768 * 4;
    if (prompt.size() > max_chars) {
        return prompt.substr(0, max_chars);
    }
    return prompt;
}

std::string LargeMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 64K tokens (approximate as characters/4)
    size_t max_chars = 65536 * 4;
    if (prompt.size() > max_chars) {
        return prompt.substr(0, max_chars);
    }
    return prompt;
}

std::string HugeMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 128K tokens (approximate as characters/4)
    size_t max_chars = 131072 * 4;
    if (prompt.size() > max_chars) {
        return prompt.substr(0, max_chars);
    }
    return prompt;
}

std::string MassiveMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 256K tokens (approximate as characters/4)
    size_t max_chars = 262144 * 4;
    if (prompt.size() > max_chars) {
        return prompt.substr(0, max_chars);
    }
    return prompt;
}

std::string GiganticMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 512K tokens (approximate as characters/4)
    size_t max_chars = 524288 * 4;
    if (prompt.size() > max_chars) {
        return prompt.substr(0, max_chars);
    }
    return prompt;
}

std::string UltimateMemoryModule::Process(const std::string& prompt, size_t context_size) {
    // Truncate to 1M tokens (approximate as characters/4)
    size_t max_chars = 1048576 * 4;
    if (prompt.size() > max_chars) {
        return prompt.substr(0, max_chars);
    }
    return prompt;
}
