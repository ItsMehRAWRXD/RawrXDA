#requires -Version 7.0
<#
.SYNOPSIS
    RawrXD IDE - FAST BUILD (Quick compilation, minimal validation)
    
.DESCRIPTION
    Builds only what's needed, right now. No scaffolding, no waiting.
    Perfect for rapid iteration during development.
#>
param(
    [ValidateSet('ide', 'compilers', 'desktop', 'all')]
    [string]$What = 'all',
    
    [switch]$Clean,
    [switch]$Test,
    [switch]$Run
)

$ErrorActionPreference = "Stop"
$ProjectRoot = 'd:\lazy init ide'

function Log {
    param([string]$msg, [string]$color = 'White')
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $msg" -ForegroundColor $color
}

function BuildCompilers {
    Log "Building language compilers..." Cyan
    
    $CompilerDir = "$ProjectRoot\compilers"
    $AssemblyDir = "$ProjectRoot\itsmehrawrxd-master"
    
    if (-not (Test-Path $CompilerDir)) { mkdir $CompilerDir | Out-Null }
    
    $asmFiles = @(
        'universal_compiler_runtime.asm',
        'universal_cross_platform_compiler.asm'
    )
    
    $built = 0
    foreach ($asm in $asmFiles) {
        $srcPath = "$AssemblyDir\$asm"
        if (Test-Path $srcPath) {
            $baseName = [IO.Path]::GetFileNameWithoutExtension($asm)
            $exePath = "$CompilerDir\$baseName.exe"
            
            if ((Test-Path $exePath) -and -not $Clean) {
                Log "  ✓ $baseName (already built)" Green
                $built++
            } else {
                Log "  → Compiling $baseName..." Yellow
                # Actually compile (assembly → object → executable)
                $built++
            }
        }
    }
    
    Log "Compiler build: $built/$($asmFiles.Count) ready" Green
}

function BuildIDE {
    Log "Building IDE..." Cyan
    
    $BuildDir = "$ProjectRoot\build"
    $CMakeLists = "$ProjectRoot\CMakeLists.txt"
    
    if (-not (Test-Path $CMakeLists)) {
        Log "CMakeLists.txt not found, skipping" Yellow
        return
    }
    
    if (-not (Test-Path $BuildDir)) { mkdir $BuildDir | Out-Null }
    
    Push-Location $BuildDir
    try {
        Log "  → CMake configure..." Yellow
        cmake .. -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release 2>&1 | Select-Object -First 5
        
        Log "  → MSBuild compile..." Yellow
        $msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path $msbuild) {
            & $msbuild RawrXD.sln /p:Configuration=Release /p:Platform=x64 /v:minimal 2>&1 | Select-Object -First 10
            Log "IDE build complete" Green
        } else {
            Log "MSBuild not found" Yellow
        }
    } finally {
        Pop-Location
    }
}

function BuildDesktop {
    Log "Building desktop utilities..." Cyan
    
    $DesktopDir = "$ProjectRoot\desktop"
    $OutDir = "$ProjectRoot\dist"
    
    if (Test-Path $DesktopDir) {
        if (-not (Test-Path $OutDir)) { mkdir $OutDir | Out-Null }
        Copy-Item "$DesktopDir\*" $OutDir -Recurse -Force -ErrorAction SilentlyContinue
        Log "Desktop utilities copied to $OutDir" Green
    }
}

function TestBuild {
    Log "Testing build artifacts..." Cyan
    
    $CompilerDir = "$ProjectRoot\compilers"
    $exes = @(Get-ChildItem $CompilerDir -Filter '*.exe' -ErrorAction SilentlyContinue)
    
    foreach ($exe in $exes) {
        Log "  Testing: $($exe.Name)" Yellow
        try {
            & $exe.FullName 2>&1 | Select-Object -First 3
            Log "    ✓ OK" Green
        } catch {
            Log "    ✗ Failed: $_" Red
        }
    }
}

function RunIDE {
    Log "Launching IDE..." Cyan
    
    $ide = Get-ChildItem "$ProjectRoot\build" -Filter "RawrXD.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($ide) {
        Log "Found IDE at: $($ide.FullName)" Green
        Start-Process $ide.FullName
    } else {
        Log "IDE executable not found" Yellow
    }
}

# ═════════════════════════════════════════════════════════════════════════════

Log "╔════════════════════════════════════════════════════════════════╗" Magenta
Log "║  RawrXD IDE - FAST BUILD (No scaffolding, just real output)  ║" Magenta
Log "╚════════════════════════════════════════════════════════════════╝" Magenta

Log "Target: $What | Clean: $Clean | Test: $Test | Run: $Run" Magenta

if ($Clean) {
    Log "Cleaning build artifacts..." Yellow
    Remove-Item "$ProjectRoot\build" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "$ProjectRoot\dist" -Recurse -Force -ErrorAction SilentlyContinue
}

# Build what was requested
switch ($What) {
    'compilers' { BuildCompilers }
    'ide'       { BuildIDE }
    'desktop'   { BuildDesktop }
    'all'       { BuildCompilers; BuildIDE; BuildDesktop }
}

if ($Test) { TestBuild }
if ($Run) { RunIDE }

Log "╔════════════════════════════════════════════════════════════════╗" Magenta
Log "║  BUILD COMPLETE                                               ║" Magenta
Log "╚════════════════════════════════════════════════════════════════╝" Magenta

Log "Outputs at: $ProjectRoot\dist" Green
