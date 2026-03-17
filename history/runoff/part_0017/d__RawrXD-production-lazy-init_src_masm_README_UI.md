# RawrXD UI MASM Layer

This document covers the MASM Win32 UI layer in `ui_masm.asm`, including the terminal execution path and the Problems panel navigation flow.

## Features
- **Terminal execution**: `execute_terminal_command` spawns `cmd.exe /c` with output capture and streams the result into the terminal pane.
- **Build action**: `handle_run_command` runs `build_masm_hotpatch.bat Release` and reports status dialogs.
- **Problems panel**: `add_problem_to_panel` formats `file(line): error` and `navigate_problem_panel` opens the file and moves the caret to the line.

## Manual smoke test
1. Build and run the UI layer.
2. Use the Run command to trigger a build; confirm output appears in the terminal pane.
3. Inject a test problem entry via `add_problem_to_panel`, double-click it in the list, and verify the editor opens the file and jumps to the line.

## Notes
- Terminal output capture uses Win32 pipes and `CreateProcessA` with `STARTF_USESTDHANDLES`.
- Problems panel uses a dedicated listbox created in `ui_create_controls` and resized in `WM_SIZE`.
