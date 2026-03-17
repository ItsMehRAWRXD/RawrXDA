#!/usr/bin/env pwsh
<#
.SYNOPSIS
MASM Port Deployment Package - Verify and package all components

.DESCRIPTION
Final verification and packaging of all MASM port components

#>

param(
    [switch]$Clean,
    [switch]$Package,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"
$ProjectRoot = "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader"

function Write-Header {
    param([string]$Message)
    Write-Host "`n╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║ $Message" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "✓ $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "✗ $Message" -ForegroundColor Red
}

function Verify-Files {
    Write-Header "File Verification"
    
    $files = @(
        "src/streaming_token_manager.h",
        "src/streaming_token_manager.cpp",
        "src/model_router.h",
        "src/model_router.cpp",
        "src/tool_registry.h",
        "src/simple_tool_registry.cpp",
        "src/agentic_planner.h",
        "src/agentic_planner.cpp",
        "src/command_palette.h",
        "src/command_palette.cpp",
        "src/diff_viewer.h",
        "src/diff_viewer.cpp",
        "src/masm_integration_manager.h",
        "src/masm_integration_manager.cpp",
        "src/component_test.cpp",
        "CMakeLists_masm_components.txt",
        "CMakeLists_complete.txt",
        "MASM_INTEGRATION_GUIDE.md",
        "MASM_IMPLEMENTATION_SUMMARY.md",
        "example_integration.cpp",
        "run_masm_port_tests.bat",
        "run_complete_integration.bat",
        "complete_masm_integration.ps1",
        "FINAL_INTEGRATION_PACKAGE.md"
    )
    
    $missing = @()
    foreach ($file in $files) {
        $path = Join-Path $ProjectRoot $file
        if (Test-Path $path) {
            Write-Success "$file"
        } else {
            $missing += $file
            Write-Error "$file (MISSING)"
        }
    }
    
    if ($missing.Count -gt 0) {
        Write-Host "`nMissing files: $($missing -join ', ')" -ForegroundColor Red
        return $false
    }
    
    return $true
}

function Count-Lines {
    Write-Header "Code Statistics"
    
    $srcFiles = Get-ChildItem "$ProjectRoot\src\*.cpp", "$ProjectRoot\src\*.h" -ErrorAction SilentlyContinue
    
    $totalLines = 0
    $codeLines = 0
    
    foreach ($file in $srcFiles) {
        $content = Get-Content $file
        $lines = $content.Count
        if ($lines -eq $null) { $lines = 1 }
        
        $commentLines = ($content | Where-Object { $_ -match '^\s*//|^\s*/\*|^\s*\*' }).Count
        $codeLines += ($lines - $commentLines)
        $totalLines += $lines
        
        Write-Host "$($file.Name): $lines lines"
    }
    
    Write-Host "`nTotal: $totalLines lines"
    Write-Host "Code: $codeLines lines"
    Write-Host "Documentation: $($totalLines - $codeLines) lines"
}

function Test-Build {
    Write-Header "Build Verification"
    
    $testExe = "$ProjectRoot\masm_test_build\build\Release\masm_port_test.exe"
    
    if (-not (Test-Path $testExe)) {
        Write-Error "Test executable not found. Run .\run_masm_port_tests.bat first"
        return $false
    }
    
    Write-Success "Test executable found"
    
    # Run tests
    $env:PATH = "C:\Qt\6.7.3\msvc2022_64\bin;$env:PATH"
    
    $output = & $testExe 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-Success "All tests passed"
        if ($Verbose) {
            Write-Host $output
        }
        return $true
    } else {
        Write-Error "Tests failed"
        Write-Host $output
        return $false
    }
}

function Create-Package {
    Write-Header "Creating Deployment Package"
    
    $packageDir = Join-Path $ProjectRoot "masm_port_package"
    
    if (Test-Path $packageDir) {
        Remove-Item -Recurse -Force $packageDir
    }
    
    New-Item -ItemType Directory -Path "$packageDir\src" -Force | Out-Null
    New-Item -ItemType Directory -Path "$packageDir\build" -Force | Out-Null
    New-Item -ItemType Directory -Path "$packageDir\docs" -Force | Out-Null
    
    # Copy sources
    Copy-Item "$ProjectRoot\src\*.h" "$packageDir\src\" -Force
    Copy-Item "$ProjectRoot\src\*.cpp" "$packageDir\src\" -Force
    
    # Copy CMakeLists
    Copy-Item "$ProjectRoot\CMakeLists_masm_components.txt" "$packageDir\" -Force
    Copy-Item "$ProjectRoot\CMakeLists_complete.txt" "$packageDir\" -Force
    
    # Copy docs
    Copy-Item "$ProjectRoot\MASM_INTEGRATION_GUIDE.md" "$packageDir\docs\" -Force
    Copy-Item "$ProjectRoot\MASM_IMPLEMENTATION_SUMMARY.md" "$packageDir\docs\" -Force
    Copy-Item "$ProjectRoot\FINAL_INTEGRATION_PACKAGE.md" "$packageDir\docs\" -Force
    Copy-Item "$ProjectRoot\example_integration.cpp" "$packageDir\docs\" -Force
    
    # Copy scripts
    Copy-Item "$ProjectRoot\run_masm_port_tests.bat" "$packageDir\" -Force
    Copy-Item "$ProjectRoot\run_complete_integration.bat" "$packageDir\" -Force
    
    # Create README
    $readme = @"
# MASM Port Integration Package

## Contents

- **src/**: Source files for all components
- **build/**: Build directory (CMake output)
- **docs/**: Documentation and examples
- **CMakeLists_*.txt**: Build configuration files
- **run_*.bat**: Test and integration scripts

## Quick Start

1. Build tests:
   ```
   .\run_masm_port_tests.bat
   ```

2. Run integration:
   ```
   .\run_complete_integration.bat
   ```

3. Read documentation:
   - docs/MASM_INTEGRATION_GUIDE.md
   - docs/FINAL_INTEGRATION_PACKAGE.md

## Integration into Your IDE

Include in CMakeLists.txt:
```cmake
include(CMakeLists_masm_components.txt)
target_link_libraries(your_app `${MASM_LIBRARY})
```

In your MainWindow:
```cpp
#include "masm_integration_manager.h"

MASMIntegrationManager* masm = new MASMIntegrationManager(this);
masm->initialize();
```

## Components

1. StreamingTokenManager - Real-time token streaming
2. ModelRouter - Model selection with mode flags
3. ToolRegistry - JSON-based tool calling
4. AgenticPlanner - Multi-step task execution
5. CommandPalette - Cmd-K style command interface
6. DiffViewer - Side-by-side code comparison
7. MASMIntegrationManager - One-step integration

## Support

All components include:
- Comprehensive inline documentation
- Signal/slot integration
- Error handling
- Qt6 compatibility

For detailed information, see docs/FINAL_INTEGRATION_PACKAGE.md
"@
    
    Set-Content -Path "$packageDir\README.md" -Value $readme
    
    Write-Success "Package created at: $packageDir"
    return $packageDir
}

function Create-CheckList {
    Write-Header "Integration Checklist"
    
    $checklist = @"
# MASM Port Integration Checklist

## Pre-Integration
- [ ] Review FINAL_INTEGRATION_PACKAGE.md
- [ ] Review MASM_INTEGRATION_GUIDE.md
- [ ] Review example_integration.cpp
- [ ] Run .\run_masm_port_tests.bat successfully
- [ ] Run .\run_complete_integration.bat successfully

## Integration
- [ ] Include CMakeLists_masm_components.txt in your CMakeLists.txt
- [ ] Add masm_integration_manager.h to your includes
- [ ] Create MASMIntegrationManager in your MainWindow constructor
- [ ] Call initialize() after MainWindow is created
- [ ] Link against masm_components library

## Testing
- [ ] Test Cmd-K (Ctrl+Shift+P) opens command palette
- [ ] Test Ctrl+T toggles thinking UI
- [ ] Test model mode selection from menu
- [ ] Test tool execution
- [ ] Test task execution with AgenticPlanner
- [ ] Test diff viewer with Accept/Reject

## Customization
- [ ] Register custom commands in CommandPalette
- [ ] Implement custom tool callbacks
- [ ] Connect to existing IDE features
- [ ] Test with real tasks
- [ ] Verify performance metrics

## Deployment
- [ ] Build release version of masm_components library
- [ ] Include all required Qt6 DLLs
- [ ] Test on target machine
- [ ] Verify all shortcuts work
- [ ] Verify menu integration
- [ ] Performance testing with large tasks

## Sign-Off
- [ ] Code review completed
- [ ] Testing completed
- [ ] Documentation reviewed
- [ ] Ready for production

---
Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
Status: READY FOR INTEGRATION
"@
    
    Set-Content -Path "$ProjectRoot\INTEGRATION_CHECKLIST.md" -Value $checklist
    Write-Success "Created INTEGRATION_CHECKLIST.md"
}

function Main {
    Write-Header "MASM Port - Final Verification & Packaging"
    
    # Verify files
    if (-not (Verify-Files)) {
        exit 1
    }
    
    # Count code
    Count-Lines
    
    # Test build
    if (-not (Test-Build)) {
        exit 1
    }
    
    # Create checklist
    Create-CheckList
    
    # Create package if requested
    if ($Package) {
        $packageDir = Create-Package
    }
    
    # Summary
    Write-Header "✓ Verification Complete"
    
    Write-Host "Status: ALL SYSTEMS GO" -ForegroundColor Green
    Write-Host "`nDeployment Ready:" -ForegroundColor Cyan
    Write-Host "  • Components: 7 ✓"
    Write-Host "  • Tests: ALL PASS ✓"
    Write-Host "  • Documentation: COMPLETE ✓"
    Write-Host "  • Build: VERIFIED ✓"
    
    Write-Host "`nNext Steps:" -ForegroundColor Green
    Write-Host "  1. Review docs/FINAL_INTEGRATION_PACKAGE.md"
    Write-Host "  2. Include CMakeLists_masm_components.txt in your IDE"
    Write-Host "  3. Add MASMIntegrationManager to MainWindow"
    Write-Host "  4. Link masm_components library"
    Write-Host "  5. Test with real tasks"
    
    if ($Package) {
        Write-Host "`nPackage Location: $packageDir" -ForegroundColor Yellow
    }
    
    Write-Host ""
}

Main
