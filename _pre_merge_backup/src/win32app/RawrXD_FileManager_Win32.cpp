// ============================================================================
// RawrXD_FileManager_Win32.cpp — Win32 file management for the IDE
// ============================================================================
// Implements: File open/save dialogs, recent files, file watching, directory
//             browsing. All pure Win32 API — no Qt, no external deps.
// ============================================================================

#include "Win32IDE.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <string>
#include <fstream>
#include <windows.h>
#include <vector>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")

// ============================================================================
// File Dialog Helpers
// ============================================================================

static const char* kFileFilter =
    "All Files (*.*)\0*.*\0"
    "C/C++ Files (*.c;*.cpp;*.h;*.hpp)\0*.c;*.cpp;*.h;*.hpp\0"
    "Assembly Files (*.asm;*.inc)\0*.asm;*.inc\0"
    "Python Files (*.py)\0*.py\0"
    "JavaScript/TypeScript (*.js;*.ts;*.jsx;*.tsx)\0*.js;*.ts;*.jsx;*.tsx\0"
    "Rust Files (*.rs)\0*.rs\0"
    "JSON Files (*.json)\0*.json\0"
    "Markdown Files (*.md)\0*.md\0"
    "PowerShell Scripts (*.ps1;*.psm1)\0*.ps1;*.psm1\0"
    "GGUF Models (*.gguf)\0*.gguf\0"
    "\0";

std::string Win32IDE::getFileDialogPath(bool isSave)
{
    logFunction(__FUNCTION__);

    char szFile[MAX_PATH] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndMain;
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = kFileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle  = isSave ? "Save File As" : "Open File";

    if (!m_currentDirectory.empty()) {
        ofn.lpstrInitialDir = m_currentDirectory.c_str();
    }

    BOOL result = FALSE;
    if (isSave) {
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        result = GetSaveFileNameA(&ofn);
    } else {
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        result = GetOpenFileNameA(&ofn);
    }

    if (result) {
        logFileOperation(isSave ? "SaveDialog" : "OpenDialog", szFile, true);
        return std::string(szFile);
    }

    DWORD err = CommDlgExtendedError();
    if (err != 0) {
        logError(__FUNCTION__, "CommDlg error code: " + std::to_string(err));
    }
    return "";
}

// ============================================================================
// New / Open / Close
// ============================================================================

void Win32IDE::newFile()
{
    logFunction(__FUNCTION__);

    if (m_fileModified && !promptSaveChanges()) {
        return; // User cancelled
    }

    // Clear editor content
    if (m_hwndEditor) {
        SetWindowTextA(m_hwndEditor, "");
    }

    m_currentFile.clear();
    m_fileModified = false;
    updateTitleBarText();
    logFileOperation("NewFile", "", true);
}

void Win32IDE::openFile()
{
    logFunction(__FUNCTION__);
    openFileDialog();
}

void Win32IDE::openFileDialog()
{
    logFunction(__FUNCTION__);

    if (m_fileModified && !promptSaveChanges()) {
        return;
    }

    std::string path = getFileDialogPath(false);
    if (!path.empty()) {
        openFile(path);
    }
}

void Win32IDE::openFile(const std::string& filePath)
{
    logFunction(__FUNCTION__);

    // Read file content
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        logError(__FUNCTION__, "Failed to open file: " + filePath);
        std::string errMsg = "Failed to open file:\n" + filePath;
        MessageBoxA(m_hwndMain, errMsg.c_str(), "RawrXD - Open Error", MB_OK | MB_ICONERROR);
        return;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        logError(__FUNCTION__, "Failed to get file size: " + filePath);
        return;
    }

    // Guard against absurdly large files (>256 MB)
    if (fileSize.QuadPart > 256 * 1024 * 1024) {
        CloseHandle(hFile);
        MessageBoxA(m_hwndMain, "File is too large to open in the editor (>256 MB).",
                     "RawrXD", MB_OK | MB_ICONWARNING);
        return;
    }

    std::string content;
    content.resize(static_cast<size_t>(fileSize.QuadPart));

    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hFile, content.data(), static_cast<DWORD>(fileSize.QuadPart),
                            &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!readOk) {
        logError(__FUNCTION__, "ReadFile failed for: " + filePath);
        return;
    }

    content.resize(bytesRead);

    // Set editor content
    if (m_hwndEditor) {
        SetWindowTextA(m_hwndEditor, content.c_str());
    }

    m_currentFile = filePath;
    m_fileModified = false;
    setCurrentDirectoryFromFile(filePath);
    updateRecentFiles(filePath);
    updateTitleBarText();

    logFileOperation("OpenFile", filePath, true);
    logInfo("Opened: " + filePath + " (" + std::to_string(bytesRead) + " bytes)");
}

void Win32IDE::openRecentFile(int index)
{
    logFunction(__FUNCTION__);

    if (index < 0 || index >= static_cast<int>(m_recentFiles.size())) {
        logWarning(__FUNCTION__, "Invalid recent file index: " + std::to_string(index));
        return;
    }

    std::string path = m_recentFiles[index];

    // Verify file still exists
    DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        std::string msg = "File no longer exists:\n" + path + "\n\nRemove from recent files?";
        if (MessageBoxA(m_hwndMain, msg.c_str(), "RawrXD", MB_YESNO | MB_ICONQUESTION) == IDYES) {
            m_recentFiles.erase(m_recentFiles.begin() + index);
            saveRecentFiles();
        }
        return;
    }

    openFile(path);
}

void Win32IDE::closeFile()
{
    logFunction(__FUNCTION__);

    if (m_fileModified && !promptSaveChanges()) {
        return;
    }

    if (m_hwndEditor) {
        SetWindowTextA(m_hwndEditor, "");
    }

    m_currentFile.clear();
    m_fileModified = false;
    updateTitleBarText();
}

// ============================================================================
// Save
// ============================================================================

bool Win32IDE::saveFile()
{
    logFunction(__FUNCTION__);

    if (m_currentFile.empty()) {
        return saveFileAs();
    }

    // Get editor content
    int len = GetWindowTextLengthA(m_hwndEditor);
    std::string content(len + 1, '\0');
    GetWindowTextA(m_hwndEditor, content.data(), len + 1);
    content.resize(len);

    // Write to file
    HANDLE hFile = CreateFileA(m_currentFile.c_str(), GENERIC_WRITE, 0,
                                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        logError(__FUNCTION__, "Failed to create file for writing: " + m_currentFile);
        MessageBoxA(m_hwndMain, "Failed to save file.", "RawrXD - Save Error",
                     MB_OK | MB_ICONERROR);
        return false;
    }

    DWORD bytesWritten = 0;
    BOOL writeOk = WriteFile(hFile, content.c_str(), static_cast<DWORD>(content.size()),
                              &bytesWritten, nullptr);
    CloseHandle(hFile);

    if (!writeOk || bytesWritten != static_cast<DWORD>(content.size())) {
        logError(__FUNCTION__, "WriteFile incomplete for: " + m_currentFile);
        return false;
    }

    m_fileModified = false;
    updateTitleBarText();
    logFileOperation("SaveFile", m_currentFile, true);

    return true;
}

bool Win32IDE::saveFileAs()
{
    logFunction(__FUNCTION__);

    std::string path = getFileDialogPath(true);
    if (path.empty()) return false;

    m_currentFile = path;
    setCurrentDirectoryFromFile(path);
    updateRecentFiles(path);

    return saveFile();
}

void Win32IDE::saveAll()
{
    logFunction(__FUNCTION__);
    // In single-file mode, just save the current file
    if (!m_currentFile.empty() && m_fileModified) {
        saveFile();
    }
}

bool Win32IDE::promptSaveChanges()
{
    logFunction(__FUNCTION__);

    std::string title = m_currentFile.empty() ? "Untitled" : extractLeafName(m_currentFile);
    std::string msg = "Do you want to save changes to " + title + "?";

    int result = MessageBoxA(m_hwndMain, msg.c_str(), "RawrXD - Unsaved Changes",
                              MB_YESNOCANCEL | MB_ICONQUESTION);

    switch (result) {
        case IDYES:    return saveFile();
        case IDNO:     return true;  // Discard changes, proceed
        case IDCANCEL: return false; // Cancel the operation
    }
    return false;
}

// ============================================================================
// Recent Files
// ============================================================================

void Win32IDE::updateRecentFiles(const std::string& filePath)
{
    logFunction(__FUNCTION__);

    // Remove if already in list
    auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), filePath);
    if (it != m_recentFiles.end()) {
        m_recentFiles.erase(it);
    }

    // Add to front
    m_recentFiles.insert(m_recentFiles.begin(), filePath);

    // Trim to max
    if (m_recentFiles.size() > MAX_RECENT_FILES) {
        m_recentFiles.resize(MAX_RECENT_FILES);
    }

    saveRecentFiles();
}

void Win32IDE::loadRecentFiles()
{
    logFunction(__FUNCTION__);

    m_recentFiles.clear();

    // Load from %APPDATA%/RawrXD/recent_files.txt
    char appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        std::string recentPath = std::string(appData) + "\\RawrXD\\recent_files.txt";
        std::ifstream ifs(recentPath);
        if (ifs.is_open()) {
            std::string line;
            while (std::getline(ifs, line)) {
                if (!line.empty() && m_recentFiles.size() < MAX_RECENT_FILES) {
                    m_recentFiles.push_back(line);
                }
            }
        }
    }

    logInfo("Loaded " + std::to_string(m_recentFiles.size()) + " recent files");
}

void Win32IDE::saveRecentFiles()
{
    logFunction(__FUNCTION__);

    char appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        std::string dirPath = std::string(appData) + "\\RawrXD";
        CreateDirectoryA(dirPath.c_str(), nullptr);

        std::string recentPath = dirPath + "\\recent_files.txt";
        std::ofstream ofs(recentPath, std::ios::trunc);
        if (ofs.is_open()) {
            for (const auto& f : m_recentFiles) {
                ofs << f << "\n";
            }
        }
    }
}

void Win32IDE::clearRecentFiles()
{
    logFunction(__FUNCTION__);
    m_recentFiles.clear();
    saveRecentFiles();
}

// ============================================================================
// Source File Picker
// ============================================================================

void Win32IDE::showSourceFilePicker()
{
    logFunction(__FUNCTION__);

    // Build list of source files in the workspace
    if (m_explorerRootPath.empty()) {
        MessageBoxA(m_hwndMain, "No workspace folder is open.",
                     "RawrXD - Source File Picker", MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Walk directory for source files
    m_sourceFileDisplayPaths.clear();
    m_sourceFileAbsolutePaths.clear();

    try {
        namespace fs = std::filesystem;
        static const std::vector<std::string> exts = {
            ".c", ".cpp", ".h", ".hpp", ".asm", ".inc",
            ".py", ".js", ".ts", ".rs", ".json", ".md", ".ps1"
        };

        for (const auto& entry : fs::recursive_directory_iterator(
                 m_explorerRootPath, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            bool match = false;
            for (const auto& e : exts) {
                if (ext == e) { match = true; break; }
            }
            if (!match) continue;

            // Compute relative display path
            std::string abs = entry.path().string();
            std::string rel = abs;
            if (abs.find(m_explorerRootPath) == 0) {
                rel = abs.substr(m_explorerRootPath.size());
                if (!rel.empty() && (rel[0] == '\\' || rel[0] == '/')) rel = rel.substr(1);
            }

            m_sourceFileDisplayPaths.push_back(rel);
            m_sourceFileAbsolutePaths.push_back(abs);

            // Cap at 5000 files to avoid UI overload
            if (m_sourceFileAbsolutePaths.size() >= 5000) break;
        }
    } catch (const std::exception& ex) {
        logError(__FUNCTION__, std::string("Directory scan error: ") + ex.what());
    }

    logInfo("Source file picker: found " + std::to_string(m_sourceFileAbsolutePaths.size()) + " files");
}

// ============================================================================
// Utility Helpers
// ============================================================================

std::string Win32IDE::extractLeafName(const std::string& path) const
{
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) return path.substr(pos + 1);
    return path;
}

void Win32IDE::setCurrentDirectoryFromFile(const std::string& filePath)
{
    size_t pos = filePath.find_last_of("\\/");
    if (pos != std::string::npos) {
        m_currentDirectory = filePath.substr(0, pos);
    }
}

// ============================================================================
// Format Document
// ============================================================================

void Win32IDE::cmdFormatDocument()
{
    logFunction(__FUNCTION__);

    if (!m_hwndEditor) return;

    // Get editor content
    int len = GetWindowTextLengthA(m_hwndEditor);
    if (len <= 0) return;

    std::string content(len + 1, '\0');
    GetWindowTextA(m_hwndEditor, content.data(), len + 1);
    content.resize(len);

    // Basic whitespace normalization: convert tabs to spaces, trim trailing whitespace
    std::istringstream iss(content);
    std::ostringstream oss;
    std::string line;

    while (std::getline(iss, line)) {
        // Convert tabs to 4 spaces
        std::string expanded;
        expanded.reserve(line.size());
        for (char c : line) {
            if (c == '\t') expanded += "    ";
            else expanded += c;
        }

        // Trim trailing whitespace
        size_t end = expanded.find_last_not_of(" \r");
        if (end != std::string::npos) expanded = expanded.substr(0, end + 1);
        else expanded.clear();

        oss << expanded << "\r\n";
    }

    std::string formatted = oss.str();

    // Only update if content actually changed
    if (formatted != content) {
        SetWindowTextA(m_hwndEditor, formatted.c_str());
        m_fileModified = true;
        updateTitleBarText();
        logInfo("Document formatted");
    }
}


