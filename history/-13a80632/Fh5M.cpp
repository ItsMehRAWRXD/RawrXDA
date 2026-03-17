#include "os_abstraction.h"
#include <iostream>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <direct.h>
#elif defined(__APPLE__)
#include <pwd.h>
#include <unistd.h>
#else
#include <pwd.h>
#include <unistd.h>
#include <dirent.h>
#endif

std::string OSAbstraction::getHomeDirectory() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, path))) {
        return std::string(path);
    }
    return "";
#else
    const char* home = getenv("HOME");
    if (home) return std::string(home);
    
    struct passwd* pw = getpwuid(getuid());
    if (pw) return std::string(pw->pw_dir);
    
    return "";
#endif
}

std::string OSAbstraction::getCurrentDirectory() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (GetCurrentDirectoryA(MAX_PATH, path)) {
        return std::string(path);
    }
    return "";
#else
    char path[4096];
    if (getcwd(path, sizeof(path))) {
        return std::string(path);
    }
    return "";
#endif
}

bool OSAbstraction::setCurrentDirectory(const std::string& path) {
#ifdef _WIN32
    return SetCurrentDirectoryA(path.c_str()) != 0;
#else
    return chdir(path.c_str()) == 0;
#endif
}

std::vector<std::string> OSAbstraction::listDirectory(const std::string& path) {
    std::vector<std::string> entries;

#ifdef _WIN32
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return entries;
    }

    do {
        entries.push_back(std::string(findFileData.cFileName));
    } while (FindNextFileA(hFind, &findFileData));

    FindClose(hFind);
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) return entries;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        entries.push_back(std::string(entry->d_name));
    }

    closedir(dir);
#endif

    return entries;
}

bool OSAbstraction::createDirectory(const std::string& path) {
#ifdef _WIN32
    return CreateDirectoryA(path.c_str(), nullptr) != 0;
#else
    return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool OSAbstraction::fileExists(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

std::string OSAbstraction::openFileDialog(const std::string& title, const std::string& filter) {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrFilter = filter.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        return std::string(szFile);
    }
    return "";
#elif defined(__APPLE__)
    // macOS implementation using NSOpenPanel
    NSOpenPanel* openPanel = [NSOpenPanel openPanel];
    [openPanel setTitle:[NSString stringWithUTF8String:title.c_str()]];
    [openPanel setCanChooseFiles:YES];
    [openPanel setCanChooseDirectories:NO];
    [openPanel setAllowsMultipleSelection:NO];
    
    // Set allowed file types from filter if provided
    if (!filter.empty()) {
        // Parse filter: "Text Files (*.txt)\0*.txt\0"
        NSMutableArray* fileTypes = [NSMutableArray array];
        std::string filterCopy = filter;
        size_t pos = 0;
        while (pos < filterCopy.length()) {
            size_t nextNull = filterCopy.find('\0', pos);
            if (nextNull == std::string::npos) break;
            std::string pattern = filterCopy.substr(pos, nextNull - pos);
            
            // Extract extension from pattern (e.g., "*.txt" -> "txt")
            if (pattern.length() > 2 && pattern[0] == '*' && pattern[1] == '.') {
                NSString* ext = [NSString stringWithUTF8String:pattern.substr(2).c_str()];
                [fileTypes addObject:ext];
            }
            pos = nextNull + 1;
        }
        if ([fileTypes count] > 0) {
            [openPanel setAllowedFileTypes:fileTypes];
        }
    }
    
    NSInteger result = [openPanel runModal];
    if (result == NSModalResponseOK) {
        NSURL* url = [openPanel URL];
        return std::string([[url path] UTF8String]);
    }
    return "";
#else
    // Linux implementation using GTK would go here
    std::cout << "[Dialog] Linux open file dialog not yet implemented" << std::endl;
    return "";
#endif
}

std::string OSAbstraction::saveFileDialog(const std::string& title, const std::string& filter) {
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrFilter = filter.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileNameA(&ofn)) {
        return std::string(szFile);
    }
    return "";
#elif defined(__APPLE__)
    // macOS implementation using NSSavePanel
    NSSavePanel* savePanel = [NSSavePanel savePanel];
    [savePanel setTitle:[NSString stringWithUTF8String:title.c_str()]];
    [savePanel setCanCreateDirectories:YES];
    
    // Set allowed file types from filter if provided
    if (!filter.empty()) {
        // Parse filter: "Text Files (*.txt)\0*.txt\0"
        NSMutableArray* fileTypes = [NSMutableArray array];
        std::string filterCopy = filter;
        size_t pos = 0;
        while (pos < filterCopy.length()) {
            size_t nextNull = filterCopy.find('\0', pos);
            if (nextNull == std::string::npos) break;
            std::string pattern = filterCopy.substr(pos, nextNull - pos);
            
            // Extract extension from pattern (e.g., "*.txt" -> "txt")
            if (pattern.length() > 2 && pattern[0] == '*' && pattern[1] == '.') {
                NSString* ext = [NSString stringWithUTF8String:pattern.substr(2).c_str()];
                [fileTypes addObject:ext];
            }
            pos = nextNull + 1;
        }
        if ([fileTypes count] > 0) {
            [savePanel setAllowedFileTypes:fileTypes];
        }
    }
    
    NSInteger result = [savePanel runModal];
    if (result == NSModalResponseOK) {
        NSURL* url = [savePanel URL];
        return std::string([[url path] UTF8String]);
    }
    return "";
#else
    std::cout << "[Dialog] Linux save file dialog not yet implemented" << std::endl;
    return "";
#endif
}

std::string OSAbstraction::selectDirectoryDialog(const std::string& title) {
#ifdef _WIN32
    BROWSEINFOA bi;
    char path[MAX_PATH];
    
    ZeroMemory(&bi, sizeof(bi));
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl && SHGetPathFromIDListA(pidl, path)) {
        CoTaskMemFree(pidl);
        return std::string(path);
    }
    return "";
#elif defined(__APPLE__)
    std::cout << "[Dialog] macOS directory dialog not yet implemented" << std::endl;
    return "";
#else
    std::cout << "[Dialog] Linux directory dialog not yet implemented" << std::endl;
    return "";
#endif
}

std::string OSAbstraction::getDefaultShell() {
#ifdef _WIN32
    return "pwsh";
#elif defined(__APPLE__)
    return "zsh";
#else
    return "bash";
#endif
}

std::vector<std::string> OSAbstraction::getAvailableShells() {
    std::vector<std::string> shells;

#ifdef _WIN32
    shells.push_back("pwsh");
    shells.push_back("cmd");
#else
    shells.push_back("bash");
    shells.push_back("zsh");
    shells.push_back("fish");
#endif

    return shells;
}

bool OSAbstraction::executeCommand(const std::string& command, std::string& output, std::string& error) {
#ifdef _WIN32
    HANDLE hStdoutRead, hStdoutWrite;
    HANDLE hStderrRead, hStderrWrite;
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) return false;
    if (!SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0)) return false;
    
    if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) return false;
    if (!SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0)) return false;
    
    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStderrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    
    if (!CreateProcessA(nullptr, (LPSTR)command.c_str(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrRead);
        CloseHandle(hStderrWrite);
        return false;
    }
    
    CloseHandle(hStdoutWrite);
    CloseHandle(hStderrWrite);
    
    // Read output
    DWORD dwRead = 0;
    char chBuf[4096];
    while (ReadFile(hStdoutRead, chBuf, sizeof(chBuf), &dwRead, nullptr) && dwRead > 0) {
        output.append(chBuf, dwRead);
    }
    
    while (ReadFile(hStderrRead, chBuf, sizeof(chBuf), &dwRead, nullptr) && dwRead > 0) {
        error.append(chBuf, dwRead);
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdoutRead);
    CloseHandle(hStderrRead);
    
    return true;
#else
    // Unix/Linux implementation
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return false;
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
    pclose(pipe);
    return true;
#endif
}

std::string OSAbstraction::pathJoin(const std::string& path1, const std::string& path2) {
#ifdef _WIN32
    return path1 + "\\" + path2;
#else
    return path1 + "/" + path2;
#endif
}

std::string OSAbstraction::pathBaseName(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash == std::string::npos) return path;
    return path.substr(lastSlash + 1);
}

std::string OSAbstraction::pathDirName(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash == std::string::npos) return ".";
    return path.substr(0, lastSlash);
}

bool OSAbstraction::pathIsAbsolute(const std::string& path) {
#ifdef _WIN32
    return path.length() > 1 && path[1] == ':';
#else
    return !path.empty() && path[0] == '/';
#endif
}

std::string OSAbstraction::getOSName() {
#ifdef _WIN32
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string OSAbstraction::getOSVersion() {
#ifdef _WIN32
    OSVERSIONINFOA osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOA));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    GetVersionExA(&osvi);
    return std::to_string(osvi.dwMajorVersion) + "." + std::to_string(osvi.dwMinorVersion);
#else
    return "Unknown";
#endif
}

std::string OSAbstraction::getArchitecture() {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x86_64" : "x86";
#else
    return "Unknown";
#endif
}