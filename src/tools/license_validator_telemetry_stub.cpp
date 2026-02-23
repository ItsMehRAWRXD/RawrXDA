// ============================================================================
// license_validator_telemetry_stub.cpp — No-op UTC_LogEvent for standalone validator
// ============================================================================
// The license_gate_validator links license_enforcement.cpp which may call
// UTC_LogEvent when RAWR_HAS_MASM is defined. This stub provides a no-op
// implementation so the validator can build without the telemetry ASM kernel.
//
// Contract: Signature must match include/rawrxd_telemetry_exports.h
//   uint64_t UTC_LogEvent(const char* message);
// Only UTC_LogEvent is required for this target. UTC_FlushToDisk and other
// UTC_* symbols are used only when RAWRXD_LINK_TELEMETRY_KERNEL_ASM or
// RAWR_HAS_MASM is set (ASM kernel linked); do not add them here unless
// a build that uses this stub also references them.
// ============================================================================

#include <cstdint>

extern "C" {

uint64_t UTC_LogEvent(const char* /*message*/) {
    return 0;
}

} // extern "C"
