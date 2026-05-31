---
name: quality-gates
description: "**Quality Gate Validation / Production-Ready Code Finishers** — Enforces evidence-based go or no-go decisions for each day and phase of the 14-day RawrXD expansion. Use when: validating implementation claims, checking readiness to advance, auditing regressions, confirming performance and security thresholds, or issuing final production decisions."
version: "1.0.0"
specialistAgent: "@AgentPolish"
alwaysInject: true
priority: 20
phases:
  - phase1
  - phase2
  - phase3
  - phase4
---

# Quality Gate Validation System

## Mission

This skill enforces hard validation for the 14-day expansion so progress reflects working production code, not optimistic status updates.

When active, this skill must:
- Require direct evidence before marking any gate complete
- Block phase transitions on unresolved correctness or security risks
- Distinguish between pass, proceed with risk, and blocked outcomes
- Keep gate logic aligned with the expansion-coordinator daily execution model

## Overview

This skill provides systematic validation of quality gates for each phase of the RawrXD 14-day production expansion, ensuring no phase transitions occur without meeting completion criteria.

## Core Validation Rules

1. Evidence over claims: A gate cannot pass without reproducible build, test, runtime, or benchmark proof.
2. Fail closed on security: Any unresolved critical security issue blocks progression.
3. Correctness before optimization: Performance wins do not offset broken behavior.
4. No hidden regressions: A gate fails if core workflows regress, even if the target feature works.
5. Daily closure required: Each day ends with a verdict and clear next action.

## Gate Verdicts

Use exactly one verdict for every gate review:

- Pass: All required evidence is present and acceptance criteria are met.
- Proceed with risk: Non-critical gaps remain with explicit owner and mitigation deadline.
- Blocked: Critical criteria not met or evidence is missing or invalid.

## Required Evidence Pack

Every gate review must include:

1. Build Evidence: Compile status and relevant target results.
2. Runtime Evidence: Smoke or scenario execution proving behavior.
3. Regression Evidence: Checks showing existing workflows still work.
4. Performance Evidence: Measurements when performance targets apply.
5. Security Evidence: Boundary and abuse-path validation when applicable.
6. Documentation Evidence: Updated operational notes or handoff instructions.

If any required evidence category is missing, default verdict is Blocked.

## 14-Day Gate Ledger

Use this ledger for daily validation in addition to phase summaries.

### Day 1 Gate: Baseline Integrity
Criteria:
- Current completion claims are reconciled with real implementation status.
- Critical path and ownership are explicitly assigned.

Validation Tests:
- [ ] Build baseline run and status capture complete
- [ ] Blockers list includes severity and owner
- [ ] Remaining work is ordered by dependency and risk

Exit Decision:
- Pass only if no ambiguous ownership remains for critical-path work.

### Day 2 Gate: Workflow Persistence
Criteria:
- Agent execution state survives restart and resumes accurately.

Validation Tests:
- [ ] Multi-step run survives restart and continues correctly
- [ ] Persisted state includes pending operations and context
- [ ] Corrupt or partial state recovery path fails safely

### Day 3 Gate: Memory Retrieval Quality
Criteria:
- Memory retrieval is relevant, safe, and useful for real task continuation.

Validation Tests:
- [ ] Retrieval returns contextually relevant results in representative scenarios
- [ ] Missing or stale memory is handled without unsafe behavior
- [ ] Stored knowledge can be reused in subsequent task execution

### Day 4 Gate: Todo and Dependency Execution
Criteria:
- Planning artifacts are wired into actual execution and status transitions.

Validation Tests:
- [ ] Task dependencies enforce execution order
- [ ] Task state transitions are durable and accurate
- [ ] Blocked tasks surface actionable blocker metadata

### Day 5 Gate: Autonomous Operation Demonstration
Criteria:
- A meaningful multi-step task completes with minimal intervention and correct verification.

Validation Tests:
- [ ] Autonomous run performs planning, execution, and validation
- [ ] Human approval boundaries are respected where required
- [ ] Failure handling path is demonstrated

### Day 6 Gate: Extension Host Runtime Foundation
Criteria:
- Extension host lifecycle is functional and failure-aware.

Validation Tests:
- [ ] Host startup and shutdown are reliable
- [ ] Crash behavior is isolated from main IDE process
- [ ] Lifecycle telemetry or logs support debugging

### Day 7 Gate: Process Isolation and IPC
Criteria:
- Extension host IPC is reliable and process isolation is enforced.

Validation Tests:
- [ ] Bidirectional IPC contract validates payloads and responses
- [ ] Timeout and retry behavior is bounded
- [ ] Extension process isolation is proven under fault scenarios

### Day 8 Gate: Security Sandbox Enforcement
Criteria:
- Sandbox boundaries are technically enforced, not advisory.

Validation Tests:
- [ ] Unauthorized file access attempts are blocked
- [ ] Unauthorized process or network actions are blocked
- [ ] Violation handling fails closed and is auditable

### Day 9 Gate: VS Code API Compatibility Slice
Criteria:
- A useful subset of extension APIs works through the hosted model.

Validation Tests:
- [ ] Real extension flow executes using supported API surface
- [ ] API calls return expected behavior under normal and failure conditions
- [ ] Unsupported APIs fail predictably with clear diagnostics

### Day 10 Gate: Workspace Index Truth
Criteria:
- Workspace indexing and symbol truth are reliable enough for global operations.

Validation Tests:
- [ ] Symbol queries match workspace reality on representative samples
- [ ] Document lifecycle changes update index correctness
- [ ] Cross-file references resolve consistently

### Day 11 Gate: Rename and Symbol Search Performance
Criteria:
- Cross-file rename correctness and global search responsiveness meet targets.

Validation Tests:
- [ ] Rename updates references correctly across files
- [ ] Rename failure path does not corrupt workspace state
- [ ] Global symbol search meets latency targets on representative workload

### Day 12 Gate: LSP Compliance and IntelliSense Quality
Criteria:
- LSP behavior and advanced completion quality satisfy required expectations.

Validation Tests:
- [ ] Required LSP behaviors are implemented and validated
- [ ] Completion quality improves with context-sensitive behavior
- [ ] Protocol errors are handled gracefully with usable diagnostics

### Day 13 Gate: Performance and Stability
Criteria:
- Throughput, latency, and memory stability improvements are measurable and repeatable.

Validation Tests:
- [ ] Before and after benchmark delta is documented
- [ ] Stress runs show no critical instability or leak trend
- [ ] Core interactive operations remain responsive under load

### Day 14 Gate: Final Production Readiness
Criteria:
- End-to-end production readiness is evidenced across functionality, security, performance, and operations.

Validation Tests:
- [ ] Final integration and regression suites pass
- [ ] Security review has no unresolved critical issues
- [ ] Performance targets are met on representative workloads
- [ ] Deployment and recovery documentation is complete and actionable

## Validation Framework

### **Phase 1: Agent Polish - Quality Gates**

#### **Day 2 Gate: Workflow State Persistence**
**Criteria**: Agent execution states can be serialized, saved, and restored
**Validation Tests**:
- [ ] Start multi-step workflow, save state, restart IDE, resume successfully
- [ ] State includes tool chain dependencies and execution context
- [ ] Error recovery works when resuming from checkpoint
- [ ] Progress can be tracked across session boundaries

**Testing Commands**:
```powershell
# Test workflow persistence
.\test-agent-persistence.ps1 -Scenario MultiStepWorkflow
```

#### **Day 3 Gate: Enhanced Memory System**  
**Criteria**: Semantic indexing, knowledge retrieval, auto-summarization functional
**Validation Tests**:
- [ ] Memory can store and retrieve project-specific knowledge
- [ ] Semantic queries return relevant results with proper scoring
- [ ] Auto-summarization reduces context while preserving key information
- [ ] Session memory persists across conversation restarts

**Testing Commands**:
```powershell
# Test memory system capabilities
.\test-memory-system.ps1 -TestSuite Semantic
```

#### **Day 4 Gate: Todo/Task Integration**
**Criteria**: Agent workflows integrate with persistent task management
**Validation Tests**:
- [ ] Complex tasks can be broken down into manageable subtasks
- [ ] Task dependencies are tracked and respected
- [ ] Progress is automatically updated as subtasks complete
- [ ] Task state survives agent and IDE restarts

#### **Day 5 Gate: Autonomous Operation**
**Criteria**: Self-directed task execution with quality assurance
**Validation Tests**:
- [ ] Agent can decompose high-level goal into executable steps
- [ ] Autonomous execution completes without human intervention
- [ ] Quality checks validate work before marking complete
- [ ] Human-in-loop points work for critical decisions

### **Phase 2: Extension Host - Quality Gates**

#### **Day 7 Gate: Process Isolation & IPC**
**Criteria**: Extensions run in secure isolated processes with working IPC
**Validation Tests**:
- [ ] Extension loads in separate process (not main IDE process)
- [ ] IPC channel enables bidirectional communication
- [ ] Extension process crash doesn't affect main IDE
- [ ] Memory spaces are properly isolated

**Testing Commands**:
```powershell
# Test extension process isolation
.\test-extension-host.ps1 -TestSuite ProcessIsolation
```

#### **Day 8 Gate: Security Sandboxing**
**Criteria**: Sandbox enforces security boundaries properly
**Validation Tests**:
- [ ] File system access restricted to approved directories
- [ ] Network access respects permission configuration
- [ ] Registry access limited and controlled
- [ ] No privilege escalation possible from extension context

#### **Day 9 Gate: VS Code API Compatibility**
**Criteria**: Core VS Code extension APIs work with real extensions
**Validation Tests**:
- [ ] Real extension flow executes using supported API surface
- [ ] API calls return expected behavior under normal and failure conditions
- [ ] Unsupported APIs fail predictably with clear diagnostics

### **Phase 3: LSP Final Features - Quality Gates**

#### **Day 10 Gate: Workspace Index Truth**
**Criteria**: Symbol index is accurate and incrementally maintained
**Validation Tests**:
- [ ] Index reflects current workspace state after edits
- [ ] Cross-file references resolve correctly
- [ ] Index rebuild completes in <30s for 100K LOC workspace

#### **Day 11 Gate: Rename and Search Performance**
**Criteria**: Cross-file rename and global search meet targets
**Validation Tests**:
- [ ] Rename updates all references correctly
- [ ] Rename rollback works if operation fails mid-way
- [ ] Global symbol search <500ms for 100K LOC workspace

#### **Day 12 Gate: LSP Compliance**
**Criteria**: LSP 3.17 specification compliance
**Validation Tests**:
- [ ] All required LSP methods implemented
- [ ] Protocol error handling is graceful
- [ ] Completion quality improves with context

### **Phase 4: Performance & Finalization - Quality Gates**

#### **Day 13 Gate: Performance Optimization**
**Criteria**: Measurable performance improvements
**Validation Tests**:
- [ ] AI inference throughput improved 30-50%
- [ ] Memory usage stable under stress (no leaks)
- [ ] UI remains responsive under load

**Testing Commands**:
```powershell
# Run performance benchmarks
.\benchmark-runner.ps1 -Suite Performance -Iterations 5
```

#### **Day 14 Gate: Production Readiness**
**Criteria**: Ready for enterprise deployment
**Validation Tests**:
- [ ] All integration tests pass
- [ ] Security review complete with no critical issues
- [ ] Documentation complete and actionable
- [ ] Deployment scripts tested and verified

## Gate Enforcement Rules

### Automatic Gate Blocking
The following conditions automatically block gate progression:

1. **Build Failure**: Any required build target fails
2. **Test Regression**: Existing tests fail that previously passed
3. **Security Violation**: New security vulnerability introduced
4. **Performance Regression**: Performance worse than baseline
5. **Documentation Gap**: Required documentation missing or incomplete

### Gate Override Protocol
In exceptional circumstances, a gate may be overridden:

1. **Risk Assessment**: Document the risk of proceeding
2. **Mitigation Plan**: Define concrete steps to address the gap
3. **Owner Assignment**: Assign explicit owner for gap closure
4. **Deadline**: Set hard deadline for gap closure
5. **Escalation**: Require approval from project lead

### Gate Audit Trail
All gate decisions are logged:

```
[YYYY-MM-DD HH:MM:SS] Gate: <name>
  Verdict: Pass|ProceedWithRisk|Blocked
  Evidence: <summary>
  Owner: <name>
  Next Action: <description>
```

## Integration with Expansion Coordinator

This skill works in conjunction with the expansion-coordinator skill:

- **expansion-coordinator** defines WHAT to do and WHEN
- **quality-gates** defines HOW to verify and WHAT evidence is required

Both skills are always injected together to ensure coordination and validation are synchronized.

## Success Criteria

- All 14 gates have explicit pass/fail verdicts
- No gate passes without evidence
- Phase transitions blocked until all gates in current phase pass
- Audit trail complete for all decisions
- Risk items have owners and deadlines
