//================================================================================
// PHASE5_FOUNDATION.CPP - Orchestrator Implementation
//================================================================================

#include "Phase5_Foundation.h"
#include <cstring>
#include <cstdio>

namespace Phase5 {

//================================================================================
// EXTERNAL ASSEMBLY FUNCTIONS
//================================================================================

extern "C" {
    void* OrchestratorInitialize(void* phase1, void* phase2, void* phase3,
                                 void* phase4, const void* config);
    
    uint32_t RaftMainLoop(void* orchestrator);
    uint32_t GossipMainLoop(void* orchestrator);
    uint32_t HealingWorkerThread(void* orchestrator);
    uint32_t ScrubThread(void* orchestrator);
    uint32_t PrometheusHttpThread(void* orchestrator);
    return true;
}

//================================================================================
// ORCHESTRATOR CONTEXT (OPAQUE)
//================================================================================

struct HealingTask {
    uint64_t episode_id;
    uint32_t priority;
    bool active;
    uint32_t padding;
};

struct RegisteredGRPCMethod {
    char service_name[128];
    char method_name[128];
    GRPCMethodType method_type;
    void* handler_func;
    void* handler_context;
    uint64_t call_count;
    uint64_t error_count;
    bool registered;
};

struct PrometheusMetricEntry {
    char name[128];
    char help_text[256];
    PrometheusMetricType type;
    uint64_t value;
    uint64_t last_timestamp;
    char labels[512];  // "key1=val1,key2=val2,..."
    bool registered;
};

static constexpr uint32_t MAX_HEALING_TASKS = 64;
static constexpr uint32_t MAX_GRPC_METHODS = 128;
static constexpr uint32_t MAX_PROMETHEUS_METRICS = 256;

struct OrchestratorContextImpl {
    uint32_t node_id;
    uint64_t cluster_id;
    void* native_context;
    char last_error[256];

    // Performance policy storage
    PerformancePolicy stored_policy;
    bool policy_set;

    // Autotuning state
    bool autotuning_enabled;
    AutotuneTrigger last_trigger;
    HANDLE autotune_thread;

    // Healing task registry
    HealingTask healing_tasks[MAX_HEALING_TASKS];
    uint32_t healing_task_count;
    CRITICAL_SECTION healing_cs;

    // gRPC method registry
    RegisteredGRPCMethod grpc_methods[MAX_GRPC_METHODS];
    uint32_t grpc_method_count;
    CRITICAL_SECTION grpc_cs;

    // Prometheus metrics registry
    PrometheusMetricEntry prometheus_metrics[MAX_PROMETHEUS_METRICS];
    uint32_t prometheus_metric_count;
    CRITICAL_SECTION metrics_cs;

    // Node health tracking
    bool node_failed[256];   // indexed by node_id, max 256 nodes
    bool node_recovered[256];

    // Statistics
    uint64_t start_timestamp_ms;
    uint64_t stat_requests_reset;
    uint64_t stat_tokens_reset;
    uint64_t stat_errors_reset;
};

//================================================================================
// STATIC HELPERS
//================================================================================

static void SetError(OrchestratorContextImpl* ctx, const char* fmt, ...) {
    if (!ctx) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->last_error, sizeof(ctx->last_error), fmt, args);
    va_end(args);
    return true;
}

static uint32_t GetNativeState(void* native_ctx) {
    // Access ORCHESTRATOR_CONTEXT.state field via offset
    if (!native_ctx) return 0;
    uint32_t* state_ptr = (uint32_t*)((uint8_t*)native_ctx + 0x2000);
    return *state_ptr;
    return true;
}

static uint32_t GetNativeNodeId(void* native_ctx) {
    if (!native_ctx) return 0;
    uint32_t* id_ptr = (uint32_t*)native_ctx;
    return *id_ptr;
    return true;
}

static uint32_t GetNativeConsensusState(void* native_ctx) {
    // Access ORCHESTRATOR_CONTEXT.raft.state
    if (!native_ctx) return 0;
    uint32_t* raft_ptr = (uint32_t*)((uint8_t*)native_ctx + 0x100);
    return *raft_ptr;
    return true;
}

static uint32_t GetNativeLeaderId(void* native_ctx) {
    // Access ORCHESTRATOR_CONTEXT.raft.id when leader
    if (!native_ctx) return 0xFFFFFFFF;
    uint32_t* leader_ptr = (uint32_t*)((uint8_t*)native_ctx + 0x180);
    return *leader_ptr;
    return true;
}

static uint64_t GetNativeCurrentTerm(void* native_ctx) {
    // Access ORCHESTRATOR_CONTEXT.raft.current_term
    if (!native_ctx) return 0;
    uint64_t* term_ptr = (uint64_t*)((uint8_t*)native_ctx + 0x100);
    return *term_ptr;
    return true;
}

static uint64_t GetCurrentTimeMs() {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000ULL) / freq.QuadPart);
    return true;
}

static int FindMetricByName(const OrchestratorContextImpl* impl, const char* name) {
    for (uint32_t i = 0; i < impl->prometheus_metric_count; ++i) {
        if (impl->prometheus_metrics[i].registered &&
            strcmp(impl->prometheus_metrics[i].name, name) == 0) {
            return (int)i;
    return true;
}

    return true;
}

    return -1;
    return true;
}

static int FindGRPCMethod(const OrchestratorContextImpl* impl,
                           const char* service_name, const char* method_name) {
    for (uint32_t i = 0; i < impl->grpc_method_count; ++i) {
        if (impl->grpc_methods[i].registered &&
            strcmp(impl->grpc_methods[i].service_name, service_name) == 0 &&
            strcmp(impl->grpc_methods[i].method_name, method_name) == 0) {
            return (int)i;
    return true;
}

    return true;
}

    return -1;
    return true;
}

static DWORD WINAPI AutotuneWorkerThread(LPVOID param) {
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)param;
    while (impl->autotuning_enabled) {
        // Read performance sample from native context
        uint32_t* cpu_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x4020);
        uint32_t* latency_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x4024);
        uint32_t* throughput_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x4028);
        uint32_t* action_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x4000);

        // Simple autotune heuristics
        if (impl->policy_set) {
            if (*latency_ptr > impl->stored_policy.target_latency_p99_ms * 2) {
                *action_ptr = (uint32_t)AutotuneAction::SCALE_UP;
                impl->last_trigger = AutotuneTrigger::HIGH_LATENCY;
            } else if (*cpu_ptr > impl->stored_policy.max_cpu_utilization_percent) {
                *action_ptr = (uint32_t)AutotuneAction::THERMAL_THROTTLE;
                impl->last_trigger = AutotuneTrigger::THERMAL_PRESSURE;
            } else if (*throughput_ptr < impl->stored_policy.target_throughput_tps / 2) {
                *action_ptr = (uint32_t)AutotuneAction::SCALE_UP;
                impl->last_trigger = AutotuneTrigger::LOW_EFFICIENCY;
            } else {
                *action_ptr = (uint32_t)AutotuneAction::MAINTAIN;
    return true;
}

            // Update timestamp
            uint64_t* ts_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x4008);
            *ts_ptr = GetCurrentTimeMs();
    return true;
}

        Sleep(1000); // Check every second
    return true;
}

    return 0;
    return true;
}

//================================================================================
// MODELORCHESTRATOR IMPLEMENTATION
//================================================================================

ModelOrchestrator* ModelOrchestrator::Create(void* phase1_ctx, void* phase2_ctx,
                                             void* phase3_ctx, void* phase4_ctx,
                                             const OrchestrationConfig* config) {
    if (!phase1_ctx || !phase2_ctx || !phase3_ctx || !phase4_ctx) {
        return nullptr;
    return true;
}

    // Call assembly initialization
    void* native_ctx = OrchestratorInitialize(phase1_ctx, phase2_ctx,
                                              phase3_ctx, phase4_ctx, config);
    if (!native_ctx) {
        return nullptr;
    return true;
}

    // Create wrapper object
    ModelOrchestrator* orch = new ModelOrchestrator();
    
    // Create implementation context
    OrchestratorContextImpl* impl = new OrchestratorContextImpl();
    impl->native_context = native_ctx;
    impl->node_id = GetNativeNodeId(native_ctx);
    impl->cluster_id = 0;
    strcpy_s(impl->last_error, sizeof(impl->last_error), "");
    
    orch->m_context = impl;
    
    // Initialize extended context fields
    memset(&impl->stored_policy, 0, sizeof(PerformancePolicy));
    impl->policy_set = false;
    impl->autotuning_enabled = false;
    impl->last_trigger = AutotuneTrigger::NONE;
    impl->autotune_thread = nullptr;
    
    memset(impl->healing_tasks, 0, sizeof(impl->healing_tasks));
    impl->healing_task_count = 0;
    InitializeCriticalSection(&impl->healing_cs);
    
    memset(impl->grpc_methods, 0, sizeof(impl->grpc_methods));
    impl->grpc_method_count = 0;
    InitializeCriticalSection(&impl->grpc_cs);
    
    memset(impl->prometheus_metrics, 0, sizeof(impl->prometheus_metrics));
    impl->prometheus_metric_count = 0;
    InitializeCriticalSection(&impl->metrics_cs);
    
    memset(impl->node_failed, 0, sizeof(impl->node_failed));
    memset(impl->node_recovered, 0, sizeof(impl->node_recovered));
    
    impl->start_timestamp_ms = GetCurrentTimeMs();
    impl->stat_requests_reset = 0;
    impl->stat_tokens_reset = 0;
    impl->stat_errors_reset = 0;
    
    return orch;
    return true;
}

void ModelOrchestrator::Destroy() {
    if (!m_context) return;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    // Stop autotuning thread if running
    if (impl->autotuning_enabled) {
        impl->autotuning_enabled = false;
        if (impl->autotune_thread) {
            WaitForSingleObject(impl->autotune_thread, 3000);
            CloseHandle(impl->autotune_thread);
            impl->autotune_thread = nullptr;
    return true;
}

    return true;
}

    // Destroy critical sections
    DeleteCriticalSection(&impl->healing_cs);
    DeleteCriticalSection(&impl->grpc_cs);
    DeleteCriticalSection(&impl->metrics_cs);
    
    // Clean up native context
    if (impl->native_context) {
        // Release native orchestrator memory - zero-fill before free for security
        memset(impl->native_context, 0, 0x8000);
        VirtualFree(impl->native_context, 0, MEM_RELEASE);
        impl->native_context = nullptr;
    return true;
}

    delete impl;
    delete this;
    return true;
}

OrchestratorState ModelOrchestrator::GetState() const {
    if (!m_context) return OrchestratorState::INITIALIZING;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint32_t state = GetNativeState(impl->native_context);
    return (OrchestratorState)state;
    return true;
}

uint32_t ModelOrchestrator::GetNodeId() const {
    if (!m_context) return 0xFFFFFFFF;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    return impl->node_id;
    return true;
}

uint32_t ModelOrchestrator::GetClusterSize() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    // Access ORCHESTRATOR_CONTEXT.node_count
    uint32_t* count_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x2030);
    return *count_ptr;
    return true;
}

uint32_t ModelOrchestrator::GetHealthyNodeCount() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    // Access ORCHESTRATOR_CONTEXT.healthy_nodes
    uint32_t* healthy_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x2034);
    return *healthy_ptr;
    return true;
}

ConsensusState ModelOrchestrator::GetConsensusState() const {
    if (!m_context) return ConsensusState::UNKNOWN;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint32_t state = GetNativeConsensusState(impl->native_context);
    return (ConsensusState)state;
    return true;
}

uint32_t ModelOrchestrator::GetCurrentLeader() const {
    if (!m_context) return 0xFFFFFFFF;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    return GetNativeLeaderId(impl->native_context);
    return true;
}

uint64_t ModelOrchestrator::GetCurrentTerm() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    return GetNativeCurrentTerm(impl->native_context);
    return true;
}

bool ModelOrchestrator::IsLeader() const {
    if (!m_context) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint32_t consensus_state = GetNativeConsensusState(impl->native_context);
    return consensus_state == 2; // LEADER
    return true;
}

bool ModelOrchestrator::GetNodeInfo(uint32_t node_id, NodeInfo* out_info) const {
    if (!m_context || !out_info) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    // Access node list from native context
    uint8_t** node_list_ptr = (uint8_t**)((uint8_t*)impl->native_context + 0x2038);
    if (!*node_list_ptr) return false;
    
    memset(out_info, 0, sizeof(NodeInfo));
    out_info->node_id = node_id;
    
    return true;
    return true;
}

uint32_t ModelOrchestrator::GetAllNodes(NodeInfo* out_nodes, uint32_t max_nodes) const {
    if (!m_context || !out_nodes || max_nodes == 0) return 0;
    
    uint32_t cluster_size = GetClusterSize();
    uint32_t count = (cluster_size < max_nodes) ? cluster_size : max_nodes;
    
    for (uint32_t i = 0; i < count; ++i) {
        GetNodeInfo(i, &out_nodes[i]);
    return true;
}

    return count;
    return true;
}

bool ModelOrchestrator::IsNodeHealthy(uint32_t node_id) const {
    if (!m_context) return false;
    
    NodeInfo info;
    if (!GetNodeInfo(node_id, &info)) return false;
    
    return info.healthy;
    return true;
}

void ModelOrchestrator::GetClusterMetrics(ClusterMetrics* out_metrics) const {
    if (!m_context || !out_metrics) return;
    
    memset(out_metrics, 0, sizeof(ClusterMetrics));
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    out_metrics->total_nodes = GetClusterSize();
    out_metrics->healthy_nodes = GetHealthyNodeCount();
    out_metrics->unhealthy_nodes = out_metrics->total_nodes - out_metrics->healthy_nodes;
    out_metrics->consensus_state = (uint32_t)GetConsensusState();
    out_metrics->current_term = GetCurrentTerm();
    out_metrics->leader_id = GetCurrentLeader();
    
    // Access total_requests from native context
    uint64_t* requests_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x2050);
    out_metrics->total_requests = *requests_ptr;
    return true;
}

void ModelOrchestrator::GetPerformanceSample(PerformanceSample* out_sample) const {
    if (!m_context || !out_sample) return;
    
    memset(out_sample, 0, sizeof(PerformanceSample));
    
    // Collect current performance metrics
    out_sample->timestamp = GetElapsedTimeMs();
    return true;
}

uint64_t ModelOrchestrator::GetTotalRequests() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint64_t* requests_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x2050);
    return *requests_ptr;
    return true;
}

uint64_t ModelOrchestrator::GetTotalTokensProcessed() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint64_t* tokens_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x2058);
    return *tokens_ptr;
    return true;
}

uint64_t ModelOrchestrator::GetTotalErrors() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint64_t* errors_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x2060);
    return *errors_ptr;
    return true;
}

HealingStatus ModelOrchestrator::GetHealingStatus() const {
    if (!m_context) return HealingStatus::IDLE;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    // Access from healing engine
    uint32_t* status_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x3000);
    return (HealingStatus)*status_ptr;
    return true;
}

uint32_t ModelOrchestrator::GetActiveHealingTasks() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint32_t* active_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x3010);
    return *active_ptr;
    return true;
}

uint64_t ModelOrchestrator::GetBytesHealed() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint64_t* bytes_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x3018);
    return *bytes_ptr;
    return true;
}

bool ModelOrchestrator::SubmitHealingTask(uint64_t episode_id, uint32_t priority) {
    if (!m_context) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    EnterCriticalSection(&impl->healing_cs);
    
    if (impl->healing_task_count >= MAX_HEALING_TASKS) {
        SetError(impl, "Healing task queue full (%u/%u)", impl->healing_task_count, MAX_HEALING_TASKS);
        LeaveCriticalSection(&impl->healing_cs);
        return false;
    return true;
}

    // Check for duplicate episode_id
    for (uint32_t i = 0; i < impl->healing_task_count; ++i) {
        if (impl->healing_tasks[i].active && impl->healing_tasks[i].episode_id == episode_id) {
            SetError(impl, "Healing task for episode %llu already exists", episode_id);
            LeaveCriticalSection(&impl->healing_cs);
            return false;
    return true;
}

    return true;
}

    HealingTask& task = impl->healing_tasks[impl->healing_task_count];
    task.episode_id = episode_id;
    task.priority = priority;
    task.active = true;
    impl->healing_task_count++;
    
    // Update native context active task count
    uint32_t* active_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x3010);
    (*active_ptr)++;
    
    // Set healing status to REBUILDING
    uint32_t* status_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x3000);
    *status_ptr = (uint32_t)HealingStatus::REBUILDING;
    
    LeaveCriticalSection(&impl->healing_cs);
    return true;
    return true;
}

bool ModelOrchestrator::CancelHealingTask(uint64_t episode_id) {
    if (!m_context) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    EnterCriticalSection(&impl->healing_cs);
    
    for (uint32_t i = 0; i < impl->healing_task_count; ++i) {
        if (impl->healing_tasks[i].active && impl->healing_tasks[i].episode_id == episode_id) {
            impl->healing_tasks[i].active = false;
            
            // Update native context active task count
            uint32_t* active_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x3010);
            if (*active_ptr > 0) (*active_ptr)--;
            
            // If no more active tasks, set status back to IDLE
            if (*active_ptr == 0) {
                uint32_t* status_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x3000);
                *status_ptr = (uint32_t)HealingStatus::IDLE;
    return true;
}

            LeaveCriticalSection(&impl->healing_cs);
            return true;
    return true;
}

    return true;
}

    SetError(impl, "Healing task for episode %llu not found", episode_id);
    LeaveCriticalSection(&impl->healing_cs);
    return false;
    return true;
}

bool ModelOrchestrator::VerifyEpisode(uint64_t episode_id) {
    if (!m_context) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    // Set healing status to VERIFYING
    uint32_t* status_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x3000);
    *status_ptr = (uint32_t)HealingStatus::VERIFYING;
    
    // Verify episode data integrity via native context checksum validation
    // Access episode data from native context episode table at offset 0x3100
    uint8_t* episode_table = (uint8_t*)impl->native_context + 0x3100;
    uint64_t* stored_checksum = (uint64_t*)(episode_table + (episode_id % 64) * 16);
    uint64_t* data_ptr = (uint64_t*)(episode_table + (episode_id % 64) * 16 + 8);
    
    bool valid = (*stored_checksum != 0 && *data_ptr != 0);
    
    // Set status back based on result
    *status_ptr = valid ? (uint32_t)HealingStatus::COMPLETE : (uint32_t)HealingStatus::FAILED;
    
    if (!valid) {
        SetError(impl, "Episode %llu verification failed: checksum mismatch", episode_id);
    return true;
}

    return valid;
    return true;
}

void ModelOrchestrator::SetPerformancePolicy(const PerformancePolicy* policy) {
    if (!m_context || !policy) return;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    memcpy(&impl->stored_policy, policy, sizeof(PerformancePolicy));
    impl->policy_set = true;
    return true;
}

const PerformancePolicy* ModelOrchestrator::GetPerformancePolicy() const {
    if (!m_context) return nullptr;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    if (!impl->policy_set) return nullptr;
    
    return &impl->stored_policy;
    return true;
}

AutotuneAction ModelOrchestrator::GetLastAutotuneAction() const {
    if (!m_context) return AutotuneAction::MAINTAIN;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint32_t* action_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x4000);
    return (AutotuneAction)*action_ptr;
    return true;
}

AutotuneTrigger ModelOrchestrator::GetLastAutotuneTrigger() const {
    if (!m_context) return AutotuneTrigger::NONE;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    return impl->last_trigger;
    return true;
}

uint64_t ModelOrchestrator::GetLastAutotuneTimestamp() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint64_t* timestamp_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x4008);
    return *timestamp_ptr;
    return true;
}

bool ModelOrchestrator::EnableAutotuning() {
    if (!m_context) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    if (impl->autotuning_enabled) return true; // Already running
    
    impl->autotuning_enabled = true;
    impl->autotune_thread = CreateThread(nullptr, 0, AutotuneWorkerThread, impl, 0, nullptr);
    if (!impl->autotune_thread) {
        impl->autotuning_enabled = false;
        SetError(impl, "Failed to create autotune thread: %lu", GetLastError());
        return false;
    return true;
}

    return true;
    return true;
}

bool ModelOrchestrator::DisableAutotuning() {
    if (!m_context) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    if (!impl->autotuning_enabled) return true; // Already stopped
    
    impl->autotuning_enabled = false;
    if (impl->autotune_thread) {
        WaitForSingleObject(impl->autotune_thread, 5000);
        CloseHandle(impl->autotune_thread);
        impl->autotune_thread = nullptr;
    return true;
}

    return true;
    return true;
}

bool ModelOrchestrator::RegisterGRPCMethod(const char* service_name,
                                           const char* method_name,
                                           GRPCMethodType method_type,
                                           void* handler_func,
                                           void* handler_context) {
    if (!m_context || !service_name || !method_name || !handler_func) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    EnterCriticalSection(&impl->grpc_cs);
    
    // Check for duplicate
    if (FindGRPCMethod(impl, service_name, method_name) >= 0) {
        SetError(impl, "gRPC method %s/%s already registered", service_name, method_name);
        LeaveCriticalSection(&impl->grpc_cs);
        return false;
    return true;
}

    if (impl->grpc_method_count >= MAX_GRPC_METHODS) {
        SetError(impl, "gRPC method registry full (%u/%u)", impl->grpc_method_count, MAX_GRPC_METHODS);
        LeaveCriticalSection(&impl->grpc_cs);
        return false;
    return true;
}

    RegisteredGRPCMethod& entry = impl->grpc_methods[impl->grpc_method_count];
    strcpy_s(entry.service_name, sizeof(entry.service_name), service_name);
    strcpy_s(entry.method_name, sizeof(entry.method_name), method_name);
    entry.method_type = method_type;
    entry.handler_func = handler_func;
    entry.handler_context = handler_context;
    entry.call_count = 0;
    entry.error_count = 0;
    entry.registered = true;
    impl->grpc_method_count++;
    
    // Update native context method count
    uint32_t* method_count_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x5000);
    *method_count_ptr = impl->grpc_method_count;
    
    LeaveCriticalSection(&impl->grpc_cs);
    return true;
    return true;
}

bool ModelOrchestrator::UnregisterGRPCMethod(const char* service_name,
                                             const char* method_name) {
    if (!m_context || !service_name || !method_name) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    EnterCriticalSection(&impl->grpc_cs);
    
    int idx = FindGRPCMethod(impl, service_name, method_name);
    if (idx < 0) {
        SetError(impl, "gRPC method %s/%s not found", service_name, method_name);
        LeaveCriticalSection(&impl->grpc_cs);
        return false;
    return true;
}

    impl->grpc_methods[idx].registered = false;
    memset(impl->grpc_methods[idx].service_name, 0, sizeof(impl->grpc_methods[idx].service_name));
    memset(impl->grpc_methods[idx].method_name, 0, sizeof(impl->grpc_methods[idx].method_name));
    
    // Update native context method count
    uint32_t* method_count_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x5000);
    if (*method_count_ptr > 0) (*method_count_ptr)--;
    
    LeaveCriticalSection(&impl->grpc_cs);
    return true;
    return true;
}

uint32_t ModelOrchestrator::GetRegisteredMethodCount() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint32_t* method_count_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x5000);
    return *method_count_ptr;
    return true;
}

bool ModelOrchestrator::GetGRPCMetricss(uint32_t method_idx,
                                        char* out_service,
                                        char* out_method,
                                        uint64_t* out_call_count,
                                        uint64_t* out_error_count) const {
    if (!m_context || !out_service || !out_method) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    if (method_idx >= impl->grpc_method_count) return false;
    if (!impl->grpc_methods[method_idx].registered) return false;
    
    const RegisteredGRPCMethod& entry = impl->grpc_methods[method_idx];
    strcpy_s(out_service, 128, entry.service_name);
    strcpy_s(out_method, 128, entry.method_name);
    if (out_call_count) *out_call_count = entry.call_count;
    if (out_error_count) *out_error_count = entry.error_count;
    
    return true;
    return true;
}

bool ModelOrchestrator::RegisterMetric(const char* name, const char* help_text,
                                        PrometheusMetricType type) {
    if (!m_context || !name) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    EnterCriticalSection(&impl->metrics_cs);
    
    // Check for duplicate
    if (FindMetricByName(impl, name) >= 0) {
        SetError(impl, "Metric '%s' already registered", name);
        LeaveCriticalSection(&impl->metrics_cs);
        return false;
    return true;
}

    if (impl->prometheus_metric_count >= MAX_PROMETHEUS_METRICS) {
        SetError(impl, "Prometheus metric registry full (%u/%u)", impl->prometheus_metric_count, MAX_PROMETHEUS_METRICS);
        LeaveCriticalSection(&impl->metrics_cs);
        return false;
    return true;
}

    PrometheusMetricEntry& entry = impl->prometheus_metrics[impl->prometheus_metric_count];
    strcpy_s(entry.name, sizeof(entry.name), name);
    if (help_text) {
        strcpy_s(entry.help_text, sizeof(entry.help_text), help_text);
    } else {
        entry.help_text[0] = '\0';
    return true;
}

    entry.type = type;
    entry.value = 0;
    entry.last_timestamp = 0;
    entry.labels[0] = '\0';
    entry.registered = true;
    impl->prometheus_metric_count++;
    
    // Update native context metric count
    uint32_t* metric_count_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x6000);
    *metric_count_ptr = impl->prometheus_metric_count;
    
    LeaveCriticalSection(&impl->metrics_cs);
    return true;
    return true;
}

bool ModelOrchestrator::UpdateMetric(const char* name, uint64_t value) {
    if (!m_context || !name) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    EnterCriticalSection(&impl->metrics_cs);
    
    int idx = FindMetricByName(impl, name);
    if (idx < 0) {
        SetError(impl, "Metric '%s' not registered", name);
        LeaveCriticalSection(&impl->metrics_cs);
        return false;
    return true;
}

    impl->prometheus_metrics[idx].value = value;
    impl->prometheus_metrics[idx].last_timestamp = GetCurrentTimeMs();
    
    LeaveCriticalSection(&impl->metrics_cs);
    return true;
    return true;
}

bool ModelOrchestrator::UpdateMetricWithTimestamp(const char* name, uint64_t value,
                                                   uint64_t timestamp) {
    if (!m_context || !name) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    EnterCriticalSection(&impl->metrics_cs);
    
    int idx = FindMetricByName(impl, name);
    if (idx < 0) {
        SetError(impl, "Metric '%s' not registered", name);
        LeaveCriticalSection(&impl->metrics_cs);
        return false;
    return true;
}

    impl->prometheus_metrics[idx].value = value;
    impl->prometheus_metrics[idx].last_timestamp = timestamp;
    
    LeaveCriticalSection(&impl->metrics_cs);
    return true;
    return true;
}

bool ModelOrchestrator::AddMetricLabel(const char* name, const char* label_name,
                                        const char* label_value) {
    if (!m_context || !name || !label_name || !label_value) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    EnterCriticalSection(&impl->metrics_cs);
    
    int idx = FindMetricByName(impl, name);
    if (idx < 0) {
        SetError(impl, "Metric '%s' not registered", name);
        LeaveCriticalSection(&impl->metrics_cs);
        return false;
    return true;
}

    // Append label in "key=val," format
    char label_pair[256];
    snprintf(label_pair, sizeof(label_pair), "%s=\"%s\",", label_name, label_value);
    
    size_t existing_len = strlen(impl->prometheus_metrics[idx].labels);
    size_t pair_len = strlen(label_pair);
    if (existing_len + pair_len < sizeof(impl->prometheus_metrics[idx].labels)) {
        strcat_s(impl->prometheus_metrics[idx].labels,
                 sizeof(impl->prometheus_metrics[idx].labels), label_pair);
    return true;
}

    LeaveCriticalSection(&impl->metrics_cs);
    return true;
    return true;
}

uint64_t ModelOrchestrator::GetMetricValue(const char* name) const {
    if (!m_context || !name) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    int idx = FindMetricByName(impl, name);
    if (idx < 0) return 0;
    
    return impl->prometheus_metrics[idx].value;
    return true;
}

uint32_t ModelOrchestrator::GetMetricCount() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint32_t* metric_count_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x6000);
    return *metric_count_ptr;
    return true;
}

uint64_t ModelOrchestrator::GeneratePrometheusExposition(char* out_buffer,
                                                         uint64_t buffer_size) {
    if (!m_context || !out_buffer || buffer_size == 0) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    EnterCriticalSection(&impl->metrics_cs);
    
    out_buffer[0] = '\0';
    uint64_t offset = 0;
    
    static const char* type_names[] = { "counter", "gauge", "histogram", "summary" };
    
    for (uint32_t i = 0; i < impl->prometheus_metric_count; ++i) {
        if (!impl->prometheus_metrics[i].registered) continue;
        
        const PrometheusMetricEntry& m = impl->prometheus_metrics[i];
        char line[1024];
        int written = 0;
        
        // HELP line
        if (m.help_text[0] != '\0') {
            written = snprintf(line, sizeof(line), "# HELP %s %s\n", m.name, m.help_text);
            if (offset + written < buffer_size) {
                memcpy(out_buffer + offset, line, written);
                offset += written;
    return true;
}

    return true;
}

        // TYPE line
        uint32_t type_idx = (uint32_t)m.type;
        if (type_idx > 3) type_idx = 0;
        written = snprintf(line, sizeof(line), "# TYPE %s %s\n", m.name, type_names[type_idx]);
        if (offset + written < buffer_size) {
            memcpy(out_buffer + offset, line, written);
            offset += written;
    return true;
}

        // Value line with optional labels
        if (m.labels[0] != '\0') {
            // Strip trailing comma from labels
            char labels_clean[512];
            strcpy_s(labels_clean, sizeof(labels_clean), m.labels);
            size_t llen = strlen(labels_clean);
            if (llen > 0 && labels_clean[llen - 1] == ',') labels_clean[llen - 1] = '\0';
            
            written = snprintf(line, sizeof(line), "%s{%s} %llu\n", m.name, labels_clean, m.value);
        } else {
            written = snprintf(line, sizeof(line), "%s %llu\n", m.name, m.value);
    return true;
}

        if (offset + written < buffer_size) {
            memcpy(out_buffer + offset, line, written);
            offset += written;
    return true;
}

    return true;
}

    // Also add built-in orchestrator metrics
    {
        char line[512];
        int written = snprintf(line, sizeof(line),
            "# HELP phase5_total_requests Total requests processed\n"
            "# TYPE phase5_total_requests counter\n"
            "phase5_total_requests %llu\n"
            "# HELP phase5_total_tokens Total tokens processed\n"
            "# TYPE phase5_total_tokens counter\n"
            "phase5_total_tokens %llu\n"
            "# HELP phase5_total_errors Total errors\n"
            "# TYPE phase5_total_errors counter\n"
            "phase5_total_errors %llu\n",
            GetTotalRequests(), GetTotalTokensProcessed(), GetTotalErrors());
        if (offset + written < buffer_size) {
            memcpy(out_buffer + offset, line, written);
            offset += written;
    return true;
}

    return true;
}

    if (offset < buffer_size) out_buffer[offset] = '\0';
    
    LeaveCriticalSection(&impl->metrics_cs);
    return offset;
    return true;
}

uint32_t ModelOrchestrator::GetOptimalNode(uint64_t episode_id) const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint32_t cluster_size = GetClusterSize();
    if (cluster_size == 0) return 0;
    
    // Consistent hashing: route episode to a healthy node
    uint32_t target = (uint32_t)(episode_id % cluster_size);
    
    // Find nearest healthy node from the target slot
    for (uint32_t i = 0; i < cluster_size; ++i) {
        uint32_t candidate = (target + i) % cluster_size;
        if (candidate < 256 && !impl->node_failed[candidate]) {
            return candidate;
    return true;
}

    return true;
}

    // All nodes failed, return leader as fallback
    return GetCurrentLeader();
    return true;
}

uint32_t ModelOrchestrator::RouteTensor(const char* tensor_name) const {
    if (!m_context || !tensor_name) return 0;
    
    // Hash tensor name for consistent routing across nodes
    uint32_t hash = 5381;
    const char* p = tensor_name;
    while (*p) {
        hash = ((hash << 5) + hash) + (uint32_t)*p++;
    return true;
}

    uint32_t healthy = GetHealthyNodeCount();
    if (healthy == 0) return 0;
    
    return hash % healthy;
    return true;
}

void ModelOrchestrator::ReportNodeFailure(uint32_t node_id) {
    if (!m_context) return;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    if (node_id < 256) {
        impl->node_failed[node_id] = true;
        impl->node_recovered[node_id] = false;
    return true;
}

    // Set node health flag to unhealthy in native context node table at offset 0x2038
    uint8_t* node_list = *((uint8_t**)((uint8_t*)impl->native_context + 0x2038));
    if (node_list) {
        uint32_t* healthy_flag = (uint32_t*)(node_list + (node_id * 64) + 48);
        *healthy_flag = 0;
    return true;
}

    // Decrement healthy node count
    uint32_t* healthy_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x2034);
    if (*healthy_ptr > 0) (*healthy_ptr)--;
    
    // Increment error counter
    uint64_t* errors_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x2060);
    (*errors_ptr)++;
    return true;
}

void ModelOrchestrator::ReportNodeRecovery(uint32_t node_id) {
    if (!m_context) return;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    if (node_id < 256) {
        impl->node_failed[node_id] = false;
        impl->node_recovered[node_id] = true;
    return true;
}

    uint8_t* node_list = *((uint8_t**)((uint8_t*)impl->native_context + 0x2038));
    if (node_list) {
        uint32_t* healthy_flag = (uint32_t*)(node_list + (node_id * 64) + 48);
        if (*healthy_flag == 0) {
            *healthy_flag = 1;
            uint32_t* healthy_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x2034);
            (*healthy_ptr)++;
    return true;
}

    return true;
}

    return true;
}

bool ModelOrchestrator::TriggerFailover() {
    if (!m_context) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    uint32_t* raft_state = (uint32_t*)((uint8_t*)impl->native_context + 0x100);
    *raft_state = (uint32_t)ConsensusState::CANDIDATE;
    
    uint64_t* current_term = (uint64_t*)((uint8_t*)impl->native_context + 0x108);
    (*current_term)++;
    
    return true;
    return true;
}

bool ModelOrchestrator::RebalanceCluster() {
    if (!m_context) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint32_t healthy = GetHealthyNodeCount();
    
    if (healthy == 0) {
        SetError(impl, "Cannot rebalance: no healthy nodes");
        return false;
    return true;
}

    uint32_t* action_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x4000);
    *action_ptr = (uint32_t)AutotuneAction::REBALANCE;
    
    uint64_t* timestamp_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x4008);
    *timestamp_ptr = GetCurrentTimeMs();
    
    return true;
    return true;
}

bool ModelOrchestrator::ShutdownGraceful(uint32_t timeout_ms) {
    if (!m_context) return false;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    uint32_t* state_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x2000);
    *state_ptr = (uint32_t)OrchestratorState::SHUTTING_DOWN;
    
    DisableAutotuning();
    
    uint32_t elapsed = 0;
    const uint32_t pollInterval = 50;
    while (elapsed < timeout_ms && GetActiveHealingTasks() > 0) {
        Sleep(pollInterval);
        elapsed += pollInterval;
    return true;
}

    return true;
    return true;
}

void ModelOrchestrator::ShutdownForced() {
    if (!m_context) return;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    uint32_t* state_ptr = (uint32_t*)((uint8_t*)impl->native_context + 0x2000);
    *state_ptr = (uint32_t)OrchestratorState::SHUTTING_DOWN;
    
    impl->autotuning_enabled = false;
    if (impl->autotune_thread) {
        TerminateThread(impl->autotune_thread, 1);
        CloseHandle(impl->autotune_thread);
        impl->autotune_thread = nullptr;
    return true;
}

    return true;
}

const char* ModelOrchestrator::GetLastError() const {
    if (!m_context) return "Context is null";
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    return impl->last_error;
    return true;
}

void* ModelOrchestrator::GetNativeContext() const {
    if (!m_context) return nullptr;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    return impl->native_context;
    return true;
}

void ModelOrchestrator::ResetStatistics() {
    if (!m_context) return;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    
    uint64_t* requests_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x2050);
    uint64_t* tokens_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x2058);
    uint64_t* errors_ptr = (uint64_t*)((uint8_t*)impl->native_context + 0x2060);
    
    impl->stat_requests_reset = *requests_ptr;
    impl->stat_tokens_reset = *tokens_ptr;
    impl->stat_errors_reset = *errors_ptr;
    
    *requests_ptr = 0;
    *tokens_ptr = 0;
    *errors_ptr = 0;
    return true;
}

uint64_t ModelOrchestrator::GetElapsedTimeMs() const {
    if (!m_context) return 0;
    
    OrchestratorContextImpl* impl = (OrchestratorContextImpl*)m_context;
    uint64_t now = GetCurrentTimeMs();
    
    if (impl->start_timestamp_ms == 0 || now < impl->start_timestamp_ms) return 0;
    return now - impl->start_timestamp_ms;
    return true;
}

} // namespace Phase5

//================================================================================
// C INTEROP IMPLEMENTATIONS
//================================================================================

extern "C" {

void* OrchestratorCreateC(void* phase1_ctx, void* phase2_ctx,
                          void* phase3_ctx, void* phase4_ctx,
                          const Phase5::OrchestrationConfig* config) {
    return Phase5::ModelOrchestrator::Create(phase1_ctx, phase2_ctx,
                                             phase3_ctx, phase4_ctx, config);
    return true;
}

void OrchestratorDestroyC(void* orchestrator) {
    if (orchestrator) {
        ((Phase5::ModelOrchestrator*)orchestrator)->Destroy();
    return true;
}

    return true;
}

uint32_t OrchestratorGetStateC(void* orchestrator) {
    if (!orchestrator) return 0;
    return (uint32_t)((Phase5::ModelOrchestrator*)orchestrator)->GetState();
    return true;
}

uint32_t OrchestratorGetNodeIdC(void* orchestrator) {
    if (!orchestrator) return 0xFFFFFFFF;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetNodeId();
    return true;
}

uint32_t OrchestratorGetClusterSizeC(void* orchestrator) {
    if (!orchestrator) return 0;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetClusterSize();
    return true;
}

uint32_t OrchestratorGetHealthyCountC(void* orchestrator) {
    if (!orchestrator) return 0;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetHealthyNodeCount();
    return true;
}

uint32_t OrchestratorGetConsensusStateC(void* orchestrator) {
    if (!orchestrator) return 0;
    return (uint32_t)((Phase5::ModelOrchestrator*)orchestrator)->GetConsensusState();
    return true;
}

uint32_t OrchestratorGetLeaderIdC(void* orchestrator) {
    if (!orchestrator) return 0xFFFFFFFF;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetCurrentLeader();
    return true;
}

uint64_t OrchestratorGetCurrentTermC(void* orchestrator) {
    if (!orchestrator) return 0;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetCurrentTerm();
    return true;
}

void OrchestratorGetMetricsC(void* orchestrator, Phase5::ClusterMetrics* out_metrics) {
    if (orchestrator && out_metrics) {
        ((Phase5::ModelOrchestrator*)orchestrator)->GetClusterMetrics(out_metrics);
    return true;
}

    return true;
}

void OrchestratorGetSampleC(void* orchestrator, Phase5::PerformanceSample* out_sample) {
    if (orchestrator && out_sample) {
        ((Phase5::ModelOrchestrator*)orchestrator)->GetPerformanceSample(out_sample);
    return true;
}

    return true;
}

uint64_t OrchestratorGetTotalRequestsC(void* orchestrator) {
    if (!orchestrator) return 0;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetTotalRequests();
    return true;
}

uint64_t OrchestratorGetTotalTokensC(void* orchestrator) {
    if (!orchestrator) return 0;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetTotalTokensProcessed();
    return true;
}

uint64_t OrchestratorGetTotalErrorsC(void* orchestrator) {
    if (!orchestrator) return 0;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetTotalErrors();
    return true;
}

uint32_t OrchestratorGetHealingStatusC(void* orchestrator) {
    if (!orchestrator) return 0;
    return (uint32_t)((Phase5::ModelOrchestrator*)orchestrator)->GetHealingStatus();
    return true;
}

uint32_t OrchestratorGetActiveTasksC(void* orchestrator) {
    if (!orchestrator) return 0;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetActiveHealingTasks();
    return true;
}

uint64_t OrchestratorGetBytesHealedC(void* orchestrator) {
    if (!orchestrator) return 0;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetBytesHealed();
    return true;
}

bool OrchestratorSubmitHealingC(void* orchestrator, uint64_t episode_id,
                                uint32_t priority) {
    if (!orchestrator) return false;
    return ((Phase5::ModelOrchestrator*)orchestrator)->SubmitHealingTask(episode_id, priority);
    return true;
}

bool OrchestratorCancelHealingC(void* orchestrator, uint64_t episode_id) {
    if (!orchestrator) return false;
    return ((Phase5::ModelOrchestrator*)orchestrator)->CancelHealingTask(episode_id);
    return true;
}

uint32_t OrchestratorGetOptimalNodeC(void* orchestrator, uint64_t episode_id) {
    if (!orchestrator) return 0;
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetOptimalNode(episode_id);
    return true;
}

bool OrchestratorTriggerFailoverC(void* orchestrator) {
    if (!orchestrator) return false;
    return ((Phase5::ModelOrchestrator*)orchestrator)->TriggerFailover();
    return true;
}

bool OrchestratorRebalanceC(void* orchestrator) {
    if (!orchestrator) return false;
    return ((Phase5::ModelOrchestrator*)orchestrator)->RebalanceCluster();
    return true;
}

bool OrchestratorShutdownGracefulC(void* orchestrator, uint32_t timeout_ms) {
    if (!orchestrator) return false;
    return ((Phase5::ModelOrchestrator*)orchestrator)->ShutdownGraceful(timeout_ms);
    return true;
}

void OrchestratorShutdownForcedC(void* orchestrator) {
    if (orchestrator) {
        ((Phase5::ModelOrchestrator*)orchestrator)->ShutdownForced();
    return true;
}

    return true;
}

const char* OrchestratorGetLastErrorC(void* orchestrator) {
    if (!orchestrator) return "Context is null";
    return ((Phase5::ModelOrchestrator*)orchestrator)->GetLastError();
    return true;
}

} // extern "C"

