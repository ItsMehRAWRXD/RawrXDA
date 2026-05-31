#include "file_watcher.h"
#include <iostream>

namespace RawrXD::Indexing {

FileWatcher::FileWatcher(const std::string& directory) 
    : m_directory(directory), m_running(false), m_hDir(INVALID_HANDLE_VALUE), m_hStopEvent(NULL) {
}

FileWatcher::~FileWatcher() {
    Stop();
}

void FileWatcher::Start(std::function<void(const std::string&, DWORD action)> callback) {
    if (m_running) return;

    m_callback = callback;
    m_hDir = CreateFileA(
        m_directory.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (m_hDir == INVALID_HANDLE_VALUE) {
        return;
    }

    m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_running = true;
    m_thread = std::thread(&FileWatcher::WatchLoop, this);
}

void FileWatcher::Stop() {
    if (!m_running) return;

    m_running = false;
    SetEvent(m_hStopEvent);
    
    // Wake up ReadDirectoryChangesW
    CancelIoEx(m_hDir, NULL);

    if (m_thread.joinable()) {
        m_thread.join();
    }

    CloseHandle(m_hDir);
    CloseHandle(m_hStopEvent);
}

void FileWatcher::WatchLoop() {
    char buffer[1024];
    DWORD bytesReturned;
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    while (m_running) {
        if (ReadDirectoryChangesW(
            m_hDir,
            buffer,
            sizeof(buffer),
            TRUE, // watch subtree
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            &overlapped,
            NULL
        )) {
            HANDLE handles[] = { overlapped.hEvent, m_hStopEvent };
            DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

            if (wait == WAIT_OBJECT_0) {
                // Change detected
                FILE_NOTIFY_INFORMATION* pNotify;
                size_t offset = 0;
                do {
                    pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(&buffer[offset]);
                    
                    std::wstring wFileName(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));
                    // Simple conversion for now
                    std::string fileName(wFileName.begin(), wFileName.end());
                    
                    if (m_callback) {
                        m_callback(fileName, pNotify->Action);
                    }

                    offset += pNotify->NextEntryOffset;
                } while (pNotify->NextEntryOffset != 0);
                
                ResetEvent(overlapped.hEvent);
            } else {
                // Stopped or error
                break;
            }
        } else {
            break;
        }
    }

    CloseHandle(overlapped.hEvent);
}

} // namespace RawrXD::Indexing
