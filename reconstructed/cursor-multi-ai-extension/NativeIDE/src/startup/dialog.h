#pragma once

#include "native_ide.h"
#include <string>

namespace NativeIDE {

/**
 * Startup option types matching the requirements
 */
enum class StartupOption {
    OpenProject,        // Tab to open a project or solution
    CloneRepository,    // Clone a repository  
    OpenLocalFolder,    // Open Local Folder
    CreateNewProject,   // Create new project
    ContinueWithoutCode // Continue without code
};

/**
 * Result of the startup dialog
 */
struct StartupResult {
    StartupOption option = StartupOption::ContinueWithoutCode;
    std::wstring projectPath;
    std::wstring repositoryUrl;
    std::wstring templateName;
    std::wstring projectName;
    bool success = false;
};

/**
 * Shows the startup dialog with 5 options
 */
class StartupDialog {
public:
    static StartupResult Show(HWND parent = nullptr);
    
private:
    // Dialog procedure
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    
    // Button handlers
    static void OnOpenProject(HWND hDlg, StartupResult& result);
    static void OnCloneRepository(HWND hDlg, StartupResult& result);
    static void OnOpenLocalFolder(HWND hDlg, StartupResult& result);
    static void OnCreateNewProject(HWND hDlg, StartupResult& result);
    static void OnContinueWithoutCode(HWND hDlg, StartupResult& result);
    
    // Utility functions
    static std::wstring BrowseForFolder(HWND parent, const std::wstring& title);
    static std::wstring BrowseForProjectFile(HWND parent);
    static bool ValidateRepositoryUrl(const std::wstring& url);
    
    static StartupResult* s_result;
};

/**
 * Clone repository dialog
 */
class CloneRepositoryDialog {
public:
    struct CloneInfo {
        std::wstring url;
        std::wstring localPath;
        std::wstring branch;
        bool recursive = false;
    };
    
    static bool Show(HWND parent, CloneInfo& info);
    
private:
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static CloneInfo* s_cloneInfo;
};

/**
 * New project dialog
 */
class NewProjectDialog {
public:
    struct ProjectInfo {
        std::wstring templateName;
        std::wstring projectName;
        std::wstring location;
        std::wstring fullPath;
    };
    
    static bool Show(HWND parent, ProjectInfo& info);
    
private:
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static void PopulateTemplates(HWND hCombo);
    static void UpdateProjectPath(HWND hDlg);
    static ProjectInfo* s_projectInfo;
};

} // namespace NativeIDE