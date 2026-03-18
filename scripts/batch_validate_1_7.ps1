param(
    [string]$ExePath = "d:\rawrxd\build_smoke_auto\bin\RawrXD-Win32IDE.exe",
    [string]$OutFile = "d:\rawrxd\reports\batch_1_7_runtime_validation.json"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ExePath)) {
    throw "Executable not found: $ExePath"
}

$outDir = Split-Path -Parent $OutFile
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$tests = @(
    @{ id = 1; name = "ghost_text_rendering"; flag = "--test-ghost-text" },
    @{ id = 2; name = "multi_cursor_visuals"; flag = "--test-multicursor" },
    @{ id = 3; name = "peek_overlay"; flag = "--test-peek-view" },
    @{ id = 4; name = "caret_animation"; flag = "--test-caret-animation" },
    @{ id = 5; name = "tier2_tier3_cosmetics"; flag = "--test-tier-cosmetics" },
    @{ id = 6; name = "agent_ollama_client"; flag = "--test-ollama-client" },
    @{ id = 7; name = "model_discovery"; flag = "--test-model-discovery" }
)

$results = @()

foreach ($t in $tests) {
    Write-Host "[batch-1-7] Running $($t.name) ($($t.flag))" -ForegroundColor Cyan
    $start = Get-Date
    $proc = Start-Process -FilePath $ExePath -ArgumentList $t.flag -PassThru -Wait
    $end = Get-Date

    $ok = ($proc.ExitCode -eq 0)
    $results += [pscustomobject]@{
        id = $t.id
        name = $t.name
        flag = $t.flag
        exitCode = $proc.ExitCode
        passed = $ok
        startedAtUtc = $start.ToUniversalTime().ToString("o")
        finishedAtUtc = $end.ToUniversalTime().ToString("o")
        durationMs = [int](($end - $start).TotalMilliseconds)
    }

    if ($ok) {
        Write-Host "[batch-1-7] PASS: $($t.name)" -ForegroundColor Green
    }
    else {
        Write-Host "[batch-1-7] FAIL: $($t.name) (exit $($proc.ExitCode))" -ForegroundColor Red
    }
}

$summary = [pscustomobject]@{
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    exePath = $ExePath
    total = $results.Count
    passed = ($results | Where-Object { $_.passed }).Count
    failed = ($results | Where-Object { -not $_.passed }).Count
    results = $results
}

$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $OutFile -Encoding UTF8
Write-Host "[batch-1-7] Saved report: $OutFile" -ForegroundColor Yellow

if ($summary.failed -gt 0) {
    exit 1
}

exit 0
