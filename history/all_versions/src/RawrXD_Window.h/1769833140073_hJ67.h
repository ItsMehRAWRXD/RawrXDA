#pragma once
#include "RawrXD_SignalSlot.h"

namespace RawrXD {

class Window {
protected:
    HWND hwnd = nullptr;
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    
    // Event handlers - mapped from Windows messages
    virtual LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    virtual void paintEvent(PAINTSTRUCT& ps);
    virtual void resizeEvent(int w, int h);
    virtual void mousePressEvent(int x, int y, int button);
    virtual void mouseReleaseEvent(int x, int y, int button);
    virtual void mouseMoveEvent(int x, int y, int mods);
    virtual void keyPressEvent(int key, int mods);
    virtual void charEvent(wchar_t c);
    virtual void closeEvent();

public:
    Window* parent = nullptr;
    std::vector<Window*> children;
    
    Window() = default;
    explicit Window(Window* p) : parent(p) {} // Added constructor
    virtual ~Window();
    
    void create(Window* parent, const String& title, DWORD style = WS_OVERLAPPEDWINDOW, DWORD exStyle = 0);
    
    void show();
    void hide();
    void move(int x, int y);
    void resize(int w, int h);
    void setGeometry(int x, int y, int w, int h);
    
    void setTitle(const String& s);
    String title() const;
    
    HWND nativeHandle() const { return hwnd; }
    void update(); // InvalidateRect
    
    int width() const;
    int height() const;
    
    // Child management
    void addChild(Window* child);
    void removeChild(Window* child);
};

} // namespace RawrXD
