#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include "ExtensionIPCChannel.h"

extern "C" HANDLE MASM_CreateIsolatedProcess(const char* appName, char* cmdLine, const char* workDir, DWORD memLimitMB);

namespace RawrXD::Extensions {

class ExtensionInstance {
public:
    ExtensionInstance(const std::string& id, const std::string& path);
    ~ExtensionInstance();

    bool Launch();
    void Shutdown();
    bool IsRunning() const { return m_running; }

private:
    void MessageLoop();

    std::string m_id;
    std::string m_path;
    HANDLE m_hProcess = NULL;
    std::unique_ptr<ExtensionIPCChannel> m_ipc;
    std::thread m_messageThread;
    std::atomic<bool> m_running{false};
};

}
