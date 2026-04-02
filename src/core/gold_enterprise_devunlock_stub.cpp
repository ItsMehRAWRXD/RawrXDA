// Satisfies enterprise_feature_manager / license.h when RawrXD_Gold does not link
// MASM enterprise objects that define Enterprise_DevUnlock.
#include <cstdint>

extern "C" std::int64_t Enterprise_DevUnlock()
{
    return 0;
}
