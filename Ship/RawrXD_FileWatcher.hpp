// RawrXD_FileWatcher.hpp - Real-Time File System Monitoring
// Pure C++20 - No Qt Dependencies
// Watches for code changes and triggers re-analysis

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <iostream>

namespace RawrXD {

enum class FileEventType { Created, Modified, Deleted, Renamed };

struct FileEvent {
    FileEventType type;
    std::wstring path;
};

class FileWatcher {
public:
    using Callback = std::function<void(const FileEvent&)>;

    explicit FileWatcher(const std::wstring& directory, Callback callback) 
        : directory_(directory), callback_(callback), running_(false) {}

    ~FileWatcher() {
        Stop();
    }

    void Start() {
        if (running_) return;
        running_ = true;
        watchThread_ = std::thread(&FileWatcher::WatchLoop, this);
    }

    void Stop() {
        if (running_) {
            running_ = false;
            if (watchThread_.joinable()) watchThread_.join();
        }
    }

private:
    std::wstring directory_;
    Callback callback_;
    std::atomic<bool> running_;
    std::thread watchThread_;

    void WatchLoop() {
        HANDLE hDir = CreateFileW(
            directory_.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL
        );

        if (hDir == INVALID_HANDLE_VALUE) {
            std::wcerr << L"Failed to open directory for watching\n";
            return;
        }

        char buffer[8192];
        DWORD bytesReturned;

        while (running_) {
            if (ReadDirectoryChangesW(
                hDir,
                buffer,
                sizeof(buffer),
                TRUE, // Watch subtree
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                &bytesReturned,
                NULL,
                NULL
            )) {
                FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)buffer;
                do {
                    FileEvent evt;
                    evt.path = directory_ + L"\\" + std::wstring(fni->FileName, fni->FileNameLength / sizeof(WCHAR));

                    switch (fni->Action) {
                        case FILE_ACTION_ADDED:
                            evt.type = FileEventType::Created;
                            break;
                        case FILE_ACTION_MODIFIED:
                            evt.type = FileEventType::Modified;
                            break;
                        case FILE_ACTION_REMOVED:
                            evt.type = FileEventType::Deleted;
                            break;
                        case FILE_ACTION_RENAMED_NEW_NAME:
                            evt.type = FileEventType::Renamed;
                            break;
                        default:
                            continue;
                    }

                    callback_(evt);

                    if (fni->NextEntryOffset == 0) break;
                    fni = (FILE_NOTIFY_INFORMATION*)((char*)fni + fni->NextEntryOffset);
                } while (true);
            }
        }

        CloseHandle(hDir);
    }
};

} // namespace RawrXD
