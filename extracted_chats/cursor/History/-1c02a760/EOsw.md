# Command Palette Feature Wires — Audit

## 1. Flow (Ctrl+Shift+P)

| Step | Location | What happens |
|------|----------|--------------|
| Shortcut | `Win32IDE_Core.cpp` (keydown), `Win32IDE.cpp` | Ctrl+Shift+P → `showCommandPalette()` |
| Open | `Win32IDE_Commands.cpp::showCommandPalette()` | Creates palette window; if `m_commandRegistry` empty, calls `buildCommandRegistry()` |
| List | `buildCommandRegistry()` | Fills `m_commandRegistry` (id, name, shortcut, category). Populates list from it. |
| Filter | `filterCommandPalette()` | Category prefix (`:view`) + fuzzy match; updates `m_filteredCommands` and listbox. |
| Execute | Enter / double-click → `executeCommandFromPalette(index)` | Gets `cmd.id` from `m_filteredCommands[index]`, checks `isCommandEnabled(id)`, then **only** calls `routeCommandUnified(commandId, this, m_hwndMain)`. No legacy fallback. |
| Dispatch | `win32_feature_adapter.h::routeCommandUnified()` | 1) `RawrXD::Dispatch::dispatchByGuiId(id)` (COMMAND_TABLE); 2) `SharedFeatureRegistry::dispatchByCommandId(id)`; 3) returns false → **no further action** (legacy path never run from palette). |

So: **every palette command must be either in COMMAND_TABLE or in SharedFeatureRegistry**, or it will do nothing when run from the palette.

---

## 2. ID conflicts and wrong wires

### 2.1 Critical: 3006

- **COMMAND_TABLE** (`command_registry.hpp`): `3006` = `TERMINAL_CLEARALL_LEG` → `handleTerminalKill` (terminal clear all).
- **Palette** (`buildCommandRegistry`): `3006` = "View: Toggle Sidebar" (Ctrl+B).

**Effect:** Choosing "View: Toggle Sidebar" in the palette runs **Terminal Clear All** (handleTerminalKill), not sidebar toggle.

**Fix:** Use SSOT ID for sidebar in the palette: "View: Toggle Sidebar" should use **2028** (`VIEW_SIDEBAR` in COMMAND_TABLE). Add a separate palette entry "Terminal: Clear All Terminals" with id **3006**.

### 2.2 Legacy-only IDs (no COMMAND_TABLE entry)

These are in the palette but **not** in COMMAND_TABLE, so `routeCommandUnified` returns false and the palette does nothing:

- **3007** — View: AI Chat (toggle secondary sidebar). Handled in `Win32IDE_Core.cpp` early WM_COMMAND switch and legacy `routeCommand()`.
- **3008** — View: Toggle Panel. Legacy only.
- **3009** — View: Agent Chat (autonomous). Legacy only.

**Fix:** When `routeCommandUnified(id)` returns false, call legacy path (e.g. `routeCommand(id)` or `SendMessage(m_hwndMain, WM_COMMAND, id, 0)`) so these commands run from the palette.

---

## 3. Palette vs COMMAND_TABLE (View / Terminal)

| Palette label              | Palette ID (before fix) | COMMAND_TABLE ID | COMMAND_TABLE name        | Note |
|---------------------------|--------------------------|------------------|---------------------------|------|
| View: Toggle Minimap      | 3001                     | 2020             | VIEW_MINIMAP              | Mismatch; 3001 in table = TERMINAL_PS_LEGACY. |
| View: Toggle Output Panel | 3002                     | 2025             | VIEW_OUTPUT_PANEL         | 3002 = TERMINAL_CMD_LEGACY. |
| View: Toggle Sidebar      | 3006                     | 2028             | VIEW_SIDEBAR              | **Wrong wire:** 3006 = TERMINAL_CLEARALL_LEG. |
| View: AI Chat             | 3007                     | —                | (none)                    | Legacy only. |
| View: Toggle Panel        | 3008                     | —                | (none)                    | Legacy only. |
| View: Agent Chat          | 3009                     | —                | (none)                    | Legacy only. |
| Terminal: Clear All       | (missing)                | 3006             | TERMINAL_CLEARALL_LEG     | Should appear in palette as 3006. |

---

## 4. Recommendations

1. **Fix 3006 in palette:**  
   - "View: Toggle Sidebar" → use id **2028**.  
   - Add "Terminal: Clear All Terminals" with id **3006**.

2. **Legacy fallback from palette:**  
   In `executeCommandFromPalette()`, if `routeCommandUnified(commandId, this, m_hwndMain)` returns false, call `routeCommand(commandId)` (or equivalent) so legacy-only commands (3007, 3008, 3009, etc.) still run.

3. **Optional alignment:**  
   Align other View palette entries with COMMAND_TABLE (e.g. Toggle Minimap 2020, Toggle Output Panel 2025, Toggle Floating Panel 2024, Theme Editor 2023, Module Browser 2022) so all view commands go through SSOT and show up correctly in palette.

4. **Command states:**  
   Ensure `updateCommandStates()` sets `m_commandStates[2028]`, `m_commandStates[3006]`, and any other IDs used by the palette so `isCommandEnabled()` and "(unavailable)" in the list are correct.

---

## 5. Files touched

- `src/win32app/Win32IDE_Commands.cpp` — `buildCommandRegistry()`, `executeCommandFromPalette()`.
- `src/core/command_registry.hpp` — SSOT IDs (reference only for this audit).
- `src/win32app/win32_feature_adapter.h` — `routeCommandUnified()`.
- `src/win32app/Win32IDE_Core.cpp` — Ctrl+Shift+P, WM_COMMAND order (early 3007, then routeCommand, then routeCommandUnified).
