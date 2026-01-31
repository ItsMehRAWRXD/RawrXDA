# RawrXD Refactoring Tool

## Overview

The `Refactor-RawrXD.ps1` script is an enterprise-grade tool that analyzes and refactors the monolithic `RawrXD.ps1` script (29,870+ lines) into a modular architecture.

## Features

- **Automatic Function Extraction**: Identifies and extracts all 294+ functions from the source
- **Intelligent Categorization**: Groups functions into 15 logical modules based on functionality
- **Dependency Analysis**: Maps function dependencies and relationships
- **Module Generation**: Creates PowerShell module files (.psm1) with proper exports
- **Main Script Creation**: Generates a main orchestration script that loads all modules
- **Manifest Generation**: Creates a JSON manifest documenting the refactoring process

## Module Categories

The refactoring tool organizes functions into the following modules:

1. **Core** - Initialization, configuration, entry points
2. **Logging** - Logging and error handling infrastructure
3. **UI** - Windows Forms GUI components and dialogs
4. **Editor** - Text editor functionality and syntax highlighting
5. **FileOperations** - File and directory operations
6. **Git** - Git version control integration
7. **AI** - Ollama AI integration and chat functionality
8. **Browser** - WebView2 browser and video functionality
9. **Terminal** - CLI and terminal operations
10. **Settings** - Settings and configuration management
11. **Agent** - Agent task automation
12. **Marketplace** - Extension marketplace functionality
13. **Video** - YouTube video operations
14. **Security** - Encryption, authentication, validation
15. **Performance** - Performance monitoring and optimization
16. **Utilities** - Helper functions and utilities

## Usage

### Basic Usage

```powershell
.\Refactor-RawrXD.ps1 -SourceFile "RawrXD.ps1" -OutputDirectory "Refactored"
```

### Dry Run (Analysis Only)

```powershell
.\Refactor-RawrXD.ps1 -SourceFile "RawrXD.ps1" -OutputDirectory "Refactored" -DryRun
```

### Parameters

- **SourceFile** (Required): Path to the source PowerShell script
- **OutputDirectory** (Optional): Directory for refactored output (default: "Refactored")
- **DryRun** (Optional): Analyze only, don't create files

## Output Structure

After refactoring, you'll have:

```
Refactored/
├── main.ps1                          # Main entry point
├── refactoring.log                   # Refactoring process log
├── refactoring-manifest.json         # Detailed manifest
└── Modules/
    ├── RawrXD.Core.psm1
    ├── RawrXD.Logging.psm1
    ├── RawrXD.UI.psm1
    ├── RawrXD.Editor.psm1
    ├── RawrXD.FileOperations.psm1
    ├── RawrXD.Git.psm1
    ├── RawrXD.AI.psm1
    ├── RawrXD.Browser.psm1
    ├── RawrXD.Terminal.psm1
    ├── RawrXD.Settings.psm1
    ├── RawrXD.Agent.psm1
    ├── RawrXD.Marketplace.psm1
    ├── RawrXD.Video.psm1
    ├── RawrXD.Security.psm1
    ├── RawrXD.Performance.psm1
    └── RawrXD.Utilities.psm1
```

## Running the Refactored Code

After refactoring, you can run the new modular version:

```powershell
.\Refactored\main.ps1
```

Or with parameters:

```powershell
.\Refactored\main.ps1 -CliMode -Command help
```

## Manifest File

The `refactoring-manifest.json` file contains:

- Original file path
- Refactoring timestamp
- Total function count
- Module breakdown with function lists
- Statistics

## Notes

- The refactoring preserves all original functionality
- Function signatures and logic remain unchanged
- Script-level variables and initialization code are preserved in main.ps1
- All functions are properly exported from their respective modules
- Module loading is automatic and handles errors gracefully

## Troubleshooting

### Module Loading Errors

If modules fail to load, check:
1. Module files exist in the `Modules` directory
2. PowerShell execution policy allows script execution
3. No syntax errors in generated modules

### Missing Functions

If functions are missing after refactoring:
1. Check the manifest file for function assignments
2. Review the refactoring log for categorization issues
3. Functions may be in the Utilities module if not categorized

## Example Output

```
[10:30:15] [Info] Starting enterprise refactoring process...
[10:30:15] [Info] Source: RawrXD.ps1
[10:30:15] [Info] Output: Refactored
[10:30:16] [Success] Created output directory structure
[10:30:20] [Info] Reading source file...
[10:30:20] [Info] Source file: 29870 lines
[10:30:25] [Info] Extracting functions...
[10:30:30] [Success] Found 294 functions
[10:30:35] [Info] Analyzing dependencies...
[10:30:40] [Info] Categorizing functions...
[10:30:45] [Info] Generating module files...
[10:30:50] [Success] Created module: RawrXD.Core (15 functions)
[10:30:50] [Success] Created module: RawrXD.Logging (12 functions)
...
[10:31:00] [Success] Created main.ps1
[10:31:00] [Success] Created refactoring manifest

=== REFACTORING SUMMARY ===
Total Functions: 294
Modules Created: 16

Module Breakdown:
  Agent : 8 functions
  AI : 15 functions
  Browser : 12 functions
  ...
```

## License

This refactoring tool is part of the RawrXD project.

