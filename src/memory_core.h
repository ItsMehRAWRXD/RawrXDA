#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <memory>

// Defined Context Tiers (Token Counts)
enum class ContextTier : size_t {
    TIER_4K   = 4096,
    TIER_32K  = 32768,
    TIER_64K  = 65536,
    TIER_128K = 131072,
    TIER_256K = 262144,
    TIER_512K = 524288,
    TIER_1M   = 1048576
};

struct ContextBlock {
    size_t id;
    std::string content; // Raw text content
    // In a full implementation, this would also hold vector embeddings (std::vector<float>)
    long long timestamp;
};

class MemoryCore {
private:
    std::vector<ContextBlock> buffer_;
    size_t max_tokens_;
    size_t current_tokens_;
    size_t head_index_; // For Ring Buffer logic
    std::mutex mem_mutex_;
    bool is_allocated_;

public:
    MemoryCore();
    ~MemoryCore();

    // Lifecycle
    bool Allocate(ContextTier tier);
    bool Deallocate();
    bool Reallocate(ContextTier new_tier); // Hot-swap

    // Operations
    void PushContext(const std::string& input);
    std::string RetrieveContext(); // Flattens the ring buffer
    
    // Diagnostics
    size_t GetUsage();
    size_t GetCapacity();
    float GetUtilizationPercentage();
    std::string GetTierName();
    
    // Safety
    void Wipe(); // Secure zero-fill

    // React Server support
    std::vector<std::string> GetRecentHistory(size_t limit);
};
