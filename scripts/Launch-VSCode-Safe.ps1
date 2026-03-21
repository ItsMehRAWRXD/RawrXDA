#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Launch Visual Studio Code with quoted paths and optional troubleshooting flags.

.PARAMETER InstallRoot
  Folder containing Code.exe (default: D:\VS Code).

.PARAMETER DisableGpu
  Pass --disable-gpu only (VS Code warns on some Chromium-only flags; avoid those).

.PARAMETER DisableExtensions
  Pass --disable-extensions to rule out PowerShell / other extensions breaking the workbench.

.PARAMETER FreshUserData
  Use a new empty user-data directory under %TEMP% (isolates corrupt Code\User / workspaceStorage / GPU cache).

.PARAMETER DisableWinOcclusion
  Pass --disable-features=CalculateNativeWinOcclusion (fixes many "title bar/theme OK, workbench blank" cases on Windows / Chromium).

.PARAMETER MaxCompat
  Shorthand: -DisableGpu -DisableExtensions -FreshUserData -DisableWinOcclusion (use when the window is still blank after simpler tries).

.PARAMETER CloseExistingCode
  Force-stop all running Code.exe processes before launch. UNSAVED WORK in any VS Code window will be lost — use when you see "Error mutex already exists" or a stuck background Code.exe.

.PARAMETER Args
  Additional arguments for Code (e.g. a folder path).

.EXAMPLE
  .\Launch-VSCode-Safe.ps1 -DisableGpu
.EXAMPLE
  # Still blank: Windows occlusion bug + clean profile + no extensions:
  .\Launch-VSCode-Safe.ps1 -MaxCompat
.EXAMPLE
  # Only occlusion workaround (try this first if UI chrome paints but center is empty):
  .\Launch-VSCode-Safe.ps1 -DisableWinOcclusion
.EXAMPLE
  .\Launch-VSCode-Safe.ps1 -InstallRoot 'D:\VSCode' -Args @('.')
#>
param(
    [string] $InstallRoot = 'D:\VS Code',
    [switch] $DisableGpu,
    [switch] $DisableExtensions,
    [switch] $FreshUserData,
    [switch] $DisableWinOcclusion,
    [switch] $MaxCompat,
    [switch] $CloseExistingCode,
    [string[]] $Args = @()
)

$userDataDirForLogs = $null

if ($MaxCompat) {
    $DisableGpu = $true
    $DisableExtensions = $true
    $FreshUserData = $true
    $DisableWinOcclusion = $true
    Write-Host "MaxCompat: GPU off, extensions off, fresh profile, Win occlusion disabled" -ForegroundColor Magenta
}

$exe = Join-Path $InstallRoot 'Code.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    Write-Error "Code.exe not found at: $exe`nInstall VS Code or set -InstallRoot to the folder that contains Code.exe."
    exit 1
}

$launchArgs = [System.Collections.Generic.List[string]]::new()

if ($FreshUserData) {
    $profileDir = Join-Path $env:TEMP ("vscode-clean-profile-" + [Guid]::NewGuid().ToString('n').Substring(0, 8))
    New-Item -ItemType Directory -Path $profileDir -Force | Out-Null
    $launchArgs.Add('--user-data-dir')
    $launchArgs.Add($profileDir)
    Write-Host "Fresh profile: $profileDir" -ForegroundColor Yellow
}

if ($DisableGpu) {
    $launchArgs.Add('--disable-gpu')
}

if ($DisableExtensions) {
    $launchArgs.Add('--disable-extensions')
}

if ($DisableWinOcclusion) {
    $launchArgs.Add('--disable-features=CalculateNativeWinOcclusion')
}

foreach ($a in $Args) { $launchArgs.Add($a) }

$runningCode = Get-Process -Name 'Code' -ErrorAction SilentlyContinue
if ($runningCode) {
    Write-Host "Note: Code.exe is already running (PID(s): $($runningCode.Id -join ', ')). A second launch can trigger 'Error mutex already exists'. Quit all VS Code windows or use -CloseExistingCode (loses unsaved work)." -ForegroundColor Yellow
}

if ($CloseExistingCode) {
    Write-Warning 'CloseExistingCode: stopping all Code.exe processes (unsaved work will be lost).'
    Get-Process -Name 'Code' -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 800
}

Write-Host "Starting: $exe" -ForegroundColor Cyan
if ($launchArgs.Count -gt 0) {
    Write-Host ("Args: " + ($launchArgs -join ' ')) -ForegroundColor DarkGray
}

$p = Start-Process -FilePath $exe -ArgumentList $launchArgs -PassThru
Write-Host "PID: $($p.Id)" -ForegroundColor Green

$logHint = if ($userDataDirForLogs) {
    Join-Path $userDataDirForLogs 'logs'
} else {
    Join-Path $env:APPDATA 'Code\logs'
}
Write-Host "If still blank: check renderer logs under: $logHint" -ForegroundColor DarkGray
Write-Host "Chromium flags may print 'not in the list of known options' — that is OK; they are still passed to Electron." -ForegroundColor DarkGray
Write-Host "Or run: & `"$exe`" --disable-features=CalculateNativeWinOcclusion --verbose" -ForegroundColor DarkGray
