#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

class MemoryModule {
public:
    virtual ~MemoryModule() = default;
    virtual std::string Process(const std::string& prompt, size_t context_size) = 0;
    virtual bool SupportsSize(size_t context_size) = 0;
    virtual std::string GetName() = 0;
    virtual size_t GetMaxTokens() = 0;
};

class MemoryManager {
private:
    std::unordered_map<size_t, std::unique_ptr<MemoryModule>> modules_;
    size_t current_context_size_ = 4096;
    size_t max_supported_size_ = 1048576; // 1M tokens
    
public:
    enum ContextSize {
        CTX_4K = 4096,
        CTX_32K = 32768,
        CTX_64K = 65536,
        CTX_128K = 131072,
        CTX_256K = 262144,
        CTX_512K = 524288,
        CTX_1M = 1048576
    };
    
    MemoryManager();
    ~MemoryManager();
    
    bool SetContextSize(ContextSize size);
    bool SetContextSize(size_t size);
    size_t GetCurrentContextSize() const { return current_context_size_; }
    size_t GetMaxSupportedSize() const { return max_supported_size_; }
    
    std::string ProcessWithContext(const std::string& prompt);
    std::vector<ContextSize> GetAvailableSizes();
    
    // Register memory modules
    void RegisterModule(size_t size, std::unique_ptr<MemoryModule> module);
    
    // Get module for size
    MemoryModule* GetModule(size_t size);
    
    // Check if size is supported
    bool IsSizeSupported(size_t size) const;
};

// Concrete memory modules
class StandardMemoryModule : public MemoryModule {
public:
    std::string Process(const std::string& prompt, size_t context_size) override;
    bool SupportsSize(size_t context_size) override { return context_size <= 4096; }
    std::string GetName() override { return "Standard (4K)"; }
    size_t GetMaxTokens() override { return 4096; }
};

class ExtendedMemoryModule : public MemoryModule {
public:
    std::string Process(const std::string& prompt, size_t context_size) override;
    bool SupportsSize(size_t context_size) override { return context_size <= 32768; }
    std::string GetName() override { return "Extended (32K)"; }
    size_t GetMaxTokens() override { return 32768; }
};

class LargeMemoryModule : public MemoryModule {
public:
    std::string Process(const std::string& prompt, size_t context_size) override;
    bool SupportsSize(size_t context_size) override { return context_size <= 65536; }
    std::string GetName() override { return "Large (64K)"; }
    size_t GetMaxTokens() override { return 65536; }
};

class HugeMemoryModule : public MemoryModule {
public:
    std::string Process(const std::string& prompt, size_t context_size) override;
    bool SupportsSize(size_t context_size) override { return context_size <= 131072; }
    std::string GetName() override { return "Huge (128K)"; }
    size_t GetMaxTokens() override { return 131072; }
};

class MassiveMemoryModule : public MemoryModule {
public:
    std::string Process(const std::string& prompt, size_t context_size) override;
    bool SupportsSize(size_t context_size) override { return context_size <= 262144; }
    std::string GetName() override { return "Massive (256K)"; }
    size_t GetMaxTokens() override { return 262144; }
};

class GiganticMemoryModule : public MemoryModule {
public:
    std::string Process(const std::string& prompt, size_t context_size) override;
    bool SupportsSize(size_t context_size) override { return context_size <= 524288; }
    std::string GetName() override { return "Gigantic (512K)"; }
    size_t GetMaxTokens() override { return 524288; }
};

class UltimateMemoryModule : public MemoryModule {
public:
    std::string Process(const std::string& prompt, size_t context_size) override;
    bool SupportsSize(size_t context_size) override { return context_size <= 1048576; }
    std::string GetName() override { return "Ultimate (1M)"; }
    size_t GetMaxTokens() override { return 1048576; }
};
