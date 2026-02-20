# RawrXD Win32 IDE

A native Windows IDE with integrated terminal support for PowerShell and Command Prompt.

## Features

- **Code Editor**: Rich text editor with syntax highlighting support
- **File Management**: Open, save, and create new files
- **Terminal Integration**:
  - PowerShell support
  - Command Prompt support
  - Real-time output display
  - Interactive command execution
- **Native Windows GUI**: Built with Win32 API for optimal performance

## Building

### Prerequisites
- MinGW-w64 compiler
- CMake 3.20+
- Windows SDK

### Build Steps
```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DBUILD_TESTS=ON
mingw32-make RawrXD-Win32IDE
```

## Usage

1. **File Operations**:
   - File → New: Create a new file
   - File → Open: Open existing files
   - File → Save/Save As: Save current file

2. **Terminal**:
   - Terminal → PowerShell: Start PowerShell session
   - Terminal → Command Prompt: Start CMD session
   - Type commands in the bottom input field and press Enter
   - Terminal → Stop Terminal: End current session

3. **Editor**:
   - Edit code in the main text area
   - Basic text editing with Rich Edit controls

## Architecture

- `Win32IDE.h/cpp`: Main IDE window and UI management
- `Win32TerminalManager.h/cpp`: Terminal process management using Windows pipes
- `main_win32.cpp`: Application entry point

## Terminal Implementation

The terminal integration uses:
- `CreateProcess` with redirected stdin/stdout/stderr
- Anonymous pipes for I/O communication
- Separate threads for reading output and monitoring process state
- Support for both PowerShell and Command Prompt