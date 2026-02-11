# PHASE 5: API REFERENCE - Complete Method Catalog & Examples

**Status:** ✅ Complete | **Last Updated:** Jan 27, 2026

---

## 1. CORE LIFECYCLE

### Create & Initialize

```cpp
Phase5::ModelOrchestrator* orchestrator = 
    Phase5::ModelOrchestrator::Create(
        phase1_ctx,              // Phase 1 Foundation
        phase2_ctx,              // Phase 2 Models
        phase3_ctx,              // Phase 3 Agents
        phase4_ctx,              // Phase 4 Inference
        &config                  // Optional config
    );

// Or C interop
void* orchestrator_c = OrchestratorCreateC(
    phase1_ctx, phase2_ctx, phase3_ctx, phase4_ctx, &config
);
```

### Destroy

```cpp
orchestrator->Destroy();
// or
OrchestratorDestroyC(orchestrator_c);
```

---

## 2. CLUSTER STATE QUERIES

### Get Node Identity

```cpp
uint32_t node_id = orchestrator->GetNodeId();        // 0-15
uint32_t cluster_size = orchestrator->GetClusterSize();  // Total nodes
uint32_t healthy = orchestrator->GetHealthyNodeCount();  // Alive nodes
```

### Get Consensus State

```cpp
using CS = Phase5::ConsensusState;

CS state = orchestrator->GetConsensusState();
if (state == CS::LEADER) {
    printf("I am leader\n");
} else if (state == CS::FOLLOWER) {
    uint32_t leader = orchestrator->GetCurrentLeader();
    printf("Following node %u\n", leader);
} else if (state == CS::CANDIDATE) {
    printf("Running for election\n");
}

uint64_t term = orchestrator->GetCurrentTerm();
printf("Current Raft term: %llu\n", term);

bool is_leader = orchestrator->IsLeader();
```

### Get Node Information

```cpp
Phase5::NodeInfo node_info;
if (orchestrator->GetNodeInfo(node_id, &node_info)) {
    printf("Node %u: %u.%u.%u.%u:%u\n",
           node_info.node_id,
           node_info.ip_address[0], node_info.ip_address[1],
           node_info.ip_address[2], node_info.ip_address[3],
           node_info.port);
    
    printf("  State: %d\n", (int)node_info.state);
    printf("  Healthy: %s\n", node_info.healthy ? "yes" : "no");
    printf("  Latency: %dms\n", node_info.rtc_latency_ms);
}

// Get all nodes
Phase5::NodeInfo all_nodes[16];
uint32_t count = orchestrator->GetAllNodes(all_nodes, 16);
printf("Found %u nodes\n", count);

bool healthy = orchestrator->IsNodeHealthy(node_id);
```

---

## 3. CLUSTER METRICS & MONITORING

### Get Cluster Metrics

```cpp
Phase5::ClusterMetrics metrics;
orchestrator->GetClusterMetrics(&metrics);

printf("=== Cluster Status ===\n");
printf("Total nodes: %u\n", metrics.total_nodes);
printf("Healthy: %u, Unhealthy: %u\n", 
       metrics.healthy_nodes, metrics.unhealthy_nodes);
printf("Consensus state: %u\n", metrics.consensus_state);
printf("Current term: %llu\n", metrics.current_term);
printf("Leader: node %u\n", metrics.leader_id);

printf("\n=== Activity ===\n");
printf("Total requests: %llu\n", metrics.total_requests);
printf("Total tokens: %llu\n", metrics.total_tokens_processed);
printf("Total errors: %llu\n", metrics.total_errors);

printf("\n=== Healing ===\n");
printf("Active tasks: %u\n", metrics.active_healing_tasks);
printf("Completed: %u\n", metrics.completed_healing_tasks);
printf("Bytes healed: %llu\n", metrics.bytes_healed);
```

### Get Performance Sample

```cpp
Phase5::PerformanceSample sample;
orchestrator->GetPerformanceSample(&sample);

printf("Throughput: %u tps\n", sample.throughput_tps);
printf("Latency P50/P99: %u/%u ms\n", 
       sample.latency_p50_ms, sample.latency_p99_ms);
printf("Queue depth: %u\n", sample.queue_depth);
printf("Cache hit rate: %u%%\n", sample.cache_hit_rate);
printf("CPU: %u%%, GPU: %u%%, GPU Temp: %u°C\n",
       sample.cpu_utilization, sample.gpu_utilization, sample.gpu_temp);
```

### Get Total Counters

```cpp
uint64_t requests = orchestrator->GetTotalRequests();
uint64_t tokens = orchestrator->GetTotalTokensProcessed();
uint64_t errors = orchestrator->GetTotalErrors();
```

---

## 4. SELF-HEALING CONTROL

### Query Healing Status

```cpp
using HS = Phase5::HealingStatus;

HS status = orchestrator->GetHealingStatus();
if (status == HS::REBUILDING) {
    printf("Healing in progress\n");
} else if (status == HS::COMPLETE) {
    printf("All shards healthy\n");
}

uint32_t active = orchestrator->GetActiveHealingTasks();
uint64_t bytes = orchestrator->GetBytesHealed();
printf("Active tasks: %u, Bytes healed: %llu\n", active, bytes);
```

### Submit Healing Task

```cpp
uint64_t episode_id = 0x1234567890ABCDEF;
uint32_t priority = 1;  // 0=critical, 1=high, 2=normal, 3=low

if (orchestrator->SubmitHealingTask(episode_id, priority)) {
    printf("Submitted rebuild task for episode %llx\n", episode_id);
} else {
    printf("Failed to submit task\n");
}
```

### Cancel Healing

```cpp
if (orchestrator->CancelHealingTask(episode_id)) {
    printf("Cancelled rebuild\n");
}
```

### Verify Episode

```cpp
if (orchestrator->VerifyEpisode(episode_id)) {
    printf("Episode %llx verified\n", episode_id);
} else {
    printf("Episode %llx has errors\n", episode_id);
}
```

---

## 5. AUTOTUNING ENGINE

### Set Performance Policy

```cpp
Phase5::PerformancePolicy policy = {
    .target_throughput_tps = 1000,
    .target_latency_p99_ms = 500,
    .target_gpu_temp_celsius = 75,
    .max_cpu_utilization_percent = 80,
    
    .prefetch_window_entries = 16,
    .load_balance_threshold_percent = 15,
    .enable_thermal_throttling = true,
};

orchestrator->SetPerformancePolicy(&policy);
```

### Get Performance Policy

```cpp
const Phase5::PerformancePolicy* policy = 
    orchestrator->GetPerformancePolicy();

if (policy) {
    printf("Target throughput: %u tps\n", 
           policy->target_throughput_tps);
    printf("Target latency P99: %u ms\n",
           policy->target_latency_p99_ms);
}
```

### Query Autotuning Decisions

```cpp
using AA = Phase5::AutotuneAction;
using AT = Phase5::AutotuneTrigger;

AA action = orchestrator->GetLastAutotuneAction();
AT trigger = orchestrator->GetLastAutotuneTrigger();
uint64_t ts = orchestrator->GetLastAutotuneTimestamp();

printf("Last action: %d (trigger: %d) at %llu\n", 
       (int)action, (int)trigger, ts);

if (action == AA::SCALE_UP) {
    printf("Increased capacity due to: %d\n", (int)trigger);
} else if (action == AA::THERMAL_THROTTLE) {
    printf("Thermal pressure detected\n");
}
```

### Enable/Disable Autotuning

```cpp
if (orchestrator->EnableAutotuning()) {
    printf("Autotuning enabled\n");
}

// Later...
if (orchestrator->DisableAutotuning()) {
    printf("Autotuning disabled\n");
}
```

---

## 6. gRPC SERVICE MANAGEMENT

### Register gRPC Method

```cpp
bool success = orchestrator->RegisterGRPCMethod(
    "Orchestrator",
    "GetClusterHealth",
    Phase5::GRPCMethodType::UNARY,
    (void*)GetClusterHealthHandler,
    nullptr  // handler_context
);

if (!success) {
    printf("Failed to register method\n");
}
```

### Unregister gRPC Method

```cpp
orchestrator->UnregisterGRPCMethod("Orchestrator", "GetClusterHealth");
```

### Query Registered Methods

```cpp
uint32_t method_count = orchestrator->GetRegisteredMethodCount();
printf("Registered methods: %u\n", method_count);

for (uint32_t i = 0; i < method_count; ++i) {
    char service[64], method[64];
    uint64_t call_count, error_count;
    
    if (orchestrator->GetGRPCMetricss(i, service, method, 
                                       &call_count, &error_count)) {
        printf("  %s.%s: %llu calls, %llu errors\n",
               service, method, call_count, error_count);
    }
}
```

---

## 7. PROMETHEUS METRICS

### Register Metric

```cpp
using PMT = Phase5::PrometheusMetricType;

orchestrator->RegisterMetric(
    "phase5_total_requests",
    "Total requests processed",
    PMT::COUNTER
);

orchestrator->RegisterMetric(
    "phase5_active_healing_tasks",
    "Current healing operations",
    PMT::GAUGE
);

orchestrator->RegisterMetric(
    "phase5_request_latency_ms",
    "Request latency distribution",
    PMT::HISTOGRAM
);
```

### Update Metric

```cpp
orchestrator->UpdateMetric("phase5_total_requests", 12500);

// With timestamp
uint64_t now = GetCurrentTimestampMs();
orchestrator->UpdateMetricWithTimestamp(
    "phase5_active_healing_tasks", 3, now
);
```

### Add Labels

```cpp
orchestrator->AddMetricLabel(
    "phase5_active_healing_tasks",
    "node", "0"
);
```

### Query Metric

```cpp
uint64_t value = orchestrator->GetMetricValue(
    "phase5_total_requests"
);
printf("Total requests: %llu\n", value);

uint32_t metric_count = orchestrator->GetMetricCount();
printf("Registered metrics: %u\n", metric_count);
```

### Generate Exposition Format

```cpp
char buffer[1000000];  // 1MB buffer
uint64_t len = orchestrator->GeneratePrometheusExposition(
    buffer, sizeof(buffer)
);

printf("Generated %llu bytes of metrics\n", len);
// Buffer now contains Prometheus text format
```

---

## 8. REQUEST ROUTING

### Find Optimal Node

```cpp
uint64_t episode_id = 0x1234567890ABCDEF;
uint32_t optimal_node = orchestrator->GetOptimalNode(episode_id);
printf("Episode %llx best stored on node %u\n", 
       episode_id, optimal_node);
```

### Route Tensor Access

```cpp
uint32_t target = orchestrator->RouteTensor("layers.0.attention.w_q");
printf("Fetch w_q from node %u\n", target);
```

---

## 9. FAILURE HANDLING

### Report Node Failure

```cpp
orchestrator->ReportNodeFailure(node_id);
// Triggers:
// - Gossip suspect/dead marking
// - Raft follower timeout (if leader)
// - BFT view change if primary
// - Self-healing for affected shards
```

### Report Recovery

```cpp
orchestrator->ReportNodeRecovery(node_id);
// Removes from suspect list
// Resumes replication to recovered node
```

### Trigger Failover

```cpp
if (orchestrator->TriggerFailover()) {
    printf("Failover triggered\n");
    // Raft: Force new election
    // BFT: Initiate view change
    // Gossip: Suspect current leader/primary
} else {
    printf("Failover failed (majority lost?)\n");
}
```

### Rebalance Cluster

```cpp
if (orchestrator->RebalanceCluster()) {
    printf("Rebalancing in progress\n");
    // Migrate overloaded episodes to underutilized nodes
    // Update shard_assignments
    // Monitor migration progress
} else {
    printf("Rebalance not needed\n");
}
```

---

## 10. SHUTDOWN

### Graceful Shutdown

```cpp
bool success = orchestrator->ShutdownGraceful(30000);  // 30 second timeout
if (success) {
    printf("Cleanly shut down\n");
    // - Finish pending requests
    // - Close gRPC connections
    // - Write persistent state
    // - Leave cluster via Gossip
} else {
    printf("Graceful shutdown timed out\n");
}
```

### Forced Shutdown

```cpp
orchestrator->ShutdownForced();
// Immediately terminate all threads
// Abort ongoing operations
// DO NOT use except in emergency
```

---

## 11. DIAGNOSTICS

### Get Last Error

```cpp
const char* error = orchestrator->GetLastError();
printf("Last error: %s\n", error);
```

### Get Native Context

```cpp
void* native = orchestrator->GetNativeContext();
// For advanced debugging
// Access raw ORCHESTRATOR_CONTEXT structure
```

### Statistics

```cpp
uint64_t elapsed = orchestrator->GetElapsedTimeMs();
printf("Uptime: %llu ms\n", elapsed);

orchestrator->ResetStatistics();
// Clear counters for fresh baseline
```

---

## 12. COMPLETE EXAMPLE

```cpp
#include "Phase5_Foundation.h"
#include <stdio.h>

int main() {
    // Get phase contexts
    auto* phase1 = Phase1::Foundation::GetInstance();
    auto* phase2_ctx = phase1->GetPhase2Context();
    auto* phase3_ctx = phase1->GetPhase3Context();
    auto* phase4_ctx = phase1->GetPhase4Context();
    
    // Configure cluster
    Phase5::OrchestrationConfig config = {
        .node_id = 0,
        .cluster_id = 0xDEADBEEF,
        .datacenter = "us-east-1",
        .rack = "1a",
        .max_cluster_nodes = 16,
        .raft_heartbeat_ms = 50,
        .enable_bft = true,
    };
    
    // Create orchestrator
    auto* orch = Phase5::ModelOrchestrator::Create(
        phase1->GetNativeContext(),
        phase2_ctx, phase3_ctx, phase4_ctx,
        &config
    );
    
    if (!orch) {
        printf("Failed to create orchestrator\n");
        return 1;
    }
    
    // Configure performance
    Phase5::PerformancePolicy policy = {
        .target_throughput_tps = 1000,
        .target_latency_p99_ms = 500,
        .max_cpu_utilization_percent = 80,
        .enable_thermal_throttling = true,
    };
    orch->SetPerformancePolicy(&policy);
    orch->EnableAutotuning();
    
    // Register metrics
    orch->RegisterMetric(
        "my_app_requests",
        "Total app requests",
        Phase5::PrometheusMetricType::COUNTER
    );
    
    // Simulate some work
    for (int i = 0; i < 100; ++i) {
        orch->UpdateMetric("my_app_requests", i * 100);
        
        Phase5::ClusterMetrics metrics;
        orch->GetClusterMetrics(&metrics);
        
        printf("Term: %llu, Leader: %u, Requests: %llu\n",
               metrics.current_term,
               metrics.leader_id,
               metrics.total_requests);
        
        Sleep(1000);  // 1 second
    }
    
    // Graceful shutdown
    printf("Shutting down...\n");
    orch->ShutdownGraceful(10000);  // 10 second timeout
    orch->Destroy();
    
    return 0;
}
```

---

**PHASE 5 API REFERENCE - COMPLETE** ✅
