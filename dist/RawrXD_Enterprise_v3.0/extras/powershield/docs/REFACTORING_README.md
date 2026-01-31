# RawrXD Script Refactoring Guide

## Overview

The `Refactor-RawrXD.ps1` script is an enterprise-grade tool that automatically breaks down the monolithic `RawrXD.ps1` (29,870+ lines) into a modular architecture with proper separation of concerns.

## Features

✅ **Automatic Function Detection** - Identifies all functions in the source script  
✅ **Intelligent Categorization** - Groups functions into logical modules based on patterns  
✅ **Dependency Resolution** - Tracks function dependencies and import ordering  
✅ **Module Generation** - Creates proper PowerShell module files (.psm1)  
✅ **Main Script Orchestration** - Generates a main script that imports all modules  
✅ **Manifest Creation** - Produces JSON manifest with module structure  
✅ **Preservation Mode** - Optionally preserves original structure and comments  

## Module Categories

The refactoring tool organizes functions into the following modules:

| Module | Description | Priority |
|--------|-------------|----------|
| `RawrXD.Core` | Core initialization, configuration, entry point | 1 |
| `RawrXD.Logging` | Logging and error handling infrastructure | 2 |
| `RawrXD.FileSystem` | File/directory operations and explorer | 3 |
| `RawrXD.Editor` | Text editor and syntax highlighting | 4 |
| `RawrXD.GUI` | Windows Forms GUI components | 5 |
| `RawrXD.CLI` | Command-line interface and console | 6 |
| `RawrXD.AI` | Ollama integration and AI chat | 7 |
| `RawrXD.Git` | Git version control integration | 8 |
| `RawrXD.Browser` | WebView2 browser and web integration | 9 |
| `RawrXD.Marketplace` | Extension marketplace management | 10 |
| `RawrXD.Agent` | Agent task automation | 11 |
| `RawrXD.Settings` | Settings and configuration | 12 |
| `RawrXD.Terminal` | Integrated terminal functionality | 13 |
| `RawrXD.Utilities` | Utility functions and helpers | 99 |

## Usage

### Basic Usage

```powershell
# Run refactoring with defaults
.\Refactor-RawrXD.ps1

# This will:
# - Read RawrXD.ps1 from current directory
# - Create Modules/ directory
# - Generate RawrXD-Main.ps1
```

### Advanced Usage

```powershell
# Custom source and output
.\Refactor-RawrXD.ps1 `
    -SourceScript "C:\Path\To\RawrXD.ps1" `
    -OutputDirectory "RefactoredModules" `
    -MainScriptName "Main.ps1"

# Without structure preservation (cleaner code)
.\Refactor-RawrXD.ps1 -PreserveStructure:$false
```

### Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `-SourceScript` | Path to source PowerShell script | `RawrXD.ps1` |
| `-OutputDirectory` | Directory for generated modules | `Modules` |
| `-MainScriptName` | Name of main orchestration script | `RawrXD-Main.ps1` |
| `-PreserveStructure` | Preserve original code structure | `$true` |

## Output Structure

After refactoring, you'll have:

```
Powershield/
├── RawrXD-Main.ps1          # Main orchestration script
├── RawrXD.ps1               # Original (backup recommended)
└── Modules/                 # Module directory
    ├── ModuleManifest.json  # Module structure manifest
    ├── RawrXD.Core.psm1
    ├── RawrXD.Logging.psm1
    ├── RawrXD.FileSystem.psm1
    ├── RawrXD.Editor.psm1
    ├── RawrXD.GUI.psm1
    ├── RawrXD.CLI.psm1
    ├── RawrXD.AI.psm1
    ├── RawrXD.Git.psm1
    ├── RawrXD.Browser.psm1
    ├── RawrXD.Marketplace.psm1
    ├── RawrXD.Agent.psm1
    ├── RawrXD.Settings.psm1
    ├── RawrXD.Terminal.psm1
    └── RawrXD.Utilities.psm1
```

## Module Format

Each generated module follows this structure:

```powershell
<#
.SYNOPSIS
    Module description
#>

#region Module Setup
# Module initialization code
#endregion

#region Functions
# All functions in this module
#endregion

#region Module Export
Export-ModuleMember -Function @(...)
#endregion
```

## Main Script Structure

The generated main script:

1. **Imports all modules** in dependency order
2. **Preserves original parameters** and CLI mode logic
3. **Initializes core functionality**
4. **Routes to GUI or CLI mode** as appropriate

## Dependency Resolution

The refactoring tool automatically:

- ✅ Detects function calls within functions
- ✅ Identifies cross-module dependencies
- ✅ Orders module imports correctly
- ✅ Handles circular dependency warnings

## Post-Refactoring Steps

### 1. Review Generated Modules

```powershell
# Check module manifest
Get-Content Modules\ModuleManifest.json | ConvertFrom-Json
```

### 2. Test Main Script

```powershell
# Test CLI mode
.\RawrXD-Main.ps1 -CliMode -Command help

# Test GUI mode
.\RawrXD-Main.ps1
```

### 3. Adjust Module Boundaries

If functions are miscategorized, you can:

1. Edit the module file directly
2. Move functions between modules
3. Update `ModuleManifest.json`
4. Re-run refactoring if needed

### 4. Handle Edge Cases

Some manual fixes may be needed:

- **Script-level variables** - May need to be moved to a config module
- **Global state** - May need refactoring for module isolation
- **Event handlers** - May need special handling
- **Dynamic function calls** - May need wrapper functions

## Troubleshooting

### Issue: Module import fails

**Solution**: Check module syntax errors:
```powershell
$PSModuleAutoLoadingPreference = 'None'
Import-Module .\Modules\RawrXD.Core.psm1 -Force -Verbose
```

### Issue: Functions not found

**Solution**: Verify exports in module:
```powershell
Get-Module RawrXD.Core | Select-Object -ExpandProperty ExportedFunctions
```

### Issue: Circular dependencies

**Solution**: The tool detects these. Move shared functions to `RawrXD.Utilities` module.

### Issue: Missing dependencies

**Solution**: Manually add missing imports or move functions to correct modules.

## Benefits of Modular Architecture

✅ **Maintainability** - Easier to find and modify specific functionality  
✅ **Testability** - Modules can be tested independently  
✅ **Reusability** - Modules can be used in other projects  
✅ **Performance** - Only load modules you need  
✅ **Collaboration** - Multiple developers can work on different modules  
✅ **Documentation** - Each module is self-contained and documented  

## Limitations

⚠️ **Dynamic Code** - Functions called via `Invoke-Expression` may not be detected  
⚠️ **Script Blocks** - Functions defined in script blocks need manual handling  
⚠️ **Assembly Loading** - .NET type loading may need module-specific imports  
⚠️ **Event Handlers** - May need refactoring for module scope  

## Best Practices

1. **Backup First** - Always backup `RawrXD.ps1` before refactoring
2. **Test Incrementally** - Test each module as you adjust it
3. **Version Control** - Commit the original before refactoring
4. **Document Changes** - Update module documentation as needed
5. **Preserve Functionality** - Ensure all features still work post-refactoring

## Advanced Customization

### Custom Module Categories

Edit the `$ModuleCategories` hashtable in `Refactor-RawrXD.ps1`:

```powershell
$ModuleCategories['Custom'] = @{
    Name = 'RawrXD.Custom'
    Description = 'Custom functionality'
    Patterns = @('Custom-', 'My-')
    Priority = 20
}
```

### Custom Function Patterns

Add patterns to categorize functions differently:

```powershell
'GUI' = @{
    Patterns = @('Show-', 'Dialog', 'Form', 'Button', 'MyCustom-')
}
```

## Support

For issues or questions:
1. Check the `ModuleManifest.json` for module structure
2. Review generated module files for syntax errors
3. Test individual modules with `Import-Module -Force -Verbose`

---

**Generated by Refactor-RawrXD.ps1**  
**Part of the RawrXD Enterprise Modular Architecture**

