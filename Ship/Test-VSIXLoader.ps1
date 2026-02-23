# Test-VSIXLoader.ps1 — Agentic VSIX loader verification
# Verifies Install from VSIX menu/panel wiring and tests with Amazon Q / GitHub Copilot.
# Run: .\Ship\Test-VSIXLoader.ps1
#
# Get Amazon Q and GitHub Copilot .vsix from VS Code Marketplace:
#   .\Ship\Download-VSCodeMarketplaceVsix.ps1
#   (downloads to Ship\..\plugins\ by default; use -OutputDir to override)
#
# With Amazon Q / Copilot .vsix (tests metadata extraction — JS execution unavailable):
#   .\Ship\Test-VSIXLoader.ps1 -AmazonQVsix "C:\path\to\amazonq.vsix" -GitHubCopilotVsix "C:\path\to\copilot.vsix"
#   $env:AMAZONQ_VSIX="C:\..."; $env:GITHUB_COPILOT_VSIX="C:\..."; .\Ship\Test-VSIXLoader.ps1

param(
    [string]$IdePath = "",
    [string]$AmazonQVsix = $env:AMAZONQ_VSIX,
    [string]$GitHubCopilotVsix = $env:GITHUB_COPILOT_VSIX,
    [switch]$Gui,      # Launch IDE for manual verification
    [switch]$VsixTest  # Run --vsix-test agentically (loads .vsix from plugins/, writes result JSON)
)

$ErrorActionPreference = "Stop"

# Resolve IDE path
if (-not $IdePath) {
    $candidates = @(
        ".\build_ide\bin\RawrXD-Win32IDE.exe",
        ".\bin\RawrXD-Win32IDE.exe",
        "RawrXD-Win32IDE.exe"
    )
    foreach ($p in $candidates) {
        if (Test-Path $p) { $IdePath = (Resolve-Path $p).Path; break }
    }
}
if (-not $IdePath -or -not (Test-Path $IdePath)) {
    Write-Host "RawrXD IDE not found. Run from repo root or set -IdePath." -ForegroundColor Red
    exit 1
}

Write-Host "=== VSIX Loader Agentic Test ===" -ForegroundColor Cyan
Write-Host "IDE: $IdePath"
Write-Host ""

# 1. Verify menu wiring
Write-Host "[1] Menu wiring: AI Extensions > Install from VSIX..." -ForegroundColor Yellow
Write-Host "    - Check that 'Install from VSIX...' appears under AI Extensions menu"
Write-Host "    - Command ID: 10021 (IDM_QUICKJS_HOST_INSTALL_VSIX)"
Write-Host ""

# 2. Extensions panel
Write-Host "[2] Extensions panel: View > Extensions (or Exts activity bar button)" -ForegroundColor Yellow
Write-Host "    - Extensions view should show 'Install from VSIX...' button"
Write-Host "    - Clicking opens file dialog for .vsix selection"
Write-Host ""

# 3. File Explorer & Agent Chat
Write-Host "[3] File Explorer & Agent Chat:" -ForegroundColor Yellow
Write-Host "    - View > File Explorer (Ctrl+Shift+E) — shows Files sidebar"
Write-Host "    - View > AI Chat / Agent Chat (Ctrl+Alt+B) — toggles AI/agent panel"
Write-Host "    - Activity bar: Files | Exts | Chat — click to switch views"
Write-Host ""

# 4. Extension compatibility
Write-Host "[4] Extension compatibility:" -ForegroundColor Yellow
Write-Host "    - RawrXD QuickJS host: JS extensions with package.json 'main'"
Write-Host "    - RawrXD stub build: metadata-only (VSIXLoader); JS extensions unavailable"
Write-Host "    - Amazon Q / GitHub Copilot: REQUIRE Node.js + VS Code Extension Host"
Write-Host "      These CANNOT run in RawrXD. Use simple test VSIX for validation."
Write-Host ""

# 5. Test VSIX path
$pluginsDir = Join-Path (Get-Location) "plugins"
$testVsix = Get-ChildItem -Path $pluginsDir -Filter "*.vsix" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($testVsix) {
    Write-Host "[5] Test VSIX found: $($testVsix.Name)" -ForegroundColor Green
} else {
    Write-Host "[5] No .vsix in plugins/ - place a simple VSIX to test install" -ForegroundColor Yellow
}
Write-Host ""

# Agentic VSIX test: copy Amazon Q / Copilot to plugins/, run --vsix-test, report
$runVsixTest = $VsixTest -or ($AmazonQVsix -and (Test-Path $AmazonQVsix)) -or ($GitHubCopilotVsix -and (Test-Path $GitHubCopilotVsix))
if ($runVsixTest) {
    $exeDir = Split-Path -Parent $IdePath
    $pluginsDir = Join-Path $exeDir "plugins"
    if (-not (Test-Path $pluginsDir)) { New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null }
    if ($AmazonQVsix -and (Test-Path $AmazonQVsix)) {
        Copy-Item -Path $AmazonQVsix -Destination (Join-Path $pluginsDir "amazonq.vsix") -Force
        Write-Host "[Agentic] Copied Amazon Q .vsix to plugins/" -ForegroundColor Cyan
    }
    if ($GitHubCopilotVsix -and (Test-Path $GitHubCopilotVsix)) {
        Copy-Item -Path $GitHubCopilotVsix -Destination (Join-Path $pluginsDir "github-copilot.vsix") -Force
        Write-Host "[Agentic] Copied GitHub Copilot .vsix to plugins/" -ForegroundColor Cyan
    }
    $env:RAWRXD_ALLOW_UNSIGNED_EXTENSIONS = "1"
    Write-Host "[Agentic] Running: $IdePath --vsix-test" -ForegroundColor Cyan
    Push-Location $exeDir
    try { & $IdePath --vsix-test 2>&1 | Out-Null } finally { Pop-Location }
    $resultPath = Join-Path $pluginsDir "vsix_test_result.json"
    if (Test-Path $resultPath) {
        $json = Get-Content $resultPath -Raw | ConvertFrom-Json
        $loaded = @($json.loaded)
        Write-Host "[Agentic] Loaded extension IDs: $($loaded -join ', ')" -ForegroundColor Green
        if ($loaded.Count -eq 0) { Write-Host "[Agentic] No extensions loaded (metadata extraction may have failed)" -ForegroundColor Yellow }
    } else { Write-Host "[Agentic] vsix_test_result.json not found" -ForegroundColor Yellow }
}

if ($Gui) {
    Write-Host "Launching IDE for manual verification..." -ForegroundColor Cyan
    Start-Process $IdePath
    Write-Host "1. Open AI Extensions > Install from VSIX..."
    Write-Host "2. Open Extensions panel (Exts) > Click Install from VSIX..."
    Write-Host "3. View > File Explorer (Ctrl+Shift+E) | View > AI Chat (Ctrl+Alt+B)"
} elseif (-not $runVsixTest) {
    Write-Host "Pass -Gui to launch IDE, or -AmazonQVsix / -GitHubCopilotVsix for agentic test." -ForegroundColor Gray
}

Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Cyan
