# RawrXD IDE - Feature Toggle Quick Reference

## Quick Enable Guide

To enable any feature, find and uncomment its line near the top of `RawrXD_Win32_IDE.cpp` (lines 82-123).

### Copy-Paste Examples

**Enable just the basics:**
```cpp
#define ENABLE_SYNTAX_HIGHLIGHTING
#define ENABLE_INTEGRATED_TERMINAL
```

**Enable performance tracking:**
```cpp
#define ENABLE_PERF_SUBSYSTEM
#define ENABLE_WORKSPACE_CACHE
#define ENABLE_SYMBOL_DATABASE
#define ENABLE_BACKGROUND_INDEXER
```

**Enable all AI systems:**
```cpp
#define ENABLE_GGUF_LOADING
#define ENABLE_TITAN_KERNEL
#define ENABLE_MODEL_BRIDGE
#define ENABLE_HTTP_CLIENT
```

**Enable complete IDE (all features):**
```cpp
// Copy/paste all ENABLE_* lines and uncomment them
```

## Feature Availability

### Performance Systems (Lines 87-95)
- `ENABLE_PERF_SUBSYSTEM` - Performance measurement infrastructure
- `ENABLE_INCREMENTAL_SYNC` - Delta encoding for edits
- `ENABLE_SEMANTIC_TOKEN_DELTA` - LSP token deltas
- `ENABLE_VIRTUAL_SCROLLING` - Handle 100K+ line files
- `ENABLE_FUZZY_SEARCH` - Fast pattern matching
- `ENABLE_PARALLEL_PARSER` - Multi-threaded parsing
- `ENABLE_WORKSPACE_CACHE` - File content caching
- `ENABLE_SYMBOL_DATABASE` - Symbol indexing
- `ENABLE_BACKGROUND_INDEXER` - Async file indexing

### AI & ML Systems (Lines 98-102)
- `ENABLE_GGUF_LOADING` - Load GGUF model files
- `ENABLE_TITAN_KERNEL` - Titan inference engine
- `ENABLE_MODEL_BRIDGE` - Native model bridge
- `ENABLE_HTTP_CLIENT` - Model downloads
- `ENABLE_OLLAMA_PROXY` - Ollama API proxy

### UI Features (Lines 105-112)
- `ENABLE_MINIMAP` - Code minimap sidebar
- `ENABLE_BREADCRUMBS` - Navigation breadcrumbs
- `ENABLE_STICKY_SCROLL` - Sticky scroll pane
- `ENABLE_BRACKET_COLORIZATION` - Bracket coloring
- `ENABLE_INLINE_DIFF` - Inline diff view
- `ENABLE_INLINE_CHAT` - Inline chat widget
- `ENABLE_VIM_MODE` - Vim key bindings
- `ENABLE_NEON_STATUS_MATRIX` - System health display

### Editor Features (Lines 115-123)
- `ENABLE_AUTO_CLOSE_PAIRS` - Auto-closing brackets
- `ENABLE_SMART_INDENTATION` - Smart indent
- `ENABLE_FORMAT_ON_SAVE` - Auto-format save
- `ENABLE_FORMAT_ON_TYPE` - Format while typing
- `ENABLE_MULTI_CURSOR` - Multi-cursor editing
- `ENABLE_CODE_FOLDING` - Code folding
- `ENABLE_GOTO_DEFINITION` - Jump to definition
- `ENABLE_FIND_REFERENCES` - Find all refs
- `ENABLE_RENAME_SYMBOL` - Rename symbol

### Terminal & Build (Lines 126-128)
- `ENABLE_INTEGRATED_TERMINAL` - PowerShell terminal
- `ENABLE_BUILD_SYSTEM` - Build support
- `ENABLE_SYNTAX_HIGHLIGHTING` - Syntax colors

## Preset Configurations

### Minimal IDE
```cpp
// No toggles enabled - basic text editor only
```

### Standard IDE
```cpp
#define ENABLE_SYNTAX_HIGHLIGHTING
#define ENABLE_INTEGRATED_TERMINAL
#define ENABLE_BUILD_SYSTEM
#define ENABLE_AUTO_CLOSE_PAIRS
#define ENABLE_SMART_INDENTATION
```

### Developer IDE
```cpp
// All of Standard +
#define ENABLE_PERF_SUBSYSTEM
#define ENABLE_WORKSPACE_CACHE
#define ENABLE_SYMBOL_DATABASE
#define ENABLE_GOTO_DEFINITION
#define ENABLE_FIND_REFERENCES
#define ENABLE_BRACKET_COLORIZATION
#define ENABLE_CODE_FOLDING
#define ENABLE_MINIMAP
```

### ML/AI IDE
```cpp
// All of Developer +
#define ENABLE_GGUF_LOADING
#define ENABLE_TITAN_KERNEL
#define ENABLE_PARALLEL_PARSER
#define ENABLE_BACKGROUND_INDEXER
#define ENABLE_FUZZY_SEARCH
```

### Maximum Build (Everything)
Uncomment all `#define ENABLE_*` lines (about 40 total)

## Load Dependencies

Some features require external DLLs when enabled:

| Feature | DLL Required | Optional |
|---------|---|---|
| `ENABLE_GGUF_LOADING` | RawrXD_InferenceEngine.dll | Yes |
| `ENABLE_TITAN_KERNEL` | RawrXD_Titan_Kernel.dll | Yes |
| `ENABLE_MODEL_BRIDGE` | RawrXD_NativeModelBridge.dll | Yes |

If DLL not found, feature gracefully disables at runtime.

## Compile Command

Always use:
```batch
cl /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS ^
   /DNOMINMAX /EHsc /std:c++17 /W1 RawrXD_Win32_IDE.cpp ^
   /link user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib ^
   comdlg32.lib advapi32.lib shlwapi.lib ws2_32.lib wininet.lib ^
   /SUBSYSTEM:WINDOWS
```

## File Locations

- **Main File**: `d:\rawrxd\Ship\RawrXD_Win32_IDE.cpp` (7,277 lines)
- **Feature Toggles**: Lines 82-128
- **Performance System**: Lines 165-285 (inlined from RawrXD_Performance.h)
- **Documentation**: `FEATURE_TOGGLES_README.md`, `REFACTORING_SUMMARY.md`

## Default States (All Off)

```
✗ File Tree visible
✗ Output panel visible
✗ Terminal visible
✗ Chat visible
✗ Issues panel visible
✗ All DLLs loaded
✗ All AI systems active
✗ All performance tracking
✗ All optimizations
```

## Enable by Editing

For example, to enable terminal and syntax highlighting:

**Before:**
```cpp
// #define ENABLE_INTEGRATED_TERMINAL
// #define ENABLE_SYNTAX_HIGHLIGHTING
```

**After:**
```cpp
#define ENABLE_INTEGRATED_TERMINAL
#define ENABLE_SYNTAX_HIGHLIGHTING
```

Then rebuild with the compile command above.

## Verification

After editing toggle lines, verify:

1. **Check syntax**: No unbalanced quotes/braces
2. **Verify defines**: `findstr "#define ENABLE_" RawrXD_Win32_IDE.cpp`
3. **Compile**: Should complete without errors
4. **Test**: Run executable and verify features work

---

**Status**: ✅ All 40+ features available to enable  
**Default**: ✅ All disabled (safe, minimal)  
**Self-Contained**: ✅ Yes (no external headers)  
**Ready to Build**: ✅ Yes
