# Agentic Browser Layer (in-IDE WebView2)

RawrXD embeds a **second** WebView2 surface dedicated to **browsing and agent automation**, separate from the Monaco editor WebView. Together with IDE commands, the agent panel, and `postWebMessage` JSON, this forms an **agentic browser** lane inside the IDE.

## Components

| Piece | Path | Role |
|--------|------|------|
| **Layer API** | `include/rawrxd/ide/AgenticBrowserLayer.hpp`, `src/win32app/AgenticBrowserLayer.cpp` | WebView2 lifecycle, `navigate`, `executeScriptUtf8`, `postWebMessageAsJsonUtf8`, page → host message callback |
| **IDE bridge** | `src/win32app/Win32IDE_AgenticBrowser.{h,cpp}` | Child host window (bottom third of main client), toggle, relayout on `onSize`, shutdown |
| **Monaco WebView** | `Win32IDE_WebView2.*` | Unchanged — editor only |

## Using it

1. **Environment (auto-open bottom pane on startup)**  
   `RAWRXD_AGENTIC_BROWSER=1` (or `y` / `Y`) after the main window is shown.

2. **Toggle programmatically**  
   `Win32IDE_AgenticBrowser_Toggle()` — first call creates the host + WebView2; later calls show/hide.

3. **Agent / command wiring**  
   `Win32IDE_AgenticBrowser_GetLayer()` → `RawrXD::Ide::AgenticBrowserLayer*` (null until first toggle). Then e.g. `navigate(L"https://…")`, `executeScriptUtf8(...)`, `postWebMessageAsJsonUtf8(R"({"type":"ping"})")`.

4. **Layout**  
   `Win32IDE_AgenticBrowser_Relayout()` is invoked from `Win32IDE::onSize` (`Win32IDE_Core.cpp`) so the pane tracks main-window resize.

## Agentic direction

- **Page → IDE:** shell posts `agenticBrowserShellReady`; extend the `setPageMessageHandler` lambda in `Win32IDE_AgenticBrowser.cpp` to route JSON into your agent queue or `OrchestratorBridge`.
- **IDE → page:** `postWebMessageAsJsonUtf8` + `window.chrome.webview.addEventListener('message', …)` in loaded documents.
- **Full browser:** call `navigate()` to real URLs; use `executeScriptUtf8` for DOM extraction / clicks (same patterns as Playwright-style agents, host-side).

## Requirements

- WebView2 runtime + `WebView2Loader.dll` discoverable (same as Monaco path).
- STA UI thread — keep all layer calls on the message-pump thread.

## See also

- `docs/WEBVIEW2_CHROMIUM_STDERR.md` — **ICU / Crashpad** lines on stderr (Chromium noise; not GGUF).
- `docs/BGZIPXD_WASM.md` — stack identity (loader / runner / codec / RE).
- `docs/INTEGRATED_RUNTIME.md` — coordinator boot (orthogonal to this layer).
