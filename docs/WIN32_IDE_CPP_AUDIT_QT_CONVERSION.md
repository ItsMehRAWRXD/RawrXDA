# Win32IDE.cpp — Audit: Missing / Not-Yet-Converted-from-Qt Source

**Date:** 2026-02-12  
**File:** `src/win32app/Win32IDE.cpp`  
**Scope:** Entire file for Qt remnants, ANSI-only Win32 (unconverted to Unicode), and stub or placeholder logic.

---

## 0. Conversion progress (Qt removal / C++20 / Unicode)

- **Qt:** No Qt in Win32 IDE build. `include/agentic_iterative_reasoning.h`, `include/distributed_trainer.h`, `include/mainwindow_qt_original.h`, `include/extension_panel.h` are Qt-free (C++20 stubs or std:: types).
- **Unicode (W) already done:**  
  - **Menus:** File, Edit, View, Terminal in `createMenuBar()` use `AppendMenuW` and `L"..."`.  
  - **MessageBox:** `Win32IDE_Tier1Cosmetics.cpp` (Settings reset), `Win32IDE_Sidebar.cpp` (replace in files, uninstall extension, load GGUF), `Win32IDE_CursorParity.cpp` (input dialog) use `MessageBoxW` and `utf8ToWide()` where needed.  
  - **GUILayoutHotpatch:** Viewer dialog and all prompts use `CreateWindowExW`, `MessageBoxW`, `utf8ToWide()`.
- **Still ANSI:** Tools (and submenus: Voice, Backup, Alerts, Shortcuts), Modules, Help, Audit, Git, Agent menus in `Win32IDE.cpp` (~414–550); Find/Replace/Snippet dialogs; file open/save; status bar; Rich Edit/CHARFORMAT2. Convert to `AppendMenuW`/`CreateWindowExW`/`MessageBoxW`/wide strings per sections below.

---

## 1. Executive Summary

| Category | Status | Count / Notes |
|----------|--------|----------------|
| **Qt symbols** | **None** | No `Qt`, `QString`, `QWidget`, `QDialog`, or any Qt includes. File is **Qt-free**. |
| **ANSI Win32 APIs** | **Not converted** | Dozens of `CreateWindowExA`, `MessageBoxA`, `GetOpenFileNameA`, `SetWindowTextA`, etc. — conversion to Unicode (W) recommended for i18n and parity with Qt’s Unicode model. |
| **Rich Edit ANSI** | **Not converted** | `CHARFORMAT2A`, `TEXTRANGEA`, `GetWindowTextLengthA`, `EM_GETTEXTRANGE` with `TEXTRANGEA`, `CP_ACP` — editor/formatting paths still ANSI. |
| **Stub / minimal logic** | **Few** | No explicit “Qt stub” comments; a few areas are minimal (e.g. copy “with formatting” is simplified). |
| **Deferred / Phase comments** | **Informational** | “DEFERRED IMPLEMENTATIONS”, “Phase 8B/8C” etc. refer to feature phases, not Qt conversion. |

**Conclusion:** Win32IDE.cpp has **no remaining Qt source**. The main “conversion” still open is **ANSI → Unicode (W)** for dialogs, windows, and text APIs so behavior and i18n match what a Qt-based IDE would have (Unicode everywhere).

---

## 2. Qt Symbol Audit

- **Result:** No Qt includes, types, or macros found in `Win32IDE.cpp`.
- Build and design are **no Qt** (see `docs/CLI_GUI_WIN32_QT_MASM_AUDIT.md`). This file is fully migrated off Qt at the type/symbol level.

---

## 3. ANSI Win32 API — Not Yet Converted to Unicode (W)

These are the main places still using ANSI. Converting to `CreateWindowExW`, `MessageBoxW`, `GetOpenFileNameW`, `SetWindowTextW`, etc. (and `OPENFILENAMEW`, `WNDCLASSEXW`, `CHARFORMAT2W` where applicable) would complete “conversion” in the sense of matching Qt’s Unicode-by-default behavior.

### 3.1 File / Save dialogs and messages

| Approx. line | API / usage | Suggestion |
|--------------|-------------|------------|
| 1232, 1263, 1309, 1310, 1352, 1355, 1383, 1385, 1392, 1393, 1412 | `MessageBoxA` | Replace with `MessageBoxW`; message/caption from `std::wstring` or UTF-16 buffer. |
| 1273–1287, 1403–1416 | `OPENFILENAMEA`, `GetOpenFileNameA`, `GetSaveFileNameA` | Use `OPENFILENAMEW`, `GetOpenFileNameW`, `GetSaveFileNameW`; paths as `wchar_t`. |
| 1245, 1294, 1330 | `SetWindowTextA(m_hwndEditor, ...)` | Use `SetWindowTextW` with UTF-16 or Rich Edit Unicode (e.g. `EM_SETTEXTRANGEW` / stream). |
| 1251, 1298, 1351, 1378 | `SendMessage(..., SB_SETTEXT, ..., (LPARAM)"...")` | Use `SendMessageW` and wide caption strings. |

### 3.2 Find / Replace dialogs (programmatic fallback and full dialog)

| Approx. line | API / usage | Suggestion |
|--------------|-------------|------------|
| 2820–2846 | `CreateDialogParamA`, `CreateWindowExA(..., "STATIC", "Find", ...)` and all child controls | Register/use a Unicode window class and `CreateWindowExW` with `L"Find"` etc.; or keep resource as Unicode. |
| 2859–2890 | Replace dialog: `CreateWindowExA`, `"Replace"`, all `CreateWindowExA` children | Same as above; convert to W and wide strings. |
| 3182–3184, 3230–3235 | `GetWindowTextA(hwndFindText, buffer, 256)` | Use `GetWindowTextW` and `wchar_t` buffers (and map to `std::string` internally if needed). |

### 3.3 Snippet Manager dialog

| Approx. line | API / usage | Suggestion |
|--------------|-------------|------------|
| 3294–3340 | `CreateWindowExA(..., "STATIC", "Snippet Manager", ...)`, all children `CreateWindowExA` / `SendMessageA(hwndList, LB_ADDSTRING, ...)` | Use `CreateWindowExW`, `L"Snippet Manager"`, wide labels; listbox with `LB_ADDSTRING` on Unicode build or use `SendMessageW` with wide strings. |
| 3342–3402 | Modal loop + `GetWindowTextA`, `SetDlgItemTextA`, `GetDlgItemTextA` on snippet controls | Use `GetWindowTextW` / `SetDlgItemTextW` / `GetDlgItemTextW` and convert to/from `std::string` for storage if desired. |

### 3.4 Floating panel

| Approx. line | API / usage | Suggestion |
|--------------|-------------|------------|
| 2588–2597 | `WNDCLASSEXA`, `RegisterClassExA`, `"RawrXD_FloatingPanel"` | Use `WNDCLASSEXW`, `RegisterClassExW`, `L"RawrXD_FloatingPanel"`. |
| 2607–2643 | `CreateWindowExA(..., "RawrXD_FloatingPanel", "Panel", ...)`, tab buttons `CreateWindowExA(0, "BUTTON", tabLabels[i], ...)`, content `CreateWindowExA(..., "EDIT", ...)` | Use `CreateWindowExW` and wide strings for class, title, and labels. |
| 2622 | `SetWindowLongPtrA` | Prefer `SetWindowLongPtrW` for consistency (or keep for pointer storage; no string). |
| 2677–2679 | `GetWindowTextLengthA`, `SendMessageA(..., EM_SETSEL | EM_REPLACESEL | EM_SCROLLCARET)` | Use `GetWindowTextLengthW` and Unicode Rich Edit messages if the control is Rich Edit; otherwise `SendMessageW` for text. |
| 2692–2698 | `SendMessageA(hTabBtn, BM_SETSTATE, ...)` | Can stay or switch to `SendMessageW` for consistency. |

### 3.5 Output and editor formatting

| Approx. line | API / usage | Suggestion |
|--------------|-------------|------------|
| 1946–1950 | `CreateWindowExA(..., "EDIT", ...)` for output tabs | Use `CreateWindowExW` and Unicode EDIT (or Rich Edit) for output. |
| 1991–1992 | `SetWindowTextA(it->second, "")` | Use `SetWindowTextW` with `L""` (or equivalent). |
| 2006–2014 | `CHARFORMAT2A`, `SendMessage(..., EM_SETCHARFORMAT, ..., &cf)`, `SETTEXTEX` with `CP_ACP` | Use `CHARFORMAT2W` and `EM_SETCHARFORMAT` with Unicode; set codepage for UTF-16 or use `ST_UNICODE` where applicable. |
| 2024–2025 | `TEXTRANGEA`, `EM_GETTEXTRANGE` | Use `TEXTRANGEW` and `EM_GETTEXTRANGE` (Unicode Rich Edit). |
| 2033–2035 | `SetClipboardData(CF_TEXT, ...)` | Consider `CF_UNICODETEXT` for clipboard when text is Unicode. |
| 3344–3350 | `CHARFORMAT2A`, `strcpy(cf.szFaceName, "Consolas")`, `SendMessage(..., EM_SETCHARFORMAT, SCF_ALL, &cf)` | Use `CHARFORMAT2W`, `wcscpy_s(cf.szFaceName, L"Consolas")`, and Unicode path. |

### 3.6 Menu bar and other UI

| Approx. line | API / usage | Suggestion |
|--------------|-------------|------------|
| 354–520 (and surrounding) | `AppendMenuA(hFileMenu, MF_STRING, ..., "&New")` and all other `AppendMenuA` in `createMenuBar` | Use `AppendMenuW` with `L"&New"` etc. for full Unicode menus (and correct display of non-ASCII). |

### 3.7 Other scattered ANSI

- **LoadCursor:** e.g. `LoadCursor(nullptr, IDC_ARROW)` — already usable with Unicode; for custom cursors use `LoadCursorW` and resource by ID.
- **DefWindowProc:** If the window class is registered as Unicode, use `DefWindowProcW` in that window’s proc (e.g. Floating Panel); currently may be `DefWindowProcA` in places.
- **getWindowText(m_hwndEditor):** Any implementation that uses `GetWindowTextA` or `EM_GETTEXTRANGE` with `TEXTRANGEA` should have a Unicode path (e.g. `GetWindowTextW` or `TEXTRANGEW` for Rich Edit).

---

## 4. Stub or Minimal Logic (Possible Qt-Feature Shrinkage)

These are not Qt symbols but could represent reduced or placeholder behavior relative to a richer Qt IDE:

| Approx. line | Area | Note |
|--------------|------|------|
| 2017–2039 | `copyWithFormatting()` | Comment says “Simplified: copy selected plain text and store in history”. No actual rich formatting (e.g. RTF) to clipboard. A full “copy with formatting” (Qt-style) would push RTF or HTML to clipboard. |
| 1221–1224 | `createSidebar(hwnd)` | Just calls `createPrimarySidebar(hwnd)`. No secondary or split sidebar logic; acceptable if by design. |
| 6959–6961 | “DEFERRED IMPLEMENTATIONS — PowerShell Panel Dock/Float” | Implementation is present (dock/float logic). Title is historical; not a stub. |

No other obvious “TODO”, “FIXME”, or “unimplemented” blocks were found in Win32IDE.cpp that clearly denote Qt-origin missing code.

---

## 5. Recommendations

1. **Keep file Qt-free**  
   Do not reintroduce Qt includes or types in Win32IDE.cpp.

2. **Plan ANSI → Unicode (W) conversion**  
   - Prioritize: file open/save dialogs (`GetOpenFileNameW` / `GetSaveFileNameW`), message boxes (`MessageBoxW`), and editor/content paths (`CHARFORMAT2W`, `SetWindowTextW`, `GetWindowTextW`, Rich Edit Unicode).  
   - Then: Find/Replace and Snippet Manager dialogs (window class and all controls to W).  
   - Then: Floating panel, output tabs, and menu bar to `CreateWindowExW` / `AppendMenuW` and wide strings.  
   - Use a single Unicode codepage (e.g. UTF-16) and convert at boundaries if internal logic remains `std::string` (UTF-8).

3. **Clipboard**  
   Prefer `CF_UNICODETEXT` when putting editor/content text on the clipboard so other apps get proper Unicode.

4. **Rich Edit**  
   Ensure the main editor is created with the Unicode Rich Edit class (`MSFTEDIT_CLASS` or `RICHEDIT_CLASSW`) and that all `CHARFORMAT2` / `TEXTRANGE` usage is the W variant.

5. **No Qt-specific stubs to add**  
   The file does not contain “missing Qt” stubs; only the above ANSI→Unicode and minor behavior (e.g. copy with formatting) are candidates for improvement.

---

## 6. File / Line Quick Reference (ANSI usage)

- **MessageBoxA:** ~1232, 1263, 1309, 1310, 1352, 1355, 1383, 1385, 1392, 1393, 1412, 2933, and any other message boxes in the file.
- **CreateWindowExA / CreateDialogParamA:** ~2825–2846, 2859–2890, 3294–3340, 2588–2648, 1946.
- **GetOpenFileNameA / GetSaveFileNameA / OPENFILENAMEA:** ~1273–1287, 1403–1416.
- **SetWindowTextA / GetWindowTextA / GetWindowTextLengthA:** ~1245, 1294, 1330, 1369, 1991, 2677, and editor/text helpers.
- **CHARFORMAT2A / TEXTRANGEA / CP_ACP:** ~2006–2014, 3344–3350, 2024–2025.
- **AppendMenuA:** ~354–520 in `createMenuBar`.
- **WNDCLASSEXA / RegisterClassExA:** ~2588–2597.
- **SendMessageA (with text):** ~2646–2648, 2678–2679, 2695–2698.

**Addendum:** Status bar text around 1369/3728 uses `(LPARAM)(... + filepath).c_str()` (temporary); use a persistent buffer or `SendMessageW` + `std::wstring`. Command input clear at ~1469 uses `SetWindowTextA`; `getWindowText`/editor in findText uses ANSI — convert when editor is Unicode.

Use this list to drive a systematic replace pass (A to W and string literals to wide literals or std::wstring) while keeping internal UTF-8 std::string where it simplifies file I/O and parsing.

**Addendum:** Status bar at 1369/3728 passes a temporary c_str to SendMessage; use persistent buffer or SendMessageW. Command input at 1469 uses SetWindowTextA. findText uses GetWindowTextA on editor; convert when editor is Unicode.

---

## 7. Addendum — Additional ANSI / Logic Notes

| Location | Item | Note |
|----------|------|------|
| ~1369, ~3728 | `SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)("Model: " + std::string(filepath)).c_str())` | Passing pointer to temporary; use a persistent buffer or `std::wstring` + `SendMessageW` to avoid undefined behavior. |
| ~1469 | `SetWindowTextA(m_hwndCommandInput, "")` | Command input cleared via ANSI; use `SetWindowTextW` if control is Unicode. |
| `getWindowText(m_hwndEditor)` / `getWindowText(m_hwndCommandInput)` | Implementation (likely in Win32IDE_Core or same file) | If implemented with `GetWindowTextA` or `EM_GETTEXTRANGE` + `TEXTRANGEA`, add Unicode path for editor/command input. |
| Find/Replace | `findText` / `replaceText` | Use `GetWindowTextLengthA`/`GetWindowTextA` on editor (2884–2886); convert to Unicode path when editor is Unicode Rich Edit. |
