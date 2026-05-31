# Verifies production Win32 IDE sources do not pull Qt headers.
# Exit 0 = clean; exit 1 = Qt pattern found in src/win32app

$ErrorActionPreference = "Stop"
# PSScriptRoot = .../rawrxd/scripts  -> repo root = parent
$root = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $root "src\win32app"))) {
    $root = "D:\rawrxd"
}
$win32 = Join-Path $root "src\win32app"
if (-not (Test-Path $win32)) {
    Write-Host "Skip: $win32 not found"
    exit 0
}

$patterns = @(
    '#include\s*<Q[A-Z]',
    '#include\s*<Qt',
    'Qt6::',
    'Qt5::',
    '\bQ_OBJECT\b',
    '\bQApplication\b',
    '\bQString\b'
)

$bad = @()
Get-ChildItem -Path $win32 -Recurse -Include *.cpp,*.h,*.hpp -File | ForEach-Object {
    $text = Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $text) { return }
    foreach ($p in $patterns) {
        # Case-sensitive: avoid false positives on #include <queue> vs Qt's Q*
        if ($text -cmatch $p) {
            $bad += "$($_.FullName): matches $p"
        }
    }
}

if ($bad.Count -gt 0) {
    Write-Host "Qt-related pattern in Win32 IDE tree:" -ForegroundColor Red
    $bad | ForEach-Object { Write-Host $_ }
    exit 1
}

Write-Host "OK: no Qt patterns in src/win32app" -ForegroundColor Green
exit 0
