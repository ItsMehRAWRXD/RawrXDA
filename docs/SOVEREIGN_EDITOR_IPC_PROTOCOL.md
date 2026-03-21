# Sovereign editor IPC protocol (RawrXD)

This document defines the **window-message contract** between an **editor engine** (MonacoCore, WebView2, or a future core) and the **IDE host** (`HWND` of the main Win32 window). It is the stable boundary for Tier‑1 “ghost” completion UX: the engine **signals intent**; the host owns **document mutation**, **UTF‑8 buffer state**, and **telemetry**.

Implementations **must not** change reserved message numbers without updating:

- `include/editor_engine.h` — `RAWRXD_WM_GHOST_EDITOR_BRIDGE`
- `src/win32app/Win32IDE_Commands.h` — `WM_GHOST_TEXT_READY`
- `tests/smoke/test_monaco_bridge.cpp` — static assert and collision check

---

## 1. Roles

| Layer | Responsibility |
|--------|----------------|
| **Engine** | Renders inline ghost overlay when asked; captures Tab / Esc / “other key while ghost active”; **posts** messages to the host `HWND`. Does **not** require knowledge of RichEdit vs Monaco—only the protocol. |
| **Host** | Holds `m_ghostTextContent`, cursor/line metadata, and applies inserts to the real document. Calls `IEditorEngine::setGhostText` / `clearGhostText` when the active engine is **not** RichEdit (Tier‑1 path skips GDI ghost paint for those engines). |

---

## 2. Registered window messages

| ID | Name | Direction | Purpose |
|----|------|-------------|---------|
| `WM_APP + 400` | `WM_GHOST_TEXT_READY` | Worker / pipeline → **host UI thread** | Delivers completion **text** to the host: `wParam` = `requestedCursorPos` (see `onGhostTextReady(int, const char*)`), `lParam` = `char*` (heap; host frees) or null. Defined in `Win32IDE_Commands.h`. |
| `WM_APP + 401` | `RAWRXD_WM_GHOST_EDITOR_BRIDGE` | **Engine child → host** | User accepted or dismissed the ghost overlay **without** carrying payload in the message. |

**Collision rule:** `401` must never equal `400`. The smoke test asserts this.

---

## 3. `RAWRXD_WM_GHOST_EDITOR_BRIDGE` (Tier‑1 bridge)

**Source of truth:** `include/editor_engine.h`

```cpp
#define RAWRXD_WM_GHOST_EDITOR_BRIDGE (WM_APP + 401)
```

### 3.1 Delivery

- **API:** `PostMessageW(hostHwnd, RAWRXD_WM_GHOST_EDITOR_BRIDGE, wParam, lParam)`
- **Thread:** Typically the engine’s UI thread; must match the thread that owns `hostHwnd`’s message pump (standard Win32 rules).

### 3.2 Parameters

| Field | Value | Meaning |
|--------|--------|--------|
| `wParam` | `0` | **Accept** ghost text (Tab). Host runs accept path (`acceptGhostText()`). |
| `wParam` | **non‑zero** | **Dismiss** overlay (Esc, or any other key while ghost is active—see §4). Host runs dismiss path (`dismissGhostText()`). |
| `lParam` | `0` | Reserved; send **zero**. |

**Reserved:** Do not encode flags or pointers in `wParam`/`lParam` for this message without a spec revision and test updates.

### 3.3 Host handler (reference)

`Win32IDE_Core.cpp` switches on `RAWRXD_WM_GHOST_EDITOR_BRIDGE`:

- `wParam == 0` → `acceptGhostText()`
- else → `dismissGhostText()`

---

## 4. MonacoCoreEngine behavior (reference)

When ghost overlay is active (`m_ghostLine >= 0` and ghost text non‑empty):

| Input | Posted message |
|--------|----------------|
| **Tab** (`VK_TAB`) | `PostMessageW(..., RAWRXD_WM_GHOST_EDITOR_BRIDGE, 0, 0)` — accept |
| **Escape** (`VK_ESCAPE`) | `PostMessageW(..., RAWRXD_WM_GHOST_EDITOR_BRIDGE, 1, 0)` — dismiss; engine also clears local ghost |
| **Other key** (not shift/ctrl/menu/win modifiers) | Clears local ghost, may post **dismiss** (`1`, `0`) so the host clears shared state |

This keeps **SSOT** on the host for “is ghost visible?” while avoiding duplicate accept/dismiss logic in the webview for Tab/Esc.

---

## 5. UTF‑8 and document offsets

### 5.1 Buffer encoding

- The IDE document and engine `getText` / insert paths are treated as **UTF‑8** byte streams for cross‑engine consistency.
- **Do not** mix UTF‑16 code-unit indices with UTF‑8 byte offsets without an explicit conversion step.

### 5.2 Line start byte offset

`utf8LineStartByteOffset(doc, lineIndex)` (`Win32IDE_GhostText.cpp`) returns the **byte offset** in `doc` of the first character of line `lineIndex`, where lines are separated by **`\n`** (0x0A). Line `0` starts at byte `0`.

Use this (or equivalent logic) when converting **line + column** overlay positions to **absolute UTF‑8 offsets** for `insertText` / cursor APIs, so multi-byte code points do not “drift” relative to the AI’s completion.

### 5.3 Ghost placement API

`IEditorEngine::setGhostText(int line, int col, const char* text)` uses **line/column** in the engine’s coordinate system; the host sets `m_ghostTextLine` / `m_ghostTextColumn` and syncs via `syncActiveEngineGhostOverlay`. Alternative engines **must** document whether `col` is **UTF‑8 byte offset within line** or **grapheme/column index**—the host should map to UTF‑8 consistently when inserting.

---

## 6. Ghost delivery vs user gesture (`WM_APP+400` vs `+401`)

- **`WM_GHOST_TEXT_READY`:** Carries **new completion text** from background work onto the UI thread; host shows ghost and syncs overlay engines.
- **`RAWRXD_WM_GHOST_EDITOR_BRIDGE`:** Carries **no text**—only **accept** or **dismiss** after the user interacts with the engine.

Failures in “message posted but handler not hit” are caught by exercising the real `HWND` pump; see `tests/smoke/test_monaco_bridge.cpp` (mock `WndProc` + `PeekMessage`/`DispatchMessage`).

---

## 7. Implementing a new engine (NeoVim-style, Zed-style, etc.)

1. Obtain the host **`HWND`** (same as today’s `m_parentWindow` / factory parent).
2. On **accept** / **dismiss** gestures, call **`PostMessageW` with §3.2** semantics—do not redefine `wParam` meanings locally.
3. Keep **UTF‑8** document alignment with §5 when emitting positions or applying edits.
4. Add a smoke or integration test if you introduce a new message ID; extend this doc.

---

## 8. Verification

| Check | Location |
|--------|----------|
| `WM_APP+401` stable | `test_monaco_bridge.cpp` — `static_assert` |
| No collision with `WM_APP+400` | Same file |
| Accept / dismiss dispatch | Mock engine `acceptGhostText()` / `dismissGhostText()` call counts |

---

*Last updated: 2026-03-20 — aligns with `editor_engine.h`, `Win32IDE_Core.cpp`, `MonacoCoreEngine.cpp`, `Win32IDE_GhostText.cpp`.*
