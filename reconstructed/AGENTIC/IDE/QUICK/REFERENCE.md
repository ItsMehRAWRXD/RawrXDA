# RawrXD Agentic IDE - Quick Reference Guide

**Location:** `d:\temp\RawrXD-agentic-ide-production`  
**For Quick Lookups:** File paths, tool signatures, agent names

---

## 🎯 QUICK FILE LOCATION GUIDE

### Core Agentic Files
| Component | File Path | Type |
|-----------|-----------|------|
| Tool Interface | `include/agentic_tools.hpp` | Header |
| Tool Implementation | `src/agentic/agentic_tools.cpp` | Source |
| Tool Tests | `docs/agentic/COMPREHENSIVE_TEST_SUMMARY.md` | Docs |

### Main IDE Files
| Component | File Path | Type |
|-----------|-----------|------|
| Main IDE | `src/production_agentic_ide.cpp/.h` | Source |
| Main Window | `include/enhanced_main_window.h` | Header |
| Chat Editor | `src/paint_chat_editor.cpp/.h` | Source |
| Agent Orchestrator | `src/agent_orchestra.cpp` | Source |

### Model System Files
| Component | File Path | Type |
|-----------|-----------|------|
| Model Loader | `RawrXD-ModelLoader/src/gguf_loader.cpp` | Source |
| Model Router | `RawrXD-ModelLoader/src/universal_model_router.cpp` | Source |
| Model Interface | `RawrXD-ModelLoader/src/model_interface.cpp` | Source |

### Agent Files (RawrXD-ModelLoader/src/)
| Agent | File(s) |
|-------|---------|
| AdvancedCodingAgent | `advanced_coding_agent.cpp`, `AdvancedCodingAgent.cpp` |
| Orchestrator | `autonomous_intelligence_orchestrator.cpp` |
| ModelManager | `autonomous_model_manager.cpp` |
| PlanningAgent | `planning_agent.cpp` |
| FeatureEngine | `autonomous_feature_engine.cpp` |
| CopilotBridge | `agentic_copilot_bridge.cpp` |
| Engine | `agentic_engine.cpp` |
| Executor | `agentic_executor.cpp` |
| FileOperations | `agentic_file_operations.cpp` |
| MemorySystem | `agentic_memory_system.cpp` |

---

## 🔧 TOOL SIGNATURES (C++ Function Prototypes)

```cpp
// File Operations
ToolResult readFile(const std::string& filePath);
ToolResult writeFile(const std::string& filePath, const std::string& content);
ToolResult listDirectory(const std::string& dirPath);

// Process Management
ToolResult executeCommand(const std::string& program, const std::vector<std::string>& args);

// Code Search
ToolResult grepSearch(const std::string& pattern, const std::string& path);

// VCS Integration
ToolResult gitStatus(const std::string& repoPath);

// Build System
ToolResult runTests(const std::string& testPath);

// Code Analysis
ToolResult analyzeCode(const std::string& filePath);
```

---

## ⚡ TOOL USAGE EXAMPLES

### Read a File
```cpp
auto result = executor.executeTool("readFile", {"/path/to/file.txt"});
if (result.success) {
    std::cout << result.output << std::endl;
} else {
    std::cout << "Error: " << result.error << std::endl;
}
```

### Write a File
```cpp
auto result = executor.executeTool("writeFile", {
    "/path/to/newfile.cpp",
    "int main() { return 0; }"
});
std::cout << "Execution time: " << result.executionTimeMs << "ms" << std::endl;
```

### List Directory
```cpp
auto result = executor.executeTool("listDirectory", {"/src"});
// Output: [DIR]  subfolder
//         [FILE] main.cpp
//         [FILE] utils.h
```

### Execute Command
```cpp
auto result = executor.executeTool("executeCommand", {
    "cmake", "--build", "build", "--config", "Release"
});
std::cout << result.output << std::endl; // Compiler output
```

### Search with Regex
```cpp
auto result = executor.executeTool("grepSearch", {
    "\\bint\\s+main\\b",  // Regex pattern
    "/src"                  // Search path
});
// Output: /src/main.cpp:5: int main() {
```

### Check Git Status
```cpp
auto result = executor.executeTool("gitStatus", {"."});
// Output: M  file.cpp
//         ?  newfile.txt
```

### Run Tests
```cpp
auto result = executor.executeTool("runTests", {"."});
// Detects CMakeLists.txt or pytest.ini and runs tests
```

### Analyze Code
```cpp
auto result = executor.executeTool("analyzeCode", {"/src/main.cpp"});
// Output: Code Analysis for /src/main.cpp
//         Language: cpp
//         Total Lines: 245
//         Functions: 12
//         Classes: 3
```

---

## 📊 ToolResult Structure

```cpp
struct ToolResult {
    bool success;              // Tool execution succeeded
    std::string output;        // Output from successful execution
    std::string error;         // Error message on failure
    int exitCode;              // Process exit code (0 = success)
    double executionTimeMs;    // Execution time in milliseconds
};
```

---

## 🔗 CALLBACK REGISTRATION

```cpp
AgenticToolExecutor executor;

// Called after any tool execution
executor.setToolExecutedCallback([](const std::string& tool, const ToolResult& result) {
    std::cout << "Tool " << tool << " completed in " << result.executionTimeMs << "ms" << std::endl;
});

// Called on successful execution
executor.setToolExecutionCompletedCallback([](const std::string& tool, const std::string& output) {
    std::cout << "Success: " << output << std::endl;
});

// Called on failure
executor.setToolExecutionErrorCallback([](const std::string& tool, const std::string& error) {
    std::cout << "Error in " << tool << ": " << error << std::endl;
});

// Called on tool not found
executor.setToolFailedCallback([](const std::string& tool, const std::string& error) {
    std::cout << "Failed: " << error << std::endl;
});
```

---

## 🤖 AGENT QUICK REFERENCE

### AdvancedCodingAgent
**Purpose:** Code generation and refactoring  
**Location:** `RawrXD-ModelLoader/src/advanced_coding_agent.cpp`  
**Key Methods:**
- `generateCode(const QString& spec)` - Generate code from spec
- `refactorCode(const QString& code)` - Suggest refactorings
- `analyzeCodeQuality(const QString& code)` - Analyze quality

### AutonomousIntelligenceOrchestrator
**Purpose:** Coordinate multiple agents  
**Location:** `RawrXD-ModelLoader/src/autonomous_intelligence_orchestrator.cpp`  
**Key Methods:**
- `executeGoal(const QString& goal)` - Execute high-level goal
- `decomposeTask(const QString& task)` - Break into sub-tasks
- `routeToAgent(const Task& task)` - Select appropriate agent

### AutonomousModelManager
**Purpose:** Model lifecycle management  
**Location:** `RawrXD-ModelLoader/src/autonomous_model_manager.cpp`  
**Key Methods:**
- `loadModel(const QString& modelPath)` - Load a model
- `selectBestModel(const TaskType& type)` - Choose model for task
- `unloadModel(const QString& modelId)` - Free model memory

### PlanningAgent
**Purpose:** Create execution plans  
**Location:** `RawrXD-ModelLoader/src/planning_agent.cpp`  
**Key Methods:**
- `createPlan(const QString& goal)` - Generate execution plan
- `optimizePlan(const Plan& plan)` - Optimize plan
- `analyzeDependencies(const Plan& plan)` - Identify dependencies

### AutonomousFeatureEngine
**Purpose:** Detect and suggest features  
**Location:** `RawrXD-ModelLoader/src/autonomous_feature_engine.cpp`  
**Key Methods:**
- `detectPatterns(const QString& code)` - Find patterns
- `suggestFeatures(const QString& code)` - Suggest improvements
- `generateDocumentation(const QString& code)` - Auto-doc

### AgenticEngine
**Purpose:** Core reasoning and tool execution  
**Location:** `RawrXD-ModelLoader/src/agentic_engine.cpp`  
**Key Methods:**
- `executePlan(const Plan& plan)` - Execute task plan
- `selectTool(const Task& task)` - Choose tool for task
- `evaluateResult(const ToolResult& result)` - Validate result

### AgenticExecutor
**Purpose:** Monitor and execute tasks  
**Location:** `RawrXD-ModelLoader/src/agentic_executor.cpp`  
**Key Methods:**
- `executeTask(const Task& task)` - Run task with monitoring
- `trackProgress()` - Monitor execution
- `handleError(const Error& error)` - Error recovery

---

## 📈 ARCHITECTURE LAYERS

```
┌─────────────────────────────────┐
│     User Interface Layer         │
│  (EnhancedMainWindow, Widgets)   │
└─────────────────────────────────┘
           ↑         ↓
┌─────────────────────────────────┐
│    Agentic Agent Layer          │
│  (Orchestrator, Agents)         │
└─────────────────────────────────┘
           ↑         ↓
┌─────────────────────────────────┐
│     Tool Execution Layer        │
│  (AgenticToolExecutor)          │
└─────────────────────────────────┘
           ↑         ↓
┌─────────────────────────────────┐
│    Model Loading Layer          │
│  (ModelRouter, ModelManager)    │
└─────────────────────────────────┘
           ↑         ↓
┌─────────────────────────────────┐
│    Backend Layer (GGML/GPU)     │
│  (GGML, Vulkan, CPU)            │
└─────────────────────────────────┘
```

---

## 🎓 LEARNING PATHS

### Path 1: Tool Development
1. Read: `include/agentic_tools.hpp` - Understand interface
2. Study: `src/agentic/agentic_tools.cpp` - See implementations
3. Read: `docs/agentic/COMPREHENSIVE_TEST_SUMMARY.md` - Understand testing
4. Implement: New tool in agentic_tools.cpp

### Path 2: Agent Development
1. Study: `RawrXD-ModelLoader/src/autonomous_feature_engine.cpp` - Simple agent
2. Read: `RawrXD-ModelLoader/src/autonomous_intelligence_orchestrator.cpp` - Complex agent
3. Understand: Agent-to-tool mapping patterns
4. Implement: Custom agent extending base patterns

### Path 3: UI Integration
1. Look at: `include/enhanced_main_window.h` - Main window structure
2. Study: `src/paint_chat_editor.cpp` - Integrated component
3. Examine: `RawrXD-ModelLoader/src/autonomous_widgets.cpp` - Auto-widgets
4. Create: Custom widget with agent callbacks

### Path 4: Model Integration
1. Read: `RawrXD-ModelLoader/src/gguf_loader.cpp` - Model loading
2. Study: `RawrXD-ModelLoader/src/universal_model_router.cpp` - Model routing
3. Look at: Model registration patterns
4. Integrate: Custom model type

---

## 🔍 FINDING THINGS

### I need to add a new tool
**Go to:** `src/agentic/agentic_tools.hpp` and `.cpp`  
**Pattern:** Look at readFile for simple example

### I need to create an agent
**Go to:** `RawrXD-ModelLoader/src/` and look for `autonomous_*.cpp`  
**Pattern:** Study AutonomousFeatureEngine for simple example

### I need to integrate the UI
**Go to:** `src/production_agentic_ide.cpp` or `src/enhanced_main_window.cpp`  
**Pattern:** Look for signal/slot connections

### I need to add model support
**Go to:** `RawrXD-ModelLoader/src/universal_model_router.cpp`  
**Pattern:** Add case in model selection logic

### I need to understand callbacks
**Go to:** `include/agentic_tools.hpp` lines with "setTool...Callback"  
**Pattern:** Register callback in your code

---

## 🚀 BUILD & RUN

### Build Steps
```bash
cd d:\temp\RawrXD-agentic-ide-production
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Run IDE
```bash
./build/bin/AgenticIDEWin
# or
build\bin\AgenticIDEWin.exe  # Windows
```

### Run Tests
```bash
ctest --output-on-failure
```

---

## 📞 KEY INTERFACES

### AgenticToolExecutor
```cpp
class AgenticToolExecutor {
public:
    ToolResult executeTool(const std::string& toolName, const std::vector<std::string>& arguments);
    void registerTool(const std::string& name, std::function<ToolResult(...)> executor);
    void setToolExecutedCallback(std::function<void(...)> cb);
    void setToolExecutionCompletedCallback(std::function<void(...)> cb);
    void setToolExecutionErrorCallback(std::function<void(...)> cb);
    void setToolFailedCallback(std::function<void(...)> cb);
};
```

### Agent Base Pattern
```cpp
class MyAgent {
public:
    QString executeTask(const QString& request);
private:
    AgenticToolExecutor* m_toolExecutor;
    ModelRouter* m_modelRouter;
    QStringList executeTool(const QString& toolName, ...);
};
```

---

## 💡 TIPS & TRICKS

1. **Parallel Tool Execution:** Tools can be called concurrently via thread pool
2. **Tool Chaining:** Pass output of one tool to another's input
3. **Custom Tools:** Register tools at runtime for extensibility
4. **Error Handling:** Always check `result.success` before using output
5. **Performance:** Use analyzeCode for quick file metrics
6. **Search:** Use grepSearch for powerful file pattern matching
7. **Async UI:** Wrap tool calls in async/await for responsive UI
8. **Caching:** Store frequently used results to avoid re-execution

---

## 🐛 TROUBLESHOOTING

| Issue | Solution |
|-------|----------|
| Tool not found | Check tool is registered in initializeBuiltInTools() |
| Timeout | Increase timeout parameter, simplify task |
| File not found | Check path is absolute or relative to working directory |
| Permission denied | Verify file/directory permissions |
| Pattern doesn't match | Test regex pattern separately, escape special chars |
| Out of memory | Reduce input size or use streaming where available |

---

## 📚 RELATED FILES TO READ

**For Understanding Core Concepts:**
- `CMakeLists.txt` - Build configuration
- `README.md` - If exists in project root
- `docs/agentic/COMPREHENSIVE_TEST_SUMMARY.md` - Test documentation

**For Implementation Details:**
- `src/agentic/agentic_tools.cpp` - 308 lines of tool implementations
- `RawrXD-ModelLoader/src/agentic_engine.cpp` - Core execution logic
- `RawrXD-ModelLoader/src/autonomous_intelligence_orchestrator.cpp` - Orchestration

**For Integration Examples:**
- `src/production_agentic_ide.cpp` - Main IDE integration
- `src/agent_orchestra.cpp` - CLI orchestration
- `RawrXD-ModelLoader/src/ide_main_window.cpp` - Window integration

---

**End of Quick Reference Guide**  
*For detailed information, see AGENTIC_IDE_FILES_COMPREHENSIVE_INVENTORY.md and AGENTIC_IDE_TOOLS_AND_AGENTS_DETAILED.md*
