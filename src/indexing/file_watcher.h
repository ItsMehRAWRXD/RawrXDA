#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>

namespace RawrXD::Indexing {

class FileWatcher {
public:
    FileWatcher(const std::string& directory);
    ~FileWatcher();

    // Start watching in a background thread
    void Start(std::function<void(const std::string&, DWORD action)> callback);
    void Stop();

private:
    void WatchLoop();

    std::string m_directory;
    std::function<void(const std::string&, DWORD action)> m_callback;
    HANDLE m_hDir;
    HANDLE m_hStopEvent;
    std::atomic<bool> m_running;
    std::thread m_thread;
};

} // namespace RawrXD::Indexing
