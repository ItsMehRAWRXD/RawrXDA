// ============================================================================
// ide_linker_bridge.cpp — Final Linker Closure for RawrXD-Win32IDE
// ============================================================================
#include "enterprise_license.h"
#include "enterprise_feature_manager.hpp"
#include "license_enforcement.h"
#include "feature_flags_runtime.h"
#include "enterprise/multi_gpu.h"
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

    // 3. EnterpriseLicense bridge (Legacy)
    bool EnterpriseLicense::Initialize() {
        return true;
    }
}

// 4. EnterpriseFeatureManager bridge (satisfy Instance() if global symbol missing)
EnterpriseFeatureManager& EnterpriseFeatureManager::Instance() {
    static EnterpriseFeatureManager instance;
    return instance;
}

bool EnterpriseFeatureManager::Initialize() {
    return true;
}

void EnterpriseFeatureManager::Shutdown() {
}

// 5. EnterpriseLicenseV2 bridge (namespace RawrXD::License)
namespace RawrXD::License {
    EnterpriseLicenseV2& EnterpriseLicenseV2::Instance() {
        static EnterpriseLicenseV2 instance;
        return instance;
    }

    LicenseResult EnterpriseLicenseV2::initialize() {
        return LicenseResult::ok("Linker bridge initialization");
    }
}

// 6. LicenseEnforcer bridge (namespace RawrXD::Enforce)
namespace RawrXD::Enforce {
    LicenseEnforcer& LicenseEnforcer::Instance() {
        static LicenseEnforcer instance;
        return instance;
    }

    bool LicenseEnforcer::initialize() {
        return true;
    }
}

// 7. FeatureFlagsRuntime bridge (namespace RawrXD::Flags)
namespace RawrXD::Flags {
    FeatureFlagsRuntime& FeatureFlagsRuntime::Instance() {
        static FeatureFlagsRuntime instance;
        return instance;
    }

    void FeatureFlagsRuntime::refreshFromLicense() {
        // No-op
    }
}

// 8. Enterprise_DevUnlock bridge
// If the ASM one is missing or has a name mismatch, providing a C++ fallback.
extern "C" int64_t Enterprise_DevUnlock() {
    // Return 0 (denied) as fallback if not otherwise available
    return 0;
}

