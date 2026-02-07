// =============================================================================
// swarm_coordinator.h — Phase 11A: Distributed Swarm Coordinator (Leader)
// =============================================================================
// The SwarmCoordinator is the leader singleton that manages the distributed
// build cluster. It owns the task DAG, node table, scheduling, consensus,
// discovery, and failure recovery.
//
// Thread model:
//   - Main thread (IDE) calls start/stop/getStats
//   - Listener thread accepts TCP connections
//   - IOCP thread pool processes incoming packets
//   - Heartbeat thread monitors node health
//   - Discovery thread sends/receives UDP broadcasts
//   - Scheduler thread assigns tasks to ready workers
//
// All shared state protected by std::mutex. No recursive locks.
// Follows ExecutionGovernor singleton pattern from Phase 10.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#ifndef RAWRXD_SWARM_COORDINATOR_H
#define RAWRXD_SWARM_COORDINATOR_H

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "swarm_protocol.h"
#include "swarm_types.h"

// Forward declarations
class Win32IDE;

// =============================================================================
//                    CALLBACK TYPES (no std::function in hot path)
// =============================================================================

// Event callback: fired for every SwarmEvent (node join, task complete, etc.)
typedef void (*SwarmEventCallback)(const SwarmEvent& event, void* userData);

// Log callback: fired for build log lines from remote workers
typedef void (*SwarmLogCallback)(uint64_t taskId, uint32_t nodeSlot,
                                  const char* logLine, void* userData);

// =============================================================================
//                    SWARM COORDINATOR CLASS
// =============================================================================

class SwarmCoordinator {
public:
    // -------------------------------------------------------------------------
    // Singleton access (follows ExecutionGovernor pattern)
    // -------------------------------------------------------------------------
    static SwarmCoordinator& instance();

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------
    bool start(const DscConfig& config);
    void stop();
    bool isRunning() const { return m_running.load(std::memory_order_relaxed); }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------
    void setConfig(const DscConfig& config);
    DscConfig getConfig() const;
    SwarmMode getMode() const { return m_config.mode; }

    // -------------------------------------------------------------------------
    // Node Management
    // -------------------------------------------------------------------------

    // Get all known nodes.
    std::vector<SwarmNodeInfo> getNodes() const;

    // Get a specific node by slot index.
    bool getNode(uint32_t slotIndex, SwarmNodeInfo& outNode) const;

    // Get number of online nodes.
    uint32_t getOnlineNodeCount() const;

    // Manually add a node by IP:port (bypasses discovery).
    bool addNodeManual(const char* ipAddress, uint16_t port);

    // Remove a node from the cluster.
    bool removeNode(uint32_t slotIndex);

    // Blacklist a node (permanent ban until restart).
    bool blacklistNode(uint32_t slotIndex, const char* reason);

    // -------------------------------------------------------------------------
    // Task Graph (DAG) Management
    // -------------------------------------------------------------------------

    // Build a DAG from a CMakeLists.txt or compile_commands.json.
    bool buildDagFromCMake(const char* buildDir);

    // Build a DAG from an explicit list of source files.
    bool buildDagFromSources(const std::vector<std::string>& sourceFiles,
                              const std::string& compilerArgs,
                              const std::string& outputDir);

    // Add a single task to the current DAG.
    uint64_t addTask(SwarmTaskType type,
                      const std::string& sourceFile,
                      const std::string& compilerArgs,
                      const std::string& outputFile,
                      const std::vector<uint64_t>& dependencies = {});

    // Cancel a task (and optionally all dependents).
    bool cancelTask(uint64_t taskId, bool cancelDependents = true);

    // Cancel the entire build.
    void cancelAllTasks();

    // Get task info by ID.
    bool getTask(uint64_t taskId, SwarmTasklet& outTask) const;

    // Get current DAG generation.
    uint64_t getDagGeneration() const;

    // -------------------------------------------------------------------------
    // Build Execution
    // -------------------------------------------------------------------------

    // Start executing the current DAG (distributes tasks to workers).
    bool startBuild();

    // Check if a build is in progress.
    bool isBuildRunning() const { return m_buildRunning.load(std::memory_order_relaxed); }

    // Get build progress (0.0 - 1.0).
    double getBuildProgress() const;

    // -------------------------------------------------------------------------
    // Object Cache
    // -------------------------------------------------------------------------

    // Check if a content hash is in the local cache.
    bool objectCacheLookup(uint64_t contentHash, ObjectCacheEntry& outEntry) const;

    // Store a compiled object in the cache.
    void objectCacheStore(uint64_t contentHash, uint64_t resultHash,
                           const void* objectData, uint32_t objectSize,
                           const std::string& sourceFile);

    // Clear the object cache.
    void objectCacheClear();

    // Get cache size (number of entries).
    uint32_t objectCacheSize() const;

    // -------------------------------------------------------------------------
    // Statistics
    // -------------------------------------------------------------------------
    SwarmStats getStats() const;
    void resetStats();

    // -------------------------------------------------------------------------
    // Events
    // -------------------------------------------------------------------------
    void setEventCallback(SwarmEventCallback callback, void* userData);
    void setLogCallback(SwarmLogCallback callback, void* userData);

    // Get recent events (up to maxCount).
    std::vector<SwarmEvent> getRecentEvents(uint32_t maxCount = 50) const;

    // -------------------------------------------------------------------------
    // Discovery Control
    // -------------------------------------------------------------------------
    void enableDiscovery(bool enable);
    bool isDiscoveryEnabled() const { return m_discoveryEnabled.load(std::memory_order_relaxed); }

    // -------------------------------------------------------------------------
    // Consensus Control
    // -------------------------------------------------------------------------
    void setConsensusQuorum(uint32_t quorum);
    uint32_t getConsensusQuorum() const;

    // -------------------------------------------------------------------------
    // JSON serialization for HTTP API
    // -------------------------------------------------------------------------
    std::string toJson() const;
    std::string nodesToJson() const;
    std::string taskGraphToJson() const;
    std::string statsToJson() const;
    std::string eventsToJson(uint32_t maxCount = 50) const;
    std::string configToJson() const;

private:
    SwarmCoordinator();
    ~SwarmCoordinator();
    SwarmCoordinator(const SwarmCoordinator&) = delete;
    SwarmCoordinator& operator=(const SwarmCoordinator&) = delete;

    // -------------------------------------------------------------------------
    // Internal: Network threads
    // -------------------------------------------------------------------------
    static DWORD WINAPI listenerThread(LPVOID param);
    static DWORD WINAPI iocpWorkerThread(LPVOID param);
    static DWORD WINAPI heartbeatThread(LPVOID param);
    static DWORD WINAPI discoveryThread(LPVOID param);
    static DWORD WINAPI schedulerThread(LPVOID param);

    // -------------------------------------------------------------------------
    // Internal: Packet handlers (dispatched by opcode)
    // -------------------------------------------------------------------------
    void handleHeartbeat(uint32_t nodeSlot, const HeartbeatPayload* payload);
    void handleTaskPull(uint32_t nodeSlot, const TaskPullPayload* payload);
    void handleResultPush(uint32_t nodeSlot, const ResultPushPayload* payload,
                           const uint8_t* extraData, uint32_t extraLen);
    void handleAttestResponse(uint32_t nodeSlot, const AttestResponsePayload* payload);
    void handleCapsReport(uint32_t nodeSlot, const CapsReportPayload* payload);
    void handleConsensusVote(uint32_t nodeSlot, const ConsensusVotePayload* payload);
    void handleLogStream(uint32_t nodeSlot, const LogStreamPayload* payload,
                          const char* logText);
    void handleMetricReport(uint32_t nodeSlot, const MetricReportPayload* payload);

    // -------------------------------------------------------------------------
    // Internal: Scheduling
    // -------------------------------------------------------------------------
    void scheduleReadyTasks();
    uint32_t selectBestNode(const SwarmTasklet& task) const;
    bool sendTaskToNode(uint32_t nodeSlot, SwarmTasklet& task);
    void promotePendingTasks();
    void checkTaskTimeouts();
    void requeueFailedTask(uint64_t taskId);

    // -------------------------------------------------------------------------
    // Internal: Node management
    // -------------------------------------------------------------------------
    uint32_t allocateNodeSlot();
    void freeNodeSlot(uint32_t slotIndex);
    void sendAttestChallenge(uint32_t nodeSlot);
    bool verifyAttestResponse(uint32_t nodeSlot, const AttestResponsePayload* resp);
    void checkHeartbeatTimeouts();
    void evictNode(uint32_t nodeSlot, const char* reason);

    // -------------------------------------------------------------------------
    // Internal: Discovery
    // -------------------------------------------------------------------------
    void sendDiscoveryPing();
    void handleDiscoveryPing(const DiscoveryPayload* payload, const sockaddr_in& from);
    void handleDiscoveryPong(const DiscoveryPayload* payload, const sockaddr_in& from);

    // -------------------------------------------------------------------------
    // Internal: Consensus
    // -------------------------------------------------------------------------
    void initiateConsensus(uint64_t taskId, uint64_t resultHash);
    void resolveConsensus(uint64_t taskId);

    // -------------------------------------------------------------------------
    // Internal: Network I/O
    // -------------------------------------------------------------------------
    bool sendPacket(uint32_t nodeSlot, SwarmOpcode opcode,
                     const void* payload, uint16_t payloadLen,
                     uint64_t taskId = 0);
    bool sendPacketRaw(SOCKET sock, const void* data, uint32_t len);
    int  recvPacket(SOCKET sock, void* buffer, uint32_t bufferSize);

    // -------------------------------------------------------------------------
    // Internal: Events
    // -------------------------------------------------------------------------
    void emitEvent(SwarmEventType type, uint32_t nodeSlot, uint64_t taskId,
                    const char* message);

    // -------------------------------------------------------------------------
    // Internal: Utility
    // -------------------------------------------------------------------------
    uint64_t generateTaskId();
    uint32_t nextSequenceId();
    void generateNodeId(uint8_t outNodeId[16]);

    // =========================================================================
    //                         MEMBER STATE
    // =========================================================================

    // Configuration
    DscConfig                 m_config;
    mutable std::mutex          m_configMutex;

    // Running state
    std::atomic<bool>           m_running;
    std::atomic<bool>           m_buildRunning;
    std::atomic<bool>           m_discoveryEnabled;
    std::atomic<bool>           m_shutdownRequested;

    // Node table (fixed-size array for lock-free MASM heartbeat access)
    SwarmNodeInfo               m_nodes[SWARM_MAX_NODES];
    std::atomic<uint32_t>       m_nodeCount;
    mutable std::mutex          m_nodesMutex;
    uint8_t                     m_localNodeId[16];

    // Task graph
    SwarmTaskGraph              m_taskGraph;
    std::atomic<uint64_t>       m_nextTaskId;
    std::unordered_map<uint64_t, size_t>  m_taskIdToIndex;  // taskId → index in m_taskGraph.tasks
    mutable std::mutex          m_taskMapMutex;

    // Consensus
    std::unordered_map<uint64_t, ConsensusEntry>  m_consensus;
    mutable std::mutex          m_consensusMutex;

    // Object cache
    std::unordered_map<uint64_t, ObjectCacheEntry>  m_objectCache;
    mutable std::mutex          m_cacheMutex;

    // Statistics
    SwarmStats                  m_stats;
    mutable std::mutex          m_statsMutex;

    // Events
    std::vector<SwarmEvent>     m_events;
    mutable std::mutex          m_eventsMutex;
    SwarmEventCallback          m_eventCallback;
    void*                       m_eventUserData;
    SwarmLogCallback            m_logCallback;
    void*                       m_logUserData;

    // Sequence counter
    std::atomic<uint32_t>       m_sequenceCounter;

    // Network handles
    SOCKET                      m_listenSocket;
    SOCKET                      m_discoverySocket;
    HANDLE                      m_iocp;

    // Thread handles
    HANDLE                      m_hListenerThread;
    HANDLE                      m_hHeartbeatThread;
    HANDLE                      m_hDiscoveryThread;
    HANDLE                      m_hSchedulerThread;
    HANDLE                      m_hIocpThreads[8];  // Up to 8 IOCP workers
    uint32_t                    m_iocpThreadCount;

    // Ring buffers (MASM-managed)
    void*                       m_txRing;           // Outgoing packet ring
    void*                       m_rxRing;           // Incoming packet ring
    void*                       m_txRingBuffer;     // Backing memory for TX ring
    void*                       m_rxRingBuffer;     // Backing memory for RX ring

    // WSADATA for WinSock
    WSADATA                     m_wsaData;
    bool                        m_wsaInitialized;
};

#endif // RAWRXD_SWARM_COORDINATOR_H
