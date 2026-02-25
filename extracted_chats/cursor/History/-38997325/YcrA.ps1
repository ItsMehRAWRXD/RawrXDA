# Test-VSIXLoader.ps1
# Agentic test for VSIX loader: Amazon Q and GitHub Copilot extensions.
# Run from repo root. Requires RawrXD IDE built and (optionally) .vsix files in plugins/.

param(
    [string]$PluginsDir = "plugins",
    [string]$AmazonQVsix = "",
    [string]$GitHubCopilotVsix = "",
    [switch]$CreateTestPackage
)

$ErrorActionPreference = "Stop"
$script:Pass = 0
$script:Fail = 0

function Write-TestResult { param([string]$Name, [bool]$Ok, [string]$Detail = "")
    if ($Ok) { $script:Pass++; Write-Host "  [PASS] $Name" -ForegroundColor Green; if ($Detail) { Write-Host "         $Detail" -ForegroundColor Gray } }
    else     { $script:Fail++; Write-Host "  [FAIL] $Name" -ForegroundColor Red;   if ($Detail) { Write-Host "         $Detail" -ForegroundColor Gray } }
}

Write-Host "`n=== RawrXD VSIX Loader — Agentic Test ===" -ForegroundColor Cyan
Write-Host "  Plugins dir: $PluginsDir" -ForegroundColor Gray
Write-Host "  Amazon Q .vsix: $(if ($AmazonQVsix) { $AmazonQVsix } else { 'not set' })" -ForegroundColor Gray
Write-Host "  GitHub Copilot .vsix: $(if ($GitHubCopilotVsix) { $GitHubCopilotVsix } else { 'not set' })" -ForegroundColor Gray

# 1. Create minimal test package (VS Code package.json) to validate loader contract
if ($CreateTestPackage) {
    $testExtDir = Join-Path $PluginsDir "test-vscode-extension"
    $null = New-Item -ItemType Directory -Force -Path $testExtDir
    $packageJson = @"
{
  "name": "test-agentic-extension",
  "version": "0.0.1",
  "displayName": "Test Agentic Extension",
  "description": "Minimal package for VSIX loader test",
  "publisher": "rawrxd-test",
  "main": "",
  "contributes": {
    "commands": [
      { "command": "test.hello", "title": "Hello" }
    ]
  }
}
"@
    Set-Content -Path (Join-Path $testExtDir "package.json") -Value $packageJson -Encoding UTF8
    Write-TestResult "Create test package (package.json)" (Test-Path (Join-Path $testExtDir "package.json")) "Path: $testExtDir"
}

# 2. Check plugins directory and optional .vsix files
$pluginsPath = $PluginsDir
if (-not [System.IO.Path]::IsPathRooted($PluginsDir)) {
    $pluginsPath = Join-Path $PSScriptRoot ".." $PluginsDir
}
$pluginsPath = [System.IO.Path]::GetFullPath($pluginsPath)
Write-TestResult "Plugins directory exists" (Test-Path $pluginsPath) $pluginsPath

$amazonQPath = $null
if ($AmazonQVsix) {
    $amazonQPath = if ([System.IO.Path]::IsPathRooted($AmazonQVsix)) { $AmazonQVsix } else { Join-Path $pluginsPath $AmazonQVsix }
    Write-TestResult "Amazon Q .vsix present" (Test-Path $amazonQPath) $amazonQPath
}
$copilotPath = $null
if ($GitHubCopilotVsix) {
    $copilotPath = if ([System.IO.Path]::IsPathRooted($GitHubCopilotVsix)) { $GitHubCopilotVsix } else { Join-Path $pluginsPath $GitHubCopilotVsix }
    Write-TestResult "GitHub Copilot .vsix present" (Test-Path $copilotPath) $copilotPath
}

# 3. Instructions for IDE-based agentic test
Write-Host "`n--- How to test in IDE (agentic) ---" -ForegroundColor Cyan
Write-Host "  1. View → Toggle Sidebar (File Explorer)  (Ctrl+B)" -ForegroundColor Gray
Write-Host "  2. Click Activity Bar 'Extensions' (Ctrl+Shift+X)" -ForegroundColor Gray
Write-Host "  3. Place .vsix in the plugins folder, then:" -ForegroundColor Gray
Write-Host "     - Install by extension id (e.g. AmazonQ or GitHub.copilot) or" -ForegroundColor Gray
Write-Host "     - Use 'Load Plugin...' and select the .vsix or extracted folder." -ForegroundColor Gray
Write-Host "  4. AI Chat: View → AI Chat (Ctrl+Alt+B) for agent/autonomous use." -ForegroundColor Gray
Write-Host "  Plugins folder (relative to exe): $PluginsDir" -ForegroundColor Gray
Write-Host ""

# Summary
Write-Host "=== VSIX Loader Test Summary ===" -ForegroundColor Cyan
Write-Host "  Passed: $script:Pass" -ForegroundColor Green
Write-Host "  Failed: $script:Fail" -ForegroundColor $(if ($script:Fail -gt 0) { "Red" } else { "Green" })
if ($script:Fail -eq 0) {
    Write-Host "  OK. Load Amazon Q / GitHub Copilot .vsix via IDE Extensions view." -ForegroundColor Green
} else {
    exit 1
}
