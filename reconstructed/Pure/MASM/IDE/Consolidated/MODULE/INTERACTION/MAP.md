# Pure MASM IDE - Module Interaction & Dependency Map

## 🔗 Complete Module Dependency Graph

```
                        WINDOWS API LAYER
                    (kernel32, user32, etc.)
                             ▲
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
   asm_hotpatch_    asm_memory.asm         asm_sync.asm
   integration.asm      (HeapAlloc/        (Mutex/
                        HeapFree)          Atomic)
        │                    │                    │
        └────────────────────┼────────────────────┘
                             │
                    RUNTIME FOUNDATION
                             │
         ┌───────────────────┼───────────────────┐
         │                   │                   │
    asm_log.asm         asm_string.asm      asm_events.asm
    (Logging)           (UTF-8/16)          (Event Loop)
         │                   │                   │
         └───────────────────┼───────────────────┘
                             │
                  HOTPATCH ENGINE LAYER
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
    model_memory_     byte_level_          gguf_server_
    hotpatch.asm      hotpatcher.asm       hotpatch.asm
    (RAM Direct)      (Binary GGUF)        (Inference)
        │                    │                    │
        └────────────────────┼────────────────────┘
                             │
               unified_hotpatch_manager.asm
                  (Coordinator/Dispatcher)
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
    proxy_hotpatcher  agentic_failure_    agentic_puppeteer.asm
    .asm (Proxy)       detector.asm       (Response Fix)
                       (Pattern Detect)
        │                    │                    │
        └────────────────────┼────────────────────┘
                             │
            RawrXD_AgenticPatchOrchestrator
                   (Agentic Coordinator)
                             │
                             ▼
                  RAWRXD IDE APPLICATION LAYER
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
    rawrxd_main.asm      rawrxd_editor.asm    rawrxd_menu.asm
    (Entry/Init)         (Text Editor)        (Menu Bar)
        │                    │                    │
        ├──────────────────┬─┴─┬──────────────┬───┤
        │                  │   │              │   │
    rawrxd_       text_gap    text_        text_  │
    wndproc        buffer     search      tokenizer
                              │           
        ┌───────────────┬─────┴─────┬──────────────┐
        │               │           │              │
    rawrxd_       raw rxd_      raw rxd_      raw rxd_
    projecttree   toolbar       statusbar     output.asm
                                           
        │                                      │
        ├──────────────────┬──────────────────┤
        │                  │                  │
    rawrxd_         rawrxd_           rawrxd_
    fileops.asm     debug.asm         build.asm
                                      
        │                  │                  │
        └──────────────────┼──────────────────┘
                           │
                    rawrxd_settings.asm
                    rawrxd_syntax.asm
                    theme_system.asm
                    session_management.asm
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
    lsp_client.asm   agent_ipc_bridge  rawrxd_shell.asm
    (LSP Support)   (IPC to Agentic)   (Terminal)
        │                  │                  │
        └──────────────────┴──────────────────┘
                           │
                      TEST HARNESS
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
    masm_test_    asm_test_main.asm   RawrXD_Dual
    main.asm                         EngineStreamer
    (Comprehensive)
```

---

## 📊 Module Cross-Reference Table

| Module | Type | Dependencies | Dependents | Purpose |
|--------|------|--------------|-----------|---------|
| **asm_memory.asm** | Foundation | Win32 APIs | All | Heap allocation with metadata |
| **asm_sync.asm** | Foundation | Win32 APIs | Hotpatch, Agentic | Thread synchronization |
| **asm_log.asm** | Foundation | Win32 APIs | All | Structured logging |
| **asm_string.asm** | Foundation | asm_memory | IDE, Agentic | String handling |
| **asm_events.asm** | Foundation | Win32 APIs | IDE, Agentic | Event dispatch |
| **asm_hotpatch_integration.asm** | Infrastructure | Win32 APIs | Hotpatch layers | Win32 API wrapper |
| **model_memory_hotpatch.asm** | Hotpatch | asm_memory, asm_sync | unified_manager | Direct RAM patching |
| **byte_level_hotpatcher.asm** | Hotpatch | asm_memory, asm_string | unified_manager | GGUF binary patching |
| **gguf_server_hotpatch.asm** | Hotpatch | asm_memory | unified_manager | Server patching |
| **unified_hotpatch_manager.asm** | Hotpatch | All hotpatch layers | Agentic, IDE | Coordinator |
| **proxy_hotpatcher.asm** | Agentic | asm_memory | orchestrator | Output patching |
| **agentic_failure_detector.asm** | Agentic | asm_string | orchestrator | Failure detection |
| **agentic_puppeteer.asm** | Agentic | asm_string | orchestrator | Response correction |
| **RawrXD_AgenticPatchOrchestrator.asm** | Agentic | Agentic modules | IDE, IPC bridge | Coordination |
| **rawrxd_main.asm** | IDE | Win32 APIs | All IDE components | Main window |
| **rawrxd_editor.asm** | IDE | text_*, Win32 | IDE system | Editor core |
| **text_gapbuffer.asm** | IDE | asm_memory | rawrxd_editor | Text storage |
| **text_renderer.asm** | IDE | Win32 APIs | rawrxd_editor | Text rendering |
| **text_search.asm** | IDE | asm_string | rawrxd_editor | Find/replace |
| **text_tokenizer.asm** | IDE | asm_string | rawrxd_syntax | Tokenization |
| **text_undoredo.asm** | IDE | asm_memory | rawrxd_editor | Undo/redo stack |
| **rawrxd_projecttree.asm** | IDE | rawrxd_fileops | rawrxd_main | Project explorer |
| **rawrxd_fileops.asm** | IDE | Win32 APIs | projecttree, explorer | File I/O |
| **rawrxd_menu.asm** | IDE | Win32 APIs | rawrxd_main | Menu bar |
| **rawrxd_toolbar.asm** | IDE | Win32 APIs | rawrxd_main | Toolbar |
| **rawrxd_statusbar.asm** | IDE | Win32 APIs | rawrxd_main | Status bar |
| **rawrxd_output.asm** | IDE | Win32 APIs | rawrxd_main | Output panel |
| **rawrxd_shell.asm** | IDE | Win32 APIs | rawrxd_main | Terminal |
| **rawrxd_debug.asm** | IDE | Win32 APIs | rawrxd_main | Debugger |
| **rawrxd_build.asm** | IDE | Win32 APIs | rawrxd_main | Build system |
| **rawrxd_settings.asm** | IDE | asm_memory | rawrxd_main | Preferences |
| **rawrxd_syntax.asm** | IDE | text_tokenizer | rawrxd_editor | Syntax highlighting |
| **theme_system.asm** | IDE | Win32 APIs | All IDE | Theming/colors |
| **session_management.asm** | IDE | asm_memory, Win32 | rawrxd_main | Session persistence |
| **lsp_client.asm** | IDE | Win32 APIs | rawrxd_editor | LSP support |
| **agent_ipc_bridge.asm** | IDE | Win32 APIs | orchestrator | Agent coordination |
| **rawrxd_wndproc.asm** | IDE | Win32 APIs | rawrxd_main | Message handling |
| **rawrxd_utils.asm** | IDE | Various | All IDE | Utility functions |

---

## 🔄 Data Flow Patterns

### Hotpatch Request Flow
```
User Request (IDE)
    ↓
unified_hotpatch_manager.applyPatch()
    ↓ (Qt signal)
┌───────────────────────────────┐
│                               │
v                               v
model_memory_hotpatch       byte_level_hotpatcher
(RAM modification)          (Binary modification)
│                               │
└───────────────────────────────┘
    ↓
gguf_server_hotpatch
(Server transformation)
    ↓
Return PatchResult (success/failure/details)
    ↓
errorOccurred() signal → IDE status update
```

### Agentic Failure Recovery Flow
```
Agent Response Received
    ↓
agentic_failure_detector.detectFailure()
    ↓
Pattern matching (refusal/hallucination/timeout/resource/safety)
    ↓
Confidence scoring (0.0-1.0)
    ↓
IF confidence > threshold:
    ↓
RawrXD_AgenticPatchOrchestrator
    ├─ agentic_puppeteer.correctResponse()
    ├─ proxy_hotpatcher.patchOutput()
    └─ Apply token logit bias if needed
    ↓
Corrected response → IDE display
```

### IDE Text Editing Flow
```
User types character
    ↓
rawrxd_editor.onChar()
    ↓
text_gapbuffer.insertChar()
    ↓
text_undoredo.recordChange()
    ↓
text_tokenizer.retokenize()
    ↓
rawrxd_syntax.highlightTokens()
    ↓
text_renderer.render()
    ↓
Screen update via Win32 DrawText/GDI
```

---

## 🚀 Module Initialization Sequence

```
1. Win32 GetProcessHeap() → asm_get_process_heap()
2. asm_log_init() → Configure logging output
3. asm_mutex_create() → Initialize sync primitives
4. rawrxd_application.init()
   ├─ theme_system.init()
   ├─ session_management.restore()
   ├─ rawrxd_main.createMainWindow()
   │  ├─ text_gapbuffer.create()
   │  ├─ rawrxd_projecttree.load()
   │  ├─ rawrxd_editor.init()
   │  ├─ lsp_client.connect()
   │  └─ agent_ipc_bridge.init()
   └─ RawrXD_AgenticPatchOrchestrator.init()
      ├─ agentic_failure_detector.init()
      ├─ agentic_puppeteer.init()
      └─ unified_hotpatch_manager.init()
        ├─ model_memory_hotpatch.init()
        ├─ byte_level_hotpatcher.init()
        └─ gguf_server_hotpatch.init()
5. rawrxd_main.messageLoop() → Main event dispatcher
```

---

## 💾 Memory Layout

### Heap Allocator Metadata Structure
```
[Offset -32] Magic      (qword) = 0xDEADBEEFCAFEBABE
[Offset -24] Alignment  (qword) = 16/32/64
[Offset -16] Requested  (qword) = user-requested size
[Offset -8]  Total      (qword) = total allocated (metadata + data + padding)
[Offset  0]  USER DATA  ← pointer returned to caller (16/32/64 byte aligned)
```

### String Handle Structure
```
[Offset -40] Magic      (qword) = 0xABCDEF0123456789
[Offset -32] Length     (qword) = character count
[Offset -24] Capacity   (qword) = allocated bytes
[Offset -16] Encoding   (byte)  = 8 (UTF-8) or 16 (UTF-16)
[Offset -9]  [7 padding]
[Offset  0]  Data       ← pointer returned to caller
```

---

## 🧪 Testing Strategy

### Allocator Tests (masm_hotpatch_test.exe)
- ✅ Basic malloc/free
- ✅ Realloc grow (64 → 256 bytes, alignment preserved)
- ✅ Realloc shrink (256 → 64 bytes)
- ✅ Realloc NULL (malloc behavior)
- ✅ Free NULL (no-op)

### Hotpatcher Tests
- Memory layer hotpatch application
- Byte-level pattern matching
- Server request/response transformation
- Unified manager coordination

### IDE Tests (Planned)
- Text editor gap buffer operations
- Project tree file operations
- Menu/toolbar event handling
- Debug breakpoint management
- Build system integration

### Agentic Tests (Planned)
- Failure detection accuracy
- Response correction functionality
- IPC bridge coordination
- Proxy output patching

---

## ✅ Build Status

**Last Successful Build**: December 25, 2025
- masm_runtime.lib ✅
- masm_hotpatch_core.lib ⚠️ (blocked by RawrXD_DualEngineStreamer.asm syntax)
- masm_agentic.lib ✅
- masm_hotpatch_test.exe ✅ (all allocator/sync/string tests pass)

**Blockers**:
1. RawrXD_DualEngineStreamer.asm line 108 - MASM syntax error (hex constant)
2. IDE components not yet compiled (no C++ wrapper)

---

## 📝 Notes & Observations

1. **Pure MASM Implementation** - All code is native x64 assembly, no C/C++ dependencies
2. **Win32-Only** - All system calls are Windows API (kernel32.dll, user32.dll)
3. **Thread-Safe** - QMutex usage throughout (or native Win32 mutex equivalent)
4. **Metadata Everywhere** - Custom allocator tracks allocation details for validation
5. **Structured Error Handling** - Result structs instead of exceptions
6. **Signal/Slot Architecture** - Qt-style async event propagation via signals
7. **Factory Methods** - ::ok() / ::error() pattern for semantic clarity

---

End of Module Interaction Map
Generated: December 25, 2025
