// ============================================================================
// swarm_orchestrator.h — Phase 21: Distributed Swarm Inference Orchestrator
// ============================================================================
// Shards transformer layers across LAN nodes, each running Phase 20's
// Universal Model Hotpatcher locally. This is the 800B capstone: the only
// consumer-accessible distributed inference engine with hotpatch-aware
// rebalancing.
//
// Protocol:
//   UDP Discovery (Port 7946): Node presence/beaconing via multicast
//   TCP Data (Port 7947): Layer shard streaming, control commands
//   Message framing: 32-byte header [magic:4][type:2][size:2][payload:8][quant:4][crc:4][res:8]
//
// Architecture:
//   - Coordinator assigns layer shards to Worker nodes by VRAM capacity
//   - Workers run UniversalModelHotpatcher locally for per-layer quantization
//   - Heartbeat thread detects dead nodes (15s timeout)
//   - Automatic rebalancing on VRAM pressure or node failure
//   - Inference requests flow through the layer pipeline across nodes
//
// MASM Integration:
//   - swarm_tensor_stream.asm: zero-copy tensor streaming with CRC32
//   - swarm_compress_chunk_rle: optional RLE compression for sparse layers
//
// Pattern: PatchResult-style structured results, no exceptions.
// Threading: Mutex-guarded, background threads for discovery/data/heartbeat.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include "universal_model_hotpatcher.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

#pragma comment(lib, "ws2_32.lib")

// ============================================================================
// ASM Externs (from swarm_tensor_stream.asm)
// ============================================================================
extern "C" {
    int64_t swarm_stream_layer(uint64_t socket_handle, void* data, uint64_t size, uint32_t quant);
    int     swarm_receive_header(uint64_t socket_handle, void* header);
    uint32_t swarm_compute_layer_crc32(const void* data, uint64_t size);
    uint64_t swarm_compress_chunk_rle(const void* src, uint64_t src_size, void* dst, uint64_t dst_cap);
    uint32_t swarm_build_discovery_packet(void* buffer, uint32_t buf_size, uint64_t total_vram,
                                           uint64_t free_vram, uint32_t role, uint32_t max_layers);
}

// ============================================================================
// Swarm Protocol Constants
// ============================================================================
namespace RawrXD {
namespace Swarm {

constexpr uint16_t SWARM_DISCOVERY_PORT = 7946;
constexpr uint16_t SWARM_DATA_PORT      = 7947;
constexpr uint32_t SWARM_MAGIC          = 0x52574152; // 'RAWR'
constexpr uint32_t LAYER_BATCH_SIZE     = 4;          // Transformer blocks per shard
constexpr int      HEARTBEAT_INTERVAL_S = 5;
constexpr int      HEARTBEAT_TIMEOUT_S  = 15;
constexpr int      MAX_SWARM_NODES      = 64;
constexpr int      MAX_DISCOVERY_PACKET = 128;

// Message types (match swarm_tensor_stream.asm)
constexpr uint16_t MSG_LAYER_REQUEST    = 1;
constexpr uint16_t MSG_LAYER_PAYLOAD    = 2;
constexpr uint16_t MSG_VRAM_PRESSURE    = 3;
constexpr uint16_t MSG_HEARTBEAT        = 4;
constexpr uint16_t MSG_REBALANCE_CMD    = 5;
constexpr uint16_t MSG_DISCOVERY        = 6;
constexpr uint16_t MSG_LAYER_ACK        = 7;
constexpr uint16_t MSG_SHUTDOWN         = 0xFFFF;

// ============================================================================
// Enums
// ============================================================================
enum class NodeRole : uint8_t {
    Coordinator = 0,    // Owns metadata, distributes work, routes inference
    Worker      = 1,    // Holds layer shards, executes partial inference
    Hybrid      = 2,    // Both (single-node mode — bootstraps as coordinator+worker)
};

enum class NodeState : uint8_t {
    Offline     = 0,
    Joining     = 1,
    Active      = 2,
    Overloaded  = 3,    // VRAM pressure critical on this node
    Failed      = 4,    // Heartbeat timeout — presumed dead
};

// ============================================================================
// Structures
// ============================================================================

// Represents a node in the swarm
struct SwarmNode {
    std::string     nodeId;             // UUID-style identifier (hex string)
    std::string     hostname;           // DNS name or IP string
    sockaddr_in     address;            // Network address
    NodeRole        role;
    NodeState       state;
    std::chrono::steady_clock::time_point lastHeartbeat;

    // Capabilities
    uint64_t        totalVRAM;          // Bytes
    uint64_t        freeVRAM;           // Bytes
    bool            gpuAccel;           // ROCm/Vulkan available
    uint32_t        maxLayers;          // Max layers this node can host

    // Current load
    std::vector<uint32_t> hostedLayers; // Layer indices this node owns
    float           avgInferenceLatency;// ms (rolling average)
    uint32_t        activeRequests;     // Concurrent inference requests
};

// A shard of layers assigned to a node
struct LayerShard {
    uint32_t        layerStart;         // Inclusive start layer index
    uint32_t        layerEnd;           // Inclusive end layer index
    std::string     nodeId;             // Owner node
    bool            resident;           // Currently loaded in VRAM on owner
    QuantType       quant;              // Current quantization level
    uint64_t        sizeBytes;          // Shard size in bytes
    uint32_t        refCount;           // Active inference references
};

// Inference request that flows through the distributed pipeline
struct SwarmInferenceRequest {
    std::string     requestId;          // Unique request identifier
    std::vector<uint32_t> layerPath;    // Layer indices to traverse in order
    void*           inputData;          // KV cache / embedding input
    uint64_t        inputSize;          // Input size in bytes
    uint32_t        currentLayer;       // Current progress in the pipeline

    // Callback: invoked when inference completes across all shards
    // Signature: void(void* output, uint64_t outputSize)
    void            (*onComplete)(void*, uint64_t);
    void*           onCompleteUserData;
};

// Swarm orchestrator statistics
struct SwarmStats {
    std::atomic<uint64_t> nodesJoined{0};
    std::atomic<uint64_t> nodesLeft{0};
    std::atomic<uint64_t> nodesTimedOut{0};
    std::atomic<uint64_t> layersDistributed{0};
    std::atomic<uint64_t> layersMigrated{0};
    std::atomic<uint64_t> bytesSent{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<uint64_t> inferenceRequests{0};
    std::atomic<uint64_t> rebalanceEvents{0};
    std::atomic<uint64_t> heartbeatsSent{0};
    std::atomic<uint64_t> heartbeatsReceived{0};
    std::atomic<uint64_t> discoveryBeacons{0};
    std::atomic<uint64_t> errors{0};
};

// ============================================================================
// Swarm Orchestrator Result
// ============================================================================
struct SwarmResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static SwarmResult ok(const char* msg) {
        SwarmResult r;
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        return r;
    }
    static SwarmResult error(const char* msg, int code = -1) {
        SwarmResult r;
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Callback types (function pointers — no std::function in hot paths)
// ============================================================================
typedef void (*SwarmNodeEventCallback)(const SwarmNode* node, const char* event, void* userData);
typedef void (*SwarmLayerEventCallback)(uint32_t layerIndex, const char* event, void* userData);

// ============================================================================
// SwarmOrchestrator — Main Class
// ============================================================================
class SwarmOrchestrator {
public:
    static SwarmOrchestrator& instance();

    // ---- Lifecycle ----
    SwarmResult initialize(NodeRole role, const std::string& bindAddr = "0.0.0.0");
    void shutdown();
    bool isInitialized() const { return m_running.load(std::memory_order_relaxed); }

    // ---- Discovery ----
    SwarmResult joinSwarm(const std::string& coordinatorIp = "");
    void leaveSwarm();
    void sendDiscoveryBeacon();

    // ---- Model Distribution ----
    SwarmResult distributeModel(const std::string& modelPath, uint32_t totalLayers);
    SwarmResult rebalance();

    // ---- Inference ----
    SwarmResult submitInference(const SwarmInferenceRequest& req);
    void cancelInference(const std::string& requestId);

    // ---- Node Management ----
    std::vector<SwarmNode> getNodeList() const;
    bool getNodeInfo(const std::string& nodeId, SwarmNode& outNode) const;
    uint32_t getNodeCount() const;
    bool isCoordinator() const { return m_role == NodeRole::Coordinator; }
    const std::string& getNodeId() const { return m_nodeId; }
    NodeRole getRole() const { return m_role; }

    // ---- Shard Management ----
    std::vector<LayerShard> getShardList() const;
    uint32_t getShardCount() const;

    // ---- Statistics ----
    const SwarmStats& getStats() const { return m_stats; }
    void resetStats();

    // ---- Callbacks ----
    void setNodeEventCallback(SwarmNodeEventCallback cb, void* userData);
    void setLayerEventCallback(SwarmLayerEventCallback cb, void* userData);

    // ---- JSON Serialization ----
    std::string toJson() const;
    std::string topologyToJson() const;
    std::string statsToJson() const;

private:
    SwarmOrchestrator();
    ~SwarmOrchestrator();
    SwarmOrchestrator(const SwarmOrchestrator&) = delete;
    SwarmOrchestrator& operator=(const SwarmOrchestrator&) = delete;

    // ---- Background Threads ----
    void discoveryListenerThread();
    void dataListenerThread();
    void heartbeatThread();
    void inferenceWorkerThread();

    // ---- Protocol Handlers ----
    void handleNodeJoin(const sockaddr_in& addr, const char* payload, int payloadLen);
    void handleLayerRequest(SOCKET clientSock, const uint8_t* header);
    void handlePressureUpdate(const std::string& nodeId, VRAMPressure pressure);
    void handleHeartbeat(const std::string& nodeId, uint64_t freeVRAM, uint8_t pressure);
    void handleRebalanceCmd(const uint8_t* payload, int payloadLen);

    // ---- Coordination Logic ----
    std::string selectOptimalNode(uint32_t layerIdx);
    bool migrateLayer(uint32_t layerIdx, const std::string& fromNode, const std::string& toNode);
    void updateTopology();
    void detectDeadNodes();

    // ---- Network Helpers ----
    bool sendToNode(const std::string& nodeId, uint16_t msgType, const void* data, size_t len);
    bool broadcastUDP(uint16_t msgType, const void* data, size_t len);

    // ---- Node ID Generation ----
    static std::string generateNodeId();

    // ---- Member State ----
    mutable std::mutex      m_mutex;
    std::atomic<bool>       m_running{false};
    NodeRole                m_role{NodeRole::Hybrid};
    std::string             m_nodeId;
    std::string             m_coordinatorId;

    // Network sockets
    SOCKET                  m_discoverySock{INVALID_SOCKET};
    SOCKET                  m_dataSock{INVALID_SOCKET};
    sockaddr_in             m_bindAddr{};

    // Topology
    std::map<std::string, SwarmNode>    m_nodes;
    std::vector<LayerShard>             m_shards;
    std::queue<SwarmInferenceRequest>   m_pendingRequests;

    // Background threads
    std::thread             m_discoveryThread;
    std::thread             m_dataThread;
    std::thread             m_heartbeatThread;
    std::thread             m_inferenceThread;

    // Statistics
    SwarmStats              m_stats;

    // Callbacks
    SwarmNodeEventCallback  m_nodeEventCb{nullptr};
    void*                   m_nodeEventUserData{nullptr};
    SwarmLayerEventCallback m_layerEventCb{nullptr};
    void*                   m_layerEventUserData{nullptr};

    // WSA initialized flag
    bool                    m_wsaInitialized{false};
};

} // namespace Swarm
} // namespace RawrXD
