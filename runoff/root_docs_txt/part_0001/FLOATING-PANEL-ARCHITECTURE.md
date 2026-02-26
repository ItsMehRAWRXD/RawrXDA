# Non-Modal Floating Panel - Visual Architecture

## Window Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│  Desktop                                                         │
│                                                                   │
│  ┌────────────────────────────────┐  ┌──────────────────────┐   │
│  │ RawrXD IDE (Main Window)       │  │ RawrXD Tools         │   │
│  │ m_hwndMain                     │  │ m_hwndFloatingPanel  │   │
│  │                                │  │ WS_EX_TOPMOST        │   │
│  │ ┌────────────────────────────┐ │  │                      │   │
│  │ │ Menu Bar                   │ │  │ ┌──────────────────┐ │   │
│  │ │ File Edit View Terminal... │ │  │ │ Tab Control      │ │   │
│  │ └────────────────────────────┘ │  │ │ ID: 5001         │ │   │
│  │                                │  │ │ Quick Info       │ │   │
│  │ ┌────────────────────────────┐ │  │ │ Snippets | Help  │ │   │
│  │ │ Toolbar                    │ │  │ └──────────────────┘ │   │
│  │ │ [New] [Open] [Save] [Run]  │ │  │                      │   │
│  │ └────────────────────────────┘ │  │ ┌──────────────────┐ │   │
│  │                                │  │ │ Edit Control     │ │   │
│  │ ┌────────────────────────────┐ │  │ │ m_hwndFloating-  │ │   │
│  │ │ Editor (m_hwndEditor)      │ │  │ │ Content          │ │   │
│  │ │                            │ │  │ │                  │ │   │
│  │ │ function Test {            │ │  │ │ ╔══════════════╗ │ │   │
│  │ │     Write-Host "Hello"     │ │  │ │ ║ Quick Info   ║ │ │   │
│  │ │ }                          │ │  │ │ ╚══════════════╝ │ │   │
│  │ │                            │◄─┼──┼─│ IDE: Running ✓  │ │   │
│  │ │ (User can edit here)       │ │  │ │ Terminal: Active │ │   │
│  │ └────────────────────────────┘ │  │ │                  │ │   │
│  │                                │  │ │ Shortcuts:       │ │   │
│  │ ┌────────────────────────────┐ │  │ │  Ctrl+N - New    │ │   │
│  │ │ Terminal (m_hwndTerminal)  │ │  │ │  Ctrl+S - Save   │ │   │
│  │ │                            │ │  │ │  F5 - Run        │ │   │
│  │ │ PS C:\> Get-Process       │ │  │ └──────────────────┘ │   │
│  │ │ (Interactive terminal)     │ │  │                      │   │
│  │ └────────────────────────────┘ │  │ ┌──────────────────┐ │   │
│  │                                │  │ │ Ready | Non-Modal│ │   │
│  │ ┌────────────────────────────┐ │  │ └──────────────────┘ │   │
│  │ │ Status Bar                 │ │  │                      │   │
│  │ │ Line: 3 | Ready            │ │  └──────────────────────┘   │
│  │ └────────────────────────────┘ │         ▲                   │
│  └────────────────────────────────┘         │                   │
│           │                                  │                   │
│           └──────────────────────────────────┘                   │
│              Parent-Child Relationship                           │
│              (Floating panel owned by main)                      │
└─────────────────────────────────────────────────────────────────┘
```

## Interaction Flow

```
User Action                  Main Window             Floating Panel
───────────                  ───────────             ──────────────

1. Click "View > Floating Panel"
     │                           │                         │
     ├──────────────────────────►│                         │
     │                           │ toggleFloatingPanel()   │
     │                           ├────────────────────────►│
     │                           │                         │ createFloatingPanel()
     │                           │                         │ [if not exists]
     │                           │                         │
     │                           │◄────────────────────────┤
     │                           │ Panel visible           │
     │                           │                         │
2. Edit code in main window   │                         │
     │                           │                         │
     ├──────────────────────────►│                         │
     │                           │ Edit events processed   │
     │                           │                         │
     │                           │                      [Panel remains
     │                           │                       visible and
     │                           │                       accessible]
     │                           │                         │
3. Click "Snippets" tab in panel                         │
     │                           │                         │
     ├─────────────────────────────────────────────────────►
     │                           │                         │ WM_NOTIFY
     │                           │                         │ TCN_SELCHANGE
     │                           │                         │
     │                           │                         │ Update content
     │                           │                         │ to show snippets
     │                           │                         │
4. Continue typing in main window                        │
     │                           │                         │
     ├──────────────────────────►│                      [Both windows
     │                           │ Keystroke processed   remain active,
     │                           │                       no blocking]
     │                           │                         │
5. Resize floating panel                                  │
     │                           │                         │
     ├─────────────────────────────────────────────────────►
     │                           │                         │ WM_SIZE
     │                           │                         │ Resize children
     │                           │                         │ Tab control
     │                           │                         │ Content area
     │                           │                         │ Status label
```

## Component Layout (Detailed)

```
m_hwndFloatingPanel (350 x 500 pixels)
╔════════════════════════════════════════════════════════╗
║ RawrXD Tools                                      [_][X]║  <- Title bar (WS_CAPTION)
╠════════════════════════════════════════════════════════╣
║  ┌──────────────────────────────────────────────────┐  ║
║  │ Quick Info  │ Snippets  │ Help                   │  ║  <- Tab Control (30px high)
║  └──────────────────────────────────────────────────┘  ║     ID: 5001
║                                                         ║
║  ┌──────────────────────────────────────────────────┐  ║
║  │ ╔════════════════════════════════════════════╗   │  ║
║  │ ║   System Quick Info                        ║   │  ║
║  │ ╚════════════════════════════════════════════╝   │  ║
║  │                                                  │  ║
║  │ IDE Status: Running ✓                           │  ║
║  │ Terminal: Active                                │  ║
║  │ Editor: Ready                                   │  ║  <- m_hwndFloatingContent
║  │                                                  │  ║     EDIT control
║  │ Current File: None                              │  ║     ES_MULTILINE
║  │ Line: 1 | Column: 1                             │  ║     ES_READONLY
║  │                                                  │  ║     ES_AUTOVSCROLL
║  │ Memory Usage: Normal                            │  ║     WS_VSCROLL
║  │ Performance: Good                               │  ║     Consolas font
║  │                                                  │  ║
║  │ Recent Commands:                                │  ║
║  │   → None executed                               │  ║
║  │                                                  │  ║
║  │ Keyboard Shortcuts:                             │  ║
║  │   Ctrl+N - New File                             │  ║
║  │   Ctrl+O - Open File                            │  ║
║  │   Ctrl+S - Save File                            │  ║
║  │   F5 - Run Script                               │  ║
║  │                                                  ↕  ║  <- Scrollbar
║  └──────────────────────────────────────────────────┘  ║
║                                                         ║
║  ┌──────────────────────────────────────────────────┐  ║
║  │         Ready | Non-Modal | Resizable            │  ║  <- Status label
║  └──────────────────────────────────────────────────┘  ║     STATIC control
║                                                         ║
╚════════════════════════════════════════════════════════╝
 ◄───────────────► Resize handles (WS_THICKFRAME)
```

## Message Flow Diagram

```
User drags floating panel title bar
        │
        ▼
    WM_NCHITTEST ───► System determines title bar hit
        │
        ▼
    WM_NCLBUTTONDOWN ───► Begin move operation
        │
        ▼
    WM_MOVING ───────────► Panel position updates
        │
        ▼
    WM_MOVE ─────────────► Final position set
        │
        ▼
    Panel moved ✓


User resizes floating panel edge
        │
        ▼
    WM_NCHITTEST ───► System determines edge hit
        │
        ▼
    WM_SIZING ───────────► Resize operation begins
        │
        ▼
    WM_SIZE ─────────────► FloatingPanelProc handles
        │                   │
        │                   ├─► GetClientRect() get new size
        │                   │
        │                   ├─► Resize Tab Control
        │                   │    SetWindowPos(hwndTab, ...)
        │                   │
        │                   ├─► Resize Content Area
        │                   │    SetWindowPos(m_hwndFloatingContent, ...)
        │                   │
        │                   └─► Resize Status Label
        │                        SetWindowPos(hwndStatus, ...)
        ▼
    Panel resized ✓


User clicks "Snippets" tab
        │
        ▼
    WM_LBUTTONDOWN ──────► Tab Control receives click
        │
        ▼
    TCN_SELCHANGE ───────► Tab selection changed
        │
        ▼
    WM_NOTIFY ───────────► FloatingPanelProc handles
        │                   │
        │                   ├─► TabCtrl_GetCurSel() = 1
        │                   │
        │                   └─► SetWindowTextA(content, snippetsText)
        ▼
    Tab content updated ✓


User closes floating panel (X button)
        │
        ▼
    WM_CLOSE ────────────► FloatingPanelProc handles
        │                   │
        │                   └─► ShowWindow(hwnd, SW_HIDE)
        │                       [Does NOT destroy window]
        ▼
    Panel hidden ✓
    (Can be shown again via menu)
```

## Z-Order Visualization

```
Screen Layers (from back to front):

Layer 0: Desktop
  │
  ├─► Layer 1: Normal windows (other apps)
  │     │
  │     └─► Layer 2: RawrXD Main Window (m_hwndMain)
  │           │
  │           ├─► Editor control
  │           ├─► Terminal control
  │           └─► Status bar
  │
  └─► Layer 3: TOPMOST windows
        │
        └─► Floating Panel (WS_EX_TOPMOST)
              │
              ├─► Tab control
              ├─► Content area
              └─► Status label

Note: Floating panel always appears above main window
      and other normal windows due to WS_EX_TOPMOST flag
```

## Memory Footprint

```
Component                     Approx Size
─────────────────────────     ───────────
Window structure (HWND)       ~200 bytes
Tab control                   ~5 KB
Edit control                  ~10 KB
Tab content strings           ~5 KB
Status label                  ~1 KB
Font object (Consolas)        ~2 KB
Window class registration     ~1 KB
Total per instance:           ~25 KB

Multiple instances:
- Only one m_hwndFloatingPanel per IDE instance
- Lazy creation (only created when first shown)
- Lightweight enough for continuous use
```

## Event Timeline (Typical Session)

```
Time    Event                           Action
────    ─────                           ──────
0.0s    IDE starts                      Main window created
        m_hwndFloatingPanel = NULL      Panel not created yet

15.2s   User: View > Floating Panel     toggleFloatingPanel()
        Panel doesn't exist             createFloatingPanel()
        Register window class           [if not registered]
        Create window                   m_hwndFloatingPanel created
        Create tab control              3 tabs added
        Create content area             Quick Info text set
        Create status label             "Ready | Non-Modal"
        ShowWindow(panel, SW_SHOW)      Panel visible ✓

15.8s   User clicks "Snippets" tab      WM_NOTIFY received
        TCN_SELCHANGE notification      Tab index = 1
        Update content                  Snippet text displayed

42.1s   User resizes panel              WM_SIZE received
        Calculate new dimensions        width=400, height=600
        Resize tab: 380 x 30           SetWindowPos()
        Resize content: 370 x 510      SetWindowPos()
        Resize status: 370 x 20        SetWindowPos()
        
127.5s  User types in main editor       Main window processes
        Panel remains visible           No interference
        Both windows active             Non-modal behavior ✓

243.8s  User clicks X on panel          WM_CLOSE received
        ShowWindow(panel, SW_HIDE)      Panel hidden
        m_hwndFloatingPanel valid       Window still exists

301.2s  User: View > Floating Panel     toggleFloatingPanel()
        Panel exists, check visible     IsWindowVisible() = false
        ShowWindow(panel, SW_SHOW)      Panel visible again ✓
        SetForegroundWindow()           Bring to front
```

---

**Legend:**
- `─` Horizontal line
- `│` Vertical line
- `┌┐└┘` Corners
- `├┤┬┴┼` Connectors
- `═║╔╗╚╝` Double-line boxes
- `►` Arrow/direction
- `✓` Success indicator
- `↕` Scrollbar
