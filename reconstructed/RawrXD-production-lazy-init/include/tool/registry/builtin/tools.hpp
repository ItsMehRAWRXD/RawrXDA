#pragma once

#include <string>

class ToolRegistry;

namespace RawrXD {

// Registers the repo's existing backend tool implementations into the ToolRegistry
// instance used by the MASM bridge (ToolRegistry_InvokeToolSet).
void registerBackendAgenticTools(ToolRegistry& registry, const std::string& workspaceRoot);

} // namespace RawrXD
