#pragma once
#include <windows.h>

class MainWindow {
public:
    MainWindow(HINSTANCE hInst);
    ~MainWindow();

    bool create(const wchar_t* title, int width, int height);
    HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    HINSTANCE hInst_;
    HWND hwnd_;
};