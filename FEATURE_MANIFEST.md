# RawrXD IDE — Complete Feature Manifest

**Generated**: 2026-02-07 20:27:58
**Source**: Auto-introspected from actual source code by RawrXD-FeatureTest.ps1

## Coverage Summary

| IDE Variant | REAL | PARTIAL | MISSING | Coverage |
|------------|------|---------|---------|----------|
| Win32 (C++) | 69 | 2 | 14 | 81% |
| CLI Shell | 36 | 0 | 49 | 42% |
| React IDE | 18 | 0 | 66 | 21% |
| PowerShell | 67 | 0 | 38 | 64% |

## Feature Matrix

| Category | Feature | Win32 | CLI | React | PS |
|----------|---------|-------|-----|-------|----|
| Agent | Agent Execute | ✅ | ✅ | ✅ | ✅ |
| Agent | Agent Goal | ✅ | ✅ | ❌ | ❌ |
| Agent | Agent Loop | ✅ | ✅ | ✅ | ✅ |
| Agent | Agent Memory | ❌ | ✅ | ❌ | ❌ |
| Agent | Agent Stop | ✅ | ❌ | ❌ | ✅ |
| Agent | Exec Governor | ✅ | ❌ | ❌ | ❌ |
| Agent | Failure Detect | ✅ | ❌ | ❌ | ✅ |
| Agent | Plan Executor | ✅ | ❌ | ❌ | ✅ |
| AI Mode | Context Window | ✅ | ✅ | ✅ | ✅ |
| AI Mode | Deep Research | ✅ | ✅ | ✅ | ❌ |
| AI Mode | Deep Thinking | ✅ | ✅ | ✅ | ❌ |
| AI Mode | No Refusal | ✅ | ❌ | ✅ | ❌ |
| Autonomy | Rate Limit | ❌ | ✅ | ❌ | ❌ |
| Autonomy | Set Goal | ✅ | ✅ | ❌ | ❌ |
| Autonomy | Toggle | 🔶 | ✅ | ❌ | ❌ |
| Build | CLI Engine Binary | — | ✅ | — | — |
| Build | Win32 IDE Binary | ✅ | — | — | — |
| Debug | Breakpoints | ✅ | ✅ | ❌ | ❌ |
| Debug | Native DbgEng | ❌ | ❌ | ❌ | ❌ |
| Debug | Start Debug | ✅ | ✅ | ❌ | ❌ |
| Debug | Step Over | ✅ | ✅ | ❌ | ❌ |
| Decompiler | D2D Split View | ✅ | ❌ | ❌ | ❌ |
| Decompiler | SSA Var Rename | ✅ | ❌ | ❌ | ❌ |
| Decompiler | Sync Selection | ✅ | ❌ | ❌ | ❌ |
| Decompiler | Syntax Coloring | ✅ | ❌ | ❌ | ❌ |
| Editing | Copy | ✅ | ✅ | ❌ | ✅ |
| Editing | Cut | ✅ | ✅ | ❌ | ✅ |
| Editing | Find | ✅ | ✅ | ❌ | ✅ |
| Editing | Paste | ✅ | ✅ | ❌ | ✅ |
| Editing | Redo | ✅ | ✅ | ❌ | ✅ |
| Editing | Replace | ✅ | ✅ | ❌ | ✅ |
| Editing | Undo | ✅ | ✅ | ❌ | ✅ |
| File Ops | Close File | ✅ | ✅ | ❌ | ✅ |
| File Ops | Load Model | ✅ | ❌ | ✅ | ✅ |
| File Ops | Model HF | ✅ | ❌ | ❌ | ❌ |
| File Ops | New File | ✅ | ✅ | ❌ | ✅ |
| File Ops | Open File | ✅ | ✅ | ❌ | ✅ |
| File Ops | Recent Files | ✅ | ❌ | ❌ | ✅ |
| File Ops | Save As | ✅ | ✅ | ❌ | ✅ |
| File Ops | Save File | ✅ | ✅ | ❌ | ✅ |
| Git | Git Commit | ✅ | ❌ | ❌ | ✅ |
| Git | Git Status | ✅ | ❌ | ❌ | ✅ |
| Hotpatch | Byte-Level | ❌ | ❌ | ❌ | ❌ |
| Hotpatch | Hotpatch Panel | ✅ | ❌ | ✅ | ❌ |
| Hotpatch | Memory Patch | ✅ | ✅ | ✅ | ❌ |
| Hotpatch | Server Patch | ✅ | ❌ | ✅ | ❌ |
| Hotpatch | Unified Manager | ✅ | ❌ | ❌ | ❌ |
| LLM | Backend Switch | ✅ | ❌ | ✅ | ✅ |
| LLM | Local Server | ✅ | ✅ | ❌ | ✅ |
| LLM | Multi-Engine | ✅ | ✅ | ✅ | ✅ |
| LSP | LSP Client | ✅ | ❌ | ❌ | ❌ |
| PowerShell | PS Execute | ✅ | ❌ | ❌ | ✅ |
| PowerShell | PS Panel | ✅ | ❌ | ❌ | ✅ |
| PS-Only | AI Code Snippets | — | — | — | ✅ |
| PS-Only | AI Debug Metrics | — | — | — | ✅ |
| PS-Only | Chat Tabs | — | — | — | ✅ |
| PS-Only | Custom Theme Builder | — | — | — | ✅ |
| PS-Only | Dependency Tracker | — | — | — | ✅ |
| PS-Only | Encryption Test | — | — | — | ✅ |
| PS-Only | Extension Marketplace | — | — | — | ✅ |
| PS-Only | GGUF Binary Reader | — | — | — | ✅ |
| PS-Only | llama.cpp Direct | — | — | — | ✅ |
| PS-Only | LMStudio Integration | — | — | — | ✅ |
| PS-Only | Monaco Editor Embed | — | — | — | ✅ |
| PS-Only | Multi-threaded Agents | — | — | — | ✅ |
| PS-Only | Performance Profiler | — | — | — | ✅ |
| PS-Only | Pop-Out Editor | — | — | — | ✅ |
| PS-Only | PoshLLM Inference | — | — | — | ✅ |
| PS-Only | PS5.1 Video Browser | — | — | — | ✅ |
| PS-Only | Reverse HTTP Backend | — | — | — | ✅ |
| PS-Only | Security Settings | — | — | — | ✅ |
| PS-Only | Task Scheduler | — | — | — | ✅ |
| PS-Only | WebView2 Browser | — | — | — | ✅ |
| PS-Only | Zone-Streamed Tensors | — | — | — | ✅ |
| RE | CFG Generation | ✅ | ❌ | ❌ | ❌ |
| RE | Data Flow | ✅ | ❌ | ❌ | ❌ |
| RE | Disassembly | ✅ | ❌ | ❌ | ❌ |
| RE | DumpBin | ✅ | ❌ | ✅ | ❌ |
| RE | Export Ghidra | ✅ | ❌ | ❌ | ❌ |
| RE | Export IDA | ✅ | ❌ | ❌ | ❌ |
| RE | MASM Compile | ✅ | ❌ | ✅ | ❌ |
| RE | PE Analysis | ✅ | ❌ | ✅ | ❌ |
| RE | SSA Lifting | ✅ | ❌ | ❌ | ❌ |
| RE | Type Recovery | ✅ | ❌ | ❌ | ❌ |
| Session | Restore Session | ✅ | ❌ | ❌ | ✅ |
| Session | Save Session | ✅ | ❌ | ❌ | ✅ |
| Settings | Editor Config | ✅ | ❌ | ❌ | ✅ |
| Streaming | Ghost Text | ✅ | ❌ | ❌ | ✅ |
| Streaming | Token Stream | ❌ | ❌ | ❌ | ✅ |
| SubAgent | HexMag Swarm | ❌ | ✅ | ❌ | ✅ |
| SubAgent | Prompt Chain | ❌ | ✅ | ❌ | ✅ |
| SubAgent | Spawn SubAgent | ✅ | ✅ | ❌ | ✅ |
| SubAgent | Swarm Panel | ✅ | ❌ | ❌ | ❌ |
| SubAgent | Todo List | ❌ | ✅ | ❌ | ✅ |
| Syntax | 6 Languages | ✅ | ❌ | ✅ | ❌ |
| Syntax | ASM Semantic | ❌ | ❌ | ❌ | ❌ |
| Syntax | C++ Keywords | ✅ | ❌ | ✅ | ✅ |
| Terminal | Kill Terminal | ❌ | ✅ | ❌ | ✅ |
| Terminal | New Terminal | ✅ | ✅ | ❌ | ✅ |
| Terminal | Split Terminal | ❌ | ✅ | ❌ | ✅ |
| Themes | 16 Built-in | ✅ | ❌ | ❌ | ✅ |
| Themes | Theme Editor | ❌ | ❌ | ❌ | ✅ |
| Themes | Transparency | ✅ | ❌ | ❌ | ❌ |
| View | Command Palette | ✅ | ❌ | ❌ | ✅ |
| View | Minimap | ❌ | ❌ | ✅ | ✅ |
| View | Output Panel | ❌ | ❌ | ❌ | ✅ |
| View | Sidebar | 🔶 | ❌ | ❌ | ✅ |

## Legend
- ✅ REAL — Fully implemented, compiles, verified in source
- 🔶 PARTIAL — Has code but incomplete
- ❌ MISSING — Not present in this IDE variant
