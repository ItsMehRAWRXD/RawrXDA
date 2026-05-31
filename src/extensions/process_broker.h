/**
 * @file process_broker.h
 * @brief MASM64 Extension Process Broker — C++ interface
 *
 * Provides:
 * - Out-of-process extension launching
 * - Named-pipe IPC with message framing
 * - Process lifecycle (spawn, monitor, terminate)
 * - Resource quota enforcement
 */

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <windows.h>

namespace RawrXD::Extensions {

// ============================================================================
// Process Broker Configuration
// ============================================================================

struct BrokerConfig {
    std::string pipePrefix = "\\\\.\\pipe\\RawrXD_Ext_";
    size_t maxExtensions = 64;
    size_t maxMemoryPerExtension = 256 * 1024 * 1024; // 256 MB
    uint32_t maxCpuPercent = 25; // per extension
    uint32_t pipeTimeoutMs = 5000;
    uint32_t heartbeatIntervalMs = 3000;
    uint32_t killTimeoutMs = 5000;
};

// ============================================================================
// Extension Process Info
// ============================================================================

struct ExtProcessInfo {
    int64_t extId = -1;
    HANDLE hProcess = nullptr;
    HANDLE hThread = nullptr;
    DWORD processId = 0;
    std::string pipeName;
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    bool active = false;
    size_t peakMemory = 0;
    uint64_t spawnTime = 0;
    uint64_t lastHeartbeat = 0;
};

// ============================================================================
// Message Framing
// ============================================================================

struct BrokerMessage {
    uint32_t magic = 0x5242574D; // 'RBWM'
    uint32_t type = 0;
    uint32_t payloadLen = 0;
    uint32_t crc32 = 0;
    std::vector<uint8_t> payload;
};

enum class BrokerMsgType : uint32_t {
    Handshake = 1,
    Request = 2,
    Response = 3,
    Event = 4,
    Heartbeat = 5,
    Shutdown = 6
};

// ============================================================================
// Process Broker
// ============================================================================

class ProcessBroker {
public:
    explicit ProcessBroker(const BrokerConfig& config);
    ~ProcessBroker();

    bool initialize();
    void shutdown();

    // Lifecycle
    int64_t spawnExtension(const std::string& exePath,
                           const std::string& workingDir,
                           const std::vector<std::string>& args);
    bool terminateExtension(int64_t extId);
    bool killExtension(int64_t extId);

    // Monitoring
    bool isExtensionAlive(int64_t extId) const;
    size_t getActiveCount() const;
    std::vector<int64_t> listActive() const;

    // IPC
    bool sendMessage(int64_t extId, const BrokerMessage& msg);
    bool recvMessage(int64_t extId, BrokerMessage& msg, uint32_t timeoutMs);

    // Resource quotas
    bool setMemoryLimit(int64_t extId, size_t bytes);
    bool setCpuLimit(int64_t extId, uint32_t percent);
    size_t getPeakMemory(int64_t extId) const;

    // Callbacks
    using DeathCallback = std::function<void(int64_t extId, uint32_t exitCode)>;
    void onExtensionDeath(const DeathCallback& cb);

private:
    bool createPipe(const std::string& name, HANDLE& hPipe);
    bool connectPipe(HANDLE hPipe, uint32_t timeoutMs);
    bool writeFrame(HANDLE hPipe, const BrokerMessage& msg);
    bool readFrame(HANDLE hPipe, BrokerMessage& msg, uint32_t timeoutMs);
    void monitorLoop();
    void updatePeakMemory(ExtProcessInfo& info);

    BrokerConfig m_config;
    mutable std::mutex m_mutex;
    std::map<int64_t, std::unique_ptr<ExtProcessInfo>> m_processes;
    int64_t m_nextExtId = 1;
    bool m_running = false;
    std::thread m_monitorThread;
    DeathCallback m_deathCallback;
};

} // namespace RawrXD::Extensions
