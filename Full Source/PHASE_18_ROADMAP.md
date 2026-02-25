# Phase 18 Roadmap — Distributed Inference Scheduler & Swarm Coalescence Protocol

> Version: 18.0.0-draft | Target: RawrXD v15.0.0 | Status: **DESIGN**

---

## 1. Executive Summary

Phase 18 introduces **distributed inference scheduling** across a mesh of RawrXD
nodes, enabling multi-machine model parallelism for models that exceed single-node
VRAM/RAM capacity (e.g., 120B+ parameter models on commodity hardware clusters).

The core innovation is the **Swarm Coalescence Protocol (SCP)** — a peer-to-peer
protocol where independent RawrXD instances discover each other, negotiate shard
assignments, and execute coordinated inference with sub-millisecond synchronization.

---

## 2. Problem Statement

| Constraint | Current Limit | Phase 18 Target |
|-----------|---------------|-----------------|
| Max model size (single node, 64GB RAM) | ~70B Q4_K | 120B+ across 2–8 nodes |
| Decode latency (single node) | ~50ms/token | ~25ms/token (pipeline parallel) |
| Context window (KV cache) | 4K tokens (RAM bound) | 32K+ (distributed KV) |
| Fault tolerance | None (crash = loss) | Auto-recovery, shard migration |

---

## 3. Architecture: Swarm Coalescence Protocol (SCP)

### 3.1 Discovery Layer

```
Node A                    Node B                    Node C
  │                         │                         │
  ├──── UDP multicast ──────┼────────────────────────→│
  │     "SCP_DISCOVER"      │                         │
  │                         ├──── UDP response ──────→│
  │                         │     "SCP_ANNOUNCE"      │
  │←──── TCP handshake ─────┤                         │
  │                         │                         │
  ╰─────────── SCP Mesh Established ──────────────────╯
```

- **Discovery**: UDP multicast on port 18180 (configurable)
- **Handshake**: TCP with HMAC-SHA256 mutual authentication
- **Heartbeat**: 100ms interval, 3 missed = node eviction
- **Transport**: TCP + optional WebRTC DataChannel (Phase 20 bridge)

### 3.2 Shard Assignment

When a model load request arrives, the **Coalescence Coordinator** (elected via
Raft-like leader election among mesh nodes) performs:

1. **Inventory**: Collect available RAM/VRAM from each node
2. **Partition**: Assign transformer layers to nodes using a cost model:
   ```
   cost(node, layer) = layer_bytes / node_bandwidth + layer_flops / node_throughput
   ```
3. **Distribute**: Stream GGUF tensor data to assigned nodes via zero-copy mmap + RDMA
4. **Verify**: Each node computes tensor checksums, reports ready

### 3.3 Pipeline Parallelism

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   Node A     │    │   Node B     │    │   Node C     │
│ Layers 0-13  │───→│ Layers 14-27 │───→│ Layers 28-39 │
│ + Embedding  │    │              │    │ + LM Head    │
│              │←───│              │←───│              │
│  (gradient)  │    │  (gradient)  │    │  (gradient)  │
└──────────────┘    └──────────────┘    └──────────────┘
     ↑                                       │
     └───────── Token output ────────────────┘
```

- **Micro-batching**: Overlap forward pass of batch N+1 on Node A while Node B
  processes batch N, achieving ~90% pipeline utilization with 3+ stages.
- **Activation transfer**: Intermediate activations sent as fp16 tensors via
  shared-memory (same machine) or TCP (cross-machine). ~2MB per layer for 4K context.
- **KV cache distribution**: Each node owns KV cache for its assigned layers.
  Cache coherency maintained via sequence-number tagged invalidation.

### 3.4 Tensor Parallelism (Optional, Phase 18.1)

For extremely large layers (e.g., 120B MoE with 16K hidden dim), split a single
layer's weight matrices across nodes:

```
Node A: W[0:H/2, :]     Node B: W[H/2:H, :]
         ↓                        ↓
    partial_out_A            partial_out_B
         └──────── AllReduce ────────┘
                      ↓
               combined_output
```

AllReduce implementation: Ring-reduce over TCP, or NCCL-like tree-reduce for
3+ nodes. Bandwidth-optimal ring requires 2 × tensor_size / (N-1) bytes total.

---

## 4. Scheduler Design

### 4.1 DAG-Based Task Scheduler

The inference pipeline is modeled as a Directed Acyclic Graph:

```
[Tokenize] → [Embed] → [Layer0] → [Layer1] → ... → [LayerN] → [LMHead] → [Sample]
                                                                    ↓
                                                              [KV Update]
```

Each node executes its assigned subgraph. The scheduler:

1. **Profiles** each layer's latency during warmup (first 10 tokens)
2. **Rebalances** shard assignments if latency skew > 20%
3. **Prefetches** next-layer weights during current-layer compute
4. **Spills** KV cache to NVMe when RAM pressure exceeds 90%

### 4.2 Scheduling Policies

| Policy | Description | Use Case |
|--------|-------------|----------|
| `PIPELINE_GREEDY` | First-fit layer assignment by RAM | Default, fast startup |
| `PIPELINE_BALANCED` | Minimize max-node latency | Production throughput |
| `TENSOR_SPLIT` | Split large layers across nodes | MoE models, >8K hidden |
| `SPECULATIVE_ASYNC` | Draft model on fast node, verify on full | Speed-critical |

### 4.3 Failure Recovery

```
Normal:  A ──→ B ──→ C ──→ output
               ↓ (B fails)
Recovery: A ──→ A' ──→ C ──→ output
          (A absorbs B's layers, slower but alive)
```

1. **Detection**: Missed heartbeats (300ms timeout)
2. **Repartition**: Coordinator redistributes failed node's layers
3. **State recovery**: KV cache rebuilt from last checkpoint (JSONL append log)
4. **Resumption**: Inference continues from last completed token

---

## 5. Wire Protocol (SCP v1)

### 5.1 Message Format

```
┌────────────┬────────────┬────────────┬────────────────────┐
│ Magic (4B) │ Type (2B)  │ Len (4B)   │ Payload (variable) │
│ "SCPX"     │ enum       │ uint32 LE  │ msgpack or raw      │
└────────────┴────────────┴────────────┴────────────────────┘
```

### 5.2 Message Types

| Code | Name | Direction | Payload |
|------|------|-----------|---------|
| 0x01 | `DISCOVER` | Broadcast | Node capabilities JSON |
| 0x02 | `ANNOUNCE` | Unicast | Node ID + capacity |
| 0x03 | `SHARD_ASSIGN` | Coordinator → Node | Layer range + tensor offsets |
| 0x04 | `SHARD_ACK` | Node → Coordinator | Checksum + ready flag |
| 0x05 | `ACTIVATION_PUSH` | Node → Next | fp16 tensor (raw bytes) |
| 0x06 | `ACTIVATION_ACK` | Next → Node | Sequence number |
| 0x07 | `KV_INVALIDATE` | Any → All | Sequence + layer range |
| 0x08 | `HEARTBEAT` | Bidirectional | Timestamp + load metrics |
| 0x09 | `NODE_EVICT` | Coordinator → All | Failed node ID |
| 0x0A | `REBALANCE` | Coordinator → All | New shard map |
| 0x0B | `TOKEN_RESULT` | Last node → First | Token ID + logits |
| 0x0C | `SHUTDOWN` | Any | Graceful teardown |

### 5.3 Security

- **Authentication**: HMAC-SHA256 on every message (shared cluster secret)
- **Encryption**: Optional AES-256-GCM (flag in DISCOVER message)
- **Node identity**: Ed25519 keypair generated at first launch
- **Rate limiting**: 10,000 messages/sec per node (prevent amplification)

---

## 6. Implementation Plan

### Phase 18.0 — Core Protocol (4 weeks)

| Week | Deliverable | Files |
|------|-------------|-------|
| 1 | SCP discovery + handshake | `src/core/scp_discovery.{hpp,cpp}` |
| 1 | Heartbeat manager | `src/core/scp_heartbeat.{hpp,cpp}` |
| 2 | Shard assignment coordinator | `src/core/scp_coordinator.{hpp,cpp}` |
| 2 | Layer partitioner (cost model) | `src/core/scp_partitioner.{hpp,cpp}` |
| 3 | Activation transfer (TCP) | `src/core/scp_activation_transport.{hpp,cpp}` |
| 3 | Pipeline scheduler | `src/core/scp_pipeline_scheduler.{hpp,cpp}` |
| 4 | Failure recovery + rebalancing | `src/core/scp_failure_recovery.{hpp,cpp}` |
| 4 | Win32IDE Swarm panel update | `src/win32app/Win32IDE_SwarmPanel.cpp` (extend) |

### Phase 18.1 — Tensor Parallelism (2 weeks)

| Deliverable | Files |
|-------------|-------|
| AllReduce ring implementation | `src/core/scp_allreduce.{hpp,cpp}` |
| Tensor split partitioner | `src/core/scp_tensor_split.{hpp,cpp}` |
| MASM kernel for fp16 reduce | `src/asm/scp_reduce.asm` |

### Phase 18.2 — Production Hardening (2 weeks)

| Deliverable | Files |
|-------------|-------|
| Deterministic replay over mesh | `src/core/scp_replay.{hpp,cpp}` |
| Performance dashboard | `src/win32app/Win32IDE_SCPDashboard.cpp` |
| Chaos testing framework | `tests/scp_chaos_test.cpp` |
| Cluster benchmark tool | `tools/scp_benchmark.cpp` |

---

## 7. ASM Kernel Requirements

### 7.1 New Kernels for Phase 18

| Kernel | File | Purpose |
|--------|------|---------|
| `scp_fp16_pack` | `scp_reduce.asm` | fp32→fp16 conversion for activation transfer |
| `scp_fp16_unpack` | `scp_reduce.asm` | fp16→fp32 conversion on receiver |
| `scp_ring_reduce` | `scp_reduce.asm` | In-place fp32 addition for AllReduce |
| `scp_checksum_avx2` | `scp_reduce.asm` | CRC32C of tensor data for verification |

### 7.2 Register Constraints (Windows x64 ABI)

All Phase 18 ASM kernels MUST follow:

```
╔════════════════════════════════════════════════════════════╗
║  VOLATILE (free to clobber):                              ║
║    RAX, RCX, RDX, R8, R9, R10, R11                      ║
║    XMM0-XMM5  (+ ZMM16-ZMM31 for AVX-512)               ║
║                                                           ║
║  NON-VOLATILE (MUST save/restore via push or XSAVE):     ║
║    RBX, RBP, RSI, RDI, R12, R13, R14, R15               ║
║    XMM6-XMM15 (128-bit only; upper YMM/ZMM auto-zeroed) ║
║                                                           ║
║  STACK: RSP 16-byte aligned before CALL                   ║
║  SHADOW: 32 bytes reserved above return address           ║
║  VZEROUPPER: Required before returning to C++ code        ║
╚════════════════════════════════════════════════════════════╝
```

---

## 8. Configuration (config/scp.json)

```json
{
  "scp_enabled": false,
  "scp_port": 18180,
  "scp_multicast_group": "239.18.18.0",
  "scp_cluster_secret": "",
  "scp_encryption": false,
  "scp_heartbeat_ms": 100,
  "scp_eviction_timeout_ms": 300,
  "scp_max_nodes": 8,
  "scp_scheduling_policy": "PIPELINE_BALANCED",
  "scp_activation_format": "fp16",
  "scp_kv_spill_threshold": 0.9,
  "scp_rebalance_skew_threshold": 0.2
}
```

---

## 9. CLI Commands

| Command | Description |
|---------|-------------|
| `/swarm status` | Show mesh topology and node health |
| `/swarm join <host:port>` | Manually join a specific node |
| `/swarm leave` | Gracefully leave the mesh |
| `/swarm rebalance` | Force shard rebalancing |
| `/swarm benchmark` | Run cross-node latency/bandwidth test |
| `/swarm assign <model>` | Load model across mesh |

---

## 10. Win32IDE Integration

New command IDs (10000–10099 range):

| ID | Command | Description |
|----|---------|-------------|
| 10001 | Swarm Status Dashboard | Real-time mesh topology view |
| 10002 | Node Health Monitor | Per-node CPU/RAM/bandwidth graphs |
| 10003 | Shard Visualizer | Layer assignment heatmap |
| 10004 | Pipeline Profiler | Micro-batch timing waterfall |
| 10005 | Force Rebalance | Manual rebalance trigger |
| 10006 | Chaos Inject | Simulate node failure for testing |

---

## 11. Success Criteria

- [ ] 2-node pipeline parallel achieves ≥1.7× single-node throughput
- [ ] 4-node pipeline parallel achieves ≥3.0× single-node throughput
- [ ] Node failure recovery completes in <1 second
- [ ] 120B Q4_K model loads across 4 × 32GB RAM nodes
- [ ] KV cache supports 32K context across distributed nodes
- [ ] Latency overhead per hop <2ms (same-LAN)
- [ ] Zero data loss on graceful shutdown
- [ ] All ASM kernels pass Windows x64 ABI compliance audit

---

## 12. Dependencies

| Dependency | Phase | Status |
|-----------|-------|--------|
| Swarm coordinator (Phase 11) | `src/core/swarm_coordinator.cpp` | ✅ Complete |
| WebRTC signaling (Phase 20) | `src/core/webrtc_signaling.cpp` | ✅ Complete |
| Flash Attention AVX-512 | `src/asm/FlashAttention_AVX512.asm` | ✅ Hardened |
| Inference core GEMM/GEMV | `src/asm/inference_core.asm` | ✅ New (Phase 18-prep) |
| Quantization kernels | `src/asm/quant_avx2.asm` | ✅ New (Phase 18-prep) |
| CMake ASM_MASM wiring | `CMakeLists.txt` | ✅ Complete |

---

*This document is the canonical Phase 18 design reference.*
*Last updated: 2026-02-07*
