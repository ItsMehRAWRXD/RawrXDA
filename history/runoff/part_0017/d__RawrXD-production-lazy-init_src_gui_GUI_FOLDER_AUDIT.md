# GUI Folder Audit - January 23, 2026

## Overview
This folder contains all GUI-related source files for the RawrXD IDE. It includes the GUI-CLI bridge, IPC server/client, command handlers, and main entry points for the graphical application.

## Key Components
- **cli_bridge.cpp/hpp**: Implements the bridge between the GUI and CLI, enabling command and data exchange.
- **ipc_server.cpp/hpp, ipc_server_impl.cpp/hpp**: Implements the IPC server for the GUI, allowing it to receive commands from the CLI or other clients.
- **ipc_client.cpp/hpp, ipc_connection.cpp/hpp**: Implements IPC client and connection logic for communicating with other processes.
- **command_handlers.cpp/hpp, command_registry.cpp/hpp**: Registers and manages GUI commands, ensuring parity with CLI.
- **main_gui.cpp**: Entry point for launching the GUI application.
- **editor_agent_integration.cpp/hpp**: Integrates agentic features into the GUI editor.

## IPC/Communication
- The GUI can act as both an IPC server and client, supporting two-way communication with the CLI.
- The bridge ensures feature parity and shared state between CLI and GUI.

## Status
- All major GUI features are implemented.
- IPC server/client and bridge exist, but should be fully tested for robust CLI-GUI communication.
- No external dependencies (e.g., Vulkan/ROCm) remain in this folder.

## TODO
- Confirm all GUI commands are mirrored in the CLI and vice versa.
- Validate IPC server/client and bridge integration with the CLI.
- Ensure documentation is up to date for all new/changed commands.
