#pragma once
#include <windows.h>
#include <string>

namespace RawrXD {

class Panel {
    HWND hwnd;
    HWND hParent;
    HWND hEdit;
    
public:
    Panel();
    ~Panel();
    
    bool create(HWND parent, int x, int y, int w, int h);
    HWND handle() const { return hwnd; }
    void onResize(int w, int h);
    
    void log(const std::wstring& msg);
    void clear();
};

}
