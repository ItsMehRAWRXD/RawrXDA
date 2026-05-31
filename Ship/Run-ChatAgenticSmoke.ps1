# Runs RawrXD agentic parity smoke: offline AgentToolRegistry read_file, optional live Ollama when RAWRXD_AGENTIC_SMOKE_LIVE=1.
# Usage: .\Ship\Run-ChatAgenticSmoke.ps1 [-ExePath <path>]
# Writes logs\agentic_smoke_last.json at repo root (from PSScriptRoot parent).

param(
    [string] $ExePath = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$artifactDir = Join-Path $repoRoot "logs"
$artifactPath = Join-Path $artifactDir "agentic_smoke_last.json"

$exe = $null
if ($ExePath -and (Test-Path -LiteralPath $ExePath)) {
    $exe = $ExePath
}
if (-not $exe) {
    if ($env:RAWRXD_EXE -and (Test-Path -LiteralPath $env:RAWRXD_EXE)) {
        $exe = $env:RAWRXD_EXE
    }
}
if (-not $exe) {
    foreach ($c in @(
            (Join-Path $repoRoot "build-win32\bin\Release\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build-win32\bin\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build-ninja\bin\Release\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build-ninja\bin\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build-win32\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build-ninja-ctx2\bin\Release\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build-ninja-ctx2\bin\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build-ninja\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build-ninja-ctx2\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build\bin\Release\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build\bin\Debug\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "build\bin\RawrXD-Win32IDE.exe"),
            (Join-Path $repoRoot "bin\RawrXD-Win32IDE.exe"))) {
        if (Test-Path -LiteralPath $c) { $exe = $c; break }
    }
}
if (-not $exe) {
    Write-Error "RawrXD-Win32IDE.exe not found. Build the IDE (cmake --build ... --target RawrXD-Win32IDE) or pass -ExePath or set RAWRXD_EXE."
    exit 1
}

Write-Host "Run-ChatAgenticSmoke: $exe" -ForegroundColor Cyan
$p = Start-Process -FilePath $exe -ArgumentList @("--agentic-smoke") -NoNewWindow -Wait -PassThru
$ok = ($p.ExitCode -eq 0)
$payload = [ordered]@{
    ok         = $ok
    exitCode   = $p.ExitCode
    exe        = $exe
    timestamp  = (Get-Date).ToString("o")
    liveOllama = ($env:RAWRXD_AGENTIC_SMOKE_LIVE -eq "1")
}
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
($payload | ConvertTo-Json -Compress) | Set-Content -LiteralPath $artifactPath -Encoding utf8
Write-Host "Wrote $artifactPath exit=$($p.ExitCode)" -ForegroundColor $(if ($ok) { "Green" } else { "Red" })
exit $p.ExitCode
