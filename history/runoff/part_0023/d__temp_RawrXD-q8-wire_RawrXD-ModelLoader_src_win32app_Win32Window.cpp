/**
 * @file Win32Window.cpp
 * @brief Implementation of pure Win32 window framework
 */

#include "Win32Window.hpp"
#include <stdexcept>
#include <iostream>

namespace RawrXD::Win32 {

// Static members
std::unordered_map<HWND, Win32Window*> Win32Window::s_windowMap;
int Win32Window::s_windowCounter = 0;

Win32Window::Win32Window(const WindowConfig& config)
    : m_config(config)
    , m_hInstance(GetModuleHandle(nullptr))
{
    // Generate unique class name
    m_className = "RawrXDWin32Window_" + std::to_string(s_windowCounter++);
}

Win32Window::~Win32Window()
{
    destroy();
}

bool Win32Window::create()
{
    if (m_created) return true;

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = m_className.c_str();
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        std::cerr << "Failed to register window class" << std::endl;
        return false;
    }

    // Create window
    m_hwnd = CreateWindowEx(
        m_config.exStyle,
        m_className.c_str(),
        m_config.title.c_str(),
        m_config.style,
        m_config.x,
        m_config.y,
        m_config.width,
        m_config.height,
        nullptr,        // parent
        nullptr,        // menu
        m_hInstance,
        nullptr         // lpParam
    );

    if (!m_hwnd) {
        std::cerr << "Failed to create window" << std::endl;
        return false;
    }

    // Store window instance
    s_windowMap[m_hwnd] = this;

    m_created = true;
    onCreate();

    return true;
}

void Win32Window::show(int nCmdShow)
{
    if (m_hwnd) {
        ShowWindow(m_hwnd, nCmdShow);
        UpdateWindow(m_hwnd);
    }
}

void Win32Window::hide()
{
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void Win32Window::close()
{
    if (m_hwnd) {
        PostMessage(m_hwnd, WM_CLOSE, 0, 0);
    }
}

void Win32Window::destroy()
{
    if (m_hwnd) {
        s_windowMap.erase(m_hwnd);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_created = false;
}

bool Win32Window::isVisible() const
{
    return m_hwnd && IsWindowVisible(m_hwnd);
}

bool Win32Window::isMinimized() const
{
    return m_hwnd && IsIconic(m_hwnd);
}

bool Win32Window::isMaximized() const
{
    return m_hwnd && IsZoomed(m_hwnd);
}

void Win32Window::minimize()
{
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_MINIMIZE);
    }
}

void Win32Window::maximize()
{
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_MAXIMIZE);
    }
}

void Win32Window::restore()
{
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_RESTORE);
    }
}

void Win32Window::setTitle(const std::string& title)
{
    if (m_hwnd) {
        SetWindowTextA(m_hwnd, title.c_str());
    }
}

std::string Win32Window::getTitle() const
{
    if (!m_hwnd) return "";

    char buffer[256];
    GetWindowTextA(m_hwnd, buffer, sizeof(buffer));
    return buffer;
}

void Win32Window::setSize(int width, int height)
{
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    }
}

void Win32Window::getSize(int& width, int& height) const
{
    if (m_hwnd) {
        RECT rect;
        GetClientRect(m_hwnd, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    } else {
        width = height = 0;
    }
}

void Win32Window::setPosition(int x, int y)
{
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

void Win32Window::getPosition(int& x, int& y) const
{
    if (m_hwnd) {
        RECT rect;
        GetWindowRect(m_hwnd, &rect);
        x = rect.left;
        y = rect.top;
    } else {
        x = y = 0;
    }
}

void Win32Window::processMessages()
{
    MSG msg;
    while (pumpMessage(msg)) {
        // Continue processing
    }
}

bool Win32Window::pumpMessage(MSG& msg)
{
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        return true;
    }
    return false;
}

LRESULT CALLBACK Win32Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto it = s_windowMap.find(hwnd);
    if (it != s_windowMap.end()) {
        Win32Window* window = it->second;
        return window->handleMessage(msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT Win32Window::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CLOSE:
            if (m_onClose) m_onClose();
            return 0;

        case WM_DESTROY:
            onDestroy();
            if (m_onDestroy) m_onDestroy();
            PostQuitMessage(0);
            return 0;

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            onResize(width, height);
            if (m_onResize) m_onResize(width, height);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            onPaint(hdc);
            if (m_onPaint) m_onPaint();
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_KEYDOWN:
            onKeyDown(wParam, lParam);
            if (m_onKeyDown && m_onKeyDown(wParam, lParam)) return 0;
            break;

        case WM_KEYUP:
            onKeyUp(wParam, lParam);
            if (m_onKeyUp && m_onKeyUp(wParam, lParam)) return 0;
            break;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            onMouseDown(x, y, wParam);
            if (m_onMouseDown && m_onMouseDown(x, y, wParam)) return 0;
            break;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            onMouseUp(x, y, wParam);
            if (m_onMouseUp && m_onMouseUp(x, y, wParam)) return 0;
            break;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            onMouseMove(x, y, wParam);
            if (m_onMouseMove && m_onMouseMove(x, y, wParam)) return 0;
            break;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

void Win32Window::onCreate() {}
void Win32Window::onDestroy() {}
void Win32Window::onPaint(HDC hdc) {}
void Win32Window::onResize(int width, int height) {}
void Win32Window::onKeyDown(WPARAM wParam, LPARAM lParam) {}
void Win32Window::onKeyUp(WPARAM wParam, LPARAM lParam) {}
void Win32Window::onMouseDown(int x, int y, WPARAM wParam) {}
void Win32Window::onMouseUp(int x, int y, WPARAM wParam) {}
void Win32Window::onMouseMove(int x, int y, WPARAM wParam) {}

} // namespace RawrXD::Win32