// ToolDispatchTable.h — Per-Tool Dispatch Interface
// HIGH PRIORITY FIX #3 Header
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

namespace RawrXD::Agentic {

// Execute a tool by its unique ID (0-46)
// CRITICAL FIX: No aliasing - each tool has independent handler
int ExecuteToolByID(uint32_t tool_id, const char* args, char* output, size_t outlen);

// Validate dispatch table: verify all 47 tools have handlers
bool ValidateToolDispatchTable();

}  // namespace RawrXD::Agentic
