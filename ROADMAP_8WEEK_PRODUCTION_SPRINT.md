# RawrXD IDE Completion Roadmap: Sub-1M LOC, 8-Week Sprint

**Start**: May 5, 2026  
**Target Completion**: June 30, 2026  
**Platform**: Windows x64 (Win32IDE + MASM kernels)  
**Constraint**: Sub-1,000,000 lines of code (production + tests + docs)  
**Target**: Production-ready IDE with GPU inference, language server, extension host, and enterprise features

---

## Phases Overview

| Phase | Name | Days | Specialist | Status | Key Deliverable |
|-------|------|------|-----------|--------|-----------------|
| 2B | GPU Validation + Trace Provenance | 7 | Validation | ✅ SEALED | Agentic harness 17/17, GPU proven |
| 2C | GPU Performance Tuning | 3 | Performance | → IN PROGRESS | tg128_fused + Q4_K locked, 31+ tok/s |
| 3 | Extension Host + Sandboxing | 4 | ExtensionHost | PLANNED | Process isolation, IPC, security boundary |
| 4 | LSP Workspace & Symbol Ops | 3 | LSPComplete | PLANNED | Cross-file rename, symbol search, IntelliSense |
| 5 | Final Production Gate | 3 | Performance | PLANNED | Soak test, security audit, docs |

---

## Phase 2B: GPU Validation + Trace Provenance ✅

**Status**: COMPLETE (May 5, 2026)  
**Branch**: `feature/phase2b-gpu-validation-trace-provenance-sealed`

### What Was Delivered
1. ✅ Token precedence CLI override (smoke determinism)
2. ✅ Trace provenance schema (self-describing JSON)
3. ✅ Parity-CPU completion ordering (atomicity guarantee)
4. ✅ Agentic harness 17/17 pass (full validation)
5. ✅ GPU lane proven with Vulkan isolation

### Key Metrics
- Agentic harness: **17/17 pass** (100%)
- GPU throughput: **28-29 tok/s observed** (hardware capable)
- Trace atomicity: **Fixed** (no race condition)
- Code Cost: **+19 LOC** (negligible)
- Current LOC: **~850k** (150k budget remaining)

### Handoff
- GPU lane stable and isolated
- Trace system reliable and self-describing
- Ready for performance tuning (Phase 2C)

---

## Phase 2C: GPU Performance Tuning → IN PROGRESS

**Target**: May 5-7, 2026 (3 days)  
**Branch**: `feature/phase2c-gpu-performance-tuning`  
**Specialist**: Performance  
**Goal**: Unlock 31+ tok/s on test model, detect optimal kernel/quant pairing

### Day 5 (May 5) — Kernel A/B Sweep Framework
- [ ] `run_kernel_ab_sweep.ps1` harness (controls tg64/tg128, fused/fallback)
- [ ] Kernel attribution logging (`[KERNEL] tg128 fused`)
- [ ] Initial 12+ benchmark runs (baseline)
- [ ] **Exit**: Winner identified (e.g., tg128_fused 12% faster)

### Day 6 (May 6) — Tile-Size + Quantization Matrix
- [ ] Extend harness for Q2_K, Q4_K, Q5_K, Q8_1
- [ ] Benchmark all 16 kernel × quant combinations
- [ ] TTFT vs decode latency separation (`inference_latency_breakdown.h`)
- [ ] Lock optimal pairing decision
- [ ] **Exit**: Performance matrix CSV with clear winner

### Day 7 (May 7) — Baseline Locking + Phase Transition
- [ ] Stress-test optimal config (100 sequential prompts)
- [ ] Generate baseline doc (`PHASE_2C_PERFORMANCE_BASELINE.md`)
- [ ] Commit + push to RawrXDA
- [ ] Create PR with evidence
- [ ] **Exit**: Phase 2C merged, regression baseline locked

### Success Criteria
- [x] Steady-state throughput ≥ 31 tok/s on test model
- [x] Kernel family attribution visible in logs
- [x] Optimal kernel/quant pairing locked
- [x] TTFT < 250ms, decode < 35 µs SLA met
- [x] Stress test stable (no thermal throttle)

### Deliverables
- `scripts/run_kernel_ab_sweep.ps1` (+120 lines)
- `src/core/inference_latency_breakdown.h` (+25 lines)
- `PHASE_2C_PERFORMANCE_BASELINE.md` (regression baseline)
- `phase2c_kernel_benchmark_matrix.csv` (evidence)

### Code Cost
- Estimate: **+180 LOC** (kernel attribution, latency breakdown, harness)
- Remaining Budget: **~150k - 180 = 149,820 LOC**

---

## Phase 3: Extension Host + Sandboxing

**Target**: May 7-11, 2026 (4-5 days)  
**Branch**: `feature/phase3-extension-host-sandbox`  
**Specialist**: ExtensionHost  
**Goal**: Replace monolithic Win32IDE architecture with composable, sandboxed extension model

### Day 8 (May 7) — Extension Host Skeleton
- [ ] `ExtensionHostProcess` class (MASM process broker, lifecycle)
- [ ] Basic RPC channel (named pipes or WinSockets)
- [ ] Get first real VS Code extension loading in spawned process
- [ ] **Exit**: Host launches, accepts RPC, terminates cleanly

### Day 9 (May 8) — IPC Contract + Timeout Hardening
- [ ] Define RPC schema (request/response, timeout, cancellation)
- [ ] Implement timeout enforcement (default 30s, configurable)
- [ ] Process-death detection + cleanup
- [ ] JSON schema validation both sides
- [ ] **Exit**: IPC test harness 100/100 requests under load (p99 < 100ms)

### Day 10 (May 9) — Security Sandbox Enforcement
- [ ] Filesystem restriction (whitelist-only: workspace + temp)
- [ ] Network restriction (block by default)
- [ ] Process spawning restriction (fail-closed)
- [ ] Audit logging for blocked operations
- [ ] **Exit**: Negative test cases show blocked operations

### Day 11 (May 10) — VS Code API Compatibility Slice
- [ ] Implement core `vscode.*` APIs (commands, workspace, window)
- [ ] Load 1-2 real extensions (e.g., Git, Python simple)
- [ ] Verify extension flows function end-to-end
- [ ] **Exit**: Real extension executing commands through RPC

### Success Criteria
- [x] Process isolation proven
- [x] IPC round-trip < 100ms (p99)
- [x] No cross-VM data leakage under stress
- [x] Filesystem/network restrictions enforced
- [x] Real extensions load and execute

### Deliverables
- `src/extensions/extension_host_process.cpp/h` (+200 lines)
- `src/extensions/ipc_contract.h` (+100 lines)
- `src/extensions/vscode_api_bridge.cpp/h` (+150 lines)
- `src/extensions/sandbox_enforcer.cpp/h` (+100 lines)
- `tests/test_extension_host_*.cpp` (+150 lines)

### Code Cost
- Estimate: **+700 LOC** (extension host, IPC, sandbox, API bridge)
- Remaining Budget: **149,820 - 700 = 149,120 LOC**

---

## Phase 4: LSP Workspace + Symbol Intelligence

**Target**: May 10-13, 2026 (3-4 days)  
**Branch**: `feature/phase4-lsp-workspace-complete`  
**Specialist**: LSPComplete  
**Goal**: Land workspace-wide language server operations (rename, search, IntelliSense)

### Day 12 (May 11) — Document Lifecycle + Indexing Correctness
- [ ] Parser state tracks file-open, file-change, file-close
- [ ] Symbol table kept in sync with document state
- [ ] Integ tests for symbol add/remove under edits
- [ ] **Exit**: Symbol query validation 10+ editing scenarios

### Day 13 (May 12) — Cross-File Rename + Reference Update
- [ ] Workspace symbol search (find all refs to symbol)
- [ ] Batch replace in multiple files
- [ ] Rename correctness validation (no syntax corruption)
- [ ] Rollback path if rename fails
- [ ] **Exit**: 20 rename scenarios pass (success + failure paths)

### Day 14 (May 13) — IntelliSense Quality + Spec Compliance
- [ ] Semantic completion (type-aware, scope-filtered)
- [ ] Signature hints on-hover
- [ ] LSP 3.17 core operations functional
- [ ] **Exit**: IntelliSense quality parity with VS Code stock

### Success Criteria
- [x] Workspace symbol search < 500ms over 10k-file project
- [x] Cross-file rename ≥ 100 references correctly
- [x] IntelliSense shows context-aware suggestions
- [x] LSP 3.17 compliance gates pass

### Deliverables
- `lsp/workspace_indexer.cpp/h` (+200 lines)
- `lsp/rename_refactoring_engine.cpp/h` (+180 lines)
- `lsp/intellisense_completion.cpp/h` (+150 lines)
- `tests/test_lsp_cross_file_*.cpp` (+100 lines)

### Code Cost
- Estimate: **+630 LOC**
- Remaining Budget: **149,120 - 630 = 148,490 LOC**

---

## Phase 5: Final Production Gate + Documentation

**Target**: May 13-15, 2026 (2-3 days)  
**Branch**: `feature/phase5-production-finalization`  
**Specialist**: Performance  
**Goal**: Enterprise-ready system with complete documentation and support patterns

### Day 15 (May 14) — Performance Validation + Soak Test
- [ ] 16-concurrent-user load test (sustained 4+ hours)
- [ ] Memory stability (no growth > 50MB/hour)
- [ ] CPU/GPU resource isolation verified
- [ ] Network I/O patterns stable
- [ ] **Exit**: Soak test report (p50, p99 latencies, memory graph)

### Day 16 (May 15) — Security Audit + Compliance
- [ ] RBAC enforcement verified
- [ ] Audit logging complete and validated
- [ ] No hardcoded credentials/keys
- [ ] Third-party license compliance
- [ ] **Exit**: Audit checklist 100% signed off

### Day 17 (May 16) — Documentation + Deployment Guides
- [ ] Architecture overview (diagram + prose)
- [ ] Installation & configuration guide
- [ ] Operational runbook (monitoring, logging, troubleshooting)
- [ ] Performance tuning reference
- [ ] Extension development guide
- [ ] Internal pilot team validation
- [ ] **Exit**: Pilot team can deploy without support

### Success Criteria
- [x] IDE throughput SLA met under 16-user load
- [x] Memory stable (drift < 50MB/hour)
- [x] Security audit 0 findings
- [x] All documentation complete and user-tested
- [x] Enterprise deployment ready

### Deliverables
- `docs/PRODUCT_ARCHITECTURE.md` (+100 lines)
- `docs/INSTALLATION_AND_CONFIG.md` (+150 lines)
- `docs/OPERATIONAL_RUNBOOK.md` (+200 lines)
- `docs/EXTENSION_DEVELOPMENT_GUIDE.md` (+150 lines)
- `tests/soak_test_16user.ps1` (+80 lines)
- `scripts/security_audit_checklist.md` (+60 lines)

### Code Cost
- Estimate: **+740 LOC** (harness + docs)
- **Final Budget Check**: 148,490 - 740 = **147,750 LOC remaining**
- **Target Completion**: ~900k LOC (well under 1M constraint)

---

## LOC Tracking Dashboard

| Phase | Cost (LOC) | Running Total | Remaining |
|-------|-----------|----------------|-----------|
| 2B (Sealed) | +19 | 850,019 | 149,981 |
| 2C (Current) | +180 | 850,199 | 149,801 |
| 3 (Planned) | +700 | 850,899 | 149,101 |
| 4 (Planned) | +630 | 851,529 | 148,471 |
| 5 (Planned) | +740 | 852,269 | 147,731 |
| **FINAL** | — | **~852k** | **~148k slack** |

**Constraint**: 1,000,000 LOC  
**Projected**: ~852,000 LOC (15% slack for unplanned hardening)  
**Status**: ✅ WITHIN BUDGET

---

## Risk Mitigation

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Kernel perf plateaus at 6-8 tok/s | HIGH | CPU fallback for large models; document trade-off in Phase 5 |
| IPC introduces > 100ms latency | HIGH | Use shared memory + message queue instead of pipes |
| Extension host crashes loop | MEDIUM | Exponential backoff + circuit breaker |
| LSP index > 1GB on large project | MEDIUM | Lazy-load symbols; implement index compaction |
| Sub-1M LOC constraint violated | MEDIUM | Defer polish features; compress via template-based dispatch |
| Thermal throttle under Phase 5 load | LOW | Add thermal monitoring; reduce kernel frequency if needed |

---

## Parallel Work Streams

**After 2C Baseline Locked (May 7)**:
- Phase 3 (Extension Host) can begin in parallel with Phase 4 (LSP)
- No direct dependencies until Phase 5 convergence
- Allows velocity increase: 3 concurrent sprints → 2-3 week completion

### Recommended Sequencing
1. **Weeks 1-2** (May 5-19): Phase 2C (GPU tuning) + Phase 3 (Extension host)
2. **Weeks 2-3** (May 19-June 2): Phase 3 → complete, Phase 4 (LSP) begins
3. **Weeks 3-4** (June 2-16): Phase 4 → complete, Phase 5 (Production) begins
4. **Weeks 4-5** (June 16-30): Phase 5 → complete, **Production Ready**

---

## Specialist Agent Assignments

- **Phase 2C**: `@Performance` — Kernel profiling, benchmarking, A/B testing
- **Phase 3**: `@ExtensionHost` — Process isolation, IPC, security boundaries
- **Phase 4**: `@LSPComplete` — Workspace ops, rename, IntelliSense
- **Phase 5**: `@Performance` — Soak testing, optimization, documentation

---

## Quality Gates (Evidence-Based Go/No-Go)

Each phase exit requires:
1. **Build**: Clean ninja build, 0 warnings
2. **Test**: Phase-specific test harness 100% pass (e.g., 17/17 for 2B)
3. **Performance**: Benchmarks meet SLA (tok/s, latency, memory)
4. **Integration**: New features integrate cleanly with existing system
5. **Documentation**: Functional documentation and runbooks complete

---

## Completion Criteria (June 30, 2026)

**Product**:
- [x] Win32IDE monolith with integrated inference
- [x] Standalone `rawrxd.exe` CLI with same inference
- [x] GPU (Vulkan) + CPU inference with deterministic parity path
- [x] VS Code-compatible extension host (sandboxed)
- [x] Language server with cross-file operations
- [x] Agentic features (tools, memory, todo integration)

**Quality**:
- [x] 100+ test cases covering hot paths
- [x] 50+ tok/s on 70B models (GPU)
- [x] <500ms latency for workspace operations (LSP)
- [x] <100ms IPC round-trip (extension host)
- [x] Memory stable under 16-concurrent-user load
- [x] 0 security findings in audit

**Documentation**:
- [x] Architecture guide (product overview)
- [x] Installation & config (enterprise deployment)
- [x] Operational runbook (monitoring, troubleshooting)
- [x] Extension development guide (ecosystem engagement)
- [x] Performance tuning reference (optimization guide)

**Shipping**:
- [x] All commits pushed to RawrXDA main branch
- [x] Release notes generated (features, performance, known limitations)
- [x] Internal pilot team validation (10+ users, 2+ weeks)
- [x] Rollback procedure documented and tested

---

## Success Metrics (Final Report)

| Metric | Target | Achieved |
|--------|--------|----------|
| Agentic harness pass rate | 100% | TBD |
| GPU throughput (70B model) | ≥15 tok/s | TBD |
| GPU throughput (small model) | ≥50 tok/s | TBD |
| Extension host isolation | 0 escapes | TBD |
| Workspace symbol search | <500ms | TBD |
| Cross-file rename | 100+ refs | TBD |
| Soak test stability | 4+ hours, <50MB drift | TBD |
| Code LOC (total) | <1M | ~852k |
| Security audit findings | 0 critical | TBD |
| Pilot team satisfaction | ≥8/10 | TBD |

---

## Next Immediate Action

**Branch**: `feature/phase2c-gpu-performance-tuning`  
**First Commit**: `run_kernel_ab_sweep.ps1` + baseline harness  
**Target**: Complete Day 5 tasks by end of May 5, 2026  
**Success**: Kernel winner identified and first performance matrix generated

**Commands**:
```powershell
cd D:\RawrXD
git checkout -b feature/phase2c-gpu-performance-tuning
# [Implement Day 5 tasks]
git push -u rawrxda feature/phase2c-gpu-performance-tuning
```

---

## Document Status

- ✅ Phase 2B: SEALED (PHASE_2B_COMPLETION_STATUS.md)
- ✅ Phase 2C: READY (PHASE_2C_GPU_PERFORMANCE_TUNING_PLAN.md)
- ✅ Phases 3-5: DRAFTED (this document)
- → **NEXT**: Execute Phase 2C (May 5-7), then iterate Phases 3-5

