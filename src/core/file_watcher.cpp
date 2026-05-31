#include "file_watcher.h"
#include <windows.h>
#include <thread>

namespace RawrXD::Core {
    FileWatcher::FileWatcher() : m_hDir(INVALID_HANDLE_VALUE), m_running(false) {}

    FileWatcher::~FileWatcher() { stop(); }

    bool FileWatcher::watch(const std::string& path, FileChangeCallback cb) {
        stop();
        m_hDir = CreateFileA(path.c_str(), FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (m_hDir == INVALID_HANDLE_VALUE) return false;

        m_callback = cb;
        m_running = true;
        m_worker = std::thread([this, path]() {
            char buffer[4096];
            OVERLAPPED ol{};
            ol.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
            DWORD bytesReturned;

            while (m_running) {
                ResetEvent(ol.hEvent);
                BOOL ok = ReadDirectoryChangesW(m_hDir, buffer, sizeof(buffer), TRUE,
                    FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
                    &bytesReturned, &ol, nullptr);
                if (!ok) break;

                DWORD wait = WaitForSingleObject(ol.hEvent, 100);
                if (wait == WAIT_OBJECT_0) {
                    DWORD transferred;
                    GetOverlappedResult(m_hDir, &ol, &transferred, FALSE);
                    if (transferred > 0) {
                        FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
                        do {
                            int len = WideCharToMultiByte(CP_UTF8, 0, info->FileName,
                                info->FileNameLength / sizeof(WCHAR), nullptr, 0, nullptr, nullptr);
                            std::string name(len, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, info->FileName,
                                info->FileNameLength / sizeof(WCHAR), name.data(), len, nullptr, nullptr);

                            FileChangeEvent evt;
                            evt.path = path + "\\" + name;
                            switch (info->Action) {
                                case FILE_ACTION_ADDED: evt.type = FileChangeType::CREATED; break;
                                case FILE_ACTION_REMOVED: evt.type = FileChangeType::DELETED; break;
                                case FILE_ACTION_MODIFIED: evt.type = FileChangeType::MODIFIED; break;
                                case FILE_ACTION_RENAMED_OLD_NAME: evt.type = FileChangeType::RENAMED_OLD; break;
                                case FILE_ACTION_RENAMED_NEW_NAME: evt.type = FileChangeType::RENAMED_NEW; break;
                                default: evt.type = FileChangeType::MODIFIED; break;
                            }
                            if (m_callback) m_callback(evt);
                            info = info->NextEntryOffset ?
                                reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(info) + info->NextEntryOffset) : nullptr;
                        } while (info);
                    }
                }
            }
            CloseHandle(ol.hEvent);
        });
        return true;
    }

    void FileWatcher::stop() {
        m_running = false;
        if (m_hDir != INVALID_HANDLE_VALUE) {
            CancelIo(m_hDir);
            CloseHandle(m_hDir);
            m_hDir = INVALID_HANDLE_VALUE;
        }
        if (m_worker.joinable()) m_worker.join();
    }
}
