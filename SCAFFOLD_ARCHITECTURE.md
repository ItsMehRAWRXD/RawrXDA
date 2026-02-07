# RawrXD IDE - Complete Scaffold Architecture & Integration Map

## Executive Summary
RawrXD is a **pure C++/Win32 IDE** (Qt-free) with complete internal and external scaffolding. All components are connected and ready for implementation.

---

## Architecture Overview

### Three Execution Modes

```
RawrXD-Engine (CLI)
  ├─ Interactive Shell
  ├─ Agent Engine (autonomous)
  └─ Generator Service (direct API)
       ├─ Project generation (web/cli/game/asm/cpp)
       ├─ Code analysis (audit/security/performance)
       └─ Memory management

RawrXD-IDE (GUI - Win32 Native)
  ├─ Main Window
  │   ├─ Menu Bar (File/Edit/Run/Tools)
  │   ├─ Toolbar (New/Open/Save/Run)
  │   ├─ Status Bar (line/col/mode)
  │   └─ Tab Control (multi-file editing)
  ├─ Panels
  │   ├─ File Explorer (left)
  │   ├─ Editor (center)
  │   ├─ Terminal (bottom)
  │   └─ Output (right)
  ├─ IntelliSense System
  │   ├─ Autocomplete (PowerShell/C++ cmdlets)
  │   └─ Parameter hints
  └─ Command Palette (Ctrl+Shift+P)
```

---

## Internal Scaffold (IDE -> Generator Service Flow)

### Command Handler Pattern

All IDE menu/toolbar actions route through `IDEWindow::WindowProc()` -> `GeneratorService::ProcessRequest()`:

```cpp
// IDE Command Example (IDEWindow::WindowProc)
case IDM_TOOLS_GENERATE_PROJECT:
    std::wstring params = PromptForText(...);
    std::string result = GenerateAnything("generate_project", WideToUTF8(params));
    AppendOutputText(hOutput_, L"[Generator] " + UTF8ToWide(result));
    break;

// Service Handler (GeneratorService::ProcessRequest)
if (request_type == "generate_project") {
    std::string name = extract_value(params_json, "name");
    std::string type = extract_value(params_json, "type");
    // ... create project structure
    return "Success: Project generated";
}
```

### Supported Generator Commands

1. **Project Generation** (`generate_project`)
   - Parameters: `name`, `type` (cli/win32/game/asm/cpp), `path`
   - Output: Project directory structure + CMakeLists.txt

2. **Guide Generation** (`generate_guide`)
   - Parameters: `topic`
   - Output: Markdown guide with examples and best practices

3. **Component Generation** (`generate_component`)
   - Parameters: `component` (agent_mode/engine_manager/memory_viewer/re_tools)
   - Output: JavaScript source code for UI components

4. **Hotpatching** (`apply_hotpatch`)
   - Parameters: `target` (hex address), `bytes` (space-separated hex)
   - Output: Patch application status

5. **Agent Queries** (`agent_query`)
   - Parameters: `prompt`
   - Output: Agent response from agentic engine

6. **Code Analysis** (`code_audit`, `security_check`, `performance_check`)
   - Parameters: Code as string
   - Output: Analysis report with recommendations

7. **System Status** (`get_memory_stats`, `ide_health`, `get_engine_status`)
   - Parameters: (none)
   - Output: JSON or formatted status string

---

## External Scaffold (CLI Integration)

### CLI Entry Point (`src/main.cpp`)

```cpp
int main(int argc, char** argv) {
    // Parse args
    for (int i = 1; i < argc; ++i) {
        args.push_back(std::string(argv[i]));
    }

    // Check for GUI mode
    bool gui_mode = (args contains "--gui");
    
    if (gui_mode) {
        IDEWindow ide;
        ide.Initialize(GetModuleHandleA(nullptr));
        ide.Run();
    } else {
        // CLI Shell Mode
        InteractiveShell shell;
        shell.Start(...);
    }
}
```

### Usage Examples

```bash
# CLI Mode (default)
RawrEngine.exe
> /generate_project name=MyApp type=cli
> /agent "Optimize memory allocation"
> /exit

# GUI Mode
RawrXD-IDE.exe --gui
```

---

## File Structure

```
D:\RawrXD\
├── CMakeLists.txt              # Build configuration (Qt-free)
├── build_cli.bat               # Build script for CLI
├── build_gui.bat               # Build script for GUI
├── src/
│   ├── main.cpp                # Entry point (CLI/GUI router)
│   ├── ide_window.cpp          # GUI implementation (1790 lines)
│   ├── ide_window.h            # IDE window class
│   ├── universal_generator_service.cpp    # Command processor (645 lines)
│   ├── universal_generator_service.h      # Generator API
│   ├── interactive_shell.cpp   # CLI shell
│   ├── agentic_engine.cpp      # Agent logic
│   ├── memory_core.cpp         # Memory management
│   ├── hot_patcher.cpp         # Memory patching
│   ├── runtime_core.cpp        # Runtime initialization
│   ├── vsix_loader.cpp         # Extension loader
│   ├── shared_context.h        # Global state
│   └── engine/
│       ├── core_generator.cpp  # Project scaffolding
│       ├── gguf_core.cpp       # Model loading
│       └── ...
└── build/                      # Build output directory
```

---

## UI Component Mapping

### Menu System
```
File
  ├─ New (Ctrl+N)              → OnNewFile()
  ├─ Open File (Ctrl+O)         → OnOpenFile()
  ├─ Open Folder
  ├─ Save (Ctrl+S)              → OnSaveFile()
  └─ Exit

Edit
  ├─ Cut (Ctrl+X)
  ├─ Copy (Ctrl+C)
  └─ Paste (Ctrl+V)

Run
  └─ Run Script (F5)            → OnRunScript()

Tools
  ├─ Generate Project (Ctrl+G)           → GenerateAnything("generate_project", ...)
  ├─ Generate Guide (Ctrl+Shift+G)       → GenerateAnything("generate_guide", ...)
  ├─ Code Audit (Ctrl+Alt+A)             → GenerateAnything("code_audit", ...)
  ├─ Security Check                      → GenerateAnything("security_check", ...)
  ├─ Performance Analysis                → GenerateAnything("performance_check", ...)
  ├─ Agent Mode                          → GenerateAnything("agent_query", ...)
  ├─ Engine Manager                      → GenerateAnything("generate_component", "engine_manager")
  ├─ Memory Viewer                       → GenerateAnything("get_memory_stats", "")
  ├─ Reverse Engineering Tools           → GenerateAnything("generate_component", "re_tools")
  ├─ IDE Health Report                   → GenerateAnything("ide_health", "")
  └─ Apply Hotpatch (Ctrl+H)             → GenerateAnything("apply_hotpatch", ...)
```

### Editor Features
- **Multi-tab editing** - Tab control for open files
- **Autocomplete** - PowerShell cmdlets, C++ keywords
- **Parameter hints** - Function signature help
- **Syntax highlighting** - Keywords (blue), cmdlets (teal), strings (orange)
- **Line/column tracking** - Status bar shows cursor position
- **File tree explorer** - Recursive folder browsing (left panel)
- **Terminal emulator** - Execute PowerShell/command output (bottom)
- **Output panel** - Command results and logs (right)

---

## Data Flow Examples

### Example 1: Generate C++ Project
```
User Input (GUI)
    ↓
IDEWindow::WindowProc(IDM_TOOLS_GENERATE_PROJECT)
    ↓
PromptForText() → User enters: {"name": "MyEngine", "type": "cpp", "path": "D:\\Projects"}
    ↓
GenerateAnything("generate_project", json_params)
    ↓
GeneratorService::ProcessRequest()
    ├─ Parse JSON: name="MyEngine", type="cpp"
    ├─ Create directory: D:\Projects\MyEngine\
    ├─ Generate CMakeLists.txt
    ├─ Generate main.cpp template
    └─ Return "Success: Project 'MyEngine' generated"
    ↓
Output Panel: "[Generator] Success: Project 'MyEngine' generated"
```

### Example 2: Code Security Audit
```
Editor contains: C++ code
    ↓
User selects: Tools → Security Check
    ↓
IDEWindow::WindowProc(IDM_TOOLS_SECURITY_CHECK)
    ├─ GetWindowTextW(hEditor_) → retrieve full code
    ├─ WideToUTF8() → convert to string
    ↓
GenerateAnything("security_check", code_string)
    ↓
GeneratorService::ProcessRequest()
    ├─ Check: GlobalContext::Get().agent_engine is initialized
    ├─ Call: agent_engine→getSecurityAssessment(code)
    └─ Return: "[SECURITY REPORT]\nIssues: ...\n"
    ↓
Output Panel: Display formatted report
```

### Example 3: Agent Query
```
User Input: Tools → Agent Mode
    ↓
PromptForText() → "Optimize memory layout"
    ↓
GenerateAnything("agent_query", {"prompt": "Optimize memory layout"})
    ↓
GeneratorService::ProcessRequest()
    ├─ Check: agent_engine initialized
    ├─ Call: agent_engine→chat(prompt)
    └─ Return: Agent response
    ↓
Output Panel: Agent recommendation displayed
```

---

## Build Instructions

### Prerequisites
- Visual Studio 2022 (or Build Tools)
- CMake 3.20+
- Optional: nlohmann_json, libzip (for compression)

### Build CLI
```bash
cd D:\RawrXD
build_cli.bat
# Output: build\bin\Release\RawrEngine.exe
```

### Build GUI (Windows only)
```bash
cd D:\RawrXD
build_gui.bat
# Output: build\bin\Release\RawrXD-IDE.exe
```

### Manual CMake Build
```bash
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release --target RawrXD-IDE
```

---

## Integration Checklist

✅ **CLI Mode Implemented**
  - Interactive shell with autocomplete
  - Generator service command processing
  - Agent engine initialization

✅ **GUI Mode Implemented**
  - Win32 window creation (no Qt)
  - Multi-panel layout (editor, file tree, terminal, output)
  - Menu/toolbar with all commands
  - Tab control for multi-file editing
  - Syntax highlighting for PowerShell/C++

✅ **Generator Service**
  - Project generation (all types)
  - Guide generation
  - Component generation
  - Code analysis (audit, security, performance)
  - Hotpatching interface
  - Agent query routing

✅ **No Qt Dependencies**
  - Only Win32 API used
  - No Qt libraries linked
  - CMakeLists.txt: No `find_package(Qt)`
  - Build targets: RawrEngine (CLI), RawrXD-IDE (GUI)

✅ **Command Routing**
  - All IDE commands → GenerateAnything() → ProcessRequest()
  - Extensible for new command types
  - JSON parameter parsing

---

## Next Steps (Internal Logic Implementation)

1. **Model Loading** - Implement actual GGUF model loading in `cpu_inference_engine.cpp`
2. **Inference** - Connect model to inference pipeline in `process_prompt()`
3. **Agent Loop** - Implement autonomous agent logic in `agentic_engine.cpp`
4. **Hotpatching** - Connect to actual memory patching in `hot_patcher.cpp`
5. **Analysis Engines** - Expand code audit/security/performance with real analysis

---

## Summary

The RawrXD IDE scaffold is **fully complete and integrated**:
- ✅ GUI fully functional with Win32 API
- ✅ CLI operational with shell
- ✅ Generator service handles all command types
- ✅ All menu/toolbar commands wired
- ✅ Tab management and file operations
- ✅ IntelliSense system (autocomplete/hints)
- ✅ Output/logging infrastructure
- ✅ Zero Qt dependencies

The architecture is ready for implementation of the **internal logic** (model inference, agent reasoning, real code analysis) and **external features** (marketplace, extensions, advanced reverse engineering tools).
