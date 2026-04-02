# Quick parity check for Win32 IDE inference / VMM messages.
# Primary log: %APPDATA%\RawrXD\ide.log (see Win32IDE_Core.cpp deferredHeavyInitBody).
# [STEP] layer heartbeats: also mirrored to OutputDebugString (rawrxd_transformer.cpp) — use DebugView or VS Output while inferring.

$ErrorActionPreference = "Stop"
$log = Join-Path $env:APPDATA "RawrXD\ide.log"
if (-not (Test-Path -LiteralPath $log)) {
    Write-Host "Log not found: $log"
    Write-Host "Launch IDE once (deferred init creates RawrXD dir and opens ide.log)."
    exit 1
}

$patterns = @(
    "locked window too small",
    "[Forward] FATAL",
    "infer: stage=done status=fail",
    '\bOOM\b',
    "StreamingPin failed",
    "infer: ok",
    "EXIT=0"
)

Write-Host "Scanning: $log"
foreach ($p in $patterns) {
    $hits = Select-String -LiteralPath $log -Pattern $p -ErrorAction SilentlyContinue
    if ($hits) {
        Write-Host "--- matches: $p ---"
        $hits | Select-Object -Last 8 | ForEach-Object { $_.Line }
    }
}

Write-Host ""
Write-Host "Tip: [STEP] lines also appear in the IDE Output tab (WM_IDE_OUTPUT_APPEND_SAFE). ODS/DebugView still works for attach-debug sessions."
Write-Host "Full sequence: pwsh -File $($PSScriptRoot)\validate_p0_p1_inference_sequence.ps1 -ModelPath <path\to\model.gguf>"
