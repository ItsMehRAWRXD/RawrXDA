#pragma once
/**
 * DistributedPersistence - Enhancement #7: Multi-Node State Sync
 * 
 * Enables state synchronization across multiple nodes.
 * Supports leader-follower replication and conflict resolution.
 * 
 * Symbols: DP_MODE_LEADER, DP_MODE_FOLLOWER, DP_MODE_STANDALONE
 */

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

// Node modes
#define DP_MODE_STANDALONE      0x00
#define DP_MODE_LEADER          0x01
#define DP_MODE_FOLLOWER        0x02
#define DP_MODE_CANDIDATE       0x03

// Replication flags
#define DP_SYNC_SYNC            0x01  // Synchronous replication
#define DP_SYNC_ASYNC           0x02  // Asynchronous replication
#define DP_SYNC_QUORUM          0x04  // Wait for quorum

// Conflict resolution strategies
#define DP_CONFLICT_LAST_WRITE  0x01
#define DP_CONFLICT_MERGE       0x02
#define DP_CONFLICT_REJECT      0x03
#define DP_CONFLICT_CUSTOM      0x04

namespace DistributedPersistence {

    /**
     * Node identity
     */
    struct NodeId {
        std::string id;
        std::string address;
        uint16_t port = 0;
        uint8_t mode = DP_MODE_STANDALONE;
        int64_t lastSeen = 0;
        bool isHealthy = false;
    };

    /**
     * Replication entry
     */
    struct ReplicationEntry {
        std::string executionId;
        int64_t timestamp;
        int64_t sequenceNumber;
        nlohmann::json delta;
        std::string sourceNode;
        std::vector<uint8_t> signature;
    };

    /**
     * Cluster configuration
     */
    struct ClusterConfig {
        std::string clusterId;
        std::vector<NodeId> nodes;
        int replicationFactor = 2;
        uint32_t syncMode = DP_SYNC_ASYNC;
        uint32_t conflictStrategy = DP_CONFLICT_LAST_WRITE;
        int64_t heartbeatIntervalMs = 1000;
        int64_t electionTimeoutMs = 5000;
    };

    /**
     * Conflict resolution interface
     */
    class ConflictResolver {
    public:
        virtual ~ConflictResolver() = default;
        
        // Resolve conflict between local and remote versions
        virtual nlohmann::json resolve(
            const std::string& executionId,
            const nlohmann::json& local,
            const nlohmann::json& remote,
            int64_t localTimestamp,
            int64_t remoteTimestamp) = 0;
    };

    /**
     * Distributed state manager
     */
    class DistributedStateManager {
    public:
        DistributedStateManager();
        ~DistributedStateManager();

        // Initialize with cluster config
        bool initialize(const ClusterConfig& config, const NodeId& self);
        
        // Shutdown
        void shutdown();

        // Join cluster
        bool joinCluster(const std::string& seedNode);
        
        // Leave cluster
        void leaveCluster();

        // Replicate state change
        bool replicate(
            const std::string& executionId,
            const nlohmann::json& delta,
            uint32_t flags = DP_SYNC_ASYNC);
        
        // Get state from cluster
        std::optional<nlohmann::json> getState(
            const std::string& executionId);

        // Get cluster status
        struct ClusterStatus {
            uint8_t currentMode;
            std::string leaderId;
            std::vector<NodeId> healthyNodes;
            std::vector<NodeId> unhealthyNodes;
            size_t replicationLag;
            bool isPartitioned;
        };
        ClusterStatus getStatus() const;

        // Set custom conflict resolver
        void setConflictResolver(std::unique_ptr<ConflictResolver> resolver);

        // Force election (for testing)
        void forceElection();

        // Get replication statistics
        struct ReplicationStats {
            size_t entriesReplicated = 0;
            size_t entriesReceived = 0;
            size_t conflictsResolved = 0;
            size_t conflictsRejected = 0;
            double avgReplicationLatencyMs = 0;
            size_t failedReplications = 0;
        };
        ReplicationStats getReplicationStats() const;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Raft consensus implementation
     */
    class RaftConsensus {
    public:
        RaftConsensus();
        ~RaftConsensus();

        // Initialize
        bool initialize(const NodeId& self, const std::vector<NodeId>& peers);
        
        // Process incoming message
        void handleMessage(const nlohmann::json& message);
        
        // Append entry to log
        bool appendEntry(const ReplicationEntry& entry);
        
        // Get current term
        int64_t getCurrentTerm() const;
        
        // Check if leader
        bool isLeader() const;
        
        // Get leader ID
        std::string getLeaderId() const;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Built-in conflict resolvers
     */
    class LastWriteWinsResolver : public ConflictResolver {
    public:
        nlohmann::json resolve(
            const std::string& executionId,
            const nlohmann::json& local,
            const nlohmann::json& remote,
            int64_t localTimestamp,
            int64_t remoteTimestamp) override;
    };

    class MergeResolver : public ConflictResolver {
    public:
        nlohmann::json resolve(
            const std::string& executionId,
            const nlohmann::json& local,
            const nlohmann::json& remote,
            int64_t localTimestamp,
            int64_t remoteTimestamp) override;
    };

} // namespace DistributedPersistence
