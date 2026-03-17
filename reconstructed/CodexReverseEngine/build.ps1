# CODEX REVERSE ENGINE ULTIMATE v7.0
# Professional PE Analysis & Build System
# ============================================

param(
    [switch]$Clean,
    [switch]$Debug
)

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "CODEX REVERSE ENGINE ULTIMATE v7.0" -ForegroundColor Cyan
Write-Host "Professional PE Analysis & Build System" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Configuration
$ProjectName = "CodexUltimate"
$SourceFile = "CodexUltimate.asm"
$OutputDir = "bin"
$IncludeDir = "include"

# MASM64 Paths (Multiple possible locations)
$MASM64Paths = @(
    "C:\masm64\bin64\ml64.exe",
    "D:\masm64\bin64\ml64.exe",
    "E:\masm64\bin64\ml64.exe",
    "C:\Program Files\MASM64\bin64\ml64.exe",
    "D:\Program Files\MASM64\bin64\ml64.exe",
    "C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
)

# Find MASM64
$ML64 = $null
foreach ($Path in $MASM64Paths) {
    if ($Path -like "*\*\*") {
        # Handle wildcard paths
        $Resolved = Resolve-Path -Path $Path -ErrorAction SilentlyContinue
        if ($Resolved) {
            $ML64 = $Resolved.Path
            break
        }
    } else {
        if (Test-Path $Path) {
            $ML64 = $Path
            break
        }
    }
}

if (-not $ML64) {
    Write-Host "ERROR: MASM64 (ml64.exe) not found!" -ForegroundColor Red
    Write-Host "Please install MASM64 or Visual Studio with C++ workload." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Download MASM64 from: http://www.masm32.com/download.htm" -ForegroundColor Cyan
    Write-Host "Or install Visual Studio 2022 with 'Desktop development with C++'" -ForegroundColor Cyan
    exit 1
}

$ML64Path = Split-Path $ML64 -Parent
Write-Host "Found MASM64: $ML64" -ForegroundColor Green

# Create output directories
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
    Write-Host "Created output directory: $OutputDir" -ForegroundColor Green
}

if (-not (Test-Path $IncludeDir)) {
    New-Item -ItemType Directory -Path $IncludeDir | Out-Null
    Write-Host "Created include directory: $IncludeDir" -ForegroundColor Green
}

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    Remove-Item -Path "$OutputDir\*" -Force -ErrorAction SilentlyContinue
    Remove-Item -Path "$IncludeDir\*" -Force -ErrorAction SilentlyContinue
}

# Build configuration
$ObjFile = "$OutputDir\${ProjectName}.obj"
$ExeFile = "$OutputDir\${ProjectName}.exe"
$MapFile = "$OutputDir\${ProjectName}.map"

# MASM64 include paths
$MASM64Root = Split-Path $ML64Path -Parent
$IncludePaths = @(
    "$MASM64Root\include64",
    ".\include"
)

# Build command
$BuildArgs = @(
    $SourceFile,
    "/c",                           # Compile only
    "/Fo$ObjFile",                 # Object file output
    "/Zi",                        # Debug info
    "/W3"                         # Warning level 3
)

# Add include paths
foreach ($IncPath in $IncludePaths) {
    if (Test-Path $IncPath) {
        $BuildArgs += "/I$IncPath"
    }
}

Write-Host ""
Write-Host "Assembling $SourceFile..." -ForegroundColor Yellow
Write-Host "Command: ml64.exe $($BuildArgs -join ' ')" -ForegroundColor Gray

# Execute MASM64
& $ML64 @BuildArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Assembly failed with code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Assembly successful!" -ForegroundColor Green

# Linking
Write-Host ""
Write-Host "Linking $ObjFile..." -ForegroundColor Yellow

# Find link.exe
$LinkPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe"
)

$Link = $null
foreach ($Path in $LinkPaths) {
    $Resolved = Resolve-Path -Path $Path -ErrorAction SilentlyContinue
    if ($Resolved) {
        $Link = $Resolved.Path
        break
    }
}

if (-not $Link) {
    Write-Host "WARNING: link.exe not found, skipping linking phase" -ForegroundColor Yellow
    Write-Host "Object file created at: $ObjFile" -ForegroundColor Cyan
    exit 0
}

$LinkArgs = @(
    $ObjFile,
    "/OUT:$ExeFile",                # Output executable
    "/SUBSYSTEM:CONSOLE",           # Console application
    "/ENTRY:main",                  # Entry point
    "/MACHINE:X64",                 # 64-bit target
    "/MAP:$MapFile",                # Generate map file
    "/DEBUG"                        # Debug information
)

# Add libraries
$LibPaths = @(
    "$MASM64Root\lib64\kernel32.lib",
    "$MASM64Root\lib64\user32.lib",
    "$MASM64Root\lib64\advapi32.lib",
    "$MASM64Root\lib64\shlwapi.lib",
    "$MASM64Root\lib64\psapi.lib",
    "$MASM64Root\lib64\dbghelp.lib"
)

foreach ($Lib in $LibPaths) {
    if (Test-Path $Lib) {
        $LinkArgs += $Lib
    } else {
        # Try without path
        $LibName = Split-Path $Lib -Leaf
        $LinkArgs += $LibName
    }
}

Write-Host "Command: link.exe $($LinkArgs -join ' ')" -ForegroundColor Gray

# Execute linker
& $Link @LinkArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Linking failed with code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Executable: $ExeFile" -ForegroundColor Cyan
Write-Host "Map File: $MapFile" -ForegroundColor Cyan
Write-Host ""
Write-Host "To run the tool:" -ForegroundColor Yellow
Write-Host "  .\$OutputDir\$ProjectName.exe" -ForegroundColor White
Write-Host ""
Write-Host "Usage:" -ForegroundColor Yellow
Write-Host "  1. Single file analysis" -ForegroundColor White
Write-Host "  2. Directory batch processing" -ForegroundColor White
Write-Host "  3. Exit" -ForegroundColor White
Write-Host ""

# Verify executable
if (Test-Path $ExeFile) {
    $FileInfo = Get-Item $ExeFile
    Write-Host "File size: $([math]::Round($FileInfo.Length / 1KB, 2)) KB" -ForegroundColor Green
    Write-Host "Created: $($FileInfo.CreationTime)" -ForegroundColor Green
} else {
    Write-Host "WARNING: Executable not found at expected location" -ForegroundColor Yellow
}

exit 0
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Assembly failed!" -ForegroundColor Red
    exit 1
}
Write-Host "SUCCESS: Assembly completed." -ForegroundColor Green
Write-Host ""

# Step 2: Link
Write-Host "[2/3] Linking CodexUltimate.obj..." -ForegroundColor Yellow
& "$ml64Path\link.exe" CodexUltimate.obj /subsystem:console /entry:main /out:CodexUltimate.exe
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Linking failed!" -ForegroundColor Red
    exit 1
}
Write-Host "SUCCESS: Linking completed." -ForegroundColor Green
Write-Host ""

# Step 3: Verify
Write-Host "[3/3] Verifying build..." -ForegroundColor Yellow
if (Test-Path "CodexUltimate.exe") {
    Write-Host "SUCCESS: CodexUltimate.exe created successfully!" -ForegroundColor Green
    Write-Host ""
    Get-Item CodexUltimate.exe | Format-List Name, Length, LastWriteTime
    Write-Host ""
    Write-Host "Build completed at $(Get-Date -Format 'HH:mm:ss') on $(Get-Date -Format 'yyyy-MM-dd')" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "To run the tool:" -ForegroundColor Cyan
    Write-Host "  .\CodexUltimate.exe" -ForegroundColor White
    Write-Host ""
    Write-Host "To analyze a directory:" -ForegroundColor Cyan
    Write-Host "  1. Select option 2 (Directory Mode)" -ForegroundColor White
    Write-Host "  2. Input: C:\Path\To\Target" -ForegroundColor White
    Write-Host "  3. Output: C:\Reversed\Output" -ForegroundColor White
    Write-Host "  4. Project: YourProjectName" -ForegroundColor White
} else {
    Write-Host "ERROR: CodexUltimate.exe not found!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "BUILD COMPLETE" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan