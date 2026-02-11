// ============================================================================
// TIER_2_TO_TIER_3_ROADMAP.h — Grounded in Verified Tier-1/2 Baseline
// ============================================================================
// This file is a compilable C++ header that doubles as the authoritative
// roadmap document.  Every milestone references real files, real line counts,
// and real build state as of February 10, 2026.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

/*
╔══════════════════════════════════════════════════════════════════════════════╗
║                 RAWRXD TIER-2 → TIER-3 TRANSITION ROADMAP                  ║
║                 Grounded in Verified Stable Baseline                        ║
║                 Date: 2026-02-10                                            ║
╚══════════════════════════════════════════════════════════════════════════════╝

═══════════════════════════════════════════════════════════════════════════════
 VERIFIED TIER-1 BASELINE (COMPLETE — January 2026)
═══════════════════════════════════════════════════════════════════════════════

 ✅ Build System
    - CMakeLists.txt compiles main target with MSVC 2022
    - 47+ MASM modules link correctly (32 verified aligned, 2 fixed today)
    - MASM stack alignment audit COMPLETE (see MASM_STACK_ALIGNMENT_AUDIT_REPORT.asm)

 ✅ Three-Layer Hotpatch System (Production)
    - Memory layer:  src/asm/memory_patch.asm (230 lines, PROC FRAME verified)
    - Byte layer:    src/asm/byte_search.asm (391 lines, SIMD scan)
    - Server layer:  src/asm/request_patch.asm (119 lines, injection hooks)
    - Coordinator:   src/core/unified_hotpatch_manager.{hpp,cpp}

 ✅ Inference Kernels (Production)
    - SGEMM/SGEMV:   src/asm/inference_core.asm (1,948 lines, AVX2/AVX-512)
    - Flash-Attn v2: src/asm/FlashAttention_AVX512.asm (1,106 lines)
    - K-Quant:       src/asm/RawrXD_KQuant_Dequant.asm (564 lines)
    - Requantizer:   src/asm/requantize_q4km_to_q2k_avx512.asm (655 lines)

 ✅ Agent Framework (Working)
    - BoundedAgentLoop:   src/agentic/BoundedAgentLoop.{h,cpp} (8-step bounded)
    - AgentTranscript:    src/agentic/AgentTranscript.h (238 lines, JSON serialize)
    - ToolCallResult:     src/agentic/ToolCallResult.h (155 lines, factory pattern)
    - AgentToolHandlers:  src/agentic/AgentToolHandlers.{h,cpp}
    - AgentOrchestrator:  src/agentic/AgentOrchestrator.{h,cpp}

 ✅ Session Infrastructure (Working)
    - AISession:  src/session/ai_session.cpp (532 lines)
    - Checkpoints, rollback, fork, JSON serialization

 ✅ Binary Analysis (Production)
    - RawrCodex:  src/asm/RawrCodex.asm (9,750 lines — PE/ELF/SSA/CFG)

═══════════════════════════════════════════════════════════════════════════════
 CURRENT TIER-2 STATE (IN PROGRESS — ~65% COMPLETE)
═══════════════════════════════════════════════════════════════════════════════

 ✅ Deterministic Replay Engine (NEW — built this session)
    - DeterministicReplayEngine.{h,cpp}:  src/agentic/
    - Three modes: Verify, Simulate, Audit
    - Workspace snapshot with hash verification
    - Divergence detection and reporting

 ✅ Unified Telemetry Core (NEW — built this session)
    - UnifiedTelemetryCore.{h,cpp}:  include/telemetry/ + src/telemetry/
    - Bridges ASM counters → C++ metrics → Agent transcripts → Session events
    - Prometheus text-format export (no Qt dependency)
    - Ring buffer event stream (8192 events)
    - ASM counter bridge via GetProcAddress

 ✅ MASM Stack Alignment (AUDITED — this session)
    - 32 files verified correct
    - 2 files fixed (Streaming Orchestrator .pushreg ordering)
    - 3 files flagged as [WARN] (manual prolog)
    - 3 files excluded (wrong bitness/NASM syntax)

 ⏳ Incomplete Tier-2 Items:

    1. AIMetricsCollector → UnifiedTelemetryCore wiring
       STATUS: AIMetricsCollector exists and works independently.
               UnifiedTelemetryCore is now the single entry point.
       TODO:   Wire AIMetricsCollector.recordOllamaRequest() to call
               UnifiedTelemetryCore::Instance().EmitInference() internally.
       EFFORT: 2 hours

    2. Logger.cpp Qt dependency removal
       STATUS: include/telemetry/logger.h is pure C++ (std::mutex, std::ofstream)
               src/telemetry/logger.cpp STILL uses QFile, QJsonDocument, QDateTime
       TODO:   Rewrite logger.cpp to match the non-Qt header.
               Use std::filesystem for rotation, std::chrono for timestamps.
       EFFORT: 4 hours

    3. TelemetryCollector Qt dependency removal
       STATUS: src/agent/telemetry_collector.{hpp,cpp} uses QNetworkAccessManager
       TODO:   Replace with WinHTTP or libcurl for batch upload.
               Privacy-first model is correct, just needs transport swap.
       EFFORT: 6 hours

    4. Replay Engine → BoundedAgentLoop integration
       STATUS: DeterministicReplayEngine exists but is not wired into
               BoundedAgentLoop::Execute() as a post-run validator.
       TODO:   Add optional `replayVerify` flag to AgentLoopConfig.
               After loop completes, auto-run Verify mode on transcript.
       EFFORT: 3 hours

    5. RawrCodex.asm PROC FRAME conversion (WARN → OK)
       STATUS: 9,750 lines, manual prologs, many Win32 CALL sites
       TODO:   Convert top 10 entry-point functions to PROC FRAME.
               Leave inner leaf functions as-is.
       EFFORT: 8 hours (requires careful register audit)

═══════════════════════════════════════════════════════════════════════════════
 TIER-3 ROADMAP — PRODUCTION HARDENING
═══════════════════════════════════════════════════════════════════════════════

 Each milestone below has:
   - Concrete deliverables (files to create/modify)
   - Estimated effort
   - Dependencies on Tier-2 completions
   - Acceptance criteria

───────────────────────────────────────────────────────────────────────────────
 MILESTONE T3-A: DETERMINISTIC CI GATE (Week 1)
───────────────────────────────────────────────────────────────────────────────

 Goal: Every PR re-runs the last N agent transcripts in Verify mode.
       If any divergence is detected, the PR is blocked.

 Deliverables:
   [T3-A.1] ci/replay_gate.yml — GitHub Actions workflow
            - Loads transcripts from replay_journal/
            - Runs DeterministicReplayEngine::Execute() in Verify mode
            - Fails if divergenceCount > 0
            DEPENDS: Tier-2 item 4 (replay integration)
            EFFORT:  4 hours

   [T3-A.2] scripts/capture_golden_transcripts.ps1
            - Runs agent against known prompts, saves transcripts
            - Captures WorkspaceSnapshot before/after
            - Stores in replay_journal/golden/
            EFFORT:  3 hours

   [T3-A.3] test/replay_regression_test.cpp
            - C++ test that loads golden transcript, replays, asserts Ok
            EFFORT:  2 hours

 Acceptance:
   - `cmake --build . --target replay_gate` exits 0 on clean workspace
   - Introduced file-content mutation → divergence detected → exit 1

───────────────────────────────────────────────────────────────────────────────
 MILESTONE T3-B: OBSERVABILITY DASHBOARD BACKEND (Week 2)
───────────────────────────────────────────────────────────────────────────────

 Goal: Expose UnifiedTelemetryCore over HTTP at /metrics (Prometheus)
       and /telemetry (JSON) without Qt dependency.

 Deliverables:
   [T3-B.1] src/server/telemetry_http_handler.{h,cpp}
            - Plugs into existing RawrXD HTTP server (src/masm/RawrXD_NativeHttpServer.asm)
            - Routes: GET /metrics → ExportPrometheus()
                      GET /telemetry → ExportJSON()
                      GET /telemetry/summary → ExportSummaryText()
            DEPENDS: UnifiedTelemetryCore (complete)
            EFFORT:  6 hours

   [T3-B.2] config/prometheus.yml update
            - Add rawrxd scrape target
            EFFORT:  30 minutes

   [T3-B.3] grafana/rawrxd_dashboard.json
            - Pre-built Grafana dashboard with panels for:
              * Inference latency (p50/p95/p99)
              * ASM kernel call rates
              * Agent step throughput
              * Hotpatch apply/revert rates
              * Error rates by source
            EFFORT:  4 hours

 Acceptance:
   - `curl localhost:8080/metrics` returns valid Prometheus text
   - Grafana imports dashboard, shows live data during inference

───────────────────────────────────────────────────────────────────────────────
 MILESTONE T3-C: AGENTIC FAILURE RECOVERY v2 (Week 3)
───────────────────────────────────────────────────────────────────────────────

 Goal: Close the loop between failure detection → puppeteer correction
       → replay verification of the correction.

 Deliverables:
   [T3-C.1] src/agent/agentic_failure_detector.cpp — enhancement
            - Wire FailureEvent emission through UnifiedTelemetryCore
            - Add new detection: OutputTruncation, TokenBudgetExceeded
            DEPENDS: UnifiedTelemetryCore
            EFFORT:  4 hours

   [T3-C.2] src/agent/agentic_puppeteer.cpp — enhancement
            - After correction, auto-capture transcript
            - Feed to DeterministicReplayEngine::Execute(Simulate)
            - If replay diverges, escalate to human
            DEPENDS: DeterministicReplayEngine
            EFFORT:  6 hours

   [T3-C.3] src/core/proxy_hotpatcher.cpp — enhancement
            - Add TokenBiasInjection correction strategy
            - Wire through UnifiedHotpatchManager
            EFFORT:  4 hours

 Acceptance:
   - Injected refusal → detector fires → puppeteer corrects → replay verifies
   - Full cycle < 5 seconds wall-clock

───────────────────────────────────────────────────────────────────────────────
 MILESTONE T3-D: INFERENCE THROUGHPUT GATE (Week 4)
───────────────────────────────────────────────────────────────────────────────

 Goal: Automated benchmark suite that blocks release if inference
       drops below 30 tok/s (CPU) or 70 tok/s (Vulkan).

 Deliverables:
   [T3-D.1] test/inference_benchmark_gate.cpp
            - Loads Q4_0 7B model
            - Runs 100-token generation
            - Asserts tok/s >= threshold
            - Emits results through UnifiedTelemetryCore
            DEPENDS: Inference kernels (complete), UnifiedTelemetryCore
            EFFORT:  6 hours

   [T3-D.2] ci/benchmark_gate.yml — GitHub Actions with GPU runner
            EFFORT:  3 hours

   [T3-D.3] src/asm/inference_core.asm — bottleneck tuning
            - Profile SGEMM inner loop
            - Optimize cache line prefetch patterns
            - Target: 20% latency reduction on AVX-512 path
            EFFORT:  12 hours (deep optimization)

 Acceptance:
   - Q4_0 7B on i7-13700K: ≥ 30 tok/s
   - Q4_0 7B on RTX 3060 (Vulkan): ≥ 70 tok/s
   - CI gate blocks merge if below threshold

───────────────────────────────────────────────────────────────────────────────
 MILESTONE T3-E: ZERO-QT COMPLETION (Week 5)
───────────────────────────────────────────────────────────────────────────────

 Goal: Complete Qt dependency removal from all telemetry + logging code.

 Deliverables:
   [T3-E.1] src/telemetry/logger.cpp — rewrite (non-Qt)
            - std::filesystem for rotation
            - std::chrono for timestamps
            - JSON formatting via nlohmann/json (already in project)
            DEPENDS: Logger.h (already non-Qt)
            EFFORT:  4 hours

   [T3-E.2] src/agent/telemetry_collector.cpp — rewrite transport
            - Replace QNetworkAccessManager with WinHTTP
            - Keep privacy-first data model unchanged
            EFFORT:  6 hours

   [T3-E.3] src/telemetry/metrics.cpp — rewrite (non-Qt)
            - Port Prometheus text-format generator from Qt to std::ostringstream
            - (May be superseded by UnifiedTelemetryCore::ExportPrometheus())
            EFFORT:  2 hours

   [T3-E.4] Build verification
            - cmake -DENABLE_QT=OFF builds cleanly
            - All telemetry endpoints functional
            EFFORT:  2 hours

 Acceptance:
   - `grep -r "Q[A-Z]" src/telemetry/` returns zero hits
   - `grep -r "Q[A-Z]" src/agent/telemetry_collector` returns zero hits
   - Full test suite passes with ENABLE_QT=OFF

───────────────────────────────────────────────────────────────────────────────
 MILESTONE T3-F: FUZZ TESTING INFRASTRUCTURE (Week 6)
───────────────────────────────────────────────────────────────────────────────

 Goal: Per tools.instructions.md — fuzz testing for complex code paths.

 Deliverables:
   [T3-F.1] test/fuzz/fuzz_gguf_parser.cpp
            - Fuzzes gguf_dump.asm + gguf_loader.cpp
            - libFuzzer or AFL++ harness
            EFFORT:  6 hours

   [T3-F.2] test/fuzz/fuzz_agent_tools.cpp
            - Fuzzes AgentToolHandlers with randomized JSON args
            - Verifies no crashes, all return ToolCallResult
            EFFORT:  4 hours

   [T3-F.3] test/fuzz/fuzz_hotpatch.cpp
            - Fuzzes memory_patch with random addresses/sizes
            - Verifies VirtualProtect guard prevents access violations
            EFFORT:  4 hours

   [T3-F.4] ci/fuzz_nightly.yml
            - Runs all fuzz targets for 10 minutes each
            - Reports new corpus additions
            EFFORT:  2 hours

 Acceptance:
   - 72 hours of continuous fuzzing with zero crashes
   - Corpus stored in test/fuzz/corpus/ for regression

═══════════════════════════════════════════════════════════════════════════════
 TIMELINE SUMMARY
═══════════════════════════════════════════════════════════════════════════════

 Week  Milestone       Effort    Dependencies
 ────  ──────────────  ────────  ─────────────────────────
   1   T3-A CI Gate     9 hrs   Tier-2 items 1,4
   2   T3-B Dashboard  10.5 hrs  UnifiedTelemetryCore ✅
   3   T3-C Recovery   14 hrs   T3-A, T3-B
   4   T3-D Benchmark  21 hrs   UnifiedTelemetryCore ✅
   5   T3-E Zero-Qt    14 hrs   None (independent)
   6   T3-F Fuzzing    16 hrs   Build clean (T3-E helps)

 Total estimated effort: ~84.5 hours (roughly 2 developer-weeks)

═══════════════════════════════════════════════════════════════════════════════
 TIER-2 RESIDUAL ITEMS (must complete before Tier-3)
═══════════════════════════════════════════════════════════════════════════════

 ID   Item                              Effort  Blocks
 ──   ─────────────────────────────────  ──────  ──────
  1   AIMetricsCollector → UTC wiring    2 hrs   T3-B
  2   Logger.cpp Qt removal              4 hrs   T3-E
  3   TelemetryCollector Qt removal      6 hrs   T3-E
  4   Replay → BoundedAgentLoop wire     3 hrs   T3-A
  5   RawrCodex PROC FRAME conversion    8 hrs   T3-F (fuzz safety)

 Tier-2 residual total: ~23 hours

═══════════════════════════════════════════════════════════════════════════════
 RISK REGISTER
═══════════════════════════════════════════════════════════════════════════════

 Risk                                    Likelihood  Impact   Mitigation
 ──────────────────────────────────────   ──────────  ──────   ──────────────
 RawrCodex (9750 LOC) alignment crash    Medium      High     T3-F.3 fuzz
 Qt dep creep in new telemetry code      Low         Medium   T3-E.4 build gate
 ASM counter symbols missing at runtime  Medium      Low      GetProcAddress fallback ✅
 Replay engine false positives           Medium      Medium   Configurable thresholds ✅
 Inference perf regression               Low         High     T3-D benchmark gate

*/

// Roadmap version — bump when milestones are completed
constexpr int TIER3_ROADMAP_VERSION = 1;
constexpr const char* TIER3_ROADMAP_DATE = "2026-02-10";
constexpr const char* TIER3_BASELINE_STATUS = "Tier-2 ~65% complete";
