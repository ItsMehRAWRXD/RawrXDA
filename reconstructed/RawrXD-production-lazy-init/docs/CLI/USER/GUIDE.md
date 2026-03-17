# RawrXD CLI User Guide

## Overview

RawrXD-CLI is a command-line interface that provides **identical functionality** to the RawrXD IDE GUI. It's designed for:

- **Headless automation** - Run IDE operations in CI/CD pipelines
- **Remote access** - Manage projects via SSH without X11 forwarding
- **Safe mode recovery** - Diagnose and fix issues when GUI fails
- **Power users** - Rapid project management via keyboard

## Installation

Both executables are built from the same codebase:
- `rawrxd-qtshell.exe` - Full GUI application
- `rawrxd-cli.exe` - Command-line interface

## Quick Start

### Interactive Mode
```powershell
# Start interactive REPL
rawrxd-cli --interactive

# Start with a project already open
rawrxd-cli -p "D:\MyProject" --interactive
```

### Batch Mode
```powershell
# Run a single command
rawrxd-cli build --config Release

# Chain commands
rawrxd-cli project open D:\MyProject
rawrxd-cli build
rawrxd-cli test run
```

### JSON Output
```powershell
# Get machine-readable output
rawrxd-cli --json diag run
rawrxd-cli --json git status
```

## Command Reference

### Project Management

| Command | Description |
|---------|-------------|
| `project` | Show current project |
| `project open <path>` | Open a project |
| `project close` | Close current project |
| `project create <path> <template>` | Create new project |

**Templates:** `cpp`, `python`, `rust`

```powershell
# Create a new C++ project
project create D:\NewProject cpp

# Open existing project
project open D:\ExistingProject
```

### Build System

| Command | Description |
|---------|-------------|
| `build [target] [--config Release\|Debug]` | Build project |
| `clean` | Clean build artifacts |
| `rebuild [target]` | Clean and rebuild |
| `configure [-G generator]` | Configure CMake |

```powershell
# Build in release mode
build --config Release

# Build specific target
build RawrXD-Core --config Debug

# Reconfigure CMake
configure -G "Visual Studio 17 2022"
```

### Version Control (Git)

| Command | Description |
|---------|-------------|
| `git status` | Show working tree status |
| `git add [files...]` | Stage files |
| `git commit <message>` | Commit changes |
| `git push [remote] [branch]` | Push to remote |
| `git pull [remote] [branch]` | Pull from remote |
| `git branch [name]` | List or create branches |
| `git checkout <branch>` | Switch branches |
| `git log [--count N]` | Show commit history |

```powershell
# Standard workflow
git add src/main.cpp
git commit "Fix memory leak"
git push origin main
```

### File Operations

| Command | Description |
|---------|-------------|
| `file read <path>` | Read file contents |
| `file write <path> <content>` | Write to file |
| `file find <pattern> [path]` | Find files by glob |
| `file search <query> [--pattern glob]` | Search in files |
| `file replace <search> <replace> [--pattern glob] [--dry-run]` | Replace in files |

```powershell
# Find all C++ files
file find "*.cpp"

# Search for a string
file search "TODO" --pattern "*.cpp"

# Replace (with dry-run preview)
file replace "oldFunc" "newFunc" --pattern "*.cpp" --dry-run
```

### Execution

| Command | Description |
|---------|-------------|
| `exec <command> [args...]` | Execute program |
| `shell <command>` | Run shell command |

```powershell
# Run a program
exec python main.py --verbose

# Shell command
shell dir /s *.cpp
```

### AI/ML Operations

| Command | Description |
|---------|-------------|
| `ai load <model-path>` | Load GGUF model |
| `ai unload` | Unload current model |
| `ai infer <prompt>` | Run inference |
| `ai status` | Show model status |

```powershell
# Load a model
ai load models/codellama-7b.gguf

# Run inference
ai infer "Write a function to calculate fibonacci"

# Check status
ai status
```

### Testing

| Command | Description |
|---------|-------------|
| `test discover` | Find all tests |
| `test run [test-ids...]` | Run tests |
| `test coverage` | Generate coverage report |

```powershell
# Discover and run all tests
test discover
test run

# Run specific tests
test run test_parser test_lexer
```

### Diagnostics

| Command | Description |
|---------|-------------|
| `diag run` | Full diagnostic scan |
| `diag info` | System information |
| `diag status` | Subsystem status |

```powershell
# Run diagnostics
diag run

# Get JSON output for scripting
rawrxd-cli --json diag run > diagnostics.json
```

### Utilities

| Command | Description |
|---------|-------------|
| `pwd` | Print working directory |
| `ls [path]` | List directory contents |
| `clear` | Clear screen |
| `help` | Show help |
| `exit` | Exit CLI |

## Safe Mode Usage

Safe mode is automatically activated when you use the CLI. It provides:

1. **Core functionality** - All essential IDE operations
2. **Minimal dependencies** - No GPU, reduced memory
3. **Diagnostic focus** - Emphasis on recovery and repair

### Recovery Workflow

```powershell
# 1. Start CLI in verbose mode
rawrxd-cli --verbose --interactive

# 2. Run diagnostics
diag run

# 3. Check system info
diag info

# 4. Review subsystem status
diag status

# 5. If project issues, try opening directly
project open D:\ProblemProject

# 6. Check for build issues
configure
build --config Debug
```

### Diagnostic Report

The `diag run` command checks:

- **Toolchain**: Git, CMake, MSVC, GCC, Clang, Python, Node.js, Rust
- **Subsystems**: Build, VCS, Debug, Profiler, AI, Terminal, Test, Hotpatch
- **Directories**: AppData, Temp, Config writable
- **Memory**: Physical and virtual memory status
- **Health Score**: 0-100 with critical issues and warnings

## Environment Variables

| Variable | Description |
|----------|-------------|
| `RAWRXD_PROJECT` | Default project path |
| `RAWRXD_CONFIG` | Configuration file path |
| `RAWRXD_LOG_LEVEL` | Logging verbosity (debug/info/warn/error) |

## Exit Codes

| Code | Description |
|------|-------------|
| 0 | Success |
| 1 | General error |
| 2 | Invalid arguments |
| 3 | Project not found |
| 4 | Build failure |
| 5 | Test failure |
| 10+ | Tool-specific errors |

## Examples

### CI/CD Integration

```yaml
# GitHub Actions example
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: |
          rawrxd-cli project open .
          rawrxd-cli configure
          rawrxd-cli build --config Release
      - name: Test
        run: rawrxd-cli test run
```

### PowerShell Script

```powershell
# Automated build script
$project = "D:\MyProject"
$config = "Release"

rawrxd-cli project open $project
if ($LASTEXITCODE -ne 0) { exit 1 }

rawrxd-cli build --config $config
if ($LASTEXITCODE -ne 0) { exit 1 }

rawrxd-cli test run
exit $LASTEXITCODE
```

### Batch Processing

```powershell
# Process multiple projects
$projects = @("D:\Project1", "D:\Project2", "D:\Project3")

foreach ($proj in $projects) {
    Write-Host "Building $proj..."
    rawrxd-cli project open $proj
    rawrxd-cli build --config Release
    rawrxd-cli project close
}
```

## Troubleshooting

### CLI won't start
1. Check Qt DLLs are in PATH or alongside executable
2. Run `where rawrxd-cli` to verify installation

### Build fails
1. Run `diag run` to check toolchain
2. Verify CMake is installed: `cmake --version`
3. Check compiler availability: `cl` or `gcc --version`

### Project won't open
1. Verify path exists: `ls D:\MyProject`
2. Check for CMakeLists.txt or other build files
3. Run `diag run` for detailed diagnostics

### AI model won't load
1. Check model file exists
2. Verify available memory with `diag info`
3. Try smaller model variant

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    RawrXD-Core Library                  │
│  ┌───────────────────────────────────────────────────┐  │
│  │              OrchestraManager                      │  │
│  │  - Build System    - File Operations              │  │
│  │  - VCS (Git)       - AI/Inference                 │  │
│  │  - Debug           - Terminal                     │  │
│  │  - Test            - Hotpatch                     │  │
│  │  - Diagnostics     - Project Management           │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
           │                              │
           ▼                              ▼
┌─────────────────────┐      ┌─────────────────────┐
│   RawrXD-QtShell    │      │    RawrXD-CLI       │
│   (GUI Executable)  │      │  (CLI Executable)   │
│                     │      │                     │
│   - Qt Widgets      │      │   - REPL Loop       │
│   - Visual Editor   │      │   - Batch Mode      │
│   - Menus/Toolbars  │      │   - JSON Output     │
│   - Panel System    │      │   - PowerShell UX   │
└─────────────────────┘      └─────────────────────┘
```

Both executables share the exact same core functionality through `OrchestraManager`. The only difference is the user interface.

## License

RawrXD is proprietary software. See LICENSE file for details.
