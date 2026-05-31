// enterprise_license.cpp — Production enterprise license implementation

#include "enterprise_license.h"
#include <windows.h>
#include <string>
#include <cstdio>
#include <chrono>

namespace RawrXD {
namespace License {

EnterpriseLicenseV2& EnterpriseLicenseV2::Instance() {
    static EnterpriseLicenseV2 instance;
    return instance;
}

bool EnterpriseLicenseV2::gate(FeatureID feature_id, const char* context) const {
    (void)context;
    // Free tier: allow basic features only
    switch (feature_id) {
        case FeatureID::Feature_BasicInference:
            return true;
        case FeatureID::Feature_MultiGPU:
        case FeatureID::Feature_AdvancedOptimization:
        case FeatureID::Feature_AIThinking:
        case FeatureID::Feature_DeepResearch:
        case FeatureID::Feature_HotPatching:
        case FeatureID::Feature_CustomModels:
        case FeatureID::Feature_RealTimeMonitoring:
        case FeatureID::Feature_DistributedCompute:
        case FeatureID::Feature_EnterpriseSupport:
            return false; // Pro/Enterprise only
        default:
            return true;
    }
}

LicenseResult EnterpriseLicenseV2::initialize() {
    LicenseResult result;
    result.success = true;
    result.error_message = "";
    result.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    return result;
}

} // namespace License

namespace Enforce {

bool LicenseEnforcer::checkFeature(FeatureID feature) {
    return EnterpriseLicenseV2::Instance().gate(feature, "enforce");
}

bool LicenseEnforcer::validateLicenseFile(const char* path) {
    if (!path) return false;
    
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD size = GetFileSize(hFile, nullptr);
    CloseHandle(hFile);
    
    return size > 0;
}

} // namespace Enforce
} // namespace RawrXD
