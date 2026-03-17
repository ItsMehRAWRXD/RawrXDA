#include "IocpFileWatcher.h"
#include "IDELogger.h"

#include <filesystem>

namespace {
std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

std::string ActionToString(DWORD action) {
    switch (action) {
        case FILE_ACTION_ADDED: return "added";
        case FILE_ACTION_REMOVED: return "removed";
        case FILE_ACTION_MODIFIED: return "modified";
        case FILE_ACTION_RENAMED_OLD_NAME: return "renamed_old";
        case FILE_ACTION_RENAMED_NEW_NAME: return "renamed_new";
        default: return "unknown";
    }
}
}

IocpFileWatcher::IocpFileWatcher()
    : m_dirHandle(INVALID_HANDLE_VALUE)
    , m_iocp(nullptr)
    , m_overlapped{}
    , m_buffer(64 * 1024)
    , m_running(false) {
    ZeroMemory(&m_overlapped, sizeof(m_overlapped));
}

IocpFileWatcher::~IocpFileWatcher() {
    Stop();
}

void IocpFileWatcher::SetCallback(ChangeCallback callback) {
    m_callback = std::move(callback);
}

bool IocpFileWatcher::Start(const std::wstring& path) {
    if (m_running.exchange(true)) return true;

    m_path = path;
    if (!std::filesystem::exists(m_path)) {
        LOG_WARNING("IocpFileWatcher: path does not exist");
        m_running = false;
        return false;
    }

    m_dirHandle = CreateFileW(
        m_path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (m_dirHandle == INVALID_HANDLE_VALUE) {
        LOG_ERROR("IocpFileWatcher: failed to open directory handle");
        m_running = false;
        return false;
    }

    m_iocp = CreateIoCompletionPort(m_dirHandle, nullptr, reinterpret_cast<ULONG_PTR>(this), 0);
    if (!m_iocp) {
        LOG_ERROR("IocpFileWatcher: failed to create IOCP");
        CloseHandle(m_dirHandle);
        m_dirHandle = INVALID_HANDLE_VALUE;
        m_running = false;
        return false;
    }

    if (!ArmWatch()) {
        Stop();
        return false;
    }

    m_worker = std::thread([this]() { WorkerLoop(); });
    LOG_INFO("IocpFileWatcher started");
    return true;
}

void IocpFileWatcher::Stop() {
    if (!m_running.exchange(false)) return;

    if (m_dirHandle != INVALID_HANDLE_VALUE) {
        CancelIoEx(m_dirHandle, &m_overlapped);
    }

    if (m_iocp) {
        PostQueuedCompletionStatus(m_iocp, 0, 0, nullptr);
    }

    if (m_worker.joinable()) {
        m_worker.join();
    }

    if (m_dirHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_dirHandle);
        m_dirHandle = INVALID_HANDLE_VALUE;
    }

    if (m_iocp) {
        CloseHandle(m_iocp);
        m_iocp = nullptr;
    }

    LOG_INFO("IocpFileWatcher stopped");
}

bool IocpFileWatcher::ArmWatch() {
    DWORD bytesReturned = 0;
    ZeroMemory(&m_overlapped, sizeof(m_overlapped));

    BOOL ok = ReadDirectoryChangesW(
        m_dirHandle,
        m_buffer.data(),
        static_cast<DWORD>(m_buffer.size()),
        TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_SIZE,
        &bytesReturned,
        &m_overlapped,
        nullptr
    );

    if (!ok) {
        LOG_ERROR("IocpFileWatcher: ReadDirectoryChangesW failed");
        return false;
    }

    return true;
}

void IocpFileWatcher::WorkerLoop() {
    while (m_running.load()) {
        DWORD bytesTransferred = 0;
        ULONG_PTR key = 0;
        LPOVERLAPPED overlapped = nullptr;
        BOOL ok = GetQueuedCompletionStatus(m_iocp, &bytesTransferred, &key, &overlapped, INFINITE);
        if (!m_running.load()) break;
        if (!ok || !overlapped) {
            continue;
        }

        if (bytesTransferred > 0) {
            HandleNotifications(bytesTransferred);
        }

        if (m_running.load()) {
            ArmWatch();
        }
    }
}

void IocpFileWatcher::HandleNotifications(DWORD bytesTransferred) {
    BYTE* base = m_buffer.data();
    DWORD offset = 0;

    while (offset < bytesTransferred) {
        auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(base + offset);
        std::wstring fileName(info->FileName, info->FileNameLength / sizeof(WCHAR));
        std::filesystem::path fullPath = std::filesystem::path(m_path) / fileName;
        std::string payload = "file_change:" + ActionToString(info->Action) + ":" + WideToUtf8(fullPath.wstring());

        if (m_callback) {
            m_callback(payload);
        }

        if (info->NextEntryOffset == 0) break;
        offset += info->NextEntryOffset;
    }
}
