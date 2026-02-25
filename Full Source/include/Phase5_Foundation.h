//================================================================================
// PHASE5_FOUNDATION.H - Orchestrator Public C++ API
// Distributed consensus, self-healing, autotuning, metrics, gRPC
//================================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <windows.h>
#include <winsock2.h>

namespace Phase5 {

//================================================================================
// ENUMERATIONS
//================================================================================

// Consensus states
enum class ConsensusState : uint32_t {
    UNKNOWN = 0,
    FOLLOWER = 1,
    CANDIDATE = 2,
    LEADER = 3,
    PRE_VOTE = 4
};

// Healing status
enum class HealingStatus : uint32_t {
    IDLE = 0,
    REBUILDING = 1,
    VERIFYING = 2,
    MIGRATING = 3,
    COMPLETE = 4,
    FAILED = 5
};

// Healing task types
enum class HealingTaskType : uint32_t {
    REBUILD = 0,
    VERIFY = 1,
    MIGRATE = 2
};

// Autotune actions
enum class AutotuneAction : uint32_t {
    MAINTAIN = 0,
    SCALE_UP = 1,
    SCALE_DOWN = 2,
    REBALANCE = 3,
    THERMAL_THROTTLE = 4
};

// Autotune decision trigger
enum class AutotuneTrigger : uint32_t {
    NONE = 0,
    LOW_EFFICIENCY = 1,
    HIGH_LATENCY = 2,
    THERMAL_PRESSURE = 3,
    LOAD_IMBALANCE = 4,
    CACHE_MISS = 5
};

// gRPC method types
enum class GRPCMethodType : uint32_t {
    UNARY = 0,
    CLIENT_STREAMING = 1,
    SERVER_STREAMING = 2,
    BIDI_STREAMING = 3
};

// Prometheus metric types
enum class PrometheusMetricType : uint32_t {
    COUNTER = 0,
    GAUGE = 1,
    HISTOGRAM = 2,
    SUMMARY = 3
};

// Orchestrator states
enum class OrchestratorState : uint32_t {
    INITIALIZING = 0,
    ACTIVE = 1,
    DEGRADED = 2,
    SHUTTING_DOWN = 3
};

//================================================================================
// STRUCTURES
//================================================================================

// Orchestration configuration
struct OrchestrationConfig {
    uint32_t node_id;
    uint64_t cluster_id;
    char datacenter[64];
    char rack[64];
    
    uint16_t bind_port;
    uint16_t gossip_port;
    uint16_t raft_port;
    uint16_t grpc_port;
    uint16_t metrics_port;
    
    uint32_t max_cluster_nodes;
    uint32_t raft_heartbeat_ms;
    uint32_t raft_election_min_ms;
    uint32_t raft_election_max_ms;
    
    uint32_t gossip_interval_ms;
    uint32_t scrub_interval_ms;
    uint32_t autotune_interval_ms;
    
    bool enable_bft;
    bool enable_tls;
    uint32_t padding;
};

// Node information
struct NodeInfo {
    uint32_t node_id;
    uint8_t ip_address[16];
    uint16_t port;
    ConsensusState state;
    
    uint64_t last_seen;
    uint32_t rtc_latency_ms;
    bool healthy;
    uint32_t padding;
};

// Cluster metrics
struct ClusterMetrics {
    uint32_t total_nodes;
    uint32_t healthy_nodes;
    uint32_t unhealthy_nodes;
    uint32_t consensus_state;
    
    uint64_t current_term;
    uint32_t leader_id;
    uint32_t primary_id;
    
    uint64_t total_requests;
    uint64_t total_tokens_processed;
    uint64_t total_errors;
    
    uint32_t active_healing_tasks;
    uint32_t completed_healing_tasks;
    uint64_t bytes_healed;
    
    uint32_t raft_log_entries;
    uint32_t committed_entries;
    uint32_t applied_entries;
};

// Performance policy for autotuning
struct PerformancePolicy {
    uint32_t target_throughput_tps;
    uint32_t target_latency_p99_ms;
    uint32_t target_gpu_temp_celsius;
    uint32_t max_cpu_utilization_percent;
    
    uint32_t prefetch_window_entries;
    uint32_t load_balance_threshold_percent;
    bool enable_thermal_throttling;
    uint32_t padding;
};

// Performance sample snapshot
struct PerformanceSample {
    uint64_t timestamp;
    uint32_t cpu_utilization;
    uint64_t memory_used;
    uint64_t memory_available;
    uint32_t gpu_utilization;
    uint64_t gpu_memory_used;
    uint32_t gpu_temp;
    
    uint32_t throughput_tps;
    uint32_t latency_p50_ms;
    uint32_t latency_p99_ms;
    uint32_t queue_depth;
    uint32_t cache_hit_rate;
    
    uint32_t episodes_hot;
    uint32_t episodes_loading;
    uint32_t episodes_skipped;
    uint32_t prefetch_hit_rate;
    
    uint32_t efficiency_score;
};

//================================================================================
// MAIN CLASS - ModelOrchestrator
//================================================================================

class ModelOrchestrator {
public:
    // Lifecycle
    static ModelOrchestrator* Create(void* phase1_ctx, void* phase2_ctx,
                                      void* phase3_ctx, void* phase4_ctx,
                                      const OrchestrationConfig* config = nullptr);
    void Destroy();
    
    // Cluster State
    OrchestratorState GetState() const;
    uint32_t GetNodeId() const;
    uint32_t GetClusterSize() const;
    uint32_t GetHealthyNodeCount() const;
    
    ConsensusState GetConsensusState() const;
    uint32_t GetCurrentLeader() const;
    uint64_t GetCurrentTerm() const;
    bool IsLeader() const;
    
    // Node Information
    bool GetNodeInfo(uint32_t node_id, NodeInfo* out_info) const;
    uint32_t GetAllNodes(NodeInfo* out_nodes, uint32_t max_nodes) const;
    bool IsNodeHealthy(uint32_t node_id) const;
    
    // Metrics & Monitoring
    void GetClusterMetrics(ClusterMetrics* out_metrics) const;
    void GetPerformanceSample(PerformanceSample* out_sample) const;
    
    uint64_t GetTotalRequests() const;
    uint64_t GetTotalTokensProcessed() const;
    uint64_t GetTotalErrors() const;
    
    // Healing Control
    HealingStatus GetHealingStatus() const;
    uint32_t GetActiveHealingTasks() const;
    uint64_t GetBytesHealed() const;
    
    bool SubmitHealingTask(uint64_t episode_id, uint32_t priority);
    bool CancelHealingTask(uint64_t episode_id);
    bool VerifyEpisode(uint64_t episode_id);
    
    // Autotuning
    void SetPerformancePolicy(const PerformancePolicy* policy);
    const PerformancePolicy* GetPerformancePolicy() const;
    
    AutotuneAction GetLastAutotuneAction() const;
    AutotuneTrigger GetLastAutotuneTrigger() const;
    uint64_t GetLastAutotuneTimestamp() const;
    
    bool EnableAutotuning();
    bool DisableAutotuning();
    
    // gRPC Service Management
    bool RegisterGRPCMethod(const char* service_name,
                           const char* method_name,
                           GRPCMethodType method_type,
                           void* handler_func,
                           void* handler_context);
    
    bool UnregisterGRPCMethod(const char* service_name,
                              const char* method_name);
    
    uint32_t GetRegisteredMethodCount() const;
    bool GetGRPCMetricss(uint32_t method_idx,
                        char* out_service,
                        char* out_method,
                        uint64_t* out_call_count,
                        uint64_t* out_error_count) const;
    
    // Prometheus Metrics
    bool RegisterMetric(const char* name, const char* help_text,
                        PrometheusMetricType type);
    
    bool UpdateMetric(const char* name, uint64_t value);
    bool UpdateMetricWithTimestamp(const char* name, uint64_t value,
                                    uint64_t timestamp);
    
    bool AddMetricLabel(const char* name, const char* label_name,
                        const char* label_value);
    
    uint64_t GetMetricValue(const char* name) const;
    uint32_t GetMetricCount() const;
    
    // Exposition
    uint64_t GeneratePrometheusExposition(char* out_buffer,
                                          uint64_t buffer_size);
    
    // Request Routing
    uint32_t GetOptimalNode(uint64_t episode_id) const;
    uint32_t RouteTensor(const char* tensor_name) const;
    
    // Failure Handling
    void ReportNodeFailure(uint32_t node_id);
    void ReportNodeRecovery(uint32_t node_id);
    
    bool TriggerFailover();
    bool RebalanceCluster();
    
    // Shutdown
    bool ShutdownGraceful(uint32_t timeout_ms);
    void ShutdownForced();
    
    // Diagnostics
    const char* GetLastError() const;
    void* GetNativeContext() const;
    
    // Statistics
    void ResetStatistics();
    uint64_t GetElapsedTimeMs() const;

private:
    ModelOrchestrator() = default;
    ~ModelOrchestrator() = default;
    
    void* m_context;
};

//================================================================================
// C INTEROP FUNCTIONS
//================================================================================

extern "C" {
    // Core lifecycle
    void* OrchestratorCreateC(void* phase1_ctx, void* phase2_ctx,
                              void* phase3_ctx, void* phase4_ctx,
                              const OrchestrationConfig* config);
    
    void OrchestratorDestroyC(void* orchestrator);
    
    // State queries
    uint32_t OrchestratorGetStateC(void* orchestrator);
    uint32_t OrchestratorGetNodeIdC(void* orchestrator);
    uint32_t OrchestratorGetClusterSizeC(void* orchestrator);
    uint32_t OrchestratorGetHealthyCountC(void* orchestrator);
    
    uint32_t OrchestratorGetConsensusStateC(void* orchestrator);
    uint32_t OrchestratorGetLeaderIdC(void* orchestrator);
    uint64_t OrchestratorGetCurrentTermC(void* orchestrator);
    
    // Metrics
    void OrchestratorGetMetricsC(void* orchestrator, ClusterMetrics* out_metrics);
    void OrchestratorGetSampleC(void* orchestrator, PerformanceSample* out_sample);
    
    uint64_t OrchestratorGetTotalRequestsC(void* orchestrator);
    uint64_t OrchestratorGetTotalTokensC(void* orchestrator);
    uint64_t OrchestratorGetTotalErrorsC(void* orchestrator);
    
    // Healing
    uint32_t OrchestratorGetHealingStatusC(void* orchestrator);
    uint32_t OrchestratorGetActiveTasksC(void* orchestrator);
    uint64_t OrchestratorGetBytesHealedC(void* orchestrator);
    
    bool OrchestratorSubmitHealingC(void* orchestrator, uint64_t episode_id,
                                    uint32_t priority);
    
    bool OrchestratorCancelHealingC(void* orchestrator, uint64_t episode_id);
    
    // Autotuning
    void OrchestratorSetPolicyC(void* orchestrator,
                                const PerformancePolicy* policy);
    
    const PerformancePolicy* OrchestratorGetPolicyC(void* orchestrator);
    
    uint32_t OrchestratorGetLastActionC(void* orchestrator);
    uint32_t OrchestratorGetLastTriggerC(void* orchestrator);
    
    bool OrchestratorEnableAutotune C(void* orchestrator);
    bool OrchestratorDisableAutotuneC(void* orchestrator);
    
    // gRPC
    bool OrchestratorRegisterGRPCC(void* orchestrator,
                                   const char* service_name,
                                   const char* method_name,
                                   uint32_t method_type,
                                   void* handler_func,
                                   void* handler_context);
    
    // Prometheus
    bool OrchestratorRegisterMetricC(void* orchestrator,
                                     const char* name,
                                     const char* help_text,
                                     uint32_t type);
    
    bool OrchestratorUpdateMetricC(void* orchestrator,
                                   const char* name,
                                   uint64_t value);
    
    uint64_t OrchestratorGenerateExpositionC(void* orchestrator,
                                             char* out_buffer,
                                             uint64_t buffer_size);
    
    // Routing
    uint32_t OrchestratorGetOptimalNodeC(void* orchestrator, uint64_t episode_id);
    
    // Failure handling
    void OrchestratorReportFailureC(void* orchestrator, uint32_t node_id);
    void OrchestratorReportRecoveryC(void* orchestrator, uint32_t node_id);
    
    bool OrchestratorTriggerFailoverC(void* orchestrator);
    bool OrchestratorRebalanceC(void* orchestrator);
    
    // Shutdown
    bool OrchestratorShutdownGracefulC(void* orchestrator, uint32_t timeout_ms);
    void OrchestratorShutdownForcedC(void* orchestrator);
    
    // Diagnostics
    const char* OrchestratorGetLastErrorC(void* orchestrator);
}

//================================================================================
// CONVENIENCE MACROS
//================================================================================

#define PHASE5_ORCHESTRATOR Phase5::ModelOrchestrator
#define PHASE5_STATE_INITIALIZING Phase5::OrchestratorState::INITIALIZING
#define PHASE5_STATE_ACTIVE Phase5::OrchestratorState::ACTIVE
#define PHASE5_STATE_DEGRADED Phase5::OrchestratorState::DEGRADED
#define PHASE5_STATE_SHUTTING_DOWN Phase5::OrchestratorState::SHUTTING_DOWN

#define PHASE5_CONSENSUS_FOLLOWER Phase5::ConsensusState::FOLLOWER
#define PHASE5_CONSENSUS_LEADER Phase5::ConsensusState::LEADER
#define PHASE5_CONSENSUS_CANDIDATE Phase5::ConsensusState::CANDIDATE

#define PHASE5_HEALING_IDLE Phase5::HealingStatus::IDLE
#define PHASE5_HEALING_REBUILDING Phase5::HealingStatus::REBUILDING
#define PHASE5_HEALING_COMPLETE Phase5::HealingStatus::COMPLETE

#define PHASE5_AUTOTUNE_SCALE_UP Phase5::AutotuneAction::SCALE_UP
#define PHASE5_AUTOTUNE_SCALE_DOWN Phase5::AutotuneAction::SCALE_DOWN
#define PHASE5_AUTOTUNE_REBALANCE Phase5::AutotuneAction::REBALANCE

#define PHASE5_METRIC_COUNTER Phase5::PrometheusMetricType::COUNTER
#define PHASE5_METRIC_GAUGE Phase5::PrometheusMetricType::GAUGE
#define PHASE5_METRIC_HISTOGRAM Phase5::PrometheusMetricType::HISTOGRAM

} // namespace Phase5

#endif // PHASE5_FOUNDATION_H
