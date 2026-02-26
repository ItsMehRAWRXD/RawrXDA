/**
 * @file AUTONOMOUS_AGENT_IMPLEMENTATION_ROADMAP.md
 * @brief Complete agentic action system specification for RawrXD IDE
 * @date December 5, 2025
 *
 * This document outlines all components needed for full autonomous IDE operations
 * with cursor-style "wish" execution, self-patching, and zero-touch automation.
 */

# RawrXD IDE - Full Autonomous Agent Implementation Roadmap

## Executive Summary

Current State:
- ✅ MASM Editor with ghost text (TriggerAgent/AcceptAgent stubs in assembly)
- ✅ Multi-file search widget (async, thread-safe, .gitignore aware)
- ✅ AutoBootstrap skeleton (wish → plan → execute pipeline)
- ✅ Agent infrastructure files exist (30+ files in src/agent/)
- ⚠️ **CRITICAL GAP**: Most agent implementations are stub/placeholder

## Missing Implementations (Blocking Full Autonomy)

### 1. **Agentic Model Integration** (TIER-1: Foundation)

**Current State:** AutoBootstrap::startWithWishInternal() exists but agent LLM invocation is stubbed

**What Needs Implementation:**
```cpp
// src/agent/model_invoker.hpp (NEW)
class ModelInvoker {
public:
    struct InvocationParams {
        QString wish;                 // User's natural language request
        QString context;              // IDE state context
        QStringList tools;            // Available tools (search, patch, build)
        int maxTokens = 2000;
        double temperature = 0.7;
    };
    
    // Core method: Transform wish → structured action plan
    QJsonArray invokeAgent(const InvocationParams& params);
    
    // Fetch response from Ollama (local) or Claude/GPT (cloud)
    QString queryLLM(const QString& prompt);
    
    // Parse LLM output into actionable steps
    QJsonArray parseAgentResponse(const QString& llmOutput);
};
```

**Implementation Tasks:**
- [ ] Ollama client integration (existing GGUF_SERVER infrastructure to leverage)
- [ ] Prompt engineering for wish→plan transformation
- [ ] JSON response parsing (extract tool calls, arguments)
- [ ] Error recovery if LLM refuses request

**Files to Create:**
- `src/agent/model_invoker.cpp`
- `src/agent/model_invoker.hpp`

---

### 2. **Action Executor Framework** (TIER-1: Foundation)

**Current State:** Agent main loop references tools but execution is incomplete

**What Needs Implementation:**
```cpp
// src/agent/action_executor.hpp (NEW)
class ActionExecutor : public QObject {
    Q_OBJECT
public:
    enum ActionType {
        FileEdit,          // Modify/create files
        SearchFiles,       // Use multi_file_search
        RunBuild,          // cmake/msbuild
        ExecuteTests,      // Run test suite
        CommitGit,         // Version control
        QueryKnowledge,    // Ask human
        InvokeAgent        // Recursive agent call
    };
    
    struct Action {
        ActionType type;
        QString target;
        QJsonObject params;
        QString description;
    };
    
    // Execute single action with observability
    QJsonObject execute(const Action& action);
    
    // Execute action chain with failure recovery
    bool executePlan(const QJsonArray& actions);
    
signals:
    void actionStarted(const QString& description);
    void actionProgress(const QString& status);
    void actionCompleted(const QString& result);
    void actionFailed(const QString& error, bool recoverable);
};
```

**Implementation Tasks:**
- [ ] FileEdit action (integrates with file_manager.h for safe edits)
- [ ] SearchFiles action (uses MultiFileSearchWidget)
- [ ] RunBuild action (Qt/CMake integration)
- [ ] ExecuteTests action (run test binaries)
- [ ] CommitGit action (git2-cpp bindings)
- [ ] QueryKnowledge action (human-in-the-loop)
- [ ] Failure recovery with backtracking

**Files to Create:**
- `src/agent/action_executor.cpp`
- `src/agent/action_executor.hpp`

---

### 3. **Safety & Failure Detection** (TIER-1: Critical)

**Current State:** AgenticFailureDetector exists but is partially implemented

**What Needs Implementation:**
```cpp
// Enhance src/agent/agentic_failure_detector.hpp
class AgenticFailureDetector : public QObject {
    // ... existing methods ...
    
    // NEW: Execution safety gates
    bool validateActionBeforeExecution(const Action& action);
    bool isDangerousCommand(const QString& commandStr);
    bool wouldCauseDataLoss(const Action& action);
    
    // NEW: Recovery recommendations
    QString suggestRecoveryAction(const FailureInfo& failure);
};
```

**Implementation Tasks:**
- [ ] Dangerous command detection (rm -rf, format drive, etc.)
- [ ] Data loss prevention (backup before destructive ops)
- [ ] Refusal detection from agent responses
- [ ] Hallucination detection (agent making false claims)
- [ ] Infinite loop detection (actions that repeat indefinitely)
- [ ] Token limit handling (truncate/summarize context)
- [ ] Timeout handling (long-running operations)

**Critical Patterns to Detect:**
```plaintext
- "I can't", "I cannot", "I'm unable" (refusals)
- Agent making up file paths that don't exist
- Agent suggesting modifications to system files
- Circular dependencies in generated actions
- Missing tool implementations
```

---

### 4. **IDE Integration Endpoints** (TIER-2: Integration)

**What Needs Implementation:** Plugin system for IDE to call agent

```cpp
// src/agent/ide_agent_bridge.hpp (NEW)
class IDEAgentBridge : public QObject {
    Q_OBJECT
public:
    // Called by IDE UI when user selects "Magic Wand" or types in agent prompt
    QFuture<bool> executeWish(const QString& wish);
    
    // Query agent without execution (preview plan)
    QFuture<QJsonArray> planWish(const QString& wish);
    
    // Real-time progress during execution
    QFuture<QString> streamAgentExecution();
    
signals:
    void agentThinkingStarted(const QString& wish);
    void agentGeneratedPlan(const QJsonArray& plan);
    void agentExecutionProgress(const QString& action, int progress);
    void agentCompleted(bool success, const QString& summary);
    void agentError(const QString& error, bool recoverable);
    void agentNeedsUserInput(const QString& query, QStringList options);
};
```

**Integration Points:**
- [ ] Main IDE window "Magic Wand" button ⚡
- [ ] Context menu integration (right-click → "Ask Agent")
- [ ] Command palette integration (Ctrl+K → agent prompt)
- [ ] Ghost text display in editor (TriggerAgent in assembly ↔ C++ callback)
- [ ] Status bar showing agent activity

---

### 5. **Editor Agent Hooks** (TIER-2: Polish)

**Current State:** editor.asm has `TriggerAgent` and `AcceptAgent` stubs

**What Needs Implementation:**
```cpp
// src/gui/editor_agent_integration.hpp (NEW)
class EditorAgentIntegration : public QObject {
    Q_OBJECT
public:
    // Called when user presses TAB (TriggerAgent in assembly)
    void onTriggerAgent(int cursorPos, const QString& linePrefix);
    
    // Called when user presses ENTER (AcceptAgent in assembly)
    void onAcceptAgent(const QString& suggestion);
    
    // Update ghost text overlay in real-time
    void updateGhostText(const QString& suggestion, int column);
    
    // Called periodically (~300ms) to fetch new suggestions
    void refreshSuggestions();
    
signals:
    void ghostTextAvailable(const QString& text, int column);
    void ghostTextDismissed();
    void suggestionAccepted(const QString& code);
};
```

**Assembly Integration:**
- Modify editor.asm to call C++ callbacks when TAB/ENTER pressed
- Instead of stubbed procedures, invoke FFI to C++ agent invoker
- Display ghost text from agent response in real-time

---

### 6. **Self-Patching System** (TIER-2: Advanced)

**Current State:** `self_patch.hpp` exists but implementation is incomplete

**What Needs Implementation:**
```cpp
// Complete src/agent/self_patch.hpp
class SelfPatch : public QObject {
public:
    // Core: Parse existing code and add new capability
    bool addKernel(const QString& kernelName, const QString& templateType);
    bool addCpp(const QString& fileName, const QString& dependencies);
    
    // Implementation: Use AST or regex-based code generation
    // For C++: Parse existing patterns, replicate structure
    // For Assembly: Generate MASM procedures from template
    
    // Safety: Always create .backup files
    // Validation: Compile after patching, rollback on error
};
```

**Features:**
- [ ] Template-based code generation
- [ ] AST-aware C++ insertion (not regex)
- [ ] Dependency resolution
- [ ] Automatic backup creation
- [ ] Post-patch compilation validation
- [ ] Git tracking of changes

---

### 7. **Planner Expansion** (TIER-2: Domain Logic)

**Current State:** Planner::plan() has method stubs

**What Needs Implementation:**
```cpp
// Complete src/agent/planner.hpp
class Planner {
public:
    QJsonArray plan(const QString& humanWish);
    
private:
    // Domain-specific planners
    QJsonArray planQuantKernel(const QString& wish);    // ✅ impl needed
    QJsonArray planRelease(const QString& wish);        // ✅ impl needed
    QJsonArray planWebProject(const QString& wish);     // ✅ impl needed
    QJsonArray planSelfReplication(const QString& wish);// ✅ impl needed
    QJsonArray planGeneric(const QString& wish);        // ✅ impl needed
    
    // Natural language classification
    QString detectDomain(const QString& wish);
};
```

**Planning Logic Example (planQuantKernel):**
```cpp
// If wish contains "add Q8_K kernel" or "add quantization"
// Return steps: [
//   { type: "file_search", path: "src/kernels", pattern: "*q4*" },
//   { type: "file_edit", file: "CMakeLists.txt", action: "add_target" },
//   { type: "build", target: "q8_k_kernel" },
//   { type: "bench", kernel: "q8_k" }
// ]
```

---

### 8. **Testing & Validation** (TIER-3: Quality Assurance)

**What Needs Implementation:**
```cpp
// src/agent/eval_framework.hpp (NEW)
class EvalFramework {
public:
    struct TestCase {
        QString wish;
        QString expectedOutcome;
        int maxExecutionTime = 30000; // 30 seconds
        bool requiresNetwork = false;
    };
    
    // Run suite of wishes, measure success rate
    QJsonObject runEvaluation(const QVector<TestCase>& tests);
    
    // Record metrics for agent improvement
    void logExecution(const QString& wish, bool success, int timeMs, const QString& error);
};
```

**Evaluation Scenarios:**
- [ ] "Add a new C++ module" → File created, compiled, linked
- [ ] "Find all TODO comments in project" → Correct count returned
- [ ] "Optimize the search widget" → Code changes don't break tests
- [ ] "Generate test cases for X" → Valid unit tests created
- [ ] "Add a new command-line flag" → Works end-to-end

---

## Implementation Priority & Dependencies

```
PHASE 1 (Foundation - Weeks 1-2):
  ├─ ModelInvoker (wish → LLM → plan)
  ├─ ActionExecutor (execute individual actions)
  └─ Enhanced FailureDetector (catch problems early)

PHASE 2 (Integration - Weeks 3-4):
  ├─ IDEAgentBridge (UI ↔ agent communication)
  ├─ Planner domain logic (quantize, release, web)
  └─ SelfPatch implementation (code generation)

PHASE 3 (Refinement - Week 5):
  ├─ EditorAgentIntegration (TAB/ENTER hooks)
  ├─ EvalFramework (validation & metrics)
  └─ Assembly FFI callbacks (asm ↔ C++ integration)

PHASE 4 (Production Hardening - Week 6):
  ├─ Exhaustive safety testing
  ├─ Error recovery workflows
  ├─ Performance profiling
  └─ Documentation & examples
```

## File Creation Checklist

**New Files to Create (8 Total):**
```plaintext
[ ] src/agent/model_invoker.hpp          (LLM integration interface)
[ ] src/agent/model_invoker.cpp          (Ollama/API client)
[ ] src/agent/action_executor.hpp        (Action dispatch & execution)
[ ] src/agent/action_executor.cpp        (Implementation)
[ ] src/agent/ide_agent_bridge.hpp       (IDE plugin interface)
[ ] src/agent/ide_agent_bridge.cpp       (Implementation)
[ ] src/gui/editor_agent_integration.hpp (Editor TAB/ENTER hooks)
[ ] src/gui/editor_agent_integration.cpp (Ghost text management)
```

**Files to Complete/Enhance (6 Total):**
```plaintext
[X] src/agent/agentic_failure_detector.hpp    (add safety validators)
[X] src/agent/agentic_failure_detector.cpp    (implement detection)
[ ] src/agent/planner.hpp                    (domain logic)
[ ] src/agent/planner.cpp                    (planning implementation)
[ ] src/agent/self_patch.hpp                 (code generation)
[ ] src/agent/self_patch.cpp                 (implementation)
[ ] src/agent/eval_framework.hpp             (testing framework)
[ ] src/agent/eval_framework.cpp             (evaluation runner)
```

## Example: "Add Q8_K Kernel" End-to-End Flow

```
User Input: "Add Q8_K quantization kernel"
    ↓
IDEAgentBridge::executeWish()
    ↓
ModelInvoker::invokeAgent()
    ├─ Query: "The user wants to add Q8_K quantization kernel to the IDE.
    │          Available tools: search, file_edit, build, run_tests.
    │          Current project: RawrXD GGUF Model Server.
    │          Generate structured action plan."
    ├─ Ollama response parsed
    └─ Returns JSON: [
        { type: "search_files", path: "src/kernels", pattern: "*.cpp" },
        { type: "file_edit", action: "insert_after", marker: "Kernel Registry", code: "..." },
        { type: "edit_cmake", target: "q8_k_kernel" },
        { type: "build", target: "q8_k_kernel" },
        { type: "run_bench", kernel: "q8_k", model: "mistral-7b" }
       ]
    ↓
ActionExecutor::executePlan()
    ├─ Step 1: MultiFileSearchWidget finds kernel patterns
    ├─ Step 2: SelfPatch::addKernel() generates MASM code
    ├─ Step 3: CMakeLists.txt updated
    ├─ Step 4: Build compiles successfully
    ├─ Step 5: Benchmark runs, results logged
    └─ On Error → FailureDetector suggests rollback
    ↓
IDEAgentBridge emits agentCompleted(true, "Q8_K kernel added successfully...")
    ↓
GUI updates with green checkmark, shows benchmark results
```

## Critical Success Factors

1. **Model Quality**: LLM must understand code structure (use RAG with codebase embeddings)
2. **Safety First**: Every action must be validated before execution
3. **Observability**: Log every decision point for debugging
4. **Graceful Degradation**: If agent fails, fallback to UI prompts
5. **Testing Coverage**: 100+ test cases before "autonomous" status
6. **Human Oversight**: Always show plan before executing

## Next Immediate Action

Start with `model_invoker.hpp/cpp`:
- Design Ollama client (leverage existing GGUF server infra)
- Implement prompt templates for wish→plan transformation
- Create response parser (extract JSON from LLM output)
- Write unit tests for LLM parsing

