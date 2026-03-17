#!/usr/bin/env pwsh
# Smoke test: validate error dashboard filtering via Win32 messages

$ErrorActionPreference = 'Stop'

# Paths
$repoRoot = Split-Path -Parent $PSScriptRoot  # points to masm_ide
$ideCandidates = @(
  (Join-Path $repoRoot 'build\AgenticIDEWin.exe'),
  (Join-Path $repoRoot 'build\Release\AgenticIDEWin.exe')
)
$ideExe = $ideCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
$logPath  = 'C:\RawrXD\logs\ide_errors.log'

if (-not $ideExe) {
  Write-Host "IDE executable not found. Checked:" -ForegroundColor Red
  $ideCandidates | ForEach-Object { Write-Host "  • $_" -ForegroundColor Yellow }
  Write-Host "Build the IDE (e.g., build_masm.ps1 or cmake --build) and rerun." -ForegroundColor Yellow
  exit 1
}

# Ensure log directory and empty file
New-Item -ItemType Directory -Force -Path (Split-Path $logPath) | Out-Null
Set-Content -Path $logPath -Value '' -NoNewline

# P/Invoke helpers
Add-Type -Namespace Win32 -Name NativeMethods -MemberDefinition @'
  using System;
  using System.Runtime.InteropServices;
  public static class NativeMethods {
    [DllImport("user32.dll", SetLastError=true, CharSet=CharSet.Auto)]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [DllImport("user32.dll", SetLastError=true, CharSet=CharSet.Auto)]
    public static extern IntPtr FindWindowEx(IntPtr hwndParent, IntPtr hwndChildAfter, string lpszClass, string lpszWindow);
    [DllImport("user32.dll", SetLastError=true, CharSet=CharSet.Auto)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
  }
'@

$WM_COMMAND    = 0x0111
$CB_SETCURSEL  = 0x014E
$LB_GETCOUNT   = 0x018B
$CBN_SELCHANGE = 1
$ID_LISTBOX    = 100
$ID_FILTER     = 101

function Get-HandleOrFail {
  param([scriptblock]$expr, [string]$name)
  $h = & $expr
  if ($h -eq [IntPtr]::Zero) {
    throw "Handle not found: $name"
  }
  return $h
}

# Start IDE
$proc = Start-Process -FilePath $ideExe -PassThru
Start-Sleep -Milliseconds 500

try {
  # Wait for dashboard window
  $hDash = [IntPtr]::Zero
  for ($i=0; $i -lt 30 -and $hDash -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 200
    $hDash = [Win32.NativeMethods]::FindWindow('ErrorDashboard', $null)
  }
  if ($hDash -eq [IntPtr]::Zero) { throw 'ErrorDashboard window not found' }

  $hCombo = Get-HandleOrFail { [Win32.NativeMethods]::FindWindowEx($hDash, [IntPtr]::Zero, 'ComboBox', $null) } 'Filter Combo'
  $hList  = Get-HandleOrFail { [Win32.NativeMethods]::FindWindowEx($hDash, [IntPtr]::Zero, 'ListBox', $null) } 'Log ListBox'

  # Append sample log entries after dashboard is running so tail picks them up
  $timestamp = '2025-12-20 12:00:00'
  $lines = @(
    "[$timestamp] [INFO] info msg",
    "[$timestamp] [WARNING] warn msg",
    "[$timestamp] [ERROR] error one",
    "[$timestamp] [ERROR] error two",
    "[$timestamp] [FATAL] fatal msg"
  )
  Add-Content -Path $logPath -Value $lines

  # Give the dashboard timer a moment to read
  Start-Sleep -Seconds 1

  $expected = @{ 0 = 5; 1 = 1; 2 = 1; 3 = 2; 4 = 1 }
  $results = @{}

  foreach ($kvp in $expected.GetEnumerator() | Sort-Object Key) {
    $idx = [int]$kvp.Key
    # Select filter
    [Win32.NativeMethods]::SendMessage($hCombo, $CB_SETCURSEL, [IntPtr]$idx, [IntPtr]::Zero) | Out-Null
    $wParam = (($CBN_SELCHANGE -shl 16) -bor $ID_FILTER)
    [Win32.NativeMethods]::SendMessage($hDash, $WM_COMMAND, [IntPtr]$wParam, $hCombo) | Out-Null
    Start-Sleep -Milliseconds 200
    $count = [Win32.NativeMethods]::SendMessage($hList, $LB_GETCOUNT, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    $results[$idx] = $count
    if ($count -ne $kvp.Value) {
      throw "Filter $idx expected $($kvp.Value) got $count"
    }
  }

  Write-Host "Dashboard smoke test passed." -ForegroundColor Green
  Write-Host "Counts:" ($results.GetEnumerator() | Sort-Object Key | ForEach-Object { "[$($_.Key)]=$($_.Value)" }) -join ', '
  exit 0
}
catch {
  Write-Host "Dashboard smoke test failed: $_" -ForegroundColor Red
  exit 1
}
finally {
  if ($proc -and -not $proc.HasExited) {
    try { $proc.CloseMainWindow() | Out-Null; Start-Sleep -Milliseconds 500 } catch {}
    if (-not $proc.HasExited) { try { Stop-Process -Id $proc.Id -Force } catch {} }
  }
}
