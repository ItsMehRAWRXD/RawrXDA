---
name: expansion-coordinator
description: "**14-Day Expansion Coordinator / Production-Ready Code Finishers** — Drives RawrXD from partial implementation to production-ready completion through phase planning, specialist routing, code-finishing priorities, quality gates, blocker removal, and end-to-end delivery control. Use when: converting roadmap items into shippable work, coordinating specialized agents, validating readiness, managing risks, or closing remaining implementation gaps."
version: "1.0.0"
specialistAgent: "@AgentPolish"
alwaysInject: true
priority: 10
phases:
  - phase1
  - phase2
  - phase3
  - phase4
---

# RawrXD 14-Day Expansion Coordinator

## Mission

This skill turns the RawrXD 14-day expansion plan into an execution system for production-ready code finishers.

It is not a passive project tracker. It is an active delivery coordinator that:
- Converts vague goals into implementation-ready workstreams
- Routes work to the correct specialist agent
- Forces validation before claiming progress
- Tracks blockers, dependencies, and rollback risk
- Pushes every phase toward working, verified, production-grade code

## Workflow Overview

This skill orchestrates the complete production-ready expansion of RawrXD IDE from 72% to 100% completion across 4 specialized phases.

Use this skill when the task is larger than a single patch and requires coordinated execution, sequencing, verification, or phase-level decision making.

### Phase Management Strategy

1. **Track Current Phase**: Monitor active phase and progress within it
2. **Validate Quality Gates**: Ensure each phase meets completion criteria before advancement
3. **Coordinate Specialists**: Route work to appropriate specialized agents 
4. **Manage Dependencies**: Track cross-phase dependencies and blockers
5. **Risk Mitigation**: Identify and address issues before they become blockers

## Operating Rules

When this skill is active, follow these rules:

1. **Finish Real Code First**: Prefer implementation, integration, validation, and defect removal over planning language.
2. **No Synthetic Progress**: Do not mark a gate complete without concrete evidence such as a build, test, benchmark, or code-path verification.
3. **Work the Critical Path**: Prioritize items that unlock other work or reduce risk across phases.
4. **Route by Specialization**: Use the mapped specialist agent when work clearly falls into its domain.
5. **Surface Blocking Assumptions**: State missing prerequisites, environmental constraints, and unresolved dependencies early.
6. **Preserve Momentum**: If blocked on one path, identify the next highest-value unblocked production task.
7. **Close the Loop**: Every major action should end with verification, updated status, and the next concrete step.

## Production-Ready Code Finisher Model

Treat each day as a delivery slice with four required outputs:

1. **Implemented**: Real code or configuration landed
2. **Integrated**: Connected to the rest of the system, not isolated
3. **Validated**: Build/test/runtime evidence collected
4. **Documented**: Status, risks, and handoff notes updated

If one of these four is missing, the slice is not done.

## Phase Coordination Protocol

### **Phase 1: Agent Polish (Days 1-5)**
**Specialist Agent**: `@AgentPolish`
**Focus**: Multi-step persistence, memory management, autonomous operation

#### Primary Deliverables
- Workflow save and restore across sessions
- Durable memory retrieval with useful indexing or lookup behavior
- Todo and dependency tracking integrated into agent execution flow
- Autonomous multi-step operation with verification checkpoints

#### Quality Gates
- [ ] **Day 2**: Workflow state persistence functional
- [ ] **Day 3**: Enhanced memory system with semantic indexing
- [ ] **Day 4**: Todo/task integration with agent workflows
- [ ] **Day 5**: Autonomous operation demonstration

#### Success Criteria
- Agent workflows can be saved/restored across sessions
- Memory system supports intelligent knowledge retrieval
- Todo system tracks and manages complex task dependencies
- Autonomous mode completes multi-step tasks without intervention

### **Phase 2: Native Extension Host (Days 6-9)**
**Specialist Agent**: `@ExtensionHost`
**Focus**: MASM process isolation, extension sandboxing, VS Code API compatibility

#### Primary Deliverables
- Extension host process boundary with reliable lifecycle management
- Hardened IPC contract between IDE and hosted extensions
- Security boundary enforcement for filesystem, process, and network access
- Working subset of VS Code-compatible extension APIs

#### Quality Gates
- [ ] **Day 7**: Extension process isolation with IPC working
- [ ] **Day 8**: Security sandboxing enforces boundaries
- [ ] **Day 9**: Core VS Code extension APIs functional

#### Success Criteria
- Extensions run in isolated processes with secure IPC
- Sandboxing prevents unauthorized system access
- Real VS Code extensions load and function correctly

### **Phase 3: LSP Final Features (Days 10-12)**
**Specialist Agent**: `@LSPComplete` 
**Focus**: Workspace-wide operations, advanced language intelligence

#### Primary Deliverables
- Cross-file rename and reference update reliability
- Global symbol indexing and fast search over large workspaces
- IntelliSense completion quality improvements with stronger context handling
- Compliance checks against required LSP 3.17 behaviors

#### Quality Gates
- [ ] **Day 11**: Cross-file rename with dependency tracking
- [ ] **Day 11**: Global symbol search <500ms response time
- [ ] **Day 12**: Advanced IntelliSense with ML suggestions
- [ ] **Day 12**: LSP 3.17 specification compliance

#### Success Criteria
- Workspace-wide operations work reliably across large projects
- Language intelligence matches or exceeds VS Code capabilities
- Performance targets met for interactive operations

### **Phase 4: Performance & Finalization (Days 13-14)**
**Specialist Agent**: `@Performance`
**Focus**: Speculative decoding, optimization, testing, documentation

#### Primary Deliverables
- Performance bottleneck removal on the hot path
- Memory and resource stability verification under stress
- Final integration validation across core product lanes
- Deployment, usage, and recovery documentation suitable for production handoff

#### Quality Gates
- [ ] **Day 13**: 30-50% AI inference performance improvement
- [ ] **Day 13**: Memory optimization with leak elimination
- [ ] **Day 14**: All enterprise performance targets met
- [ ] **Day 14**: Complete documentation and deployment guides

#### Success Criteria
- Performance benchmarks meet enterprise standards
- Security assessment passes with no critical vulnerabilities
- Documentation enables enterprise deployment and usage

## Coordination Workflow

### **Daily Check-in Process**
1. **Progress Review**: Assess completion status against daily targets
2. **Blocker Identification**: Identify any impediments or dependency issues
3. **Resource Allocation**: Direct appropriate specialist agents to priority work
4. **Risk Assessment**: Evaluate potential risks to timeline or quality
5. **Next-Day Planning**: Set priorities and objectives for following day

### **Daily Execution Contract**

For each day in the 14-day expansion, produce:

1. **Current State**: What is actually working right now
2. **Target Delta**: What must be true by end of day
3. **Implementation Queue**: Ordered list of code-finishing tasks
4. **Assigned Specialist**: Which agent owns each task and why
5. **Verification Plan**: Build, test, benchmark, or smoke checks required
6. **Exit Criteria**: Conditions that must be met before the day is considered complete
7. **Fallback Path**: What to do if the main plan is blocked

### **Phase Transition Protocol**
1. **Quality Gate Validation**: Verify all phase objectives met
2. **Integration Testing**: Ensure new features integrate with existing systems
3. **Documentation Update**: Complete phase documentation and handoff notes
4. **Next Phase Preparation**: Brief next specialist agent and provide context
5. **Stakeholder Communication**: Report phase completion and next phase plans

### **Risk Management Framework**
- **Technical Risks**: Architecture mismatches, performance bottlenecks
- **Integration Risks**: Cross-component compatibility issues
- **Timeline Risks**: Scope creep, underestimated complexity
- **Quality Risks**: Insufficient testing, security vulnerabilities

### **Blocker Handling Rules**

Escalate immediately when any of the following occurs:
- A quality gate depends on unverified architecture assumptions
- A build or runtime failure invalidates prior completion claims
- A specialist agent produces output that does not land in working code
- Performance or security targets require redesign rather than patching
- A dependency from an earlier phase was assumed complete but is not actually stable

### **Success Metrics Dashboard**
- **Completion Percentage**: Current % complete toward 100% target
- **Phase Progress**: Days completed vs. planned for current phase
- **Quality Gates**: Number passed vs. total required
- **Performance Metrics**: Benchmarks against enterprise targets
- **Risk Status**: Open risks and mitigation progress

## 14-Day Execution Map

Use this schedule to convert the roadmap into daily production-finishing work. Each day must end with shipped changes, verification evidence, and an explicit go/no-go status for the next day.

### **Day 1: Reality Baseline And Critical Path Lock**
- **Objective**: Establish what is genuinely complete, broken, partial, or unverifiable.
- **Primary Work**: Audit current implementation against the 4 phases, identify false-complete claims, and lock the critical path.
- **Primary Specialist**: `@AgentPolish`
- **Required Evidence**: Baseline build status, current blockers list, prioritized implementation queue.
- **Exit Criteria**: No ambiguous ownership for remaining work.

### **Day 2: Workflow Persistence Completion**
- **Objective**: Make agent workflow state persistence actually survive restart and restore correctly.
- **Primary Work**: Save and reload execution state, recover pending steps, validate replay or continuation behavior.
- **Primary Specialist**: `@AgentPolish`
- **Required Evidence**: Restart-persistence smoke test and failure-path verification.
- **Exit Criteria**: Day 2 quality gate can be demonstrated end-to-end.

### **Day 3: Memory Retrieval Hardening**
- **Objective**: Turn memory from storage into useful retrieval.
- **Primary Work**: Improve lookup quality, indexing, recall relevance, and failure handling for missing or stale memory entries.
- **Primary Specialist**: `@AgentPolish`
- **Required Evidence**: Retrieval scenarios showing relevant recall and safe behavior on bad inputs.
- **Exit Criteria**: Memory system supports intelligent reuse rather than raw accumulation.

### **Day 4: Todo And Dependency Integration**
- **Objective**: Bind planning artifacts to execution state so agents can track and complete real work.
- **Primary Work**: Integrate todo/task tracking with workflow execution, dependency ordering, and status transitions.
- **Primary Specialist**: `@AgentPolish`
- **Required Evidence**: Multi-step task run showing pending, in-progress, completed, and blocked states.
- **Exit Criteria**: Day 4 quality gate is supported by live workflow behavior.

### **Day 5: Autonomous Operation Demo**
- **Objective**: Prove the system can complete a meaningful multi-step task with limited intervention.
- **Primary Work**: Execute a contained autonomous scenario that reads context, makes decisions, applies changes, and validates results.
- **Primary Specialist**: `@AgentPolish`
- **Required Evidence**: Demonstration log with checkpoints, outputs, and failure recovery path.
- **Exit Criteria**: Phase 1 is demonstrated, not just described.

### **Day 6: Extension Host Skeleton To Real Runtime**
- **Objective**: Replace partial extension-host assumptions with a working process model.
- **Primary Work**: Establish extension host lifecycle, startup, shutdown, crash handling, and broker boundaries.
- **Primary Specialist**: `@ExtensionHost`
- **Required Evidence**: Host process launch and termination behavior verified under normal and failure conditions.
- **Exit Criteria**: Phase 2 begins from a real runtime foundation.

### **Day 7: IPC And Isolation Gate**
- **Objective**: Complete reliable, bounded IPC between IDE and extension host.
- **Primary Work**: Implement request/response channels, contract validation, timeout behavior, and process isolation guarantees.
- **Primary Specialist**: `@ExtensionHost`
- **Required Evidence**: IPC smoke tests, timeout tests, and process isolation proof.
- **Exit Criteria**: Day 7 quality gate passes with working IPC.

### **Day 8: Security Sandbox Enforcement**
- **Objective**: Enforce extension boundaries instead of trusting cooperative behavior.
- **Primary Work**: Restrict unauthorized filesystem, process, and network actions; fail closed on violations.
- **Primary Specialist**: `@ExtensionHost`
- **Required Evidence**: Negative tests showing blocked unauthorized operations.
- **Exit Criteria**: Day 8 quality gate is backed by enforcement, not policy text.

### **Day 9: VS Code API Compatibility Slice**
- **Objective**: Make a useful subset of VS Code-style extension APIs function in the hosted model.
- **Primary Work**: Implement the smallest high-value compatible surface that unlocks real extensions.
- **Primary Specialist**: `@ExtensionHost`
- **Required Evidence**: One or more real extension flows functioning through supported APIs.
- **Exit Criteria**: Phase 2 ends with practical compatibility, not placeholder stubs.

### **Day 10: LSP Indexing And Workspace Truth**
- **Objective**: Build or harden the workspace-wide symbol and reference foundation.
- **Primary Work**: Implement or improve symbol indexing, cross-file reference tracking, and incremental update correctness.
- **Primary Specialist**: `@LSPComplete`
- **Required Evidence**: Symbol queries return correct results on representative samples; index updates after edits.
- **Exit Criteria**: Workspace truth is reliable enough for global operations.

### **Day 11: Rename And Symbol Search**
- **Objective**: Cross-file rename and global symbol search meet quality and performance targets.
- **Primary Work**: Harden rename correctness, optimize search latency, handle edge cases.
- **Primary Specialist**: `@LSPComplete`
- **Required Evidence**: Rename updates references correctly; search <500ms on large workspaces.
- **Exit Criteria**: Day 11 quality gates pass with measurable performance.

### **Day 12: LSP Compliance And IntelliSense**
- **Objective**: LSP behavior and completion quality satisfy required expectations.
- **Primary Work**: Validate LSP 3.17 compliance, improve completion relevance with context.
- **Primary Specialist**: `@LSPComplete`
- **Required Evidence**: LSP test suite passes; completion quality improves with ML suggestions.
- **Exit Criteria**: Phase 3 ends with production-grade language intelligence.

### **Day 13: Performance Optimization**
- **Objective**: Achieve 30-50% AI inference performance improvement and eliminate memory leaks.
- **Primary Work**: Profile hot paths, optimize speculative decoding, fix leak sources.
- **Primary Specialist**: `@Performance`
- **Required Evidence**: Before/after benchmarks show target improvement; stress tests stable.
- **Exit Criteria**: Performance targets met with no critical instability.

### **Day 14: Final Production Readiness**
- **Objective**: Complete end-to-end production readiness across all dimensions.
- **Primary Work**: Final integration testing, security review, documentation, deployment guides.
- **Primary Specialist**: `@Performance`
- **Required Evidence**: All gates pass; security review clean; documentation actionable.
- **Exit Criteria**: RawrXD is ready for enterprise deployment.
