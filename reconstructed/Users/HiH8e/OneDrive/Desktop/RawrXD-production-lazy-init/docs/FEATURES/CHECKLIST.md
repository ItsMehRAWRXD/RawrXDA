# Feature Checklist — Win32/MASM Agentic IDE (Qt-Free)

Date: 2025-12-18
Scope: Track fully working features to (re)enable one-by-one in the Qt-free IDE.

## UI Layout & Panels
- [ ] Left: File Browser (all drives)
  - Required: Enumerate `A:`..`Z:` drives; TreeView; context menu (Open, Reveal, New File/Folder, Rename, Delete, Refresh)
  - Open on double-click; keyboard nav; large folder perf OK
  - Source: `src/win32app/` (Win32); replaces `src/qtapp/file_browser.*`
- [ ] Middle: Tabbed Editor
  - Required: TabControl with new/close; Ctrl+T/Ctrl+W; unsaved indicator; per-tab encoding/line-endings; search (Ctrl+F) + replace
  - Optional: split view; undock tabs; MRU switching (Ctrl+Tab)
- [ ] Right: Chat/Assistant (Tabbed)
  - Required: Multi-tab chat; streaming output; slash-commands (`/cursor`, `/paintgen`, `/grep`, `/orchestrate`)
  - Undockable panel; resizable; copy-to-clipboard; save chat transcript
- [ ] Bottom: Split Terminal + Orchestrator
  - Required: Terminal (PowerShell) on left with scrollback; on right an "Orchestra" panel
  - Orchestra: model dropdown, Max Mode toggle, objective textbox, Run button, output log
  - Wire to ToolRegistry pipeline and model bridge
- [ ] Max Mode toggle (F11 + View menu + toolbar)
  - Hides side/bottom panes; focuses editor; toggles back cleanly
- [ ] Paint Pane + Image Generation
  - `/paintgen <prompt>` invokes image bridge → decodes base64 PNG via WIC → opens canvas tab
  - Save/Export PNG; basic brush/erase/zoom

## Core Logic (Non-UI)
- [ ] Tool Registry wired (non-Qt)
  - Files: `include/tool_registry.hpp`, `src/tool_registry.cpp`
  - Required: Register and execute tools from Orchestra + chat; timeouts/retry/metrics
- [ ] GGUF Streaming Loader (std/Win32 path)
  - Files: `src/streaming_gguf_loader.*` (non-Qt), ensure no Qt deps
  - Required: Load, stream tokens, cancel/stop, backpressure; expose status for UI
- [ ] MASM Compression/Decompression
  - Files: `src/masm_decompressor.cpp`, `src/gzip_masm_store.cpp`, `src/qtapp/inflate_*.asm` (port glue off Qt)
  - Required: Thin C++ wrappers using `std::vector<uint8_t>` + `std::filesystem`
- [ ] Telemetry + Metrics (file-based)
  - Files: `include/logging/logger.h`, `include/metrics/metrics_emitter.h`, `src/telemetry*.cpp`
  - Required: logs/ directory; optional `enterprise_metrics.prom` output; opt-in flag
- [ ] Settings & Persistence
  - Replace QSettings with JSON in `%APPDATA%/RawrXD` (Windows), `$XDG_CONFIG_HOME` (Linux), `~/Library/Application Support` (macOS)

## Assistant & Orchestration
- [ ] Model Selector wired
  - Dropdown sources: local GGUF, remote (Ollama, OpenAI via env vars)
  - Per-request config: temperature, max tokens, system prompt presets
- [ ] Orchestrate Workflow
  - Plan → Execute tools → Reflect → Commit changes → Report
  - Uses ToolRegistry; emits progress and final report to panel and chat
- [ ] Cursor Bridge
  - `/cursor <objective>` → node bridge → insert returned `result.code` into active editor
  - Env: `OPENAI_API_KEY`, `RAWRXD_NODE_PATH` optional

## Editor Essentials
- [ ] Syntax highlighting (C/C++/JSON/MD/PowerShell)
- [ ] Indentation, brace match, basic lint hooks
- [ ] File encodings (UTF-8 default), CRLF/LF detection and convert

## Platform Targets
- [ ] Windows (Win32)
- [ ] macOS (Cocoa; placeholder OK)
- [ ] Linux (X11/Wayland; placeholder OK)

## Acceptance Gates (per feature)
For each item above when marking complete:
- Runs in Release build with no Qt DLLs loaded
- Has basic unit/integration smoke where applicable
- Logs non-fatal errors instead of crashing
- Documented in `docs/README_FEATURES.md`

References
- Tool registry: `include/tool_registry.hpp`, `src/tool_registry.cpp`
- GGUF: `src/streaming_gguf_loader.*`, `src/gguf_loader.cpp`
- MASM glue: `src/masm_decompressor.cpp`, `src/gzip_masm_store.cpp`
- Logging: `include/logging/logger.h`
