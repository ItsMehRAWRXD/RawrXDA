// ============================================================================
// startup_phase_registry.h — Dynamic, lazy startup phase order
// ============================================================================
// Phase order is loaded from config/startup_phases.txt (one name per line).
// If the file is missing, a built-in default order is used. No hardcoded sequence.
// Lines starting with # or "lazy:" are supported; "lazy:name" phases are skipped
// at boot and run on first runPhaseLazy(name).
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_set>
#include <unordered_map>

namespace RawrXD {
namespace Startup {

// Phase order: load from config/startup_phases.txt or return default order.
std::vector<std::string> getPhaseOrder();

// Run a phase by name only if it was marked lazy and not yet run (idempotent).
using PhaseFn = std::function<void()>;
void registerLazyPhase(const std::string& name, PhaseFn fn);
bool runPhaseLazy(const std::string& name);

// Check whether a phase name is marked lazy in the config (or default set).
bool isPhaseLazy(const std::string& name);

} // namespace Startup
} // namespace RawrXD
