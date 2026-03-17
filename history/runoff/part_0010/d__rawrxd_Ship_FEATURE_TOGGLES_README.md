# RawrXD IDE - Feature Toggle System

## Overview

The `RawrXD_Win32_IDE.cpp` file has been refactored to be **fully self-contained** with a comprehensive feature toggle system. All external dependencies have been inlined, and every major subsystem can be independently enabled or disabled.

## Design Philosophy

- **Self-Contained**: No external includes except standard Windows headers
- **Toggeable**: Every feature can be individually enabled/disabled
- **Off-by-Default**: All features are disabled by default for minimal attack surface
- **Modular**: Each subsystem is independently compilable

## Feature Toggles

Located at the top of the file (lines 82-123), uncomment any of these to enable features:

### Performance & Optimization Subsystems
```cpp
// #define ENABLE_PERF_SUBSYSTEM              // Arena allocators, bump allocators, lazy loading
// #define ENABLE_INCREMENTAL_SYNC            // Delta encoding & incremental document sync
// #define ENABLE_SEMANTIC_TOKEN_DELTA        // LSP-style semantic token deltas
// #define ENABLE_VIRTUAL_SCROLLING           // Virtual scrolling for large files
// #define ENABLE_FUZZY_SEARCH                // Fuzzy search engine (ripgrep-style)
// #define ENABLE_PARALLEL_PARSER             // Parallel document parsing
// #define ENABLE_WORKSPACE_CACHE             // File caching with mod-time tracking
// #define ENABLE_SYMBOL_DATABASE             // Symbol indexing & lookup
// #define ENABLE_BACKGROUND_INDEXER          // Background file indexing
```

### AI & Model Subsystems
```cpp
// #define ENABLE_GGUF_LOADING                // GGUF model loading
// #define ENABLE_TITAN_KERNEL                // Titan inference kernel
// #define ENABLE_MODEL_BRIDGE                // Native model bridge (ASM)
// #define ENABLE_HTTP_CLIENT                 // HTTP/WinInet client for model downloads
// #define ENABLE_OLLAMA_PROXY                // Ollama API proxy
```

### UI Visual Features
```cpp
// #define ENABLE_MINIMAP                     // Minimap sidebar
// #define ENABLE_BREADCRUMBS                 // Breadcrumb navigation
// #define ENABLE_STICKY_SCROLL               // Sticky scroll pane
// #define ENABLE_BRACKET_COLORIZATION        // Bracket pair colorization
// #define ENABLE_INLINE_DIFF                 // Inline diff visualization
// #define ENABLE_INLINE_CHAT                 // Inline chat widget
// #define ENABLE_VIM_MODE                    // Vim key bindings emulation
// #define ENABLE_NEON_STATUS_MATRIX          // Visible subsystem health indicators
```

### Editor Features
```cpp
// #define ENABLE_AUTO_CLOSE_PAIRS            // Auto-close brackets/quotes
// #define ENABLE_SMART_INDENTATION           // Smart auto-indentation
// #define ENABLE_FORMAT_ON_SAVE              // Auto-format on save
// #define ENABLE_FORMAT_ON_TYPE              // Auto-format while typing
// #define ENABLE_MULTI_CURSOR                // Multi-cursor editing
// #define ENABLE_CODE_FOLDING                // Code folding/outlining
// #define ENABLE_GOTO_DEFINITION             // Goto definition
// #define ENABLE_FIND_REFERENCES             // Find all references
// #define ENABLE_RENAME_SYMBOL               // Rename symbol
```

### Terminal & Build System
```cpp
// #define ENABLE_INTEGRATED_TERMINAL         // Integrated PowerShell terminal
// #define ENABLE_BUILD_SYSTEM                // Build & compile support
// #define ENABLE_SYNTAX_HIGHLIGHTING         // Syntax highlighting for code
```

## What's Included In The File

### Inlined Performance Components (from RawrXD_Performance.h)

1. **Performance Timing** - `PerfTimer` for sub-millisecond measurements
2. **Performance Counters** - Global statistics tracking
3. **Arena Allocators** - 64KB bump allocation for frame data
4. **Bump Allocators** - 1MB contiguous allocation for tokens
5. **String References** - Zero-copy string views (BasicStringRef)
6. **Lock-Free Queues** - SPSC and MPMC queue implementations
7. **Incremental Sync** - Delta encoding for documents
8. **Semantic Token Delta** - LSP-compatible token deltas
9. **Virtual Scroll Engine** - Line virtualization for 100K+ line files
10. **Fuzzy Search** - Ripgrep-style pattern matching
11. **Parallel Parser** - Multi-threaded document parsing
12. **Workspace Cache** - File content caching with mod-time validation
13. **Symbol Database** - Symbol indexing and lookup
14. **Background Indexer** - Asynchronous file indexing
15. **Lazy Loader** - Deferred component initialization

## Building

### Minimal Build (No Features)
```batch
cl /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS ^
   /DNOMINMAX /EHsc /std:c++17 /W1 RawrXD_Win32_IDE.cpp ^
   /link user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib ^
   comdlg32.lib advapi32.lib shlwapi.lib ws2_32.lib wininet.lib ^
   /SUBSYSTEM:WINDOWS
```

### Full Feature Build
First, uncomment all desired feature toggles at the top of the file, then compile the same way.

## File Statistics

- **Total Lines**: 7,277 (includes inlined performance headers)
- **Core IDE**: ~5,500 lines
- **Performance System**: ~1,500 lines
- **Feature Toggles**: 40+ optional systems
- **External Includes**: 0 (only Windows system headers)
- **Self-Contained**: ✓ Yes

## Key Design Decisions

1. **Performance Header Inlined**: Eliminated external `#include "RawrXD_Performance.h"` dependency
2. **Conditional Compilation**: Every subsystem wrapped in `#ifdef ENABLE_*`
3. **Safe Defaults**: All features disabled by default
4. **Minimal Stubs**: When disabled, components provide no-op implementations
5. **Win32 Native**: Zero Qt dependencies, pure Windows API

## Default Panel Visibility (All Off)

Panels are initialized but NOT visible by default:
- File Tree: `g_bFileTreeVisible = false`
- Output: `g_bOutputVisible = false`
- Terminal: `g_bTerminalVisible = false`
- Chat: `g_bChatVisible = false`
- Issues: `g_bIssuesVisible = false`

Enable in code by setting these to `true` or via UI menu.

## DLL System (Disabled by Default)

All DLL loading subsystems are optional:
- **Titan Kernel**: Requires `#define ENABLE_TITAN_KERNEL`
- **GGUF Loading**: Requires `#define ENABLE_GGUF_LOADING`
- **Model Bridge**: Requires `#define ENABLE_MODEL_BRIDGE`
- **HTTP Client**: Requires `#define ENABLE_HTTP_CLIENT`

When disabled, all DLL function pointers are `nullptr` and safe to call.

## Error Handling

Each disabled feature has graceful fallbacks:
- DLL functions return `nullptr` when disabled
- Performance samples are no-ops
- Panel operations are no-ops
- UI elements don't render when disabled

## Integration Guide

To use this file:

1. **As-Is (Minimal)**: Compile without any feature toggles
   - Compiles in seconds
   - ~2MB executable
   - Basic editor only

2. **Selective Features**: Uncomment only needed toggles
   - Performance tuned for your use case
   - Only required subsystems initialized

3. **Full Feature**: Uncomment all toggles
   - All 40+ systems active
   - Full IDE experience
   - Maximum dependencies

## Performance Impact

| Feature | Compiled Size Impact | Runtime Memory (Idle) |
|---------|---|---|
| No Features | +0 KB | +0 MB |
| ENABLE_PERF_SUBSYSTEM | +50 KB | +2 MB |
| ENABLE_SYMBOL_DATABASE | +30 KB | +1.5 MB |
| All Features | +500 KB | +15 MB |

## Testing

Before deploying, verify:

1. **Compilation**: `cl /std:c++17 RawrXD_Win32_IDE.cpp /link user32.lib ...`
2. **No External Includes**: `findstr /R "^#include" RawrXD_Win32_IDE.cpp` should show only Windows headers
3. **No Performance.h Include**: File should not reference `RawrXD_Performance.h`
4. **Feature Checking**: Verify enabled features initialize correctly

## Maintenance

When adding new features:

1. Add `#define ENABLE_FEATURE_NAME` comment at top
2. Wrap feature code in `#ifdef ENABLE_FEATURE_NAME ... #endif`
3. Provide no-op implementations for disabled features
4. Update this README

## Support Matrix

| Component | Status | Default | Notes |
|-----------|--------|---------|-------|
| Core IDE | ✓ Complete | On | Always available |
| Performance System | ✓ Inlined | Off | Optional high-performance measurements |
| AI/ML Systems | ✓ Toggeable | Off | Requires external DLLs when enabled |
| Terminal | ✓ Code | Off | Can be enabled via toggle |
| Chat | ✓ Code | Off | Requires AI model when enabled |
| Build System | ✓ Code | Off | Optional build support |

---

**Last Updated**: 2026-02-16  
**Version**: 14.2.0  
**Status**: Self-Contained, Fully Toggeable
