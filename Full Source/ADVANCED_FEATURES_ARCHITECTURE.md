# Advanced RawrXD Features - Architecture & Implementation Guide

**Status**: 🟢 **DESIGN PHASE COMPLETE** | **Ready for Implementation**  
**Date**: December 5, 2025  
**Architecture Tier**: **Enterprise-Grade AI-Native IDE**

---

## 🎯 Feature Overview

Four transformational features for next-generation AI development:

1. **Multi-Agent Orchestration + LLM Router + Plan-Mode + Voice**
2. **Inline Diff/Autocomplete Model + Yolo-Mode**
3. **AI-Native Git Diff & Merge UI**
4. **Sandboxed Terminal & Zero-Retention Option**

---

## 1️⃣ MULTI-AGENT ORCHESTRATION WITH LLM ROUTER + PLAN-MODE + VOICE

### 1.1 Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    USER INTERFACE LAYER                      │
│  ┌─────────────────┬──────────────────────┬────────────────┐ │
│  │  Voice Input    │  Plan Mode UI        │  Agent Monitor │ │
│  │  (STT Module)   │  (Checklist View)    │  (Real-time)   │ │
│  └─────────────────┴──────────────────────┴────────────────┘ │
└────────────────────────────┬─────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────┐
│                   ORCHESTRATION LAYER                         │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │  LLM Router (Multi-Model Selection & Load Balancing)     │ │
│  │  ├─ Model Score Calculation (capability matrix)          │ │
│  │  ├─ Latency Predictor (response time estimation)         │ │
│  │  ├─ Cost Optimizer (token usage minimization)            │ │
│  │  ├─ Confidence Scorer (output quality assessment)        │ │
│  │  └─ Fallback Handler (automatic recovery)                │ │
│  └──────────────────────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │  Agent Coordinator (Multi-Agent Task Distribution)       │ │
│  │  ├─ Agent Pool Manager (lifecycle management)            │ │
│  │  ├─ Task Dependency Graph (DAG execution)                │ │
│  │  ├─ Context Sharing (shared memory between agents)       │ │
│  │  ├─ Conflict Resolution (concurrent access control)      │ │
│  │  └─ Progress Tracking (real-time execution visibility)   │ │
│  └──────────────────────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │  Voice Processing Pipeline                               │ │
│  │  ├─ Speech-to-Text (Whisper API / local)                 │ │
│  │  ├─ Intent Recognition (NLU via LLM router)              │ │
│  │  ├─ Context Injection (conversation history)             │ │
│  │  ├─ Plan Generation (runSubagent → plan steps)           │ │
│  │  └─ Text-to-Speech (TTS feedback)                        │ │
│  └──────────────────────────────────────────────────────────┘ │
└────────────────────────────┬─────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────┐
│                    AGENT EXECUTION LAYER                      │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │  Agent Pool: [Research] [Coder] [Reviewer] [Optimizer]   │ │
│  │  ├─ Research Agent: Code search, documentation gathering │ │
│  │  ├─ Coder Agent: Code generation, refactoring            │ │
│  │  ├─ Reviewer Agent: Code review, testing                 │ │
│  │  ├─ Optimizer Agent: Performance, security analysis      │ │
│  │  └─ Deployer Agent: Release, versioning, publishing      │ │
│  └──────────────────────────────────────────────────────────┘ │
└────────────────────────────┬─────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────────┐
│              BACKEND & INFRASTRUCTURE LAYER                   │
│  ├─ GGUF Model Server (localhost:11434)                       │
│  ├─ Plan Storage (SQLite indexed plans)                       │
│  ├─ Telemetry & Metrics (agent performance)                   │
│  └─ Service Registry (available agents & models)              │
└──────────────────────────────────────────────────────────────┘
```

### 1.2 LLM Router - Multi-Model Orchestration

**File**: `src/orchestration/llm_router.hpp`

```cpp
#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <memory>

// Model capability scores (0-100)
struct ModelCapabilities {
    int reasoning = 0;           // Logic & analysis
    int coding = 0;              // Code generation
    int planning = 0;            // Task planning
    int creativity = 0;          // Novel solutions
    int speed = 0;               // Response latency (inverted)
    int costEfficiency = 0;      // Token cost (inverted)
};

// Model registry entry
struct ModelInfo {
    QString id;                  // "gpt-4", "claude", "llama70b", etc.
    QString provider;            // "openai", "anthropic", "ollama"
    int contextWindow = 8192;
    double avgTokenCost = 0.0;
    double avgLatencyMs = 0.0;
    ModelCapabilities capabilities;
    bool available = true;
};

// Router decision result
struct RoutingDecision {
    QString selectedModelId;
    int confidenceScore = 0;     // 0-100
    QString routingReason;
    QStringList alternativeModels;
    ModelInfo selectedInfo;
};

class LLMRouter : public QObject {
    Q_OBJECT

public:
    LLMRouter();
    
    // Register available models
    void registerModel(const ModelInfo& model);
    
    // Route a task to optimal model(s)
    RoutingDecision route(
        const QString& taskDescription,
        const QString& preferredCapability,  // "reasoning", "coding", "speed"
        int maxCostTokens = 0  // 0 = unlimited
    );
    
    // Route with multi-model ensemble
    QStringList routeEnsemble(
        const QString& taskDescription,
        int numModels = 3,      // How many models to use in parallel
        const QString& consensusMethod = "voting"  // "voting", "weighted", "unanimous"
    );
    
    // Get model health status
    QJsonObject getModelStatus(const QString& modelId);
    
    // Update model performance metrics
    void recordPerformance(
        const QString& modelId,
        int taskDurationMs,
        int tokensUsed,
        double qualityScore  // 0.0-1.0
    );
    
    // Fallback handling
    void handleModelFailure(const QString& modelId, const QString& error);
    RoutingDecision getFallbackModel(const QString& failedModelId);

signals:
    void modelRegistered(const QString& modelId);
    void routingDecisionMade(const RoutingDecision& decision);
    void modelHealthChanged(const QString& modelId, bool healthy);

private:
    QMap<QString, ModelInfo> m_models;
    QMap<QString, QJsonObject> m_performanceMetrics;
    
    // Scoring functions
    int calculateTaskRelevanceScore(const ModelInfo& model, const QString& capability);
    int calculateCostEfficiencyScore(const ModelInfo& model, int maxTokens);
    int calculateLatencyScore(const ModelInfo& model);
    
    // Load balancing
    QString selectFromCandidates(const QStringList& candidates);
};
```

**Implementation File**: `src/orchestration/llm_router.cpp` (250 lines)

### 1.3 Agent Coordinator - Multi-Agent Orchestration

**File**: `src/orchestration/agent_coordinator.hpp`

```cpp
#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <memory>
#include <atomic>

// Agent types and specializations
enum class AgentType {
    RESEARCH,      // Information gathering
    CODER,         // Code generation
    REVIEWER,      // Quality assurance
    OPTIMIZER,     // Performance/security
    DEPLOYER,      // Release management
    DEBUGGER,      // Error resolution
    ARCHITECT      // System design
};

// Task definition in DAG
struct AgentTask {
    QString id;
    QString description;
    AgentType preferredAgent;
    QStringList dependencies;    // IDs of tasks that must complete first
    QJsonObject context;         // Task-specific data
    int priority = 5;            // 1-10, higher = more urgent
    int timeoutMs = 300000;      // 5 minutes default
};

// Execution state
enum class ExecutionState {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    RETRYING,
    CANCELLED
};

// Task result
struct TaskResult {
    QString taskId;
    ExecutionState state;
    QJsonObject output;
    QString error;
    int durationMs = 0;
    QString executingAgentId;
};

class AgentCoordinator : public QObject {
    Q_OBJECT

public:
    AgentCoordinator();
    
    // Create agent instances
    void createAgent(AgentType type, int count = 1);
    
    // Submit task DAG (directed acyclic graph)
    QString submitTaskDAG(const QJsonArray& tasks);
    
    // Execute tasks respecting dependencies
    void executeDAG(const QString& dagId);
    
    // Get real-time progress
    QJsonObject getDAGProgress(const QString& dagId);
    
    // Inter-agent communication (shared context)
    void setSharedContext(const QString& key, const QJsonValue& value);
    QJsonValue getSharedContext(const QString& key);
    
    // Conflict resolution (concurrent access control)
    bool acquireResource(const QString& resourceId, const QString& agentId);
    void releaseResource(const QString& resourceId, const QString& agentId);
    
    // Agent health monitoring
    QJsonObject getAgentStatus(const QString& agentId);
    void rebalanceLoadIfNeeded();
    
    // Cancellation & rollback
    void cancelDAG(const QString& dagId);
    void rollbackDAG(const QString& dagId);

signals:
    void taskCompleted(const TaskResult& result);
    void taskFailed(const TaskResult& result);
    void dagCompleted(const QString& dagId);
    void dagFailed(const QString& dagId, const QString& error);
    void progressUpdated(const QString& dagId, int percent);

private:
    QMap<QString, AgentTask> m_tasks;
    QMap<QString, TaskResult> m_results;
    QMap<QString, std::atomic<ExecutionState>> m_taskStates;
    QMap<QString, QJsonValue> m_sharedContext;
    QMap<QString, QString> m_resourceLocks;  // resourceId -> agentId
    
    void executeTask(const AgentTask& task);
    bool checkDependenciesComplete(const AgentTask& task);
    QStringList getReadyTasks(const QString& dagId);
};
```

### 1.4 Voice Processing Pipeline

**File**: `src/orchestration/voice_processor.hpp`

```cpp
#pragma once
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <memory>

enum class STTProvider {
    OPENAI_WHISPER,
    LOCAL_WHISPER,
    GOOGLE_SPEECH,
    AZURE_SPEECH
};

enum class TTSProvider {
    GOOGLE_TTS,
    AZURE_TTS,
    ELEVENLABS,
    LOCAL_ESPEAK
};

class VoiceProcessor : public QObject {
    Q_OBJECT

public:
    VoiceProcessor();
    
    // Speech-to-Text
    void startListening();
    void stopListening();
    QString transcribeAudio(const QByteArray& audioData);
    
    // Intent recognition + plan generation
    void processVoiceCommand(const QString& transcription);
    
    // Text-to-Speech feedback
    void speak(const QString& text, TTSProvider provider = TTSProvider::GOOGLE_TTS);
    
    // Configure providers
    void setSTTProvider(STTProvider provider);
    void setTTSProvider(TTSProvider provider);

signals:
    void listeningStarted();
    void transcriptionReceived(const QString& text);
    void intentDetected(const QString& intent);
    void planGenerated(const QJsonArray& plan);
    void feedbackSpoken();
    void error(const QString& message);
};
```

### 1.5 UI Layer - Plan Mode + Voice Integration

**File**: `src/ui/plan_mode_ui.hpp`

```cpp
// Plan checklist display
class PlanChecklist : public QWidget {
public:
    void displayPlan(const QJsonArray& planSteps);
    void updateStepProgress(int stepIndex, int percent);
    void markStepComplete(int stepIndex);
    void markStepFailed(int stepIndex, const QString& error);
    
    // Voice control integration
    void enableVoiceControl(bool enable);
    void onVoiceCommand(const QString& command);
};

// Agent execution monitor
class AgentExecutionMonitor : public QWidget {
public:
    void displayAgentStates(const QJsonObject& states);
    void displaySharedContext(const QJsonObject& context);
    void displayResourceLocks(const QMap<QString, QString>& locks);
};

// LLM Router statistics
class RouterStatsPanel : public QWidget {
public:
    void displayModelScores(const QMap<QString, int>& scores);
    void displayRoutingHistory(const QJsonArray& decisions);
    void displayEnsembleResults(const QJsonObject& results);
};
```

---

## 2️⃣ INLINE DIFF/AUTOCOMPLETE MODEL + YOLO-MODE

### 2.1 Architecture

```
┌──────────────────────────────────────────────────────────┐
│              INLINE PREDICTION ENGINE                    │
│  ┌────────────────────────────────────────────────────┐  │
│  │ Live Diff Predictor (keystroke-level)              │  │
│  │ ├─ Token prediction (what's likely next)           │  │
│  │ ├─ Diff calculation (before/after)                 │  │
│  │ ├─ Ghost text rendering                            │  │
│  │ └─ Accept/Reject keybindings (Tab/Esc)             │  │
│  └────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────┐  │
│  │ Autocomplete Model (context-aware)                 │  │
│  │ ├─ Function signature completion                   │  │
│  │ ├─ Variable name suggestions                       │  │
│  │ ├─ Import statement generation                     │  │
│  │ ├─ Error recovery suggestions                      │  │
│  │ └─ Multi-line code block insertion                 │  │
│  └────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────┐  │
│  │ YOLO Mode (Fast & Loose)                           │  │
│  │ ├─ Aggressive caching (skip validation)            │  │
│  │ ├─ Lookahead inference (batch predictions)         │  │
│  │ ├─ Quantized models (faster inference)             │  │
│  │ ├─ No safety checks (speed over safety)            │  │
│  │ └─ Immediate accept by default                     │  │
│  └────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

**File**: `src/editor/inline_predictor.hpp`

```cpp
#pragma once
#include <QString>
#include <QJsonArray>
#include <QTimer>
#include <memory>

enum class PredictionMode {
    CONSERVATIVE,    // Wait for model confidence > 80%
    BALANCED,        // Medium latency/quality trade-off
    YOLO             // Fast & loose (no validation)
};

struct InlinePrediction {
    QString text;
    float confidence = 0.0;
    int charOffset = 0;
    QJsonArray alternatives;
    int msLatency = 0;
};

class InlinePredictor : public QObject {
    Q_OBJECT

public:
    InlinePredictor();
    
    // Set prediction mode
    void setMode(PredictionMode mode);
    
    // Called on every keystroke
    void onTextEdited(const QString& currentLine, int cursorPos);
    
    // Get next prediction
    InlinePrediction predict();
    
    // Accept/reject suggestion
    void acceptPrediction();
    void rejectPrediction();
    
    // Train on user acceptance patterns
    void recordAcceptance(const InlinePrediction& pred, bool accepted);
    
    // YOLO mode: full-speed batch predictions
    QJsonArray predictBatch(int lookAheadChars = 20);

signals:
    void predictionReady(const InlinePrediction& prediction);
    void ghostTextUpdated(const QString& ghostText);
    void modeChanged(PredictionMode newMode);

private:
    PredictionMode m_mode = PredictionMode::BALANCED;
    QString m_context;
    int m_cursorPos = 0;
    QTimer m_predictionThrottle;
    std::unique_ptr<class LocalLLMCache> m_cache;
    
    // YOLO mode features
    bool m_yoloAgggressiveCache = false;
    bool m_yoloSkipValidation = false;
    bool m_yoloImmedateAccept = false;
};
```

### 2.2 Ghost Text Rendering

**File**: `src/editor/ghost_text_renderer.hpp`

```cpp
class GhostTextRenderer : public QObject {
    Q_OBJECT

public:
    // Render semi-transparent suggestion text
    void renderGhostText(
        const QString& ghostText,
        int startPos,
        QColor color = QColor(150, 150, 150, 100)
    );
    
    // Interactive diff view
    void showDiffPreview(
        const QString& originalText,
        const QString& suggestedText
    );
    
    // Keystroke handling
    void onTabPressed();        // Accept suggestion
    void onEscapePressed();     // Reject suggestion
    void onArrowKeyPressed();   // Navigate alternatives
};
```

---

## 3️⃣ AI-NATIVE GIT DIFF & MERGE UI

### 3.1 Architecture

```
┌──────────────────────────────────────────────────────┐
│         AI-NATIVE GIT DIFF & MERGE INTERFACE        │
│  ┌──────────────────────────────────────────────────┐│
│  │ Semantic Diff Analysis                           ││
│  │ ├─ AST-based comparison (structure aware)        ││
│  │ ├─ Functional impact analysis                    ││
│  │ ├─ Breaking change detection                     ││
│  │ ├─ Code smell identification                     ││
│  │ └─ Auto-categorization (refactor/fix/feature)    ││
│  └──────────────────────────────────────────────────┘│
│  ┌──────────────────────────────────────────────────┐│
│  │ AI-Powered Merge Resolution                      ││
│  │ ├─ Conflict detection (semantic + syntactic)     ││
│  │ ├─ Auto-resolution strategies                    ││
│  │ ├─ Three-way merge with AI reasoning             ││
│  │ ├─ Human approval UI for conflicts               ││
│  │ └─ Merge validation (build/test)                 ││
│  └──────────────────────────────────────────────────┘│
│  ┌──────────────────────────────────────────────────┐│
│  │ Visual Diff UI (Rich Presentation)               ││
│  │ ├─ Side-by-side + inline view modes              ││
│  │ ├─ Color-coded change types                      ││
│  │ ├─ AI-generated commit message suggestions       ││
│  │ ├─ Impact badge (small/medium/large)             ││
│  │ └─ Review sentiment (important/risky/refactor)   ││
│  └──────────────────────────────────────────────────┘│
└──────────────────────────────────────────────────────┘
```

**File**: `src/git/semantic_diff_analyzer.hpp`

```cpp
#pragma once
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

enum class ChangeType {
    REFACTOR,
    BUGFIX,
    FEATURE,
    OPTIMIZATION,
    SECURITY,
    DOCUMENTATION,
    STYLE,
    DEPENDENCY
};

enum class ImpactLevel {
    SMALL,      // < 10 lines, isolated change
    MEDIUM,     // 10-50 lines, affects multiple functions
    LARGE       // > 50 lines, changes core logic
};

enum class RiskLevel {
    LOW,
    MEDIUM,
    HIGH,       // Breaking changes, security implications
    CRITICAL    // Affects multiple modules, potential crash
};

struct SemanticDiff {
    ChangeType changeType;
    ImpactLevel impact;
    RiskLevel risk;
    
    QString fileName;
    int linesAdded = 0;
    int linesRemoved = 0;
    int functionsModified = 0;
    
    QStringList breakingChanges;     // List of incompatibilities
    QStringList codeSmells;          // Detected issues
    
    QString suggestedCommitMessage;
    QString suggestedReviewFocus;
};

class SemanticDiffAnalyzer : public QObject {
    Q_OBJECT

public:
    // Analyze single file diff
    SemanticDiff analyzeDiff(
        const QString& fileName,
        const QString& originalContent,
        const QString& newContent
    );
    
    // Analyze entire PR
    QJsonArray analyzePullRequest(
        const QString& baseBranch,
        const QString& featureBranch
    );
    
    // Detect and categorize conflicts
    QJsonArray detectConflicts(
        const QString& baseContent,
        const QString& branchAContent,
        const QString& branchBContent
    );

signals:
    void diffAnalyzed(const SemanticDiff& diff);
    void prAnalyzed(const QJsonArray& diffs);
    void riskDetected(const QString& message);
};
```

### 3.2 Merge Resolution Engine

**File**: `src/git/ai_merge_resolver.hpp`

```cpp
class AIMergeResolver : public QObject {
    Q_OBJECT

public:
    // Three-way merge with AI assistance
    struct MergeResult {
        QString resolvedContent;
        QJsonArray manualConflicts;  // Conflicts requiring human decision
        float autoResolveConfidence = 0.0;
        QString strategy;  // "ours", "theirs", "combined", "custom"
    };
    
    MergeResult mergeWithAI(
        const QString& baseContent,
        const QString& oursContent,
        const QString& theirsContent,
        bool autoResolveSmallConflicts = true
    );
    
    // Get AI-suggested resolution for specific conflict
    struct ConflictResolution {
        int startLine, endLine;
        QString suggestedResolution;
        float confidence = 0.0;
        QString reasoning;
        QStringList alternatives;
    };
    
    ConflictResolution resolveConflict(
        const QString& conflictMarker,
        const QString& context
    );
    
    // Validate merged code
    bool validateMerge(
        const QString& mergedContent,
        const QString& language
    );

signals:
    void mergeResolved(const MergeResult& result);
    void conflictDetected(int lineNum, const QString& content);
    void validationComplete(bool valid, const QString& error);
};
```

### 3.3 Rich UI Components

**File**: `src/ui/semantic_diff_widget.hpp`

```cpp
class SemanticDiffWidget : public QWidget {
    Q_OBJECT

public:
    // Display modes
    enum ViewMode { SIDE_BY_SIDE, INLINE, TREE };
    
    void setDiff(const SemanticDiff& diff);
    void setViewMode(ViewMode mode);
    
    // Change type indicators
    void renderChangeTypeBadge(ChangeType type);
    void renderRiskBadge(RiskLevel risk);
    void renderImpactBadge(ImpactLevel impact);
    
    // AI suggestions
    void showCommitMessageSuggestion(const QString& message);
    void showReviewFocusPoints(const QStringList& points);
    
    // Interactive controls
    void enableConflictResolution(bool enable);
    void onResolveConflictClicked(const QString& resolution);
};
```

---

## 4️⃣ SANDBOXED TERMINAL + ZERO-RETENTION OPTION

### 4.1 Architecture

```
┌──────────────────────────────────────────────────────┐
│      SANDBOXED TERMINAL ENVIRONMENT                  │
│  ┌──────────────────────────────────────────────────┐│
│  │ Process Isolation & Containment                   ││
│  │ ├─ Process namespace (Linux)                      ││
│  │ ├─ Windows Job Object (Windows)                   ││
│  │ ├─ Resource limits (CPU, RAM, I/O)                ││
│  │ ├─ File system jailing (chroot / isolated paths)  ││
│  │ └─ Network blocking (no external access)          ││
│  └──────────────────────────────────────────────────┘│
│  ┌──────────────────────────────────────────────────┐│
│  │ Command Filtering & Validation                    ││
│  │ ├─ Blacklist dangerous commands (rm -rf, format)  ││
│  │ ├─ Whitelist approved commands                    ││
│  │ ├─ Input sanitization (injection prevention)      ││
│  │ ├─ Permission checking (read/write/execute)       ││
│  │ └─ Audit logging (all commands logged)            ││
│  └──────────────────────────────────────────────────┘│
│  ┌──────────────────────────────────────────────────┐│
│  │ Zero-Retention Mode                               ││
│  │ ├─ Command history disabled                       ││
│  │ ├─ No output log files                            ││
│  │ ├─ RAM-only buffers (no disk persistence)         ││
│  │ ├─ Automatic cleanup on exit                      ││
│  │ ├─ Shred temp files (secure deletion)             ││
│  │ └─ Environment variable sanitization              ││
│  └──────────────────────────────────────────────────┘│
└──────────────────────────────────────────────────────┘
```

**File**: `src/terminal/sandboxed_terminal.hpp`

```cpp
#pragma once
#include <QString>
#include <QJsonObject>
#include <QProcess>
#include <memory>

enum class SandboxLevel {
    PERMISSIVE,      // Minimal restrictions
    STANDARD,        // Balanced security
    STRICT,          // High restrictions (limited file access)
    MAXIMUM          // Maximum isolation (read-only filesystem)
};

enum class RetentionMode {
    FULL,            // Store everything (history, logs)
    MINIMAL,         // Only current session
    ZERO             // Delete everything on exit
};

struct SandboxConfig {
    SandboxLevel level = SandboxLevel::STANDARD;
    RetentionMode retentionMode = RetentionMode::FULL;
    
    // Resource limits
    int maxCpuPercent = 80;
    int maxMemoryMB = 512;
    int maxOpenFiles = 100;
    int maxProcesses = 5;
    
    // File system restrictions
    QStringList accessiblePaths;  // Directories process can read/write
    QStringList readOnlyPaths;    // Directories process can only read
    QStringList blockedPaths;     // Directories completely forbidden
    
    // Command restrictions
    QStringList blacklistedCommands;
    QStringList whitelistedCommands;  // If set, only these allowed
};

class SandboxedTerminal : public QObject {
    Q_OBJECT

public:
    SandboxedTerminal(const SandboxConfig& config);
    
    // Execute command in sandbox
    void executeCommand(
        const QString& command,
        const QString& workingDirectory = ""
    );
    
    // Get available commands (based on sandbox restrictions)
    QStringList getAvailableCommands();
    
    // Check if command is allowed
    bool isCommandAllowed(const QString& command);
    
    // Configure sandbox
    void setSandboxLevel(SandboxLevel level);
    void setRetentionMode(RetentionMode mode);
    
    // Get resource usage
    QJsonObject getResourceUsage();
    
    // Cleanup on exit (zero-retention)
    void secureCleanup();

signals:
    void commandExecuted(const QString& command);
    void outputReceived(const QString& output);
    void errorOccurred(const QString& error);
    void resourceLimitReached(const QString& resource);
    void commandBlocked(const QString& command, const QString& reason);

private:
    SandboxConfig m_config;
    std::unique_ptr<QProcess> m_process;
    QStringList m_commandHistory;  // RAM-only buffer
    
    // Enforcement
    bool validateCommand(const QString& command);
    bool enforceResourceLimits();
    void sanitizeEnvironment();
    void cleanupHistoryOnExit();
};
```

### 4.2 Zero-Retention Implementation

**File**: `src/terminal/zero_retention_manager.hpp`

```cpp
class ZeroRetentionManager : public QObject {
    Q_OBJECT

public:
    // Enable zero-retention mode
    void enableZeroRetention();
    void disableZeroRetention();
    bool isZeroRetentionEnabled() const;
    
    // Manual cleanup
    void clearCommandHistory();
    void clearOutputBuffers();
    void clearTempFiles();
    void shredAllData();  // Secure deletion with multiple passes
    
    // Automatic cleanup on session end
    void scheduleCleanupOnExit();
    
    // Verify no data remains
    bool verifyNoDataRemains();

signals:
    void dataCleared(const QString& type, int bytesCleared);
    void cleanupComplete();
};
```

---

## 📋 Implementation Roadmap

### Phase 1: Foundation (Week 1-2)
- [ ] LLM Router infrastructure
- [ ] Model registry & scoring system
- [ ] Agent Coordinator base

### Phase 2: Multi-Agent System (Week 3-4)
- [ ] Agent pool management
- [ ] Task DAG execution
- [ ] Context sharing

### Phase 3: Voice Integration (Week 5-6)
- [ ] STT pipeline
- [ ] Intent recognition
- [ ] TTS feedback

### Phase 4: Inline Prediction (Week 7-8)
- [ ] Token predictor
- [ ] Ghost text rendering
- [ ] YOLO mode implementation

### Phase 5: Git Integration (Week 9-10)
- [ ] Semantic diff analysis
- [ ] AI merge resolver
- [ ] Rich diff UI

### Phase 6: Terminal Sandboxing (Week 11-12)
- [ ] Process isolation
- [ ] Command filtering
- [ ] Zero-retention engine

---

## 🏗️ Integration Points

### With Existing Hot-Patching System
```cpp
// LLM Router can select optimal model for hallucination detection
RoutingDecision decision = router.route(
    "detect hallucinations in model output",
    "reasoning",  // Use highly-reasoning model
    maxCostTokens
);

// Agent Coordinator can orchestrate hot-patch deployment
QString dagId = coordinator.submitTaskDAG({
    {"task": "detect_hallucination"},
    {"task": "generate_correction"},
    {"task": "apply_patch"},
    {"task": "verify_fix"}
});
```

### With Existing Plan Mode
```cpp
// Voice input → Plan generation
voiceProcessor.processVoiceCommand("Create a database migration");
// Output: triggers existing planModeHandler with generated plan

// AI routing for plan execution
RoutingDecision decision = router.route(
    "Execute database migration plan",
    "planning"
);
```

---

## 🔒 Security Considerations

1. **LLM Router**: Monitor model outputs for injection attacks
2. **Agent Coordinator**: Validate inter-agent messages
3. **Voice Processing**: Ensure STT transcripts don't leak sensitive data
4. **Inline Predictor**: Don't suggest credentials or secrets
5. **Git Integration**: Validate merge results before commit
6. **Sandboxed Terminal**: Enforce all restrictions even with root

---

## 📊 Performance Targets

| Feature | Latency | Throughput |
|---------|---------|-----------|
| LLM Router decision | < 100ms | 1000 routes/sec |
| Agent task dispatch | < 50ms | 10000 tasks/sec |
| Voice STT | < 2s | Real-time |
| Inline prediction | < 200ms | Per keystroke |
| Semantic diff | < 500ms | Per file |
| Terminal command | < 1s | Real-time |

---

**Status**: 🟢 **Ready for Implementation**  
**Next Step**: Begin with LLM Router foundation in Phase 1

