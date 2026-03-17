#include "native_file_tree.h"
#include "native_widgets.h"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <filesystem>

#ifdef _WIN32

NativeFileTree::NativeFileTree() 
    : m_treeView(nullptr)
    , m_parent(nullptr)
    , m_onDoubleClick(nullptr)
    , m_onContextMenu(nullptr) {
}

NativeFileTree::~NativeFileTree() {
    if (m_treeView) {
        DestroyWindow(m_treeView);
    }
}

bool NativeFileTree::create(NativeWidget* parent, int x, int y, int width, int height, HWND parentHandle) {
    HWND resolvedParent = nullptr;
    if (parent && parent->getHandle()) {
        resolvedParent = parent->getHandle();
    } else if (parentHandle) {
        resolvedParent = parentHandle;
    }

    if (!resolvedParent) return false;
    
    m_parent = resolvedParent;
    
    // Initialize common controls for tree view
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icex);
    
    m_treeView = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEW,
        "",
        WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
        x, y, width, height,
        m_parent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (m_treeView) {
        // Subclass the tree view to handle custom messages (store original proc to avoid recursion)
        SetWindowLongPtr(m_treeView, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        m_originalProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtr(m_treeView, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TreeViewProc)));
        
        // Set root to current directory
        char currentDir[MAX_PATH];
        GetCurrentDirectory(MAX_PATH, currentDir);
        setRootPath(currentDir);
        
        return true;
    }
    
    return false;
}

void NativeFileTree::setRootPath(const std::string& path) {
    m_rootPath = path;
    refresh();
}

void NativeFileTree::refresh() {
    if (m_rootPath.empty()) return;
    
    // Clear existing items
    TreeView_DeleteAllItems(m_treeView);
    m_entries.clear();
    
    enumerateDirectory(m_rootPath);
    updateTreeView();
}

void NativeFileTree::enumerateDirectory(const std::string& path) {
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            FileEntry fileEntry;
            fileEntry.path = entry.path().string();
            fileEntry.name = entry.path().filename().string();
            fileEntry.isDirectory = entry.is_directory();
            
            if (!fileEntry.isDirectory) {
                fileEntry.size = entry.file_size();
            }
            
            // Get modification time
            auto ftime = std::filesystem::last_write_time(entry);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
            char timeBuf[64];
            ctime_s(timeBuf, sizeof(timeBuf), &tt);
            fileEntry.modified = timeBuf;
            fileEntry.modified.pop_back(); // Remove newline
            
            m_entries.push_back(fileEntry);
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        std::cout << "[FileTree] Error enumerating " << path << ": " << ex.what() << std::endl;
    }
}

void NativeFileTree::updateTreeView() {
    TVINSERTSTRUCT tvInsert;
    tvInsert.hParent = TVI_ROOT;
    tvInsert.hInsertAfter = TVI_LAST;
    tvInsert.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvInsert.item.pszText = const_cast<char*>(m_rootPath.c_str());
    tvInsert.item.iImage = 0;
    tvInsert.item.iSelectedImage = 0;
    
    HTREEITEM rootItem = TreeView_InsertItem(m_treeView, &tvInsert);
    
    for (const auto& entry : m_entries) {
        tvInsert.hParent = rootItem;
        tvInsert.item.pszText = const_cast<char*>(entry.name.c_str());
        tvInsert.item.iImage = entry.isDirectory ? 1 : 2;
        tvInsert.item.iSelectedImage = entry.isDirectory ? 1 : 2;
        
        TreeView_InsertItem(m_treeView, &tvInsert);
    }
    
    TreeView_Expand(m_treeView, rootItem, TVE_EXPAND);
}

void NativeFileTree::setOnDoubleClick(std::function<void(const std::string&)> callback) {
    m_onDoubleClick = callback;
}

void NativeFileTree::setOnContextMenu(std::function<void(int, int)> callback) {
    m_onContextMenu = callback;
}

std::string NativeFileTree::getSelectedPath() const {
    HTREEITEM selected = TreeView_GetSelection(m_treeView);
    if (!selected) return "";
    
    TVITEM item;
    item.mask = TVIF_TEXT;
    item.hItem = selected;
    char buffer[MAX_PATH];
    item.pszText = buffer;
    item.cchTextMax = MAX_PATH;
    
    if (TreeView_GetItem(m_treeView, &item)) {
        return buffer;
    }
    
    return "";
}

std::vector<NativeFileTree::FileEntry> NativeFileTree::getCurrentEntries() const {
    return m_entries;
}

void NativeFileTree::show() {
    if (m_treeView) {
        ShowWindow(m_treeView, SW_SHOW);
    }
}

void NativeFileTree::hide() {
    if (m_treeView) {
        ShowWindow(m_treeView, SW_HIDE);
    }
}

void NativeFileTree::setVisible(bool visible) {
    if (m_treeView) {
        ShowWindow(m_treeView, visible ? SW_SHOW : SW_HIDE);
    }
}

LRESULT CALLBACK NativeFileTree::TreeViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NativeFileTree* tree = reinterpret_cast<NativeFileTree*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    if (tree) {
        return tree->handleMessage(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT NativeFileTree::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_LBUTTONDBLCLK: {
            if (m_onDoubleClick) {
                std::string selectedPath = getSelectedPath();
                if (!selectedPath.empty()) {
                    m_onDoubleClick(selectedPath);
                }
            }
            break;
        }
        case WM_RBUTTONDOWN: {
            if (m_onContextMenu) {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                m_onContextMenu(x, y);
            }
            break;
        }
        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->code == TVN_SELCHANGED) {
                // Selection changed - could update status or preview
            }
            break;
        }
    }
    
    if (msg == WM_NCDESTROY) {
        // Best-effort: restore original proc before destruction.
        if (m_originalProc) {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalProc));
        }
    }

    if (m_originalProc) {
        return CallWindowProc(m_originalProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

#endif // _WIN32