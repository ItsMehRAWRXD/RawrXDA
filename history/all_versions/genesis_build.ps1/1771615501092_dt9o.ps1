# genesis_build.ps1
# Final Link One-Liner (PowerShell wiring) for Monolithic EXE
# Architecture: source = linking - assembling (No recompile, just link existing .obj files)

param(
    [string]$Root = "D:\rawrxd",
    [string]$OutDir = "$env:LOCALAPPDATA\RawrXD\bin",
    [string]$ObjDir = "$env:LOCALAPPDATA\RawrXD\build",
    [string]$LibDir = "$env:LOCALAPPDATA\RawrXD\lib"
)

$ErrorActionPreference = "Stop"

Write-Host "============================================================" -Fore Cyan
Write-Host " RAWRXD MONOLITHIC LINKER - 1600+ SOURCE GLORY" -Fore Cyan
Write-Host " Architecture: Monolithic EXE (All agents as threads)" -Fore Gray
Write-Host "============================================================" -Fore Cyan

# Ensure output directory exists
if(!(Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

# 1. Load PowerLang Linker
$PowLangLinker = "G:\Everything\powlang\tools\linker.ps1"
if(!(Test-Path $PowLangLinker)) {
    throw "PowerLang linker not found at $PowLangLinker"
}

Write-Host "[INFO] Using PowerLang Linker: $PowLangLinker" -Fore Green
. $PowLangLinker

# 2. Gather all .obj files from the custom compiler's output directory
Write-Host "[INFO] Scanning for .obj files in $ObjDir..." -Fore White
$objFiles = Get-ChildItem -Path $ObjDir -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue

if ($objFiles.Count -eq 0) {
    Write-Host "[WARNING] No .obj files found in $ObjDir." -Fore Yellow
    Write-Host "Please ensure your custom compiler has finished generating the standard x64 COFF .obj files." -Fore Yellow
    Write-Host "Falling back to scanning D:\rawrxd for .obj files matching .asm sources..." -Fore Yellow
    
    # Fallback: Find .obj files in D:\rawrxd that correspond to .asm files
    $asmFiles = Get-ChildItem -Path $Root -Filter "*.asm" -Recurse -ErrorAction SilentlyContinue
    $objFiles = @()
    foreach($asm in $asmFiles) {
        $expectedObj = [System.IO.Path]::ChangeExtension($asm.FullName, ".obj")
        if(Test-Path $expectedObj) {
            $objFiles += Get-Item $expectedObj
        }
    }
}

Write-Host "[INFO] Found $($objFiles.Count) .obj files for the final link." -Fore Cyan

if($objFiles.Count -eq 0) {
    throw "No .obj files found! Cannot link."
}

# 3. Define the final executable path
$finalExe = Join-Path $OutDir "RawrXD.exe"

# 4. Add static libraries if they exist
$coreLib = Join-Path $LibDir "rawrxd_core.lib"
$gpuLib = Join-Path $LibDir "rawrxd_gpu.lib"

$allInputFiles = @()
$allInputFiles += $objFiles.FullName
if (Test-Path $coreLib) { $allInputFiles += $coreLib } else { Write-Host "[NOTE] $coreLib not found, skipping." -Fore Gray }
if (Test-Path $gpuLib) { $allInputFiles += $gpuLib } else { Write-Host "[NOTE] $gpuLib not found, skipping." -Fore Gray }

Write-Host "`n[LINKING] Forging Monolithic Executable with PowerLang: $finalExe..." -Fore Yellow

# 5. Execute the PowerLang linker
try {
    Invoke-Linker -InputFiles $allInputFiles `
                  -OutputFile $finalExe `
                  -EntrySymbol "WinMain" `
                  -BaseAddress 0x140000000 `
                  -Format "PE32+" `
                  -Verbose
} catch {
    Write-Host "`n[ERROR] PowerLang Linking failed: $_" -Fore Red
    exit 1
}

Write-Host "`n============================================================" -Fore Green
Write-Host " SUCCESS! 1600+ SOURCE GLORY ACHIEVED" -Fore Green
Write-Host " Final Executable: $finalExe" -Fore White
Write-Host " All agents are now consolidated as threads inside the single executable." -Fore Green
Write-Host "============================================================" -Fore Green
