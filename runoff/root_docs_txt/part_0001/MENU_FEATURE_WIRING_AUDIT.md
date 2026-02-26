# Menu & Feature Wiring Audit

## Full Audit — Menu Items, Breadcrumbs, File Explorer, Chat Panel, Audit Dashboard

### Summary

| Area | Issue | Status |
|------|-------|--------|
| Ctrl+Shift+A (Audit Dashboard) | Not showing when pressed | **Fixed** |
| Chat panel Send/Clear | Buttons not wired | **Fixed** (previous session) |
| File explorer | Visibility / access | **Already correct** |
| AI Chat / Agent panel | No way to open | **Fixed** (Activity Bar, View menu, GH button, Ctrl+Alt+B) |
| Placeholder labels | "GitHub Copilot Chat" | **Fixed** → "AI Chat" |
| Breadcrumbs | Symbol path (File > Class > Method) | **Working** — not placeholder |

---

## Issues Found and Fixed

### 1. Chat Panel Send/Clear Buttons Not Working
**Root cause:** The secondary sidebar (AI Chat) used the default STATIC window proc. When the user clicked Send or Clear, `WM_COMMAND` was sent to the secondary sidebar's window—but that proc did not forward it to the main window, so `onCommand` never saw IDs 1204/1205.

**Fix:** Subclassed `m_hwndSecondarySidebar` with `SecondarySidebarProc` and added `WM_COMMAND` handling that forwards to the parent (main window):
```cpp
case WM_COMMAND:
    if (pThis && GetParent(hwnd))
        SendMessage(GetParent(hwnd), WM_COMMAND, wParam, lParam);
    return 0;
```

### 2. HandleCopilotSend Crashed When Model Selector Missing
**Root cause:** `HandleCopilotSend()` always called `SendMessage(m_hwndModelSelector, ...)`. The VSCodeUI chat panel does not create `m_hwndModelSelector`; only `createChatPanel()` does (and that path is unused).

**Fix:** Guard model selector access:
```cpp
if (m_hwndModelSelector && IsWindow(m_hwndModelSelector)) { ... }
else selectedModel = RAWRXD_NATIVE_DISPLAY;
```

### 3. Placeholder "GitHub Copilot Chat" Labels
**Root cause:** UI text referred to "GitHub Copilot Chat" though the feature is a local AI chat (RawrXD/GGUF/Ollama).

**Fix:** Replaced with "AI Chat" in header, welcome text, and clear message.

### 4. No Way to Open Chat Panel
**Root cause:** The chat panel (secondary sidebar) could only be toggled via Ctrl+Alt+B or command 3007, which were not exposed in the UI.

**Fix:**
- Added **Activity Bar "Chat" button** (IDC_ACTIVITY_CHAT 6007) that calls `toggleSecondarySidebar()`
- Added **View > AI Chat** menu item (ID 3007) with Ctrl+Alt+B shortcut
- Wired **Title bar "GH" button** (IDC_BTN_GITHUB 1022) to toggle the AI Chat panel
- Added **Ctrl+Alt+B** shortcut in the message loop

### 5. File Explorer Visibility
**Status:** The file explorer was already correctly wired:
- `createPrimarySidebar` → `createExplorerView` → `m_hwndExplorerTree`
- Default view is `SidebarView::Explorer`
- Activity Bar "Files" button (IDC_ACTIVITY_EXPLORER) calls `setSidebarView(SidebarView::Explorer)`
- Sidebar visible by default (`m_sidebarVisible = true`)

## Files Modified

| File | Changes |
|------|---------|
| `Win32IDE_VSCodeUI.cpp` | Subclass secondary sidebar, forward WM_COMMAND, relabel to "AI Chat" |
| `Win32IDE_Core.cpp` | Handle 1022 (GH btn), 3007 (View>AI Chat), Ctrl+Alt+B, audit 9500–9600 direct handling, pass hwnd to routeCommandUnified |
| `Win32IDE_Sidebar.cpp` | Add IDC_ACTIVITY_CHAT button, wire to `toggleSecondarySidebar()` |
| `Win32IDE.cpp` | View menu "AI Chat" item, `HandleCopilotSend` model-selector guard |
| `win32_feature_adapter.h` | Add hwndMain param to routeCommandUnified, set ctx.hwnd |
| `ssot_handlers.cpp` | Use ctx.hwnd in delegateToGui and audit handlers; fallback to old cast if null |
| `Win32IDE_Commands.cpp` | Pass m_hwndMain to routeCommandUnified |

### 6. Ctrl+Shift+A Audit Dashboard Not Showing
**Root cause:** Two problems:
1. **PostMessage loop:** `handleAuditDashboard` Posted WM_COMMAND 9500 to the main window. When processed, `onCommand` called `routeCommandUnified(9500)` again, which invoked `handleAuditDashboard` again → infinite Post loop, never reaching `cmdAuditShowDashboard`.
2. **Wrong HWND:** `delegateToGui` and audit handlers used `*reinterpret_cast<HWND*>(ctx.idePtr)` — treating `Win32IDE*` as pointer-to-HWND. The first 8 bytes of the object (often vtable) were used as HWND, so `PostMessage` went to the wrong or invalid window.

**Fix:**
- Add audit range (9500–9600) handling in `onCommand` *before* `routeCommandUnified`: `if (id >= 9500 && id < 9600) { handleAuditCommand(id); return; }`
- Pass main window HWND into `routeCommandUnified(id, idePtr, hwndMain)` so `ctx.hwnd` is set
- Update `delegateToGui` and audit handlers to use `ctx.hwnd` when available

---

## How to Use

| Feature | How to Access |
|---------|---------------|
| **File Explorer** | Activity Bar → **Files**, View → **File Explorer**, Ctrl+Shift+E, or Ctrl+B to show sidebar |
| **AI Chat (agent panel)** | Activity Bar → **Chat**, View → **AI Chat**, Ctrl+Alt+B, or title bar **AI** button |
| **Chat Send/Clear** | Type message, click **Send** or **Clear** in the AI Chat panel |
| **Audit Dashboard** | Ctrl+Shift+A, or Audit → Show Dashboard |
| **Bounded Agent** | Agent → Bounded Agent (FIM Tools), or Ctrl+Shift+I |
