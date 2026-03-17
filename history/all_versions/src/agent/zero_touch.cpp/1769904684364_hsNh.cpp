#include "zero_touch.hpp"
#include "auto_bootstrap.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <map>

namespace fs = std::filesystem;

ZeroTouch::ZeroTouch() : m_running(false) {}

void ZeroTouch::installAll() {
    installGitHook();
    
    // File watcher and voice trigger need a running loop or thread. 
    // Since this is called from installAll which seemed synchronous, 
    // but the original used QFileSystemWatcher (async) and void* (async event loop).
    // We should probably spawn threads for these.
    
    std::thread([this]() { installFileWatcher(); }).detach();
    std::thread([this]() { installVoiceTrigger(); }).detach();


}

void ZeroTouch::installFileWatcher() {
    fs::path srcRoot = fs::current_path() / "src";
    if (!fs::exists(srcRoot)) {
        
        return;
    }

    // Simple polling watcher since we can't use QFileSystemWatcher and don't want complex ReadDirectoryChangesW loop here immediately
    std::map<std::string, fs::file_time_type> lastWriteTimes;
    
    for (const auto& entry : fs::recursive_directory_iterator(srcRoot)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".cpp" || ext == ".hpp") {
                lastWriteTimes[entry.path().string()] = fs::last_write_time(entry);
            }
        }
    }
    
    m_running = true;
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        for (const auto& entry : fs::recursive_directory_iterator(srcRoot)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                std::string ext = entry.path().extension().string();
                if (ext != ".cpp" && ext != ".hpp") continue;

                auto currentStr = fs::last_write_time(entry);
                if (lastWriteTimes.find(path) == lastWriteTimes.end()) {
                    lastWriteTimes[path] = currentStr;
                    // New file
                } else {
                    if (lastWriteTimes[path] != currentStr) {
                        lastWriteTimes[path] = currentStr;
                        // Changed


                        // Debounce/wait a bit? 
                        // Original had 5s delay.
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        
                        std::string wish = "Auto-fix and ship after source change in " + entry.path().filename().string();
                        _putenv_s("RAWRXD_AUTO_APPROVE", "1");
                        AutoBootstrap::startWithWish(wish);
                    }
                }
            }
        }
    }
}

void ZeroTouch::installGitHook() {
    fs::path hooksDir = fs::current_path() / ".git" / "hooks";
    if (!fs::exists(hooksDir)) {
        
        return;
    }

    fs::path hookPath = hooksDir / "post-commit";
    fs::path agentExe = fs::current_path() / "build" / "bin" / "Release" / "RawrXD-Agent.exe";
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

    std::ofstream hookFile(hookPath);
    if (!hookFile) {
        
        return;
    }

    hookFile << hookScript;
    hookFile.close();
    
    // Set executable permissions? On Windows fs::permissions might work or is ignored.
    // fs::permissions(hookPath, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add);


}

void ZeroTouch::installVoiceTrigger() {
    // Polling clipboard 
    m_running = true;
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        if (OpenClipboard(NULL)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData != NULL) {
                char* pszText = static_cast<char*>(GlobalLock(hData));
                if (pszText != NULL) {
                    std::string spoken(pszText);
                    GlobalUnlock(hData);
                    CloseClipboard();
                    
                    if (!spoken.empty() && spoken != m_lastVoiceWish) {
                         if (spoken.length() > 10 && spoken.length() < 200) {
                             std::string lowerSpoken = spoken;
                             std::transform(lowerSpoken.begin(), lowerSpoken.end(), lowerSpoken.begin(), ::tolower);
                             
                             if (lowerSpoken.find("ship") != std::string::npos || 
                                 lowerSpoken.find("release") != std::string::npos || 
                                 lowerSpoken.find("fix") != std::string::npos) {
                                     
                                m_lastVoiceWish = spoken;
                                // Clear clipboard? 
                                if (OpenClipboard(NULL)) {
                                    EmptyClipboard();
                                    CloseClipboard();
                                }
                                
                                _putenv_s("RAWRXD_AUTO_APPROVE", "1");
                                AutoBootstrap::startWithWish(spoken);
                             }
                         }
                    }
                } else {
                    CloseClipboard();
                }
            } else {
                CloseClipboard();
            }
        }
    }
}
