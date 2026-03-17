#pragma once
// RawrXD_MainWindow.h
// Pure Win32 Main Window replacement for QMainWindow

#ifndef RAWRXD_MAINWINDOW_H
#define RAWRXD_MAINWINDOW_H

#include "../RawrXD_Foundation.h"
#include <windows.h>
#include <memory>

class AIIntegrationHub; // Forward decl

namespace RawrXD {

class MainWindow {
    HWND hwnd;
    
    // Child Windows
    class EditorWindow* editor;
    class Sidebar* sidebar;
    class Panel* panel;

    std::shared_ptr<AIIntegrationHub> aiHub;

    // Layout
    int sidebarWidth;
    int panelHeight;
    
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    
public:
    MainWindow();
    ~MainWindow();
    
    bool create(const String& title, int w = 1200, int h = 800);
    void show();
    void hide();
    
    HWND handle() const { return hwnd; }
    
    // Components
    void setEditor(EditorWindow* ed) { editor = ed; }
    EditorWindow* getEditor() const { return editor; }
    
    // Layout
    void updateLayout();
};

} // namespace RawrXD

#endif // RAWRXD_MAINWINDOW_H
