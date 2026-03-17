// RawrXD_MmfProducer.hpp
// Zero-copy, cache-line-aligned MMF ring buffer for Phase-3

#pragma once
#include <windows.h>
#include <atomic>
#include <memory>
#include <expected>
#include <span>

namespace RawrXD::Agentic::MMF {

constexpr size_t CACHE_LINE_SIZE = 64;
constexpr size_t MMF_DEFAULT_SIZE = 256 * 1024 * 1024;
constexpr uint32_t MMF_SIGNATURE = 0x52415752;

struct alignas(64) MmfControlBlock {
    uint32_t signature;
    uint32_t version;
    uint64_t sequence;
    
    alignas(64) std::atomic<uint64_t> writeOffset;
    std::atomic<uint64_t> writeSeq;
    
    alignas(64) std::atomic<uint64_t> readOffset;
    std::atomic<uint64_t> readSeq;
    
    alignas(64) std::atomic<uint32_t> state;
    std::atomic<uint32_t> thermalZone;
    std::atomic<uint64_t> lastHeartbeat;
    
    uint8_t reserved[256 - 64];
};

enum class ProducerError {
    Success = 0,
    InsufficientSpace,
    InvalidState,
    ThermalThrottling,
    Timeout,
    IntegrityFailure,
    ConsumerLagging
};

class MmfProducer {
public:
    static std::expected<std::unique_ptr<MmfProducer>, ProducerError>
        create(const wchar_t* name, size_t size = MMF_DEFAULT_SIZE);
    
    ~MmfProducer();
    
    MmfProducer(const MmfProducer&) = delete;
    MmfProducer& operator=(const MmfProducer&) = delete;
    MmfProducer(MmfProducer&&) noexcept;
    MmfProducer& operator=(MmfProducer&&) noexcept;

    std::expected<void*, ProducerError> allocate(size_t size);
    bool commit(void* ptr, size_t actualSize);
    bool abort(void* ptr);
    
    bool sendToolInvocation(uint32_t toolId, 
                           std::span<const uint8_t> jsonPayload,
                           uint64_t correlationId = 0);
    
    void signalShutdown();
    bool isConsumerHealthy(uint64_t timeoutMs = 1000) const;
    void updateThermalZone(uint32_t zone);
    
    struct Stats {
        uint64_t messagesProduced;
        uint64_t bytesProduced;
        uint64_t batchCommits;
        uint64_t thermalDelays;
    };
    Stats getStats() const;

private:
    MmfProducer() = default;
    
    bool initialize(const wchar_t* name, size_t size);
    
    HANDLE hMapFile_ = nullptr;
    void* pView_ = nullptr;
    MmfControlBlock* ctrl_ = nullptr;
    uint8_t* dataRegion_ = nullptr;
    size_t dataSize_ = 0;
    size_t totalSize_ = 0;
    
    uint64_t cachedWriteOffset_ = 0;
    uint64_t cachedReadOffset_ = 0;
    
    mutable std::atomic<uint64_t> messagesProduced_{0};
    mutable std::atomic<uint64_t> bytesProduced_{0};
};

MmfProducer& getGlobalProducer();

} // namespace
