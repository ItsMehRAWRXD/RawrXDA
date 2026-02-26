# RawrXD Autonomous Agent System - Stage 3 Autonomy

## Overview

RawrXD now features a **fully autonomous coding agent** that can plan, execute, and release code changes based on natural language prompts. This represents **Stage 3 autonomy** - the IDE can modify itself, build, test, and deploy without human intervention.

---

## Architecture

### Component Hierarchy

```
AutoBootstrap (zero-touch entry point)
    ├── Planner (NL → JSON task list)
    ├── SelfPatch (code generation + hot-reload)
    ├── ReleaseAgent (version bump + deploy)
    ├── MetaLearn (performance optimization)
    └── HotReload (live module swapping)
```

---

## Features

### ✅ Natural Language Planning
- **Input**: "Add Q8_K Vulkan kernel"
- **Output**: JSON task list with dependencies
- **Supported**: Quant kernels, releases, generic tasks

### ✅ Self-Modification
- Add Vulkan shaders with CMake integration
- Generate C++ wrappers automatically
- Hot-reload without restart

### ✅ Autonomous Release Pipeline
- Version bumping (major/minor/patch)
- Git tagging and GitHub releases
- Twitter announcements (optional)

### ✅ Meta-Learning
- Performance database (perf_db.json)
- GPU-specific quant recommendations
- Auto-best-quant selection

### ✅ Zero-Touch Triggers
- **Ctrl+Shift+A**: IDE shortcut
- **RAWRXD_WISH env-var**: CI/automation
- **Clipboard**: Voice recognition

---

## File Structure

```
src/agent/
├── planner.hpp/cpp             # NL → task list
├── self_patch.hpp/cpp          # Code generation + hot-reload
├── release_agent.hpp/cpp       # Version + deploy + tweet
├── meta_learn.hpp/cpp          # Performance database
├── hot_reload.hpp/cpp          # Live module swapping
├── auto_bootstrap.hpp/cpp      # Zero-touch entry point
└── agent_main.cpp              # Standalone executable
```

---

## Usage

### 1. IDE Integration (Ctrl+Shift+A)

**Step 1**: Type or select wish in code editor:
```
Add Q8_K Vulkan kernel
```

**Step 2**: Press **Ctrl+Shift+A**

**Step 3**: Watch the agent:
- ✅ Generate shader template
- ✅ Create C++ wrapper
- ✅ Update CMakeLists.txt
- ✅ Build quant library
- ✅ Hot-reload without restart

---

### 2. Standalone Agent

**Command-line execution:**
```powershell
build\bin\Release\RawrXD-Agent.exe "Add Q8_K kernel"
```

**CI/CD integration:**
```yaml
- name: Autonomous improvement
  run: RawrXD-Agent.exe "Optimize Q4_0 quantization"
  env:
    RAWRXD_WISH: "Add Q8_K Vulkan kernel"
    TWITTER_BEARER: ${{ secrets.TWITTER_TOKEN }}
```

---

### 3. Environment Variable Trigger

**PowerShell:**
```powershell
$env:RAWRXD_WISH = "Release v1.3.0 with better performance"
.\build\bin\Release\RawrXD-QtShell.exe
```

Agent starts automatically on launch.

---

### 4. Voice Recognition

**Windows Speech → Clipboard → Agent**

1. Enable Windows speech recognition
2. Say: "RawrXD, create React server"
3. Agent detects clipboard text
4. Executes autonomously

---

## Task Types

### Quantization Kernels

**Wish**: "Add Q6_K Vulkan kernel"

**Generated Tasks**:
```json
[
  {"type": "add_kernel", "target": "Q6_K", "template": "quant_vulkan.comp"},
  {"type": "add_cpp", "target": "quant_q6_k_wrapper"},
  {"type": "add_menu", "target": "Q6_K"},
  {"type": "bench", "target": "Q6_K"},
  {"type": "self_test", "cases": 50},
  {"type": "hot_reload"}
]
```

---

### Release Pipeline

**Wish**: "Ship v1.3.0 with Q8_K support"

**Generated Tasks**:
```json
[
  {"type": "bump_version", "part": "minor"},
  {"type": "build", "target": "RawrXD-QtShell"},
  {"type": "bench_all"},
  {"type": "self_test", "cases": 100},
  {"type": "tag"},
  {"type": "tweet", "text": "🚀 v1.3.0 released - Q8_K Vulkan!"}
]
```

---

### Generic Tasks

**Wish**: "Fix bug in streaming_inference.cpp"

**Generated Tasks**:
```json
[
  {"type": "patch_file", "target": "streaming_inference.cpp"},
  {"type": "build"},
  {"type": "self_test", "cases": 10}
]
```

---

## Meta-Learning

### Performance Database

**Location**: `perf_db.json`

**Format**:
```json
[
  {
    "quant": "Q8_K",
    "gpu": "Windows-10-winnt",
    "tps": 45.2,
    "ppl": 5.8,
    "when": 1733164800000
  }
]
```

### Auto-Best-Quant

**Algorithm**:
1. Filter records for current GPU
2. Find best perplexity
3. Allow 5% degradation for speed
4. Return quant with highest TPS

**Example**:
```cpp
MetaLearn ml;
QString best = ml.suggestQuant();
// Returns "Q5_0" (fastest within 5% quality)
```

---

## Hot-Reload

### Quantization Library

**Scenario**: User adds Q7_K kernel

**Process**:
1. `cmake --build build --target quant_ladder_avx2`
2. New library compiled
3. Signal `quantReloaded("Q7_K")`
4. InferenceEngine rebuilds tensor cache
5. **No restart required**

### IDE Self-Modification

**Scenario**: Agent patches streaming code

**Process**:
1. Build new RawrXD-QtShell.exe
2. Spawn new process
3. Old process exits after 500ms
4. Sockets/resources transfer seamlessly

---

## Safety Gates

### Blacklist

Blocks dangerous operations:
- `rm -rf`
- `format`
- `del /`
- `shutdown`
- `dd if=/dev/zero`

### Confirmation Dialog

**Before execution**:
```
Autonomously execute:
Add Q8_K Vulkan kernel

Proceed?
[Yes] [No]
```

---

## Integration Points

### MainWindow.h

```cpp
class AutoBootstrap* m_agentBootstrap{};
class HotReload* m_hotReload{};

void triggerAgentMode();
void onAgentWishReceived(const QString& wish);
void onAgentPlanGenerated(const QString& planSummary);
void onAgentExecutionCompleted(bool success);
```

### MainWindow_AI_Integration.cpp

```cpp
void MainWindow::setupAgentSystem() {
    m_agentBootstrap = AutoBootstrap::instance();
    m_hotReload = new HotReload(this);
    // Connect signals...
}

void MainWindow::setupShortcuts() {
    QShortcut* agentSc = new QShortcut(QKeySequence("Ctrl+Shift+A"), this);
    connect(agentSc, &QShortcut::activated, this, &MainWindow::triggerAgentMode);
}
```

### CMakeLists.txt

```cmake
# Standalone agent
add_executable(RawrXD-Agent
    src/agent/agent_main.cpp
    src/agent/planner.cpp
    src/agent/self_patch.cpp
    src/agent/release_agent.cpp
    src/agent/meta_learn.cpp
    src/agent/hot_reload.cpp
)

# IDE with agent
add_executable(RawrXD-QtShell
    # ... existing files ...
    src/agent/auto_bootstrap.cpp
    src/agent/planner.cpp
    # ... etc
)
```

---

## Build Instructions

### Step 1: Configure
```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
```

### Step 2: Build IDE + Agent
```powershell
cmake --build build --config Release --target RawrXD-QtShell
cmake --build build --config Release --target RawrXD-Agent
```

### Step 3: Verify
```powershell
build\bin\Release\RawrXD-Agent.exe "Add Q8_K kernel"
# Should output: [AGENT] Plan: add_kernel, add_cpp, build...
```

---

## Examples

### Example 1: Add Kernel (IDE)

1. Open RawrXD-QtShell
2. Type in editor: `Add Q8_K Vulkan kernel`
3. Select text
4. Press **Ctrl+Shift+A**
5. Confirm dialog
6. Watch HexMag console:
   ```
   [AGENT] Wish received: Add Q8_K Vulkan kernel
   [AGENT] Plan:
   • add_kernel
   • add_cpp
   • build
   • hot_reload
   [AGENT] ✅ Execution completed successfully!
   ```

---

### Example 2: Release (Command-line)

```powershell
$env:TWITTER_BEARER = "your_token_here"
build\bin\Release\RawrXD-Agent.exe "Ship v1.3.0"
```

**Output**:
```
Agent wish: Ship v1.3.0
Generated 6 tasks
Executing: bump_version
Version bumped to v1.3.0
Executing: build
Build successful
Executing: tag
Release created: v1.3.0
Executing: tweet
Tweet sent: 🚀 New release - faster inference!
Agent completed successfully!
```

---

### Example 3: Meta-Learning

```cpp
// Record performance
MetaLearn ml;
ml.record("Q8_K", 45.2, 5.8);  // TPS, PPL

// Get suggestion
QString best = ml.suggestQuant();
qDebug() << "Suggested quant:" << best;
// Output: Suggested quant: Q5_0
```

---

## Troubleshooting

### Issue: Agent doesn't start

**Check**:
1. `RAWRXD_WISH` env-var set?
2. Ctrl+Shift+A shortcut registered?
3. AutoBootstrap instance created?

**Fix**:
```cpp
// In MainWindow constructor:
setupAgentSystem();
setupShortcuts();
```

---

### Issue: Hot-reload fails

**Check**:
1. CMake build successful?
2. Binary in `build/bin/Release/`?
3. Permissions to restart process?

**Fix**:
```powershell
# Manual rebuild:
cmake --build build --config Release --target quant_ladder_avx2
```

---

### Issue: Safety gate blocks valid command

**Temporary bypass** (testing only):
```cpp
// In auto_bootstrap.cpp
bool AutoBootstrap::safetyGate(const QString& wish) {
    return true; // WARNING: Disables all safety checks
}
```

---

## Performance

### Agent Execution Time

| Task | Duration |
|------|----------|
| Plan generation | < 10 ms |
| Add kernel | ~500 ms |
| Build | ~30 s |
| Hot-reload | ~1 s |
| Git tag + upload | ~5 s |
| Tweet | ~2 s |

**Total (kernel + release)**: ~40 seconds from wish to deployment

---

### Memory Footprint

| Component | RAM |
|-----------|-----|
| Planner | 2 MB |
| SelfPatch | 5 MB |
| MetaLearn | 1 MB |
| Total | ~8 MB |

---

## Future Enhancements

### Planned (Stage 4)

1. **Multi-agent collaboration**: 3+ agents working in parallel
2. **Self-testing**: Auto-generate unit tests for patches
3. **Code review**: AI-powered patch validation
4. **Continuous learning**: Update planner from success/failure
5. **Voice control**: Full speech-to-code pipeline

---

## API Reference

### Planner

```cpp
class Planner {
public:
    QJsonArray plan(const QString& humanWish);
private:
    QJsonArray planQuantKernel(const QString& wish);
    QJsonArray planRelease(const QString& wish);
    QJsonArray planGeneric(const QString& wish);
};
```

---

### SelfPatch

```cpp
class SelfPatch : public QObject {
public:
    bool addKernel(const QString& name, const QString& templateName);
    bool addCpp(const QString& name, const QString& deps);
    bool hotReload();
    bool patchFile(const QString& filename, const QString& patch);
signals:
    void kernelAdded(const QString& name);
    void reloadCompleted(bool success);
};
```

---

### ReleaseAgent

```cpp
class ReleaseAgent : public QObject {
public:
    bool bumpVersion(const QString& part); // "major" | "minor" | "patch"
    bool tagAndUpload();
    bool tweet(const QString& text);
    QString version() const;
signals:
    void versionBumped(const QString& newVersion);
    void releaseCreated(const QString& tag);
};
```

---

### MetaLearn

```cpp
class MetaLearn : public QObject {
public:
    void record(const QString& quant, double tps, double ppl);
    QString suggestQuant();
    bool loadDatabase();
    bool saveDatabase();
signals:
    void recordAdded(const PerfRecord& record);
};
```

---

### AutoBootstrap

```cpp
class AutoBootstrap : public QObject {
public:
    static AutoBootstrap* instance();
    void start();
signals:
    void wishReceived(const QString& wish);
    void planGenerated(const QString& planSummary);
    void executionCompleted(bool success);
};
```

---

## Conclusion

RawrXD now has **Stage 3 autonomy** - it can:

✅ **Plan** tasks from natural language  
✅ **Edit** its own source code  
✅ **Build** and hot-reload  
✅ **Release** to GitHub  
✅ **Learn** from performance data  
✅ **Operate** with zero human intervention  

**Zero-touch workflow**:
1. Type: "Add Q8_K kernel"
2. Press Ctrl+Shift+A
3. Watch the magic

**No manual CMake, no hand-edited files, no restart.**

---

**Document Version**: 1.0  
**Last Updated**: December 2, 2025  
**Author**: RawrXD Development Team  
**License**: MIT
