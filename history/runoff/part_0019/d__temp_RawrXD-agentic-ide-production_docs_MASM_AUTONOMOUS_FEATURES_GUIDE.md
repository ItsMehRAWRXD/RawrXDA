# MASM Autonomous Features - Quick Reference

## Overview

Pure MASM64 implementation of autonomous IDE features (suggestions, security alerts, optimizations) using Windows Win32 runtime. **No Qt dependencies** - uses native Win32 API only.

## Architecture

```
autonomous_features.asm     → Core data structures and business logic
autonomous_widgets.asm      → Win32 UI widgets (RichEdit, ListView, Buttons)
autonomous_integration.inc  → Public API header (C++ interop)
test_autonomous_masm.cpp    → Test harness
```

## Build Integration

**CMakeLists.txt** (lines 2063-2080):
```cmake
# Pure MASM Autonomous Features (Windows Runtime)
if(MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    enable_language(ASM_MASM)
    list(APPEND AGENTICIDE_SOURCES src/masm_pure/autonomous_features.asm)
    list(APPEND AGENTICIDE_SOURCES src/masm_pure/autonomous_widgets.asm)
    set_source_files_properties(...
        COMPILE_FLAGS "/nologo /Zi /c /Cp /W3 /I\"${CMAKE_SOURCE_DIR}/src/masm_pure\""
    )
endif()
```

**Test executable**:
```cmake
add_executable(test_autonomous_masm
    tests/test_autonomous_masm.cpp
    src/masm_pure/autonomous_features.asm
    src/masm_pure/autonomous_widgets.asm
)
```

## Data Structures

### AutonomousSuggestion (2048 bytes)
- **suggestionId**: 64 bytes (fixed char buffer)
- **suggestionType**: 32 bytes (test_generation, optimization, refactoring, security_fix, documentation)
- **filePath**: 260 bytes (MAX_PATH)
- **lineNumber**: DWORD
- **originalCode / suggestedCode / explanation**: 512 bytes each
- **confidence**: REAL8 (0-1)
- **wasAccepted**: DWORD (TRUE/FALSE)
- **pBenefitsList**: QWORD (pointer to StringList)
- **pMetadataMap**: QWORD (pointer to KeyValueMap)

### SecurityIssue (2048 bytes)
- **issueId**: 64 bytes
- **severity**: 16 bytes (critical, high, medium, low)
- **issueType**: 64 bytes (sql_injection, xss, buffer_overflow, etc.)
- **filePath**: 260 bytes
- **lineNumber**: DWORD
- **vulnerableCode / description / suggestedFix**: 512 bytes each
- **cveReference**: 64 bytes
- **riskScore**: REAL8 (0-10)
- **pAffectedComponents**: QWORD (StringList)

### PerformanceOptimization (2048 bytes)
- **optimizationId**: 64 bytes
- **optimizationType**: 32 bytes (algorithm, caching, parallelization, memory)
- **filePath**: 260 bytes
- **lineNumber**: DWORD
- **currentImplementation / optimizedImplementation / reasoning**: 512 bytes each
- **expectedSpeedup**: REAL8 (e.g., 2.5x)
- **expectedMemorySaving**: QWORD (bytes)
- **confidence**: REAL8 (0-1)

### Widget Structures (64 bytes each)
- **SuggestionWidget**: paneID, hWnd, pSuggestion, hBtnAccept, hBtnReject, hEditPreview
- **SecurityAlertWidget**: paneID, hWnd, pIssue, hListView, hStaticSeverity, hBtnFix
- **OptimizationPanelWidget**: paneID, hWnd, pOptimization, hListView, hProgressBar, hBtnApply

## API Reference

### Autonomous Suggestion API
```cpp
extern "C" {
    void* AutonomousSuggestion_Create(
        const char* suggestionId,
        const char* type,
        const char* filePath,
        int lineNumber
    );
    
    void AutonomousSuggestion_Destroy(void* pSuggestion);
    
    int AutonomousSuggestion_SetCode(
        void* pSuggestion,
        const char* originalCode,
        const char* suggestedCode,
        const char* explanation
    );
    
    void AutonomousSuggestion_SetConfidence(
        void* pSuggestion,
        double confidence
    );
    
    void AutonomousSuggestion_Accept(void* pSuggestion);
    void AutonomousSuggestion_Reject(void* pSuggestion);
}
```

### Security Issue API
```cpp
extern "C" {
    void* SecurityIssue_Create(
        const char* issueId,
        const char* severity,
        const char* issueType,
        const char* filePath,
        int lineNumber
    );
    
    void SecurityIssue_Destroy(void* pIssue);
    
    int SecurityIssue_SetDetails(
        void* pIssue,
        const char* vulnerableCode,
        const char* description,
        const char* suggestedFix
    );
    
    void SecurityIssue_SetRiskScore(void* pIssue, double riskScore);
}
```

### Performance Optimization API
```cpp
extern "C" {
    void* PerformanceOptimization_Create(
        const char* optimizationId,
        const char* optimizationType,
        const char* filePath,
        int lineNumber
    );
    
    void PerformanceOptimization_Destroy(void* pOptimization);
    
    int PerformanceOptimization_SetImplementations(
        void* pOptimization,
        const char* currentImplementation,
        const char* optimizedImplementation,
        const char* reasoning
    );
    
    void PerformanceOptimization_SetMetrics(
        void* pOptimization,
        double expectedSpeedup,
        long long expectedMemorySaving,
        double confidence
    );
}
```

### Widget API
```cpp
extern "C" {
    void* SuggestionWidget_Create(
        HWND hParentWnd,
        void* pSuggestion,
        int x, int y,
        int width, int height
    );
    
    void SuggestionWidget_Destroy(void* pWidget);
    void SuggestionWidget_OnAccept(void* pWidget);
    void SuggestionWidget_OnReject(void* pWidget);
    
    void* SecurityAlertWidget_Create(...);     // TODO: Full implementation
    void* OptimizationPanelWidget_Create(...); // TODO: Full implementation
}
```

### Collection API
```cpp
extern "C" {
    void* StringList_Create();
    void StringList_Destroy(void* pList);
    
    void* KeyValueMap_Create();
    void KeyValueMap_Destroy(void* pMap);
}
```

## Usage Examples

### MASM Example
```asm
; Create suggestion
lea rcx, szSuggestionId
lea rdx, szType
lea r8, szFilePath
mov r9d, 42
call AutonomousSuggestion_Create
mov [pSuggestion], rax

; Set code
mov rcx, [pSuggestion]
lea rdx, szOriginalCode
lea r8, szSuggestedCode
lea r9, szExplanation
call AutonomousSuggestion_SetCode

; Set confidence
mov rcx, [pSuggestion]
movsd xmm0, real8 ptr 0.85
call AutonomousSuggestion_SetConfidence

; Create widget
mov rcx, [hMainWindow]
mov rdx, [pSuggestion]
mov r8d, 100  ; x
mov r9d, 100  ; y
push 600      ; height
push 400      ; width
call SuggestionWidget_Create

; Clean up
mov rcx, [pSuggestion]
call AutonomousSuggestion_Destroy
```

### C++ Example
```cpp
// Create suggestion
void* suggestion = AutonomousSuggestion_Create(
    "SUG-001",
    "optimization",
    "src/main.cpp",
    42
);

// Set code details
AutonomousSuggestion_SetCode(
    suggestion,
    "for (int i = 0; i < n; i++)",
    "std::for_each(begin, end, lambda)",
    "Use STL algorithm for better performance"
);

// Set confidence
AutonomousSuggestion_SetConfidence(suggestion, 0.85);

// Create widget to display
void* widget = SuggestionWidget_Create(
    hMainWnd,
    suggestion,
    100, 100,  // x, y
    400, 600   // width, height
);

// Accept suggestion
SuggestionWidget_OnAccept(widget);

// Clean up
SuggestionWidget_Destroy(widget);
AutonomousSuggestion_Destroy(suggestion);
```

## Win32 UI Integration

### Control IDs (autonomous_widgets.asm)
```
IDC_SUGGESTION_PREVIEW      = 5001
IDC_SUGGESTION_BTN_ACCEPT   = 5002
IDC_SUGGESTION_BTN_REJECT   = 5003
IDC_SECURITY_LISTVIEW       = 5010
IDC_SECURITY_BTN_FIX        = 5011
IDC_OPTIMIZATION_LISTVIEW   = 5020
IDC_OPTIMIZATION_BTN_APPLY  = 5022
```

### Message Handling
Widgets use standard Win32 message loop with `WM_COMMAND` for button clicks:
- **BN_CLICKED** on Accept button → `SuggestionWidget_OnAccept`
- **BN_CLICKED** on Reject button → `SuggestionWidget_OnReject`
- **BN_CLICKED** on Fix button → Apply security fix
- **BN_CLICKED** on Apply button → Apply optimization

### Widget Layout (SuggestionWidget)
```
+------------------------------------+
|  Autonomous Suggestion             |
|  +------------------------------+  |
|  |  RichEdit Preview            |  |
|  |  (70% height)                |  |
|  |  Shows suggestedCode         |  |
|  +------------------------------+  |
|  [Accept]          [Reject]        |
|  (30% height)                      |
+------------------------------------+
```

## Global State

### Widget Arrays (autonomous_features.asm)
```asm
g_SuggestionWidgets     dd 32 dup(0)  ; Max 32 suggestion widgets
g_SuggestionCount       dd 0

g_SecurityAlertWidgets  dd 32 dup(0)  ; Max 32 security alert widgets
g_SecurityAlertCount    dd 0

g_OptimizationWidgets   dd 32 dup(0)  ; Max 32 optimization widgets
g_OptimizationCount     dd 0
```

### Theme Colors
```asm
g_clrSuggestionAccent   dd 0x007ACC   ; Blue for suggestions
g_clrSecurityCritical   dd 0xFF0000   ; Red for critical security
g_clrSecurityHigh       dd 0xFF8800   ; Orange for high security
g_clrSecurityMedium     dd 0xFFFF00   ; Yellow for medium
g_clrSecurityLow        dd 0x00FF00   ; Green for low
g_clrOptimizationGood   dd 0x008000   ; Dark green for optimizations
```

## Runtime Conventions

Follows **qt_pane_system.asm** and **ui_masm.asm** patterns:
- **Memory allocation**: `GlobalAlloc` with `GMEM_FIXED | GMEM_ZEROINIT`
- **String handling**: Fixed-size buffers (64, 260, 512 bytes) with truncation
- **Error handling**: Return NULL on allocation failure, check pointers before use
- **Window creation**: `CreateWindowExA` with `WS_CHILD | WS_VISIBLE`
- **Control creation**: Standard Win32 controls (RichEdit, Button, ListView)

## Testing

### Build and Run Test
```powershell
cd d:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\build
cmake --build . --target test_autonomous_masm --config Release
.\tests\Release\test_autonomous_masm.exe
```

### Test Coverage
- ✅ Autonomous Suggestion Lifecycle
- ✅ Security Issue Lifecycle
- ✅ Performance Optimization Lifecycle
- ✅ Collection Management (StringList, KeyValueMap)
- ✅ Multiple Suggestions Stress Test (10 instances)
- ✅ Null Pointer Safety
- ✅ Long String Truncation Handling

## Implementation Status

### ✅ Complete
- **autonomous_features.asm**: All core data structures and procedures
- **autonomous_widgets.asm**: SuggestionWidget (full implementation)
- **autonomous_integration.inc**: Public API header
- **test_autonomous_masm.cpp**: Full test suite
- **CMakeLists.txt**: Build integration

### 🔄 Partial (Stubs)
- **SecurityAlertWidget_Create**: Stub (returns NULL)
- **OptimizationPanelWidget_Create**: Stub (returns NULL)
- TODO: Implement ListView + severity color coding
- TODO: Implement ProgressBar for speedup visualization

### 📋 Future Enhancements
- Implement full ListView with columns and sorting
- Add syntax highlighting to RichEdit preview
- Implement drag-and-drop for widget repositioning
- Add keyboard shortcuts (Ctrl+Enter = Accept, Esc = Reject)
- Persist accepted/rejected suggestions to disk
- Integrate with editor's undo/redo stack
- Add telemetry for suggestion acceptance rates

## Compatibility

- **Platform**: Windows x64 only (MASM64)
- **Compiler**: MSVC 2022 (ml64.exe assembler)
- **Runtime**: Native Win32 API (kernel32, user32, gdi32, comctl32)
- **Dependencies**: None (no Qt, no C++ STL in MASM modules)
- **C++ Interop**: `extern "C"` linkage for all public functions

## File Locations

```
d:\temp\RawrXD-agentic-ide-production\
├── src\masm_pure\
│   ├── autonomous_features.asm       (1200 lines)
│   ├── autonomous_widgets.asm        (800 lines)
│   └── autonomous_integration.inc    (200 lines)
├── tests\
│   └── test_autonomous_masm.cpp      (300 lines)
└── RawrXD-ModelLoader\
    └── CMakeLists.txt                (build integration)
```

## Performance Characteristics

- **Struct allocation**: `GlobalAlloc` (heap allocation, ~100-200 cycles)
- **String copy**: Manual loop with length limit (~10 cycles per char)
- **Widget creation**: Win32 `CreateWindowExA` (~10,000 cycles)
- **Memory footprint**: 2KB per suggestion/issue/optimization
- **Widget limit**: 32 of each type (hard-coded arrays)

## References

- **Win32 API**: kernel32.lib, user32.lib, gdi32.lib, comctl32.lib
- **MASM64 Syntax**: Microsoft Macro Assembler x64 Reference
- **Runtime patterns**: `qt_pane_system.asm`, `ui_masm.asm` (existing codebase)
- **Test framework**: Custom C++ test harness (test_autonomous_masm.cpp)

---

**Last Updated**: December 2025  
**Author**: AI-Assisted Development  
**Status**: Production-ready (core features), Partial (widgets)
