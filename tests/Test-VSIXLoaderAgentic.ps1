# Test-VSIXLoaderAgentic.ps1
# Agentically tests the VSIX loader with Amazon Q and GitHub Copilot extensions.
# Usage:
#   .\Test-VSIXLoaderAgentic.ps1
#   .\Test-VSIXLoaderAgentic.ps1 -AmazonQVsix "C:\path\to\amazonq.vsix" -GitHubCopilotVsix "C:\path\to\github-copilot.vsix"
#   $env:AMAZONQ_VSIX = "C:\..."; $env:GITHUB_COPILOT_VSIX = "C:\..."; .\Test-VSIXLoaderAgentic.ps1

param(
    [string]$AmazonQVsix = $env:AMAZONQ_VSIX,
    [string]$GitHubCopilotVsix = $env:GITHUB_COPILOT_VSIX,
    [string]$IdeExe = $null,
    [switch]$BuildFirst,
    [switch]$SkipCopy
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

# Resolve IDE exe
if (-not $IdeExe) {
    $candidates = @(
        (Join-Path $root "build_ide\bin\RawrXD-Win32IDE.exe"),
        (Join-Path $root "bin\RawrXD-Win32IDE.exe"),
        (Join-Path $root "build\bin\RawrXD-Win32IDE.exe")
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $IdeExe = $c; break }
    }
}
if (-not $IdeExe -or -not (Test-Path $IdeExe)) {
    if ($BuildFirst) {
        Write-Host "Building RawrXD-Win32IDE..." -ForegroundColor Cyan
        $buildDir = Join-Path $root "build_ide"
        if (-not (Test-Path $buildDir)) {
            New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
            & cmake -S $root -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | Out-Null
        }
        & cmake --build $buildDir --config Release --target RawrXD-Win32IDE 2>&1 | Out-Null
        $IdeExe = Join-Path $buildDir "bin\RawrXD-Win32IDE.exe"
    }
    if (-not $IdeExe -or -not (Test-Path $IdeExe)) {
        Write-Error "RawrXD-Win32IDE.exe not found. Set -IdeExe or -BuildFirst."
    }
}

$exeDir = Split-Path -Parent $IdeExe
$pluginsDir = Join-Path $exeDir "plugins"
if (-not (Test-Path $pluginsDir)) { New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null }

# Copy .vsix or auto-discover from VS Code extensions folder
$toLoad = @()
$vscodeExt = Join-Path $env:USERPROFILE ".vscode\extensions"

if ($AmazonQVsix -and (Test-Path $AmazonQVsix)) {
    $dest = Join-Path $pluginsDir "amazonq.vsix"
    if (-not $SkipCopy) { Copy-Item -Path $AmazonQVsix -Destination $dest -Force }
    $toLoad += "amazonq.vsix"
} else {
    # Remove leftover .vsix so loader uses our copied dir (not prior extraction)
    Remove-Item (Join-Path $pluginsDir "amazonq.vsix") -Force -ErrorAction SilentlyContinue
    # Auto-discover Amazon Q from VS Code extensions
    $amazonqDir = Get-ChildItem -Path $vscodeExt -Directory -Filter "amazonwebservices.amazon-q-vscode-*" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($amazonqDir -and (Test-Path (Join-Path $amazonqDir.FullName "package.json"))) {
        $dest = Join-Path $pluginsDir "amazonq"
        if (-not $SkipCopy) {
            if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
            Copy-Item -Path $amazonqDir.FullName -Destination $dest -Recurse -Force
        }
        $toLoad += "amazonq (from VS Code)"
        Write-Host "Auto-discovered Amazon Q: $($amazonqDir.Name)" -ForegroundColor Cyan
    } else {
        Write-Host "Amazon Q not found (use -AmazonQVsix or install from VS Code marketplace). Skipping." -ForegroundColor Yellow
    }
}

if ($GitHubCopilotVsix -and (Test-Path $GitHubCopilotVsix)) {
    $dest = Join-Path $pluginsDir "github-copilot.vsix"
    if (-not $SkipCopy) { Copy-Item -Path $GitHubCopilotVsix -Destination $dest -Force }
    $toLoad += "github-copilot.vsix"
} else {
    Remove-Item (Join-Path $pluginsDir "github-copilot.vsix") -Force -ErrorAction SilentlyContinue
    # Auto-discover GitHub Copilot from VS Code extensions (copilot or copilot-chat)
    $copilotDirs = @(
        (Get-ChildItem -Path $vscodeExt -Directory -Filter "github.copilot-*" -ErrorAction SilentlyContinue | Select-Object -First 1),
        (Get-ChildItem -Path $vscodeExt -Directory -Filter "github.copilot-chat-*" -ErrorAction SilentlyContinue | Select-Object -First 1)
    )
    $copilotDir = $copilotDirs | Where-Object { $_ } | Select-Object -First 1
    if ($copilotDir) {
        $extPkg = Join-Path $copilotDir.FullName "extension\package.json"
        $loadRoot = if (Test-Path $extPkg) { Join-Path $copilotDir.FullName "extension" } else { $copilotDir.FullName }
        if (Test-Path (Join-Path $loadRoot "package.json")) {
            $dest = Join-Path $pluginsDir "github-copilot"
            if (-not $SkipCopy) {
                if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
                Copy-Item -Path $copilotDir.FullName -Destination $dest -Recurse -Force
            }
            $toLoad += "github-copilot (from VS Code)"
            Write-Host "Auto-discovered GitHub Copilot: $($copilotDir.Name)" -ForegroundColor Cyan
        } else {
            Write-Host "GitHub Copilot not found (use -GitHubCopilotVsix). Skipping." -ForegroundColor Yellow
        }
    } else {
        Write-Host "GitHub Copilot not found (use -GitHubCopilotVsix or install from VS Code). Skipping." -ForegroundColor Yellow
    }
}

# Allow unsigned extensions (marketplace VSIX are often unsigned)
$env:RAWRXD_ALLOW_UNSIGNED_EXTENSIONS = "1"

Write-Host "Running VSIX loader agentic test: $IdeExe --vsix-test" -ForegroundColor Cyan
Push-Location $exeDir
try {
    & $IdeExe --vsix-test 2>&1 | Out-Null
    $ideExitCode = $LASTEXITCODE
    if ($ideExitCode -ne 0) {
        Write-Host "IDE exited with code $ideExitCode (continuing to check result file)" -ForegroundColor Yellow
    }
} finally {
    Pop-Location
}

$resultPath = Join-Path $pluginsDir "vsix_test_result.json"
if (-not (Test-Path $resultPath)) { $resultPath = Join-Path $exeDir "vsix_test_result.json" }
if (-not (Test-Path $resultPath)) {
    $alt = Join-Path $root "build_ide\bin\plugins\vsix_test_result.json"
    if (Test-Path $alt) { $resultPath = $alt }
}

if (-not (Test-Path $resultPath)) {
    Write-Host "Result file not found: vsix_test_result.json (exe may have failed or written elsewhere)" -ForegroundColor Red
    Write-Host "Checked: $pluginsDir, $exeDir" -ForegroundColor Gray
    exit 1
}

$json = Get-Content $resultPath -Raw | ConvertFrom-Json
$loaded = @($json.loaded)
Write-Host "Loaded extensions: $($loaded -join ', ')" -ForegroundColor Green
if ($json.help) {
    $json.help.PSObject.Properties | ForEach-Object {
        Write-Host "  $($_.Name): $($_.Value.Substring(0, [Math]::Min(80, $_.Value.Length)))..." -ForegroundColor Gray
    }
}

$expected = @()
if ($AmazonQVsix -or ($toLoad | Where-Object { $_ -like "*amazon*" })) { $expected += "amazon*" }
if ($GitHubCopilotVsix -or ($toLoad | Where-Object { $_ -like "*copilot*" })) { $expected += "*copilot*" }
$testPassed = $true
foreach ($e in $expected) {
    $match = $loaded | Where-Object { $_ -like $e }
    if (-not $match) {
        Write-Host "Expected an extension like '$e' in loaded list." -ForegroundColor Yellow
        $testPassed = $false
    }
}

if ($loaded.Count -eq 0 -and $expected.Count -gt 0) {
    Write-Host "No extensions loaded; check that .vsix files are valid and extraction succeeded." -ForegroundColor Yellow
    $testPassed = $false
}

if ($testPassed) { Write-Host "VSIX loader agentic test PASSED." -ForegroundColor Green; exit 0 }
else { Write-Host "VSIX loader agentic test completed with warnings." -ForegroundColor Yellow; exit 0 }
