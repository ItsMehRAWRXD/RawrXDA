#include "memory_core.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "logging/logger.h"
static Logger s_logger("memory_core");

MemoryCore::MemoryCore() : max_tokens_(0), current_tokens_(0), head_index_(0), is_allocated_(false) {}

MemoryCore::~MemoryCore() {
    if (is_allocated_) {
        Deallocate();
    }
}

bool MemoryCore::Allocate(ContextTier tier) {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    
    if (is_allocated_) {
        return false; // Must deallocate first or use Reallocate
    }

    max_tokens_ = static_cast<size_t>(tier);
    
    try {
        // Reserve memory upfront to prevent fragmentation
        // We estimate 1 'block' implies roughly 1 token unit for this architecture
        buffer_.reserve(max_tokens_); 
        is_allocated_ = true;
        current_tokens_ = 0;
        head_index_ = 0;
        
        s_logger.info("[MEMORY] Heap allocation successful: ");
        return true;
    } catch (const std::bad_alloc& e) {
        s_logger.error( "[MEMORY CRITICAL] Allocation failed: " << e.what() << "\n";
        return false;
    }
}

bool MemoryCore::Deallocate() {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    if (!is_allocated_) return true;

    Wipe(); // Security wipe before release
    
    buffer_.clear();
    buffer_.shrink_to_fit(); // Force return memory to OS
    
    is_allocated_ = false;
    max_tokens_ = 0;
    current_tokens_ = 0;
    
    s_logger.info("[MEMORY] Context window released.\n");
    return true;
}

void MemoryCore::Wipe() {
    // Secure wipe: Overwrite data with zeroes before deletion
    for (auto& block : buffer_) {
        std::fill(block.content.begin(), block.content.end(), 0);
        block.content.clear();
    }
}

bool MemoryCore::Reallocate(ContextTier new_tier) {
    s_logger.info("[MEMORY] Hot-swapping context window...\n");
    
    // 1. Snapshot current context
    std::string preserved_context = RetrieveContext();
    
    // 2. Tear down old memory
    Deallocate();
    
    // 3. Allocate new tier
    if (!Allocate(new_tier)) {
        return false;
    }
    
    // 4. Reinject context
    PushContext(preserved_context);
    return true;
}

void MemoryCore::PushContext(const std::string& input) {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    if (!is_allocated_) return;

    // Simplified Tokenizer estimation: 1 word ~ 1 token for this logic
    std::stringstream ss(input);
    std::string word;
    
    while (ss >> word) {
        ContextBlock block;
        block.id = current_tokens_;
        block.content = word;
        block.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

        if (buffer_.size() < max_tokens_) {
            // Fill phase
            buffer_.push_back(block);
        } else {
            // Ring Buffer phase (Overwrite oldest)
            buffer_[head_index_] = block;
            head_index_ = (head_index_ + 1) % max_tokens_;
        }
        
        if (current_tokens_ < max_tokens_) current_tokens_++;
    }
}

std::string MemoryCore::RetrieveContext() {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    if (!is_allocated_ || buffer_.empty()) return "";

    std::stringstream context_stream;
    
    // If buffer is full, we start reading from head_index_ (oldest) to end, then 0 to head_index_
    if (buffer_.size() == max_tokens_) {
        for (size_t i = head_index_; i < max_tokens_; ++i) {
            context_stream << buffer_[i].content << " ";
        }
        for (size_t i = 0; i < head_index_; ++i) {
            context_stream << buffer_[i].content << " ";
        }
    } else {
        // Linear read if not yet wrapped
        for (const auto& block : buffer_) {
            context_stream << block.content << " ";
        }
    }
    
    return context_stream.str();
}

size_t MemoryCore::GetUsage() { return current_tokens_; }
size_t MemoryCore::GetCapacity() { return max_tokens_; }

float MemoryCore::GetUtilizationPercentage() {
    if (max_tokens_ == 0) return 0.0f;
    return (static_cast<float>(current_tokens_) / max_tokens_) * 100.0f;
}

std::string MemoryCore::GetTierName() {
    switch(static_cast<ContextTier>(max_tokens_)) {
        case ContextTier::TIER_4K: return "4K (Standard)";
        case ContextTier::TIER_32K: return "32K (Extended)";
        case ContextTier::TIER_64K: return "64K (Deep)";
        case ContextTier::TIER_128K: return "128K (Ultra)";
        case ContextTier::TIER_256K: return "256K (Max)";
        case ContextTier::TIER_512K: return "512K (Extreme)";
        case ContextTier::TIER_1M: return "1M (God Mode)";
        default: return "Custom/Unknown";
    }
}

std::vector<std::string> MemoryCore::GetRecentHistory(size_t limit) {
    std::lock_guard<std::mutex> lock(mem_mutex_);
    if (!is_allocated_ || buffer_.empty()) return {};

    std::vector<std::string> history;
    size_t count = 0;
    
    // Iterate backwards from head
    size_t current = head_index_ == 0 ? buffer_.size() - 1 : head_index_ - 1;
    
    while(count < limit && count < buffer_.size()) {
        history.push_back(buffer_[current].content);
        if (current == 0) current = buffer_.size() - 1;
        else current--;
        count++;
    }
    
    return history;
}

// Global instance for the runtime to use
MemoryCore g_memory_system;

void memory_system_init(size_t tier_size) {
    // Determine closest tier
    ContextTier tier = ContextTier::TIER_4K;
    if (tier_size >= 1048576) tier = ContextTier::TIER_1M;
    else if (tier_size >= 524288) tier = ContextTier::TIER_512K;
    else if (tier_size >= 262144) tier = ContextTier::TIER_256K;
    else if (tier_size >= 131072) tier = ContextTier::TIER_128K;
    else if (tier_size >= 65536)  tier = ContextTier::TIER_64K;
    else if (tier_size >= 32768)  tier = ContextTier::TIER_32K;
    
    g_memory_system.Allocate(tier);
}
