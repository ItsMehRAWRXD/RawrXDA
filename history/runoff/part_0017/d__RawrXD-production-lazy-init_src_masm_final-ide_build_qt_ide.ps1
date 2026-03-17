#==========================================================================
# build_qt_ide.ps1 - Build Script for RAWR1024 Autonomous ML IDE
# ==========================================================================
# Builds MASM engine modules and Qt UI integration layer
#==========================================================================

param(
    [string]$Action = "all",
    [string]$QtPath = "C:\\Qt\\6.7.0\\msvc2019_64",
    [string]$VSPath = "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools",
    [switch]$Debug = $false
)

# Configuration
$MASMPath = "$VSPath\\VC\\Tools\\MSVC\\14.44.35207\\bin\\Hostx64\\x64"
$ML64 = "$MASMPath\\ml64.exe"
$Linker = "$MASMPath\\link.exe"
$QMake = "$QtPath\\bin\\qmake.exe"
$Make = "nmake.exe"

# Colors for output
$Green = "Green"
$Yellow = "Yellow"
$Red = "Red"
$Cyan = "Cyan"

function Write-ColorOutput($Color, $Text) {
    Write-Host $Text -ForegroundColor $Color
}

function Test-Tool($ToolPath, $ToolName) {
    if (Test-Path $ToolPath) {
        Write-ColorOutput $Green "✓ $ToolName found: $ToolPath"
        return $true
    } else {
        Write-ColorOutput $Red "✗ $ToolName not found: $ToolPath"
        return $false
    }
}

function Build-MASM-Modules {
    Write-ColorOutput $Cyan "Building MASM engine modules..."
    
    $Modules = @(
        "rawr1024_dual_engine_custom.asm",
        "rawr1024_model_streaming.asm", 
        "rawr1024_ide_menu_integration.asm",
        "rawr1024_full_integration.asm"
    )
    
    foreach ($Module in $Modules) {
        Write-ColorOutput $Yellow "  Compiling $Module..."
        
        $ObjFile = $Module.Replace(".asm", ".obj")
        $Args = "/c", "/nologo", $Module
        
        $Result = & $ML64 $Args 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-ColorOutput $Green "    ✓ $ObjFile created"
        } else {
            Write-ColorOutput $Red "    ✗ Failed to compile $Module"
            Write-Host $Result
            return $false
        }
    }
    
    Write-ColorOutput $Green "✓ All MASM modules compiled successfully"
    return $true
}

function Build-Qt-IDE {
    Write-ColorOutput $Cyan "Building Qt IDE integration..."
    
    # Generate Makefile
    Write-ColorOutput $Yellow "  Running qmake..."
    $Result = & $QMake RAWR1024_QT_IDE.pro 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-ColorOutput $Red "✗ qmake failed"
        Write-Host $Result
        return $false
    }
    
    # Build with nmake
    Write-ColorOutput $Yellow "  Building with nmake..."
    if ($Debug) {
        $Result = & $Make debug 2>&1
    } else {
        $Result = & $Make release 2>&1
    }
    
    if ($LASTEXITCODE -eq 0) {
        Write-ColorOutput $Green "✓ Qt IDE built successfully"
        
        # Check if executable was created
        if ($Debug) {
            $ExeName = "rawr1024_qt_ide_debug.exe"
        } else {
            $ExeName = "rawr1024_qt_ide.exe"
        }
        
        if (Test-Path $ExeName) {
            $Size = (Get-Item $ExeName).Length
            Write-ColorOutput $Green "  Executable: $ExeName ($Size bytes)"
        }
        
        return $true
    } else {
        Write-ColorOutput $Red "✗ Build failed"
        Write-Host $Result
        return $false
    }
}

function Create-Standalone-Executable {
    Write-ColorOutput $Cyan "Creating standalone executable..."
    
    # Link MASM modules into standalone test
    $Args = @(
        "/ENTRY:main",
        "/SUBSYSTEM:CONSOLE", 
        "/OUT:rawr1024_standalone_test.exe",
        "rawr1024_dual_engine_custom.obj",
        "rawr1024_model_streaming.obj",
        "rawr1024_ide_menu_integration.obj",
        "rawr1024_full_integration.obj",
        "rawr1024_integration_test.obj"
    )
    
    $Result = & $Linker $Args 2>&1
    if ($LASTEXITCODE -eq 0 -and (Test-Path "rawr1024_standalone_test.exe")) {
        $Size = (Get-Item "rawr1024_standalone_test.exe").Length
        Write-ColorOutput $Green "✓ Standalone test created: rawr1024_standalone_test.exe ($Size bytes)"
        return $true
    } else {
        Write-ColorOutput $Red "✗ Failed to create standalone executable"
        Write-Host $Result
        return $false
    }
}

function Run-Tests {
    Write-ColorOutput $Cyan "Running integration tests..."
    
    if (Test-Path "rawr1024_standalone_test.exe") {
        $Result = .\rawr1024_standalone_test.exe
        if ($LASTEXITCODE -eq 0) {
            Write-ColorOutput $Green "✓ All integration tests passed"
            return $true
        } else {
            Write-ColorOutput $Red "✗ Tests failed with exit code $LASTEXITCODE"
            Write-Host $Result
            return $false
        }
    } else {
        Write-ColorOutput $Yellow "⚠ No test executable found, skipping tests"
        return $true
    }
}

function Clean-Build {
    Write-ColorOutput $Cyan "Cleaning build artifacts..."
    
    $FilesToRemove = @(
        "*.obj",
        "*.exe", 
        "Makefile",
        "Makefile.Debug",
        "Makefile.Release",
        "*.o",
        "debug\\",
        "release\\"
    )
    
    foreach ($Pattern in $FilesToRemove) {
        Remove-Item $Pattern -Force -ErrorAction SilentlyContinue
    }
    
    Write-ColorOutput $Green "✓ Build cleaned"
}

# Main execution
Write-ColorOutput $Cyan "=============================================="
Write-ColorOutput $Cyan "RAWR1024 Autonomous ML IDE Build System"
Write-ColorOutput $Cyan "=============================================="

# Check required tools
$Tools = @(
    @("$ML64", "MASM Compiler (ml64.exe)"),
    @("$Linker", "Linker (link.exe)"),
    @("$QMake", "Qt qmake"),
    @("$Make", "nmake")
)

$AllToolsFound = $true
foreach ($Tool in $Tools) {
    if (-not (Test-Tool $Tool[0] $Tool[1])) {
        $AllToolsFound = $false
    }
}

if (-not $AllToolsFound) {
    Write-ColorOutput $Red "Required tools missing. Please check paths and install dependencies."
    exit 1
}

# Execute requested action
switch ($Action.ToLower()) {
    "masm" {
        if (-not (Build-MASM-Modules)) { exit 1 }
    }
    "qt" {
        if (-not (Build-Qt-IDE)) { exit 1 }
    }
    "standalone" {
        if (-not (Build-MASM-Modules)) { exit 1 }
        if (-not (Create-Standalone-Executable)) { exit 1 }
    }
    "test" {
        if (-not (Run-Tests)) { exit 1 }
    }
    "clean" {
        Clean-Build
    }
    "all" {
        if (-not (Build-MASM-Modules)) { exit 1 }
        if (-not (Create-Standalone-Executable)) { exit 1 }
        if (-not (Run-Tests)) { exit 1 }
        if (-not (Build-Qt-IDE)) { exit 1 }
    }
    default {
        Write-ColorOutput $Red "Unknown action: $Action"
        Write-ColorOutput $Yellow "Valid actions: masm, qt, standalone, test, clean, all"
        exit 1
    }
}

Write-ColorOutput $Green "=============================================="
Write-ColorOutput $Green "Build completed successfully!"
Write-ColorOutput $Green "=============================================="

# Show final artifacts
Write-ColorOutput $Cyan "Generated artifacts:"
Get-ChildItem *.exe | ForEach-Object {
    Write-ColorOutput $Green "  $($_.Name) ($($_.Length) bytes)"
}

if (Test-Path "rawr1024_qt_ide.exe") {
    Write-ColorOutput $Cyan ""
    Write-ColorOutput $Green "🎉 AUTONOMOUS ML IDE READY!"
    Write-ColorOutput $Yellow "Run: .\\rawr1024_qt_ide.exe"
    Write-ColorOutput $Yellow "Features: 8-engine architecture, GGUF streaming, memory management, hotpatching"
}