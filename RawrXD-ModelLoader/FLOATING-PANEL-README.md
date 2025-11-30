# RawrXD Non-Modal Floating Panel

## Overview

The RawrXD IDE now includes a **non-modal floating panel** that provides quick access to tools, information, and references while you work in the main IDE window.

## Key Features

### ✓ Non-Modal Design
- **Independent Operation**: The floating panel doesn't block interaction with the main IDE window
- **Work Continuously**: Edit code, run commands, and interact with the terminal while the panel remains visible
- **No Dialog Lock**: Unlike modal dialogs, you can switch between windows freely

### ✓ Always On Top
- **Persistent Visibility**: Panel stays on top of other windows using `WS_EX_TOPMOST` flag
- **Quick Reference**: Perfect for keeping help text, snippets, or info visible at all times
- **Auto-Restore**: Panel maintains topmost position even when other windows are activated

### ✓ Fully Resizable & Draggable
- **Custom Sizing**: Resize the panel to fit your screen layout using `WS_THICKFRAME`
- **Position Anywhere**: Drag the panel to any screen location
- **Smart Layout**: Child controls automatically resize to fit the panel dimensions
- **Persistent Position**: (Coming soon) Panel remembers its size and position between sessions

### ✓ Tabbed Interface
- **Multiple Views**: Switch between Quick Info, Snippets, and Help tabs
- **Organized Content**: Each tab provides different tools and information
- **Easy Navigation**: Click tabs to switch content instantly

## Architecture

### Window Styles

```cpp
// Extended styles for floating behavior
WS_EX_TOOLWINDOW | WS_EX_TOPMOST

// Base styles for appearance and interaction
WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE
```

**Style Breakdown:**
- `WS_EX_TOOLWINDOW`: Tool window appearance (small title bar, doesn't appear in taskbar)
- `WS_EX_TOPMOST`: Always stays on top of other windows
- `WS_POPUP`: Independent window without default frame
- `WS_CAPTION`: Shows title bar for dragging
- `WS_SYSMENU`: Adds system menu (close button)
- `WS_THICKFRAME`: Makes window resizable
- `WS_VISIBLE`: Shows window immediately on creation

### Component Hierarchy

```
m_hwndFloatingPanel (Parent Window)
├── Tab Control (ID: 5001)
│   ├── Tab 0: Quick Info
│   ├── Tab 1: Snippets
│   └── Tab 2: Help
├── m_hwndFloatingContent (Edit Control)
│   └── Scrollable multi-line text area
└── Status Label (Static Control)
    └── Shows panel state
```

### Window Procedure

The `FloatingPanelProc` static method handles:

1. **WM_CREATE**: Initialize panel, store instance pointer
2. **WM_SIZE**: Resize child controls proportionally
3. **WM_NOTIFY**: Handle tab selection changes
4. **WM_CLOSE**: Hide instead of destroy (non-modal behavior)
5. **WM_ACTIVATE**: Ensure panel stays on top

## Usage

### Opening the Panel

**Via Menu:**
```
View → Floating Panel
```

**Via Code:**
```cpp
ide.showFloatingPanel();
```

### Toggling Visibility

```cpp
ide.toggleFloatingPanel();  // Show if hidden, hide if visible
```

### Closing the Panel

- Click the X button (hides, doesn't destroy)
- Use View menu to toggle off
- Call `ide.hideFloatingPanel()`

### Switching Tabs

Click on the tab headers to switch between:
1. **Quick Info** - System status, shortcuts, recent commands
2. **Snippets** - Code templates and common patterns
3. **Help** - PowerShell command reference

**Programmatic Tab Switching:**
```cpp
ide.setFloatingPanelTab(0);  // Quick Info
ide.setFloatingPanelTab(1);  // Snippets
ide.setFloatingPanelTab(2);  // Help
```

### Updating Content

```cpp
std::string customContent = "My custom panel content\nLine 2\nLine 3";
ide.updateFloatingPanelContent(customContent);
```

## Implementation Details

### Creating the Panel

```cpp
void Win32IDE::createFloatingPanel() {
    // 1. Register window class (if not already registered)
    WNDCLASSEX wc = { /* ... */ };
    RegisterClassEx(&wc);
    
    // 2. Calculate position (right side of screen)
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int posX = screenWidth - panelWidth - 50;
    
    // 3. Create window with floating styles
    m_hwndFloatingPanel = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "FloatingPanelClass",
        "RawrXD Tools",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE,
        posX, posY, panelWidth, panelHeight,
        m_hwndMain,  // Parent
        NULL, hInstance, this
    );
    
    // 4. Create tab control
    HWND hwndTab = CreateWindowEx(0, WC_TABCONTROL, ...);
    
    // 5. Create content area
    m_hwndFloatingContent = CreateWindowEx(..., "EDIT", ...);
    
    // 6. Add status label
    HWND hwndStatus = CreateWindowEx(..., "STATIC", ...);
}
```

### Dynamic Resizing

```cpp
case WM_SIZE: {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    
    // Resize tab control
    SetWindowPos(hwndTab, NULL, 5, 5, width - 20, 30, SWP_NOZORDER);
    
    // Resize content area
    SetWindowPos(m_hwndFloatingContent, NULL,
        10, 40, width - 30, height - 90, SWP_NOZORDER);
    
    // Resize status
    SetWindowPos(hwndStatus, NULL,
        10, height - 40, width - 30, 20, SWP_NOZORDER);
}
```

### Tab Content Switching

```cpp
case WM_NOTIFY: {
    LPNMHDR pnmh = (LPNMHDR)lParam;
    if (pnmh->code == TCN_SELCHANGE) {
        int iTab = TabCtrl_GetCurSel(hwndTab);
        switch (iTab) {
            case 0: SetWindowTextA(content, quickInfoText); break;
            case 1: SetWindowTextA(content, snippetsText); break;
            case 2: SetWindowTextA(content, helpText); break;
        }
    }
}
```

## Technical Benefits

### Non-Modal Advantages

1. **No Event Loop Blocking**: Main window message pump continues running
2. **Multi-Window Workflow**: Users can interact with both windows simultaneously
3. **Background Updates**: Panel can update while user works in main IDE
4. **No Z-Order Issues**: Panel uses `WS_EX_TOPMOST` for reliable positioning

### Performance Characteristics

- **Lightweight**: Minimal memory footprint (~50KB for window + controls)
- **Efficient Rendering**: Only redraws on resize or tab change
- **No Polling**: Event-driven updates via WM_NOTIFY
- **Fast Creation**: Lazy initialization only when first shown

## Future Enhancements

### Planned Features

- [ ] Keyboard shortcut (Ctrl+Shift+F) to toggle panel
- [ ] Save/restore panel position and size in settings
- [ ] More tab types (Variables, Call Stack, Watch)
- [ ] Docking capability (snap to screen edges)
- [ ] Transparency control
- [ ] Multiple floating panels for different tool categories
- [ ] Live update of Quick Info with real-time IDE stats
- [ ] Snippet insertion via double-click
- [ ] Search functionality in Help tab
- [ ] Custom user tabs

### Extensibility Points

```cpp
// Add custom tabs programmatically
void Win32IDE::addFloatingPanelTab(const std::string& name, const std::string& content);

// Register custom content providers
void Win32IDE::registerPanelContentProvider(int tabId, ContentProviderFunc func);

// Set panel theme
void Win32IDE::setFloatingPanelTheme(const IDETheme& theme);
```

## Comparison: Modal vs Non-Modal

| Feature | Modal Dialog | Non-Modal Floating Panel |
|---------|--------------|--------------------------|
| Main window interaction | ❌ Blocked | ✅ Available |
| Window focus | 🔒 Locked | 🔄 Switchable |
| Taskbar entry | ✅ Yes | ❌ No (tool window) |
| Always on top | ❌ No | ✅ Yes |
| Use case | User decision required | Continuous reference |
| Close behavior | Destroys window | Hides window |

## Code Examples

### Example 1: Show panel on startup

```cpp
Win32IDE::Win32IDE(HINSTANCE hInstance) {
    // ... initialization ...
    
    // Optionally show panel on startup
    // createFloatingPanel();
}
```

### Example 2: Update panel with live data

```cpp
void Win32IDE::onTerminalOutput(const std::string& output) {
    // Update terminal in main window
    appendText(m_hwndTerminal, output);
    
    // Also update floating panel Quick Info tab
    if (m_hwndFloatingPanel && IsWindowVisible(m_hwndFloatingPanel)) {
        std::string info = "Last Output:\n" + output.substr(0, 100);
        updateFloatingPanelContent(info);
    }
}
```

### Example 3: Context-sensitive help

```cpp
void Win32IDE::showContextHelp(const std::string& cmdlet) {
    showFloatingPanel();
    setFloatingPanelTab(2);  // Switch to Help tab
    
    std::string helpText = "Help for: " + cmdlet + "\n\n";
    helpText += getHelpText(cmdlet);
    updateFloatingPanelContent(helpText);
}
```

## Troubleshooting

### Panel doesn't stay on top
- Ensure `WS_EX_TOPMOST` is set in CreateWindowEx
- Check WM_ACTIVATE handler resets topmost flag

### Panel not resizing properly
- Verify WM_SIZE handler updates all child controls
- Use `SWP_NOZORDER` flag in SetWindowPos to prevent z-order changes

### Panel appears in taskbar
- Ensure `WS_EX_TOOLWINDOW` is set
- Remove `WS_EX_APPWINDOW` if present

### Content not updating on tab switch
- Check WM_NOTIFY handler for TCN_SELCHANGE
- Verify TabCtrl_GetCurSel returns correct index

## References

- Win32 API: [CreateWindowEx](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa)
- Window Styles: [Extended Window Styles](https://docs.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles)
- Tab Control: [Tab Control Reference](https://docs.microsoft.com/en-us/windows/win32/controls/tab-control-reference)
- Non-Modal Dialogs: [Modeless Dialog Boxes](https://docs.microsoft.com/en-us/windows/win32/dlgbox/using-dialog-boxes#creating-a-modeless-dialog-box)

## License

Part of the RawrXD IDE project. See main project LICENSE for details.
