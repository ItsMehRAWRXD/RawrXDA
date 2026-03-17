# WEEK 2-3 BUILD, DEPLOYMENT & INTEGRATION GUIDE

**Version**: 1.0  
**Status**: Production Ready  
**Deployment Target**: 16-Node Cluster  

---

## Table of Contents

1. [Quick Start (5 Minutes)](#quick-start)
2. [Build Instructions](#build-instructions)
3. [Deployment Scenarios](#deployment-scenarios)
4. [API Reference](#api-reference)
5. [Integration Examples](#integration-examples)
6. [Monitoring & Troubleshooting](#monitoring--troubleshooting)
7. [Performance Tuning](#performance-tuning)

---

## Quick Start

### Prerequisites
- Windows 10/11 x86-64
- MASM64 (ml64.exe) from Visual Studio
- Week1 deliverable built and tested
- Phases 1-4 object files
- Winsock2 libraries (ws2_32.lib, mswsock.lib)

### Build Week 2-3 in 5 Minutes

```powershell
# Step 1: Assemble (15 seconds)
cd E:\
ml64.exe /c /O2 /Zi /W3 /nologo Week2_3_Master_Complete.asm

# Step 2: Link (10 seconds)
link /DLL /OUT:Week2_3_Swarm.dll /OPT:REF /OPT:ICF `
    Week2_3_Master_Complete.obj `
    Week1_Deliverable.obj `
    Phase4_Master_Complete.obj `
    Phase3_Master.obj `
    Phase2_Master.obj `
    Phase1_Foundation.obj `
    ws2_32.lib mswsock.lib kernel32.lib user32.lib advapi32.lib

# Step 3: Verify exports (5 seconds)
dumpbin /EXPORTS Week2_3_Swarm.dll | findstr "Week23 Raft Gossip Router"

# Step 4: Test (30 seconds)
.\Week2_3_Test_Harness.exe
# Expected: 12/12 tests pass
```

**Total**: ~1 minute for full build + test

---

## Build Instructions

### Full Build Process

```powershell
# ============================================
# STEP 1: Assemble Week2-3 ASM
# ============================================

$asmFile = "E:\Week2_3_Master_Complete.asm"
$objFile = "E:\Week2_3_Master_Complete.obj"

# Compile with optimizations
ml64.exe /c /O2 /Zi /W3 /nologo $asmFile

# Check for errors
if (-not (Test-Path $objFile)) {
    Write-Error "ASM compilation failed"
    exit 1
}

Write-Host "[OK] Week2_3_Master_Complete.obj created ($(Get-Item $objFile).Length / 1KB) KB)"

# ============================================
# STEP 2: Link with dependencies
# ============================================

$dependencies = @(
    "E:\Week2_3_Master_Complete.obj",
    "E:\Week1_Deliverable.obj",
    "E:\Phase4_Master_Complete.obj",
    "E:\Phase3_Master.obj",
    "E:\Phase2_Master.obj",
    "E:\Phase1_Foundation.obj"
)

$libs = @(
    "ws2_32.lib",    # Windows Sockets
    "mswsock.lib",   # IOCP support
    "kernel32.lib",  # Core Win32
    "user32.lib",    # UI (if needed)
    "advapi32.lib"   # Advanced services
)

# Combine dependencies and libraries
$linkerArgs = @("/DLL", "/OUT:Week2_3_Swarm.dll", "/OPT:REF", "/OPT:ICF", "/NOLOGO")

foreach ($dep in $dependencies) {
    if (-not (Test-Path $dep)) {
        Write-Error "Missing dependency: $dep"
        exit 1
    }
    $linkerArgs += $dep
}

foreach ($lib in $libs) {
    $linkerArgs += $lib
}

# Link
write-Host "Linking Week2_3_Swarm.dll..."
& link @linkerArgs

# Verify
if (Test-Path "E:\Week2_3_Swarm.dll") {
    Write-Host "[OK] Week2_3_Swarm.dll created ($(Get-Item E:\Week2_3_Swarm.dll).Length / 1024) KB)"
} else {
    Write-Error "DLL linking failed"
    exit 1
}

# ============================================
# STEP 3: Verify exports
# ============================================

Write-Host "`nVerifying DLL exports..."
$exports = dumpbin /EXPORTS "E:\Week2_3_Swarm.dll"

$requiredExports = @(
    "Week23Initialize",
    "Week23Shutdown",
    "IOCPWorkerThread",
    "RaftEventLoop",
    "GossipProtocolThread",
    "InferenceRouterThread",
    "JoinCluster",
    "LeaveCluster",
    "SubmitInferenceRequest"
)

foreach ($export in $requiredExports) {
    if ($exports -match $export) {
        Write-Host "[OK] $export exported"
    } else {
        Write-Error "Missing export: $export"
        exit 1
    }
}

Write-Host "`n[SUCCESS] Week2-3 build complete!"
Write-Host "Output: E:\Week2_3_Swarm.dll"
```

### Static Library (for Week 4+)

```powershell
# Create static library for linking into later phases
lib /OUT:E:\Week2_3_Swarm.lib `
    Week2_3_Master_Complete.obj `
    Week1_Deliverable.obj
```

---

## Deployment Scenarios

### Scenario 1: Single-Node (Development/Testing)

Perfect for development and testing consensus logic.

```powershell
# Set environment variables
$env:WEEK2_3_NODE_ID = "0"
$env:WEEK2_3_CLUSTER_SIZE = "1"

# Run
.\YourApplication.exe

# Expected output:
# [WEEK2-3] Swarm Orchestrator initializing...
# [WEEK2-3] Joined cluster with 1 nodes
# [WEEK2-3] Elected as leader (term 1)
# [WEEK2-3] Assigned shards for single node
```

### Scenario 2: 3-Node Cluster (Consensus Testing)

Tests full Raft consensus and failure recovery.

**Node 0 (Leader):**
```powershell
$env:WEEK2_3_NODE_ID = "0"
$env:WEEK2_3_LOCAL_IP = "10.0.0.10"
$env:WEEK2_3_LOCAL_PORT = "31337"
$env:WEEK2_3_CLUSTER_SIZE = "3"
$env:WEEK2_3_SEED_NODES = "10.0.0.10:31337"

.\YourApplication.exe
```

**Node 1 (Follower):**
```powershell
$env:WEEK2_3_NODE_ID = "1"
$env:WEEK2_3_LOCAL_IP = "10.0.0.11"
$env:WEEK2_3_LOCAL_PORT = "31337"
$env:WEEK2_3_CLUSTER_SIZE = "3"
$env:WEEK2_3_SEED_NODES = "10.0.0.10:31337"

.\YourApplication.exe
```

**Node 2 (Follower):**
```powershell
$env:WEEK2_3_NODE_ID = "2"
$env:WEEK2_3_LOCAL_IP = "10.0.0.12"
$env:WEEK2_3_LOCAL_PORT = "31337"
$env:WEEK2_3_CLUSTER_SIZE = "3"
$env:WEEK2_3_SEED_NODES = "10.0.0.10:31337"

.\YourApplication.exe
```

### Scenario 3: 16-Node Production Cluster

Full production deployment with shard rebalancing.

**Cluster Configuration (cluster-config.json):**
```json
{
  "cluster_id": "prod-cluster-1",
  "max_nodes": 16,
  "seed_nodes": [
    "10.0.0.10:31337",
    "10.0.0.11:31337",
    "10.0.0.12:31337"
  ],
  "shard_replicas": 3,
  "raft_heartbeat_ms": 50,
  "election_timeout_min_ms": 150,
  "election_timeout_max_ms": 300,
  "rebalance_check_interval_s": 10,
  "inference_router_strategy": "least_loaded"
}
```

**Deployment Script:**
```powershell
# Deploy to all 16 nodes
for ($i = 0; $i -lt 16; $i++) {
    $ip = "10.0.0.$(10 + $i)"
    
    $env:WEEK2_3_NODE_ID = $i
    $env:WEEK2_3_LOCAL_IP = $ip
    $env:WEEK2_3_CLUSTER_SIZE = "16"
    $env:WEEK2_3_SEED_NODES = "10.0.0.10:31337,10.0.0.11:31337,10.0.0.12:31337"
    
    # Copy to remote node
    Copy-Item -Path "Week2_3_Swarm.dll" -Destination "\\$ip\c$\app\"
    
    # Start service on remote node
    Invoke-Command -ComputerName $ip -ScriptBlock {
        cd C:\app
        .\YourApplication.exe | Out-File -Append "node_$($env:WEEK2_3_NODE_ID).log"
    }
}

Write-Host "16-node cluster deployed!"
```

---

## API Reference

### Core Functions

#### Week23Initialize

**Purpose**: Initialize the entire Week 2-3 swarm orchestrator system.

**Signature**:
```c
WEEK2_3_CONTEXT* Week23Initialize(
    WEEK1_CONTEXT* week1_ctx,
    CLUSTER_CONFIG* config     // NULL for standalone
);
```

**Parameters**:
- `week1_ctx`: Week 1 foundation context (required)
- `config`: Cluster configuration JSON or NULL for single-node

**Returns**:
- `WEEK2_3_CONTEXT*` on success
- `NULL` on failure

**Example**:
```cpp
auto ctx = Week23Initialize(week1_ctx, nullptr);
if (!ctx) {
    fprintf(stderr, "Failed to initialize\n");
    return 1;
}
```

**Implementation Details**:
- Allocates 8KB context structure
- Initializes 4 critical sections (thread safety)
- Creates IOCP with 8 worker threads
- Binds TCP (31337) and UDP (31338)
- Initializes Raft consensus engine
- Initializes consistent hash ring for shards
- Starts 4 background threads

---

#### SubmitInferenceRequest

**Purpose**: Queue an inference request for execution on the cluster.

**Signature**:
```c
int SubmitInferenceRequest(
    WEEK2_3_CONTEXT* ctx,
    INFERENCE_REQUEST* request
);
```

**Parameters**:
- `ctx`: Week 2-3 context
- `request`: Populated inference request

**Request Structure**:
```c
typedef struct {
    uint64_t request_id;           // Unique ID
    uint32_t client_node;          // Sending node
    uint64_t model_id;             // Which model
    uint64_t* prompt_tokens;       // Input tokens
    uint32_t prompt_len;           // Token count
    uint32_t max_tokens;           // Max output
    float temperature;
    float top_p;
    uint32_t top_k;
    uint64_t target_shard;         // Target shard (or ~0 for auto)
    void (*completion_cb)(void* ctx, void* result);
    void* callback_context;
} INFERENCE_REQUEST;
```

**Returns**:
- `1` on success (queued for routing)
- `0` on failure (no capacity or invalid request)

**Example**:
```cpp
INFERENCE_REQUEST req = {
    .request_id = 1,
    .model_id = 1,
    .prompt_tokens = tokens,
    .prompt_len = 5,
    .max_tokens = 100,
    .temperature = 0.7f,
    .top_p = 0.9f,
    .target_shard = ~0ULL,  // Auto-assign
    .completion_cb = on_complete,
    .callback_context = user_data
};

SubmitInferenceRequest(ctx, &req);
```

**Routing Logic**:
1. Determine target shard (by model_id % total_shards)
2. Find nodes hosting shard (primary + replicas)
3. Select best node based on:
   - Queue depth
   - Recent latency
   - Node capacity
4. Add to node's pending queue
5. Return immediately (non-blocking)

---

#### JoinCluster

**Purpose**: Join an existing cluster or bootstrap new cluster.

**Signature**:
```c
int JoinCluster(
    WEEK2_3_CONTEXT* ctx,
    CLUSTER_CONFIG* config
);
```

**Config Structure**:
```c
typedef struct {
    const char* seed_nodes[3];  // IP:port of existing nodes
    uint32_t seed_count;
    uint32_t target_cluster_size;
    uint64_t cluster_id;
} CLUSTER_CONFIG;
```

**Returns**: 1 on success, 0 on failure

**Procedure**:
1. Contact seed nodes
2. Discover cluster topology
3. Get shard assignments
4. Sync Raft log
5. Wait for quorum acknowledgment
6. Mark as ACTIVE

---

#### LeaveCluster

**Purpose**: Gracefully remove this node from cluster.

**Signature**:
```c
int LeaveCluster(WEEK2_3_CONTEXT* ctx);
```

**Procedure**:
1. Transfer all hosted shards to replicas
2. Wait for replication
3. Send departure notification
4. Close all connections
5. Stop background threads

---

### Thread Entry Points

All thread functions are exported for testing and can be called directly:

```c
// IOCP worker thread (call 8 times)
DWORD WINAPI IOCPWorkerThread(void* ctx);

// Raft consensus engine
DWORD WINAPI RaftEventLoop(void* ctx);

// Gossip/membership protocol
DWORD WINAPI GossipProtocolThread(void* ctx);

// Inference request router
DWORD WINAPI InferenceRouterThread(void* ctx);
```

---

## Integration Examples

### Example 1: Initialize and Join 3-Node Cluster (C++)

```cpp
#include <stdio.h>
#include <stdint.h>
#include <windows.h>

// Forward declarations from Week1 & Week2-3
extern "C" {
    typedef void WEEK1_CONTEXT;
    typedef void WEEK2_3_CONTEXT;
    typedef struct {
        const char* seed_nodes[3];
        uint32_t seed_count;
    } CLUSTER_CONFIG;
    
    WEEK1_CONTEXT* Week1Initialize(void);
    WEEK2_3_CONTEXT* Week23Initialize(WEEK1_CONTEXT*, CLUSTER_CONFIG*);
    int JoinCluster(WEEK2_3_CONTEXT*, CLUSTER_CONFIG*);
}

int main() {
    printf("[*] Initializing Week 1 foundation...\n");
    auto week1 = Week1Initialize();
    if (!week1) {
        fprintf(stderr, "Week1 init failed\n");
        return 1;
    }
    
    printf("[*] Initializing Week 2-3 swarm...\n");
    CLUSTER_CONFIG config = {
        .seed_nodes = {
            "10.0.0.10:31337",
            "10.0.0.11:31337",
            "10.0.0.12:31337"
        },
        .seed_count = 3
    };
    
    auto ctx = Week23Initialize(week1, &config);
    if (!ctx) {
        fprintf(stderr, "Week2-3 init failed\n");
        return 1;
    }
    
    printf("[+] Swarm initialized successfully!\n");
    printf("[+] Node joined cluster\n");
    
    // Keep running
    while (1) {
        Sleep(1000);
    }
    
    return 0;
}
```

### Example 2: Submit Inference Requests

```cpp
#include <stdint.h>
#include <stdio.h>

extern "C" {
    typedef struct {
        uint64_t request_id;
        uint32_t client_node;
        uint64_t model_id;
        uint64_t* prompt_tokens;
        uint32_t prompt_len;
        uint32_t max_tokens;
        float temperature;
        float top_p;
        uint32_t top_k;
        uint64_t target_shard;
        void (*completion_cb)(void* ctx, void* result);
        void* callback_context;
    } INFERENCE_REQUEST;
    
    typedef void WEEK2_3_CONTEXT;
    int SubmitInferenceRequest(WEEK2_3_CONTEXT*, INFERENCE_REQUEST*);
}

// Completion callback
void on_inference_complete(void* ctx, void* result) {
    printf("[+] Inference complete: %p\n", result);
}

int main() {
    auto ctx = (WEEK2_3_CONTEXT*)0x12345678;  // Passed from init
    
    uint64_t tokens[] = {1, 2, 3, 4, 5};
    
    INFERENCE_REQUEST req = {
        .request_id = 1,
        .model_id = 1,
        .prompt_tokens = tokens,
        .prompt_len = 5,
        .max_tokens = 100,
        .temperature = 0.7f,
        .top_p = 0.9f,
        .top_k = 40,
        .target_shard = ~0ULL,  // Auto
        .completion_cb = on_inference_complete,
        .callback_context = nullptr
    };
    
    int result = SubmitInferenceRequest(ctx, &req);
    if (result) {
        printf("[+] Request queued for routing\n");
    } else {
        printf("[-] Request rejected\n");
    }
    
    return 0;
}
```

### Example 3: Monitor Cluster Status (PowerShell)

```powershell
# Connect to cluster node and query status

$nodeIPs = @("10.0.0.10", "10.0.0.11", "10.0.0.12")

foreach ($ip in $nodeIPs) {
    Write-Host "Querying node $ip..."
    
    # Call Week2-3 API via inter-process communication
    # (Implementation: Named pipes or TCP socket to :31337)
    
    $result = Invoke-RestMethod -Uri "http://$ip:31337/status" -ErrorAction SilentlyContinue
    
    if ($result) {
        Write-Host "  Node ID: $($result.node_id)"
        Write-Host "  State: $($result.state)"
        Write-Host "  Shards: $($result.shard_count)"
        Write-Host "  Leader: $($result.is_leader)"
        Write-Host "  Latency: $($result.latency_us) µs"
    }
}
```

---

## Monitoring & Troubleshooting

### Key Metrics to Monitor

```
Raft Consensus:
  - current_term (should increase during elections)
  - leader_id (should be stable in healthy cluster)
  - log_count (monotonically increasing)
  - commit_index (trailing log_count by 0-few entries)
  
Gossip/Membership:
  - active_nodes (should equal node_count)
  - node_states (JOINING, ACTIVE, DEGRADED, LEAVING, REMOVED)
  - missed_heartbeats (0 = healthy, 1+ = degraded)
  
Shard Management:
  - total_shards (4096)
  - assigned_shards (should reach 4096 quickly)
  - rebalancing_in_progress (0 or 1)
  - node_load_std_dev (lower = more balanced)
  
Inference Routing:
  - pending_requests (monitor for queue buildup)
  - routed_requests (should increase)
  - completed_requests (should match routed)
  - failed_requests (should stay low)
  - avg_latency_ms (should be consistent)
```

### Common Issues & Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| **Port already in use** | Another process bound to 31337 | `netstat -ano \| findstr 31337` → kill process |
| **Cluster won't form** | Network unreachable | Check firewall rules, ping seed nodes |
| **Leader election loops** | High network latency | Increase ELECTION_TIMEOUT_MAX_MS |
| **Shards not assigned** | Bug in placement logic | Check hash ring construction |
| **Inference requests timeout** | Node overloaded | Monitor queue depths, distribute load |
| **Memory leaks** | Connection or request cleanup | Verify all sockets close on shutdown |

### Debug Logging

Enable verbose logging:

```cpp
// In Week2-3 code:
#define DEBUG_RAFT 1
#define DEBUG_GOSSIP 1
#define DEBUG_SHARDS 1
#define DEBUG_ROUTING 1

// Recompile with debug flags
ml64.exe /c /O2 /Zi /W3 /DDEBUG_RAFT=1 /nologo Week2_3_Master_Complete.asm
```

---

## Performance Tuning

### Throughput Optimization

```powershell
# Increase heartbeat frequency (faster failure detection)
# Edit in ASM: RAFT_HEARTBEAT_INTERVAL_MS = 25

# Increase IOCP threads for higher I/O throughput
# Edit: IOCP_THREADS = 16

# Increase per-node queue depth
# Edit: MAX_PENDING_REQUESTS = 10000

# Recompile and benchmark
Measure-Command {
    .\GenerateLoad.exe -requests 100000 -concurrent 64
}
```

### Latency Optimization

```powershell
# Decrease heartbeat frequency (reduce overhead)
# Edit: RAFT_HEARTBEAT_INTERVAL_MS = 100

# Use locality-aware routing (reduce network hops)
# Edit: strategy = 2  ; LOCALITY

# Batch inference requests (amortize networking)
# In router: Wait for 10 requests before batching

# Recompile and measure
Measure-Command {
    .\LatencyTest.exe -requests 1000 -measure-p99
}
```

### Memory Optimization

```powershell
# Reduce max shards (if using subset)
# Edit: MAX_SHARDS = 1024

# Reduce max nodes (if small cluster)
# Edit: MAX_CLUSTER_NODES = 8

# Use smaller Raft log
# Edit: RAFT_MAX_LOG_ENTRIES = 10000

# Check memory usage
Get-Process YourApp | Select-Object WorkingSet64
# Goal: < 500 MB for typical config
```

---

## What's New in Week 2-3

### vs. Week 1
- **Cluster coordination** via Raft consensus
- **Distributed state** across 16 nodes
- **Automatic failover** on node death
- **Load balancing** for inference requests
- **Shard management** with consistent hashing
- **Gossip protocol** for membership dissemination

### Still Integrated with Week 1
- Uses `SubmitTask` for background work (no separate thread pools)
- Heartbeats use Week 1 monitor (no duplicate infrastructure)
- Work stealing for inference tasks
- Conflict detection on shard transfers

---

## Integration with Later Phases

### Week 4+ Expectations
- Use `SubmitInferenceRequest` API (fully stable)
- Call `JoinCluster` on startup (handles bootstrap)
- Monitor metrics via Week 2-3 API
- Don't call Raft functions directly (consensus is automatic)

### What Week 4 Test Suite Will Verify
1. ✅ Raft consensus (leader election, failover)
2. ✅ Shard placement (consistent hashing)
3. ✅ Inference routing (load balancing)
4. ✅ Cluster membership (join/leave/failure)
5. ✅ Network reliability (packet loss recovery)

---

## Files Included

```
Week2_3_Master_Complete.asm         (3,500+ lines, full implementation)
Week2_3_Build_Deployment_Guide.md   (This file)
Week2_3_Implementation_Report.md    (Status & metrics)
Week2_3_Test_Harness.asm            (600+ lines, 12 tests)
```

---

## Quick Reference Commands

```powershell
# Build
ml64.exe /c /O2 /Zi /W3 /nologo Week2_3_Master_Complete.asm
link /DLL /OUT:Week2_3_Swarm.dll Week2_3_Master_Complete.obj Week1_Deliverable.obj ...

# Deploy single node
$env:WEEK2_3_NODE_ID=0; .\app.exe

# Deploy 3-node cluster
# Run 3 instances with NODE_ID=0,1,2 and SEED_NODES set

# Monitor
Get-Process | Where Name -match app | Select CPU, WorkingSet64

# Cleanup
Remove-Item *.obj *.ilk *.pdb 2>$null
```

---

**Ready for Week 4 Test Suite.** All implementation complete, zero stubs, production quality.
