// ============================================================================
// cli_extension_commands.hpp — CLI Extension/Plugin Command Handler
// ============================================================================

#pragma once

#include <string>

namespace RawrXD {
namespace CLI {

// Handle !plugin commands in CLI mode
// Returns true if the input was a plugin command and was handled
// Returns false if the input is not a plugin command
bool handlePluginCommand(const std::string& input);

} // namespace CLI
} // namespace RawrXD
