#pragma once
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>

#pragma comment(lib, "shlwapi.lib")

namespace RawrXD::Agentic {

// ============================================================================
// File type detection and icon mapping
// ============================================================================
struct FileTypeInfo {
    const wchar_t* extension;
    const wchar_t* description;
    int iconIndex;       // index in our custom imagelist
    bool isSource;       // is it source code?
};

static const FileTypeInfo g_fileTypes[] = {
    {L".cpp",   L"C++ Source",        0,  true},
    {L".c",     L"C Source",          0,  true},
    {L".h",     L"C/C++ Header",      1,  true},
    {L".hpp",   L"C++ Header",        1,  true},
    {L".py",    L"Python",            2,  true},
    {L".js",    L"JavaScript",        3,  true},
    {L".ts",    L"TypeScript",        3,  true},
    {L".rs",    L"Rust",              4,  true},
    {L".go",    L"Go",                4,  true},
    {L".json",  L"JSON",              5,  false},
    {L".xml",   L"XML",               5,  false},
    {L".yaml",  L"YAML",              5,  false},
    {L".yml",   L"YAML",              5,  false},
    {L".md",    L"Markdown",          6,  false},
    {L".txt",   L"Text",              6,  false},
    {L".asm",   L"Assembly",          7,  true},
    {L".cmake", L"CMake",             8,  false},
    {L".bat",   L"Batch",             9,  true},
    {L".ps1",   L"PowerShell",        9,  true},
    {L".dll",   L"Library",           10, false},
    {L".exe",   L"Executable",        10, false},
    {L".lib",   L"Static Library",    10, false},
    {L".obj",   L"Object File",       10, false},
    {L".gguf",  L"GGUF Model",        11, false},
    {L".log",   L"Log File",          6,  false},
    {nullptr,   L"File",              12, false},  // default
};

// ============================================================================
// FileNode - represents a file or directory in the tree
// ============================================================================
struct FileNode {
    std::wstring name;
    std::wstring fullPath;
    bool isDirectory = false;
    bool isExpanded = false;
    bool childrenLoaded = false;
    HTREEITEM hTreeItem = nullptr;
    std::vector<std::unique_ptr<FileNode>> children;

    int getIconIndex() const {
        if (isDirectory) return 13; // folder icon index

        std::wstring ext = name;
        size_t dotPos = ext.find_last_of(L'.');
        if (dotPos != std::wstring::npos) {
            ext = ext.substr(dotPos);
            // Convert to lowercase
            for (auto& c : ext) c = towlower(c);

            for (int i = 0; g_fileTypes[i].extension != nullptr; ++i) {
                if (ext == g_fileTypes[i].extension) {
                    return g_fileTypes[i].iconIndex;
                }
            }
        }
        return 12; // default file icon
    }

    bool isSourceFile() const {
        if (isDirectory) return false;
        std::wstring ext = name;
        size_t dotPos = ext.find_last_of(L'.');
        if (dotPos != std::wstring::npos) {
            ext = ext.substr(dotPos);
            for (auto& c : ext) c = towlower(c);
            for (int i = 0; g_fileTypes[i].extension != nullptr; ++i) {
                if (ext == g_fileTypes[i].extension && g_fileTypes[i].isSource) {
                    return true;
                }
            }
        }
        return false;
    }
};

// ============================================================================
// FileWatcher - monitors filesystem changes
// ============================================================================
class FileWatcher {
private:
    std::wstring watchPath_;
    HANDLE hDirectory_ = INVALID_HANDLE_VALUE;
    HANDLE hStopEvent_ = nullptr;
    std::unique_ptr<std::thread> watchThread_;
    std::atomic<bool> running_{false};
    std::function<void(const std::wstring&, DWORD)> onChange_;

public:
    FileWatcher() {
        hStopEvent_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    }

    ~FileWatcher() {
        stop();
        if (hStopEvent_) CloseHandle(hStopEvent_);
    }

    void start(const std::wstring& path, std::function<void(const std::wstring&, DWORD)> onChange) {
        stop();

        watchPath_ = path;
        onChange_ = onChange;

        hDirectory_ = CreateFileW(
            path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (hDirectory_ == INVALID_HANDLE_VALUE) return;

        running_ = true;
        ResetEvent(hStopEvent_);
        watchThread_ = std::make_unique<std::thread>([this]() { watchLoop(); });
    }

    void stop() {
        running_ = false;
        if (hStopEvent_) SetEvent(hStopEvent_);

        if (watchThread_ && watchThread_->joinable()) {
            watchThread_->join();
        }
        watchThread_.reset();

        if (hDirectory_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hDirectory_);
            hDirectory_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    void watchLoop() {
        BYTE buffer[4096];
        OVERLAPPED overlapped = {};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        HANDLE handles[2] = { overlapped.hEvent, hStopEvent_ };

        while (running_) {
            DWORD bytesReturned = 0;
            ResetEvent(overlapped.hEvent);

            if (!ReadDirectoryChangesW(
                hDirectory_,
                buffer,
                sizeof(buffer),
                TRUE, // watch subtree
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_CREATION,
                &bytesReturned,
                &overlapped,
                nullptr
            )) {
                break;
            }

            DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            if (waitResult == WAIT_OBJECT_0 + 1) {
                // Stop event signaled
                CancelIo(hDirectory_);
                break;
            }

            if (waitResult == WAIT_OBJECT_0) {
                if (!GetOverlappedResult(hDirectory_, &overlapped, &bytesReturned, FALSE)) {
                    continue;
                }

                FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)buffer;
                while (fni) {
                    std::wstring fileName(fni->FileName, fni->FileNameLength / sizeof(wchar_t));
                    if (onChange_) {
                        onChange_(fileName, fni->Action);
                    }

                    if (fni->NextEntryOffset == 0) break;
                    fni = (FILE_NOTIFY_INFORMATION*)((BYTE*)fni + fni->NextEntryOffset);
                }
            }
        }

        CloseHandle(overlapped.hEvent);
    }
};

// ============================================================================
// FileExplorer - full-featured file tree with lazy loading and file watching
// ============================================================================
class FileExplorer {
private:
    HWND hwndTree_ = nullptr;
    HWND hwndParent_ = nullptr;
    HIMAGELIST hImageList_ = nullptr;

    std::wstring rootPath_;
    std::unique_ptr<FileNode> rootNode_;
    std::unordered_map<HTREEITEM, FileNode*> nodeMap_;

    FileWatcher watcher_;
    std::mutex mutex_;

    // Callbacks
    std::function<void(const std::wstring&)> onFileOpen_;
    std::function<void(const std::wstring&)> onFileSelected_;
    std::function<void(const std::wstring&)> onDirectoryChanged_;

    // Ignore patterns
    std::vector<std::wstring> ignorePatterns_ = {
        L".git", L".vs", L"node_modules", L"__pycache__", L".cache",
        L"build", L"Debug", L"Release", L"x64", L"x86",
        L".obj", L".pdb", L".ilk", L".suo", L".user"
    };

    // Sort settings
    bool sortDirectoriesFirst_ = true;
    bool showHiddenFiles_ = false;

public:
    FileExplorer() = default;

    ~FileExplorer() {
        watcher_.stop();
        if (hImageList_) ImageList_Destroy(hImageList_);
    }

    // Initialize the file explorer in a given parent window
    bool initialize(HWND hwndParent, const RECT& rect) {
        hwndParent_ = hwndParent;

        // Create image list for file icons
        createImageList();

        // Create the tree view control
        hwndTree_ = CreateWindowExW(
            0,
            WC_TREEVIEWW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER |
            TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT |
            TVS_SHOWSELALWAYS | TVS_EDITLABELS | TVS_TRACKSELECT,
            rect.left, rect.top,
            rect.right - rect.left, rect.bottom - rect.top,
            hwndParent,
            (HMENU)1050,
            GetModuleHandle(nullptr),
            nullptr
        );

        if (!hwndTree_) return false;

        // Assign image list
        TreeView_SetImageList(hwndTree_, hImageList_, TVSIL_NORMAL);

        // Set colors for dark theme
        TreeView_SetBkColor(hwndTree_, RGB(30, 30, 30));
        TreeView_SetTextColor(hwndTree_, RGB(212, 212, 212));
        TreeView_SetLineColor(hwndTree_, RGB(60, 60, 60));

        // Enable tooltips
        DWORD style = GetWindowLong(hwndTree_, GWL_STYLE);
        SetWindowLong(hwndTree_, GWL_STYLE, style | TVS_INFOTIP);

        return true;
    }

    // Set the root directory and populate the tree
    void setRootPath(const std::wstring& path) {
        std::lock_guard<std::mutex> lock(mutex_);

        rootPath_ = path;
        nodeMap_.clear();

        // Clear existing tree
        TreeView_DeleteAllItems(hwndTree_);

        // Create root node
        rootNode_ = std::make_unique<FileNode>();
        rootNode_->name = path.substr(path.find_last_of(L'\\') + 1);
        rootNode_->fullPath = path;
        rootNode_->isDirectory = true;

        // Insert root into tree
        TVINSERTSTRUCT tvis = {};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_FIRST;
        tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM;
        tvis.item.pszText = (LPWSTR)rootNode_->name.c_str();
        tvis.item.iImage = 14;       // open folder icon
        tvis.item.iSelectedImage = 14;
        tvis.item.cChildren = 1;     // has children (lazy load)
        tvis.item.lParam = (LPARAM)rootNode_.get();

        rootNode_->hTreeItem = TreeView_InsertItem(hwndTree_, &tvis);
        nodeMap_[rootNode_->hTreeItem] = rootNode_.get();

        // Load first level
        loadChildren(rootNode_.get());
        TreeView_Expand(hwndTree_, rootNode_->hTreeItem, TVE_EXPAND);
        rootNode_->isExpanded = true;

        // Start file watcher
        watcher_.start(path, [this](const std::wstring& fileName, DWORD action) {
            onFileSystemChange(fileName, action);
        });
    }

    // Handle tree view notifications - call this from parent WndProc
    bool handleNotify(LPARAM lParam) {
        NMHDR* nmhdr = (NMHDR*)lParam;
        if (nmhdr->hwndFrom != hwndTree_) return false;

        switch (nmhdr->code) {
            case TVN_ITEMEXPANDING: {
                NMTREEVIEW* pnmtv = (NMTREEVIEW*)lParam;
                if (pnmtv->action == TVE_EXPAND) {
                    FileNode* node = (FileNode*)pnmtv->itemNew.lParam;
                    if (node && !node->childrenLoaded) {
                        loadChildren(node);
                    }
                    node->isExpanded = true;
                }
                return true;
            }

            case TVN_SELCHANGED: {
                NMTREEVIEW* pnmtv = (NMTREEVIEW*)lParam;
                FileNode* node = (FileNode*)pnmtv->itemNew.lParam;
                if (node && onFileSelected_) {
                    onFileSelected_(node->fullPath);
                }
                return true;
            }

            case NM_DBLCLK: {
                HTREEITEM hItem = TreeView_GetSelection(hwndTree_);
                if (hItem) {
                    auto it = nodeMap_.find(hItem);
                    if (it != nodeMap_.end()) {
                        FileNode* node = it->second;
                        if (!node->isDirectory && onFileOpen_) {
                            onFileOpen_(node->fullPath);
                        }
                    }
                }
                return true;
            }

            case NM_RCLICK: {
                // Show context menu
                showContextMenu();
                return true;
            }

            case TVN_GETINFOTIP: {
                NMTVGETINFOTIP* pTip = (NMTVGETINFOTIP*)lParam;
                FileNode* node = (FileNode*)pTip->lParam;
                if (node) {
                    // Show full path and file size
                    std::wstring tip = node->fullPath;
                    if (!node->isDirectory) {
                        WIN32_FILE_ATTRIBUTE_DATA fad;
                        if (GetFileAttributesExW(node->fullPath.c_str(), GetFileExInfoStandard, &fad)) {
                            ULONGLONG size = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
                            tip += L"\nSize: " + formatFileSize(size);
                        }
                    }
                    wcsncpy_s(pTip->pszText, pTip->cchTextMax, tip.c_str(), _TRUNCATE);
                }
                return true;
            }

            case TVN_BEGINLABELEDIT: {
                // Allow editing
                return false; // FALSE = allow edit
            }

            case TVN_ENDLABELEDIT: {
                NMTVDISPINFO* pdi = (NMTVDISPINFO*)lParam;
                if (pdi->item.pszText) {
                    FileNode* node = (FileNode*)pdi->item.lParam;
                    if (node) {
                        return renameNode(node, pdi->item.pszText);
                    }
                }
                return false;
            }
        }

        return false;
    }

    // Resize the tree view
    void resize(const RECT& rect) {
        if (hwndTree_) {
            MoveWindow(hwndTree_, rect.left, rect.top,
                      rect.right - rect.left, rect.bottom - rect.top, TRUE);
        }
    }

    // Get the tree view HWND
    HWND getHWND() const { return hwndTree_; }

    // Set callbacks
    void setOnFileOpen(std::function<void(const std::wstring&)> callback) {
        onFileOpen_ = callback;
    }
    void setOnFileSelected(std::function<void(const std::wstring&)> callback) {
        onFileSelected_ = callback;
    }
    void setOnDirectoryChanged(std::function<void(const std::wstring&)> callback) {
        onDirectoryChanged_ = callback;
    }

    // Refresh the tree
    void refresh() {
        if (!rootPath_.empty()) {
            setRootPath(rootPath_);
        }
    }

    // Find and select a file in the tree
    bool selectFile(const std::wstring& filePath) {
        FileNode* node = findNodeByPath(rootNode_.get(), filePath);
        if (node && node->hTreeItem) {
            TreeView_SelectItem(hwndTree_, node->hTreeItem);
            TreeView_EnsureVisible(hwndTree_, node->hTreeItem);
            return true;
        }
        return false;
    }

    // Create new file in current directory
    bool createNewFile(const std::wstring& parentPath, const std::wstring& fileName) {
        std::wstring fullPath = parentPath + L"\\" + fileName;
        HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
            refresh();
            return true;
        }
        return false;
    }

    // Create new directory
    bool createNewDirectory(const std::wstring& parentPath, const std::wstring& dirName) {
        std::wstring fullPath = parentPath + L"\\" + dirName;
        if (CreateDirectoryW(fullPath.c_str(), nullptr)) {
            refresh();
            return true;
        }
        return false;
    }

    // Delete file or directory
    bool deleteNode(const std::wstring& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) return false;

        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            return RemoveDirectoryW(path.c_str()) != 0;
        } else {
            return DeleteFileW(path.c_str()) != 0;
        }
    }

private:
    // Create the image list with file type icons
    void createImageList() {
        hImageList_ = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 16, 4);

        // We'll create simple colored rectangle icons for each type
        // In production, you'd load real icons from resources
        struct IconDef { COLORREF color; const wchar_t* letter; };
        IconDef iconDefs[] = {
            {RGB(50, 120, 200),  L"C"},   // 0:  C/C++ source
            {RGB(80, 150, 80),   L"H"},   // 1:  header
            {RGB(55, 118, 171),  L"P"},   // 2:  Python
            {RGB(247, 223, 30),  L"J"},   // 3:  JavaScript/TS
            {RGB(222, 165, 132), L"R"},   // 4:  Rust/Go
            {RGB(180, 180, 60),  L"{}"},  // 5:  JSON/XML/YAML
            {RGB(150, 150, 150), L"T"},   // 6:  Text/Markdown
            {RGB(200, 100, 50),  L"A"},   // 7:  Assembly
            {RGB(100, 180, 100), L"M"},   // 8:  CMake
            {RGB(0, 120, 200),   L">"},   // 9:  Script (bat/ps1)
            {RGB(180, 80, 80),   L"B"},   // 10: Binary
            {RGB(200, 150, 50),  L"G"},   // 11: GGUF model
            {RGB(120, 120, 120), L"?"},   // 12: Default file
            {RGB(200, 180, 50),  L"D"},   // 13: Folder (closed)
            {RGB(220, 200, 80),  L"D"},   // 14: Folder (open)
        };

        HDC hScreenDC = GetDC(nullptr);
        for (int i = 0; i < 15; ++i) {
            HDC hdc = CreateCompatibleDC(hScreenDC);
            HBITMAP hbm = CreateCompatibleBitmap(hScreenDC, 16, 16);
            SelectObject(hdc, hbm);

            // Fill with icon color
            RECT rc = {0, 0, 16, 16};
            HBRUSH hBrush = CreateSolidBrush(iconDefs[i].color);
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            // Draw border
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
            SelectObject(hdc, hPen);
            MoveToEx(hdc, 0, 0, nullptr);
            LineTo(hdc, 15, 0); LineTo(hdc, 15, 15);
            LineTo(hdc, 0, 15); LineTo(hdc, 0, 0);
            DeleteObject(hPen);

            // Draw letter
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            HFONT hFont = CreateFont(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, 0, 0, 0, 0, L"Consolas");
            SelectObject(hdc, hFont);
            DrawTextW(hdc, iconDefs[i].letter, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(hFont);

            ImageList_Add(hImageList_, hbm, nullptr);
            DeleteObject(hbm);
            DeleteDC(hdc);
        }
        ReleaseDC(nullptr, hScreenDC);
    }

    // Load children of a node (lazy loading)
    void loadChildren(FileNode* parent) {
        if (!parent || !parent->isDirectory) return;
        if (parent->childrenLoaded) return;

        parent->children.clear();
        parent->childrenLoaded = true;

        std::wstring searchPath = parent->fullPath + L"\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        std::vector<std::unique_ptr<FileNode>> dirs;
        std::vector<std::unique_ptr<FileNode>> files;

        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

            // Check ignore patterns
            if (shouldIgnore(fd.cFileName)) continue;

            // Check hidden files
            if (!showHiddenFiles_ && (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) continue;

            auto node = std::make_unique<FileNode>();
            node->name = fd.cFileName;
            node->fullPath = parent->fullPath + L"\\" + fd.cFileName;
            node->isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

            if (node->isDirectory) {
                dirs.push_back(std::move(node));
            } else {
                files.push_back(std::move(node));
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);

        // Sort: directories first, then files, both alphabetically
        auto sortFn = [](const std::unique_ptr<FileNode>& a, const std::unique_ptr<FileNode>& b) {
            return _wcsicmp(a->name.c_str(), b->name.c_str()) < 0;
        };
        std::sort(dirs.begin(), dirs.end(), sortFn);
        std::sort(files.begin(), files.end(), sortFn);

        // Insert directories first
        for (auto& dir : dirs) {
            insertTreeNode(parent, dir.get());
            parent->children.push_back(std::move(dir));
        }

        // Then files
        for (auto& file : files) {
            insertTreeNode(parent, file.get());
            parent->children.push_back(std::move(file));
        }
    }

    // Insert a node into the tree view
    void insertTreeNode(FileNode* parent, FileNode* node) {
        TVINSERTSTRUCT tvis = {};
        tvis.hParent = parent->hTreeItem;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM;
        tvis.item.pszText = (LPWSTR)node->name.c_str();

        int iconIdx = node->getIconIndex();
        tvis.item.iImage = iconIdx;
        tvis.item.iSelectedImage = (node->isDirectory) ? 14 : iconIdx;
        tvis.item.cChildren = node->isDirectory ? 1 : 0;
        tvis.item.lParam = (LPARAM)node;

        node->hTreeItem = TreeView_InsertItem(hwndTree_, &tvis);
        nodeMap_[node->hTreeItem] = node;
    }

    // Check if a file/directory should be ignored
    bool shouldIgnore(const wchar_t* name) const {
        for (const auto& pattern : ignorePatterns_) {
            if (_wcsicmp(name, pattern.c_str()) == 0) return true;

            // Check extension patterns (e.g., ".obj")
            if (pattern[0] == L'.') {
                size_t nameLen = wcslen(name);
                size_t patLen = pattern.length();
                if (nameLen > patLen) {
                    if (_wcsicmp(name + nameLen - patLen, pattern.c_str()) == 0) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Find a node by file path
    FileNode* findNodeByPath(FileNode* root, const std::wstring& path) {
        if (!root) return nullptr;
        if (_wcsicmp(root->fullPath.c_str(), path.c_str()) == 0) return root;

        for (auto& child : root->children) {
            FileNode* result = findNodeByPath(child.get(), path);
            if (result) return result;
        }
        return nullptr;
    }

    // Handle filesystem change notifications
    void onFileSystemChange(const std::wstring& fileName, DWORD action) {
        // Post a message to the parent window to handle on the UI thread
        PostMessage(hwndParent_, WM_USER + 100, (WPARAM)action, 0);
    }

    // Rename a node
    bool renameNode(FileNode* node, const wchar_t* newName) {
        std::wstring oldPath = node->fullPath;
        std::wstring newPath = oldPath.substr(0, oldPath.find_last_of(L'\\') + 1) + newName;

        if (MoveFileW(oldPath.c_str(), newPath.c_str())) {
            node->name = newName;
            node->fullPath = newPath;
            return true;
        }
        return false;
    }

    // Show right-click context menu
    void showContextMenu() {
        POINT pt;
        GetCursorPos(&pt);

        HTREEITEM hItem = TreeView_GetSelection(hwndTree_);
        FileNode* node = nullptr;
        if (hItem) {
            auto it = nodeMap_.find(hItem);
            if (it != nodeMap_.end()) node = it->second;
        }

        HMENU hMenu = CreatePopupMenu();

        if (node && node->isDirectory) {
            AppendMenu(hMenu, MF_STRING, 5001, L"New File...");
            AppendMenu(hMenu, MF_STRING, 5002, L"New Folder...");
            AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        }

        if (node) {
            AppendMenu(hMenu, MF_STRING, 5003, L"Rename");
            AppendMenu(hMenu, MF_STRING, 5004, L"Delete");
            AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenu(hMenu, MF_STRING, 5005, L"Copy Path");
            AppendMenu(hMenu, MF_STRING, 5006, L"Open in Explorer");
        }

        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(hMenu, MF_STRING, 5007, L"Refresh");
        AppendMenu(hMenu, MF_STRING, 5008, L"Collapse All");

        UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, 0, hwndParent_, nullptr);
        DestroyMenu(hMenu);

        handleContextMenuCommand(cmd, node);
    }

    // Handle context menu commands
    void handleContextMenuCommand(UINT cmd, FileNode* node) {
        switch (cmd) {
            case 5001: { // New File
                if (node) {
                    // Prompt for filename
                    wchar_t fileName[MAX_PATH] = L"untitled.txt";
                    // In production, use a dialog; for now use TreeView_EditLabel
                    createNewFile(node->fullPath, fileName);
                }
                break;
            }
            case 5002: { // New Folder
                if (node) {
                    createNewDirectory(node->fullPath, L"NewFolder");
                }
                break;
            }
            case 5003: { // Rename
                if (node && node->hTreeItem) {
                    TreeView_EditLabel(hwndTree_, node->hTreeItem);
                }
                break;
            }
            case 5004: { // Delete
                if (node) {
                    std::wstring msg = L"Delete \"" + node->name + L"\"?";
                    if (MessageBoxW(hwndParent_, msg.c_str(), L"Confirm Delete", MB_YESNO | MB_ICONWARNING) == IDYES) {
                        if (deleteNode(node->fullPath)) {
                            refresh();
                        }
                    }
                }
                break;
            }
            case 5005: { // Copy Path
                if (node) {
                    if (OpenClipboard(hwndParent_)) {
                        EmptyClipboard();
                        size_t size = (node->fullPath.length() + 1) * sizeof(wchar_t);
                        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
                        if (hGlobal) {
                            wchar_t* buffer = (wchar_t*)GlobalLock(hGlobal);
                            wcscpy_s(buffer, node->fullPath.length() + 1, node->fullPath.c_str());
                            GlobalUnlock(hGlobal);
                            SetClipboardData(CF_UNICODETEXT, hGlobal);
                        }
                        CloseClipboard();
                    }
                }
                break;
            }
            case 5006: { // Open in Explorer
                if (node) {
                    std::wstring path = node->isDirectory ? node->fullPath : node->fullPath.substr(0, node->fullPath.find_last_of(L'\\'));
                    ShellExecuteW(nullptr, L"explore", path.c_str(), nullptr, nullptr, SW_SHOW);
                }
                break;
            }
            case 5007: // Refresh
                refresh();
                break;
            case 5008: { // Collapse All
                if (rootNode_ && rootNode_->hTreeItem) {
                    collapseAll(rootNode_->hTreeItem);
                }
                break;
            }
        }
    }

    // Collapse all tree items
    void collapseAll(HTREEITEM hItem) {
        HTREEITEM hChild = TreeView_GetChild(hwndTree_, hItem);
        while (hChild) {
            collapseAll(hChild);
            hChild = TreeView_GetNextSibling(hwndTree_, hChild);
        }
        TreeView_Expand(hwndTree_, hItem, TVE_COLLAPSE);
    }

    // Helper: create new file
    bool createNewFile(const std::wstring& parentPath, const std::wstring& fileName) {
        std::wstring fullPath = parentPath + L"\\" + fileName;
        HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
            refresh();
            return true;
        }
        return false;
    }

    // Helper: create new directory
    bool createNewDirectory(const std::wstring& parentPath, const std::wstring& dirName) {
        std::wstring fullPath = parentPath + L"\\" + dirName;
        if (CreateDirectoryW(fullPath.c_str(), nullptr)) {
            refresh();
            return true;
        }
        return false;
    }

    // Format file size for tooltips
    static std::wstring formatFileSize(ULONGLONG size) {
        wchar_t buf[64];
        if (size < 1024) {
            swprintf_s(buf, L"%llu B", size);
        } else if (size < 1024 * 1024) {
            swprintf_s(buf, L"%.1f KB", size / 1024.0);
        } else if (size < 1024ULL * 1024 * 1024) {
            swprintf_s(buf, L"%.1f MB", size / (1024.0 * 1024.0));
        } else {
            swprintf_s(buf, L"%.2f GB", size / (1024.0 * 1024.0 * 1024.0));
        }
        return buf;
    }
};

} // namespace RawrXD::Agentic
