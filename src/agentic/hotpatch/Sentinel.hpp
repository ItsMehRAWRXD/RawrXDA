#pragma once

#include <windows.h>
#include <vector>
#include <mutex>
#include <map>
#include <atomic>
#include <thread>
#include <functional>
#include <string>
#include <chrono>

namespace RawrXD::Agentic::Hotpatch {

struct PatchSentinel {
    void* target;
    size_t size;
    uint64_t expectedHash;
    std::vector<uint8_t> patchBytes;
    std::string description;
    std::atomic<uint32_t> violationCount;
    bool selfHeal = true;

    PatchSentinel() : target(nullptr), size(0), expectedHash(0), violationCount(0) {}
    PatchSentinel(const PatchSentinel& other) : target(other.target), size(other.size), expectedHash(other.expectedHash), patchBytes(other.patchBytes), description(other.description), selfHeal(other.selfHeal) { violationCount.store(other.violationCount.load()); }
    PatchSentinel(PatchSentinel&& other) noexcept : target(other.target), size(other.size), expectedHash(other.expectedHash), patchBytes(std::move(other.patchBytes)), description(std::move(other.description)), selfHeal(other.selfHeal) { violationCount.store(other.violationCount.load()); }
    PatchSentinel& operator=(const PatchSentinel& other) { target = other.target; size = other.size; expectedHash = other.expectedHash; patchBytes = other.patchBytes; description = other.description; selfHeal = other.selfHeal; violationCount.store(other.violationCount.load()); return *this; }
    PatchSentinel& operator=(PatchSentinel&& other) noexcept { target = other.target; size = other.size; expectedHash = other.expectedHash; patchBytes = std::move(other.patchBytes); description = std::move(other.description); selfHeal = other.selfHeal; violationCount.store(other.violationCount.load()); return *this; }
};

class SentinelSystem {
public:
    static SentinelSystem& instance();

    void registerRegion(void* target, size_t size, const char* name, const uint8_t* patchData = nullptr);
    void audit();
    void startBackgroundMonitor(uint32_t intervalMs = 1000);
    void stopBackgroundMonitor();

    struct Report {
        void* address;
        std::string name;
        uint32_t violations;
        bool isCorrupt;
    };
    std::vector<Report> getStatus();
    void setHeartbeatThreshold(uint64_t cycles);
    bool checkHealth();

private:
    SentinelSystem() : running(false), heartbeatThreshold(2000000000ULL) {} // Default ~1s at 2GHz
    ~SentinelSystem() { stopBackgroundMonitor(); }

    void monitorLoop(uint32_t intervalMs);

    std::map<void*, PatchSentinel> activePatches;
    std::mutex mtx;
    std::atomic<bool> running;
    std::thread worker;
    uint64_t heartbeatThreshold;
};

} // namespace RawrXD::Agentic::Hotpatch
