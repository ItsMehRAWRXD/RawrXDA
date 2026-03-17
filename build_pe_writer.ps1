<# ============================================================================
   build_pe_writer.ps1 — Build RawrXD_PE_Writer.cpp into executable
   
   Usage: .\build_pe_writer.ps1
   Output: bin\RawrXD_PE_Writer.exe (test executable)
   ============================================================================ #>
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path "$RepoRoot\src\RawrXD_PE_Writer.cpp")) {
    $RepoRoot = $PSScriptRoot  # script might be in repo root
}
$SrcFile  = Join-Path $RepoRoot 'src\RawrXD_PE_Writer.cpp'
$BinDir   = Join-Path $RepoRoot 'bin'
$OutExe   = Join-Path $BinDir   'RawrXD_PE_Writer.exe'

if (-not (Test-Path $SrcFile)) {
    Write-Error "Source not found: $SrcFile"
    exit 1
}

# ── Locate MSVC cl.exe ──
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstallDir = $null
if (Test-Path $vswhere) {
    $vsInstallDir = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null | Select-Object -First 1
}
if (-not $vsInstallDir) {
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
    Write-Error 'Visual Studio 2022 x64 tools not found.'
    exit 1
}

Write-Host "[Build] VS at: $vsInstallDir" -ForegroundColor Cyan

# ── Init VS environment ──
$vcvars = Join-Path $vsInstallDir 'VC\Auxiliary\Build\vcvars64.bat'
$envBlock = cmd /c "`"$vcvars`" >nul 2>&1 && set" 2>$null
foreach ($line in $envBlock) {
    if ($line -match '^([^=]+)=(.*)$') {
        $envName  = $Matches[1]
        $envValue = $Matches[2]
        Set-Item -Path "env:$envName" -Value $envValue -ErrorAction SilentlyContinue
    }
}

$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $cl) {
    $msvcBins = Get-ChildItem -Path (Join-Path $vsInstallDir 'VC\Tools\MSVC') -Recurse -Filter 'cl.exe' -ErrorAction SilentlyContinue |
        Where-Object { $_.DirectoryName -like '*Hostx64\x64*' } | Select-Object -First 1
    if ($msvcBins) {
        $env:PATH = "$($msvcBins.DirectoryName);$env:PATH"
        $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    }
    if (-not $cl) {
        Write-Error 'cl.exe not found'
        exit 1
    }
}
Write-Host "[Build] cl.exe: $($cl.Source)" -ForegroundColor Cyan

# ── Resolve SDK paths ──
$msvcToolsDir = Get-ChildItem -Path (Join-Path $vsInstallDir 'VC\Tools\MSVC') -Directory |
    Sort-Object Name | Select-Object -Last 1
$msvcRoot = $msvcToolsDir.FullName

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

# ── Ensure bin/ ──
if (-not (Test-Path $BinDir)) { New-Item -ItemType Directory -Path $BinDir -Force | Out-Null }

# ── Compile + Link ──
Write-Host "[Build] Compiling RawrXD_PE_Writer.cpp → EXE..." -ForegroundColor Yellow

Push-Location $RepoRoot
try {
    & cl.exe /nologo /O2 /W3 /Fe"$OutExe" `
        "$SrcFile" `
        /link /SUBSYSTEM:CONSOLE
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compilation failed (exit code $LASTEXITCODE)"
        exit 1
    }
} finally {
    Pop-Location
}

# ── Verify output ──
if (Test-Path $OutExe) {
    $size = (Get-Item $OutExe).Length
    Write-Host "[Build] ✅ $OutExe  ($([math]::Round($size/1024)) KB)" -ForegroundColor Green
} else {
    Write-Error "EXE not produced at $OutExe"
    exit 1
}

# Clean up .obj
@('RawrXD_PE_Writer.obj') | ForEach-Object {
    $f = Join-Path $RepoRoot $_
    if (Test-Path $f) { Remove-Item $f -Force }
}

Write-Host "[Build] Done." -ForegroundColor Green
