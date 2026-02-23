#pragma once
// Consolidated: forwards to plugins/MemoryPlugin.hpp which now contains
// the merged interface, factory, and plugin implementations from both
// the original src/memory_plugin.hpp and plugins/MemoryPlugin.hpp.
#include "plugins/MemoryPlugin.hpp"

// Re-export into the global namespace for callers that expect unqualified names
using RawrXD::StandardMemoryPlugin;
using RawrXD::LargeContextPlugin;
using RawrXD::MemoryPluginFactory;
