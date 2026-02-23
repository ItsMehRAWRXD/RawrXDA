# Test-VSIXInstall.ps1 — Agentic VSIX loader test for Amazon Q & GitHub Copilot
# Downloads .vsix from marketplace, validates, optionally launches IDE.

param(
    [switch]$LaunchIDE,
    [string]$VsixPath,
    [switch]$CreateTestVsix,
    [switch]$DownloadCopilot,
    [switch]$DownloadAmazonQ
)

$ErrorActionPreference = "Stop"

# Ensure unsigned extensions allowed for Amazon Q / GitHub Copilot
$env:RAWRXD_ALLOW_UNSIGNED_EXTENSIONS = "1"

# Marketplace extension IDs
$Extensions = @{
    Copilot = @{ Publisher = "GitHub"; Extension = "copilot"; ItemName = "GitHub.copilot" }
    AmazonQ = @{ Publisher = "AmazonWebServices"; Extension = "amazon-q-vscode"; ItemName = "AmazonWebServices.amazon-q-vscode" }
}

$BuildDir = Join-Path $PSScriptRoot "..\build_ide"
$ExtensionsDir = Join-Path $env:APPDATA "RawrXD\extensions"
$ideExe = Join-Path $BuildDir "bin\RawrXD-Win32IDE.exe"

function Write-Step { param($Msg) Write-Host "`n[Test-VSIX] $Msg" -ForegroundColor Cyan }
function Write-Ok   { param($Msg) Write-Host "  OK: $Msg" -ForegroundColor Green }
function Write-Warn { param($Msg) Write-Host "  WARN: $Msg" -ForegroundColor Yellow }
function Write-Err  { param($Msg) Write-Host "  ERR: $Msg" -ForegroundColor Red }

# ---- Download from Marketplace (agentic) ----
function Get-MarketplaceVsix {
    param([string]$Publisher, [string]$Extension, [string]$ItemName)
    try {
        $itemUrl = "https://marketplace.visualstudio.com/items?itemName=$ItemName"
        $item = Invoke-RestMethod -Uri $itemUrl -UseBasicParsing -ErrorAction Stop
        $version = "latest"
        if ($item.versions -and $item.versions[0].version) { $version = $item.versions[0].version }
        $downloadUrl = "https://marketplace.visualstudio.com/_apis/public/gallery/publishers/$Publisher/vsextensions/$Extension/$version/vspackage"
        $baseName = if ($version -eq "latest") { "${Extension}-latest" } else { "$Extension-$version" }
        $outFile = Join-Path $PSScriptRoot "..\${baseName}.vsix"
        Invoke-WebRequest -Uri $downloadUrl -OutFile $outFile -UseBasicParsing -ErrorAction Stop
        return $outFile
    } catch {
        Write-Err "Download failed: $_"
        return $null
    }
}

if ($DownloadCopilot) {
    Write-Step "Downloading GitHub Copilot .vsix from Marketplace"
    $ext = $Extensions.Copilot
    $path = Get-MarketplaceVsix -Publisher $ext.Publisher -Extension $ext.Extension -ItemName $ext.ItemName
    if ($path -and (Test-Path $path)) {
        Write-Ok "Downloaded: $path"
        Write-Host "  Install via: View > Extensions > Install .vsix... > $path"
        if ($LaunchIDE) { $VsixPath = $path }
    }
}

if ($DownloadAmazonQ) {
    Write-Step "Downloading Amazon Q .vsix from Marketplace"
    $ext = $Extensions.AmazonQ
    $path = Get-MarketplaceVsix -Publisher $ext.Publisher -Extension $ext.Extension -ItemName $ext.ItemName
    if ($path -and (Test-Path $path)) {
        Write-Ok "Downloaded: $path"
        Write-Host "  Install via: View > Extensions > Install .vsix... > $path"
        if ($LaunchIDE -and -not $VsixPath) { $VsixPath = $path }
    }
}

if ($DownloadCopilot -or $DownloadAmazonQ) {
    if ($LaunchIDE -and (Test-Path $ideExe)) {
        Write-Step "Launching IDE"
        Start-Process -FilePath $ideExe -WorkingDirectory (Split-Path $ideExe)
        Write-Ok "IDE launched — use View > Extensions > Install .vsix... to install"
    }
    exit 0
}

# ---- Create minimal test VSIX ----
if ($CreateTestVsix) {
    Write-Step "Creating minimal test .vsix"
    $testDir = Join-Path $env:TEMP "RawrXD_vsix_test"
    New-Item -ItemType Directory -Force -Path $testDir | Out-Null
    $pkgPath = Join-Path $testDir "package.json"
    @{
        name = "rawrxd-test-extension"
        displayName = "RawrXD Test"
        version = "0.0.1"
        publisher = "RawrXD"
        description = "Minimal test VSIX"
        engines = @{ vscode = "^1.60.0" }
    } | ConvertTo-Json | Set-Content -Path $pkgPath -Encoding utf8
    $zipOut = Join-Path $env:TEMP "rawrxd-test.zip"
    $vsixOut = Join-Path $PSScriptRoot "..\rawrxd-test-0.0.1.vsix"
    Compress-Archive -Path "$testDir\*" -DestinationPath $zipOut -Force
    Move-Item -Path $zipOut -Destination $vsixOut -Force
    Remove-Item -Recurse -Force $testDir -ErrorAction SilentlyContinue
    Write-Ok "Created $vsixOut"
    Write-Host "  Install via: View > Extensions > Install .vsix... > select $vsixOut"
    exit 0
}

# ---- Verify extraction with PowerShell ----
Write-Step "Verifying PowerShell Expand-Archive (used by VSIX loader)"
try {
    $testZip = Join-Path $env:TEMP "vsix_test_$(Get-Random).zip"
    "test" | Out-File (Join-Path $env:TEMP "vsix_test_content.txt")
    Compress-Archive -Path "$env:TEMP\vsix_test_content.txt" -DestinationPath $testZip -Force
    $extractDir = Join-Path $env:TEMP "vsix_test_extract"
    Expand-Archive -LiteralPath $testZip -DestinationPath $extractDir -Force
    if (Test-Path "$extractDir\vsix_test_content.txt") { Write-Ok "PowerShell extraction works" }
    Remove-Item $testZip, "$env:TEMP\vsix_test_content.txt" -Force -ErrorAction SilentlyContinue
    Remove-Item -Recurse $extractDir -Force -ErrorAction SilentlyContinue
} catch {
    Write-Err "PowerShell Expand-Archive failed: $_"
}

# ---- Check IDE and extensions dir ----
if (Test-Path $ideExe) {
    Write-Ok "IDE found: $ideExe"
} else {
    Write-Warn "IDE not found. Build first: cmake --build build_ide --config Release --target RawrXD-Win32IDE"
}

if (-not (Test-Path $ExtensionsDir)) {
    New-Item -ItemType Directory -Force -Path $ExtensionsDir | Out-Null
    Write-Ok "Created extensions dir: $ExtensionsDir"
} else {
    Write-Ok "Extensions dir: $ExtensionsDir"
}

# ---- Download / VSIX path ----
if ($VsixPath) {
    if (Test-Path $VsixPath) {
        Write-Ok "VSIX file: $VsixPath"
        Write-Host "  Install via IDE: View > Extensions > Install .vsix... > $VsixPath"
        Write-Host "  Or terminal: /install $VsixPath"
    } else {
        Write-Err "VSIX not found: $VsixPath"
    }
}

# ---- Amazon Q & GitHub Copilot ----
Write-Step "Amazon Q & GitHub Copilot"
Write-Host @"

  Download .vsix from VS Code Marketplace:
    • GitHub Copilot: https://marketplace.visualstudio.com/items?itemName=GitHub.copilot
      → Click "Download Extension" (right sidebar)
    • Amazon Q:       https://marketplace.visualstudio.com/items?itemName=AmazonWebServices.amazon-q-vscode
      → Click "Download Extension" (right sidebar)

  Then:
    1. Launch IDE (use -LaunchIDE switch)
    2. View > Extensions (Activity Bar > Exts)
    3. Click "Install .vsix..."
    4. Select the downloaded .vsix file

  Note: Full Copilot/Q chat requires VS Code extension host. RawrXD currently
        installs and extracts; activation/chat UI integration is future work.

"@ -ForegroundColor Gray

# ---- Launch IDE ----
if ($LaunchIDE -and (Test-Path $ideExe)) {
    Write-Step "Launching IDE with RAWRXD_ALLOW_UNSIGNED_EXTENSIONS=1"
    Start-Process -FilePath $ideExe -WorkingDirectory (Split-Path $ideExe)
    Write-Ok "IDE launched"
}

Write-Host ""
