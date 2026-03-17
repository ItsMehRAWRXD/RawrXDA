// File Operations Implementation for Win32IDE
// 9 comprehensive file management features

#include "Win32IDE.h"
#include "IDELogger.h"
#include "IDEConfig.h"
#include <fstream>
#include <sstream>
#include <commdlg.h>
#include <commctrl.h>
#include <algorithm>

// ============================================================================
// FILE OPERATIONS (9 Features)
// ============================================================================

void Win32IDE::openFileDialog() {
    SCOPED_METRIC("file.open_dialog_fileops");
    METRICS.increment("file.dialog_opens");
    LOG_INFO("openFileDialog() called");
    OPENFILENAMEA ofn = {};
    char szFile[MAX_PATH] = {0};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Text Files (*.txt;*.ps1;*.cpp;*.h;*.md;*.json)\0*.txt;*.ps1;*.cpp;*.h;*.md;*.json\0"
                      "All Files (*.*)\0*.*\0"
                      "PowerShell Scripts (*.ps1)\0*.ps1\0"
                      "C++ Files (*.cpp;*.h)\0*.cpp;*.h\0"
                      "GGUF Models (*.gguf)\0*.gguf\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = m_currentDirectory.empty() ? NULL : m_currentDirectory.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING;
    
    LOG_DEBUG("Opening file dialog");
    if (GetOpenFileNameA(&ofn)) {
        std::string filePath = szFile;
        LOG_INFO("File selected: " + filePath);
        
        // Check if it's a GGUF model file FIRST - DON'T load as text, bypass size limits!
        if (filePath.find(".gguf") != std::string::npos || filePath.find(".GGUF") != std::string::npos) {
            LOG_INFO("Detected GGUF file, loading as model");
            // GGUF files can be multi-GB, they use streaming loader
            // Add safety check to prevent crashes on corrupted/invalid files
            try {
                if (loadGGUFModel(filePath)) {
                    std::string message = "✅ Model loaded: " + filePath + "\n\n" + getModelInfo();
                    appendToOutput(message, "Output", OutputSeverity::Info);
                    MessageBoxA(m_hwndMain, "Model loaded successfully! Check Output panel and Copilot Chat for agentic features.", "Model Loaded", MB_OK | MB_ICONINFORMATION);
                    LOG_INFO("GGUF model loaded successfully");
                } else {
                    LOG_ERROR("Failed to load GGUF model: " + filePath);
                    MessageBoxA(m_hwndMain, "Failed to load GGUF model. Check Output/Errors panel for details.", "Model Load Failed", MB_OK | MB_ICONERROR);
                }
            } catch (const std::exception& e) {
                std::string error = "Exception while loading GGUF: " + std::string(e.what());
                LOG_ERROR(error);
                appendToOutput(error, "Errors", OutputSeverity::Error);
                MessageBoxA(m_hwndMain, error.c_str(), "Model Load Error", MB_OK | MB_ICONERROR);
            } catch (...) {
                LOG_ERROR("Unknown exception while loading GGUF file");
                appendToOutput("Unknown exception while loading GGUF file", "Errors", OutputSeverity::Error);
                MessageBoxA(m_hwndMain, "Unknown error loading GGUF file.", "Model Load Error", MB_OK | MB_ICONERROR);
            }
            return;  // Exit early - GGUF files never load into editor
        }
        
        // For text files only: check for unsaved changes
        if (m_fileModified && !promptSaveChanges()) {
            return;
        }
        
        // Load text file with size check (10MB limit for editor display only)
        try {
            // ── Encoding-aware file loading ──────────────────────────────
            RawrXD::FileSystem::BufferState bufState;
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, szFile, -1, nullptr, 0);
            std::wstring widePath(wideLen - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, szFile, -1, widePath.data(), wideLen);

            if (m_bufferManager.OpenFile(widePath, bufState)) {
                SetWindowTextA(m_hwndEditor, bufState.content.c_str());

                m_currentFile = szFile;
                m_fileModified = false;
                m_currentEncodingInfo = bufState.encoding;
                m_currentEncoding = (bufState.encoding.encoding == RawrXD::FileSystem::TextEncoding::UTF16_LE) ? 1200 :
                                    (bufState.encoding.encoding == RawrXD::FileSystem::TextEncoding::UTF16_BE) ? 1201 :
                                    (bufState.encoding.encoding == RawrXD::FileSystem::TextEncoding::LATIN1)   ? 1252 :
                                    (bufState.encoding.encoding == RawrXD::FileSystem::TextEncoding::ASCII)    ? 20127 : 65001;

                // Update status bar with detected encoding
                m_statusBarInfo.encoding = RawrXD::FileSystem::EncodingName(bufState.encoding.encoding);
                m_statusBarInfo.eolSequence = RawrXD::FileSystem::LineEndingName(bufState.encoding.lineEnding);

                // Update active tab's encoding if we have one
                if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_editorTabs.size())) {
                    m_editorTabs[m_activeTabIndex].encodingInfo = bufState.encoding;
                    m_editorTabs[m_activeTabIndex].editSequence = 0;
                    m_editorTabs[m_activeTabIndex].savedSequence = 0;
                }

                setCurrentDirectoryFromFile(m_currentFile);
                updateTitleBarText();

                // Update recent files
                updateRecentFiles(szFile);

                // Update window title
                std::string title = "RawrXD IDE - " + m_currentFile;
                SetWindowTextA(m_hwndMain, title.c_str());

                // Update status bar
                updateEnhancedStatusBar();
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File opened successfully");

                LOG_INFO("File opened with encoding: " + std::string(RawrXD::FileSystem::EncodingName(bufState.encoding.encoding))
                         + ", line ending: " + std::string(RawrXD::FileSystem::LineEndingName(bufState.encoding.lineEnding)));
            } else {
                // Fallback: raw binary read if BufferManager fails
                std::ifstream file(szFile, std::ios::binary);
                if (file) {
                    file.seekg(0, std::ios::end);
                    size_t fileSize = file.tellg();
                    file.seekg(0, std::ios::beg);

                    if (fileSize > 10 * 1024 * 1024) {
                        MessageBoxA(m_hwndMain, "Text file too large to open in editor (>10MB).", "File Too Large", MB_OK | MB_ICONWARNING);
                        return;
                    }

                    std::string content((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
                    SetWindowTextA(m_hwndEditor, content.c_str());

                    m_currentFile = szFile;
                    m_fileModified = false;
                    setCurrentDirectoryFromFile(m_currentFile);
                    updateTitleBarText();
                    updateRecentFiles(szFile);

                    std::string title = "RawrXD IDE - " + m_currentFile;
                    SetWindowTextA(m_hwndMain, title.c_str());
                    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File opened successfully");
                    file.close();
                } else {
                    MessageBoxA(m_hwndMain, "Failed to open file", "Error", MB_OK | MB_ICONERROR);
                }
            }
        }
        catch (const std::exception& e) {
            std::string error = "Error opening file: " + std::string(e.what());
            MessageBoxA(m_hwndMain, error.c_str(), "Error", MB_OK | MB_ICONERROR);
        }
    }
}

void Win32IDE::openRecentFile(int index) {
    if (index >= 0 && index < m_recentFiles.size()) {
        const std::string& filePath = m_recentFiles[index];
        
        // Check for unsaved changes
        if (m_fileModified && !promptSaveChanges()) {
            return;
        }
        
        // Load the file
        std::ifstream file(filePath, std::ios::binary);
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)), 
                              std::istreambuf_iterator<char>());
            SetWindowTextA(m_hwndEditor, content.c_str());
            
            m_currentFile = filePath;
            m_fileModified = false;
            setCurrentDirectoryFromFile(m_currentFile);
            updateTitleBarText();
            
            // Move to top of recent files
            updateRecentFiles(filePath);
            
            // Update window title
            std::string title = "RawrXD IDE - " + m_currentFile;
            SetWindowTextA(m_hwndMain, title.c_str());
            
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Recent file opened");
            file.close();
        } else {
            MessageBoxA(m_hwndMain, 
                       ("File not found: " + filePath).c_str(), 
                       "Error", MB_OK | MB_ICONERROR);
            
            // Remove from recent files
            m_recentFiles.erase(m_recentFiles.begin() + index);
            saveRecentFiles();
        }
    }
}

std::string Win32IDE::getFileDialogPath(bool isSave) {
    OPENFILENAMEA ofn = {};
    char szFile[MAX_PATH] = {0};
    
    if (!m_currentFile.empty()) {
        strncpy(szFile, m_currentFile.c_str(), MAX_PATH - 1);
    }
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "All Files (*.*)\0*.*\0"
                      "PowerShell Scripts (*.ps1)\0*.ps1\0"
                      "C++ Files (*.cpp;*.h)\0*.cpp;*.h\0"
                      "Text Files (*.txt)\0*.txt\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = m_currentDirectory.empty() ? NULL : m_currentDirectory.c_str();
    ofn.lpstrDefExt = m_defaultFileExtension.c_str();
    
    if (isSave) {
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ENABLESIZING;
        if (GetSaveFileNameA(&ofn)) {
            return std::string(szFile);
        }
    } else {
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING;
        if (GetOpenFileNameA(&ofn)) {
            return std::string(szFile);
        }
    }
    
    return "";
}

void Win32IDE::saveAll() {
    if (!m_currentFile.empty() && m_fileModified) {
        saveFile();
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"All files saved");
    }
}

void Win32IDE::closeFile() {
    if (m_fileModified && !promptSaveChanges()) {
        return;
    }
    
    // Clear editor
    SetWindowTextA(m_hwndEditor, "");
    
    // Reset state
    m_currentFile.clear();
    m_fileModified = false;
    updateTitleBarText();
    
    // Update window title
    SetWindowTextA(m_hwndMain, "RawrXD IDE - New File");
    
    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File closed");
}

bool Win32IDE::promptSaveChanges() {
    int result = MessageBoxA(m_hwndMain, 
                            "The current file has unsaved changes.\n\nDo you want to save them?",
                            "Unsaved Changes", 
                            MB_YESNOCANCEL | MB_ICONWARNING);
    
    if (result == IDCANCEL) {
        return false;
    }
    
    if (result == IDYES) {
        return saveFile();
    }
    
    return true; // IDNO - discard changes
}

void Win32IDE::updateRecentFiles(const std::string& filePath) {
    // Remove if already exists
    auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), filePath);
    if (it != m_recentFiles.end()) {
        m_recentFiles.erase(it);
    }
    
    // Add to front
    m_recentFiles.insert(m_recentFiles.begin(), filePath);
    
    // Limit size
    if (m_recentFiles.size() > MAX_RECENT_FILES) {
        m_recentFiles.resize(MAX_RECENT_FILES);
    }
    
    // Save to disk
    saveRecentFiles();
}

void Win32IDE::loadRecentFiles() {
    m_recentFiles.clear();
    
    std::ifstream file("recent_files.ini");
    if (file) {
        std::string line;
        while (std::getline(file, line) && m_recentFiles.size() < MAX_RECENT_FILES) {
            if (!line.empty()) {
                m_recentFiles.push_back(line);
            }
        }
        file.close();
    }
}

void Win32IDE::saveRecentFiles() {
    std::ofstream file("recent_files.ini");
    if (file) {
        for (const auto& path : m_recentFiles) {
            file << path << "\n";
        }
        file.close();
    }
}

void Win32IDE::clearRecentFiles() {
    m_recentFiles.clear();
    saveRecentFiles();
    MessageBoxA(m_hwndMain, "Recent files list cleared", "Recent Files", MB_OK | MB_ICONINFORMATION);
}

// Note: openFile() is defined in Win32IDE_clean.cpp and just calls openFileDialog()
