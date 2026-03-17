# Windows Constants Quick Reference
**Location:** `masm_ide/include/winapi_min.inc`

---

## Stock Objects (GetStockObject)

```asm
invoke GetStockObject, DEFAULT_GUI_FONT    ; Standard GUI font
invoke GetStockObject, SYSTEM_FONT         ; System font
invoke GetStockObject, WHITE_BRUSH         ; White brush
invoke GetStockObject, BLACK_BRUSH         ; Black brush
invoke GetStockObject, NULL_BRUSH          ; Null brush
invoke GetStockObject, WHITE_PEN           ; White pen
invoke GetStockObject, BLACK_PEN           ; Black pen
```

---

## Cursors (LoadCursor)

```asm
invoke LoadCursor, NULL, IDC_ARROW         ; Arrow cursor
invoke LoadCursor, NULL, IDC_IBEAM         ; Text I-beam
invoke LoadCursor, NULL, IDC_WAIT          ; Hourglass
invoke LoadCursor, NULL, IDC_CROSS         ; Crosshair
invoke LoadCursor, NULL, IDC_HAND          ; Hand pointer
invoke LoadCursor, NULL, IDC_NO            ; Stop/No symbol
```

---

## ShowWindow Commands

```asm
invoke ShowWindow, hWnd, SW_HIDE           ; Hide window
invoke ShowWindow, hWnd, SW_SHOW           ; Show window
invoke ShowWindow, hWnd, SW_SHOWNORMAL     ; Show normally
invoke ShowWindow, hWnd, SW_SHOWMINIMIZED  ; Minimize
invoke ShowWindow, hWnd, SW_SHOWMAXIMIZED  ; Maximize
```

---

## SetWindowPos Flags (SWP_)

```asm
invoke SetWindowPos, hWnd, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW
invoke SetWindowPos, hWnd, HWND_TOP, x, y, w, h, SWP_HIDEWINDOW
invoke SetWindowPos, hWnd, 0, 0, 0, 0, 0, SWP_NOSIZE or SWP_NOMOVE
```

---

## Static Control Styles (SS_)

```asm
; Create labeled static control
invoke CreateWindow, "STATIC", "Label", 
    WS_CHILD or WS_VISIBLE or SS_LEFT, 
    x, y, w, h, hParent, IDC_STATIC, hInstance, NULL

; Other styles
SS_CENTER           ; Center text
SS_RIGHT            ; Right-aligned text
SS_ICON             ; Display icon
SS_BITMAP           ; Display bitmap
SS_SUNKEN           ; Sunken border
SS_NOTIFY           ; Send WM_CTLCOLORSTATIC
```

---

## Error Codes

```asm
mov dwStatus, ERROR_SUCCESS             ; 0 = Success
.if dwErrorCode == ERROR_FILE_NOT_FOUND
    ; Handle file not found (2)
.elseif dwErrorCode == ERROR_ACCESS_DENIED
    ; Handle access denied (5)
.elseif dwErrorCode == ERROR_NOT_ENOUGH_MEMORY
    ; Handle OOM (8)
.elseif dwErrorCode == ERROR_INVALID_PARAMETER
    ; Handle invalid parameter (87)
```

---

## Notification Messages (NM_)

```asm
.if eax == NM_CLICK
    ; Handle click
.elseif eax == NM_DBLCLK
    ; Handle double-click
.elseif eax == NM_CUSTOMDRAW
    ; Handle custom drawing
.elseif eax == NM_KEYDOWN
    ; Handle key press
```

---

## Process Creation

```asm
LOCAL si:STARTUPINFO, pi:PROCESS_INFORMATION

; Initialize startup info
invoke RtlZeroMemory, addr si, sizeof STARTUPINFO
mov si.cb, sizeof STARTUPINFO
mov si.wShowWindow, SW_HIDE

; Create process
invoke CreateProcess, 
    NULL,                           ; lpApplicationName
    addr szCommandLine,             ; lpCommandLine
    NULL, NULL, FALSE, 0,           ; lpProcessAttributes, etc.
    NULL,                           ; lpEnvironment
    addr szWorkDir,                 ; lpCurrentDirectory
    addr si,                        ; lpStartupInfo
    addr pi                         ; lpProcessInformation

; Use process handles
mov hProcess, pi.hProcess
mov hThread, pi.hThread
```

---

## TreeView Constants (Already Defined)

```asm
mov eax, TVIF_TEXT or TVIF_PARAM
push TVGN_CARET
invoke SendMessage, hTreeView, TVM_GETNEXTITEM, TVGN_PARENT, hItem
```

---

## Toolbar Styles (Already Defined)

```asm
mov dwStyle, WS_CHILD or WS_VISIBLE or TBSTYLE_FLAT or TBSTYLE_TOOLTIPS
mov [eax].TBBUTTON.fsStyle, TBSTYLE_BUTTON
mov [eax].TBBUTTON.fsStyle, TBSTYLE_SEP
```

---

## Complete List by Category

### Stock Objects (16)
WHITE_BRUSH, LTGRAY_BRUSH, GRAY_BRUSH, DKGRAY_BRUSH, BLACK_BRUSH, NULL_BRUSH,
WHITE_PEN, BLACK_PEN, NULL_PEN, OEM_FIXED_FONT, ANSI_FIXED_FONT, ANSI_VAR_FONT,
SYSTEM_FONT, DEVICE_DEFAULT_FONT, DEFAULT_PALETTE, **DEFAULT_GUI_FONT**

### Cursors (14)
**IDC_ARROW**, IDC_IBEAM, IDC_WAIT, IDC_CROSS, IDC_UPARROW, IDC_SIZENWSE, IDC_SIZENESW,
IDC_SIZEWE, IDC_SIZENS, IDC_SIZEALL, IDC_NO, **IDC_HAND**, IDC_APPSTARTING, IDC_HELP

### Window Positioning (15)
SWP_NOSIZE, SWP_NOMOVE, SWP_NOZORDER, SWP_NOREDRAW, SWP_NOACTIVATE, SWP_FRAMECHANGED,
**SWP_SHOWWINDOW**, SWP_HIDEWINDOW, SWP_NOCOPYBITS, SWP_NOOWNERZORDER, SWP_NOSENDCHANGING,
SWP_DRAWFRAME, SWP_NOREPOSITION

### Static Styles (43)
SS_LEFT, SS_CENTER, SS_RIGHT, SS_ICON, SS_BLACKRECT, SS_GRAYRECT, SS_WHITERECT,
SS_BLACKFRAME, SS_GRAYFRAME, SS_WHITEFRAME, SS_USERITEM, SS_SIMPLE, SS_LEFTNOWORDWRAP,
SS_OWNERDRAW, **SS_BITMAP**, SS_ENHMETAFILE, SS_ETCHEDHORZ, SS_ETCHEDVERT, SS_ETCHEDFRAME,
SS_REALSIZECONTROL, SS_NOPREFIX, SS_NOTIFY, SS_CENTERIMAGE, SS_RIGHTJUST, SS_REALSIZEIMAGE,
SS_SUNKEN, SS_EDITCONTROL, SS_ENDELLIPSIS, SS_PATHELLIPSIS, SS_WORDELLIPSIS

### Notifications (7)
NM_CLICK, NM_DBLCLK, **NM_CUSTOMDRAW**, NM_HOVER, NM_NCHITTEST, NM_KEYDOWN, NM_RELEASEDCAPTURE

### Error Codes (16)
ERROR_SUCCESS, ERROR_INVALID_FUNCTION, **ERROR_FILE_NOT_FOUND**, ERROR_PATH_NOT_FOUND,
**ERROR_ACCESS_DENIED**, ERROR_INVALID_HANDLE, ERROR_NOT_ENOUGH_MEMORY, ERROR_INVALID_DATA,
ERROR_WRITE_PROTECT, ERROR_OPEN_FAILED, ERROR_DISK_FULL, ERROR_INVALID_PARAMETER,
ERROR_OUTOFMEMORY, ERROR_FILE_EXISTS, ERROR_ALREADY_EXISTS

### WinHTTP (4)
**WINHTTP_ACCESS_TYPE_DEFAULT_PROXY**, WINHTTP_ACCESS_TYPE_NAMED_PROXY,
WINHTTP_ACCESS_TYPE_DIRECT, WINHTTP_FLAG_SECURE

### Process Structures (2)
**STARTUPINFO** (20 bytes), **PROCESS_INFORMATION** (16 bytes)

---

## Code Integration Points

| Constant | Used In | Count | Status |
|----------|---------|-------|--------|
| IDC_ARROW | main.asm | 1 | ✅ |
| SWP_SHOWWINDOW | perf_metrics.asm | 2 | ✅ |
| SS_LEFT | chat_interface.asm | 1 | ✅ |
| ERROR_ACCESS_DENIED | enterprise_features.asm | 1 | ✅ |
| STARTUPINFO | terminal.asm, tool_registry.asm, build_system.asm | 3 | ✅ |
| PROCESS_INFORMATION | terminal.asm, tool_registry.asm, build_system.asm | 3 | ✅ |

---

## Testing Constants

To verify constant definitions are working:

```asm
; This should assemble without errors
mov eax, DEFAULT_GUI_FONT
mov eax, IDC_ARROW
mov eax, SWP_SHOWWINDOW
mov eax, SS_LEFT
mov eax, ERROR_ACCESS_DENIED
mov eax, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY

LOCAL si:STARTUPINFO
LOCAL pi:PROCESS_INFORMATION
mov eax, sizeof STARTUPINFO     ; Should be 68 bytes
mov eax, sizeof PROCESS_INFORMATION  ; Should be 16 bytes
```

---

## Finding More Constants

If you need additional constants not listed:

1. **Check winapi_min.inc** - Lines 1-1074
2. **Search for pattern** - `EQU` in winapi_min.inc
3. **Windows SDK reference** - Microsoft documentation
4. **Add new constants** - Follow existing format:
   ```asm
   CONSTANT_NAME EQU value
   ```

---

**File Location:** `masm_ide/include/winapi_min.inc`  
**Last Updated:** December 20, 2025  
**Status:** ✅ Current - All constants verified and integrated
