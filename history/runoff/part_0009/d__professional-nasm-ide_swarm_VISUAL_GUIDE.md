# 🎨 Model Chaining System - Visual Guide

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    USER INTERFACE LAYER                         │
├─────────────────────────────────────────────────────────────────┤
│  CLI: chain_cli.py               │  Python API: chain_controller │
│  - list                          │  - execute_chain_on_code      │
│  - execute                       │  - execute_chain_on_file      │
│  - review / secure / document    │  - create_custom_chain        │
│  - optimize / create-chain       │  - QuickChainExecutor         │
└──────────────────┬───────────────┼──────────────────┬────────────┘
                   │               │                  │
┌──────────────────┴───────────────┴──────────────────┴────────────┐
│            CHAIN CONTROLLER LAYER (chain_controller.py)          │
│ ┌────────────────────────────────────────────────────────────┐  │
│ │ • Configuration loading                                    │  │
│ │ • Chain registration                                       │  │
│ │ • File/code processing                                    │  │
│ │ • Report generation                                       │  │
│ │ • Custom chain creation                                   │  │
│ └────────────────────────────────────────────────────────────┘  │
└──────────────────────────────┬───────────────────────────────────┘
                               │
┌──────────────────────────────┴───────────────────────────────────┐
│         MODEL CHAIN ORCHESTRATOR (model_chain_orchestrator.py)    │
│ ┌────────────────────────────────────────────────────────────┐  │
│ │ execute_chain()                                            │  │
│ │ - split_into_chunks()     → [Chunk1, Chunk2, ...]        │  │
│ │ - For each chunk:                                          │  │
│ │   - For each agent in chain:                              │  │
│ │     - process_chunk()     → ChainResult                   │  │
│ │ - Track execution         → ChainExecution                │  │
│ └────────────────────────────────────────────────────────────┘  │
└──┬──────────────────────────────────────────────────────────┬───┘
   │                                                          │
   └────────────────────────────┬─────────────────────────────┘
                                │
                    ┌───────────┴──────────┐
                    │                      │
    ┌───────────────┴──────┐   ┌──────────┴──────────┐
    │  CHAINABLE AGENTS    │   │   CHAINS CONFIG    │
    │ (per model wrapper)  │   │  (chains_config)   │
    ├──────────────────────┤   ├───────────────────┤
    │ • agent_id           │   │ • code_review     │
    │ • agent_role         │   │ • secure_coding   │
    │ • process_chunk()    │   │ • documentation   │
    │ • _build_prompt()    │   │ • optimization    │
    │ • state tracking     │   │ • complete_analysis
    │ • error handling     │   │ • debugging       │
    │                      │   │ • refactoring     │
    └──────────────────────┘   └───────────────────┘
            ↓                           ↑
    [Analyzer, Validator,      [Custom chains supported]
     Optimizer, Reviewer,
     Security, Debugger,
     Documenter, Formatter,
     Generator, Architect]
```

## Data Flow Diagram

```
Input Code File (10,000 lines)
        │
        ├──────→ Read File
        │
        ├──────→ Detect Language
        │
        ├──────→ Split into Chunks (500 lines each)
        │        [Chunk 1: Lines 1-500]
        │        [Chunk 2: Lines 501-1000]
        │        [... 20 chunks total ...]
        │
        ├──────→ FOR EACH CHUNK:
        │        FOR EACH FEEDBACK LOOP:
        │          FOR EACH AGENT IN CHAIN:
        │
        │            ┌─────────────────────────────┐
        │            │  ChainableAgent.process()   │
        │            ├─────────────────────────────┤
        │            │ 1. Build prompt             │
        │            │ 2. Call model               │
        │            │ 3. Extract analysis         │
        │            │ 4. Record metrics           │
        │            │ 5. Return ChainResult       │
        │            └─────────────────────────────┘
        │                       │
        │                       ├──→ Analyzer Result
        │                       ├──→ Validator Result
        │                       ├──→ Optimizer Result
        │                       └──→ Reviewer Result
        │
        ├──────→ Aggregate Results
        │
        ├──────→ Calculate Metrics
        │
        ├──────→ Generate Report
        │
        └──────→ Export (JSON)

Output: ChainExecution Report
  ├── Status: completed/failed
  ├── Duration: 45.23 seconds
  ├── Chunks: 20/20 processed
  ├── Success Rate: 100%
  └── Results: [80 per-model results]
```

## Execution Phases

```
INITIALIZATION PHASE
    │
    ├─→ Load chain config
    ├─→ Validate agents
    ├─→ Prepare execution tracker
    │
    ✓ Ready to process

ANALYSIS PHASE (First agent in chain)
    │
    ├─→ Analyzer reads chunk
    ├─→ Identifies structure
    ├─→ Returns findings
    │
    ✓ Analysis complete

PROCESSING PHASES (Middle agents)
    │
    ├─→ Validator checks
    ├─→ Optimizer suggests improvements
    ├─→ Accumulate results
    │
    ✓ Processing complete

FINALIZATION PHASE (Last agent in chain)
    │
    ├─→ Reviewer provides final assessment
    ├─→ Compile all suggestions
    │
    ✓ Chunk processing complete

    [REPEAT for all chunks and feedback loops]

COMPLETION PHASE
    │
    ├─→ Aggregate metrics
    ├─→ Calculate success rate
    ├─→ Generate report
    ├─→ Export results
    │
    ✓ Execution complete
```

## Chunk Processing Timeline

```
Chunk 1 (Lines 1-500)     Chunk 2 (Lines 501-1000)   ...
├─ Analyzer ──┐           ├─ Analyzer ──┐
│   30s       │           │   30s       │
├─ Validator ─┤           ├─ Validator ─┤
│   35s       │           │   35s       │
├─ Optimizer ─┤           ├─ Optimizer ─┤
│   40s       │           │   40s       │
└─ Reviewer ──┘           └─ Reviewer ──┘
   25s                        25s
─────────────            ─────────────
Total: 130s              Total: 130s
(2+ minutes)             (2+ minutes)

Total for 20 chunks: ~45 minutes (with 4-agent chain)
```

## Agent Role Specialization Matrix

```
┌────────────┬──────────┬────────────┬──────────────┐
│ Agent Role │ Input    │ Processing │ Output       │
├────────────┼──────────┼────────────┼──────────────┤
│ Analyzer   │ Raw Code │ Structure  │ Findings     │
│ Validator  │ Analysis │ Correctness│ Issues List  │
│ Optimizer  │ Previous │ Performance│ Suggestions  │
│ Reviewer   │ All Prev │ Quality    │ Final Review │
├────────────┼──────────┼────────────┼──────────────┤
│ Security   │ Raw Code │ Vulns.     │ Issues       │
│ Debugger   │ Analysis │ Bug/Fix    │ Solutions    │
├────────────┼──────────┼────────────┼──────────────┤
│ Documenter │ Code     │ Context    │ Docs         │
│ Formatter  │ Code     │ Style      │ Formatted    │
├────────────┼──────────┼────────────┼──────────────┤
│ Generator  │ Spec     │ Creation   │ New Code     │
│ Architect  │ Design   │ Structure  │ Decisions    │
└────────────┴──────────┴────────────┴──────────────┘
```

## Chain Examples

### Code Review Chain
```
Input Code
    ↓
[Analyzer] ──→ Identify patterns, complexity
    ↓
[Validator] ──→ Check correctness, edge cases
    ↓
[Optimizer] ──→ Performance opportunities
    ↓
[Reviewer] ──→ Final quality assessment
    ↓
Output: Comprehensive Review Report
```

### Security Audit Chain (2 Feedback Loops)
```
LOOP 1:
Input Code
    ↓
[Analyzer] ──→ Code structure
    ↓
[Security] ──→ Vulnerability scan
    ↓
[Debugger] ──→ Fix opportunities
    ↓
[Optimizer] ──→ Secure performance

LOOP 2 (with Loop 1 context):
[Analyzer] ──→ Re-analyze with feedback
    ↓
[Security] ──→ Deep security check
    ↓
[Debugger] ──→ Additional fixes
    ↓
[Optimizer] ──→ Final optimization
    ↓
Output: In-depth Security Report
```

### Performance Optimization Chain
```
Input Code
    ↓
[Analyzer] ──→ Performance bottlenecks
    ↓
[Optimizer] ──→ Optimization strategies
    ↓
[Validator] ──→ Verify optimization
    ↓
Output: Optimized Code + Benchmarks
```

## Report Structure Visualization

```
ChainExecution Report
├─ Metadata
│  ├─ execution_id: "exec_code_review_chain_..."
│  ├─ chain_id: "code_review_chain"
│  ├─ status: "completed"
│  └─ timestamp: "2024-11-25T10:30:00"
│
├─ Metrics
│  ├─ duration_seconds: 45.23
│  ├─ total_chunks: 20
│  ├─ processed_chunks: 20
│  ├─ failed_chunks: 0
│  └─ success_rate: "100%"
│
└─ Results (80 total = 20 chunks × 4 agents)
   ├─ Result[0]: Analyzer on Chunk 1
   │  ├─ execution_time_ms: 610.5
   │  ├─ success: true
   │  ├─ analysis: {...}
   │  └─ suggestions: [...]
   │
   ├─ Result[1]: Validator on Chunk 1
   │  ├─ execution_time_ms: 720.3
   │  ├─ success: true
   │  ├─ analysis: {...}
   │  └─ suggestions: [...]
   │
   ├─ Result[2]: Optimizer on Chunk 1
   │  └─ ...
   │
   ├─ Result[3]: Reviewer on Chunk 1
   │  └─ ...
   │
   └─ ... (76 more results for Chunks 2-20)
```

## State Machine: Chunk Processing

```
┌──────────┐
│  PENDING │ ← New chunk waiting
└────┬─────┘
     │
     ↓
┌──────────────┐
│  PROCESSING  │ ← Currently processing with ChainableAgent
└────┬─────────┘
     │
     ├─→ Success ──→ ┌──────────┐
     │               │ COMPLETE │
     │               └──────────┘
     │
     ├─→ Failure ──→ ┌────────┐
                     │ FAILED │
                     └────────┘
     │
     └─→ Timeout ──→ ┌──────────┐
                     │ TIMEOUT  │
                     └──────────┘
```

## Performance Comparison

```
Single Model Processing:
10,000 lines → 1 Model → ~2 hours

Model Chain (4 agents):
10,000 lines → 20 chunks → 4 agents each → ~45 minutes

Why faster:
- Parallel processing opportunity
- Specialized models (faster than generalist)
- Chunked context (reduces token count)
- Cached results (between phases)
```

## CLI Command Flow

```
User Input: python chain_cli.py review mycode.py
    │
    ├─→ Parse arguments
    │
    ├─→ Create ChainCLI instance
    │
    ├─→ Call: execute_on_file()
    │   │
    │   ├─→ Read file
    │   ├─→ Detect language
    │   └─→ Call controller.execute_chain_on_file()
    │       │
    │       └─→ ModelChainOrchestrator.execute_chain()
    │
    ├─→ Get report
    │
    ├─→ Print summary
    │
    ├─→ Save to JSON (if requested)
    │
    └─→ Exit with status code
```

## Integration Points

```
Professional NASM IDE Architecture
│
├─ Swarm Controller (Existing)
│  │
│  └─ Model Pool
│     ├─ 10 models (200-400MB each)
│     └─ Agent capabilities
│
├─ Model Chain System (New)
│  │
│  ├─ ChainController
│  ├─ ChainableAgent (wraps swarm agents)
│  └─ ModelChainOrchestrator
│
└─ User Interface
   ├─ CLI (chain_cli.py)
   ├─ Python API (chain_controller.py)
   └─ Quick Shortcuts (QuickChainExecutor)
```

## Memory Usage Pattern

```
Time ─────────────────────────────────────→

Chunk 1 loaded
    ↓
Analyzer processing  [■ Analyzer model]
    ↓
Validator processing [■ Analyzer + Validator models]
    ↓
Optimizer processing [■ Analyzer + Validator + Optimizer]
    ↓
Reviewer processing  [■ All 4 models]
    ↓
Results stored, models released
    ↓
Chunk 2 loaded
    ↓
    [Pattern repeats...]

Peak memory: ~1-2 GB for 4-model chain
(200-400MB per model × 4 + overhead)
```

---

**Visual Guide - Model Chain Orchestrator**
