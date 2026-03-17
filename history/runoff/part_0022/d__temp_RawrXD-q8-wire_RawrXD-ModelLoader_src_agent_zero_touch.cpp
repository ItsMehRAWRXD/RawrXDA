#include "zero_touch.hpp"
#include "auto_bootstrap.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <windows.h>

namespace fs = std::filesystem;

namespace RawrXD {

ZeroTouch::ZeroTouch() {}

void ZeroTouch::installAll() {
    installFileWatcher();
    installGitHook();
    installVoiceTrigger();
    std::cout << "Zero-touch triggers installed" << std::endl;
}

void ZeroTouch::installFileWatcher() {
    fs::path srcRoot = fs::current_path() / "src";
    if (!fs::exists(srcRoot)) {
        std::cout << "ZeroTouch: src directory missing, skipping file watcher" << std::endl;
        return;
    }

    // Start a thread to watch for changes using Win32 API
    std::thread([srcRoot]() {
        HANDLE hDir = CreateFileA(
            srcRoot.string().c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL
        );

        if (hDir == INVALID_HANDLE_VALUE) return;

        char buffer[1024];
        DWORD bytesReturned;
        while (ReadDirectoryChangesW(
            hDir, buffer, sizeof(buffer), TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
            &bytesReturned, NULL, NULL
        )) {
            FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)buffer;
            do {
                std::wstring wFileName(fni->FileName, fni->FileNameLength / sizeof(WCHAR));
                fs::path filePath = srcRoot / wFileName;
                
                if (filePath.extension() == ".cpp" || filePath.extension() == ".hpp") {
                    std::string fileName = filePath.filename().string();
                    std::thread([fileName]() {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        std::string wish = "Auto-fix and ship after source change in " + fileName;
                        SetEnvironmentVariableA("RAWRXD_AUTO_APPROVE", "1");
                        AutoBootstrap::startWithWish(wish);
                    }).detach();
                }

                if (fni->NextEntryOffset == 0) break;
                fni = (FILE_NOTIFY_INFORMATION*)((char*)fni + fni->NextEntryOffset);
            } while (true);
        }
        CloseHandle(hDir);
    }).detach();
}

void ZeroTouch::installGitHook() {
    fs::path hooksDir = fs::current_path() / ".git/hooks";
    if (!fs::exists(hooksDir)) {
        std::cout << "ZeroTouch: git hooks directory missing - skip" << std::endl;
        return;
    }

    fs::path hookPath = hooksDir / "post-commit";
    fs::path agentExe = fs::current_path() / "build/bin/Release/RawrXD-Agent.exe";
    std::string agentExeStr = agentExe.string();
    std::replace(agentExeStr.begin(), agentExeStr.end(), '\\', '/');

    std::string hookScript = 
        "#!/bin/sh\n"
        "# RawrXD zero-touch trigger\n"
        "WISH=$(git log -1 --pretty=%B | head -1)\n"
        "if echo \"$WISH\" | grep -qE \"(ship|release|fix|add)\"; then\n"
        "  export RAWRXD_WISH=\"$WISH\"\n"
        "  " + agentExeStr + "\n"
        "fi\n";

    std::ofstream hookFile(hookPath, std::ios::binary);
    if (!hookFile.is_open()) {
        std::cerr << "ZeroTouch: failed to write git hook " << hookPath << std::endl;
        return;
    }

    hookFile << hookScript;
    hookFile.close();

    // In a production environment, we'd set execution bits if on Linux/macOS.
    // On Windows, git bash handles the shebang.
    
    std::cout << "ZeroTouch: post-commit hook installed" << std::endl;
}

void ZeroTouch::installVoiceTrigger() {
    // Placeholder for voice trigger logic
}

} // namespace RawrXD
