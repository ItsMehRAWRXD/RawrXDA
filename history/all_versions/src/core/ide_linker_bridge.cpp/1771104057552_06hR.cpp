// ============================================================================
// ide_linker_bridge.cpp — Final Linker Closure for RawrXD-Win32IDE
// ============================================================================
#include "enterprise_license.h"
#include "enterprise/multi_gpu.h"
#include "enterprise_feature_manager.hpp"
#include "telemetry/UnifiedTelemetryCore.h"
#include "telemetry/logger.h"
#include "rawrxd_telemetry_exports.h"
#include <cstdarg>
#include <cstdio>

namespace RawrXD {

    // 1. MultiGPUManager bridge (RawrXD::MultiGPUManager → RawrXD::Enterprise::MultiGPUManager)
    class MultiGPUManager {
    public:
        static MultiGPUManager& Instance();
    };

    MultiGPUManager& MultiGPUManager::Instance() {
        return reinterpret_cast<MultiGPUManager&>(RawrXD::Enterprise::MultiGPUManager::Instance());
    }

    namespace Telemetry {
        // 2. Telemetry Logger bridge (RawrXD::Telemetry::Logger::Log → UTC_LogEvent)
        class Logger {
        public:
            static void Log(TelemetryLevel level, const char* fmt, ...);
        };

        void Logger::Log(TelemetryLevel level, const char* fmt, ...) {
            char buffer[2048];
            va_list args;
            va_start(args, fmt);
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            va_end(args);
            
            // Forward to the global singleton logger
            ::Logger::instance().log(::Logger::Info, buffer);

            // Also forward to the ASM telemetry kernel ring buffer if available
#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
            UTC_LogEvent(buffer);
#endif
        }
    }
}

// 3. EnterpriseFeatureManager bridge (satisfy Instance() if global symbol missing)
EnterpriseFeatureManager& EnterpriseFeatureManager::Instance() {
    static EnterpriseFeatureManager instance;
    return instance;
}

// 4. Enterprise_DevUnlock bridge
// If the ASM one is missing or has a name mismatch, providing a C++ fallback.
extern "C" int64_t Enterprise_DevUnlock() {
    // Return 0 (denied) as fallback if not otherwise available
    return 0;
}
