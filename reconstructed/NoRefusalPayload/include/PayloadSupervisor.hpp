#pragma once

#include <windows.h>
#include <cstdint>

namespace Engine {

    /**
     * @class PayloadSupervisor
     * @brief Manages the lifecycle and hardening of the "No-Refusal" payload.
     * 
     * This class implements process-level hardening, anti-debugging measures,
     * and ensures the ASM core remains in continuous execution state.
     */
    class PayloadSupervisor {
    public:
        PayloadSupervisor();
        ~PayloadSupervisor() = default;

        /**
         * @brief Initializes process hardening mechanisms.
         * @return true if hardening was successful, false otherwise.
         */
        bool InitializeHardening();

        /**
         * @brief Launches the MASM No-Refusal core.
         * Transfers execution control to the assembly layer.
         */
        void LaunchCore();

        /**
         * @brief Protects memory regions containing the ASM payload.
         * Sets the code section to PAGE_EXECUTE_READ.
         */
        void ProtectMemoryRegions();

        /**
         * @brief Checks if the payload is currently active.
         * @return true if active, false otherwise.
         */
        bool IsActive() const { return m_isActive; }

    private:
        HANDLE m_processHandle;
        bool m_isActive;

        /**
         * @brief Console control handler callback (static).
         * Intercepts termination signals.
         */
        static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType);
    };

} // namespace Engine
