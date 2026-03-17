#include <cstdint>
#include <cstdlib>

#include "enterprise_license.h"

// Provides Enterprise_DevUnlock for MASM-enabled builds where the ASM export
// is not currently linked into RawrEngine.
#if defined(RAWR_HAS_MASM) && RAWR_HAS_MASM
namespace RawrXD {

extern "C" int64_t Enterprise_DevUnlock() {
    const char* devGate = std::getenv("RAWRXD_ENTERPRISE_DEV");
    if (!devGate || devGate[0] == '\0' || devGate[0] == '0') {
        return 0;
    }

    EnterpriseLicense& lic = EnterpriseLicense::Instance();
    (void)lic.Initialize();

    if (lic.Is800BUnlocked() || lic.HasFeatureMask(LicenseFeature::DualEngine800B)) {
        g_800B_Unlocked = 1;
        g_EnterpriseFeatures |= LicenseFeature::EnterpriseAll;
        return 1;
    }

    // Dev override path: explicitly elevate feature mask for local dev flows.
    g_800B_Unlocked = 1;
    g_EnterpriseFeatures |= LicenseFeature::EnterpriseAll;
    return 1;
}

}  // namespace RawrXD
#endif
