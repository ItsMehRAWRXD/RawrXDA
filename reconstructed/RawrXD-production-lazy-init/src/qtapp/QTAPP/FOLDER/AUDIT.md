# QTAPP Folder Audit - January 23, 2026

## Overview
This folder contains the core Qt application logic for the RawrXD IDE, including advanced model routing, chat interfaces, analytics, and integration with agentic and MASM features.

## Key Components
- **phase5_model_router.cpp/h**: Implements the advanced model router for inference, load balancing, and analytics. Central to both CLI and GUI operation.
- **phase5_chat_interface.cpp/h**: Provides chat session management, model selection, and streaming inference, tightly integrated with the model router.
- **api_server.h**: API server for inter-process communication and remote control.
- **masm_bridge/qt_masm_bridge.cpp**: MASM integration for agentic and hotpatch features.
- **MainWindow.cpp/h**: Main application window and UI logic for the GUI IDE.
- **testing/**: Contains test and validation logic for the Qt application.

## IPC/Communication
- The API server and model router are used by both CLI and GUI for shared backend logic.
- MASM and agentic bridges enable advanced features and parity between CLI and GUI.

## Status
- All major Qt application features are implemented.
- Model router and chat interface are shared and tested.
- MASM and agentic integration present.
- No external dependencies (e.g., Vulkan/ROCm) remain in this folder.

## TODO
- Ensure all API server endpoints are documented and tested.
- Validate MASM and agentic bridges for new features.
- Confirm all CLI/GUI commands are routed through the shared backend.
- Update documentation for new/changed features.
