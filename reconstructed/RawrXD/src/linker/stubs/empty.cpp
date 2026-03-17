#include "linker_stubs.h"

// All classes now have real implementations:
// - HotPatcher -> hot_patcher.cpp
// - MemoryCore -> memory_core.cpp
// - AgenticEngine -> agentic_engine.cpp
// - VSIXLoader -> vsix_loader.cpp
// - MemoryManager -> modules/memory_manager.cpp
// - AdvancedFeatures -> final_linker_stubs.cpp (ApplyHotPatch stub)
// - ToolRegistry -> tool_registry.cpp (inject_tools implementation)
// - ReactServerGenerator -> react_generator_stubs.cpp
// - g_memory_system -> memory_core.cpp (extern variable)

// This file provides only forward declarations and type definitions
// No actual implementations needed here
