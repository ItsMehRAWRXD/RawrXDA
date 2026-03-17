#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>

#define IDC_MAIN_EDIT 101
#define IDC_MAIN_STATUS 102

class Win32IDE {
public:
    Win32IDE(HINSTANCE hInstance);
    ~Win32IDE();

    bool Create(const std::wstring& title, int width, int height);
    int Run();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void OnCreate(HWND hWnd);
    void OnSize(int width, int height);
    void OnCommand(WPARAM wParam);
    void OnClose();

    void CreateMainMenu(HWND hWnd);
    void CreateControls(HWND hWnd);

    HINSTANCE hInstance_;
    HWND hWnd_;
    HWND hEdit_;
    HWND hStatus_;
    std::wstring className_;
};
