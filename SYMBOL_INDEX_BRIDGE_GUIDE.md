# Symbol Index Bridge — Integration Guide

## Overview

The **Symbol Index Bridge** connects the Rust parser output to the completion engine, enabling:
- File-scoped symbol indexing (fast lookup)
- Project-scoped cross-file resolution (workspace-wide)
- Incremental updates (only re-parse changed files)
- Thread-safe background parsing

## Architecture

```
rust_parser_v2.cpp  →  SymbolTable  →  SymbolIndexBridge  →  CompletionEngine
     (parse)             (index)          (query API)          (ghost text)
```

## Files

| File | Purpose |
|------|---------|
| `src/bridge/symbol_index_bridge.hpp` | Header with API |
| `src/bridge/symbol_index_bridge.cpp` | Implementation |
| `tests/test_symbol_index_bridge.cpp` | Unit tests |

## Quick Start

### C++ API

```cpp
#include "bridge/symbol_index_bridge.hpp"

// Create and initialize
rawrxd::bridge::SymbolIndexBridge bridge;
bridge.initialize();

// Index a file
bridge.indexFile("main.rs", source_code);

// Query completions at cursor position
auto candidates = bridge.queryCompletions("main.rs", "pri", 42, 10);

// Check trigger at cursor
auto trigger = bridge.detectTrigger(source_code, cursor_pos);
if (trigger.kind == TriggerKind::Identifier) {
    // Show completions
}
```

### C API (for FFI)

```c
#include "bridge/symbol_index_bridge.hpp"

// Create bridge
RawrXD_SymbolIndexBridge* bridge = rawrxd_symbol_index_create();

// Index file
rawrxd_symbol_index_file(bridge, "main.rs", source_code);

// Query completions
size_t count = 0;
RawrXD_SymbolCandidate* candidates = rawrxd_symbol_query_completions(
    bridge, "main.rs", "pri", 42, 10, &count);

// Use candidates...
for (size_t i = 0; i < count; i++) {
    printf("%s: %s\n", candidates[i].name, candidates[i].signature);
}

// Free and destroy
rawrxd_symbol_candidates_free(candidates, count);
rawrxd_symbol_index_destroy(bridge);
```

## Trigger Detection

The bridge detects these completion triggers:

| Trigger | Pattern | Example |
|---------|---------|---------|
| Identifier | 2+ alphanumeric chars | `pri` → `print_hello` |
| Scope Resolution | `::` | `std::` → `std::vec` |
| Method Call | `.` or `->` | `obj.` → `obj.method` |
| Type Annotation | `:` | `let x:` → `i32`, `String` |
| Attribute | `#[` | `#[` → `derive`, `test]` |

## Next Steps

1. **Ghost Text Renderer** (Phase 1c) — Win32 `DrawText`/`ExtTextOut` with faded gray suggestion
2. **Accept/Reject Handler** (Phase 1d) — `Tab` to accept, `Esc` to dismiss
3. **LSP Protocol** (Phase 2) — In-process LSP over virtual transport

## Build

```bash
cd build-ninja
ninja test_symbol_index_bridge
./test_symbol_index_bridge
```

## Status

✅ **Phase 1a COMPLETE** — Symbol index bridge implemented and tested
