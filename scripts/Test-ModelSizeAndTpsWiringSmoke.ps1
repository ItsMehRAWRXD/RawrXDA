$ErrorActionPreference = "Stop"
#
# Source-level smoke: asserts model size + load wall-ms are wired into status bar UX
# and that model load completion triggers a status refresh (reverse-engineered parity).
#
# Run: pwsh -NoProfile -File scripts/Test-ModelSizeAndTpsWiringSmoke.ps1
#

$repoRoot = Split-Path -Parent $PSScriptRoot

function Assert-FileContains([string]$relPath, [string]$pattern, [string]$desc) {
    $p = Join-Path $repoRoot $relPath
    if (-not (Test-Path -LiteralPath $p)) { throw "Missing $relPath" }
    $raw = Get-Content -LiteralPath $p -Raw
    if ($raw -notmatch $pattern) { throw "FAIL: $desc ($relPath)" }
}

Assert-FileContains "src\win32app\Win32IDE.h" 'm_lastLoadedModelBytes' "Win32IDE tracks last loaded model bytes"
Assert-FileContains "src\win32app\Win32IDE.h" 'm_lastLoadedModelWallMs' "Win32IDE tracks last loaded model wall-ms"
Assert-FileContains "src\win32app\Win32IDE_Core.cpp" 'model\.file_bytes' "WM_MODEL_LOAD_DONE records model.file_bytes gauge"
Assert-FileContains "src\win32app\Win32IDE_Core.cpp" 'WM_STATUSBAR_REFRESH_COPILOT' "WM_MODEL_LOAD_DONE triggers statusbar refresh"
Assert-FileContains "src\win32app\Win32IDE_VSCodeUI.cpp" 'GB' "Status bar renders model size in GB"
Assert-FileContains "src\win32app\Win32IDE_VSCodeUI.cpp" 't/s est' "Status bar still renders ~t/s estimate text"

Write-Host "OK: model size + TPS wiring smoke."
exit 0

