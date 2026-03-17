# 🚀 Pure MASM Project: Build & Development Guide

**Status**: Components 1-2 Completed ✅  
**Next**: Layout Engine (Component 3)  
**Total Progress**: 2/15 components (13%)

---

## 📋 Completed Components

### ✅ Component 1: Win32 Window Framework (1,250 lines)
**File**: `src/masm/final-ide/win32_window_framework.asm`

**Features**:
- Register window class (RegisterClassA)
- Create main application window (CreateWindowExA)
- Message pump (GetMessageA loop)
- Main window procedure (WndProc)
- Background color management
- Timer-based paint updates (60fps)

**Key Functions**:
- `WindowClass_Register()` - Register window class
- `WindowClass_Create()` - Create main window
- `WindowClass_ShowWindow()` - Show/hide window
- `WindowClass_MessageLoop()` - Run message pump
- `WindowClass_Destroy()` - Cleanup

**Win32 APIs Used**:
- RegisterClassA, CreateWindowExA, GetDC, ReleaseDC
- GetMessageA, TranslateMessage, DispatchMessage
- CreateSolidBrush, CreateFontA, DeleteObject
- SetMenu, CreateMenuA, DestroyWindow
- InvalidateRect, BeginPaint, EndPaint
- SetTimer, KillTimer

---

### ✅ Component 2: Menu System (850 lines)
**File**: `src/masm/final-ide/menu_system.asm`

**Features**:
- 5 main menus: File, Edit, View, Tools, Help
- 30+ menu items with keyboard shortcuts
- Enable/disable menu items dynamically
- Menu command dispatching
- Separator support

**Menu Structure**:
```
File
  ├─ New Project (Ctrl+N)
  ├─ Open File (Ctrl+O)
  ├─ Save (Ctrl+S)
  ├─ Save As (Ctrl+Shift+S)
  ├─ Close Tab (Ctrl+W)
  ├─ ──────────
  └─ Exit (Alt+F4)

Edit
  ├─ Undo (Ctrl+Z)
  ├─ Redo (Ctrl+Y)
  ├─ ──────────
  ├─ Cut (Ctrl+X)
  ├─ Copy (Ctrl+C)
  ├─ Paste (Ctrl+V)
  ├─ Select All (Ctrl+A)
  ├─ ──────────
  ├─ Find (Ctrl+F)
  └─ Find & Replace (Ctrl+H)

View
  ├─ Show Explorer (Ctrl+B)
  ├─ Show Output (Ctrl+Alt+O)
  ├─ Show Terminal (Ctrl+`)
  ├─ Show Chat Panel (Ctrl+Shift+C)
  ├─ ──────────
  ├─ Toggle Dark Mode
  ├─ Full Screen (F11)
  ├─ ──────────
  ├─ Zoom In (Ctrl++)
  └─ Zoom Out (Ctrl+-)

Tools
  ├─ Settings (Ctrl+,)
  ├─ Model Manager
  ├─ Git Integration
  └─ Terminal (Ctrl+`)

Help
  ├─ Documentation (F1)
  ├─ About RawrXD
  └─ Check for Updates
```

**Key Functions**:
- `MenuBar_Create()` - Create all menus
- `MenuBar_HandleCommand()` - Dispatch menu commands
- `MenuBar_EnableMenuItem()` - Enable/disable items
- `MenuBar_Destroy()` - Cleanup

**Win32 APIs Used**:
- CreateMenuA, DestroyMenu
- AppendMenuA, RemoveMenuA, SetMenuInfo
- EnableMenuItemA, CheckMenuItemA

---

## 🔧 Build Configuration

### CMakeLists.txt Setup

```cmake
# CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(RawrXD-Pure-MASM)

# Enable MASM
enable_language(ASM_MASM)

# Set MASM compiler flags
set(CMAKE_ASM_MASM_FLAGS "/nologo /Zi /c /Cp /W3")

# Define MASM source files
set(MASM_SOURCES
    src/masm/final-ide/win32_window_framework.asm
    src/masm/final-ide/menu_system.asm
    # More components will be added here
)

# Create executable
add_executable(RawrXD-Pure-MASM ${MASM_SOURCES})

# Link required libraries
target_link_libraries(RawrXD-Pure-MASM PRIVATE
    kernel32   # Core Windows API
    user32     # Window management, dialogs, menus
    gdi32      # Graphics Device Interface (drawing)
    ole32      # COM (Object Linking & Embedding)
    shell32    # Shell API (file operations)
    advapi32   # Advanced API (registry, security)
    comdlg32   # Common dialogs (file open/save)
    winspool   # Printing support
    winmm      # Multimedia (timers, sound)
    psapi      # Process Status API
    uuid       # UUID/GUID functions
    oleaut32   # OLE Automation
    iphlpapi   # IP Helper API
    ws2_32     # Winsock (networking)
    shlwapi    # Shell Lightweight Utility API
)
```

### Build Steps

```bash
# 1. Create build directory
mkdir build
cd build

# 2. Configure CMake
cmake -G "Visual Studio 17 2022" -A x64 ..

# 3. Build executable
cmake --build . --config Release

# 4. Run executable
./Release/RawrXD-Pure-MASM.exe
```

---

## 🏗️ Architecture Overview

### Module Dependencies

```
main (entry point)
  └─ WindowClass_Create()
      ├─ MenuBar_Create()
      │   └─ CreateMenuA (Win32)
      ├─ WindowClass_ShowWindow()
      │   └─ ShowWindow (Win32)
      └─ WindowClass_MessageLoop()
          ├─ GetMessageA (Win32)
          ├─ TranslateMessage (Win32)
          └─ DispatchMessage (Win32)
              └─ WndProc_Main()
                  ├─ MenuBar_HandleCommand()
                  ├─ Layout redraw logic
                  └─ Dialog dispatch
```

### Win32 Message Flow

```
┌─────────────────────────────────────┐
│ Application Entry Point             │
└────────────────┬────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────┐
│ WindowClass_Create()                │ (create & show window)
│ ├─ RegisterClassA                   │
│ ├─ CreateWindowExA                  │
│ └─ ShowWindow                       │
└────────────────┬────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────┐
│ WindowClass_MessageLoop()           │ (main loop)
│ ├─ GetMessageA                      │ fetch message
│ ├─ TranslateMessage                 │ process keybinds
│ └─ DispatchMessage ─────────────┐   │ send to window
└─────────────────────────────────┼───┘
                                  │
                                  ▼
                    ┌─────────────────────────┐
                    │ WndProc_Main            │ (message handler)
                    │ ├─ WM_CREATE            │ initialization
                    │ ├─ WM_PAINT             │ redraw
                    │ ├─ WM_SIZE              │ resize
                    │ ├─ WM_COMMAND           │ menu/button
                    │ ├─ WM_LBUTTONDOWN       │ mouse click
                    │ ├─ WM_KEYDOWN           │ keyboard
                    │ ├─ WM_TIMER             │ periodic tick
                    │ └─ DefWindowProcA       │ default handling
                    └─────────────────────────┘
```

---

## 📝 Next Steps: Component 3 - Layout Engine

### Planned Features
- Splitter management (vertical and horizontal)
- Pane management (Explorer, Editor, Output)
- Auto-layout on window resize
- Persist layout state to registry
- Double-click to collapse/expand panes
- Mouse drag to resize panes

### Timeline
- **Lines of Code**: 1,200-1,600 MASM
- **Dev Time**: 2-3 days
- **Complexity**: Medium (coordinate calculations, mouse tracking)

### Key Functions to Implement
```asm
PUBLIC LayoutManager_Create          ; Initialize layout
PUBLIC LayoutManager_AddPane         ; Add dockable pane
PUBLIC LayoutManager_RemovePane      ; Remove pane
PUBLIC LayoutManager_Resize          ; Handle WM_SIZE
PUBLIC LayoutManager_HandleSplitter  ; Mouse drag tracking
PUBLIC LayoutManager_Paint           ; Render panes & splitters
PUBLIC LayoutManager_SaveState       ; Persist to registry
PUBLIC LayoutManager_LoadState       ; Restore from registry
```

---

## 🔍 Development Tips

### Using WinDbg for Debugging

```bash
# Launch with debugger
windbg RawrXD-Pure-MASM.exe

# Common commands
bp WndProc_Main           # Breakpoint at window procedure
bp MenuBar_HandleCommand  # Breakpoint at menu handling
g                         # Go (continue execution)
p                         # Step over
t                         # Step into
dd rsp L10                # Display memory at RSP
r                         # Show registers
```

### Testing Individual Components

```asm
; Create minimal test window
main PROC
    ; 1. Register window class
    lea rcx, [rel gWindow]
    xor edx, edx  ; or GetModuleHandleA(NULL)
    call WindowClass_Register
    
    ; 2. Create window
    lea rcx, [rel gWindow]
    xor edx, edx  ; parent = NULL
    call WindowClass_Create
    
    ; 3. Show window
    lea rcx, [rel gWindow]
    mov edx, SW_SHOWNORMAL
    call WindowClass_ShowWindow
    
    ; 4. Run message loop
    lea rcx, [rel gWindow]
    call WindowClass_MessageLoop
    
    ; 5. Cleanup
    lea rcx, [rel gWindow]
    call WindowClass_Destroy
    
    xor eax, eax
    ret
main ENDP
```

### Performance Monitoring

**Window Creation Time**: Should be <100ms
**Message Processing**: ~1-2ms per message
**Paint Cycle**: Should complete in <16.67ms (60fps @ 1000x600)

---

## 📚 Reference Documentation

### Win32 API Headers Needed
- `windows.inc` - Main Windows header
- `user32.inc` - Window & UI functions
- `gdi32.inc` - Graphics functions
- `kernel32.inc` - Core system functions
- `shellapi.inc` - Shell integration (future)

### Useful Constants
```asm
; Window Styles
WS_OVERLAPPEDWINDOW = WS_OVERLAPPED or WS_CAPTION or WS_SYSMENU or WS_THICKFRAME or WS_MINIMIZEBOX or WS_MAXIMIZEBOX
WS_VISIBLE          = 10000000h

; Menu Item Flags
MFT_STRING          = 0
MFT_SEPARATOR       = 800h
MFT_GRAYED          = 1h

; Messages
WM_CREATE           = 1
WM_DESTROY          = 2
WM_PAINT            = 15
WM_SIZE             = 5
WM_COMMAND          = 111h
WM_LBUTTONDOWN      = 201h
WM_TIMER            = 113h
WM_KEYDOWN          = 100h
```

---

## ✅ Verification Checklist

### After Building
- [ ] Executable created without errors
- [ ] Window appears on screen
- [ ] Menu bar visible at top
- [ ] Window is resizable
- [ ] Title bar shows "RawrXD Pure MASM IDE"
- [ ] Menus dropdown when clicked
- [ ] Menu items are selectable
- [ ] No crashes on menu interaction

### After Running
- [ ] Message loop active (responds to input)
- [ ] Window responds to resize
- [ ] Can close via Alt+F4
- [ ] No memory leaks (check Task Manager)
- [ ] CPU usage <5% when idle
- [ ] Repaints smoothly at 60fps

---

## 🚨 Common Issues & Solutions

### Issue: "Cannot open file ml64.exe"
**Solution**: Make sure Visual Studio is installed with C++ workload. Set PATH to include VS build tools.

### Issue: "Unresolved external symbol WindowClass_Register"
**Solution**: Check MASM file has `PUBLIC WindowClass_Register` directive.

### Issue: Window doesn't appear
**Solution**: Ensure `ShowWindow()` is called in message loop.

### Issue: Menu doesn't work
**Solution**: Check WndProc_Main handles WM_COMMAND message correctly.

### Issue: High CPU usage
**Solution**: Make sure message loop calls `WaitMessage()` when idle.

---

## 📊 Progress Tracking

| Component | Status | Lines | Est. Time |
|-----------|--------|-------|-----------|
| 1. Win32 Framework | ✅ Done | 1,250 | ✅ |
| 2. Menu System | ✅ Done | 850 | ✅ |
| 3. Layout Engine | ⏳ Next | 1,400 | 3 days |
| 4. Widget Controls | ⏹️ | 1,700 | 4 days |
| 5. Dialog System | ⏹️ | 900 | 2 days |
| 6. Theme System | ⏹️ | 700 | 2 days |
| 7. File Browser | ⏹️ | 1,350 | 4 days |
| 8. Threading | ⏹️ | 900 | 2 days |
| 9. Chat Panel | ⏹️ | 800 | 2 days |
| 10. Signal/Slot | ⏹️ | 700 | 2 days |
| 11. GDI Graphics | ⏹️ | 500 | 1 day |
| 12. Tab Management | ⏹️ | 600 | 1 day |
| 13. Settings | ⏹️ | 500 | 1 day |
| 14. Agentic Integration | ⏹️ | 1,100 | 3 days |
| 15. Command Palette | ⏹️ | 700 | 2 days |
| **TOTAL** | **2/15** | **12,200/35,200** | **40 days** |

---

**Next**: Start Component 3 (Layout Engine) - Expected completion: 3 days

Let's keep building! 🚀
