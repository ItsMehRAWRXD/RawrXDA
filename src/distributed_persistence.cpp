/**
 * DistributedPersistence Implementation
 * Enhancement #7: Multi-Node State Sync
 */

#include "distributed_persistence.h"
#include <unordered_map>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace DistributedPersistence {

    // ===== DistributedStateManager Implementation =====

    class DistributedStateManager::Impl {
    public:
        ClusterConfig config;
        NodeId self;
        uint8_t currentMode = DP_MODE_STANDALONE;
        std::string leaderId;
        
        std::unordered_map<std::string, NodeId> nodes;
        std::unordered_map<std::string, nlohmann::json> localState;
        std::queue<ReplicationEntry> replicationQueue;
        
        std::unique_ptr<ConflictResolver> conflictResolver;
        
        std::mutex mutex;
        std::condition_variable cv;
        std::thread workerThread;
        std::atomic<bool> running{false};
        
        ReplicationStats stats;
        
        void workerLoop();
        void replicateToNode(const NodeId& node, const ReplicationEntry& entry);
        bool sendToNode(const NodeId& node, const nlohmann::json& message);
    };

    void DistributedStateManager::Impl::workerLoop() {
        while (running) {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this] { return !running || !replicationQueue.empty(); });
            
            if (!running) break;
            if (replicationQueue.empty()) continue;
            
            auto entry = replicationQueue.front();
            replicationQueue.pop();
            lock.unlock();
            
            // Replicate to all healthy nodes
            for (const auto& pair : nodes) {
                if (pair.second.isHealthy && pair.second.id != self.id) {
                    replicateToNode(pair.second, entry);
                }
            }
        }
    }

    void DistributedStateManager::Impl::replicateToNode(
        const NodeId& node,
        const ReplicationEntry& entry) {
        
        nlohmann::json message;
        message["type"] = "replicate";
        message["executionId"] = entry.executionId;
        message["timestamp"] = entry.timestamp;
        message["sequenceNumber"] = entry.sequenceNumber;
        message["delta"] = entry.delta;
        message["sourceNode"] = self.id;
        
        auto start = std::chrono::steady_clock::now();
        bool success = sendToNode(node, message);
        auto end = std::chrono::steady_clock::now();
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        if (success) {
            stats.entriesReplicated++;
            stats.avgReplicationLatencyMs = 
                (stats.avgReplicationLatencyMs * (stats.entriesReplicated - 1) + elapsed) 
                / stats.entriesReplicated;
        } else {
            stats.failedReplications++;
        }
    }

    bool DistributedStateManager::Impl::sendToNode(
        const NodeId& node,
        const nlohmann::json& message) {
        // In production: actual network I/O
        // For now: simulate success
        return true;
    }

    DistributedStateManager::DistributedStateManager() 
        : m_impl(std::make_unique<Impl>()) {
    }

    DistributedStateManager::~DistributedStateManager() {
        shutdown();
    }

    bool DistributedStateManager::initialize(const ClusterConfig& config, const NodeId& self) {
        m_impl->config = config;
        m_impl->self = self;
        m_impl->currentMode = self.mode;
        
        // Initialize node map
        for (const auto& node : config.nodes) {
            m_impl->nodes[node.id] = node;
        }
        
        // Set default conflict resolver
        if (!m_impl->conflictResolver) {
            m_impl->conflictResolver = std::make_unique<LastWriteWinsResolver>();
        }
        
        m_impl->running = true;
        m_impl->workerThread = std::thread(&Impl::workerLoop, m_impl.get());
        
        return true;
    }

    void DistributedStateManager::shutdown() {
        m_impl->running = false;
        m_impl->cv.notify_all();
        
        if (m_impl->workerThread.joinable()) {
            m_impl->workerThread.join();
        }
    }

    bool DistributedStateManager::joinCluster(const std::string& seedNode) {
        // In production: actual cluster join protocol
        m_impl->currentMode = DP_MODE_FOLLOWER;
        return true;
    }

    void DistributedStateManager::leaveCluster() {
        m_impl->currentMode = DP_MODE_STANDALONE;
        m_impl->leaderId.clear();
    }

    bool DistributedStateManager::replicate(
        const std::string& executionId,
        const nlohmann::json& delta,
        uint32_t flags) {
        
        if (m_impl->currentMode == DP_MODE_STANDALONE) {
            // Just store locally
            m_impl->localState[executionId] = delta;
            return true;
        }
        
        ReplicationEntry entry;
        entry.executionId = executionId;
        entry.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        entry.sequenceNumber = m_impl->stats.entriesReplicated;
        entry.delta = delta;
        entry.sourceNode = m_impl->self.id;
        
        {
            std::lock_guard<std::mutex> lock(m_impl->mutex);
            m_impl->replicationQueue.push(entry);
        }
        
        m_impl->cv.notify_one();
        
        if (flags & DP_SYNC_SYNC) {
            // Wait for replication
            // In production: wait for acknowledgments
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        return true;
    }

    std::optional<nlohmann::json> DistributedStateManager::getState(
        const std::string& executionId) {
        
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        
        auto it = m_impl->localState.find(executionId);
        if (it != m_impl->localState.end()) {
            return it->second;
        }
        
        // In production: fetch from other nodes
        return std::nullopt;
    }

    DistributedStateManager::ClusterStatus DistributedStateManager::getStatus() const {
        ClusterStatus status;
        status.currentMode = m_impl->currentMode;
        status.leaderId = m_impl->leaderId;
        status.isPartitioned = false;
        
        for (const auto& pair : m_impl->nodes) {
            if (pair.second.isHealthy) {
                status.healthyNodes.push_back(pair.second);
            } else {
                status.unhealthyNodes.push_back(pair.second);
            }
        }
        
        status.replicationLag = m_impl->replicationQueue.size();
        
        return status;
    }

    void DistributedStateManager::setConflictResolver(
        std::unique_ptr<ConflictResolver> resolver) {
        m_impl->conflictResolver = std::move(resolver);
    }

    void DistributedStateManager::forceElection() {
        if (m_impl->currentMode == DP_MODE_FOLLOWER) {
            m_impl->currentMode = DP_MODE_CANDIDATE;
            // In production: start Raft election
        }
    }

    DistributedStateManager::ReplicationStats 
    DistributedStateManager::getReplicationStats() const {
        return m_impl->stats;
    }

    // ===== Conflict Resolvers =====

    nlohmann::json LastWriteWinsResolver::resolve(
        const std::string& executionId,
        const nlohmann::json& local,
        const nlohmann::json& remote,
        int64_t localTimestamp,
        int64_t remoteTimestamp) {
        
        return (remoteTimestamp > localTimestamp) ? remote : local;
    }

    nlohmann::json MergeResolver::resolve(
        const std::string& executionId,
        const nlohmann::json& local,
        const nlohmann::json& remote,
        int64_t localTimestamp,
        int64_t remoteTimestamp) {
        
        nlohmann::json merged = local;
        
        // Merge remote into local
        for (auto it = remote.begin(); it != remote.end(); ++it) {
            if (!merged.contains(it.key())) {
                merged[it.key()] = it.value();
            } else if (it.value().is_object() && merged[it.key()].is_object()) {
                // Recursive merge for objects
                for (auto inner = it.value().begin(); inner != it.value().end(); ++inner) {
                    merged[it.key()][inner.key()] = inner.value();
                }
            } else if (remoteTimestamp > localTimestamp) {
                // Remote wins for conflicting non-object values
                merged[it.key()] = it.value();
            }
        }
        
        return merged;
    }

} // namespace DistributedPersistence
