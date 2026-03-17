#pragma once

#include "native_ide.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace NativeIDE {

/**
 * Git repository status information
 */
struct GitStatus {
    std::wstring branch;
    int aheadBy = 0;
    int behindBy = 0;
    std::vector<std::wstring> modifiedFiles;
    std::vector<std::wstring> stagedFiles;
    std::vector<std::wstring> untrackedFiles;
    std::vector<std::wstring> conflictedFiles;
    bool isClean = true;
    bool hasRemote = false;
};

/**
 * Git commit information
 */
struct GitCommit {
    std::wstring hash;
    std::wstring shortHash;
    std::wstring author;
    std::wstring email;
    std::wstring date;
    std::wstring message;
    std::vector<std::wstring> files;
};

/**
 * Git branch information
 */
struct GitBranch {
    std::wstring name;
    bool isCurrent = false;
    bool isRemote = false;
    std::wstring tracking;
    int aheadBy = 0;
    int behindBy = 0;
};

/**
 * Git remote repository information
 */
struct GitRemote {
    std::wstring name;
    std::wstring url;
    bool canFetch = false;
    bool canPush = false;
};

/**
 * Git clone progress callback
 */
using GitProgressCallback = std::function<void(int percent, const std::wstring& message)>;

/**
 * Manages Git integration for version control operations
 */
class GitIntegration {
public:
    GitIntegration() = default;
    ~GitIntegration() = default;
    
    // Repository management
    bool InitRepository(const std::wstring& path);
    bool CloneRepository(const std::wstring& url, const std::wstring& localPath, 
                        GitProgressCallback progressCallback = nullptr);
    bool OpenRepository(const std::wstring& path);
    bool IsGitRepository(const std::wstring& path);
    void CloseRepository();
    
    // Repository information
    bool HasOpenRepository() const { return !repositoryPath_.empty(); }
    const std::wstring& GetRepositoryPath() const { return repositoryPath_; }
    GitStatus GetStatus();
    std::wstring GetCurrentBranch();
    std::vector<GitBranch> GetBranches(bool includeRemote = true);
    std::vector<GitRemote> GetRemotes();
    
    // File operations
    bool AddFile(const std::wstring& filePath);
    bool AddAllFiles();
    bool RemoveFile(const std::wstring& filePath);
    bool ResetFile(const std::wstring& filePath);
    bool ResetAllFiles();
    
    // Staging operations
    bool StageFile(const std::wstring& filePath);
    bool UnstageFile(const std::wstring& filePath);
    bool StageAllFiles();
    bool UnstageAllFiles();
    
    // Commit operations
    bool Commit(const std::wstring& message, const std::wstring& author = L"");
    bool AmendCommit(const std::wstring& message = L"");
    std::vector<GitCommit> GetCommitHistory(int maxCount = 50);
    GitCommit GetCommitDetails(const std::wstring& commitHash);
    
    // Branch operations
    bool CreateBranch(const std::wstring& branchName, const std::wstring& baseBranch = L"");
    bool CheckoutBranch(const std::wstring& branchName);
    bool MergeBranch(const std::wstring& branchName);
    bool DeleteBranch(const std::wstring& branchName, bool force = false);
    bool RenameBranch(const std::wstring& oldName, const std::wstring& newName);
    
    // Remote operations
    bool AddRemote(const std::wstring& name, const std::wstring& url);
    bool RemoveRemote(const std::wstring& name);
    bool Fetch(const std::wstring& remote = L"origin");
    bool Pull(const std::wstring& remote = L"origin", const std::wstring& branch = L"");
    bool Push(const std::wstring& remote = L"origin", const std::wstring& branch = L"");
    bool PushNewBranch(const std::wstring& remote, const std::wstring& branch);
    
    // Diff operations
    std::wstring GetFileDiff(const std::wstring& filePath);
    std::wstring GetStagedDiff();
    std::wstring GetCommitDiff(const std::wstring& commitHash);
    std::wstring GetBranchDiff(const std::wstring& branch1, const std::wstring& branch2);
    
    // Stash operations
    bool Stash(const std::wstring& message = L"");
    bool StashPop();
    bool StashApply(int stashIndex = 0);
    bool StashDrop(int stashIndex = 0);
    std::vector<std::wstring> GetStashList();
    
    // Tag operations
    bool CreateTag(const std::wstring& tagName, const std::wstring& message = L"");
    bool DeleteTag(const std::wstring& tagName);
    std::vector<std::wstring> GetTags();
    
    // Configuration
    bool SetConfig(const std::wstring& key, const std::wstring& value, bool global = false);
    std::wstring GetConfig(const std::wstring& key, bool global = false);
    bool SetUserInfo(const std::wstring& name, const std::wstring& email, bool global = false);
    
    // Utility functions
    std::wstring GetGitVersion();
    bool IsGitInstalled();
    std::wstring GetLastError() const { return lastError_; }
    
    // Event callbacks
    using StatusChangeCallback = std::function<void(const GitStatus&)>;
    void SetOnStatusChanged(StatusChangeCallback callback) { onStatusChanged_ = callback; }

private:
    std::wstring repositoryPath_;
    std::wstring lastError_;
    StatusChangeCallback onStatusChanged_;
    
    // Internal command execution
    bool ExecuteGitCommand(const std::wstring& command, std::wstring& output);
    bool ExecuteGitCommandInPath(const std::wstring& path, const std::wstring& command, std::wstring& output);
    std::vector<std::wstring> SplitLines(const std::wstring& text);
    std::wstring EscapePath(const std::wstring& path);
    void SetLastError(const std::wstring& error) { lastError_ = error; }
    void NotifyStatusChanged();
    
    // Parsing helpers
    GitStatus ParseStatus(const std::wstring& output);
    std::vector<GitCommit> ParseCommitHistory(const std::wstring& output);
    std::vector<GitBranch> ParseBranches(const std::wstring& output);
    std::vector<GitRemote> ParseRemotes(const std::wstring& output);
    GitCommit ParseCommitDetails(const std::wstring& output);
    
    // Validation helpers
    bool ValidateRepository();
    bool ValidateBranchName(const std::wstring& name);
    bool ValidateRemoteName(const std::wstring& name);
};

/**
 * Git credentials manager for authentication
 */
class GitCredentials {
public:
    static bool PromptCredentials(const std::wstring& url, std::wstring& username, std::wstring& password);
    static bool StoreCredentials(const std::wstring& url, const std::wstring& username, const std::wstring& password);
    static bool LoadCredentials(const std::wstring& url, std::wstring& username, std::wstring& password);
    static void ClearCredentials(const std::wstring& url);
    
private:
    static std::wstring GetCredentialKey(const std::wstring& url);
};

/**
 * Git clone dialog helper
 */
class GitCloneDialog {
public:
    struct CloneInfo {
        std::wstring url;
        std::wstring localPath;
        std::wstring branch;
        bool recursive = false;
        std::wstring username;
        std::wstring password;
    };
    
    static bool ShowCloneDialog(HWND parent, CloneInfo& info);
    
private:
    static INT_PTR CALLBACK CloneDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
};

} // namespace NativeIDE