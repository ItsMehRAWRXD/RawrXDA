// ignite_800b.cpp — Force-enable 800B swarm sharding/dual-engine features.
// This intentionally bypasses license gating for v23 ignition.

#include "enterprise_license.h"
#include "enterprise_feature_manifest.hpp"

namespace {
struct Ignite800B {
    Ignite800B() {
        // Signal ASM/C++ bridges that 800B is unlocked.
        RawrXD::g_800B_Unlocked = 1;
        // Grant 800B dual-engine and distributed swarm feature bits.
        RawrXD::g_EnterpriseFeatures |= (RawrXD::Enterprise::Mask::DualEngine800B |
                                         RawrXD::Enterprise::Mask::DistributedSwarm);
    }
};

static Ignite800B g_ignite800b;
} // namespace
