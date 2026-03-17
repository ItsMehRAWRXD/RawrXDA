# Copilot Instructions for RawrXD Agentic IDE v3.0

**Project**: RawrXD Agentic IDE v3.0 — Pure MASM frontend + C++20 backend orchestrator
**UI Layer**: Win32 + ml64 (menu/toolbar/status bar, Scintilla editor, agent chat)
**Backend**: AgentOrchestrator + TaskGraph + ToolRegistry + CodebaseIndexer + MCP/Composer engines
**Build**: Windows-first (VS 2022, ml64.exe, cl.exe), optional CMake; keep x64 calling convention

---

## Architecture Ground Rules
- Frontend is MASM-only (64-bit). Backend is C++20; communicate via C exports and message passing. Never block the UI thread; post messages to main window handles for UI updates.
- Agent orchestration: `AgentOrchestrator` is the singleton entry. Tasks become `TaskGraph` nodes executed by tools in `ToolRegistry`; keep tools stateless and re-usable.
- Code intelligence: `CodebaseIndexer` runs in the background and powers semantic search, imports, and composer context. Re-index on write operations or watcher callbacks; avoid blocking calls.
- MCP/GitHub/web access must respect `PrivacyMode`. When enabled, do not dial remote endpoints; prefer local models and skip telemetry.
- Configuration lives in `config/settings.json`. Do not hardcode endpoints, tokens, or model names; read from config and honor feature flags.

## Coding Conventions
- C++: Prefer RAII for every Win32/OS handle; use smart pointers; return structured results rather than throwing; log errors and continue where possible. Keep UTF-8 internally; convert to UTF-16 for Win32 API boundaries. Avoid std::function for hot paths where MSVC ABI issues appear—use function pointers or lightweight wrappers.
- MASM: x64 calling convention, ml64 syntax, 64-bit only. UI adjustments (menus, dialogs, DPI) must be message-driven; never spin-wait on the UI thread. Use PostMessage/SendMessage with existing control handles.
- Threading: Guard shared state with the existing mutex/locker patterns in C++; for MASM components use message-based coordination. No busy-waiting except where already confined to worker threads.
- Observability: Add structured logging around complex flows (task scheduling, tool execution, indexing, MCP calls) with durations and inputs/outputs when feasible. Favor existing logging utilities and keep logs concise.

## Safety, Privacy, and Network
- Honor `PrivacyMode`: disable cloud LLMs, MCP/Web/GitHub access, and telemetry toggles when set. Never write secrets to logs.
- External requests must be optional and configurable; default to local resources if a config entry is missing.

## Build and Layout
- Primary build: `build_all.bat` (C++ then MASM, then link). Alternative: CMake (`CMAKE_CXX_STANDARD 20`, set ASM_MASM on MASM sources). Link against SciLexer, websocketpp, sqlite_vec, WinHTTP, common Win32 libs.
- Keep ml64/VS paths configurable; do not hardcode user-specific directories. Ensure new headers/sources are added to both the batch build lists and CMake lists when applicable.

## File/Module Notes
- Frontend: `ide_main_layout.asm`, `ide_menu.asm`, `ide_toolbar.asm`, `ide_statusbar.asm`, dialogs (`settings_dialog.asm`, `model_manager_dialog.asm`), preview/review windows. Keep handles global as defined; use existing resource IDs.
- Backend: Task graph (`agent_orchestrator.*`, `task_graph.*`, `tool_registry.*`), indexing (`codebase_indexer.*`, `embedding_store.*`, `incremental_watcher.*`), composition/review engines, MCP client/server, GitHub client, privacy/project config managers.

## Testing and Quality
- Favor deterministic behaviors; avoid non-deterministic ordering in task graphs unless explicitly needed. Add smoke or unit coverage for new tools or parser changes. Keep build warnings clean; prefer /W4-friendly code.

## Strings and Encoding
- Internal strings are UTF-8; convert to UTF-16 at Win32 boundaries. MASM literals should remain ASCII unless a Win32 wide literal is required.
