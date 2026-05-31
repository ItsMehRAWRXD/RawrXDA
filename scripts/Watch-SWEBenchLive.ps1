param(
    [string]$LogPath = "",
    [int]$Tail = 40,
    [switch]$Raw
)

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $LogPath) {
    foreach ($rel in @(
            "build-ninja-ctx2\bin\swe_live_debug_stdout.txt",
            "build-win32\bin\swe_live_debug_stdout.txt",
            "build-ninja\bin\swe_live_debug_stdout.txt")) {
        $c = Join-Path $repoRoot $rel
        if (Test-Path -LiteralPath $c) { $LogPath = $c; break }
    }
}
if (-not $LogPath) {
    $LogPath = Join-Path $repoRoot "build-ninja-ctx2\bin\swe_live_debug_stdout.txt"
}

if (-not (Test-Path -LiteralPath $LogPath)) {
    Write-Host "[watch] waiting for log file: $LogPath" -ForegroundColor Yellow
    while (-not (Test-Path -LiteralPath $LogPath)) {
        Start-Sleep -Milliseconds 500
    }
}

Write-Host "[watch] tailing $LogPath" -ForegroundColor Cyan
Write-Host "[watch] highlighting HTTP status and budget/adaptation lines" -ForegroundColor Cyan

Get-Content -LiteralPath $LogPath -Tail $Tail -Wait | ForEach-Object {
    $line = $_
    if ($Raw) {
        Write-Host $line
        return
    }

    if ($line -match "\[SWE\]\[HTTP\].*status=.*\b200\b") {
        Write-Host $line -ForegroundColor Green
    }
    elseif ($line -match "\[SWE\]\[HTTP\].*status=") {
        Write-Host $line -ForegroundColor Red
    }
    elseif ($line -match "\[SWE\]\[BUDGET\].*adapted=true") {
        Write-Host $line -ForegroundColor Yellow
    }
    elseif ($line -match "\[SWE\]\[BUDGET\]") {
        Write-Host $line -ForegroundColor DarkCyan
    }
    elseif ($line -match "reason:|FAILED") {
        Write-Host $line -ForegroundColor Magenta
    }
    else {
        Write-Host $line -ForegroundColor Gray
    }
}
