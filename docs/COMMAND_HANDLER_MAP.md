# RawrXD Command Handler Map

**Purpose:** Maps every command ID from `include/command_registry.hpp` (COMMAND_TABLE) to its implementation and proof. Used for zero-patch audit and routeToIde verification.

**Audit date:** 2026-02-22.  
**Real lane:** Build with `-DRAWRXD_ENABLE_MISSING_HANDLER_STUBS=OFF`.

---

## Legend

| Symbol | Meaning |
|--------|--------|
| ✅ Real | Handler has real implementation (no stub). |
| ⚠️ Routed | SSOT calls `routeToIde(ctx, id, name)`; IDE handles via `routeCommand` or `onCommand` switch. |
| 🔀 Dual | COMMAND_TABLE ID (e.g. 1001) and menu ID (e.g. 2001) both map to same action. |

---

## 1. File (1001–1099)

| cmdId | Name | SSOT Handler | IDE Handler | Status |
|-------|------|--------------|-------------|--------|
| 1001 | file.new | handleFileNew | routeCommand(1001)→handleFileCommand(1001)→newFile() | ✅ Real (added 1001 case) |
| 1002 | file.open | handleFileOpen | handleFileCommand(1002)→openFile() | ✅ Real |
| 1003 | file.save | handleFileSave | handleFileCommand(1003)→saveFile() | ✅ Real |
| 1004 | file.saveAs | handleFileSaveAs | handleFileCommand(1004)→saveFileAs() | ✅ Real |
| 1005 | file.saveAll | handleFileSaveAll | handleFileCommand(1005)→saveAll() | ✅ Real |
| 1006 | file.close | handleFileClose | handleFileCommand(1006)→closeFile() | ✅ Real |
| 1010 | file.recentFiles | handleFileRecentFiles | handleFileCommand(1010)→openRecentFile (default branch) | ✅ Real |
| 1020 | file.recentClear | handleFileRecentClear | handleFileCommand(1020)→clearRecentFiles() | ✅ Real |
| 1030–1035 | file.loadModel, modelFromHF, Ollama, URL, unified, quickLoad | handleFile* | handleFileCommand / onCommand switch 1030–1035 | ✅ Real |
| 1099 | file.exit | handleFileExit | handleFileCommand(1099)→PostQuitMessage / onCommand 2005 | ✅ Real |

**Proof:** Command palette "file.new" (id 1001) → routeCommand(1001) → handleFileCommand → case 1001 → newFile(). Menu File > New sends 2001 → onCommand switch case 2001 → newFile().

---

## 2. Edit (2001–2019)

| cmdId | Name | SSOT Handler | IDE Handler | Status |
|-------|------|--------------|-------------|--------|
| 2001 | edit.undo | handleEditUndo | handleEditCommand(2001)=IDM_EDIT_UNDO → EM_UNDO | ✅ Real |
| 2002 | edit.redo | handleEditRedo | handleEditCommand(2002) → EM_REDO | ✅ Real |
| 2003–2006 | cut, copy, paste, selectAll | handleEdit* | handleEditCommand → WM_CUT/COPY/PASTE, EM_SETSEL | ✅ Real |
| 2012–2019 | snippet, copyFormat, pastePlain, clipboardHist, find, replace, findNext, findPrev | routeToIde or handleEdit* | handleEditCommand cases 2012–2019, showFindDialog, findNext, etc. | ✅ Real |

**Proof:** Ctrl+Z / palette edit.undo → 2001 → handleEditCommand(2001) → SendMessage(EM_UNDO).

---

## 3. View (2020–2029, 3000+)

| cmdId | Name | SSOT Handler | IDE Handler | Status |
|-------|------|--------------|-------------|--------|
| 2020–2029 | view.minimap, outputTabs, moduleBrowser, themeEditor, floatingPanel, outputPanel, streamingLoader, vulkanRenderer, sidebar, terminal | handleView* (routeToIde) | routeCommand(2020–2999)→handleViewCommand | ✅ Real |
| 3020–3024 | git.status, commit, push, pull, diff | handleGit* | routeCommand(3000–3999)→handleViewCommand → handleGitCommand | ⚠️ Verify per ID |

**Proof:** View > Output Panel (2021) → routeCommand(2021) → handleViewCommand(2021) → toggleOutputPanel().

---

## 4. VS Code Extension API (10000–10009)

| cmdId | Name | SSOT Handler | IDE Handler | Status |
|-------|------|--------------|-------------|--------|
| 10000 | vscext.status | handleVscExtStatus | routeCommand(10000–10099)→handleVSCExtAPICommand → VscextRegistry::getStatusString | ✅ Real |
| 10001 | vscext.reload | handleVscExtReload | handleVSCExtAPICommand → VscextRegistry::reload | ✅ Real |
| 10002 | vscext.listCommands | handleVscExtListCommands | handleVSCExtAPICommand → VscextRegistry::listCommands | ✅ Real |
| 10003 | vscext.listProviders | handleVscExtListProviders | handleVSCExtAPICommand → VscextRegistry::listProviders | ✅ Real |
| 10004 | vscext.diagnostics | handleVscExtDiagnostics | handleVSCExtAPICommand → VscextRegistry::getDiagnosticsReport | ✅ Real |
| 10005 | vscext.extensions | handleVscExtExtensions | handleVSCExtAPICommand → VscextRegistry::listExtensions | ✅ Real |
| 10006 | vscext.stats | handleVscExtStats | handleVSCExtAPICommand → VscextRegistry::getStatsJson | ✅ Real |
| 10007 | vscext.loadNative | handleVscExtLoadNative | handleVSCExtAPICommand (load native ext) | ✅ Real |
| 10008 | vscext.deactivateAll | handleVscExtDeactivateAll | handleVSCExtAPICommand | ✅ Real |
| 10009 | vscext.exportConfig | handleVscExtExportConfig | handleVSCExtAPICommand → VscextRegistry::exportConfig | ✅ Real |

**Proof:** `--selftest` posts WM_COMMAND 10002; handleVSCExtAPICommand calls VscextRegistry::listCommands. CLI `!vscext commands` uses handleVscExtListCommands in ssot_handlers_ext_isolated.cpp (real lane).

---

## 5. routeCommand ID ranges (Win32IDE_Commands.cpp)

| Range | Handler | Notes |
|-------|---------|--------|
| 1000–1999 | handleFileCommand | File menu + COMMAND_TABLE 1001–1099 |
| 2000–2019 | handleEditCommand | Edit (COMMAND_TABLE 2001–2019) |
| 2020–2999 | handleViewCommand | View panels/toggles |
| 3000–3999 | handleViewCommand | Git, theme, etc. |
| 4000–4099 | handleTerminalCommand | Terminal |
| 4100–4399 | handleAgentCommand | Agent/Autonomy |
| 5000–5999 | handleToolsCommand | Tools |
| 9100–9199 | handleMonacoCommand | Monaco editor |
| 9500–9599 | handleAuditCommand | Audit (also onCommand direct) |
| 9600–9699 | handleGauntletCommand | Gauntlet |
| 9700–9799 | handleVoiceChatCommand | Voice |
| 9800–9899 | handleQuickWinCommand | Quick wins |
| 9900–9999 | handleTelemetryCommand | Telemetry |
| 10000–10099 | handleVSCExtAPICommand | VS Code Extension API |
| 10400–10499 | handleBuildCommand | Build |
| 11500–11599 | handleFeaturesCommand | Features (refactor, language, vision, etc.) |
| 12000–12099 | handleTier1Command | Tier1 cosmetics |
| … | (see routeCommand) | Full list in Win32IDE_Commands.cpp |

---

## 6. onCommand direct switch (Win32IDE_Core.cpp)

These IDs are handled before routeCommand so that menu and toolbar behave correctly (no double dispatch):

- **2001–2005:** newFile, openFile, saveFile, saveFileAs, PostQuitMessage.
- **2007–2011:** Edit Undo, Redo, Cut, Copy, Paste (menu IDs).
- **1030–1035:** Model load, HF, Ollama, URL, unified, quickLoad.
- **2026, 2027:** Streaming loader, Vulkan renderer toggles.
- **502, 1024, 1106:** Settings.
- **1022, 3007, 3009:** Toggle AI/Agent chat (secondary sidebar).
- **IDM_VIEW_AGENT_PANEL, IDM_SECURITY_*, IDM_BUILD_*:** Security scans, build solution/project/clean.

---

## 7. Summary

- **File:** COMMAND_TABLE 1001–1099 and menu 2001/2002/… both drive the same actions (handleFileCommand now has explicit 1001–1099 cases where needed).
- **Edit:** COMMAND_TABLE 2001–2019 = IDM_EDIT_UNDO etc.; handleEditCommand covers them.
- **View / Git / Theme / Terminal / Agent / Tools / Monaco / Audit / Gauntlet / Voice / QuickWin / Telemetry / VSCExt / Build / Features / Tier1:** All routed via routeCommand(id) to the corresponding handle*Command; VSCExt (10000–10009) use VscextRegistry (real).
- **Selftest:** `RawrXD-Win32IDE.exe --selftest` runs file I/O, AIWorkers queue, Vscext status/listCommands, WebSocketHub start/stop, and WM_COMMAND 10002 dispatch.

---

## 8. Verification commands

```powershell
# Build real lane (default is OFF for stubs)
cd D:\rawrxd\build
cmake -DRAWRXD_ENABLE_MISSING_HANDLER_STUBS=OFF ..
cmake --build . --config Release --target RawrXD-Win32IDE

# Run selftest (exit 0 = pass)
.\bin\Release\RawrXD-Win32IDE.exe --selftest

# Validation script (includes selftest if exe found)
.\VALIDATE_REVERSE_ENGINEERING.ps1
```
