// ============================================================================
// startup_phase_registry.h — Dynamic, lazy startup phase order
// ============================================================================
// Phase order is loaded from config/startup_phases.txt (one name per line).
// If the file is missing, a built-in default order is used. No hardcoded sequence.
// Lines starting with # or "lazy:" are supported; "lazy:name" phases are skipped
// at boot and run on first runPhaseLazy(name).
// ============================================================================

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RawrXD
{
namespace Startup
{

// Optional absolute path to startup_phases.txt (e.g. next to RawrXD-Win32IDE.exe).
// When empty, getPhaseOrder() tries config/startup_phases.txt then startup_phases.txt in cwd.
void setPhaseOrderFileOverride(std::string absoluteOrEmpty);

// Phase order: load from config/startup_phases.txt or return default order.
std::vector<std::string> getPhaseOrder();

// Run a phase by name only if it was marked lazy and not yet run (idempotent).
using PhaseFn = std::function<void()>;
void registerLazyPhase(const std::string& name, PhaseFn fn);
bool runPhaseLazy(const std::string& name);

// Check whether a phase name is marked lazy in the config (or default set).
bool isPhaseLazy(const std::string& name);

// One stable id per process for startup / message-loop logs (PR02, E0x tracing).
void ensureStartupSessionId() noexcept;
const char* getStartupSessionId() noexcept;

}  // namespace Startup
}  // namespace RawrXD
