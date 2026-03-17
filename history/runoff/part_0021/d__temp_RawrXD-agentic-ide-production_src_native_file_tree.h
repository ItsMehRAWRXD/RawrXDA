#ifndef NATIVE_FILE_TREE_H
#define NATIVE_FILE_TREE_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

#include "native_layout.h"

class NativeFileTree {
public:
    struct FileEntry {
        std::string name;
        std::string path;
        bool isDirectory;
        size_t size;
        std::string modified;
    };

    NativeFileTree();
    ~NativeFileTree();

    bool create(NativeWidget* parent, int x, int y, int width, int height, HWND parentHandle = nullptr);
    void setRootPath(const std::string& path);
    void refresh();
    
    void setOnDoubleClick(std::function<void(const std::string&)> callback);
    void setOnContextMenu(std::function<void(int, int)> callback);
    
    std::string getSelectedPath() const;
    std::vector<FileEntry> getCurrentEntries() const;
    
    void show();
    void hide();
    void setVisible(bool visible);

#ifdef _WIN32
    HWND getHandle() const { return m_treeView; }
#endif

private:
    void enumerateDirectory(const std::string& path);
    void updateTreeView();
    
#ifdef _WIN32
    HWND m_treeView;
    HWND m_parent;
    WNDPROC m_originalProc = nullptr;
    
    static LRESULT CALLBACK TreeViewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif
    
    std::string m_rootPath;
    std::vector<FileEntry> m_entries;
    std::function<void(const std::string&)> m_onDoubleClick;
    std::function<void(int, int)> m_onContextMenu;
};

#endif // NATIVE_FILE_TREE_H