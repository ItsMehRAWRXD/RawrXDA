# Chat, Settings, Command Palette & Menu Audit

**Date:** 2026-02-15  
**Scope:** Win32 IDE — chat panel, Send button, Settings, Command Palette, menu items

---

## Issues Found & Fixes Applied

### 1. **Send button not working**
- **Root cause:** The secondary sidebar (AI Chat panel) used `SidebarProc`, which did not handle `WM_COMMAND`. Send and Clear button clicks were never handled.
- **Fix:** Switched the secondary sidebar to use `SidebarProcImpl`, which handles `WM_COMMAND` for Send (`IDC_COPILOT_SEND_BTN`) and Clear (`IDC_COPILOT_CLEAR_BTN`).

### 2. **Wrong control IDs**
- **Model selector** had `IDC_COPILOT_SEND_BTN` (1204) — selecting a model would incorrectly trigger Send.
- **Max Tokens slider** had `IDC_COPILOT_CLEAR_BTN` (1205) — dragging the slider would incorrectly trigger Clear.
- **Fix:** Added `IDC_MODEL_SELECTOR` (1208) and `IDC_MAX_TOKENS_SLIDER` (1209), and updated `createChatPanel()` to use them.

### 3. **Max Tokens and Context sliders**
- **Root cause:** `SidebarProcImpl` did not handle `WM_HSCROLL` for trackbars.
- **Fix:** Added `WM_HSCROLL` handling to call `onMaxTokensChanged()` and `onContextSizeChanged()` when the sliders are moved.

---

## Verified Working

### Command Palette (Ctrl+Shift+P)
- **Main loop:** `Win32IDE_Core.cpp` lines 756–762 handle `Ctrl+Shift+P` and call `showCommandPalette()` / `hideCommandPalette()`.
- **Editor:** `Win32IDE.cpp` lines 6707–6710 also handle `Ctrl+Shift+P` when the editor has focus.
- Both paths call `showCommandPalette()` from `Win32IDE_Commands.cpp`.

### Settings
- **Tools > Settings (502):** `Win32IDE_Core.cpp` line 1895.
- **Title bar gear (1024):** `Win32IDE_Core.cpp` line 1896.
- **Activity bar Settings (1106):** `Win32IDE_Core.cpp` line 1897.
- **Ctrl+, (comma):** `Win32IDE_Core.cpp` line 789.
- All invoke `showSettingsGUIDialog()` in `Win32IDE_Tier1Cosmetics.cpp` (`SettingsGUIProc`).

### Chat panel
- `createChatPanel()` in `Win32IDE.cpp` creates the secondary sidebar, model selector, input/output, Send/Clear, and sliders.
- Visibility controlled by `m_secondarySidebarVisible` and `toggleSecondarySidebar()` (Ctrl+Alt+B or Ctrl+Shift+C).

---

## Menu Items (from wiring manifest)

Items with 0 handlers may need follow-up wiring. Those addressed in this audit:

| ID | Description | Status |
|----|-------------|--------|
| 1204 | Send button | Fixed — handled in SidebarProcImpl |
| 1205 | Clear button | Fixed — handled in SidebarProcImpl |
| 502 | Tools > Settings | Working — showSettingsGUIDialog |
| 1024 | Title bar gear | Working — showSettingsGUIDialog |
| 1106 | Activity bar Settings | Working — showSettingsGUIDialog |

---

## Files Modified

1. **`src/win32app/Win32IDE.cpp`**
   - Added `IDC_MODEL_SELECTOR` (1208), `IDC_MAX_TOKENS_SLIDER` (1209).
   - Updated `createChatPanel()` to use correct IDs for model selector and max tokens slider.
   - Switched secondary sidebar from `SidebarProc` to `SidebarProcImpl`.
   - Added Send/Clear handling in `SidebarProcImpl` WM_COMMAND.
   - Added WM_HSCROLL handling for max tokens and context sliders.
