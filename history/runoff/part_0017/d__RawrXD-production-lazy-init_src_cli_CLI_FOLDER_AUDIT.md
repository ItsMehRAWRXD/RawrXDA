# CLI Folder Audit - January 23, 2026

## Overview
This folder contains all CLI-related source files for the RawrXD IDE. It includes command handlers, IPC clients, local model logic, and entry points for the CLI application.

## Key Components
- **cli_command_handler.cpp/hpp**: Implements the CLI command handler, registering all commands and integrating with the model router, chat interface, debugger, profiler, and test runner.
- **cli_ipc_client.cpp/hpp**: Handles IPC communication from the CLI to other processes (e.g., GUI or backend services).
- **cli_local_model.cpp/hpp**: Manages local model operations and commands.
- **main_cli.cpp / ide_cli.cpp**: Entry points for launching the CLI application.
- **cli_command_integration.cpp/hpp**: Integrates CLI commands with other system components.

## IPC/Communication
- IPC is supported via `cli_ipc_client` for communication with the GUI or backend.
- The CLI can act as a client to a shared backend or server process.

## Status
- All major CLI features are implemented.
- IPC client exists, but ensure it is fully tested with the GUI bridge.
- No external dependencies (e.g., Vulkan/ROCm) remain in this folder.

## TODO
- Confirm all CLI commands are mirrored in the GUI and vice versa.
- Validate IPC client/server integration with the GUI.
- Ensure documentation is up to date for all new/changed commands.
