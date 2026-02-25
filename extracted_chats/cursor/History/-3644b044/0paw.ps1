# Test-VSIXLoader.ps1 — VSIX loader agentic test (Amazon Q, GitHub Copilot)
# Forwards to Test-VSIXInstall.ps1 and documents IDE wiring: File Explorer + AI Chat.

param(
    [switch]$LaunchIDE,
    [string]$VsixPath,
    [switch]$CreateTestPackage,
    [switch]$CreateTestVsix
)

$scriptDir = $PSScriptRoot
$installScript = Join-Path $scriptDir "Test-VSIXInstall.ps1"
if (-not (Test-Path $installScript)) {
    Write-Host "[Test-VSIXLoader] Test-VSIXInstall.ps1 not found. Run from repo root." -ForegroundColor Red
    exit 1
}

# Alias -CreateTestPackage to -CreateTestVsix for Test-VSIXInstall.ps1
$args = @()
if ($LaunchIDE) { $args += "-LaunchIDE" }
if ($VsixPath)  { $args += "-VsixPath"; $args += $VsixPath }
if ($CreateTestPackage -or $CreateTestVsix) { $args += "-CreateTestVsix" }

& $installScript @args

Write-Host @"

[Test-VSIXLoader] IDE wiring for agentic use:
  • File Explorer: View > Toggle Sidebar (Ctrl+B) — then select Explorer icon in sidebar.
  • AI Chat / Agent: View > AI Chat / Agent (autonomous) (Ctrl+Alt+B).
  • Extensions: View > Toggle Sidebar > Extensions; Install .vsix... for Amazon Q / GitHub Copilot.

"@ -ForegroundColor Gray
