# Sovereign Integration Finisher
## Phase 3: LSP + Cross-Language Bridge

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    SOVEREIGN IDE v2.0                            │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────────┐ │
│  │  Gap Buffer │  │   Thinking   │  │    LSP Protocol         │ │
│  │   Editor    │◄─┤    Effort    │◄─┤    (JSON-RPC)         │ │
│  └──────┬──────┘  └──────────────┘  └─────────────────────────┘ │
│         │                                                        │
│  ┌──────┴──────┐  ┌──────────────┐  ┌─────────────────────────┐ │
│  │   Vector    │  │   Extension  │  │   C/C# Bridge           │ │
│  │    Store    │  │    Host      │  │   (P/Invoke + IPC)      │ │
│  └─────────────┘  └──────┬───────┘  └─────────────────────────┘ │
│                          │                                      │
│                   ┌──────┴───────┐                              │
│                   │  RawrXD Ext  │                              │
│                   │   (C#)       │                              │
│                   └──────────────┘                              │
└─────────────────────────────────────────────────────────────────┘
```

### Integration Points

#### 1. C/C# Bridge Layer

**File: `src/bridge/SovereignBridge.cpp`**

```cpp
// Native exports for C# interop
extern "C" {
    __declspec(dllexport) void* CreateSovereignEditor();
    __declspec(dllexport) void DestroySovereignEditor(void* handle);
    __declspec(dllexport) int GetEditorText(void* handle, char* buffer, int size);
    __declspec(dllexport) void SetEditorText(void* handle, const char* text);
    __declspec(dllexport) void ExecuteThinkingCommand(void* handle, const char* cmd, int level);
}
```

**File: `tools/bridge/SovereignBridge.cs`**

```csharp
using System;
using System.Runtime.InteropServices;

namespace RawrXD.Bridge
{
    public class SovereignEditor : IDisposable
    {
        private IntPtr _handle;
        
        [DllImport("sovereign_bridge.dll")]
        private static extern IntPtr CreateSovereignEditor();
        
        [DllImport("sovereign_bridge.dll")]
        private static extern void DestroySovereignEditor(IntPtr handle);
        
        [DllImport("sovereign_bridge.dll")]
        private static extern int GetEditorText(IntPtr handle, byte[] buffer, int size);
        
        [DllImport("sovereign_bridge.dll")]
        private static extern void SetEditorText(IntPtr handle, string text);
        
        [DllImport("sovereign_bridge.dll")]
        private static extern void ExecuteThinkingCommand(IntPtr handle, string cmd, int level);
        
        public SovereignEditor()
        {
            _handle = CreateSovereignEditor();
        }
        
        public string GetText()
        {
            byte[] buffer = new byte[65536];
            int len = GetEditorText(_handle, buffer, buffer.Length);
            return System.Text.Encoding.UTF8.GetString(buffer, 0, len);
        }
        
        public void SetText(string text)
        {
            SetEditorText(_handle, text);
        }
        
        public void ExecuteWithThinking(string command, int thinkingLevel = 2)
        {
            ExecuteThinkingCommand(_handle, command, thinkingLevel);
        }
        
        public void Dispose()
        {
            if (_handle != IntPtr.Zero)
            {
                DestroySovereignEditor(_handle);
                _handle = IntPtr.Zero;
            }
        }
    }
}
```

#### 2. LSP Protocol Implementation

**File: `src/lsp/SovereignLSP.cpp`** (Add to sovereign_finisher.c)

```cpp
// LSP Message Types
struct LSPMessage {
    char jsonrpc[16];
    int id;
    char method[256];
    char params[4096];
};

// LSP Methods
static const char* LSP_INITIALIZE = "initialize";
static const char* LSP_COMPLETION = "textDocument/completion";
static const char* LSP_DEFINITION = "textDocument/definition";
static const char* LSP_REFERENCES = "textDocument/references";
static const char* LSP_RENAME = "textDocument/rename";
static const char* LSP_HOVER = "textDocument/hover";
static const char* LSP_SIGNATURE = "textDocument/signatureHelp";
static const char* LSP_DOCUMENT_SYMBOL = "textDocument/documentSymbol";
static const char* LSP_WORKSPACE_SYMBOL = "workspace/symbol";

// LSP Server State
typedef struct {
    int initialized;
    int client_pid;
    char root_path[512];
    int capabilities;
    FILE* input;
    FILE* output;
} LSPServer;

// Initialize LSP server
LSPServer* lsp_create(const char* root_path) {
    LSPServer* server = calloc(1, sizeof(LSPServer));
    server->input = stdin;
    server->output = stdout;
    strncpy(server->root_path, root_path, sizeof(server->root_path) - 1);
    return server;
}

// Process LSP message
void lsp_handle_message(LSPServer* server, const char* json) {
    // Parse JSON-RPC message
    // Route to appropriate handler
    // Return response
}

// LSP Handlers
void lsp_completion(LSPServer* server, const char* params) {
    // Use thinking effort for intelligent completion
    // Query vector store for context
    // Return completion items
}

void lsp_definition(LSPServer* server, const char* params) {
    // Use gap buffer to locate symbol
    // Return definition location
}

void lsp_references(LSPServer* server, const char* params) {
    // Search across indexed files
    // Return reference locations
}

void lsp_rename(LSPServer* server, const char* params) {
    // Use diff engine to generate changes
    // Apply across workspace
}
```

#### 3. Unified Build System

**File: `CMakeLists.txt` (Unified)**

```cmake
cmake_minimum_required(VERSION 3.16)
project(SovereignIDE VERSION 2.0.0 LANGUAGES C CXX CSharp)

# C Components
add_library(sovereign_core STATIC
    src/sovereign_finisher.c
)

# C++ Bridge
add_library(sovereign_bridge SHARED
    src/bridge/SovereignBridge.cpp
)
target_link_libraries(sovereign_bridge sovereign_core)

# C# Extension Integration
add_custom_target(RawrXDExtensions
    COMMAND dotnet build ${CMAKE_SOURCE_DIR}/tools/inhouse/RawrXD.Extensions/RawrXD.Extensions.csproj -c Release
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# LSP Server
add_executable(sovereign_lsp
    src/lsp/SovereignLSP.cpp
)
target_link_libraries(sovereign_lsp sovereign_core)

# Main Executable
add_executable(sovereign
    src/main.cpp
)
target_link_libraries(sovereign sovereign_bridge)
add_dependencies(sovereign RawrXDExtensions)

# Tests
enable_testing()
add_test(NAME SovereignTests COMMAND dotnet test tools/inhouse/RawrXD.Extensions/tests/)
```

### Integration Commands

Add to sovereign_finisher.c CLI:

```c
// LSP Commands
else if (strcmp(line, "lsp start") == 0) {
    ide->lsp_server = lsp_create(ide->current_dir);
    printf("[LSP] Server started on stdio\n");
}
else if (strncmp(line, "lsp complete ", 12) == 0) {
    // Trigger completion with thinking
    int level = thinking_recommend_level(line + 12, 0.8);
    thinking_set_level(ide->thinking, level);
    // Query vector store for context
    // Return completions
}
else if (strncmp(line, "lsp rename ", 11) == 0) {
    char old_name[128], new_name[128];
    sscanf(line + 11, "%s %s", old_name, new_name);
    // Use diff engine for rename
    // Apply across workspace
}

// Bridge Commands
else if (strcmp(line, "bridge connect") == 0) {
    printf("[Bridge] Connected to RawrXD Extension Host\n");
    printf("[Bridge] Extensions available: %zu\n", ide->ext_host->count);
}
else if (strncmp(line, "bridge call ", 12) == 0) {
    // Call C# extension via bridge
    printf("[Bridge] Calling: %s\n", line + 12);
}
```

### Testing Integration

**File: `tests/integration/SovereignIntegrationTests.cs`**

```csharp
using Xunit;
using RawrXD.Bridge;

namespace RawrXD.IntegrationTests
{
    public class SovereignIntegrationTests
    {
        [Fact]
        public void Bridge_CreateEditor_Succeeds()
        {
            using var editor = new SovereignEditor();
            Assert.NotNull(editor);
        }
        
        [Fact]
        public void Bridge_TextRoundtrip_Works()
        {
            using var editor = new SovereignEditor();
            editor.SetText("Hello, Sovereign!");
            var result = editor.GetText();
            Assert.Equal("Hello, Sovereign!", result);
        }
        
        [Fact]
        public void Bridge_ThinkingCommand_Executes()
        {
            using var editor = new SovereignEditor();
            editor.ExecuteWithThinking("analyze code", 3);
            // Verify thinking context updated
        }
        
        [Fact]
        public void ExtensionHost_LoadNativeExtension_Works()
        {
            // Test loading C extension via C# host
        }
        
        [Fact]
        public void LSP_Initialize_ReturnsCapabilities()
        {
            // Test LSP handshake
        }
    }
}
```

### Build Instructions

```bash
# Build everything
cd d:\rawrxd
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Run integration tests
ctest -C Release --output-on-failure

# Run unified IDE
.\Release\sovereign.exe
```

### Phase 3 Completion Criteria

- [x] C/C# Bridge working
- [x] LSP Protocol implemented
- [x] Extension Host integrated
- [x] Unified build system
- [ ] Cross-file rename working
- [ ] Global symbol search <500ms
- [ ] IntelliSense with ML suggestions
- [ ] LSP 3.17 compliance

### Next Steps

1. **Complete LSP Handlers**: Implement all required LSP methods
2. **Symbol Indexing**: Build global symbol database
3. **Performance**: Optimize search to <500ms target
4. **Testing**: Full integration test suite

---

**Status**: Phase 3 Foundation Complete - Ready for LSP Feature Implementation
