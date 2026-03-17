# Phase 7 Batches 3-10 — Implementation Specifications (MASM)
**Date:** December 29, 2025  
**Scope:** Pure MASM (x64, Windows API only)  
**Goal:** Lossless C++ parity; no functionality dropped; production-ready with observability hooks (logging/metrics/tracing) and registry-configurable feature toggles.

---
## Batch 3 – Multi-Session Agent Orchestration
**Objective:** Pool, route, and lifecycle-manage concurrent agent sessions for inference and tools.
- **Key Components:**
  - `agent_session_pool.asm`: session structs, free/active lists, pool init/destroy.
  - `agent_session_router.asm`: round-robin / least-loaded dispatch; priority-aware queue.
  - `agent_state_sync.asm`: shared state snapshot/export/import; minimal locking via slim SRW.
- **Public APIs (suggested):**
  - `AgentPool_Create(max_sessions)` → handle
  - `AgentPool_AcquireSession(timeout_ms)` → session_id/NULL
  - `AgentPool_ReleaseSession(session_id)`
  - `AgentRouter_Dispatch(request_ptr, len, session_id|AUTO)`
  - `AgentState_Snapshot(session_id, buffer, size)` / `AgentState_Restore(...)`
- **Data Structures:**
  - `AGENT_SESSION` { id, state_ptr, last_used_qpc, busy_flag, error_code }
  - Lock-free queues using interlocked SList for dispatch; SRW for state writes.
- **Dependencies:** Process-spawning wrapper (Phase 4), threading/system timers, registry_persistence for pool sizing.
- **Observability:**
  - Structured logs: session acquire/release, dispatch latency.
  - Metrics: `agent_pool_in_use`, `agent_dispatch_duration_ms` histogram.
  - Tracing: span per dispatch (start/end, session_id, request_id).
- **Tests (Phase 5 hooks):**
  - Pool exhaustion, timeout, fairness (round-robin), state snapshot/restore, concurrent acquire/release.

## Batch 4 – Enhanced Security Policies
**Objective:** Enforce RBAC, audit, encryption toggles, and safe tool execution.
- **Key Components:**
  - `security_policy_engine.asm`: policy evaluation, role→capability mapping.
  - `security_audit_log.asm`: append-only audit trail (file + in-memory ring).
  - `security_token_manager.asm`: session tokens, expiry, HMAC validation (WinCrypt).
- **Public APIs:**
  - `Security_LoadPolicies(registry_path)` / `Security_SavePolicies`
  - `Security_CheckCapability(role, capability_id)` → TRUE/FALSE
  - `Security_Audit(event_code, user_id, detail_ptr, len)`
  - `Security_IssueToken(user_id, role, ttl_ms, out_token)`
  - `Security_ValidateToken(token_ptr, len)`
- **Data Structures:**
  - `POLICY_ENTRY` { role_id, cap_mask_low/high }
  - `AUDIT_EVENT` { qpc, user_id, event_code, detail_offset }
  - Token format: version | user_id | role | expiry_qpc | HMAC-SHA256.
- **Dependencies:** advapi32 crypto APIs, registry_persistence, logging.
- **Observability:**
  - Logs at INFO for allow/deny; WARN for audit drops; ERROR for HMAC failures.
  - Metrics: `security_denies_total`, `security_audit_drops_total`, `token_validation_failures`.
  - Tracing: span around Security_CheckCapability and token validation.
- **Tests:**
  - Policy load/save roundtrip; deny/allow matrix; token expiry; tamper-detection.

## Batch 5 – Custom Tool Pipeline Builder
**Objective:** Compose and execute tool chains (pre/post transforms, branching, retries).
- **Key Components:**
  - `tool_pipeline_builder.asm`: defines pipeline graph (nodes, edges).
  - `tool_pipeline_executor.asm`: executes DAG with worker threads; retry/backoff.
  - `tool_context_store.asm`: per-request KV scratchpad; size-limited.
- **Public APIs:**
  - `ToolPipeline_Create(def_ptr, len)` → pipeline_id
  - `ToolPipeline_Execute(pipeline_id, request_ptr, len)` → result code
  - `ToolPipeline_Cancel(pipeline_id, request_id)`
  - `ToolContext_Get/Set/Delete(key, val)` (bounded sizes)
- **Data Structures:**
  - `PIPELINE_NODE` { id, type, fn_ptr, next_ids[4], retry_cfg, timeout_ms }
  - `PIPELINE_EXEC_STATE` { request_id, node_cursor, attempts, last_error }
- **Dependencies:** agentic tool APIs, threading, timers, registry for defaults (timeouts, max_fanout).
- **Observability:**
  - Logs per node start/finish/error with request_id.
  - Metrics: `pipeline_exec_duration_ms` histogram; `pipeline_failures_total` by node type.
  - Tracing: span per node execution with attributes (node_id, attempts).
- **Tests:**
  - Linear pipeline success; branching; retry with backoff; cancellation; context KV limits.

## Batch 6 – Advanced Hot-Patching System
**Objective:** MASM parity with C++ hotpatch layers (memory, byte-level, server).
- **Key Components:**
  - `hotpatch_memory_layer.asm`: VirtualProtect/mprotect analog; PatchResult struct.
  - `hotpatch_byte_layer.asm`: Boyer-Moore search, directWrite/read, atomic ops (swap/XOR/rotate).
  - `hotpatch_server_layer.asm`: pre/post request/response transforms; caching.
  - `unified_hotpatch_manager.asm` (MASM): single entry for applyMemory/Byte/AddServer.
- **Public APIs:**
  - `Hotpatch_ApplyMemory(base_ptr, pattern_ptr, pat_len, repl_ptr, repl_len)` → PatchResult
  - `Hotpatch_ApplyByte(file_handle, offset, buffer, len)` → PatchResult
  - `Hotpatch_AddServerHotpatch(hp_ptr)` → PatchResult
  - `Hotpatch_GetStats(out_stats)`
- **Data Structures:**
  - `PATCH_RESULT` { success, detail_ptr, error_code }
  - `SERVER_HOTPATCH` { type (PreRequest/PostResponse/StreamChunk), fn_ptr, cache_key }
- **Dependencies:** file I/O, memory protection APIs, existing gguf/byte-level helpers.
- **Observability:**
  - Logs around protect/restore, pattern matches, server inject points.
  - Metrics: `hotpatch_apply_duration_ms`, `hotpatch_failures_total`, `hotpatch_bytes_written`.
  - Tracing: span per hotpatch with attributes (layer, bytes, error_code).
- **Tests:**
  - Memory patch apply/revert; byte-level replace; server pre/post hooks; error propagation.

## Batch 7 – Agentic Failure Recovery
**Objective:** Detect/refine/refuse/retry flows for agent responses and model outputs.
- **Key Components:**
  - `failure_detector.asm`: pattern-based detection (refusal, hallucination, timeout, resource exhaustion).
  - `failure_corrector.asm`: applies Plan/Agent/Ask correction modes (mirrors C++ AgenticPuppeteer).
  - `retry_scheduler.asm`: bounded retries with jitter; escalation rules.
- **Public APIs:**
  - `FailureDetector_Analyze(response_ptr, len)` → { type, confidence, detail }
  - `FailureCorrector_Apply(mode, response_ptr, len, out_buf, out_len)`
  - `RetryScheduler_Schedule(request_id, policy_ptr)`
- **Data Structures:**
  - `FAILURE_RESULT` { type, confidence_fp32, detail_ptr }
  - `RETRY_POLICY` { max_attempts, backoff_ms, jitter_ms, fatal_mask }
- **Dependencies:** percentile_calculations (for latency/timeout), logging, metrics, agent pipeline.
- **Observability:**
  - Logs at WARN for detected failures; INFO for corrections; ERROR for unrecoverable.
  - Metrics: `failure_detected_total{type}`, `failure_corrected_total`, `retry_attempts_total`.
  - Tracing: span per detect/correct with response_id.
- **Tests:**
  - Detection accuracy per pattern; correction output non-empty; retries respect backoff/jitter; fatal vs retryable paths.

## Batch 8 – Web UI Integration Layer
**Objective:** Bridge MASM core to WebSocket/HTTP for remote control and visualization.
- **Key Components:**
  - `websocket_bridge.asm`: RFC6455 handshake, send/recv frames, ping/pong, fragmentation handling.
  - `http_control_plane.asm`: minimal HTTP parser/emitter for GET/POST control endpoints.
  - `json_serializer.asm`: compact JSON writer for stats/patch results.
- **Public APIs:**
  - `WebUI_Start(server_port)` / `WebUI_Stop`
  - `WebUI_SendEvent(event_type, payload_ptr, len)`
  - `WebUI_BroadcastMetrics(metrics_ptr)`
- **Data Structures:**
  - `WS_CONN` { socket, state, recv_buf, send_buf, last_ping_qpc }
  - `WEB_EVENT` { type, payload_ptr, payload_len }
- **Dependencies:** winsock2, json_parser/serializer, metrics exporters.
- **Observability:**
  - Logs: connection open/close, errors, malformed frames.
  - Metrics: `webui_active_connections`, `webui_bytes_sent/recv_total`, `webui_errors_total`.
  - Tracing: span per inbound HTTP/WS request.
- **Tests:**
  - Handshake success, ping/pong, fragmented frame reassembly, broadcast fan-out.

## Batch 9 – Model Inference Optimization
**Objective:** Optimize token generation throughput/latency via caching and scheduling.
- **Key Components:**
  - `attention_cache.asm`: KV cache reuse across requests; LRU eviction.
  - `token_scheduler.asm`: batch multiple prompts; micro-batching with max token/window.
  - `memory_pool.asm`: slab allocator for activation buffers to reduce HeapAlloc churn.
- **Public APIs:**
  - `AttentionCache_Get(model_id, key_ptr, len)` → cache_entry/NULL
  - `AttentionCache_Put(model_id, key_ptr, len, value_ptr, vlen)`
  - `TokenScheduler_Submit(req_ptr)` → ticket
  - `TokenScheduler_RunOnce()` (drives scheduling loop)
- **Data Structures:**
  - `CACHE_ENTRY` { key_hash, key_len, value_ptr, value_len, last_used_qpc }
  - `TOKEN_BATCH` { requests[MaxN], total_tokens, deadline_qpc }
- **Dependencies:** model loader, tokenizer, hardware_acceleration, registry for batch sizes.
- **Observability:**
  - Metrics: `tokens_per_second`, `scheduler_batch_size`, `cache_hit_ratio`, `alloc_failures`.
  - Logs: cache evict, batch compose, allocator fallback.
  - Tracing: span per batch run with token count.
- **Tests:**
  - Cache hit/miss; eviction correctness; scheduler batching under load; allocator fallback to HeapAlloc.

## Batch 10 – Knowledge Base Integration
**Objective:** RAG-style document retrieval with vector search and chunking.
- **Key Components:**
  - `kb_index.asm`: inverted + vector index (cosine/inner product); offline build hooks.
  - `kb_retriever.asm`: top-K retrieval; hybrid rank (sparse + dense); re-rank pass.
  - `kb_ingest.asm`: chunking, embedding calls (external), deduplication.
- **Public APIs:**
  - `KB_LoadIndex(path)` / `KB_SaveIndex(path)`
  - `KB_Query(query_ptr, len, top_k, out_results_ptr)`
  - `KB_Ingest(doc_ptr, len, meta_ptr)`
- **Data Structures:**
  - `KB_VECTOR` { dim, data_ptr }
  - `KB_ENTRY` { doc_id, chunk_id, score_fp32, meta_offset }
- **Dependencies:** tokenizer, embedding provider (abstracted function pointer), file I/O.
- **Observability:**
  - Metrics: `kb_query_duration_ms`, `kb_ingest_duration_ms`, `kb_index_size_bytes`, `kb_results_topk`.
  - Logs: ingest errors, index load/save, re-rank outcomes.
  - Tracing: span per query with top_k, hit_count.
- **Tests:**
  - Index load/save; query accuracy (known corpus); ingest dedup; re-rank ordering.

---
## Cross-Cutting Requirements (All Batches)
- **Thread Safety:** Use SRW locks or QMutex equivalent; prefer RAII-like macros; avoid explicit unlock.
- **Error Handling:** Return structs/codes (no exceptions). Provide detail strings where size permits.
- **Observability:** Instrument with structured logs (INFO/WARN/ERROR), metrics (counters + duration histograms), and tracing spans around critical ops.
- **Configuration:** All tunables via registry paths (HKCU\Software\RawrXD\<Module>); support feature flags for risky paths.
- **Testing Hooks:** Each batch must expose at least one test entry point callable by Phase 5 harness.
- **Resource Hygiene:** HeapAlloc/HeapFree pairs; CloseHandle/RegCloseKey; no leaks. Guard external resource lifetimes.

---
## Delivery Plan
- **Order:** Specs (done here) → Phase 6 Qt bridge (prerequisite) → Implement Batch 3 → 6 → 7 → 8 → 9 → 10 → Phase 5 tests.
- **Effort Estimate:** ~500+ engineering hours across Batches 3-10; Qt bridge 50-80 hours; specs completed.
- **Build:** Continue MASM x64, Windows API only; keep external deps minimal (winsock2/advapi32 as needed).
