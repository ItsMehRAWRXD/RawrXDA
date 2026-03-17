// RawrXD_MmfProducer.cpp
// Implementation of zero-copy MMF ring buffer producer

#include "RawrXD_MmfProducer.hpp"
#include "license_enforcement.h"
#include <cstring>
#include <chrono>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace RawrXD::Agentic::MMF {

constexpr uint32_t MAGIC_TOOL = 0x4C4F4F54;
constexpr uint32_t MAGIC_COMMIT = 0x544D4D43;

struct ToolMessage {
    uint32_t magic;
    uint32_t toolId;
    uint32_t payloadSize;
    uint32_t reserved;
    uint64_t correlationId;
    uint64_t timestamp;
    uint8_t payload[];
};

std::expected<std::unique_ptr<MmfProducer>, ProducerError>
MmfProducer::create(const wchar_t* name, size_t size) {
    auto producer = std::make_unique<MmfProducer>();
    if (!producer->initialize(name, size)) {
        return std::unexpected(ProducerError::InvalidState);
    }
    return producer;
}

bool MmfProducer::initialize(const wchar_t* name, size_t size) {
    // Enterprise License Gate — MMF Producer requires Enterprise tier
    if (!LicenseEnforcementGate::getInstance().checkAccess(EnterpriseFeature::MMFProducer, "initialize")) {
        return false;
    }
    hMapFile_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                   (size >> 32), (uint32_t)size, name);
    if (!hMapFile_) {
        spdlog::error("Failed to create file mapping: {}", GetLastError());
        return false;
    }
    
    pView_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!pView_) {
        CloseHandle(hMapFile_);
        spdlog::error("Failed to map view: {}", GetLastError());
        return false;
    }
    
    totalSize_ = size;
    ctrl_ = static_cast<MmfControlBlock*>(pView_);
    dataRegion_ = reinterpret_cast<uint8_t*>(ctrl_) + 4096;
    dataSize_ = size - 4096;
    
    if (ctrl_->signature != MMF_SIGNATURE) {
        ctrl_->signature = MMF_SIGNATURE;
        ctrl_->version = 1;
        ctrl_->sequence = 0;
        ctrl_->writeOffset.store(0, std::memory_order_release);
        ctrl_->readOffset.store(0, std::memory_order_release);
        ctrl_->state.store(1, std::memory_order_release);
        ctrl_->thermalZone.store(0);
        ctrl_->lastHeartbeat.store(GetTickCount64());
    }
    
    return true;
}

MmfProducer::~MmfProducer() {
    if (pView_) UnmapViewOfFile(pView_);
    if (hMapFile_) CloseHandle(hMapFile_);
}

MmfProducer::MmfProducer(MmfProducer&& other) noexcept
    : hMapFile_(other.hMapFile_), pView_(other.pView_), ctrl_(other.ctrl_),
      dataRegion_(other.dataRegion_), dataSize_(other.dataSize_),
      totalSize_(other.totalSize_), cachedWriteOffset_(other.cachedWriteOffset_),
      cachedReadOffset_(other.cachedReadOffset_),
      messagesProduced_(other.messagesProduced_.load()),
      bytesProduced_(other.bytesProduced_.load()) {
    other.hMapFile_ = nullptr;
    other.pView_ = nullptr;
    other.ctrl_ = nullptr;
}

MmfProducer& MmfProducer::operator=(MmfProducer&& other) noexcept {
    if (this != &other) {
        if (pView_) UnmapViewOfFile(pView_);
        if (hMapFile_) CloseHandle(hMapFile_);
        
        hMapFile_ = other.hMapFile_;
        pView_ = other.pView_;
        ctrl_ = other.ctrl_;
        dataRegion_ = other.dataRegion_;
        dataSize_ = other.dataSize_;
        totalSize_ = other.totalSize_;
        
        other.hMapFile_ = nullptr;
        other.pView_ = nullptr;
        other.ctrl_ = nullptr;
    }
    return *this;
}

std::expected<void*, ProducerError> MmfProducer::allocate(size_t size) {
    if (!ctrl_ || size > dataSize_ - 256) {
        return std::unexpected(ProducerError::InvalidState);
    }
    
    uint64_t readOff = ctrl_->readOffset.load(std::memory_order_acquire);
    uint64_t writeOff = ctrl_->writeOffset.load(std::memory_order_relaxed);
    
    if (writeOff >= readOff) {
        if (writeOff - readOff > dataSize_ - size) {
            if (writeOff + size > dataSize_) {
                if (readOff < size) {
                    return std::unexpected(ProducerError::InsufficientSpace);
                }
                ctrl_->writeOffset.store(0, std::memory_order_release);
                writeOff = 0;
            }
        }
    }
    
    uint32_t thermalZone = ctrl_->thermalZone.load(std::memory_order_acquire);
    if (thermalZone >= 2) {
        Sleep(10 << (thermalZone - 1));
    }
    
    ctrl_->lastHeartbeat.store(GetTickCount64(), std::memory_order_release);
    return dataRegion_ + (writeOff % dataSize_);
}

bool MmfProducer::commit(void* ptr, size_t actualSize) {
    if (!ptr || !ctrl_) return false;
    
    uint64_t writeOff = ctrl_->writeOffset.load(std::memory_order_acquire);
    uint64_t nextOffset = writeOff + actualSize;
    
    if (nextOffset > dataSize_) {
        nextOffset = nextOffset % dataSize_;
    }
    
    ctrl_->writeOffset.store(nextOffset, std::memory_order_release);
    ctrl_->writeSeq.fetch_add(1, std::memory_order_release);
    
    messagesProduced_.fetch_add(1, std::memory_order_relaxed);
    bytesProduced_.fetch_add(actualSize, std::memory_order_relaxed);
    
    return true;
}

bool MmfProducer::abort(void* ptr) {
    return true;
}

bool MmfProducer::sendToolInvocation(uint32_t toolId,
                                     std::span<const uint8_t> jsonPayload,
                                     uint64_t correlationId) {
    size_t msgSize = sizeof(ToolMessage) + jsonPayload.size();
    auto result = allocate(msgSize);
    
    if (!result) {
        spdlog::warn("Tool invocation allocation failed: {}", (int)result.error());
        return false;
    }
    
    auto* msg = new (result.value()) ToolMessage{
        .magic = MAGIC_TOOL,
        .toolId = toolId,
        .payloadSize = (uint32_t)jsonPayload.size(),
        .reserved = 0,
        .correlationId = correlationId,
        .timestamp = GetTickCount64()
    };
    
    std::memcpy(msg->payload, jsonPayload.data(), jsonPayload.size());
    
    return commit(result.value(), msgSize);
}

void MmfProducer::signalShutdown() {
    if (ctrl_) {
        ctrl_->state.store(0, std::memory_order_release);
    }
}

bool MmfProducer::isConsumerHealthy(uint64_t timeoutMs) const {
    if (!ctrl_) return false;
    
    uint64_t lastHb = ctrl_->lastHeartbeat.load(std::memory_order_acquire);
    uint64_t now = GetTickCount64();
    
    return (now - lastHb) < timeoutMs;
}

void MmfProducer::updateThermalZone(uint32_t zone) {
    if (ctrl_) {
        ctrl_->thermalZone.store(zone, std::memory_order_release);
    }
}

MmfProducer::Stats MmfProducer::getStats() const {
    return {
        messagesProduced_.load(),
        bytesProduced_.load(),
        ctrl_ ? ctrl_->writeSeq.load() / 8 : 0,
        ctrl_ ? ctrl_->thermalZone.load() : 0
    };
}

static MmfProducer* g_producer = nullptr;

MmfProducer& getGlobalProducer() {
    if (!g_producer) {
        auto result = MmfProducer::create(L"RawrXD_MMF_Producer");
        if (result) {
            g_producer = result.value().release();
        }
    }
    return *g_producer;
}

}
