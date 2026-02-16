# RawrXD IDE — UI Audit Report
**Date:** 2025-02-15  
**Scope:** Chat, Send button, Settings menu, Command palette, menu items

---

## Executive Summary

This audit identified **multiple issues** that can prevent chat from loading, hide the send button, block the settings menu, and prevent the command palette from opening. Fixes are provided below.

---

## 1. CHAT NOT LOADING

### Win32 C++ IDE
- **Root cause:** The AI Chat panel (secondary sidebar) starts **hidden by default** in some build paths. Users must explicitly open it.
- **How to show chat:**
  - Click the **"Chat"** icon in the Activity Bar (left side)
  - **Ctrl+Alt+B** — toggle AI Chat
  - **Ctrl+Shift+C** — toggle AI Chat panel
  - View menu → AI Chat
- **Bug:** `createChatPanel()` in `Win32IDE.cpp` incorrectly assigns `IDC_COPILOT_SEND_BTN` to the Model Selector combobox and `IDC_COPILOT_CLEAR_BTN` to the Max Tokens slider. This causes **ID collisions** with the actual Send/Clear buttons, potentially breaking click handling.

### HTML Chatbot (ide_chatbot_win32.html)
- **Root cause:** Requires backend (Ollama / tool_server) at `http://localhost:8080` or `ws://localhost:11434`. If backend is offline, chat shows "Offline" and uses fallback responses.
- **Mitigation:** Use **"Connect to Backend"** (⚡) in the header, or start the tool server / Ollama first.

---

## 2. NO SEND BUTTON

### Win32 C++ IDE
- **Cause A:** Chat panel is hidden — Send button is inside the panel; show chat first (see §1).
- **Cause B:** **ID collision** — Model Selector and Max Tokens Slider reuse Send/Clear IDs, so WM_COMMAND can be misrouted. **Fix applied:** Use unique IDs for Model Selector and Sliders.

### HTML Chatbot
- **Cause:** During streaming, `showStopHideSend()` hides the Send button and shows Stop. If an error occurs before `showSendHideStop()`, the Send button stays hidden.
- **Mitigation:** Refresh the page (F5) or add error-path call to `showSendHideStop()` in all catch/finally blocks (see `ide_chatbot_engine.js`).

---

## 3. SETTINGS MENU NOT WORKING

### Win32 C++ IDE
- **Entry points:**
  - **Ctrl+,** (Ctrl+OemComma) — Opens full Settings GUI
  - Activity Bar: **No dedicated Settings icon** in the Sidebar implementation. Use Ctrl+, or:
  - **View menu** → Settings (if present)
  - **Tools menu** → Settings
  - **Title bar** — "Gear" button (IDC_BTN_SETTINGS) when using custom-drawn title bar
- **Handler:** `showSettingsGUIDialog()` is wired for command IDs 502, 1024, 1106.

### HTML Chatbot (ide_chatbot_win32.html)
- **Gap:** No dedicated **"Settings"** menu. Configuration is spread across:
  - **Backends** — Backend switcher
  - **Debug** — Debug panel with URL, reconnect options
  - **Gen-settings** — Generation parameters in sidebar
- **Recommendation:** Add a visible "Settings" or gear button that opens a unified settings panel.

---

## 4. COMMAND PALETTE NOT WORKING

### Win32 C++ IDE
- **Shortcut:** **Ctrl+Shift+P**
- **Behavior:** Implemented in `Win32IDE_Commands.cpp` (`showCommandPalette()`). Keyboard handling is in the main message loop (Win32IDE_Core.cpp) and main window `WM_KEYDOWN`.
- **Potential issues:**
  - Focus in a child control (e.g., Edit, ComboBox) may prevent the main window from receiving the key. The app uses a **PeekMessage**-based loop to intercept Ctrl+Shift+P before dispatch — verify this runs for all focus states.
  - If the palette window fails to create, it silently returns.

### Multiwindow IDE (multiwindow_ide.html)
- **Shortcut:** **Ctrl+P** (not Ctrl+Shift+P)
- **Behavior:** `toggleCommandPalette()` toggles `#command-palette.show`. Element has `display:none` by default.

### HTML Chatbot (ide_chatbot_win32.html)
- **Gap:** **No command palette.** Consider adding one (e.g., Ctrl+Shift+P) for consistency.

---

## 5. MENU ITEMS AUDIT

### Win32 IDE Menu Summary

| Menu       | Items                                                                 | Notes                                              |
|-----------|-----------------------------------------------------------------------|----------------------------------------------------|
| File      | New, Open, Save, Save As, Close, Recent, Exit                        | Standard                                           |
| Edit      | Undo, Redo, Cut, Copy, Paste, Find, Replace                          | Standard                                           |
| View      | Sidebar, Output, Terminal, AI Chat (Ctrl+Alt+B), Extensions, Settings| AI Chat toggles secondary sidebar                  |
| Tools     | Settings (502)                                                        | Opens Settings GUI                                 |
| AI        | Model Registry, Checkpoint, CI/CD Settings, etc.                      | AI-specific                                        |

### Activity Bar (Sidebar implementation)
- Files, Search, Source, Debug, Exts, Recov, **Chat** — Chat toggles secondary sidebar.
- **Settings is not in the Activity Bar** in this implementation.

---

## 6. FIXES APPLIED

### Fix 1: ID Collision in createChatPanel (Win32IDE.cpp)
- **Problem:** Model Selector used `IDC_COPILOT_SEND_BTN`, Max Tokens Slider used `IDC_COPILOT_CLEAR_BTN`.
- **Fix:** Assign unique IDs (`IDC_MODEL_SELECTOR`, `IDC_MAX_TOKENS_SLIDER`) to avoid conflicts with Send/Clear buttons.

### Fix 2: Ensure Send Button Recovers on Error (ide_chatbot_engine.js)
- **Problem:** If streaming errors, `showSendHideStop()` may not run.
- **Fix:** Wrapped `sendMessage()` in try/catch/finally so `showSendHideStop()` always runs.

### Fix 3: Wire Send and Clear Buttons (Win32IDE.cpp SidebarProcImpl)
- **Problem:** Send and Clear button clicks had no handlers; `sendCopilotMessage`/`clearCopilotChat` were never called.
- **Fix:** Added WM_COMMAND handling for `IDC_COPILOT_SEND_BTN` and `IDC_COPILOT_CLEAR_BTN` in `SidebarProcImpl`.

### Fix 4: Settings + Command Palette for ide_chatbot_win32 (HTML)
- **Problem:** No visible Settings button; no Ctrl+Shift+P command palette.
- **Fix:** Added gear (Settings) button in header; added Ctrl+Shift+P shortcut to open Backend Switcher.

---

## 7. RECOMMENDATIONS

1. **Chat visibility:** Consider showing the AI Chat panel by default on first run, or add an onboarding hint.
2. **Settings access:** Add a Settings icon to the Activity Bar in the Sidebar implementation.
3. **ide_chatbot_win32:** Add a command palette (Ctrl+Shift+P) and a visible Settings entry point.
4. **Error recovery:** Ensure all streaming/async paths call `showSendHideStop()` on error.
