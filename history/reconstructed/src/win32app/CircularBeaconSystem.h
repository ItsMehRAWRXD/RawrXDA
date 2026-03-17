#pragma once
// ============================================================================
// CircularBeaconSystem.h — Compat shim → new circular_beacon_system.h
// ============================================================================
// All legacy callers (CircularBeaconManager, old pane beacons) route through
// here straight into the new BeaconHub singleton.
// NO external deps. NO Qt. NO OpenSSL/Vulkan/JNI.
// ============================================================================

#include "../../include/circular_beacon_system.h"

// ── Legacy compat aliases so old code compiles without mass rename ──
// Old code used BeaconType, new code uses RawrXD::BeaconKind.
// Old code used BeaconDirection (BIDIRECTIONAL etc), new uses RawrXD::BeaconDirection.
// Provide thin inline bridge.

using LegacyBeaconHub = RawrXD::BeaconHub;

// Standard beacon verb strings (replaces old #define BEACON_CMD_*)
#define BEACON_CMD_REFRESH        "beacon.refresh"
#define BEACON_CMD_UPDATE         "beacon.update"
#define BEACON_CMD_HOTRELOAD      "hotpatch.reload"
#define BEACON_CMD_AGENTIC_REQUEST "agent.request"
#define BEACON_CMD_TUNE_ENGINE    "gpu.tune"
#define BEACON_CMD_SWITCH_KERNEL  "hotpatch.swap_kernel"
#define BEACON_CMD_AUTONOMOUS_SCAN "agent.autonomous_scan"
#define BEACON_CMD_EXECUTE_PLAN   "agent.execute_plan"
#define BEACON_CMD_CIRCULAR_BROADCAST "beacon.circular_broadcast"