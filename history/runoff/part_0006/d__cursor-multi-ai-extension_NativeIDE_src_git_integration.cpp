#include "git_integration.h"
#include <sstream>
#include <regex>
#include <fstream>

namespace NativeIDE {

// GitIntegration Implementation
bool GitIntegration::InitRepository(const std::wstring& path) {
    std::wstring output;
    if (!ExecuteGitCommandInPath(path, L"init", output)) {
        SetLastError(L"Failed to initialize Git repository: " + output);
        return false;
    }
    
    repositoryPath_ = path;
    NotifyStatusChanged();
    return true;
}

bool GitIntegration::CloneRepository(const std::wstring& url, const std::wstring& localPath, 
                                   GitProgressCallback progressCallback) {
    std::wstring command = L"clone \"" + url + L"\" \"" + localPath + L"\"";
    std::wstring output;
    
    if (progressCallback) {
        progressCallback(0, L"Starting clone...");
    }
    
    if (!ExecuteGitCommand(command, output)) {
        SetLastError(L"Failed to clone repository: " + output);
        return false;
    }
    
    if (progressCallback) {
        progressCallback(100, L"Clone completed");
    }
    
    return OpenRepository(localPath);
}

bool GitIntegration::OpenRepository(const std::wstring& path) {
    if (!IsGitRepository(path)) {
        SetLastError(L"Not a valid Git repository");
        return false;
    }
    
    repositoryPath_ = path;
    NotifyStatusChanged();
    return true;
}

bool GitIntegration::IsGitRepository(const std::wstring& path) {
    std::filesystem::path gitDir = std::filesystem::path(path) / L".git";
    return std::filesystem::exists(gitDir);
}

void GitIntegration::CloseRepository() {
    repositoryPath_.clear();
}

GitStatus GitIntegration::GetStatus() {
    GitStatus status;
    if (!ValidateRepository()) {
        return status;
    }
    
    std::wstring output;
    if (ExecuteGitCommand(L"status --porcelain -b", output)) {
        status = ParseStatus(output);
    }
    
    return status;
}

std::wstring GitIntegration::GetCurrentBranch() {
    if (!ValidateRepository()) {
        return L"";
    }
    
    std::wstring output;
    if (ExecuteGitCommand(L"rev-parse --abbrev-ref HEAD", output)) {
        // Trim whitespace
        output.erase(0, output.find_first_not_of(L" \t\r\n"));
        output.erase(output.find_last_not_of(L" \t\r\n") + 1);
        return output;
    }
    
    return L"";
}

std::vector<GitBranch> GitIntegration::GetBranches(bool includeRemote) {
    std::vector<GitBranch> branches;
    if (!ValidateRepository()) {
        return branches;
    }
    
    std::wstring command = includeRemote ? L"branch -a -v" : L"branch -v";
    std::wstring output;
    
    if (ExecuteGitCommand(command, output)) {
        branches = ParseBranches(output);
    }
    
    return branches;
}

bool GitIntegration::AddFile(const std::wstring& filePath) {
    if (!ValidateRepository()) {
        return false;
    }
    
    std::wstring command = L"add \"" + EscapePath(filePath) + L"\"";
    std::wstring output;
    
    if (ExecuteGitCommand(command, output)) {
        NotifyStatusChanged();
        return true;
    }
    
    SetLastError(L"Failed to add file: " + output);
    return false;
}

bool GitIntegration::AddAllFiles() {
    if (!ValidateRepository()) {
        return false;
    }
    
    std::wstring output;
    if (ExecuteGitCommand(L"add .", output)) {
        NotifyStatusChanged();
        return true;
    }
    
    SetLastError(L"Failed to add all files: " + output);
    return false;
}

bool GitIntegration::Commit(const std::wstring& message, const std::wstring& author) {
    if (!ValidateRepository()) {
        return false;
    }
    
    std::wstring command = L"commit -m \"" + message + L"\"";
    if (!author.empty()) {
        command += L" --author=\"" + author + L"\"";
    }
    
    std::wstring output;
    if (ExecuteGitCommand(command, output)) {
        NotifyStatusChanged();
        return true;
    }
    
    SetLastError(L"Failed to commit: " + output);
    return false;
}

std::vector<GitCommit> GitIntegration::GetCommitHistory(int maxCount) {
    std::vector<GitCommit> commits;
    if (!ValidateRepository()) {
        return commits;
    }
    
    std::wstring command = L"log --oneline -" + std::to_wstring(maxCount) + 
                          L" --pretty=format:\"%H|%h|%an|%ae|%ad|%s\" --date=short";
    std::wstring output;
    
    if (ExecuteGitCommand(command, output)) {
        commits = ParseCommitHistory(output);
    }
    
    return commits;
}

bool GitIntegration::CreateBranch(const std::wstring& branchName, const std::wstring& baseBranch) {
    if (!ValidateRepository() || !ValidateBranchName(branchName)) {
        return false;
    }
    
    std::wstring command = L"checkout -b \"" + branchName + L"\"";
    if (!baseBranch.empty()) {
        command += L" \"" + baseBranch + L"\"";
    }
    
    std::wstring output;
    if (ExecuteGitCommand(command, output)) {
        NotifyStatusChanged();
        return true;
    }
    
    SetLastError(L"Failed to create branch: " + output);
    return false;
}

bool GitIntegration::CheckoutBranch(const std::wstring& branchName) {
    if (!ValidateRepository()) {
        return false;
    }
    
    std::wstring command = L"checkout \"" + branchName + L"\"";
    std::wstring output;
    
    if (ExecuteGitCommand(command, output)) {
        NotifyStatusChanged();
        return true;
    }
    
    SetLastError(L"Failed to checkout branch: " + output);
    return false;
}

std::wstring GitIntegration::GetFileDiff(const std::wstring& filePath) {
    if (!ValidateRepository()) {
        return L"";
    }
    
    std::wstring command = L"diff \"" + EscapePath(filePath) + L"\"";
    std::wstring output;
    
    ExecuteGitCommand(command, output);
    return output;
}

bool GitIntegration::IsGitInstalled() {
    std::wstring output;
    return ExecuteGitCommand(L"--version", output);
}

std::wstring GitIntegration::GetGitVersion() {
    std::wstring output;
    if (ExecuteGitCommand(L"--version", output)) {
        return output;
    }
    return L"Git not installed";
}

// Private implementation methods
bool GitIntegration::ExecuteGitCommand(const std::wstring& command, std::wstring& output) {
    return ExecuteGitCommandInPath(repositoryPath_, command, output);
}

bool GitIntegration::ExecuteGitCommandInPath(const std::wstring& path, const std::wstring& command, std::wstring& output) {
    std::wstring fullCommand = L"git " + command;
    
    // Create pipe for reading output
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        SetLastError(L"Failed to create pipe");
        return false;
    }
    
    // Setup process information
    PROCESS_INFORMATION pi = {};
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;
    
    // Set working directory
    const wchar_t* workingDir = path.empty() ? NULL : path.c_str();
    
    // Create process
    BOOL success = CreateProcessW(
        NULL,
        const_cast<wchar_t*>(fullCommand.c_str()),
        NULL, NULL, TRUE, 0, NULL,
        workingDir,
        &si, &pi
    );
    
    CloseHandle(hWrite);
    
    if (!success) {
        CloseHandle(hRead);
        SetLastError(L"Failed to execute Git command");
        return false;
    }
    
    // Read output
    std::string result;
    char buffer[4096];
    DWORD bytesRead;
    
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }
    
    // Wait for process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(hRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Convert output to wide string
    output = std::wstring(result.begin(), result.end());
    
    return exitCode == 0;
}

std::vector<std::wstring> GitIntegration::SplitLines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstringstream ss(text);
    std::wstring line;
    
    while (std::getline(ss, line)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == L'\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    
    return lines;
}

std::wstring GitIntegration::EscapePath(const std::wstring& path) {
    std::wstring escaped = path;
    // Replace backslashes with forward slashes for Git
    std::replace(escaped.begin(), escaped.end(), L'\\', L'/');
    return escaped;
}

void GitIntegration::NotifyStatusChanged() {
    if (onStatusChanged_) {
        GitStatus status = GetStatus();
        onStatusChanged_(status);
    }
}

GitStatus GitIntegration::ParseStatus(const std::wstring& output) {
    GitStatus status;
    auto lines = SplitLines(output);
    
    for (const auto& line : lines) {
        if (line.empty()) continue;
        
        if (line.substr(0, 2) == L"##") {
            // Branch information
            std::wregex branchRegex(LR"(## (.+?)(?:\.\.\.|$))");
            std::wsmatch match;
            if (std::regex_search(line, match, branchRegex)) {
                status.branch = match[1].str();
            }
            
            // Check for ahead/behind
            std::wregex aheadBehindRegex(LR"(\[ahead (\d+)(?:, behind (\d+))?\])");
            if (std::regex_search(line, match, aheadBehindRegex)) {
                status.aheadBy = std::stoi(match[1].str());
                if (match[2].matched) {
                    status.behindBy = std::stoi(match[2].str());
                }
            }
        } else {
            // File status
            if (line.length() >= 3) {
                wchar_t indexStatus = line[0];
                wchar_t workingStatus = line[1];
                std::wstring fileName = line.substr(3);
                
                if (indexStatus == L'A' || indexStatus == L'M' || indexStatus == L'D') {
                    status.stagedFiles.push_back(fileName);
                }
                
                if (workingStatus == L'M' || workingStatus == L'D') {
                    status.modifiedFiles.push_back(fileName);
                } else if (workingStatus == L'?') {
                    status.untrackedFiles.push_back(fileName);
                }
                
                if (indexStatus == L'U' || workingStatus == L'U') {
                    status.conflictedFiles.push_back(fileName);
                }
            }
        }
    }
    
    status.isClean = status.modifiedFiles.empty() && status.stagedFiles.empty() && 
                    status.untrackedFiles.empty() && status.conflictedFiles.empty();
    
    return status;
}

std::vector<GitCommit> GitIntegration::ParseCommitHistory(const std::wstring& output) {
    std::vector<GitCommit> commits;
    auto lines = SplitLines(output);
    
    for (const auto& line : lines) {
        if (line.empty()) continue;
        
        // Split by pipe character
        std::vector<std::wstring> parts;
        std::wstringstream ss(line);
        std::wstring part;
        
        while (std::getline(ss, part, L'|')) {
            parts.push_back(part);
        }
        
        if (parts.size() >= 6) {
            GitCommit commit;
            commit.hash = parts[0];
            commit.shortHash = parts[1];
            commit.author = parts[2];
            commit.email = parts[3];
            commit.date = parts[4];
            commit.message = parts[5];
            commits.push_back(commit);
        }
    }
    
    return commits;
}

std::vector<GitBranch> GitIntegration::ParseBranches(const std::wstring& output) {
    std::vector<GitBranch> branches;
    auto lines = SplitLines(output);
    
    for (const auto& line : lines) {
        if (line.empty()) continue;
        
        GitBranch branch;
        branch.isCurrent = (line[0] == L'*');
        
        size_t nameStart = line.find_first_not_of(L"* ");
        if (nameStart != std::wstring::npos) {
            size_t nameEnd = line.find_first_of(L" ", nameStart);
            if (nameEnd == std::wstring::npos) {
                nameEnd = line.length();
            }
            
            branch.name = line.substr(nameStart, nameEnd - nameStart);
            branch.isRemote = (branch.name.find(L"remotes/") == 0);
            
            if (branch.isRemote) {
                branch.name = branch.name.substr(8); // Remove "remotes/"
            }
        }
        
        branches.push_back(branch);
    }
    
    return branches;
}

bool GitIntegration::ValidateRepository() {
    if (repositoryPath_.empty() || !IsGitRepository(repositoryPath_)) {
        SetLastError(L"No valid Git repository is open");
        return false;
    }
    return true;
}

bool GitIntegration::ValidateBranchName(const std::wstring& name) {
    if (name.empty()) {
        SetLastError(L"Branch name cannot be empty");
        return false;
    }
    
    // Basic validation - no spaces, special characters
    for (wchar_t c : name) {
        if (c == L' ' || c == L'~' || c == L'^' || c == L':' || c == L'?' || c == L'*' || c == L'[') {
            SetLastError(L"Branch name contains invalid characters");
            return false;
        }
    }
    
    return true;
}

// GitCredentials Implementation (basic registry-based storage)
bool GitCredentials::PromptCredentials(const std::wstring& url, std::wstring& username, std::wstring& password) {
    // Simple dialog - in a real implementation, this would show a proper credential dialog
    username = L"";
    password = L"";
    return true; // For now, return true to allow anonymous access
}

bool GitCredentials::StoreCredentials(const std::wstring& url, const std::wstring& username, const std::wstring& password) {
    std::wstring key = GetCredentialKey(url);
    HKEY hKey;
    
    if (RegCreateKeyExW(HKEY_CURRENT_USER, (L"SOFTWARE\\NativeIDE\\Credentials\\" + key).c_str(),
                       0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        
        RegSetValueExW(hKey, L"Username", 0, REG_SZ, 
                      (const BYTE*)username.c_str(), (username.length() + 1) * sizeof(wchar_t));
        
        // In a real implementation, password should be encrypted
        RegSetValueExW(hKey, L"Password", 0, REG_SZ,
                      (const BYTE*)password.c_str(), (password.length() + 1) * sizeof(wchar_t));
        
        RegCloseKey(hKey);
        return true;
    }
    
    return false;
}

bool GitCredentials::LoadCredentials(const std::wstring& url, std::wstring& username, std::wstring& password) {
    std::wstring key = GetCredentialKey(url);
    HKEY hKey;
    
    if (RegOpenKeyExW(HKEY_CURRENT_USER, (L"SOFTWARE\\NativeIDE\\Credentials\\" + key).c_str(),
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        
        wchar_t buffer[256];
        DWORD bufferSize = sizeof(buffer);
        
        if (RegQueryValueExW(hKey, L"Username", NULL, NULL, (BYTE*)buffer, &bufferSize) == ERROR_SUCCESS) {
            username = std::wstring(buffer);
        }
        
        bufferSize = sizeof(buffer);
        if (RegQueryValueExW(hKey, L"Password", NULL, NULL, (BYTE*)buffer, &bufferSize) == ERROR_SUCCESS) {
            password = std::wstring(buffer);
        }
        
        RegCloseKey(hKey);
        return !username.empty();
    }
    
    return false;
}

void GitCredentials::ClearCredentials(const std::wstring& url) {
    std::wstring key = GetCredentialKey(url);
    RegDeleteTreeW(HKEY_CURRENT_USER, (L"SOFTWARE\\NativeIDE\\Credentials\\" + key).c_str());
}

std::wstring GitCredentials::GetCredentialKey(const std::wstring& url) {
    // Simple hash of URL for key name
    std::hash<std::wstring> hasher;
    return std::to_wstring(hasher(url));
}

} // namespace NativeIDE