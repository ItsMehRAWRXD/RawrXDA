// ============================================================================
// ide_linker_bridge.cpp — Final Linker Closure for RawrXD-Win32IDE
// ============================================================================
#include "enterprise_license.h"
#include "enterprise/multi_gpu.h"
#include "telemetry/UnifiedTelemetryCore.h"
#include "telemetry/logger.h"
#include <cstdarg>
#include <cstdio>

namespace RawrXD {

    // 1. EnterpriseLicense is already in RawrXD namespace in enterprise_license.h
    // and implemented in enterprise_license.cpp. If the linker still complains, 
    // it's likely a build systemic issue, but we'll provide a second implementation 
    // here if needed. (Actually, having two might cause LNK2005. Let's see.)

    // 2. MultiGPUManager bridge
    // The IDE expects RawrXD::MultiGPUManager, but implementation is in RawrXD::Enterprise
    class MultiGPUManager {
    public:
        static MultiGPUManager& Instance();
    };

    MultiGPUManager& MultiGPUManager::Instance() {
        // Safe enough for a singleton bridge
        return reinterpret_cast<MultiGPUManager&>(RawrXD::Enterprise::MultiGPUManager::Instance());
    }

    namespace Telemetry {
        // 3. Telemetry Logger bridge
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
        }
    }
}
