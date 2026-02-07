# Integration & Deployment Guide

**Date**: December 5, 2025  
**Status**: 🟢 **Complete Integration Plan**

---

## 📁 Project Structure After Integration

```
RawrXD-ModelLoader/
├── src/
│   ├── orchestration/                    [NEW]
│   │   ├── llm_router.hpp
│   │   ├── llm_router.cpp
│   │   ├── agent_coordinator.hpp
│   │   ├── agent_coordinator.cpp
│   │   ├── voice_processor.hpp
│   │   ├── voice_processor.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── editor/                           [ENHANCED]
│   │   ├── inline_predictor.hpp
│   │   ├── inline_predictor.cpp
│   │   ├── ghost_text_renderer.hpp
│   │   ├── ghost_text_renderer.cpp
│   │   └── (existing files)
│   │
│   ├── git/                              [NEW]
│   │   ├── semantic_diff_analyzer.hpp
│   │   ├── semantic_diff_analyzer.cpp
│   │   ├── ai_merge_resolver.hpp
│   │   ├── ai_merge_resolver.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── terminal/                         [NEW]
│   │   ├── sandboxed_terminal.hpp
│   │   ├── sandboxed_terminal.cpp
│   │   ├── zero_retention_manager.hpp
│   │   ├── zero_retention_manager.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── ui/                               [ENHANCED]
│   │   ├── plan_mode_ui.hpp
│   │   ├── plan_checklist_widget.cpp
│   │   ├── agent_execution_monitor.cpp
│   │   ├── router_stats_panel.cpp
│   │   ├── semantic_diff_widget.cpp
│   │   └── (existing files)
│   │
│   └── CMakeLists.txt                    [UPDATED]
│
└── CMakeLists.txt                        [UPDATED]
```

---

## 🔧 CMakeLists.txt Integration

### Root CMakeLists.txt - Add New Subdirectories

```cmake
# Add to existing CMakeLists.txt
add_subdirectory(src/orchestration)
add_subdirectory(src/git)
add_subdirectory(src/terminal)

# Update main target dependencies
target_link_libraries(RawrXD-Agent
    PRIVATE
        RawrXDOrchestration
        RawrXDGit
        RawrXDTerminal
)
```

### src/orchestration/CMakeLists.txt (NEW)

```cmake
cmake_minimum_required(VERSION 3.20)
project(RawrXDOrchestration)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Network Sql REQUIRED)

add_library(RawrXDOrchestration STATIC
    llm_router.hpp
    llm_router.cpp
    agent_coordinator.hpp
    agent_coordinator.cpp
    voice_processor.hpp
    voice_processor.cpp
)

target_link_libraries(RawrXDOrchestration
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Sql
)

target_include_directories(RawrXDOrchestration
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
```

### src/git/CMakeLists.txt (NEW)

```cmake
cmake_minimum_required(VERSION 3.20)
project(RawrXDGit)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui REQUIRED)

add_library(RawrXDGit STATIC
    semantic_diff_analyzer.hpp
    semantic_diff_analyzer.cpp
    ai_merge_resolver.hpp
    ai_merge_resolver.cpp
)

target_link_libraries(RawrXDGit
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

target_include_directories(RawrXDGit
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
```

### src/terminal/CMakeLists.txt (NEW)

```cmake
cmake_minimum_required(VERSION 3.20)
project(RawrXDTerminal)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui REQUIRED)

add_library(RawrXDTerminal STATIC
    sandboxed_terminal.hpp
    sandboxed_terminal.cpp
    zero_retention_manager.hpp
    zero_retention_manager.cpp
)

target_link_libraries(RawrXDTerminal
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

target_include_directories(RawrXDTerminal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(RawrXDTerminal PRIVATE ntdll)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(RawrXDTerminal PRIVATE seccomp)
endif()
```

---

## 🔗 Integration Points with Existing Code

### 1. Connect to Hot-Patching System

**File**: `src/agent/ide_agent_bridge_hot_patching_integration.cpp`

```cpp
// Add to existing IDE agent bridge
#include "orchestration/llm_router.hpp"
#include "orchestration/agent_coordinator.hpp"

class IDEAgentBridgeWithOrchestration : public IDEAgentBridge {
private:
    std::unique_ptr<LLMRouter> m_llmRouter;
    std::unique_ptr<AgentCoordinator> m_coordinator;
    
    void initializeOrchestration() {
        m_llmRouter = std::make_unique<LLMRouter>();
        m_coordinator = std::make_unique<AgentCoordinator>();
        
        // Register available models
        ModelInfo gpt4{
            .id = "gpt-4",
            .provider = "openai",
            .endpoint = "https://api.openai.com/v1/chat/completions",
            .contextWindow = 128000,
            .capabilities = {.reasoning=95, .coding=90, .planning=85}
        };
        m_llmRouter->registerModel(gpt4);
        
        // Register local GGUF models
        ModelInfo llama70b{
            .id = "llama-70b",
            .provider = "ollama",
            .endpoint = "http://localhost:11434",
            .capabilities = {.reasoning=80, .coding=85, .planning=80}
        };
        m_llmRouter->registerModel(llama70b);
    }
    
    // When detecting hallucinations, use router to select best model
    void detectHallucinationWithAI(const QString& modelOutput) {
        RoutingDecision decision = m_llmRouter->route(
            "Detect hallucinations in: " + modelOutput,
            "reasoning",  // Use high-reasoning model
            5000  // Max 5000 tokens
        );
        
        qDebug() << "Selected model for hallucination detection:" << decision.selectedModelId;
        // ... call detection with selected model
    }
};
```

### 2. Connect to Plan Mode

**File**: `src/qtapp/plan_mode_handler.hpp`

```cpp
// Integrate voice input with plan mode
#include "orchestration/voice_processor.hpp"

class EnhancedPlanModeHandler : public PlanModeHandler {
private:
    std::unique_ptr<VoiceProcessor> m_voiceProcessor;
    
    void setupVoiceIntegration() {
        m_voiceProcessor = std::make_unique<VoiceProcessor>();
        
        connect(m_voiceProcessor.get(), &VoiceProcessor::intentDetected,
                this, &EnhancedPlanModeHandler::onVoiceIntent);
        
        connect(m_voiceProcessor.get(), &VoiceProcessor::planGenerated,
                this, &EnhancedPlanModeHandler::onVoicePlanGenerated);
    }
    
    void onVoiceIntent(const QString& intent) {
        // Generate plan from voice command
        m_wish = intent;
        generatePlanFromWish();
    }
    
    void onVoicePlanGenerated(const QJsonArray& plan) {
        // Display plan with voice-generated steps
        displayPlan(plan);
    }
    
public slots:
    void onVoiceStartButtonClicked() {
        m_voiceProcessor->startListening();
    }
};
```

### 3. Connect to Editor

**File**: `src/win32app/Win32IDE.cpp`

```cpp
// Add inline prediction to editor
#include "editor/inline_predictor.hpp"
#include "editor/ghost_text_renderer.hpp"

class Win32IDEWithPrediction : public Win32IDE {
private:
    std::unique_ptr<InlinePredictor> m_predictor;
    std::unique_ptr<GhostTextRenderer> m_ghostRenderer;
    
    void setupInlinePrediction() {
        m_predictor = std::make_unique<InlinePredictor>();
        m_ghostRenderer = std::make_unique<GhostTextRenderer>();
        
        // On every character typed
        connect(m_editor, &RichEditControl::textChanged, [this]() {
            auto line = getCurrentLine();
            int pos = getCursorPosition();
            m_predictor->onTextEdited(line, pos);
        });
        
        // When prediction ready, show ghost text
        connect(m_predictor.get(), &InlinePredictor::predictionReady,
                [this](const InlinePrediction& pred) {
                    m_ghostRenderer->renderGhostText(pred.text);
                });
        
        // Tab to accept, Esc to reject
        connect(m_editor, &RichEditControl::tabPressed, 
                m_predictor.get(), &InlinePredictor::acceptPrediction);
        connect(m_editor, &RichEditControl::escapePressed,
                m_predictor.get(), &InlinePredictor::rejectPrediction);
    }
};
```

### 4. Connect to Git Panel

**File**: `src/ui/git_panel.cpp`

```cpp
// Add semantic diff analysis to git panel
#include "git/semantic_diff_analyzer.hpp"
#include "git/ai_merge_resolver.hpp"

class GitPanelWithAI : public GitPanel {
private:
    std::unique_ptr<SemanticDiffAnalyzer> m_diffAnalyzer;
    std::unique_ptr<AIMergeResolver> m_mergeResolver;
    
    void onFileDiffRequested(const QString& file) {
        QString original = getOriginalContent(file);
        QString current = getCurrentContent(file);
        
        // Analyze semantic changes
        SemanticDiff diff = m_diffAnalyzer->analyzeDiff(
            file, original, current);
        
        // Display with AI insights
        displayDiffWithAnalysis(diff);
    }
    
    void onConflictDetected(const QString& file) {
        QString base = getBaseContent(file);
        QString ours = getOurContent(file);
        QString theirs = getTheirContent(file);
        
        // Use AI to resolve
        AIMergeResolver::MergeResult result = 
            m_mergeResolver->mergeWithAI(base, ours, theirs);
        
        showMergeResolution(result);
    }
};
```

### 5. Connect to Terminal

**File**: `src/ui/terminal_widget.cpp`

```cpp
// Add sandboxed terminal
#include "terminal/sandboxed_terminal.hpp"

class TerminalWidgetWithSandbox : public TerminalWidget {
private:
    std::unique_ptr<SandboxedTerminal> m_sandbox;
    
    void setupSandbox() {
        SandboxConfig config{
            .level = SandboxLevel::STANDARD,
            .retentionMode = RetentionMode::FULL,
            .maxMemoryMB = 512,
            .accessiblePaths = {getCurrentWorkDir()}
        };
        
        m_sandbox = std::make_unique<SandboxedTerminal>(config);
        
        connect(m_sandbox.get(), &SandboxedTerminal::outputReceived,
                this, &TerminalWidget::appendOutput);
    }
    
    void onCommandEntered(const QString& command) {
        // Check if allowed
        if (!m_sandbox->isCommandAllowed(command)) {
            appendOutput("ERROR: Command blocked by sandbox");
            return;
        }
        
        m_sandbox->executeCommand(command);
    }
    
    void enableZeroRetention(bool enable) {
        RetentionMode mode = enable ? 
            RetentionMode::ZERO : RetentionMode::FULL;
        m_sandbox->setRetentionMode(mode);
    }
};
```

---

## 📋 Build & Compilation Checklist

### Step 1: Update CMakeLists.txt

- [ ] Add `add_subdirectory(src/orchestration)`
- [ ] Add `add_subdirectory(src/git)`
- [ ] Add `add_subdirectory(src/terminal)`
- [ ] Link libraries to main target

### Step 2: Create Directory Structure

```powershell
# PowerShell
mkdir -p src\orchestration, src\git, src\terminal
```

### Step 3: Copy Implementation Files

- [ ] Copy `llm_router.hpp/cpp` to `src/orchestration/`
- [ ] Copy `agent_coordinator.hpp/cpp` to `src/orchestration/`
- [ ] Copy `voice_processor.hpp/cpp` to `src/orchestration/`
- [ ] Copy `semantic_diff_analyzer.hpp/cpp` to `src/git/`
- [ ] Copy `ai_merge_resolver.hpp/cpp` to `src/git/`
- [ ] Copy `sandboxed_terminal.hpp/cpp` to `src/terminal/`
- [ ] Copy `zero_retention_manager.hpp/cpp` to `src/terminal/`

### Step 4: Configure & Build

```powershell
# Clear previous build
Remove-Item build -Recurse -Force

# Configure
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.7.3/msvc2022_64" `
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release -j 4
```

### Step 5: Verify Compilation

- [ ] Zero compilation errors
- [ ] Zero warnings
- [ ] RawrXD-Agent.exe builds successfully
- [ ] All Qt6 components link

---

## 🧪 Feature Testing Checklist

### LLM Router Testing
- [ ] Register multiple models successfully
- [ ] Route single task to optimal model
- [ ] Route ensemble to multiple models
- [ ] Fallback on model failure
- [ ] Performance metrics tracking
- [ ] Cost optimization working

### Agent Coordinator Testing
- [ ] Create agent pool
- [ ] Submit task DAG
- [ ] Respect task dependencies
- [ ] Share context between agents
- [ ] Resource lock management
- [ ] Progress tracking

### Voice Processing Testing
- [ ] Start/stop listening
- [ ] Transcribe audio correctly
- [ ] Detect intent from voice
- [ ] Generate plan from voice
- [ ] Text-to-speech feedback

### Inline Prediction Testing
- [ ] Predict next tokens on keystroke
- [ ] Render ghost text correctly
- [ ] Accept with Tab key
- [ ] Reject with Escape key
- [ ] YOLO mode aggressive caching
- [ ] Training on user feedback

### Semantic Diff Testing
- [ ] Analyze file diffs accurately
- [ ] Categorize change types
- [ ] Calculate impact levels
- [ ] Detect breaking changes
- [ ] Generate commit messages

### Merge Resolution Testing
- [ ] Detect conflicts correctly
- [ ] Suggest resolutions
- [ ] Three-way merge
- [ ] Validate merged code
- [ ] Auto-resolve simple conflicts

### Sandboxed Terminal Testing
- [ ] Block dangerous commands
- [ ] Enforce resource limits
- [ ] Restrict file access
- [ ] Track command history
- [ ] Zero-retention mode clears data
- [ ] Secure deletion working

---

## 📊 Performance Benchmarks

### Target Metrics

| Feature | Target | Acceptable |
|---------|--------|-----------|
| LLM Router decision | < 100ms | < 200ms |
| Inline prediction | < 200ms | < 500ms |
| Semantic diff analysis | < 500ms | < 1s |
| Merge resolution | < 1s | < 2s |
| Command validation | < 50ms | < 100ms |
| Voice STT | < 2s | < 5s |

### Profiling Tools

```cpp
// Add timing macros
#define TIMER_START auto start = std::chrono::high_resolution_clock::now();
#define TIMER_END(name) \
    auto end = std::chrono::high_resolution_clock::now(); \
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start); \
    qDebug() << name << "took" << ms.count() << "ms";
```

---

## 🚀 Deployment Strategy

### Phase 1: Internal Testing (Week 1)
- [ ] Compile all components
- [ ] Run unit tests
- [ ] Integration testing
- [ ] Performance profiling

### Phase 2: Beta Release (Week 2)
- [ ] Feature-flag new features
- [ ] Gather telemetry
- [ ] User feedback
- [ ] Bug fixes

### Phase 3: Production Release (Week 3)
- [ ] Full feature enable
- [ ] Documentation update
- [ ] Release notes
- [ ] Changelog

---

## 📝 Configuration Files

### config/advanced_features.ini

```ini
[orchestration]
enabled=true
llm_router_enabled=true
agent_coordinator_enabled=true
voice_enabled=false  ; Enable after testing

[inline_prediction]
enabled=true
mode=balanced  ; conservative, balanced, yolo
ghost_text_enabled=true

[git]
semantic_diff_enabled=true
ai_merge_enabled=true
auto_commit_message=true

[terminal]
sandboxing_enabled=true
sandbox_level=standard  ; permissive, standard, strict, maximum
retention_mode=full  ; full, minimal, zero
```

---

## 🔐 Security Hardening

### Input Validation Checklist
- [ ] Sanitize all LLM inputs
- [ ] Validate command arguments
- [ ] Check file paths for escapes
- [ ] Verify model endpoint URLs
- [ ] Validate JSON structures

### Access Control
- [ ] Restrict sandbox to specific directories
- [ ] Validate agent permissions
- [ ] Check resource ownership
- [ ] Audit all privileged operations

### Data Protection
- [ ] Encrypt sensitive API keys
- [ ] Mask credentials in logs
- [ ] Secure temp file deletion
- [ ] Validate SSL certificates

---

## 📞 Support & Troubleshooting

### Common Issues

**Issue**: CMake can't find Qt6
```powershell
# Solution: Set Qt path explicitly
$env:Qt6_DIR = "C:\Qt\6.7.3\msvc2022_64\lib\cmake\Qt6"
cmake -B build ...
```

**Issue**: LLM router can't connect to model
```cpp
// Solution: Check endpoint and API key
ModelInfo model = router.getModel("gpt-4");
qDebug() << "Endpoint:" << model.endpoint;
qDebug() << "Available:" << model.available;
```

**Issue**: Sandboxed terminal blocking valid command
```cpp
// Solution: Add to whitelist or reduce sandbox level
config.whitelistedCommands.append("mycommand");
// OR
config.level = SandboxLevel::PERMISSIVE;
```

---

**Status**: 🟢 **Ready for Implementation**  
**Next Step**: Follow build checklist starting with CMakeLists.txt updates

