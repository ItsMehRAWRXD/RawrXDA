# RawrXD IDE - Internal Architecture Guide for Developers

## Quick Start: How Everything Connects

### The Universal Generator Service: Your One-Stop Shop

Every IDE feature routes through this single C++ API:

```cpp
std::string GenerateAnything(const std::string& intent, const std::string& parameters);
```

### Example: Generate a Project

**User clicks**: Tools → Generate Project
**IDE does**:
```cpp
std::string params = "{\"name\":\"MyApp\",\"type\":\"cli\",\"path\":\".\"}"
std::string result = GenerateAnything("generate_project", params);
AppendOutputText(hOutput_, L"[Generator] " + UTF8ToWide(result));
```

**Service**:
- Extracts name/type/path using manual JSON parsing
- Calls `generate_cli_project()` or equivalent
- Creates directory structure, writes CMakeLists.txt, generates main.cpp
- Returns success/error message

**Output**: New project folder with complete build system

---

## The Menu Command Flow

### Step 1: User Clicks Menu Item
```cpp
case IDM_TOOLS_HOTPATCH:
    // User selected Tools → Hotpatch
    std::wstring target = PromptForText(hwnd_, L"Hotpatch", L"Enter target (e.g., 0x1234):", L"0x00000000");
    std::wstring bytes = PromptForText(hwnd_, L"Hotpatch", L"Enter bytes (hex):", L"90 90");
```

### Step 2: Construct JSON & Call Generator
```cpp
std::string json = "{\"target\":\"" + WideToUTF8(target) + "\", \"bytes\":\"" + WideToUTF8(bytes) + "\"}";
std::string result = GenerateAnything("apply_hotpatch", json);
```

### Step 3: Generator Routes Request
```cpp
if (request_type == "apply_hotpatch") {
    std::string target_str = extract_value(params_json, "target");
    std::string bytes_str = extract_value(params_json, "bytes");
    // ... parse hex, call GlobalContext::Get().patcher->ApplyPatch()
    return success ? "Success: Hotpatch applied to " + target_str : "Error...";
}
```

### Step 4: Display Result
```cpp
AppendOutputText(hOutput_, L"[Hotpatch] " + UTF8ToWide(result) + L"\r\n");
```

---

## Request Types Implemented

| Request Type | Parameters | Returns | Use Case |
|---|---|---|---|
| `generate_project` | name, type, path | Success/error msg | Create new C++ project |
| `generate_guide` | topic | Formatted guide text | Generate documentation |
| `apply_hotpatch` | target (hex), bytes | Patch status | Live memory modification |
| `get_memory_stats` | (none) | Memory profile JSON | Memory inspection |
| `get_engine_status` | (none) | Engine state | System diagnostics |
| `agent_query` | prompt | Agent response | Autonomous task planning |
| `code_audit` | code (string) | Audit report | Static analysis |
| `security_check` | code (string) | Security warnings | Vulnerability detection |
| `performance_check` | code (string) | Performance tips | Optimization advice |
| `ide_health` | (none) | Health report | System diagnostics |
| `load_model` | path | Load status | Model inference setup |
| `inference` | prompt | Generated text | Run inference |
| `search_extensions` | (none) | Extension list | Marketplace search |
| `install_extension` | id | Installation status | Plugin management |

---

## The IDE Window Class Structure

### Main Components

```
IDEWindow
├── Editor (RichEdit control, ID_EDITOR)
│   └── Syntax highlighting, tab management
├── File Tree (TreeView, ID_FILETREE)
│   └── Folder navigation
├── Terminal (Edit control, ID_TERMINAL)
│   └── PowerShell command execution
├── Output (Read-only Edit, ID_OUTPUT)
│   └── Tool results, error messages
├── Tab Control (Tab control, ID_TABCONTROL)
│   └── Multi-document interface
├── Status Bar
│   └── Line/col position, file mode
└── Menu Bar
    ├── File: New, Open, Save, Exit
    ├── Edit: Cut, Copy, Paste
    ├── Run: Execute Script
    └── Tools: 10 integrated features
```

### Key Handler Methods

**File Operations**:
- `OnNewFile()` → CreateNewTab()
- `OnOpenFile()` → LoadFileIntoEditor() → CreateNewTab()
- `OnSaveFile()` → SaveCurrentTab()

**Generation**:
- `IDM_TOOLS_GENERATE_PROJECT` → GenerateAnything("generate_project")
- `IDM_TOOLS_GENERATE_GUIDE` → GenerateAnything("generate_guide")

**Analysis**:
- `IDM_TOOLS_CODE_AUDIT` → GenerateAnything("code_audit", code)
- `IDM_TOOLS_SECURITY_CHECK` → GenerateAnything("security_check", code)
- `IDM_TOOLS_PERFORMANCE_ANALYZE` → GenerateAnything("performance_check", code)

**Advanced**:
- `IDM_TOOLS_HOTPATCH` → GenerateAnything("apply_hotpatch", {target, bytes})
- `IDM_TOOLS_AGENT_MODE` → GenerateAnything("agent_query", {prompt})
- `IDM_TOOLS_ENGINE_MANAGER` → GenerateAnything("generate_component", "engine_manager")
- `IDM_TOOLS_MEMORY_VIEWER` → GenerateAnything("get_memory_stats")
- `IDM_TOOLS_RE_TOOLS` → GenerateAnything("generate_component", "re_tools")
- `IDM_TOOLS_IDE_HEALTH` → GenerateAnything("ide_health")

---

## Adding a New Feature

### Example: Add "Compile & Run" Feature

**Step 1**: Define the request type
```cpp
// In universal_generator_service.cpp ProcessRequest()
if (request_type == "compile_and_run") {
    std::string code = extract_value(params_json, "code");
    std::string filepath = extract_value(params_json, "filepath");
    
    // Write to temp file, call compiler, execute
    // Return output
    return "[Compiled]\n" + output + "\n[Executed]\n" + result;
}
```

**Step 2**: Add menu item
```cpp
// In CreateMenuBar()
AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_COMPILE_RUN, L"&Compile && Run\tCtrl+Shift+B");
```

**Step 3**: Add handler
```cpp
// In WindowProc WM_COMMAND
case IDM_TOOLS_COMPILE_RUN: {
    int textLen = GetWindowTextLengthW(hwnd_->hEditor_) + 1;
    std::wstring wCode(textLen, L'\0');
    GetWindowTextW(hwnd_->hEditor_, &wCode[0], textLen);
    
    std::string json = "{\"code\":\"" + WideToUTF8(wCode) + "\", \"filepath\":\"./temp.cpp\"}";
    std::string result = GenerateAnything("compile_and_run", json);
    AppendOutputText(hwnd_->hOutput_, L"[Compile] " + UTF8ToWide(result) + L"\r\n");
    return 0;
}
```

---

## Key Design Patterns

### 1. Zero-Dependency Request Routing
No JSON library. Manual parsing keeps build lean:
```cpp
std::string extract_value(const std::string& json, const std::string& key) {
    std::string k = "\"" + key + "\":";
    auto pos = json.find(k);
    if (pos == std::string::npos) return "";
    // Extract value string
}
```

### 2. Result Propagation  
All operations return messages for display:
```cpp
// Generator returns string
// IDE appends to output panel
// User sees result immediately
```

### 3. Context-Based Resource Management
Global context holds shared resources:
```cpp
class GlobalContext {
    HotPatcher* patcher;
    MemoryCore* memory;
    AgenticEngine* agent_engine;
    CPUInference::CPUInferenceEngine* inference_engine;
};
// Accessed via GlobalContext::Get() from anywhere
```

---

## Integration with External Systems

### Hotpatcher Integration
```cpp
if (GlobalContext::Get().patcher) {
    bool success = GlobalContext::Get().patcher->ApplyPatch(
        "manual_patch_" + std::to_string(address),
        (void*)address,
        bytes
    );
}
```

### Agentic Engine Integration  
```cpp
if (GlobalContext::Get().agent_engine) {
    return GlobalContext::Get().agent_engine->chat(prompt);
    // Or: performCompleteCodeAudit(code);
    // Or: getSecurityAssessment(code);
    // Or: getPerformanceRecommendations(code);
}
```

### Memory Analysis Integration
```cpp
if (GlobalContext::Get().memory) {
    return GlobalContext::Get().memory->GetStatsString();
}
```

### Model Inference Integration
```cpp
if (GlobalContext::Get().inference_engine) {
    bool success = GlobalContext::Get().inference_engine->LoadModel(path);
}
```

---

## Testing the IDE

### Build Command (Windows/MSVC)
```bash
cd D:\RawrXD
cmake -G "Visual Studio 17 2022" -B build
cmake --build build --config Release --target RawrXD-QtShell
```

### Manual Testing Flow
1. **Generate Project**: Tools → Generate Project → Enter "TestApp", "cli"
2. **Edit Code**: Open generated main.cpp
3. **Run Script**: Press F5
4. **Analyze Code**: Tools → Code Audit
5. **Check Security**: Tools → Security Check
6. **View Memory**: Tools → Memory Viewer
7. **Apply Hotpatch**: Tools → Hotpatch → Enter address & bytes

---

## Performance Optimization Tips

- **Lazy loading**: Create UI elements on-demand, not at startup
- **String caching**: Cache UTF8/Wide conversions for repeated strings
- **Async operations**: Long-running analysis should use background threads
- **Memory pooling**: Pre-allocate tab buffers to avoid fragmentation

---

## Debugging Tips

Enable output logging:
```cpp
// In AppendOutputText, add timestamp
std::wstring timestamp = L"[" + std::to_wstring(GetTickCount64()) + L"] ";
AppendOutputText(hOutput, timestamp + text);
```

Add debug breakpoints in GenerateAnything:
```cpp
if (request_type == "suspicious_type") {
    __debugbreak();  // Stops debugger here
}
```

---

## Contributing Guidelines

1. **Add request types to GenerateAnything first** - This is the "API contract"
2. **Then add IDE handler** - Connect menu item to request
3. **Then implement logic** - In the generator service
4. **Test end-to-end** - From menu click to output panel
5. **Update this guide** - Document your new feature

---

**Status**: All scaffolding complete. Ready for feature implementation.
**Contact**: Development team
**Last Updated**: Feb 4, 2026
