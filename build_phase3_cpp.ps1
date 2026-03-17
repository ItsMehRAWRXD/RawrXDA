<# ============================================================================
   build_phase3_cpp.ps1 — Build Phase3_Agent_Kernel.dll from C++ (real impl)
   
   Usage:  .\build_phase3_cpp.ps1
   Output: bin\Phase3_Agent_Kernel.dll
   ============================================================================ #>
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path "$RepoRoot\src\Phase3_Agent_Kernel.cpp")) {
    $RepoRoot = $PSScriptRoot  # script might be in repo root
}

$SrcFile  = Join-Path $RepoRoot 'src\Phase3_Agent_Kernel.cpp'
$BinDir   = Join-Path $RepoRoot 'bin'
$OutDll   = Join-Path $BinDir   'Phase3_Agent_Kernel.dll'

if (-not (Test-Path $SrcFile)) {
    Write-Error "Source not found: $SrcFile"
    exit 1
}

# ── Locate MSVC cl.exe via vswhere ──
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallDir = $null
if (Test-Path $vswhere) {
    $vsInstallDir = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null | Select-Object -First 1
}
if (-not $vsInstallDir) {
    # Fallback: known paths
    $knownPaths = @(
        'D:\VS2022Enterprise',
        'C:\VS2022Enterprise',
        'C:\Program Files\Microsoft Visual Studio\2022\Community',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
    )
    foreach ($p in $knownPaths) {
        if (Test-Path "$p\VC\Auxiliary\Build\vcvars64.bat") {
            $vsInstallDir = $p; break
        }
    }
}
if (-not $vsInstallDir) {
    Write-Error 'Visual Studio 2022 x64 tools not found. Install Build Tools or Community.'
    exit 1
}

Write-Host "[Build] VS at: $vsInstallDir" -ForegroundColor Cyan

# ── Init VS environment via cmd /c vcvars64.bat && set ──
$vcvars = Join-Path $vsInstallDir 'VC\Auxiliary\Build\vcvars64.bat'
Write-Host "[Build] vcvars64: $vcvars" -ForegroundColor DarkGray

# Run vcvars in cmd and capture the resulting environment
$envBlock = cmd /c "`"$vcvars`" >nul 2>&1 && set" 2>$null
foreach ($line in $envBlock) {
    if ($line -match '^([^=]+)=(.*)$') {
        $envName  = $Matches[1]
        $envValue = $Matches[2]
        Set-Item -Path "env:$envName" -Value $envValue -ErrorAction SilentlyContinue
    }
}

# Verify cl.exe
$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $cl) {
    # Last-resort: look in MSVC bin directly
    $msvcBins = Get-ChildItem -Path (Join-Path $vsInstallDir 'VC\Tools\MSVC') -Recurse -Filter 'cl.exe' -ErrorAction SilentlyContinue |
        Where-Object { $_.DirectoryName -like '*Hostx64\x64*' } | Select-Object -First 1
    if ($msvcBins) {
        $env:PATH = "$($msvcBins.DirectoryName);$env:PATH"
        $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    }
    if (-not $cl) {
        Write-Error 'cl.exe not found after vcvars64.bat injection'
        exit 1
    }
}
Write-Host "[Build] cl.exe: $($cl.Source)" -ForegroundColor Cyan

# ── Ensure bin/ ──
if (-not (Test-Path $BinDir)) { New-Item -ItemType Directory -Path $BinDir -Force | Out-Null }

# ── Resolve SDK + MSVC include/lib paths (in case vcvars didn't fully propagate) ──
$msvcToolsDir = Get-ChildItem -Path (Join-Path $vsInstallDir 'VC\Tools\MSVC') -Directory |
    Sort-Object Name | Select-Object -Last 1
$msvcRoot = $msvcToolsDir.FullName

# Find Windows SDK
$sdkRoot = $null
foreach ($sr in @('C:\Program Files (x86)\Windows Kits\10', 'D:\Program Files (x86)\Windows Kits\10')) {
    if (Test-Path $sr) { $sdkRoot = $sr; break }
}
$sdkVer = '10.0.22621.0'

if ($msvcRoot -and $sdkRoot) {
    $env:INCLUDE = "$msvcRoot\include;$sdkRoot\Include\$sdkVer\ucrt;$sdkRoot\Include\$sdkVer\shared;$sdkRoot\Include\$sdkVer\um;$sdkRoot\Include\$sdkVer\winrt"
    $env:LIB     = "$msvcRoot\lib\x64;$msvcRoot\lib\onecore\x64;$sdkRoot\Lib\$sdkVer\ucrt\x64;$sdkRoot\Lib\$sdkVer\um\x64"
    Write-Host "[Build] INCLUDE: $env:INCLUDE" -ForegroundColor DarkGray
    Write-Host "[Build] LIB:     $env:LIB" -ForegroundColor DarkGray
}

# ── Compile + Link ──
Write-Host "[Build] Compiling Phase3_Agent_Kernel.cpp → DLL..." -ForegroundColor Yellow

Push-Location $RepoRoot
try {
    & cl.exe /nologo /O2 /W3 /LD `
        /Fe"$OutDll" `
        "$SrcFile" `
        /link /SUBSYSTEM:WINDOWS /LARGEADDRESSAWARE `
        kernel32.lib user32.lib comdlg32.lib shell32.lib ole32.lib
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compilation failed (exit code $LASTEXITCODE)"
        exit 1
    }
} finally {
    Pop-Location
}

# ── Verify output ──
if (Test-Path $OutDll) {
    $size = (Get-Item $OutDll).Length
    Write-Host "[Build] ✅ $OutDll  ($([math]::Round($size/1024)) KB)" -ForegroundColor Green
} else {
    Write-Error "DLL not produced at $OutDll"
    exit 1
}

# Clean up .obj / .exp / .lib if they landed in repo root
@('Phase3_Agent_Kernel.obj', 'Phase3_Agent_Kernel.lib', 'Phase3_Agent_Kernel.exp') | ForEach-Object {
    $f = Join-Path $RepoRoot $_
    if (Test-Path $f) { Remove-Item $f -Force }
}

Write-Host "[Build] Done." -ForegroundColor Green
