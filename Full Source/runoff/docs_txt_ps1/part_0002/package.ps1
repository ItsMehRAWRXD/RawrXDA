# ============================================================================
# package.ps1 — RawrXD IDE Packaging & Distribution
# ============================================================================
# Creates a distributable ZIP archive with:
#   - Signed EXE binary
#   - Runtime dependencies (WebView2 loader, VC++ runtime check)
#   - GUI assets (ide_chatbot.html, shaders, assets)
#   - Configuration templates
#   - Version manifest
#
# Usage:
#   .\package.ps1                      # Package with auto-detection
#   .\package.ps1 -Version "1.0.0"    # Package with explicit version
#   .\package.ps1 -OutputDir .\dist    # Custom output directory
# ============================================================================

param(
    [string]$Version = "",
    [string]$OutputDir = "",
    [string]$BuildDir = "",
    [switch]$IncludeDebugSymbols,
    [switch]$SkipSigning,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# ============================================================================
# Auto-detect paths
# ============================================================================
$ProjectRoot = $PSScriptRoot

function Get-BuildDir {
    if ($BuildDir -and (Test-Path $BuildDir)) { return $BuildDir }

    $candidates = @(
        (Join-Path $ProjectRoot "build" "Release"),
        (Join-Path $ProjectRoot "build"),
        (Join-Path $ProjectRoot "bin" "Release"),
        (Join-Path $ProjectRoot "bin"),
        (Join-Path $ProjectRoot "out" "Release")
    )

    foreach ($c in $candidates) {
        $exes = Get-ChildItem $c -Filter "RawrXD*.exe" -ErrorAction SilentlyContinue
        if ($exes) { return $c }
    }

    # Fallback: search project root
    return $ProjectRoot
}

function Get-Version {
    if ($Version) { return $Version }

    # Try to extract from exe version info
    $buildDir = Get-BuildDir
    $exe = Get-ChildItem $buildDir -Filter "RawrXD*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($exe) {
        $vi = $exe.VersionInfo
        if ($vi -and $vi.FileVersion) {
            return $vi.FileVersion
        }
    }

    # Try git tag
    try {
        $tag = git describe --tags --abbrev=0 2>$null
        if ($tag) { return $tag -replace '^v', '' }
    } catch {}

    # Fallback
    return "1.0.0"
}

function Get-OutputDir {
    if ($OutputDir) { return $OutputDir }
    $distDir = Join-Path $ProjectRoot "dist"
    if (-not (Test-Path $distDir)) { New-Item -ItemType Directory -Path $distDir -Force | Out-Null }
    return $distDir
}

# ============================================================================
# Manifest Generation
# ============================================================================
function New-PackageManifest {
    param(
        [string]$StagingDir,
        [string]$PackageVersion,
        [string[]]$FileList
    )

    $manifest = @{
        name = "RawrXD-IDE"
        version = $PackageVersion
        description = "RawrXD AI IDE — Advanced GGUF Model Loader with Agentic Intelligence"
        author = "ItsMehRAWRXD"
        license = "See LICENSE"
        build_date = (Get-Date -Format "yyyy-MM-dd HH:mm:ss UTC")
        build_machine = $env:COMPUTERNAME
        architecture = "x64"
        platform = "Windows 10/11"
        minimum_os = "Windows 10 20H2 (19042)"
        runtime_requirements = @{
            vcredist = "Visual C++ Redistributable 2022 (14.x)"
            webview2 = "Microsoft Edge WebView2 Runtime"
            dotnet = "Not required"
        }
        components = @{
            ide_core = "Win32 native IDE with D2D rendering"
            editor_engine = "MonacoCore gap buffer with AVX-512"
            ai_backend = "Multi-model support (Ollama, OpenAI, local GGUF)"
            voice_chat = "Native Win32 waveIn/waveOut with VAD"
            hotpatch_system = "Three-layer hotpatch (Memory, Byte, Server)"
            pdb_engine = "Native PDB parser with GSI hash resolution"
            lsp_client = "Bidirectional JSON-RPC language server"
        }
        files = $FileList
    }

    $manifestPath = Join-Path $StagingDir "manifest.json"
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $manifestPath -Encoding UTF8
    return $manifestPath
}

# ============================================================================
# Main
# ============================================================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  RawrXD IDE — Packaging Tool" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$buildDir = Get-BuildDir
$version = Get-Version
$outDir = Get-OutputDir

Write-Host "[package] Build directory: $buildDir" -ForegroundColor Gray
Write-Host "[package] Version: $version" -ForegroundColor Cyan
Write-Host "[package] Output: $outDir" -ForegroundColor Gray
Write-Host ""

# Create staging directory
$stagingName = "RawrXD-IDE-v$version-x64"
$stagingDir = Join-Path $outDir $stagingName
if (Test-Path $stagingDir) { Remove-Item $stagingDir -Recurse -Force }
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null

$fileList = @()

# ---- Copy main executable(s) ----
Write-Host "[package] Copying executables..." -ForegroundColor Yellow
$exes = Get-ChildItem $buildDir -Filter "RawrXD*.exe" -ErrorAction SilentlyContinue
foreach ($exe in $exes) {
    Copy-Item $exe.FullName -Destination $stagingDir
    $fileList += $exe.Name
    Write-Host "  + $($exe.Name) ($([math]::Round($exe.Length / 1MB, 2)) MB)" -ForegroundColor Green
}

# Also copy from project root if build dir differs
if ($buildDir -ne $ProjectRoot) {
    $rootExes = Get-ChildItem $ProjectRoot -Filter "RawrXD.exe" -ErrorAction SilentlyContinue
    foreach ($exe in $rootExes) {
        if (-not (Test-Path (Join-Path $stagingDir $exe.Name))) {
            Copy-Item $exe.FullName -Destination $stagingDir
            $fileList += $exe.Name
            Write-Host "  + $($exe.Name) ($([math]::Round($exe.Length / 1MB, 2)) MB)" -ForegroundColor Green
        }
    }
}

# ---- Copy DLLs ----
Write-Host "[package] Copying runtime DLLs..." -ForegroundColor Yellow
$dlls = Get-ChildItem $buildDir -Filter "*.dll" -ErrorAction SilentlyContinue
foreach ($dll in $dlls) {
    Copy-Item $dll.FullName -Destination $stagingDir
    $fileList += $dll.Name
}
if ($dlls) { Write-Host "  + $($dlls.Count) DLL(s)" -ForegroundColor Green }

# ---- Copy GUI assets ----
Write-Host "[package] Copying GUI assets..." -ForegroundColor Yellow
$guiDir = Join-Path $ProjectRoot "gui"
if (Test-Path $guiDir) {
    $guiStaging = Join-Path $stagingDir "gui"
    New-Item -ItemType Directory -Path $guiStaging -Force | Out-Null
    Copy-Item (Join-Path $guiDir "ide_chatbot.html") -Destination $guiStaging -ErrorAction SilentlyContinue
    $fileList += "gui/ide_chatbot.html"
    Write-Host "  + gui/ide_chatbot.html" -ForegroundColor Green
}

# ---- Copy assets ----
$assetsDir = Join-Path $ProjectRoot "assets"
if (Test-Path $assetsDir) {
    $assetsStaging = Join-Path $stagingDir "assets"
    Copy-Item $assetsDir -Destination $assetsStaging -Recurse
    $assetFiles = Get-ChildItem $assetsStaging -Recurse -File
    $fileList += $assetFiles | ForEach-Object { "assets/" + $_.Name }
    Write-Host "  + $($assetFiles.Count) asset file(s)" -ForegroundColor Green
}

# ---- Copy shaders ----
$shadersDir = Join-Path $ProjectRoot "shaders"
if (Test-Path $shadersDir) {
    $shadersStaging = Join-Path $stagingDir "shaders"
    Copy-Item $shadersDir -Destination $shadersStaging -Recurse
    Write-Host "  + shaders/" -ForegroundColor Green
}

# ---- Copy config template ----
$configExample = Join-Path $ProjectRoot "config.example.json"
if (Test-Path $configExample) {
    Copy-Item $configExample -Destination $stagingDir
    $fileList += "config.example.json"
    Write-Host "  + config.example.json" -ForegroundColor Green
}

# ---- Copy LICENSE and README ----
foreach ($docFile in @("LICENSE", "README.md")) {
    $docPath = Join-Path $ProjectRoot $docFile
    if (Test-Path $docPath) {
        Copy-Item $docPath -Destination $stagingDir
        $fileList += $docFile
        Write-Host "  + $docFile" -ForegroundColor Green
    }
}

# ---- Debug symbols (optional) ----
if ($IncludeDebugSymbols) {
    Write-Host "[package] Including debug symbols..." -ForegroundColor Yellow
    $pdbDir = Join-Path $stagingDir "debug"
    New-Item -ItemType Directory -Path $pdbDir -Force | Out-Null
    $pdbs = Get-ChildItem $buildDir -Filter "*.pdb" -Recurse -ErrorAction SilentlyContinue
    foreach ($pdb in $pdbs) {
        Copy-Item $pdb.FullName -Destination $pdbDir
        $fileList += "debug/$($pdb.Name)"
    }
    if ($pdbs) { Write-Host "  + $($pdbs.Count) PDB file(s)" -ForegroundColor Green }
}

# ---- Generate manifest ----
Write-Host "[package] Generating manifest..." -ForegroundColor Yellow
$manifestPath = New-PackageManifest -StagingDir $stagingDir -PackageVersion $version -FileList $fileList
Write-Host "  + manifest.json" -ForegroundColor Green

# ---- Sign binaries ----
if (-not $SkipSigning) {
    $signScript = Join-Path $ProjectRoot "sign.ps1"
    if (Test-Path $signScript) {
        Write-Host "[package] Signing binaries..." -ForegroundColor Yellow
        & $signScript -BuildDir $stagingDir -SelfSign
    } else {
        Write-Host "[package] sign.ps1 not found — skipping signing" -ForegroundColor Yellow
    }
}

# ---- Create ZIP archive ----
Write-Host ""
Write-Host "[package] Creating ZIP archive..." -ForegroundColor Yellow
$zipName = "$stagingName.zip"
$zipPath = Join-Path $outDir $zipName

if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path "$stagingDir\*" -DestinationPath $zipPath -CompressionLevel Optimal

$zipSize = (Get-Item $zipPath).Length
Write-Host "  + $zipName ($([math]::Round($zipSize / 1MB, 2)) MB)" -ForegroundColor Green

# ---- Compute SHA256 ----
$hash = (Get-FileHash $zipPath -Algorithm SHA256).Hash
$hashFile = Join-Path $outDir "$stagingName.sha256"
"$hash  $zipName" | Set-Content -Path $hashFile
Write-Host "  + SHA256: $hash" -ForegroundColor Cyan

# ---- Summary ----
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Package Complete!" -ForegroundColor Green
Write-Host "  Version:  $version" -ForegroundColor Cyan
Write-Host "  Archive:  $zipPath" -ForegroundColor Cyan
Write-Host "  Files:    $($fileList.Count)" -ForegroundColor Cyan
Write-Host "  Size:     $([math]::Round($zipSize / 1MB, 2)) MB" -ForegroundColor Cyan
Write-Host "  SHA256:   $hash" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
