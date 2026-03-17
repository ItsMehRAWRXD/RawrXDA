# RawrXD Agentic IDE - Full Component Architecture & File Locations

## Overview
The ProductionAgenticIDE is a fully-featured native Windows IDE built on Win32 (zero Qt dependency) with integrated agentic capabilities, file browsing, chat, code analysis, and more.

---

## 1. AGENTIC BROWSER (Sandboxed Web Navigation)
**Files:**
- Header: `include/agentic_browser.h`
- Implementation: `src/agentic_browser.cpp`

**Features:**
- Full HTTP GET/POST support with custom headers
- Navigation history (back/forward stacks)
- HTML text extraction and link extraction (up to 200 links)
- Timeout control (default 15s, configurable)
- Custom user agent (`RawrXD-AgenticBrowser/1.0`)
- Cookie control (disabled by default for security)
- Redirect following
- Structured logging and metrics integration

**Key Methods:**
- `navigate(url)` / `navigate(url, options)` - Navigate to URL
- `httpGet(url)` / `httpPost(url, body, headers)` - HTTP operations
- `extractMainText()` - Get main page text
- `extractLinks(maxLinks)` - Get links from page
- `goBack()` / `goForward()` - History navigation
- `stop()` - Stop loading

**Integration:**
- Instantiated in `ProductionAgenticIDE` as `m_agenticBrowser`
- Accessible via menu and UI commands in the IDE main window

---

## 2. FILE BROWSER / FILE TREE (Full Drive + Root Navigation)
**Files:**
- Header: `src/native_file_tree.h`
- Implementation: `src/native_file_tree.cpp`

**Features:**
- Full filesystem navigation (all drives on Windows)
- Directory enumeration with file metadata
- File size and modification time tracking
- Double-click file opening
- Right-click context menu support
- Root path switching (can browse from any drive/folder)
- Real-time refresh capability

**Key Structures:**
```cpp
struct FileEntry {
    std::string name;          // filename or dirname
    std::string path;          // full path
    bool isDirectory;          // is folder?
    size_t size;               // file size in bytes
    std::string modified;      // last modified timestamp
};
```

**Key Methods:**
- `create(parent, x, y, width, height)` - Create tree widget
- `setRootPath(path)` - Set starting directory (e.g., `C:\`, `D:\`, etc.)
- `refresh()` - Reload current directory
- `getSelectedPath()` - Get currently selected file/folder
- `getCurrentEntries()` - List all visible entries
- `setOnDoubleClick(callback)` - Handle file opening
- `setOnContextMenu(callback)` - Handle right-click menu

**Integration:**
- Instantiated in `ProductionAgenticIDE` as `m_fileTree`
- Wired to IDE via `onFileTreeDoubleClicked()` and `onFileTreeContextMenu()`
- Can switch root paths via menu or file dialog
- Supports opening files directly in code editor

---

## 3. AGENT CHAT PANE (Multi-Agent, Voice-Enabled, Model Selection)
**Files:**
- Header: `include/agent_orchestra.h`
- Implementation: `src/agent_orchestra.cpp`
- Voice Processor: `include/voice_processor.h` / `src/voice_processor.cpp`

**Features:**
- Multi-agent conversation management
- Voice input via microphone
- Voice output with accent selection (multiple accents supported)
- Real-time agent response generation
- Chat history with timestamps
- Model selection integration
- Agent switching and dynamic agent registration
- Output volume control (0-100%)

**Key Methods:**
- `addAgent(agentId, agentName)` - Register a new agent
- `setActiveAgent(agentId)` - Switch to agent
- `sendMessage(message)` - Send chat message
- `startVoiceInput()` / `stopVoiceInput()` - Voice recording control
- `setVoiceAccent(accent)` - Select voice accent
- `clearHistory()` - Clear chat history

**Voice Accents (from VoiceProcessor):**
- British English
- American English
- Australian English
- Indian English
- And more (configurable)

**Callbacks:**
- `setMessageCallback()` - On new message
- `setVoiceCallback()` - On voice text recognized
- `setErrorCallback()` - On error

**Integration:**
- Instantiated in `ProductionAgenticIDE` as `m_agentOrchestra`
- Integrated into a chat tabbed interface (100+ tabs supported)
- Connected to model router for agent response generation
- Voice processing runs in background threads

---

## 4. AGENTIC ENGINE & TOOLS (Backend Agent Execution)
**Files:**
- Main: `src/agentic/agentic_tools.hpp` (complete tool definitions)
- Executor: `src/agentic/agentic_executor.cpp`
- Tool Registry: `src/agentic/enhanced_tool_registry.cpp`
- Model Router: `src/model_router.hpp` / `src/model_router.cpp`

**Built-In Tools:**
1. **File Operations**
   - `readFile(filePath)` - Read file content
   - `writeFile(filePath, content)` - Write/create file
   - `listDirectory(dirPath)` - List directory contents

2. **Search & Analysis**
   - `grepSearch(pattern, path)` - Search files by regex
   - `analyzeCode(filePath)` - Run code analysis (AICI integration)

3. **System Operations**
   - `executeCommand(command, args)` - Run shell commands
   - `runTests(testPath)` - Execute test suites
   - `gitStatus(repoPath)` - Git repository status

4. **Custom Tools**
   - `registerTool(name, executor)` - Register custom tools

**Tool Result Structure:**
```cpp
struct AgenticToolResult {
    bool success;
    std::string output;
    std::string error;
    int exitCode;
    double executionTimeMs;
};
```

**Model Router:**
- Multi-backend model routing (local LLMs, cloud APIs)
- Round-robin load balancing with atomic counters
- Embedding support
- Streaming inference
- Health checks and metrics

**Integration:**
- Agent responses generated via `AgenticToolExecutor`
- Model selection via dropdown in IDE
- Results piped to chat interface
- Metrics and latency tracking

---

## 5. MAIN IDE WINDOW & LAYOUT
**Files:**
- Header: `include/production_agentic_ide.h`
- Implementation: `src/production_agentic_ide.cpp`
- Entry Point: `src/windows_main.cpp`

**Key Components:**
- **Main Tab Widget** (`m_mainTabWidget`) - Tabbed interface for Paint/Code/Chat
- **File Tree** (`m_fileTree`) - Full filesystem navigation
- **Features Panel** (`m_featuresPanel`) - Hierarchical feature toggles (7 categories, 11 features)
- **Terminal Pane** (`m_terminal`) - Integrated terminal for commands
- **Multi-Pane Layout** (`m_multiPaneLayout`) - Resizable panels for layout management
- **Agentic Browser** (`m_agenticBrowser`) - Sandboxed web browser
- **Agent Orchestra** (`m_agentOrchestra`) - Chat and voice interface

**Menu Structure:**
```
File → New Paint, New Code, New Chat, Open, Save, Save As, Export, Exit
Edit → Undo, Redo, Cut, Copy, Paste
View → Toggle Paint/Code/Chat/Features/FileTree/Terminal, Reset Layout
Agent → Send to Agent, Model Selection, Voice Input, Agent Switching
Tools → Command Palette (Ctrl+K), Go to File, Go to Symbol
Help → About, Statistics
```

**Main Toolbar:**
- Create new tab buttons (+/-/X for tab management)
- Save/Open buttons
- Undo/Redo buttons
- Model/Agent selection dropdowns
- Voice recording button

**Status Bar:**
- FPS counter (smoothed)
- Bitrate/streaming stats
- Status messages
- File information

---

## 6. CODE ANALYSIS INTEGRATION (AICI)
**Location:** `src/aici/` (Dependency-Free Analyzer)

**Features:**
- Static symbol extraction (functions, classes)
- Call reference tracking
- Code metrics (lines, complexity, comments)
- Security findings (strcpy, eval, subprocess shell=True, etc.)
- Multiple output formats (JSON, NDJSON, SARIF)
- Multi-threaded analysis
- Default excludes (node_modules, build, .git, etc.)

**CLI Access:**
```bash
AICodeIntelligenceCLI.exe --root <path> --report findings|metrics|symbols|json|sarif
```

**Integration in IDE:**
- Auto-analyze on file save (configurable)
- Security findings shown in features panel
- Code metrics displayed in status bar

---

## 7. WINDOW ARCHITECTURE (Native Win32, Zero Qt)
**Native Components:**
- `NativeWidget` - Base widget class
- `NativeTabWidget` - Tab management
- `NativeLayout` - Layout engine
- `NativeLabel` - Label display
- `NativeButton` - Button controls
- `NativeTextEdit` - Text input
- Other native UI primitives

**Integration:**
- All Qt stub headers in `include/` (empty implementations when Qt removed)
- Production builds use native Win32 directly
- No runtime Qt dependencies

---

## 8. FILE LOCATIONS QUICK REFERENCE

| Component | Header | Implementation |
|-----------|--------|-----------------|
| Agentic Browser | `include/agentic_browser.h` | `src/agentic_browser.cpp` |
| File Tree | `src/native_file_tree.h` | `src/native_file_tree.cpp` |
| Agent Orchestration | `include/agent_orchestra.h` | `src/agent_orchestra.cpp` |
| Voice Processor | `include/voice_processor.h` | `src/voice_processor.cpp` |
| Production IDE | `include/production_agentic_ide.h` | `src/production_agentic_ide.cpp` |
| Agentic Tools | `src/agentic/agentic_tools.hpp` | `src/agentic/agentic_executor.cpp` |
| Tool Registry | `src/agentic/agentic_tools.hpp` | `src/agentic/enhanced_tool_registry.cpp` |
| Model Router | `src/model_router.hpp` | `src/model_router.cpp` |
| Code Analyzer (AICI) | `src/aici/indexer.hpp` | `src/aici/indexer.cpp` |
| Windows Entry Point | — | `src/windows_main.cpp` |

---

## 9. BUILD & RUN

**Build:**
```pwsh
cd "d:\temp\RawrXD-agentic-ide-production"
cmake -S . -B build
cmake --build build --config Release --target AgenticIDEWin
```

**Run:**
```pwsh
.\build\bin\AgenticIDEWin.exe
```

**Outputs:**
- Paint editor with unlimited tabs
- Code editor (MASM: 1M+ tabs, standard: 100K+)
- Chat interface (100+ tabs)
- Full file browser (all drives)
- Agentic browser (sandboxed web)
- Agent chat with voice and model selection
- Real-time metrics (FPS, bitrate)

---

## 10. KEY FEATURES SUMMARY

✓ **Full-featured IDE** - Paint, Code, Chat editors  
✓ **File Browser** - Full drive navigation (C:\, D:\, E:\, etc.)  
✓ **Agentic Browser** - Sandboxed web navigation with HTTP support  
✓ **Agent Chat** - Multi-agent, voice-enabled, model selection  
✓ **Code Analysis** - AICI: security findings, metrics, symbols  
✓ **Terminal** - Integrated command execution  
✓ **Metrics** - FPS, bitrate, streaming stats  
✓ **Zero Qt** - Pure native Win32, no external dependencies  
✓ **Production-Ready** - Structured logging, error handling, metrics  

