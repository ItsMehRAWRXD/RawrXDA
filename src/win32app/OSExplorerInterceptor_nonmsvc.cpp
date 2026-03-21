#if !defined(_MSC_VER)

#include "OSExplorerInterceptor.h"

#include <windows.h>

#include <memory>

std::unique_ptr<OSExplorerInterceptor> g_osInterceptor = nullptr;

OSExplorerInterceptor::OSExplorerInterceptor()
    : m_interceptor(nullptr),
      m_targetPID(0),
      m_hTargetProcess(nullptr),
      m_isRunning(false),
      m_callback(nullptr),
      m_stopLogThread(false) {}

OSExplorerInterceptor::~OSExplorerInterceptor() {
    m_isRunning = false;
    m_stopLogThread.store(true, std::memory_order_relaxed);
    if (m_logThread && m_logThread->joinable()) {
        m_logThread->join();
    }
    if (m_hTargetProcess) {
        CloseHandle(m_hTargetProcess);
        m_hTargetProcess = nullptr;
    }
}

bool OSExplorerInterceptor::Initialize(DWORD targetPID, OSInterceptorCallback callback) {
    if (m_isRunning) {
        return false;
    }
    m_targetPID = targetPID;
    m_callback = callback;
    m_hTargetProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPID);
    return (m_hTargetProcess != nullptr);
}

bool OSExplorerInterceptor::StartInterception() {
    if (!m_hTargetProcess || m_isRunning) {
        return false;
    }
    m_isRunning = true;
    return true;
}

#endif  // !defined(_MSC_VER)
