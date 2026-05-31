#pragma once

#include <windows.h>
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

class IocpFileWatcher {
public:
    using ChangeCallback = std::function<void(const std::string&)>;

    IocpFileWatcher();
    ~IocpFileWatcher();

    bool Start(const std::wstring& path);
    void Stop();
    bool IsRunning() const { return m_running.load(); }

    void SetCallback(ChangeCallback callback);

private:
    void WorkerLoop();
    bool ArmWatch();
    void HandleNotifications(DWORD bytesTransferred);

    std::wstring m_path;
    HANDLE m_dirHandle;
    HANDLE m_iocp;
    OVERLAPPED m_overlapped;
    std::vector<BYTE> m_buffer;

    std::atomic<bool> m_running;
    std::thread m_worker;
    ChangeCallback m_callback;
};
