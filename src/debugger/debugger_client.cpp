#include "debugger_client.h"

namespace RawrXD::Debugger {
    DebuggerClient::DebuggerClient() : m_process(nullptr), m_thread(nullptr) {}

    DebuggerClient::~DebuggerClient() { detach(); }

    bool DebuggerClient::launch(const std::string& executable, const std::string& args, const std::string& cwd) {
        std::string cmd = "\"" + executable + "\" " + args;

        STARTUPINFOA si{sizeof(si)};
        PROCESS_INFORMATION pi{};

        BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
            DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE,
            nullptr, cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);

        if (!ok) return false;

        m_process = pi.hProcess;
        m_thread = pi.hThread;
        m_pid = pi.dwProcessId;

        m_debugThread = std::thread([this]() {
            DEBUG_EVENT evt;
            while (m_attached) {
                if (WaitForDebugEvent(&evt, 100)) {
                    handleDebugEvent(evt);
                    ContinueDebugEvent(evt.dwProcessId, evt.dwThreadId, DBG_CONTINUE);
                }
            }
        });
        m_attached = true;
        return true;
    }

    void DebuggerClient::detach() {
        m_attached = false;
        if (m_debugThread.joinable()) m_debugThread.join();
        if (m_process) {
            DebugActiveProcessStop(m_pid);
            CloseHandle(m_process);
            m_process = nullptr;
        }
        if (m_thread) {
            CloseHandle(m_thread);
            m_thread = nullptr;
        }
    }

    void DebuggerClient::handleDebugEvent(const DEBUG_EVENT& evt) {
        switch (evt.dwDebugEventCode) {
            case EXCEPTION_DEBUG_EVENT: {
                auto& ex = evt.u.Exception;
                BreakpointHit hit;
                hit.address = reinterpret_cast<uint64_t>(ex.ExceptionRecord.ExceptionAddress);
                hit.exceptionCode = ex.ExceptionRecord.ExceptionCode;
                if (m_breakpointCallback) m_breakpointCallback(hit);
                break;
            }
            case CREATE_PROCESS_DEBUG_EVENT:
                CloseHandle(evt.u.CreateProcessInfo.hFile);
                break;
            case LOAD_DLL_DEBUG_EVENT:
                CloseHandle(evt.u.LoadDll.hFile);
                break;
            case OUTPUT_DEBUG_STRING_EVENT: {
                auto& ds = evt.u.DebugString;
                std::string msg(ds.nDebugStringLength, '\0');
                SIZE_T read;
                ReadProcessMemory(m_process, ds.lpDebugStringData, msg.data(), ds.nDebugStringLength, &read);
                if (m_outputCallback) m_outputCallback(msg);
                break;
            }
        }
    }

    bool DebuggerClient::setBreakpoint(uint64_t address) {
        uint8_t int3 = 0xCC;
        SIZE_T written;
        return WriteProcessMemory(m_process, reinterpret_cast<LPVOID>(address), &int3, 1, &written) && written == 1;
    }
}
