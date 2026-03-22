// ============================================================================
// integrated_runtime.hpp — Single boot/shutdown hook for Transcendence (E→Ω)
// ============================================================================
#pragma once

namespace RawrXD {
namespace IntegratedRuntime {

/// Runs TranscendenceCoordinator::initializeAll() once per process (non-fatal).
void boot();

/// Reverse-order shutdown; no-op if boot was skipped via env or never called.
void shutdown();

} // namespace IntegratedRuntime
} // namespace RawrXD
