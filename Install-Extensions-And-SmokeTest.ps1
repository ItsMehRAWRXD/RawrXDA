# ============================================================================
# Install-Extensions-And-SmokeTest.ps1 — Copy Copilot/Q into RawrXD extensions, then HTTP smoke test
# ============================================================================
# Copies GitHub Copilot / Amazon Q (or other) extensions from .vscode\extensions or
# .cursor\extensions into %APPDATA%\RawrXD\extensions so the IDE can load them
# without runtime or install-path changes. Then runs SmokeTest-AgenticIDE.ps1.
#
# Usage:
#   .\Install-Extensions-And-SmokeTest.ps1
#   .\Install-Extensions-And-SmokeTest.ps1 -BaseUrl "http://localhost:8080" -CopyFromCursorOnly
# ============================================================================

param(
    [string]$BaseUrl = "http://localhost:8080",
    [switch]$CopyFromCursorOnly,
    [switch]$CopyAll,
    [string]$RepoRoot = $PSScriptRoot
)

$ErrorActionPreference = "Stop"
$extRoot = Join-Path $env:APPDATA "RawrXD\extensions"
if (-not (Test-Path $extRoot)) {
    New-Item -ItemType Directory -Path $extRoot -Force | Out-Null
}

# Source dirs: Cursor first (often has Copilot/Q), then VS Code
$cursorExt = Join-Path $env:USERPROFILE ".cursor\extensions"
$vscodeExt = Join-Path $env:USERPROFILE ".vscode\extensions"
$sources = @()
if (Test-Path $cursorExt) { $sources += $cursorExt }
if (-not $CopyFromCursorOnly -and (Test-Path $vscodeExt)) { $sources += $vscodeExt }

# By default copy only known AI extensions (Copilot, Amazon Q). Use -CopyAll to copy every extension dir.
$pattern = if ($CopyAll) { "*" } else { "github.copilot*", "amazon.q*", "AmazonWebServices.amazon-q*" }
$copied = 0
foreach ($srcDir in $sources) {
    $name = Split-Path $srcDir -Parent | Split-Path -Leaf
    foreach ($p in $pattern) {
        $dirs = Get-ChildItem -Path $srcDir -Directory -Filter $p -ErrorAction SilentlyContinue
        foreach ($d in $dirs) {
            $id = $d.Name
            $dest = Join-Path $extRoot $id
            if (-not (Test-Path $dest) -or ($d.LastWriteTime -gt (Get-Item $dest -ErrorAction SilentlyContinue).LastWriteTime)) {
                try {
                    if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
                    Copy-Item -Path $d.FullName -Destination $dest -Recurse -Force
                    Write-Host "[Extensions] Copied $id from $name to %APPDATA%\RawrXD\extensions" -ForegroundColor Green
                    $copied++
                } catch {
                    Write-Host "[Extensions] Copy failed for $id : $_" -ForegroundColor Yellow
                }
            }
        }
    }
}
if ($copied -eq 0 -and (@(Get-ChildItem $extRoot -Directory -ErrorAction SilentlyContinue).Count -eq 0)) {
    Write-Host "[Extensions] No extensions copied (sources: $($sources -join ', ')). IDE will use only built-in / Ollama until you install from VSIX or copy here." -ForegroundColor Gray
} else {
    Write-Host "[Extensions] RawrXD extension dir: $extRoot" -ForegroundColor Cyan
}

# Run HTTP smoke test
$smokeScript = Join-Path $RepoRoot "SmokeTest-AgenticIDE.ps1"
if (-not (Test-Path $smokeScript)) {
    Write-Host "SmokeTest-AgenticIDE.ps1 not found. Skipping HTTP smoke test." -ForegroundColor Yellow
    exit 0
}
& $smokeScript -BaseUrl $BaseUrl.TrimEnd('/')
