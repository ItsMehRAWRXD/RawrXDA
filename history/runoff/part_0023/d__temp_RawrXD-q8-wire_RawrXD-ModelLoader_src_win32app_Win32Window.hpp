/**
 * @file Win32Window.hpp
 * @brief Pure Win32 window framework - replaces QMainWindow
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace RawrXD::Win32 {

struct WindowConfig {
    std::string title = "RawrXD IDE";
    int width = 1200;
    int height = 800;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    bool resizable = true;
    bool maximizable = true;
    bool minimizable = true;
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = 0;
};

class Win32Window {
public:
    explicit Win32Window(const WindowConfig& config = {});
    virtual ~Win32Window();

    // Core window operations
    bool create();
    void show(int nCmdShow = SW_SHOW);
    void hide();
    void close();
    void destroy();

    // Window state
    bool isVisible() const;
    bool isMinimized() const;
    bool isMaximized() const;
    void minimize();
    void maximize();
    void restore();

    // Window properties
    void setTitle(const std::string& title);
    std::string getTitle() const;
    void setSize(int width, int height);
    void getSize(int& width, int& height) const;
    void setPosition(int x, int y);
    void getPosition(int& x, int& y) const;

    // Message handling
    void processMessages();
    bool pumpMessage(MSG& msg);

    // Event callbacks
    using WindowEventCallback = std::function<void()>;
    using SizeEventCallback = std::function<void(int width, int height)>;
    using KeyEventCallback = std::function<bool(WPARAM wParam, LPARAM lParam)>;
    using MouseEventCallback = std::function<bool(int x, int y, WPARAM wParam)>;

    void setOnClose(WindowEventCallback cb) { m_onClose = std::move(cb); }
    void setOnDestroy(WindowEventCallback cb) { m_onDestroy = std::move(cb); }
    void setOnResize(SizeEventCallback cb) { m_onResize = std::move(cb); }
    void setOnPaint(WindowEventCallback cb) { m_onPaint = std::move(cb); }
    void setOnKeyDown(KeyEventCallback cb) { m_onKeyDown = std::move(cb); }
    void setOnKeyUp(KeyEventCallback cb) { m_onKeyUp = std::move(cb); }
    void setOnMouseDown(MouseEventCallback cb) { m_onMouseDown = std::move(cb); }
    void setOnMouseUp(MouseEventCallback cb) { m_onMouseUp = std::move(cb); }
    void setOnMouseMove(MouseEventCallback cb) { m_onMouseMove = std::move(cb); }

    // Win32 specifics
    HWND getHWND() const { return m_hwnd; }
    HINSTANCE getHInstance() const { return m_hInstance; }

protected:
    // Override these in derived classes
    virtual LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    virtual void onCreate();
    virtual void onDestroy();
    virtual void onPaint(HDC hdc);
    virtual void onResize(int width, int height);
    virtual void onKeyDown(WPARAM wParam, LPARAM lParam);
    virtual void onKeyUp(WPARAM wParam, LPARAM lParam);
    virtual void onMouseDown(int x, int y, WPARAM wParam);
    virtual void onMouseUp(int x, int y, WPARAM wParam);
    virtual void onMouseMove(int x, int y, WPARAM wParam);

private:
    // Static message procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Window data
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    WindowConfig m_config;
    std::string m_className;
    bool m_created = false;

    // Event callbacks
    WindowEventCallback m_onClose;
    WindowEventCallback m_onDestroy;
    SizeEventCallback m_onResize;
    WindowEventCallback m_onPaint;
    KeyEventCallback m_onKeyDown;
    KeyEventCallback m_onKeyUp;
    MouseEventCallback m_onMouseDown;
    MouseEventCallback m_onMouseUp;
    MouseEventCallback m_onMouseMove;

    // Static registry for window instances
    static std::unordered_map<HWND, Win32Window*> s_windowMap;
    static int s_windowCounter;
};

} // namespace RawrXD::Win32