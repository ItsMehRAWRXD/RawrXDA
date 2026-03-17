# IDE Menu Wiring Analysis & Fix Plan

## Executive Summary
The IDE menu system is **correctly wired** in the command routing layer, but the actual implementations of critical UI features are **missing**. Menu items call functions that are declared but not defined.

## Architecture Discovery

### Command Routing System (✅ WORKING)
- **Location**: `d:\rawrxd\src\win32app\Win32IDE_Commands.cpp`
- **Entry Point**: `Win32IDE::routeCommand(int commandId)`
- **Routing Logic**: Command IDs are dispatched to specialized handlers:
  - 1000-1999 → File commands
  - 2000-2999 → View commands  
  - 3000-3999 → View/Terminal commands
  - 4000-4999 → Agent commands
  - 5000-5999 → Tools commands

### Menu Creation (✅ WORKING)
- **Location**: `d:\rawrxd\src\win32app\Win32IDE.cpp` → `createMenuBar()`
- **Menu Items Created**:
  ```cpp
  IDM_VIEW_AI_CHAT (3007)        → calls toggleSecondarySidebar()
  IDM_VIEW_AGENT_CHAT (3009)     → calls toggleSecondarySidebar()  
  IDM_VIEW_FILE_EXPLORER (2030)  → calls setSidebarView(Explorer)
  ```

### Command Handler (✅ WORKING)
**File**: `d:\rawrxd\src\win32app\Win32IDE_Commands.cpp`, `handleViewCommand()`

```cpp
case 3007: // IDM_VIEW_AI_CHAT
    toggleSecondarySidebar();
    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"AI Chat shown");
    break;
    
case 3009: // IDM_VIEW_AGENT_CHAT  
    toggleSecondarySidebar();
    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Agent Chat shown");
    break;
    
case 2030: // IDM_VIEW_FILE_EXPLORER
    setSidebarView(SidebarView::Explorer);
    if (!m_sidebarVisible) toggleSidebar();
    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File Explorer");
    break;
```

## ❌ MISSING IMPLEMENTATIONS

### 1. Secondary Sidebar (AI/Agent Chat Panel)

**Declared**: `d:\rawrxd\src\win32app\Win32IDE.h` (lines ~2372, ~2388)
```cpp
void createSecondarySidebar(HWND hwndParent);
void toggleSecondarySidebar();
void updateSecondarySidebarContent();
void HandleCopilotSend();
void HandleCopilotClear();
```

**Implementation Status**: ❌ NOT FOUND
- Searched all `.cpp` files in `win32app/` directory
- Functions are declared but never defined
- Menu calling these functions results in linker error or no-op

**Required Components**:
- `m_hwndSecondarySidebar` - Main panel window
- `m_hwndCopilotChatInput` - Chat input box
- `m_hwndCopilotChatOutput` - Chat output/history
- `m_hwndCopilotSendBtn` - Send button
- `m_hwndCopilotClearBtn` - Clear chat button
- GitHub Copilot/Amazon Q integration hooks

### 2. Explorer View (File Tree)

**Declared**: `d:\rawrxd\src\win32app\Win32IDE.h` (line ~1625)
```cpp
void createExplorerView(HWND hwndParent);
void refreshFileTree();
void expandFolder(const std::string& path);
void collapseAllFolders();
void newFileInExplorer();
void newFolderInExplorer();
void deleteItemInExplorer();
void renameItemInExplorer();
```

**Implementation Status**: ❌ NOT FOUND
- `createExplorerView()` is never defined
- `m_hwndExplorerTree` is declared but never created
- File tree population logic missing

**Note**: There IS a `createFileExplorer()` function but it's for a different control (`m_hwndFileExplorer`) that's model-file focused, not a general workspace explorer.

### 3. Chat Panel Implementation

**Declared**: `d:\rawrxd\src\win32app\Win32IDE.h` (lines ~2520-2527)
```cpp
void createChatPanel();
void HandleCopilotSend();
void HandleCopilotClear();
void HandleCopilotStreamUpdate(const char* token, size_t length);
void sendCopilotMessage(const std::string& message);
void clearCopilotChat();
void appendCopilotResponse(const std::string& response);
```

**Implementation Status**: ❌ NOT FOUND

## VSIX Loader Status

### VSIX Loader Header (✅ COMPLETE)
**Location**: `d:\rawrxd\src\modules\vsix_loader.h`

**Key Components**:
```cpp
struct VSIXPlugin {
    std::string id;
    std::string name;
    std::string version;
    nlohmann::json manifest;
    std::function<void()> onLoad;
    std::function<void()> onUnload;
    std::function<void(const std::string&)> onCommand;
    std::function<void(const nlohmann::json&)> onConfigure;
};

class VSIXLoader {
    bool Initialize(const std::string& plugins_dir);
    bool LoadPlugin(const std::string& vsix_path);
    bool UnloadPlugin(const std::string& plugin_id);
    bool EnablePlugin(const std::string& plugin_id);
    bool DisablePlugin(const std::string& plugin_id);
    bool ExecutePluginCommand(const std::string& plugin_id, 
                              const std::string& command,
                              const std::vector<std::string>& args);
    VSIXPlugin* GetPlugin(const std::string& plugin_id);
    std::vector<VSIXPlugin*> GetLoadedPlugins();
};
```

### VSIX Loader Implementation (⚠️ PARTIAL)
**Location**: `d:\rawrxd\src\vsix_loader.cpp`

**Reviewed**: Lines 1-100 of 555 (18%)

**Implemented So Far**:
- ✅ Constructor - Creates plugins directory, loads state
- ✅ `ExtractVSIX()` - Uses libzip to extract VSIX archives
- ✅ `LoadPluginFromDirectory()` - Reads manifest.json, validates, creates VSIXPlugin
- ✅ `LoadPlugin()` - Entry point for loading from VSIX file

**Not Yet Reviewed** (lines 100-555):
- UnloadPlugin()
- EnablePlugin()
- DisablePlugin()
- ExecutePluginCommand()
- Command registration/execution
- Memory module loading
- Engine management

## Root Cause Analysis

### Why Menu Items Don't Work

1. **Menu Definition** ✅ → Menu created with correct IDs
2. **Command Routing** ✅ → `routeCommand()` dispatches to handlers  
3. **Handler Logic** ✅ → Calls appropriate functions
4. **Function Implementation** ❌ → **FUNCTIONS DON'T EXIST**

### Impact

When user clicks:
- **View → AI Chat**: Calls `toggleSecondarySidebar()` → **Linker error or empty stub**
- **View → Agent Chat**: Calls `toggleSecondarySidebar()` → **Same failure**
- **View → File Explorer**: Calls `createExplorerView()` → **Function doesn't exist**

## Implementation Plan

### Phase 1: Complete VSIX Loader (4-6 hours)

**File**: `d:\rawrxd\src\modules\vsix_loader.cpp`

1. Review remaining 455 lines (lines 100-555)
2. Implement missing methods:
   - `UnloadPlugin()`
   - `EnablePlugin()`
   - `DisablePlugin()`  
   - `ExecutePluginCommand()`
   - Command registration system
3. Add Amazon Q specific handling
4. Add GitHub Copilot specific handling
5. Test with sample VSIX

### Phase 2: Implement Secondary Sidebar (Chat Panel) (6-8 hours)

**Create**: `d:\rawrxd\src\win32app\Win32IDE_ChatPanel.cpp`

```cpp
// ESP:m_hwndSecondarySidebar - AI Chat / Agent Chat Panel
void Win32IDE::createSecondarySidebar(HWND hwndParent) {
    // Create secondary sidebar container (right-side panel)
    m_hwndSecondarySidebar = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | SS_OWNERDRAW,
        0, 0, m_secondarySidebarWidth, 600,
        hwndParent, nullptr, m_hInstance, nullptr
    );
    
    // Create header with model selector
    m_hwndSecondarySidebarHeader = CreateWindowExA(...);
    m_hwndModelSelector = CreateWindowExA(...);  // Combobox
    
    // Create chat output (rich edit control for message history)
    m_hwndCopilotChatOutput = CreateWindowExW(
        0, MSFTEDIT_CLASS, L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_READONLY | ES_MULTILINE,
        ...
    );
    
    // Create chat input (edit control at bottom)
    m_hwndCopilotChatInput = CreateWindowExA(
        0, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
        ...
    );
    
    // Create action buttons
    m_hwndCopilotSendBtn = CreateWindowExA(..., "BUTTON", "Send", ...);
    m_hwndCopilotClearBtn = CreateWindowExA(..., "BUTTON", "Clear", ...);
    
    // Set window proc for message handling
    SetWindowLongPtrA(m_hwndSecondarySidebar, GWLP_WNDPROC, 
                      (LONG_PTR)SecondarySidebarProc);
}

void Win32IDE::toggleSecondarySidebar() {
    m_secondarySidebarVisible = !m_secondarySidebarVisible;
    ShowWindow(m_hwndSecondarySidebar, 
               m_secondarySidebarVisible ? SW_SHOW : SW_HIDE);
    layoutWindows();  // Reflow editor/sidebar layout
}

void Win32IDE::HandleCopilotSend() {
    // Get input text
    char buffer[4096];
    GetWindowTextA(m_hwndCopilotChatInput, buffer, sizeof(buffer));
    std::string message(buffer);
    
    // Add to chat history
    m_chatHistory.push_back({"user", message});
    appendCopilotMessage("user", message);
    
    // Clear input
    SetWindowTextA(m_hwndCopilotChatInput, "");
    
    // Send to AI backend (VSIX plugin or local model)
    sendCopilotMessage(message);
}

void Win32IDE::sendCopilotMessage(const std::string& message) {
    // Route to loaded VSIX plugin (GitHub Copilot or Amazon Q)
    VSIXLoader& loader = VSIXLoader::GetInstance();
    
    // Try GitHub Copilot extension
    VSIXPlugin* copilot = loader.GetPlugin("github.copilot-chat");
    if (copilot && copilot->enabled) {
        // Execute chat command via VSIX plugin API
        std::vector<std::string> args = {message};
        loader.ExecutePluginCommand("github.copilot-chat", "chat", args);
        return;
    }
    
    // Try Amazon Q extension  
    VSIXPlugin* amazonq = loader.GetPlugin("amazonq.amazonq");
    if (amazonq && amazonq->enabled) {
        std::vector<std::string> args = {message};
        loader.ExecutePluginCommand("amazonq.amazonq", "chat", args);
        return;
    }
    
    // Fallback: use local Ollama model
    std::string response;
    if (trySendToOllama(message, response)) {
        appendCopilotResponse(response);
    } else {
        appendCopilotResponse("[Error] No AI backend available");
    }
}

void Win32IDE::appendCopilotResponse(const std::string& response) {
    m_chatHistory.push_back({"assistant", response});
    appendCopilotMessage("assistant", response);
}

void Win32IDE::appendCopilotMessage(const std::string& role, 
                                    const std::string& message) {
    // Format message with role prefix
    std::string formatted = role + ": " + message + "\r\n\r\n";
    
    // Append to output control
    GETTEXTLENGTHEX gtl = {GTL_NUMCHARS, CP_ACP};
    LRESULT len = SendMessageA(m_hwndCopilotChatOutput, 
                               EM_GETTEXTLENGTHEX, 
                               (WPARAM)&gtl, 0);
    
    CHARRANGE cr = {(LONG)len, (LONG)len};
    SendMessageA(m_hwndCopilotChatOutput, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, 
                 (LPARAM)formatted.c_str());
    
    // Scroll to bottom
    SendMessageA(m_hwndCopilotChatOutput, WM_VSCROLL, SB_BOTTOM, 0);
}

void Win32IDE::HandleCopilotClear() {
    m_chatHistory.clear();
    SetWindowTextA(m_hwndCopilotChatOutput, "");
}
```

**Integration Points**:
1. Call `createSecondarySidebar()` from `onCreate()` 
2. Add layout logic to `layoutWindows()` to position sidebar
3. Wire up WM_COMMAND handlers for Send/Clear buttons
4. Connect streaming response handler for token-by-token display

### Phase 3: Implement Explorer View (4-6 hours)

**Create**: `d:\rawrxd\src\win32app\Win32IDE_ExplorerView.cpp`

```cpp
void Win32IDE::createExplorerView(HWND hwndParent) {
    // Create tree view control
    m_hwndExplorerTree = CreateWindowExW(
        0, WC_TREEVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | 
        TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 30, 250, 500,
        hwndParent, (HMENU)IDC_EXPLORER_TREE, m_hInstance, nullptr
    );
    
    // Create image list for file/folder icons
    m_hImageListExplorer = ImageList_Create(16, 16, ILC_COLOR32, 2, 2);
    // Load icons: folder, file, etc.
    TreeView_SetImageList(m_hwndExplorerTree, m_hImageListExplorer, TVSIL_NORMAL);
    
    // Create toolbar with New/Refresh buttons
    m_hwndExplorerToolbar = CreateWindowExA(...);
    
    // Populate tree with workspace root
    refreshFileTree();
}

void Win32IDE::refreshFileTree() {
    TreeView_DeleteAllItems(m_hwndExplorerTree);
    
    if (m_explorerRootPath.empty()) {
        char cwd[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, cwd);
        m_explorerRootPath = cwd;
    }
    
    // Add root item
    TVINSERTSTRUCTW tvis = {};
    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN;
    tvis.item.pszText = (LPWSTR)utf8ToWide(m_explorerRootPath).c_str();
    tvis.item.iImage = 0;  // folder icon
    tvis.item.iSelectedImage = 0;
    tvis.item.cChildren = 1;  // has children
    
    HTREEITEM hRoot = TreeView_InsertItem(m_hwndExplorerTree, &tvis);
    
    // Scan and populate directory
    scanDirectory(m_explorerRootPath, hRoot);
}

void Win32IDE::scanDirectory(const std::string& dirPath, HTREEITEM hParent) {
    namespace fs = std::filesystem;
    
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            std::string name = entry.path().filename().string();
            bool isDir = entry.is_directory();
            
            TVINSERTSTRUCTW tvis = {};
            tvis.hParent = hParent;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
            tvis.item.pszText = (LPWSTR)utf8ToWide(name).c_str();
            tvis.item.iImage = isDir ? 0 : 1;  // folder or file icon
            tvis.item.iSelectedImage = isDir ? 0 : 1;
            tvis.item.lParam = (LPARAM)new std::string(entry.path().string());
            
            if (isDir) {
                tvis.item.mask |= TVIF_CHILDREN;
                tvis.item.cChildren = 1;  // lazy load
            }
            
            TreeView_InsertItem(m_hwndExplorerTree, &tvis);
        }
    } catch (const std::exception& e) {
        appendToOutput("Explorer scan error: " + std::string(e.what()) + "\n", 
                       "Output", OutputSeverity::Error);
    }
}

void Win32IDE::onFileTreeDoubleClick(HTREEITEM hItem) {
    // Get item path
    TVITEM item = {};
    item.hItem = hItem;
    item.mask = TVIF_PARAM;
    TreeView_GetItem(m_hwndExplorerTree, &item);
    
    std::string* path = (std::string*)item.lParam;
    if (!path) return;
    
    // Open file in editor
    if (std::filesystem::is_regular_file(*path)) {
        openFileByPath(*path);
    }
}
```

**Integration Points**:
1. Call `createExplorerView()` from `createPrimarySidebar()`
2. Handle `TVN_SELCHANGED`, `TVN_ITEMEXPANDED`, `NM_DBLCLK` notifications
3. Add context menu for right-click (new file/folder, delete, rename)
4. Connect to existing file operations

### Phase 4: VSIX Integration Testing (2-4 hours)

**Test Script**: `d:\test_vsix_agentically.ps1`

```powershell
# Agentic VSIX Test Suite

$vsixFiles = @(
    "C:\Users\$env:USERNAME\.vscode\extensions\github.copilot-*\extension.vsix",
    "C:\Users\$env:USERNAME\.vscode\extensions\amazonq-*\extension.vsix"
)

Write-Host "=== VSIX Loader Agentic Test ===" -ForegroundColor Cyan

foreach ($pattern in $vsixFiles) {
    $matches = Get-ChildItem $pattern -ErrorAction SilentlyContinue
    foreach ($vsix in $matches) {
        Write-Host "`nTesting: $($vsix.Name)" -ForegroundColor Yellow
        
        # Test 1: Load plugin
        .\RawrXD-IDE.exe --vsix-test --vsix-load "$($vsix.FullName)"
        
        # Test 2: Execute chat command
        .\RawrXD-IDE.exe --vsix-test --vsix-exec "$($vsix.Name)" "chat" "Hello"
        
        # Test 3: Query capabilities
        .\RawrXD-IDE.exe --vsix-test --vsix-info "$($vsix.Name)"
    }
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Green
```

**Expected Results**:
- Amazon Q loads successfully
- GitHub Copilot Chat loads successfully
- Chat commands route to extensions
- Extensions respond with completion/chat output
- Results written to `d:\vsix_test_results.json`

## Timeline

| Phase | Task | Est. Time |
|-------|------|-----------|
| 1 | Complete VSIX loader implementation | 4-6 hrs |
| 2 | Implement secondary sidebar (chat) | 6-8 hrs |
| 3 | Implement explorer view | 4-6 hrs |
| 4 | VSIX integration testing | 2-4 hrs |
| **Total** | | **16-24 hrs** |

## Files to Create/Modify

### New Files
1. `d:\rawrxd\src\win32app\Win32IDE_ChatPanel.cpp`
2. `d:\rawrxd\src\win32app\Win32IDE_ExplorerView.cpp`  
3. `d:\test_vsix_agentically.ps1`

### Modified Files
1. `d:\rawrxd\src\modules\vsix_loader.cpp` (complete remaining methods)
2. `d:\rawrxd\src\win32app\Win32IDE_Core.cpp` (call creation functions)
3. `d:\CMakeLists.txt` (add new .cpp files to build)

## Success Criteria

✅ **Menu Items Work**:
- View → AI Chat opens chat panel on right
- View → Agent Chat opens same panel (autonomous mode label)
- View → File Explorer shows workspace tree in left sidebar

✅ **Chat Panel Functional**:
- Input box accepts text
- Send button delivers message to AI backend
- Response appears in output area
- Clear button works

✅ **File Explorer Functional**:
- Tree view shows files/folders
- Double-click opens file in editor
- Context menu for new/delete/rename
- Lazy-load subdirectories

✅ **VSIX Integration**:
- Amazon Q extension loads
- GitHub Copilot Chat extension loads
- Chat commands route to extensions
- Extensions respond correctly

## Next Steps

Would you like me to:

1. **Implement Phase 1** (Complete VSIX loader) first?
2. **Implement Phase 2** (Chat panel) first?
3. **Implement Phase 3** (Explorer view) first?
4. **Review VSIX loader** remaining code before implementing?
5. **Create all three implementations** in parallel?

All implementations will follow the codebase patterns:
- C++20, no Qt, no exceptions
- PatchResult-style returns
- HWND-based Win32 native controls
- No source file simplification
