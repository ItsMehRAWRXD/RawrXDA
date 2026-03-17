#pragma once

#ifndef NATIVE_IDE_H
#define NATIVE_IDE_H

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <regex>
#include <fstream>
#include <sstream>
#include <algorithm>

// Link required libraries
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

// Application constants
constexpr int IDE_WINDOW_MIN_WIDTH = 800;
constexpr int IDE_WINDOW_MIN_HEIGHT = 600;
constexpr int IDE_WINDOW_DEFAULT_WIDTH = 1200;
constexpr int IDE_WINDOW_DEFAULT_HEIGHT = 800;

// Menu and command IDs
#define IDM_FILE_NEW         1001
#define IDM_FILE_OPEN        1002
#define IDM_FILE_SAVE        1003
#define IDM_FILE_SAVE_AS     1004
#define IDM_FILE_EXIT        1005
#define IDM_EDIT_UNDO        1101
#define IDM_EDIT_REDO        1102
#define IDM_EDIT_CUT         1103
#define IDM_EDIT_COPY        1104
#define IDM_EDIT_PASTE       1105
#define IDM_EDIT_FIND        1106
#define IDM_EDIT_REPLACE     1107
#define IDM_BUILD_COMPILE    1201
#define IDM_BUILD_BUILD      1202
#define IDM_BUILD_RUN        1203
#define IDM_BUILD_DEBUG      1204
#define IDM_VIEW_PROJECT     1301
#define IDM_VIEW_OUTPUT      1302
#define IDM_VIEW_CONSOLE     1303
#define IDM_TOOLS_OPTIONS    1401
#define IDM_HELP_ABOUT       1501

// Control IDs
#define IDC_EDITOR           2001
#define IDC_PROJECT_TREE     2002
#define IDC_OUTPUT_WINDOW    2003
#define IDC_STATUS_BAR       2004
#define IDC_TOOLBAR          2005

// Startup dialog IDs
#define IDD_STARTUP          3001
#define IDC_OPEN_PROJECT     3002
#define IDC_CLONE_REPO       3003
#define IDC_OPEN_FOLDER      3004
#define IDC_NEW_PROJECT      3005
#define IDC_CONTINUE_EMPTY   3006

// Forward declarations
class IDEApplication;
class MainWindow;
class EditorCore;
class TextBuffer;
class SyntaxHighlighter;
class CompilerIntegration;
class PluginManager;
class ProjectManager;
class FileManager;
class GitIntegration;

// Plugin interface
struct IPlugin {
    virtual ~IPlugin() = default;
    virtual const char* GetName() const = 0;
    virtual const char* GetVersion() const = 0;
    virtual bool Initialize(IDEApplication* app) = 0;
    virtual void Shutdown() = 0;
    
    // Extension points
    virtual void OnFileOpened(const std::wstring&) {}
    virtual void OnFileSaved(const std::wstring&) {}
    virtual void OnMenuCommand(int) {}
    virtual void OnKeyPressed(int, int) {}
    virtual void OnRender(ID2D1RenderTarget*, const RECT&) {}
    virtual void OnProjectOpened(const std::wstring&) {}
};

// Compiler result structure
struct CompileResult {
    int exit_code = 0;
    std::string output;
    std::string errors;
    double compile_time = 0.0;
    std::wstring executable_path;
};

// File information structure
struct FileInfo {
    std::wstring path;
    std::wstring name;
    std::string language;
    bool is_modified = false;
    bool is_directory = false;
    size_t file_size = 0;
    FILETIME last_modified = {};
};

// Project structure
struct Project {
    std::wstring name;
    std::wstring path;
    std::vector<FileInfo> files;
    std::wstring build_command;
    std::wstring run_command;
    std::wstring debug_command;
    std::unordered_map<std::wstring, std::wstring> settings;
};

// Utility functions
namespace IDEUtils {
    std::wstring GetExecutableDirectory();
    std::wstring GetFileExtension(const std::wstring& filename);
    std::wstring GetFileName(const std::wstring& path);
    std::wstring GetDirectoryFromPath(const std::wstring& path);
    std::string WStringToString(const std::wstring& wstr);
    std::wstring StringToWString(const std::string& str);
    bool PathExists(const std::wstring& path);
    bool IsDirectory(const std::wstring& path);
    std::vector<std::wstring> SplitString(const std::wstring& str, wchar_t delimiter);
    std::wstring JoinPath(const std::wstring& path1, const std::wstring& path2);
    std::string DetectLanguageFromExtension(const std::wstring& extension);
    COLORREF ColorFromHex(uint32_t hex);
}

#endif // NATIVE_IDE_H