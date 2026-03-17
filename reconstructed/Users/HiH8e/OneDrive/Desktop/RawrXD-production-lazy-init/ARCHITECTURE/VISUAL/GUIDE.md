# RawrXD MASM IDE - Architecture & Dependency Diagrams

## System Architecture Flow

```
┌─────────────────────────────────────────────────────────────┐
│              WinMain (main_complete.asm)                    │
│                   ▲                                         │
│         Entry Point / Initialization                        │
└────────┬────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│              Editor_Init(hInstance)                         │
│         1. COM Initialize                                  │
│         2. Create main window                              │
│         3. Set up event loop                               │
└────────┬────────────────────────────────────────────────────┘
         │
         ├──────────────────────────┬──────────────────────────┐
         │                          │                          │
         ▼                          ▼                          ▼
    ┌────────┐          ┌──────────────────┐      ┌──────────────┐
    │EDITOR  │          │   UI LAYER       │      │BACKEND       │
    │ CORE   │          │ (menu,toolbar,   │      │(models,      │
    │        │          │  status,tabs)    │      │ agents)      │
    └────────┘          └──────────────────┘      └──────────────┘
         │                      │                        │
    Text I/O                 User Input            Data Processing
    Rendering                Navigation            Model Loading
    Cursors                  Feedback              Inference
```

## Critical Path to MVP

```
1. main_complete.asm  (6-8h)  ──┐
                                ├─► Working IDE
2. dialogs.asm        (8-10h) ─┤
                                ├─► (2-3 weeks)
3. build_release.ps1  (2-4h)  ─┤
                                ├─► File I/O
4. Wire integration   (6-8h)  ──┤

                                └─► Search/Replace
5. find_replace.asm   (6-8h)
```

## File Categories & Count

```
BEFORE CLEANUP          AFTER CLEANUP
─────────────────────   ────────────────────
~200 .asm files         ~45 .asm files
~250K LOC               ~100K LOC
~80 duplicates/tests    Clean organization
5-10 min builds         2-3 min builds
Confusing purpose       Clear separation
```

## Build Pipeline

```
SOURCE FILES (200)
       │
       ├─► ml.exe /c (compile)
       │
       ▼
OBJECT FILES (57)
       │
       ├─► link.exe (unified linker)
       │
       ▼
RawrXD.exe (SINGLE EXECUTABLE)
       │
       └─► Run IDE!
```

## Module Dependencies

```
main_complete.asm
    ├── Editor_Init() ──► editor_enterprise.asm
    ├── Editor_Create() ─┬─► window.asm
    │                   ├─► menu_system.asm
    │                   ├─► toolbar.asm
    │                   └─► pane_layout.asm
    │
    └── Message Loop ───┬─► WM_COMMAND handlers
                        ├─► File dialogs
                        ├─► Menu processing
                        └─► Window rendering

File > Open/Save
    ├─► dialogs.asm
    ├─► editor_enterprise.asm
    └─► config_manager.asm (recent files)

Find/Replace
    ├─► find_replace.asm
    ├─► editor_enterprise.asm
    └─► syntax_highlighting.asm

Backend Integration
    ├─► gguf_loader_unified.asm
    ├─► ollama_client.asm
    ├─► lsp_client.asm
    └─► agentic_loop.asm
```

## Success Metrics Timeline

```
Week 1: BUILD SYSTEM (Days 1-5)
  ✅ Day 1: main_complete.asm + dialogs.asm compiling
  ✅ Day 2: build_release.ps1 producing EXE
  ✅ Day 3: RawrXD.exe runs, shows window
  ✅ Day 4: Menu system responding
  ✅ Day 5: File > Open/Save working

Week 2: FILE I/O & SEARCH (Days 6-10)
  ✅ Day 6: Can load text files
  ✅ Day 7: Can save text files
  ✅ Day 8: Find/Replace basic
  ✅ Day 9: All shortcuts working
  ✅ Day 10: File explorer integrated

Week 3: INTEGRATION & POLISH (Days 11-15)
  ✅ Day 11: Backend models loading
  ✅ Day 12: Chat interface responsive
  ✅ Day 13: LSP diagnostics showing
  ✅ Day 14: Full testing
  ✅ Day 15: Production ready

```

---

This visual summary paired with the 3 detailed audit documents gives you:
1. What exists (audit)
2. What's needed (checklist)
3. How to do it (implementation)
4. Why it matters (architecture)

Ready to build! 🚀

