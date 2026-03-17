#requires -version 5.0
<#
.SYNOPSIS
Link compiled RAWR1024 object files into production RawrXD_IDE.exe executable

.DESCRIPTION
This script links 11 compiled .obj files from the MASM final-ide build into a 
production-ready RawrXD_IDE.exe executable. Requires ML64.EXE (Microsoft Macro Assembler linker).

.PARAMETER Action
Build action: 'link' (create exe), 'verify' (check obj files), 'clean' (remove exe), 'full' (verify + link)

.PARAMETER OutputDir
Output directory for RawrXD_IDE.exe (default: current directory)

.PARAMETER Verbose
Enable verbose output

.EXAMPLE
.\Link-RAWR1024-IDE.ps1 -Action full
.\Link-RAWR1024-IDE.ps1 -Action link -OutputDir ".\bin"

.NOTES
Requires:
- ML64.EXE (MASM x64 linker) in PATH
- Qt 6.7.3 libraries (or adjust paths in script)
- Windows SDK libraries
- All .obj files present in .\obj directory
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('verify', 'link', 'clean', 'full')]
    [string]$Action = 'full',
    
    [Parameter(Mandatory=$false)]
    [string]$OutputDir = '.',
    
    [Parameter(Mandatory=$false)]
    [switch]$Verbose
)

# Configuration
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ObjDir = Join-Path $ScriptRoot "obj"
$OutputExe = Join-Path $OutputDir "RawrXD_IDE.exe"

# Required object files (in dependency order)
$ObjectFiles = @(
    "main_masm.obj",              # Entry point
    "qt6_foundation.obj",          # Qt6 foundation
    "qt6_main_window.obj",         # Main window
    "qt6_syntax_highlighter.obj",  # Syntax
    "qt6_text_editor.obj",         # Editor
    "qt6_statusbar.obj",           # Status bar
    "asm_events.obj",              # Events
    "asm_log.obj",                 # Logging
    "asm_memory.obj",              # Memory
    "asm_string.obj",              # Strings
    "malloc_wrapper.obj"           # Allocation
)

# Optional GPU object files (if compiled)
$GpuObjectFiles = @(
    "rawr1024_gpu_universal.obj",
    "rawr1024_model_streaming.obj"
)

# Windows SDK libraries (adjust path if needed)
$WindowsLibs = @(
    "kernel32.lib",
    "user32.lib",
    "gdi32.lib",
    "shell32.lib",
    "winuser.lib",
    "comctl32.lib"
)

# Qt6 libraries (adjust path if Qt installed elsewhere)
$Qt6Path = "C:\Qt\6.7.3"
$Qt6Libs = @(
    "$Qt6Path\lib\Qt6Core.lib",
    "$Qt6Path\lib\Qt6Gui.lib",
    "$Qt6Path\lib\Qt6Widgets.lib",
    "$Qt6Path\lib\Qt6Network.lib"
)

# Utility functions
function Write-Status {
    param([string]$Message, [string]$Status = "INFO")
    $color = @{
        "INFO"    = "Cyan"
        "SUCCESS" = "Green"
        "WARNING" = "Yellow"
        "ERROR"   = "Red"
    }
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] [$Status] $Message" -ForegroundColor $color[$Status]
}

function Test-ObjectFiles {
    Write-Status "Verifying object files..." "INFO"
    
    $allFound = $true
    $totalSize = 0
    $foundCount = 0
    
    foreach ($obj in $ObjectFiles) {
        $path = Join-Path $ObjDir $obj
        if (Test-Path $path -PathType Leaf) {
            $size = (Get-Item $path).Length
            $sizeKB = [math]::Round($size / 1KB, 2)
            $foundCount++
            $totalSize += $size
            Write-Host "  ✓ $obj ($sizeKB KB)"
            if ($Verbose) { Write-Host "    Path: $path" }
        } else {
            Write-Host "  ✗ $obj (MISSING)" -ForegroundColor Red
            $allFound = $false
        }
    }
    
    # Check optional GPU files
    Write-Status "Checking optional GPU acceleration files..." "INFO"
    foreach ($obj in $GpuObjectFiles) {
        $path = Join-Path $ObjDir $obj
        if (Test-Path $path -PathType Leaf) {
            $size = (Get-Item $path).Length
            $sizeKB = [math]::Round($size / 1KB, 2)
            $foundCount++
            $totalSize += $size
            Write-Host "  ✓ $obj (GPU module, $sizeKB KB)"
        } else {
            Write-Host "  ○ $obj (optional, will skip)"
        }
    }
    
    $totalSizeMB = [math]::Round($totalSize / 1MB, 3)
    Write-Status "Object file verification: $foundCount files found, $totalSizeMB MB total" "SUCCESS"
    
    return $allFound
}

function Find-Linker {
    Write-Status "Locating linker (link.exe)..." "INFO"
    
    # Try to find link.exe in common locations
    $linkerPaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe",
        "link.exe"
    )
    
    foreach ($pattern in $linkerPaths) {
        $linker = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($linker) {
            Write-Status "Found linker: $($linker.FullName)" "SUCCESS"
            return $linker.FullName
        }
    }
    
    Write-Status "Linker not found in common paths. Try 'link.exe' from PATH" "WARNING"
    return "link.exe"
}

function Build-LinkCommand {
    param([array]$Objects, [array]$Libs)
    
    # Build object file list
    $objStr = ($Objects | ForEach-Object { "`"$_`"" }) -join " "
    
    # Build library list
    $libStr = ($Libs | ForEach-Object { "`"$_`"" }) -join " "
    
    # Build full command
    $cmd = "link.exe /OUT:`"$OutputExe`" " +
           "/SUBSYSTEM:WINDOWS /MACHINE:X64 " +
           "/DEBUG /INCREMENTAL " +
           "/DEFAULTLIB:libcmt.lib " +
           "$objStr $libStr"
    
    return $cmd
}

function Invoke-Linking {
    Write-Status "Building link command..." "INFO"
    
    # Verify linker exists
    $linker = Find-Linker
    
    # Collect all objects to link
    $objs = @()
    foreach ($obj in $ObjectFiles) {
        $objs += Join-Path $ObjDir $obj
    }
    
    # Add GPU files if present
    foreach ($obj in $GpuObjectFiles) {
        $path = Join-Path $ObjDir $obj
        if (Test-Path $path -PathType Leaf) {
            $objs += $path
        }
    }
    
    # Filter Qt libs based on what's available
    $libs = @($WindowsLibs)
    foreach ($qtLib in $Qt6Libs) {
        if (Test-Path $qtLib -PathType Leaf) {
            $libs += $qtLib
        } else {
            Write-Status "Qt6 library not found: $qtLib (will continue with available libs)" "WARNING"
        }
    }
    
    # Build and display command
    $cmd = Build-LinkCommand $objs $libs
    Write-Status "Executing linker command..." "INFO"
    if ($Verbose) {
        Write-Host "`n$cmd`n"
    }
    
    # Execute linking
    try {
        # Change to obj directory for relative paths
        Push-Location $ObjDir
        $objNames = $ObjectFiles + $GpuObjectFiles | Where-Object { Test-Path $_ }
        
        $linkCmd = "link.exe /OUT:`"$OutputExe`" " +
                  "/SUBSYSTEM:WINDOWS /MACHINE:X64 " +
                  "/DEBUG /INCREMENTAL " +
                  "/DEFAULTLIB:libcmt.lib " +
                  ($objNames -join " ") + " " +
                  ($libs | ForEach-Object { "`"$_`"" } | Select-Object -Unique) -join " "
        
        Write-Host "Executing: $linkCmd" -ForegroundColor Cyan
        Invoke-Expression $linkCmd
        
        if ($LASTEXITCODE -eq 0) {
            if (Test-Path $OutputExe -PathType Leaf) {
                $exeSize = (Get-Item $OutputExe).Length
                $exeSizeMB = [math]::Round($exeSize / 1MB, 2)
                Write-Status "✓ Executable created successfully: $OutputExe ($exeSizeMB MB)" "SUCCESS"
                return $true
            }
        } else {
            Write-Status "Linking failed with exit code: $LASTEXITCODE" "ERROR"
            return $false
        }
    } catch {
        Write-Status "Error during linking: $_" "ERROR"
        return $false
    } finally {
        Pop-Location
    }
}

function Test-Executable {
    Write-Status "Testing executable..." "INFO"
    
    if (Test-Path $OutputExe -PathType Leaf) {
        Write-Status "✓ Executable file exists and is accessible" "SUCCESS"
        
        # Try to get file info
        $exeInfo = Get-Item $OutputExe
        Write-Host "  Size: $([math]::Round($exeInfo.Length / 1MB, 2)) MB"
        Write-Host "  Created: $($exeInfo.CreationTime)"
        Write-Host "  Modified: $($exeInfo.LastWriteTime)"
        
        return $true
    } else {
        Write-Status "✗ Executable not found at: $OutputExe" "ERROR"
        return $false
    }
}

# Main execution
Write-Status "RawrXD IDE Linking Utility v1.0" "INFO"
Write-Status "Action: $Action | Output: $OutputDir" "INFO"
Write-Status ""

# Create output directory if needed
if (-not (Test-Path $OutputDir -PathType Container)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    Write-Status "Created output directory: $OutputDir" "INFO"
}

# Execute requested action
switch ($Action) {
    'verify' {
        $result = Test-ObjectFiles
        exit ($result ? 0 : 1)
    }
    
    'clean' {
        if (Test-Path $OutputExe) {
            Remove-Item $OutputExe -Force
            Write-Status "Removed: $OutputExe" "SUCCESS"
        }
        exit 0
    }
    
    'link' {
        $result = Invoke-Linking
        if ($result) {
            Test-Executable
            exit 0
        } else {
            exit 1
        }
    }
    
    'full' {
        $verified = Test-ObjectFiles
        Write-Status ""
        
        if ($verified) {
            $linked = Invoke-Linking
            Write-Status ""
            
            if ($linked) {
                Test-Executable
                Write-Status ""
                Write-Status "✓ RAWR1024 IDE linking completed successfully!" "SUCCESS"
                Write-Status "Next: Run 'RawrXD_IDE.exe' to verify functionality" "INFO"
                exit 0
            }
        }
        
        Write-Status "✗ Linking process failed" "ERROR"
        exit 1
    }
}
