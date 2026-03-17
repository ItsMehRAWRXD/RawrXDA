/**
 * @file command_handlers.h
 * @brief Production-ready command handlers with REAL implementations
 * 
 * NO PLACEHOLDERS - Each handler performs actual operations:
 * - File operations use Win32 file dialogs and APIs
 * - Clipboard operations use Win32 clipboard API
 * - Shell operations use ShellExecute/ShellExecuteEx
 * - All operations log via IDELogger
 */

#pragma once

#include "command_registry.h"
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace RawrXD::Handlers {

// Forward declaration for IDE access
class Win32IDE;

// ============================================================================
// HANDLER CONTEXT - Stores IDE state needed by handlers
// ============================================================================

struct HandlerContext {
    HWND mainWindow = nullptr;
    HWND editorWindow = nullptr;
    HWND terminalWindow = nullptr;
    HWND explorerTree = nullptr;
    
    std::string currentFilePath;
    std::string currentFolderPath;
    std::string selectedPath;  // For context menu operations
    bool isSelectedFolder = false;
    
    bool hasUnsavedChanges = false;
    std::vector<std::string> recentFiles;
    std::string workspacePath;
    
    // Callbacks to IDE
    std::function<void(const std::string&)> onOpenFile;
    std::function<void(const std::string&)> onOpenFolder;
    std::function<bool()> onSaveFile;
    std::function<bool(const std::string&)> onSaveFileAs;
    std::function<void()> onNewFile;
    std::function<void()> onCloseFile;
    std::function<std::wstring()> onGetEditorContent;
    std::function<void(const std::wstring&)> onSetEditorContent;
    std::function<std::wstring()> onGetSelectedText;
    std::function<void()> onUndo;
    std::function<void()> onRedo;
    std::function<bool()> onCanUndo;
    std::function<bool()> onCanRedo;
    std::function<void()> onCut;
    std::function<void()> onCopy;
    std::function<void()> onPaste;
    std::function<void()> onSelectAll;
    std::function<void()> onShowFindDialog;
    std::function<void()> onShowReplaceDialog;
    std::function<void()> onToggleTerminal;
    std::function<void()> onToggleExplorer;
    std::function<void()> onShowCommandPalette;
    std::function<void(const std::string&)> onTerminalCommand;
    std::function<void()> onRefreshExplorer;
};

// Global handler context (set by IDE at startup)
extern HandlerContext g_handlerContext;

// ============================================================================
// FILE OPERATION HANDLERS
// ============================================================================

namespace File {

// Opens Windows file dialog and returns selected path
std::string showOpenFileDialog(HWND hwnd, const std::vector<std::pair<std::string, std::string>>& filters = {});
std::string showSaveFileDialog(HWND hwnd, const std::string& defaultName = "", 
                               const std::vector<std::pair<std::string, std::string>>& filters = {});
std::string showOpenFolderDialog(HWND hwnd);

// Actual file operations
bool readFileContent(const std::string& path, std::string& content);
bool writeFileContent(const std::string& path, const std::string& content);
bool fileExists(const std::string& path);
bool createDirectory(const std::string& path);
bool deleteFile(const std::string& path);
bool deleteFolder(const std::string& path, bool recursive = false);
bool renameFile(const std::string& oldPath, const std::string& newPath);
bool copyFile(const std::string& src, const std::string& dst, bool overwrite = false);
bool moveFile(const std::string& src, const std::string& dst);

// File properties
int64_t getFileSize(const std::string& path);
std::string getFileExtension(const std::string& path);
std::string getFileName(const std::string& path);
std::string getParentFolder(const std::string& path);

// Command handlers (registered with CommandRegistry)
void handleNewFile();
void handleNewFolder();
void handleOpenFile();
void handleOpenFolder();
void handleOpenWorkspace();
void handleSave();
void handleSaveAs();
void handleSaveAll();
void handleClose();
void handleExit();
void handlePreferences();

} // namespace File

// ============================================================================
// EDIT OPERATION HANDLERS
// ============================================================================

namespace Edit {

// Clipboard operations using Win32 API
bool copyToClipboard(HWND hwnd, const std::wstring& text);
bool copyToClipboard(HWND hwnd, const std::string& text);
std::wstring getClipboardText(HWND hwnd);
bool hasClipboardText();

// Command handlers
void handleUndo();
void handleRedo();
void handleCut();
void handleCopy();
void handlePaste();
void handleSelectAll();
void handleSelectLine();
void handleFind();
void handleReplace();
void handleFindInFiles();
void handleFormatDocument();
void handleFormatSelection();
void handleToggleComment();
void handleToggleBlockComment();

} // namespace Edit

// ============================================================================
// VIEW OPERATION HANDLERS
// ============================================================================

namespace View {

void handleCommandPalette();
void handleOpenView();
void handleToggleExplorer();
void handleToggleSearch();
void handleToggleSourceControl();
void handleToggleRunDebug();
void handleToggleExtensions();
void handleToggleOutput();
void handleToggleDebugConsole();
void handleToggleTerminal();
void handleToggleWordWrap();
void handleToggleMinimap();
void handleToggleBreadcrumbs();
void handleToggleStatusBar();
void handleZoomIn();
void handleZoomOut();
void handleResetZoom();
void handleFullscreen();

} // namespace View

// ============================================================================
// CONTEXT MENU HANDLERS - Real Shell Operations
// ============================================================================

namespace Context {

// Copy operations
bool copyPathToClipboard(const std::string& path);
bool copyRelativePathToClipboard(const std::string& path, const std::string& basePath);

// Shell operations
bool revealInExplorer(const std::string& path);
bool openInDefaultApp(const std::string& path);
bool openInTerminal(const std::string& folderPath);
bool showFileProperties(const std::string& path);

// File management
bool promptRename(HWND hwnd, const std::string& path);
bool promptDelete(HWND hwnd, const std::string& path, bool isFolder);
bool promptNewFile(HWND hwnd, const std::string& folderPath);
bool promptNewFolder(HWND hwnd, const std::string& parentPath);

// Command handlers
void handleCtxOpen();
void handleCtxOpenWith();
void handleCtxCut();
void handleCtxCopy();
void handleCtxCopyPath();
void handleCtxCopyRelPath();
void handleCtxPaste();
void handleCtxRename();
void handleCtxDelete();
void handleCtxRevealExplorer();
void handleCtxOpenTerminal();
void handleCtxNewFile();
void handleCtxNewFolder();
void handleCtxFindInFolder();
void handleCtxProperties();

} // namespace Context

// ============================================================================
// TERMINAL HANDLERS
// ============================================================================

namespace Terminal {

void handleNewTerminal();
void handleSplitTerminal();
void handleKillTerminal();
void handleClearTerminal();
void handleRunTask();
void handleBuildTask();
void handleConfigureTasks();
void handlePowerShell();
void handleCmd();
void handleGitBash();

} // namespace Terminal

// ============================================================================
// AI/CHAT HANDLERS
// ============================================================================

namespace AI {

void handleOpenChat();
void handleQuickChat();
void handleNewChatEditor();
void handleNewChatWindow();
void handleConfigureInline();
void handleManageChat();
void handleExplainCode();
void handleFixCode();
void handleGenerateTests();
void handleGenerateDocs();
void handleAgentStart();
void handleAgentStop();
void handleAgentStatus();

} // namespace AI

// ============================================================================
// HELP HANDLERS
// ============================================================================

namespace Help {

void handleWelcome();
void handleDocs();
void handleShortcuts();
void handleTips();
void handlePlayground();
void handleGitHub();
void handleFeatureRequest();
void handleReportIssue();
void handleLicense();
void handlePrivacy();
void handleCheckUpdates();
void handleAbout();

// Opens URL in default browser
bool openUrl(const std::string& url);

} // namespace Help

// ============================================================================
// INITIALIZATION
// ============================================================================

// Registers all command handlers with the CommandRegistry
void registerAllHandlers();

// Sets up the handler context (call from IDE initialization)
void initializeHandlers(const HandlerContext& ctx);

} // namespace RawrXD::Handlers
