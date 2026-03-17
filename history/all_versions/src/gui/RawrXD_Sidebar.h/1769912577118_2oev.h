#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace RawrXD {

class Sidebar {
    HWND hwnd;
    HWND hParent;
    HWND hListBox;
    
public:
    Sidebar();
    ~Sidebar();
    
    bool create(HWND parent, int x, int y, int w, int h);
    HWND handle() const { return hwnd; }
    void onResize(int w, int h);
    
    void addFile(const std::wstring& filename);
    void clear();
    
    // Populate with actual directory
    void populate(const std::wstring& path);
};

}
