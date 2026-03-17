# RawrXD SafeMode CLI

This document provides instructions for using the RawrXD SafeMode Command Line Interface (CLI). This CLI provides the full functionality of the RawrXD IDE without the need for a graphical user interface. This is useful for running agentic tasks, managing models, and performing diagnostics in environments where a GUI is not available or desired.

## Commands

### Agentic Engine & Planning

-   `mission <objective>`: Start a high-level agentic mission. The Zero-Day Agentic Engine will take over and attempt to complete the objective using all available tools.
-   `plan <objective>`: Create an execution plan for a given task. The Plan Orchestrator will generate a sequence of tool calls to achieve the objective.
-   `execute_plan`: Execute the last generated plan.
-   `registry`: List all 44+ production tools available in the Tool Registry.

### Model Management

-   `run <model>`: Load and interact with a model. This command can accept a model name or a path to a GGUF file.
-   `load <path>`: Load a GGUF model from a specific path.
-   `unload`: Unload the current model.
-   `list`: List available models in the common model directories.
-   `show <model>`: Show detailed information about the currently loaded model.
-   `tier [name]`: Show or switch the compression tier for the currently loaded model.
-   `tiers`: List all available tiers for the currently loaded model.

### Inference

-   `generate <prompt>`: Generate text from a prompt using the currently loaded model.
-   `chat`: Enter an interactive chat mode with the currently loaded model.
-   `stream <prompt>`: Stream the output of a generation token-by-token.

### Hotpatch & Diagnostics

-   `hotpatch list`: List available and applied hotpatches.
-   `hotpatch apply <id>`: Apply a hotpatch by its ID.
-   `hotpatch revert <id>`: Revert a hotpatch by its ID.
-   `selftest`: Run the self-test gate to diagnose the system.

### Legacy Agentic Tools

-   `tools`: List available agentic tools (same as `registry`).
-   `tool <name> <json>`: Execute a specific tool with the given JSON parameters.
-   `file <read|write|list> ...`: Shortcuts for file operations.
-   `git <status|add|commit|...> ...`: Shortcuts for Git operations.
-   `exec <cmd>`: Execute a shell command.

### System

-   `status`: Show the current system status and health.
-   `telemetry`: Show telemetry data.
-   `api start [port]`: Start the API server.
-   `api stop`: Stop the API server.
-   `governor start`: Start the overclock governor.
-   `governor stop`: Stop the overclock governor.
-   `workspace <path>`: Set the workspace root directory.
-   `settings`: Show the current settings.
-   `save`: Save the current settings.
-   `verbose [on|off]`: Toggle verbose output.

### General

-   `help`: Show the help message.
-   `clear`: Clear the screen.
-   `quit`: Exit the SafeMode CLI.
