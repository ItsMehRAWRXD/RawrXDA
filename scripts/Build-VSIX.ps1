# ============================================================================
# Build-VSIX.ps1 — RawrXD Prometheus Native VSIX Builder
# ============================================================================
# Zero Electron. Pure C++/Win32/MASM. Produces .vsix for VS 2022.
# ============================================================================
param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$OutDir = "d:\rawrxd\build\vsix",
    [switch]$Clean,
    [switch]$Sign
)

$ErrorActionPreference = "Stop"
$VSIXName = "RawrXD.Prometheus.Native.vsix"
$PackageName = "RawrXD.Prometheus.Native"

function Write-Header($msg) {
    Write-Host "`n=== $msg ===" -ForegroundColor Cyan
}

function Ensure-Dir($path) {
    if ($Clean -and (Test-Path $path)) {
        Remove-Item $path -Recurse -Force
    }
    if (!(Test-Path $path)) {
        New-Item -ItemType Directory -Path $path -Force | Out-Null
    }
}

# ============================================================================
# Phase 1: Environment Validation
# ============================================================================
Write-Header "Phase 1: Environment Validation"

$vcvarsPath = "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path $vcvarsPath)) {
    $vcvarsPath = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
}
if (!(Test-Path $vcvarsPath)) {
    throw "vcvars64.bat not found. Install VS 2022 with C++ workload."
}

$ml64Path = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
if (!(Test-Path $ml64Path)) {
    $ml64Path = (Get-ChildItem -Path "C:\VS2022Enterprise\VC\Tools\MSVC" -Filter "ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
if (!(Test-Path $ml64Path)) {
    throw "ml64.exe not found. Install VS 2022 with MASM workload."
}

Write-Host "  vcvars64: $vcvarsPath" -ForegroundColor Gray
Write-Host "  ml64:     $ml64Path" -ForegroundColor Gray

# ============================================================================
# Phase 2: Build Native DLL
# ============================================================================
Write-Header "Phase 2: Build Native DLL"

$buildDir = "$OutDir\$Configuration"
Ensure-Dir $buildDir

$srcDir = "d:\rawrxd\vsix\src"
$includeDirs = @(
    "d:\rawrxd\include",
    "d:\rawrxd\src",
    "d:\rawrxd\src\prometheus"
)

$incFlags = ($includeDirs | ForEach-Object { "/I`"$_`"" }) -join " "
$defFlags = "/D WIN32 /D _WINDOWS /D _USRDLL /D _WINDLL /D RAWRXD_GOLD_BUILD=1 /D NOMINMAX /D WIN32_LEAN_AND_MEAN /D _CRT_SECURE_NO_WARNINGS"
$optFlags = "/O2 /Ob2 /Oi /Ot /GL /Gy /arch:AVX512"
$warnFlags = "/W3 /WX- /EHsc /MP"
$linkFlags = "/DLL /MACHINE:X64 /OPT:REF /OPT:ICF /LTCG /DEF:`"$srcDir\$PackageName.def`""

# Compile vspackage.cpp
Write-Host "  Compiling vspackage.cpp..." -NoNewline
& "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe" `
    /nologo /TP $incFlags $defFlags $optFlags $warnFlags `
    /Fo"$buildDir\vspackage.obj" /Fd"$buildDir\$PackageName.pdb" `
    /c "$srcDir\vspackage.cpp" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Compilation failed for vspackage.cpp" }
Write-Host " OK" -ForegroundColor Green

# Link DLL
Write-Host "  Linking $PackageName.dll..." -NoNewline
& "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe" `
    /nologo $linkFlags `
    /OUT:"$buildDir\$PackageName.dll" `
    "$buildDir\vspackage.obj" `
    kernel32.lib user32.lib ole32.lib oleaut32.lib shlwapi.lib advapi32.lib `
    /IMPLIB:"$buildDir\$PackageName.lib" 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Link failed for $PackageName.dll" }
Write-Host " OK" -ForegroundColor Green

# Sign if requested
if ($Sign) {
    Write-Host "  Signing DLL..." -NoNewline
    $cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Select-Object -First 1
    if ($cert) {
        Set-AuthenticodeSignature -FilePath "$buildDir\$PackageName.dll" -Certificate $cert -TimestampServer "http://timestamp.digicert.com" | Out-Null
        Write-Host " OK" -ForegroundColor Green
    } else {
        Write-Host " SKIP (no cert)" -ForegroundColor Yellow
    }
}

# ============================================================================
# Phase 3: Assemble VSIX Package
# ============================================================================
Write-Header "Phase 3: Assemble VSIX Package"

$stageDir = "$OutDir\stage"
Ensure-Dir $stageDir

# Copy manifest and metadata
Copy-Item "d:\rawrxd\vsix\extension.vsixmanifest" "$stageDir\extension.vsixmanifest" -Force
Copy-Item "d:\rawrxd\vsix\RawrXD.Prometheus.Native.pkgdef" "$stageDir\RawrXD.Prometheus.Native.pkgdef" -Force

# Copy built DLL
Copy-Item "$buildDir\$PackageName.dll" "$stageDir\$PackageName.dll" -Force
Copy-Item "$buildDir\$PackageName.pdb" "$stageDir\$PackageName.pdb" -Force

# Create Resources directory with placeholder icons
$resourcesDir = "$stageDir\Resources"
Ensure-Dir $resourcesDir

# Generate minimal PNG placeholders (1x1 transparent)
$pngHeader = [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)
$pngChunk = [byte[]](
    0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4,
    0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41,
    0x54, 0x78, 0x9C, 0x63, 0x60, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x01, 0xE2, 0x21, 0xBC, 0x33, 0x00,
    0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
    0x42, 0x60, 0x82
)
$pngData = $pngHeader + $pngChunk
[System.IO.File]::WriteAllBytes("$resourcesDir\Icon.png", $pngData)
[System.IO.File]::WriteAllBytes("$resourcesDir\Preview.png", $pngData)

# Create LICENSE.txt
@"
RawrXD Prometheus Native Extension
Copyright (c) 2025 RawrXD

Licensed under the RawrXD Sovereign License.
See https://rawrxd.dev/license for terms.
"@ | Set-Content "$stageDir\LICENSE.txt" -Encoding UTF8

# ============================================================================
# Phase 4: Zip into .vsix
# ============================================================================
Write-Header "Phase 4: Zip into .vsix"

$vsixPath = "$OutDir\$VSIXName"
if (Test-Path $vsixPath) {
    Remove-Item $vsixPath -Force
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$compressionLevel = [System.IO.Compression.CompressionLevel]::Optimal
[System.IO.Compression.ZipFile]::CreateFromDirectory($stageDir, $vsixPath, $compressionLevel, $false)

$sizeKB = [math]::Round((Get-Item $vsixPath).Length / 1KB, 2)
Write-Host "  Created: $vsixPath ($sizeKB KB)" -ForegroundColor Green

# ============================================================================
# Phase 5: Validation
# ============================================================================
Write-Header "Phase 5: Validation"

$vsixItem = Get-Item $vsixPath
Write-Host "  File: $($vsixItem.Name)" -ForegroundColor Gray
Write-Host "  Size: $([math]::Round($vsixItem.Length/1KB,2)) KB" -ForegroundColor Gray
Write-Host "  SHA256: $((Get-FileHash $vsixPath -Algorithm SHA256).Hash)" -ForegroundColor Gray

# Verify contents
$zip = [System.IO.Compression.ZipFile]::OpenRead($vsixPath)
$entries = $zip.Entries | Select-Object -ExpandProperty FullName
$zip.Dispose()

$required = @("extension.vsixmanifest", "RawrXD.Prometheus.Native.pkgdef",
              "RawrXD.Prometheus.Native.dll", "Resources\Icon.png")
$missing = $required | Where-Object { $_ -notin $entries }
if ($missing) {
    throw "Missing VSIX entries: $($missing -join ', ')"
}
Write-Host "  All required entries present." -ForegroundColor Green

# ============================================================================
# Phase 6: Install (optional)
# ============================================================================
Write-Header "Phase 6: Install to VS 2022"

$vsixInstaller = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\VSIXInstaller.exe"
if (!(Test-Path $vsixInstaller)) {
    $vsixInstaller = "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\VSIXInstaller.exe"
}
if (!(Test-Path $vsixInstaller)) {
    $vsixInstaller = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\VSIXInstaller.exe"
}

if (Test-Path $vsixInstaller) {
    Write-Host "  Installing VSIX..." -NoNewline
    & $vsixInstaller /q /a "$vsixPath" 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 1001) {
        Write-Host " OK" -ForegroundColor Green
    } else {
        Write-Host " WARN (exit $LASTEXITCODE)" -ForegroundColor Yellow
    }
} else {
    Write-Host "  SKIP (VSIXInstaller.exe not found)" -ForegroundColor Yellow
}

Write-Header "Build Complete"
Write-Host "Output: $vsixPath" -ForegroundColor Cyan
Write-Host "`nTo uninstall: VSIXInstaller.exe /q /a /u:14d0a8c2-3e2f-4a5b-9c1d-8e7f6a5b4c3d" -ForegroundColor Gray
