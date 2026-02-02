#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include <functional>
#include "ide_orchestrator.h"
#include "monaco_integration.h"
#include "utils/Expected.h"

namespace RawrXD {

class GUIMain {
public:
    GUIMain();
    ~GUIMain();

    RawrXD::Expected<void, std::string> initialize(HINSTANCE hInstance);
    RawrXD::Expected<void, std::string> run();
    void shutdown();

    HWND getMainWindow() const { return m_mainWindow; }
    HWND getEditorWindow() const { return m_editorWindow; }
    HMENU getMainMenu() const { return m_mainMenu; }

private:
    RawrXD::Expected<void, std::string> registerWindowClass();
    RawrXD::Expected<void, std::string> createMainWindow();
    RawrXD::Expected<void, std::string> createEditorWindow();
    RawrXD::Expected<void, std::string> setupLayout();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessageInternal(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onFileNewInternal();
    void onFileOpenInternal();
    void onFileSaveInternal();
    void onEditUndoInternal();
    void onEditRedoInternal();
    void onEditCutInternal();
    void onEditCopyInternal();
    void onEditPasteInternal();
    void onBuildInternal();
    void onRunInternal();
    void onDebugInternal();

    HWND m_mainWindow{NULL};
    HWND m_editorWindow{NULL};
    HMENU m_mainMenu{NULL};
};

} // namespace RawrXD
