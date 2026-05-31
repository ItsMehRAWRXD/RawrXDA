// ============================================================================
// Win32IDE_DAPServer.h — Debug Adapter Protocol Server Header
// ============================================================================
// Provides DAP 1.70 compatibility for VS Code, Cursor, Windsurf debuggers.
// ============================================================================

#pragma once

#include <string>
#include <memory>
#include <cstdint>

class Win32IDE;

class Win32IDE_DAPServer
{
  public:
    Win32IDE_DAPServer(Win32IDE* parent);
    ~Win32IDE_DAPServer();

    // Lifecycle control
    bool startServer(uint16_t port = 5678);
    void stopServer();
    bool isRunning() const;

    // Debugger state hooks (called from native debugger engine)
    void notifyBreakpointHit(int threadId, const std::string& reason, int frameId = 0);
    void notifyThreadCreated(int threadId, const std::string& threadName);
    void notifyThreadExited(int threadId);
    void notifyProgramTerminated();
    void notifyDebugOutput(const std::string& text, const std::string& category = "console");

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
