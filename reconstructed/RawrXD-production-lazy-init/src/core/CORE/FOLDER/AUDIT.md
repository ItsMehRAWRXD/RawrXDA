# SRC/CORE Folder Audit - January 23, 2026

## Overview
This folder contains core logic, orchestration, and integration for the RawrXD IDE, including autonomous agents, streaming orchestrators, and system runtime features.

## Key Components
- **autonomous_agent.*:** Autonomous agent logic and integration.
- **local_gguf_loader.*:** Local GGUF model loader for core backend.
- **orchestra_command_handler.* / orchestra_manager.*:** Orchestration command handling and management.
- **RawrXD-Streaming-Orchestrator-Production.asm:** Streaming orchestrator in assembly for high performance.
- **rawrxd_lazyinit_*.vcxproj:** Project files for lazy initialization and DLLs.
- **real_agentic_engine.*:** Real agentic engine implementation.
- **safe_mode_config.*:** Safe mode configuration and logic.
- **system_runtime.*:** System runtime integration and management.
- **win32_integration.*:** Win32-specific integration logic.

## Status
- All major core and orchestration features are present and up to date.
- No external dependencies (e.g., Vulkan/ROCm) remain in this folder.

## TODO
- Ensure all core APIs are documented and robust.
- Validate integration with CLI, GUI, and backend.
- Confirm all orchestration and agentic features are tested and production-ready.
