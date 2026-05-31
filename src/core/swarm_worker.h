// =============================================================================
// swarm_worker.h — Phase 11B: Distributed Swarm Worker Node
// =============================================================================
// Worker runtime that receives compile tasks from the leader, executes them
// locally via CreateProcess, and returns results. Runs heartbeat and metrics
// reporting threads.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#ifndef RAWRXD_SWARM_WORKER_H
#define RAWRXD_SWARM_WORKER_H

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "swarm_protocol.h"
#include "swarm_types.h"

// =============================================================================
//                    SWARM WORKER CLASS
// =============================================================================

class SwarmWorker {
public:
    static SwarmWorker& instance();

    // Lifecycle
    bool start(const DscConfig& config);
    void stop();
    bool isRunning() const { return m_running.load(std::memory_order_relaxed); }

    // Connection
    bool connectToLeader(const char* ipAddress, uint16_t port);
    bool isConnected() const { return m_connected.load(std::memory_order_relaxed); }
    void disconnect();

    // Status
    uint32_t getActiveTasks() const { return m_activeTasks.load(std::memory_order_relaxed); }
    uint32_t getCompletedTasks() const { return m_completedTasks.load(std::memory_order_relaxed); }
    uint32_t getFailedTasks() const { return m_failedTasks.load(std::memory_order_relaxed); }
    uint32_t getFitnessScore() const { return m_fitnessScore; }
    std::string getStatusString() const;

    // JSON
    std::string toJson() const;

private:
    SwarmWorker();
    ~SwarmWorker();
    SwarmWorker(const SwarmWorker&) = delete;
    SwarmWorker& operator=(const SwarmWorker&) = delete;

    // Thread functions
    static DWORD WINAPI receiverThread(LPVOID param);
    static DWORD WINAPI heartbeatSenderThread(LPVOID param);
    static DWORD WINAPI taskExecutorThread(LPVOID param);

    // Packet handlers
    void handleTaskPush(const TaskPushPayload* payload, const uint8_t* extraData, uint32_t extraLen);
    void handleAttestRequest(const AttestRequestPayload* payload);
    void handleShutdownRequest();

    // Task execution
    struct WorkerTask {
        uint64_t    taskId;
        SwarmTaskType taskType;
        std::string sourceFile;
        std::string compilerArgs;
        std::string outputFile;
        uint64_t    dagGeneration;
    };

    bool executeCompileTask(const WorkerTask& task,
                             int& outExitCode,
                             std::string& outLog,
                             std::vector<uint8_t>& outObjectData);

    // Network
    bool sendPacketToLeader(SwarmOpcode opcode, const void* payload,
                             uint16_t payloadLen, uint64_t taskId = 0);
    void sendCapsReport();
    void sendHeartbeat();
    void sendResult(uint64_t taskId, int exitCode, const std::string& log,
                     const std::vector<uint8_t>& objectData);

    // Utility
    std::string getCompilerPath(SwarmTaskType type) const;

    // State
    DscConfig              m_config;
    std::atomic<bool>        m_running;
    std::atomic<bool>        m_connected;
    std::atomic<bool>        m_shutdownRequested;
    std::atomic<uint32_t>    m_activeTasks;
    std::atomic<uint32_t>    m_completedTasks;
    std::atomic<uint32_t>    m_failedTasks;
    uint32_t                 m_fitnessScore;
    uint32_t                 m_maxConcurrentTasks;
    uint32_t                 m_nodeSlotIndex;
    uint8_t                  m_localNodeId[16];

    // Network
    SOCKET                   m_leaderSocket;
    WSADATA                  m_wsaData;
    bool                     m_wsaInitialized;
    std::atomic<uint32_t>    m_sequenceCounter;
    std::atomic<uint32_t>    m_lastCompileTimeMs;   // Last compile task wall-time in ms

    // Threads
    HANDLE                   m_hReceiverThread;
    HANDLE                   m_hHeartbeatThread;
    HANDLE                   m_hTaskThreads[8];
    uint32_t                 m_taskThreadCount;

    // Task queue
    std::vector<WorkerTask>  m_taskQueue;
    std::mutex               m_taskQueueMutex;
    HANDLE                   m_taskQueueEvent;   // Signaled when new task arrives

    // Ring buffer (MASM)
    void*                    m_rxRing;
    void*                    m_rxRingBuffer;

    mutable std::mutex       m_mutex;
};

#endif // RAWRXD_SWARM_WORKER_H
