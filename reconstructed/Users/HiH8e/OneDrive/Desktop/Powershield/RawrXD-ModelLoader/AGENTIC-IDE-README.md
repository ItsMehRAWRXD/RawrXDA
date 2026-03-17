# Scalar Agentic IDE - Complete Autonomous Development Environment

## Overview

A **100% scalar, fully autonomous agentic IDE** with integrated server, file browser, two-way AI chat, and autonomous code generation capabilities. No threading, no GPU, no SIMD—pure CPU scalar operations.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Scalar Agentic IDE                     │
├─────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Scalar     │  │  File        │  │   Chat       │  │
│  │   Server     │  │  Browser     │  │  Interface   │  │
│  │  (HTTP/WS)   │  │  (Tree)      │  │  (Two-Way)   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  Agentic     │  │  Inference   │  │  Settings    │  │
│  │  Engine      │  │  Engine      │  │  Manager     │  │
│  │  (Auto)      │  │  (Scalar)    │  │  (Config)    │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│           Scalar Event Loop (No Threading)              │
└─────────────────────────────────────────────────────────┘
```

## Features

### 1. **Integrated Scalar Server**
- **HTTP/WebSocket server** embedded in IDE process
- **No background threads** - runs in event loop
- **Port:** 8080 (configurable)
- **Protocols:** HTTP/1.1, WebSocket (RFC 6455)

**Endpoints:**
- `POST /api/chat` - Send chat messages to AI
- `GET  /api/files` - List project files
- `POST /api/files` - Create/edit/delete files
- `POST /api/agent` - Execute autonomous tasks
- `GET  /api/status` - IDE status

### 2. **File Browser Tree**
- **Recursive directory scanning** (scalar)
- **Root:** `C:\Users\HiH8e\OneDrive\Desktop\Powershield`
- **Operations:**
  - Expand/collapse directories
  - Open files
  - Create/delete files and directories
  - Rename files
  - Search by pattern

### 3. **Two-Way AI Chat Interface**
- **Message history** with timestamps
- **Roles:** User, Assistant, System
- **Streaming support** (scalar token-by-token)
- **Context management** for conversations

**Commands:**
- `/agent <task>` - Execute autonomous agent task
- `/search <query>` - Search project files
- Regular chat - AI conversation

### 4. **Autonomous Agentic Engine**

**Capabilities:**
- ✅ Code generation from natural language
- ✅ File editing with instructions
- ✅ File creation/deletion
- ✅ Code search across project
- ✅ Refactoring suggestions
- ✅ Debugging assistance
- ✅ Code explanation
- ✅ Command execution (sandboxed)

**Task Types:**
```cpp
enum class AgentTaskType {
    CODE_GENERATION,  // Generate code from prompt
    FILE_EDITING,     // Edit existing files
    FILE_CREATION,    // Create new files
    FILE_DELETION,    // Delete files
    SEARCH_CODE,      // Search codebase
    REFACTOR,         // Refactor code
    DEBUG,            // Debug errors
    EXPLAIN_CODE,     // Explain code snippets
    RUN_COMMAND       // Execute commands
};
```

**Tools System:**
- Extensible tool registration
- Built-in tools: `read_file`, `write_file`, `list_files`
- Custom tools can be added at runtime

### 5. **Scalar Inference Engine**
- **Pure CPU scalar operations** (no GPU/SIMD)
- **GGUF model support** (Q2_K, Q4_K, F32)
- **Token generation** for chat and code completion
- **Context-aware** responses

## Building

### Prerequisites
- CMake 3.15+
- Visual Studio 2022 (or compatible C++17 compiler)
- Windows SDK

### Build Commands

```powershell
# Quick build (Release)
.\build-agentic-ide.ps1

# Debug build
.\build-agentic-ide.ps1 -BuildType Debug

# Or manually with CMake
cmake -G "Visual Studio 17 2022" -A x64 -B build-agentic -C CMakeLists-agentic.txt
cmake --build build-agentic --config Release --target RawrXD-Agentic-IDE
```

### Output
```
build-agentic/Release/RawrXD-Agentic-IDE.exe
```

## Running

### Basic Usage
```powershell
.\build-agentic\Release\RawrXD-Agentic-IDE.exe
```

### With Custom Root Directory
```powershell
.\build-agentic\Release\RawrXD-Agentic-IDE.exe "C:\Your\Project\Path"
```

### On Startup
```
=======================
  SCALAR AGENTIC IDE   
  Fully Autonomous     
  100% Scalar Only     
=======================

Root directory: C:\Users\HiH8e\OneDrive\Desktop\Powershield

IDE Features:
  ✓ Scalar HTTP/WebSocket Server (port 8080)
  ✓ File Browser Tree (recursive)
  ✓ Two-Way AI Chat Interface
  ✓ Autonomous Agentic Engine
  ✓ No Threading (100% Scalar)
  ✓ No GPU/SIMD (Pure CPU)

Available Commands:
  /agent <task>   - Execute autonomous agent task
  /search <query> - Search project files
  /quit           - Exit IDE

API Endpoints:
  POST http://localhost:8080/api/chat
  GET  http://localhost:8080/api/files
  POST http://localhost:8080/api/agent
  GET  http://localhost:8080/api/status
```

## API Usage

### Chat with AI
```bash
curl -X POST http://localhost:8080/api/chat \
  -H "Content-Type: text/plain" \
  -d "Generate a scalar matrix multiplication function"
```

Response:
```json
{
  "response": "Here's a scalar matrix multiplication...",
  "status": "ok"
}
```

### Execute Agent Task
```bash
curl -X POST http://localhost:8080/api/agent \
  -H "Content-Type: text/plain" \
  -d "Create a new file test.cpp with a main function"
```

Response:
```json
{
  "status": "task_queued"
}
```

### Check Status
```bash
curl http://localhost:8080/api/status
```

Response:
```json
{
  "server_running": true,
  "root_directory": "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield",
  "message_count": 5,
  "status": "ok"
}
```

## Usage Examples

### Example 1: Generate Code
**User:** `/agent Generate a scalar bubble sort function in C++`

**Agent Output:**
```cpp
// Generated code for: Generate a scalar bubble sort function in C++
void bubbleSort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}
```

### Example 2: Search Project
**User:** `/search matrix`

**Agent Output:**
```
Search results:
src/transformer_block_scalar.cpp
include/transformer_block.h
src/vulkan_compute.cpp

Found 3 matches
```

### Example 3: Create File
**User:** `/agent Create file utils.h with common utility functions`

**Agent Output:**
```
File created successfully: utils.h
```

## Architecture Details

### Scalar Event Loop

```cpp
void AgenticIDE::Run() {
    while (is_running_) {
        ProcessEvents();  // Server + Agent queue
        UpdateUI();       // UI updates
        Sleep(10ms);      // Prevent busy-wait
    }
}
```

**No Threading:**
- All operations execute in main thread
- Server uses non-blocking sockets
- Agent tasks processed sequentially
- Chat responses generated synchronously

### Component Integration

```cpp
// Initialize all components
server_ = std::make_unique<ScalarServer>(8080);
file_browser_ = std::make_unique<FileBrowser>();
chat_interface_ = std::make_unique<ChatInterface>();
agentic_engine_ = std::make_unique<AgenticEngine>();
inference_engine_ = std::make_unique<InferenceEngine>();

// Connect callbacks (scalar)
file_browser_->SetOnFileOpen([this](const std::string& path) {
    OnFileOpen(path);
});

chat_interface_->SetOnUserMessage([this](const std::string& msg) {
    OnUserMessage(msg);
});

agentic_engine_->SetOnTaskComplete([this](const AgentTask& task, const std::string& result) {
    OnTaskComplete(task, result);
});
```

## Scalar Compliance

### ✅ What's Scalar
- All math operations (addition, multiplication, etc.)
- File I/O (read/write one byte at a time)
- String operations (character-by-character)
- Network I/O (non-blocking, single-threaded)
- Tree traversal (stack-based, iterative)
- Task processing (sequential queue)

### ❌ What's NOT Included
- ❌ No `std::thread`, `std::async`, background threads
- ❌ No `std::atomic`, `std::mutex`, locks
- ❌ No SIMD instructions (SSE, AVX, etc.)
- ❌ No GPU/Vulkan/CUDA acceleration
- ❌ No parallel algorithms
- ❌ No vectorized operations

## Performance Characteristics

**Advantages:**
- ✅ Predictable, deterministic execution
- ✅ Easy debugging (single-threaded)
- ✅ Minimal memory overhead
- ✅ Portable (any CPU)
- ✅ No race conditions

**Trade-offs:**
- ⚠️ Slower than multi-threaded/GPU versions
- ⚠️ Sequential task processing
- ⚠️ Blocking network I/O during processing
- ⚠️ No parallel file scanning

## Configuration

### Default Settings
```cpp
// Server
port: 8080
max_connections: 10
non_blocking: true

// File Browser
root: "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield"
auto_refresh: false
recursive_scan: true

// Chat
max_history: 1000
streaming_enabled: true

// Agent
max_queue_size: 100
working_directory: <root>
```

## Troubleshooting

### Server Won't Start
```
Error: Failed to initialize socket
```
**Solution:** Port 8080 may be in use. Change port in `AgenticIDE` constructor.

### File Browser Empty
```
Warning: No files found
```
**Solution:** Check root directory path exists and is accessible.

### Agent Tasks Not Processing
```
Task queued but never completes
```
**Solution:** Ensure `ProcessQueue()` is being called in event loop.

## Extension

### Adding Custom Agent Tools

```cpp
AgentTool custom_tool{
    "my_tool",
    "Description of what it does",
    {"param1", "param2"},
    [](const std::map<std::string, std::string>& params) -> std::string {
        // Scalar implementation
        return "Result";
    }
};

agentic_engine->RegisterTool(custom_tool);
```

### Adding Server Routes

```cpp
server->POST("/api/custom", [](const HttpRequest& req) -> HttpResponse {
    HttpResponse res;
    res.status_code = 200;
    res.content_type = "application/json";
    res.body = "{\"custom\":\"response\"}";
    return res;
});
```

## Roadmap

### Planned Features
- [ ] GUI interface (Qt/Win32)
- [ ] Syntax highlighting
- [ ] Code completion
- [ ] Multi-file refactoring
- [ ] Git integration
- [ ] Plugin system
- [ ] Remote development
- [ ] Collaborative editing

### Performance Optimizations
- [ ] Lazy file tree loading
- [ ] Incremental file scanning
- [ ] Response caching
- [ ] Batch file operations

## License

See main project README

## Credits

Built with 100% scalar operations for maximum portability and debuggability.
