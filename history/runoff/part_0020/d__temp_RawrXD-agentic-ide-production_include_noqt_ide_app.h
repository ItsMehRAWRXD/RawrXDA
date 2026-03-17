#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>
#include "win32_ui.h"

class NoQtIDEApp {
public:
    NoQtIDEApp() = default;

    bool create(HINSTANCE hInstance);
    int run();

private:
    static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void createMenus();
    void createToolbar();
    void createCommandPalette();
    void createChildWindows();
    void layout(int clientW, int clientH);

    void onFileOpen();
    void onFileSaveAs();
    void onExit();

    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();

    void onNewPaint();
    void onNewCode();
    void onNewChat();

    void onChatSend();
    void onTerminalRun();

    void showCommandPalette();

    HWND focusedTextWidget() const;

private:
    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;

    // Win32 UI components
    Win32MenuBar m_menuBar;
    Win32ToolBar m_toolBar;
    Win32CommandPalette m_commandPalette;

    HWND m_paintHost = nullptr;      // custom paint canvas
    HWND m_editor = nullptr;         // RichEdit
    HWND m_chatTranscript = nullptr; // RichEdit read-only
    HWND m_chatInput = nullptr;      // RichEdit single-line-ish
    HWND m_chatSendBtn = nullptr;

    HWND m_termOutput = nullptr;     // RichEdit read-only
    HWND m_termInput = nullptr;      // RichEdit
    HWND m_termRunBtn = nullptr;

    std::wstring m_currentFilePath;
};
