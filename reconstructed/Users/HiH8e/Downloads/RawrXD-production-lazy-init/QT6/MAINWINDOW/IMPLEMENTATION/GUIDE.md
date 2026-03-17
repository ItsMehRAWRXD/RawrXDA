# Qt6 Main Window Implementation Quick Reference

## Overview
The qt6_main_window.asm file contains 25 functions that need implementation. This document provides implementation guidance for each.

## Implementation Priority

### Priority 1: Core Window Management (MUST HAVE)
These functions are required for the window system to work:

1. **main_window_create()** - Create main HWND
2. **main_window_destroy()** - Cleanup window
3. **main_window_show()** - Make window visible
4. **main_window_hide()** - Hide window
5. **main_window_on_close()** - Handle close event

### Priority 2: Menu System (ESSENTIAL)
Required for user interactions:

6. **main_window_add_menu()** - Create menu
7. **main_window_add_menu_item()** - Add menu item
8. **main_window_update_menubar()** - Update menu positions

### Priority 3: Text Management (IMPORTANT)
Title and status display:

9. **main_window_set_title()** - Set window title
10. **main_window_get_title()** - Get window title
11. **main_window_set_status()** - Set status bar text
12. **main_window_get_status()** - Get status bar text

### Priority 4: Geometry & Events (SUPPORTING)
Window positioning and sizing:

13. **main_window_set_geometry()** - Set window position/size
14. **main_window_get_geometry()** - Get window position/size
15. **main_window_on_resize()** - Handle resize event

### Priority 5: System Functions (LAST)
Initialization/cleanup:

16. **main_window_system_init()** - Register window class
17. **main_window_system_cleanup()** - Unregister window class

---

## Implementation Template

### Each Function Should Follow:

```asm
; =============== function_name ===============
; Brief description of what it does
; 
; Inputs:  rcx = param1, rdx = param2, ...
; Outputs: rax = return value
; Destroys: rcx, rdx, r8, r9, r10, r11 (call-clobbered)
; Preserves: rbx, rsi, rdi, r12-r15 (call-preserved)
;
function_name PROC
    push rbp
    mov rbp, rsp
    sub rsp, XX                         ; Local variables
    
    ; Implementation here
    ; - Save arguments to stack if needed (rcx is clobbered)
    ; - Call Win32 APIs (they clobber rcx, rdx, r8, r9, r10, r11)
    ; - Manage memory (malloc/free)
    ; - Update structure fields
    ; - Return result in rax
    
    mov rax, result_value
    add rsp, XX
    pop rbp
    ret
function_name ENDP
```

---

## Implementation Details by Function

### 1. main_window_system_init() - 50 LOC

**What it does**: Register the "QMainWindow" window class with Windows

**Implementation steps**:
```asm
1. Allocate WNDCLASSEX structure on stack (80 bytes)
2. Fill out fields:
   - cbSize = 80
   - style = CS_VREDRAW | CS_HREDRAW
   - lpfnWndProc = (QWORD)&main_window_proc
   - cbClsExtra = 0
   - cbWndExtra = 8  (store pointer to MAIN_WINDOW instance)
   - hInstance = hInstance (get from kernel32)
   - hIcon = NULL (or default icon)
   - hCursor = LoadCursor(NULL, IDC_ARROW)
   - hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1)
   - lpszMenuName = NULL (menus are added separately)
   - lpszClassName = "QMainWindow"
   - hIconSm = NULL
3. Call RegisterClassEx(addr of WNDCLASSEX)
4. Check return value (class atom, or 0 on error)
5. Return 1 for success, 0 for error
```

**Key Win32 APIs**:
- RegisterClassEx(LPCWNDCLASSEX lpWndClass)
- LoadCursor(HINSTANCE hInstance, LPCSTR lpCursorName)

---

### 2. main_window_create() - 120 LOC

**What it does**: Allocate MAIN_WINDOW structure and create HWND

**Implementation steps**:
```asm
1. Allocate MAIN_WINDOW structure (~160 bytes)
   - malloc(160) → rax
   - Save pointer for later

2. Initialize OBJECT_BASE fields:
   - vmt = address of main_window_vmt table
   - hwnd = NULL (filled in later)
   - parent = NULL
   - children = NULL
   - child_count = 0

3. Allocate title buffer (512 bytes)
   - malloc(512) → buffer
   - Copy rcx (title param) to buffer
   - Store pointer in MAIN_WINDOW.title_text

4. Allocate status buffer (256 bytes)
   - malloc(256) → buffer
   - Initialize to "Ready" or empty
   - Store pointer in MAIN_WINDOW.status_text

5. Create main HWND with CreateWindowEx:
   - dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE
   - lpClassName = "QMainWindow"
   - lpWindowName = title buffer
   - dwStyle = WS_OVERLAPPEDWINDOW
   - x = CW_USEDEFAULT
   - y = CW_USEDEFAULT
   - nWidth = rdx (width param)
   - nHeight = r8 (height param)
   - hWndParent = NULL
   - hMenu = NULL (menu set later)
   - hInstance = GetModuleHandle(NULL)
   - lpParam = addr of MAIN_WINDOW (stored in window extra bytes)

6. Store HWND in MAIN_WINDOW.hwnd and g_main_hwnd

7. Create child windows:
   - Menu bar: CreateWindowEx(..., "STATIC", ...) at y=0, height=24
   - Toolbar: CreateWindowEx(..., "STATIC", ...) at y=24, height=32
   - Status bar: CreateWindowEx(..., "STATIC", ...) at bottom
   - Client area: CreateWindowEx(..., "STATIC", ...) in middle

8. Register in object registry (call object_create from foundation)

9. Return pointer to MAIN_WINDOW in rax
```

**Key Win32 APIs**:
- CreateWindowEx(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
- GetModuleHandle(LPCSTR lpModuleName)

**Memory Layout**:
```
MAIN_WINDOW structure (160 bytes):
  [0-40]   OBJECT_BASE (base object)
  [40-48]  hwnd_menubar (8 bytes)
  [48-56]  hwnd_toolbar (8 bytes)
  [56-64]  hwnd_statusbar (8 bytes)
  [64-72]  hwnd_client (8 bytes)
  [72-76]  x (4 bytes)
  [76-80]  y (4 bytes)
  [80-84]  width (4 bytes)
  [84-88]  height (4 bytes)
  [88-96]  menus_ptr (8 bytes)
  [96-100] menu_count (4 bytes)
  [100-104] max_menus (4 bytes)
  [104-112] status_text (8 bytes) → points to 256-byte buffer
  [112-116] flags (4 bytes)
  [116-124] title_text (8 bytes) → points to 512-byte buffer
  [124-160] padding/reserved
```

---

### 3. main_window_show() - 15 LOC

**What it does**: Make window visible

**Implementation**:
```asm
1. Get HWND from MAIN_WINDOW.hwnd (rcx = window, offset +8)
2. Call ShowWindow(hwnd, SW_SHOW)
3. Call UpdateWindow(hwnd)
4. Set FLAG_VISIBLE in MAIN_WINDOW.flags
5. Return 1 (success)
```

**Key Win32 APIs**:
- ShowWindow(HWND hWnd, int nCmdShow) where nCmdShow = 5 (SW_SHOW)
- UpdateWindow(HWND hWnd)

---

### 4. main_window_hide() - 15 LOC

**Implementation**:
```asm
1. Get HWND from MAIN_WINDOW.hwnd
2. Call ShowWindow(hwnd, SW_HIDE)
3. Clear FLAG_VISIBLE in MAIN_WINDOW.flags
4. Return 1 (success)
```

---

### 5. main_window_destroy() - 60 LOC

**What it does**: Clean up all resources

**Implementation**:
```asm
1. For each child window (menubar, toolbar, statusbar, client):
   - If HWND != NULL, call DestroyWindow(hwnd)

2. Destroy main HWND:
   - Call DestroyWindow(hwnd)

3. Free allocated buffers:
   - Free title_text buffer (512 bytes) with free()
   - Free status_text buffer (256 bytes) with free()
   - Free all menu items (walk linked list, free each MENU_ITEM)
   - Free all menu items (walk linked list, free each MENU_BAR_ITEM)

4. Free MAIN_WINDOW structure itself with free()

5. Clear globals:
   - g_main_window_global = NULL
   - g_main_hwnd = NULL

6. Return 1 (success)
```

---

### 6. main_window_set_title() - 30 LOC

**Implementation**:
```asm
1. rcx = MAIN_WINDOW ptr, rdx = new title (LPSTR)

2. Copy rdx to title_text buffer:
   - Get title_text pointer from MAIN_WINDOW
   - Copy max 512 bytes from rdx
   - Use rep movsb or memcpy-like pattern

3. Call SetWindowText(hwnd, title_buffer)

4. Return 1 (success)
```

---

### 7. main_window_get_title() - 10 LOC

**Implementation**:
```asm
1. rcx = MAIN_WINDOW ptr
2. Load title_text field offset from MAIN_WINDOW
3. Return pointer in rax
```

---

### 8. main_window_add_menu() - 80 LOC

**Implementation**:
```asm
1. Allocate MENU_BAR_ITEM structure (~96 bytes)
   - malloc(96) → menu_item

2. Initialize fields:
   - name_ptr: copy rdx (menu name) to allocated buffer
   - name_len: r8 (length param)
   - hwnd_dropdown = NULL (filled in later)
   - items_ptr = NULL (populated by add_menu_item)
   - item_count = 0
   - flags = 0
   - next = NULL (will link to list)

3. Create dropdown HMENU:
   - Call CreateMenu() → rax (HMENU)
   - Store in menu_item.hwnd_dropdown

4. Link to menu list:
   - If MAIN_WINDOW.menus_ptr == NULL:
     - Set MAIN_WINDOW.menus_ptr = menu_item
   - Else:
     - Walk to end of linked list (next != NULL)
     - Set last.next = menu_item

5. Increment MAIN_WINDOW.menu_count

6. Return menu_item pointer in rax
```

**Key Win32 APIs**:
- CreateMenu() → HMENU

---

### 9. main_window_set_status() - 30 LOC

**Implementation**:
```asm
1. rcx = MAIN_WINDOW ptr, rdx = status text

2. Copy rdx to status_text buffer (256 bytes max)

3. If hwnd_statusbar != NULL:
   - Call SetWindowText(hwnd_statusbar, status_buffer)

4. Return 1 (success)
```

---

### 10. main_window_on_resize() - 50 LOC

**Implementation**:
```asm
1. Update MAIN_WINDOW.width and MAIN_WINDOW.height

2. Recalculate child window positions:
   - Menu bar: y=0, height=24, width=full
   - Toolbar: y=24, height=32, width=full
   - Client: y=56, height=(total_height - 56 - 20), width=full
   - Status bar: y=(total_height - 20), height=20, width=full

3. For each child, call MoveWindow(hwnd, x, y, width, height, TRUE)

4. Post EVENT_RESIZE to event queue so layouts recalculate

5. Return 1 (success)
```

**Key Win32 APIs**:
- MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)

---

### 11. main_window_get_status() - 10 LOC

**Implementation**:
```asm
1. rcx = MAIN_WINDOW ptr
2. Load status_text field
3. Return pointer in rax
```

---

## VMT Table (Virtual Method Table)

Create a static table at the end of the file:

```asm
main_window_vmt:
    dq main_window_destroy_vmt      ; pfn_destroy
    dq main_window_paint_vmt        ; pfn_paint
    dq main_window_on_event_vmt     ; pfn_on_event
    dq main_window_get_size_vmt     ; pfn_get_size
    dq main_window_set_size_vmt     ; pfn_set_size
    dq main_window_show_vmt         ; pfn_show
    dq main_window_hide_vmt         ; pfn_hide
```

Each VMT function is a wrapper that extracts the MAIN_WINDOW pointer from the OBJECT_BASE and calls the real implementation.

---

## Window Procedure

**main_window_proc** - called by Windows for window messages:

```asm
main_window_proc PROC
    ; rcx = hwnd
    ; rdx = message (uMsg)
    ; r8 = wParam
    ; r9 = lParam

    cmp rdx, WM_CREATE
    je on_create

    cmp rdx, WM_DESTROY
    je on_destroy

    cmp rdx, WM_SIZE
    je on_size

    cmp rdx, WM_PAINT
    je on_paint

    cmp rdx, WM_CLOSE
    je on_close

    ; Default: call DefWindowProc
    jmp defproc

on_create:
    ; Store window pointer from lParam to window extra bytes
    ret

on_destroy:
    ; PostQuitMessage(0)
    ret

on_size:
    ; Extract width/height from lParam
    ; Call main_window_on_resize
    ret

on_paint:
    ; Call BeginPaint/EndPaint
    ; Dispatch to main_window_paint_vmt
    ret

on_close:
    ; Call main_window_on_close
    ; If returns 1, allow close, else prevent
    ret

defproc:
    ; Call DefWindowProc(hwnd, uMsg, wParam, lParam)
    ret

main_window_proc ENDP
```

---

## Constants & Flags

Define at top of file:

```asm
; Window flags
FLAG_VISIBLE        EQU 0x0001
FLAG_DIRTY          EQU 0x0002
FLAG_HAS_MENU       EQU 0x0004
FLAG_HAS_TOOLBAR    EQU 0x0008
FLAG_HAS_STATUSBAR  EQU 0x0010

; Menu item flags
FLAG_SEPARATOR      EQU 0x0001
FLAG_CHECKED        EQU 0x0002
FLAG_ENABLED        EQU 0x0004
FLAG_GRAYED         EQU 0x0008

; Window class name
WNDCLASS_NAME       DB "QMainWindow", 0

; Child window class names
MENUBAR_CLASS       DB "MENUBAR", 0
TOOLBAR_CLASS       DB "TOOLBAR", 0
STATUSBAR_CLASS     DB "STATUSBAR", 0
CLIENT_CLASS        DB "CLIENT", 0
```

---

## Testing Checklist

After implementation, verify:

- [ ] Window class registers successfully
- [ ] CreateWindowEx creates window with correct title
- [ ] ShowWindow makes window visible
- [ ] HideWindow hides window
- [ ] SetWindowText updates title bar
- [ ] Menu bar renders
- [ ] Menu items appear when clicked
- [ ] Resize events trigger layout recalculation
- [ ] Status bar displays text
- [ ] Close button calls on_close handler
- [ ] Memory cleanup on destroy (no leaks)

---

## Integration with Foundation

qt6_main_window.asm will integrate with qt6_foundation.asm:

```asm
; In qt6_foundation.asm (already exists):
object_create(MAIN_WINDOW_TYPE) → calls main_window_create()
object_destroy(window) → calls main_window_destroy via VMT

; In qt6_foundation.asm event queue:
post_event(window, EVENT_RESIZE, width, height) → queued
process_events() → dispatches to main_window_on_resize()

; In qt6_foundation.asm signal/slot:
connect_signal(window, SIGNAL_CLOSE, ...) → allows custom close handler
emit_signal(window, SIGNAL_CLOSE) → calls connected slots
```

---

## Size Estimates

| Function | LOC | Complexity |
|----------|-----|------------|
| main_window_system_init | 50 | Medium |
| main_window_system_cleanup | 15 | Simple |
| main_window_create | 120 | High |
| main_window_destroy | 60 | High |
| main_window_show | 15 | Simple |
| main_window_hide | 15 | Simple |
| main_window_set_title | 30 | Simple |
| main_window_get_title | 10 | Simple |
| main_window_get_status | 10 | Simple |
| main_window_set_status | 30 | Simple |
| main_window_add_menu | 80 | Medium |
| main_window_add_menu_item | 80 | Medium |
| main_window_on_resize | 50 | Medium |
| main_window_on_close | 15 | Simple |
| main_window_set_geometry | 40 | Simple |
| main_window_get_geometry | 15 | Simple |
| main_window_update_menubar | 50 | Medium |
| main_window_proc | 80 | High |
| VMT table | 20 | Simple |
| VMT wrappers (7x) | 60 | Simple |
| **TOTAL** | **900** | **~1.5 hours work** |

---

## File References

- qt6_foundation.asm - Provides OBJECT_BASE, VMT_BASE, registry, event queue
- qt6_main_window.asm - This file
- windows.inc - Win32 API definitions
- CMakeLists.txt - Build integration

---

**Next Step**: Implement Priority 1 functions, then verify compilation with ml64.exe
