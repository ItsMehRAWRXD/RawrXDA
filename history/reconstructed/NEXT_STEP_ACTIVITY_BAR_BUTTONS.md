# Next Step: Activity Bar Icon Buttons (TODO #2)

This is the FASTEST way to make the sidebar actually usable.

---

## What You Need To Do

Add 5 icon buttons to the activity bar (far left, 48px wide). When clicked, they switch the sidebar view.

### Current State
- ✅ setSidebarView() implemented and callable
- ✅ onSize() properly resizes sidebar
- ❌ No way to call setSidebarView() from UI
- ❌ Activity bar is empty static control

### What To Add

In `Win32IDE.cpp`, modify `createSidebar()`:

```cpp
void Win32IDE::createSidebar(HWND hwnd)
{
    // Existing code...
    
    if (m_hwndActivityBar) {
        // Create 5 view buttons on activity bar (48px wide, 48px tall each)
        DWORD btnStyle = WS_CHILD | WS_VISIBLE | BS_FLAT | BS_PUSHBUTTON;
        
        HWND btnExplorer = CreateWindowExA(0, "BUTTON", "📁",
            btnStyle, 0, 0, 48, 48, 
            m_hwndActivityBar, (HMENU)IDC_ACTBAR_EXPLORER, m_hInstance, nullptr);
            
        HWND btnSearch = CreateWindowExA(0, "BUTTON", "🔍",
            btnStyle, 0, 48, 48, 48,
            m_hwndActivityBar, (HMENU)IDC_ACTBAR_SEARCH, m_hInstance, nullptr);
            
        HWND btnGit = CreateWindowExA(0, "BUTTON", "✓",
            btnStyle, 0, 96, 48, 48,
            m_hwndActivityBar, (HMENU)IDC_ACTBAR_SCM, m_hInstance, nullptr);
            
        HWND btnDebug = CreateWindowExA(0, "BUTTON", "▶",
            btnStyle, 0, 144, 48, 48,
            m_hwndActivityBar, (HMENU)IDC_ACTBAR_DEBUG, m_hInstance, nullptr);
            
        HWND btnExt = CreateWindowExA(0, "BUTTON", "⚙",
            btnStyle, 0, 192, 48, 48,
            m_hwndActivityBar, (HMENU)IDC_ACTBAR_EXTENSIONS, m_hInstance, nullptr);
    }
}
```

### Then Wire Clicks in onCommand()

In `Win32IDE::onCommand()`, add these cases:

```cpp
case IDC_ACTBAR_EXPLORER:
    setSidebarView(SidebarView::Explorer);
    return;
    
case IDC_ACTBAR_SEARCH:
    setSidebarView(SidebarView::Search);
    return;
    
case IDC_ACTBAR_SCM:
    setSidebarView(SidebarView::SourceControl);
    return;
    
case IDC_ACTBAR_DEBUG:
    setSidebarView(SidebarView::RunDebug);
    return;
    
case IDC_ACTBAR_EXTENSIONS:
    setSidebarView(SidebarView::Extensions);
    return;
```

### Time Estimate
- 10 minutes to add buttons
- 5 minutes to add command handlers
- **Total: 15 minutes**

### Result
- Users can click activity bar icons to switch sidebar
- Sidebar view switching actually works
- IDE becomes more VS Code-like

---

## Why This Is Item #2

- **#1** (setSidebarView) - ✅ DONE - Implemented
- **#2** (Activity Bar buttons) - YOU ARE HERE - Make sidebar usable  
- **#3** (Problems panel) - Comes next - Add build error display
- **#4+** - Other missing panes

---

## Ctrl+B Already Works

User can press Ctrl+B to toggle sidebar visibility. Activity bar buttons complete the feature.

