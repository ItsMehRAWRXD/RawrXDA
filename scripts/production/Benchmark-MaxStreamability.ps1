param(
    [string]$ExePath = "d:/rawrxd/build/bin/RawrXD-Win32IDE.exe",
    [string]$ModelPath = "d:/ministral3.gguf",
    [string]$Prompt = "Generate a long technical checklist with numbered items and short explanations. Continue until token limit.",
    [int]$StartTokens = 128,
    [int]$MaxTokensCeiling = 8192,
    [double]$StepMultiplier = 1.5,
    [ValidateRange(1,5)]
    [int]$RunsPerPoint = 2,
    [ValidateRange(30,1800)]
    [int]$BaselineTimeoutSeconds = 300,
    [ValidateRange(0,1800)]
    [int]$OneAdditionTimeoutBoostSeconds = 120,
    [string]$OutJson = "d:/rawrxd/reports/14day/day10_max_streamability_benchmark.json",
    [string]$OutMarkdown = "d:/rawrxd/reports/14day/day10_max_streamability_benchmark.md"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-TokenEstimate {
    param([string]$Text)

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return 0
    }

    $matches = [regex]::Matches($Text, "[A-Za-z0-9_]+")
    return $matches.Count
}

function Get-TextFileOrEmpty {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }

    # Force plain text to keep JSON reports stable and compact.
    return [string](Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue)
}

function Invoke-StreamProbe {
    param(
        [int]$MaxTokens,
        [int]$TimeoutSeconds
    )

    $tmpOut = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), ("rawrxd_stream_out_{0}.log" -f ([guid]::NewGuid().ToString("N"))))
    $tmpErr = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), ("rawrxd_stream_err_{0}.log" -f ([guid]::NewGuid().ToString("N"))))

    $timedOut = $false
    $firstByteMs = $null
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    try {
        $args = @(
            "--prompt", $Prompt,
            "--model", $ModelPath,
            "--max-tokens", $MaxTokens,
            "--temperature", "0.2",
            "--no-server",
            "--json"
        )

        $proc = Start-Process -FilePath $ExePath -ArgumentList $args -PassThru -NoNewWindow -RedirectStandardOutput $tmpOut -RedirectStandardError $tmpErr

        while (-not $proc.HasExited) {
            if ($firstByteMs -eq $null -and (Test-Path -LiteralPath $tmpOut)) {
                $len = (Get-Item -LiteralPath $tmpOut).Length
                if ($len -gt 0) {
                    $firstByteMs = [double]$sw.ElapsedMilliseconds
                }
            }

            if ($sw.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                $timedOut = $true
                try { Stop-Process -Id $proc.Id -Force } catch {}
                break
            }

            Start-Sleep -Milliseconds 20
        }

        if (-not $timedOut) {
            # Ensure ExitCode is populated before scoring the run.
            $proc.WaitForExit()
        }

        $sw.Stop()

        $stdout = Get-TextFileOrEmpty -Path $tmpOut
        $stderr = Get-TextFileOrEmpty -Path $tmpErr

        if ($firstByteMs -eq $null) {
            $firstByteMs = [double]$sw.ElapsedMilliseconds
        }

        $tokenEstimate = Get-TokenEstimate -Text $stdout
        $streamMs = [Math]::Max(1.0, [double]$sw.ElapsedMilliseconds - [double]$firstByteMs)
        $tps = if ($tokenEstimate -gt 0) { [double]$tokenEstimate / ($streamMs / 1000.0) } else { 0.0 }
        $exitCode = if ($timedOut) { 124 } else { [int]$proc.ExitCode }
        $exitCodeHex = ('0x{0:X8}' -f ([BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$exitCode), 0)))
        $stderrTailText = if ($stderr.Length -gt 2048) { $stderr.Substring($stderr.Length - 2048) } else { $stderr }

        return [ordered]@{
            maxTokens = $MaxTokens
            timeoutSeconds = $TimeoutSeconds
            exitCode = $exitCode
            exitCodeHex = $exitCodeHex
            timedOut = $timedOut
            elapsedMs = [Math]::Round([double]$sw.ElapsedMilliseconds, 2)
            ttftMs = [Math]::Round([double]$firstByteMs, 2)
            tokenEstimate = $tokenEstimate
            tpsEstimate = [Math]::Round([double]$tps, 4)
            stderrTail = $stderrTailText
            success = (($exitCode -eq 0) -and (-not $timedOut) -and ($tokenEstimate -gt 0))
        }
    }
    finally {
        if (Test-Path -LiteralPath $tmpOut) { Remove-Item -LiteralPath $tmpOut -Force -ErrorAction SilentlyContinue }
        if (Test-Path -LiteralPath $tmpErr) { Remove-Item -LiteralPath $tmpErr -Force -ErrorAction SilentlyContinue }
    }
}

function Get-TokenSweep {
    param(
        [int]$Start,
        [int]$Ceiling,
        [double]$Multiplier
    )

    $tokens = New-Object System.Collections.Generic.List[int]
    $current = [Math]::Max(1, $Start)

    while ($current -le $Ceiling) {
        if (-not $tokens.Contains($current)) {
            $tokens.Add($current)
        }

        $next = [int][Math]::Ceiling($current * $Multiplier)
        if ($next -le $current) {
            $next = $current + 1
        }
        $current = $next
    }

    if (-not $tokens.Contains($Ceiling)) {
        $tokens.Add($Ceiling)
    }

    return $tokens.ToArray()
}

function Invoke-SweepLane {
    param(
        [string]$LaneName,
        [int]$TimeoutSeconds
    )

    $points = @()
    $maxStable = 0
    $maxTps = 0.0
    $consecutiveUnstable = 0

    foreach ($tokens in (Get-TokenSweep -Start $StartTokens -Ceiling $MaxTokensCeiling -Multiplier $StepMultiplier)) {
        $runs = @()
        for ($i = 0; $i -lt $RunsPerPoint; $i++) {
            $runs += Invoke-StreamProbe -MaxTokens $tokens -TimeoutSeconds $TimeoutSeconds
        }

        $successes = @($runs | Where-Object { $_.success }).Count
        $isStable = ($successes -ge [Math]::Ceiling($RunsPerPoint / 2.0))
        $medianTps = 0.0
        $tpsValues = @($runs | ForEach-Object { [double]$_.tpsEstimate } | Sort-Object)
        if ($tpsValues.Count -gt 0) {
            $mid = [int][Math]::Floor($tpsValues.Count / 2)
            if (($tpsValues.Count % 2) -eq 0) {
                $medianTps = ($tpsValues[$mid - 1] + $tpsValues[$mid]) / 2.0
            } else {
                $medianTps = $tpsValues[$mid]
            }
        }

        $point = [ordered]@{
            tokensRequested = $tokens
            stable = $isStable
            successCount = $successes
            runCount = $RunsPerPoint
            medianTps = [Math]::Round($medianTps, 4)
            runs = $runs
        }
        $points += $point

        if ($isStable) {
            $maxStable = [Math]::Max($maxStable, $tokens)
            $maxTps = [Math]::Max($maxTps, [double]$medianTps)
            $consecutiveUnstable = 0
        } else {
            $consecutiveUnstable += 1
            if (($tokens -gt $maxStable) -and ($consecutiveUnstable -ge 2)) {
                break
            }
        }
    }

    return [ordered]@{
        lane = $LaneName
        timeoutSeconds = $TimeoutSeconds
        maxStableTokens = $maxStable
        peakMedianTps = [Math]::Round($maxTps, 4)
        points = $points
    }
}

if (-not (Test-Path -LiteralPath $ExePath)) {
    Write-Error "executable not found: $ExePath"
}

if (-not (Test-Path -LiteralPath $ModelPath)) {
    Write-Error "model not found: $ModelPath"
}

$reportDir = Split-Path -Parent $OutJson
if ($reportDir -and (-not (Test-Path -LiteralPath $reportDir))) {
    New-Item -Path $reportDir -ItemType Directory -Force | Out-Null
}

$baseline = Invoke-SweepLane -LaneName "baseline_current" -TimeoutSeconds $BaselineTimeoutSeconds
$oneAdditionTimeout = $BaselineTimeoutSeconds + $OneAdditionTimeoutBoostSeconds
$oneAddition = Invoke-SweepLane -LaneName "one_addition_timeout_budget" -TimeoutSeconds $oneAdditionTimeout

$upliftTokens = [int]$oneAddition.maxStableTokens - [int]$baseline.maxStableTokens
$upliftPct = if ($baseline.maxStableTokens -gt 0) {
    [Math]::Round((100.0 * $upliftTokens) / [double]$baseline.maxStableTokens, 2)
} else {
    0.0
}

$recommendedAddition = "Add adaptive stream timeout budget: timeout = base + ceil(max_tokens / observed_tps) with bounded cap."

$payload = [ordered]@{
    generatedUtc = (Get-Date).ToUniversalTime().ToString("o")
    exePath = $ExePath
    modelPath = $ModelPath
    sweep = [ordered]@{
        startTokens = $StartTokens
        maxTokensCeiling = $MaxTokensCeiling
        stepMultiplier = $StepMultiplier
        runsPerPoint = $RunsPerPoint
    }
    baseline = $baseline
    oneAddition = $oneAddition
    comparison = [ordered]@{
        baselineMaxStableTokens = [int]$baseline.maxStableTokens
        oneAdditionMaxStableTokens = [int]$oneAddition.maxStableTokens
        upliftTokens = $upliftTokens
        upliftPercent = $upliftPct
        oneAdditionDescription = $recommendedAddition
    }
}

$payload | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $OutJson -Encoding UTF8

$md = @()
$md += "# Day 10 Max Streamability Benchmark"
$md += ""
$md += "- Generated UTC: $($payload.generatedUtc)"
$md += "- Executable: $ExePath"
$md += "- Model: $ModelPath"
$md += ""
$md += "## Baseline Lane"
$md += "- Timeout seconds: $($baseline.timeoutSeconds)"
$md += "- Max stable tokens: $($baseline.maxStableTokens)"
$md += "- Peak median TPS: $($baseline.peakMedianTps)"
$md += ""
$md += "## One-Addition Lane"
$md += "- Timeout seconds: $($oneAddition.timeoutSeconds)"
$md += "- Max stable tokens: $($oneAddition.maxStableTokens)"
$md += "- Peak median TPS: $($oneAddition.peakMedianTps)"
$md += ""
$md += "## Comparison"
$md += "- Baseline max stable tokens: $($payload.comparison.baselineMaxStableTokens)"
$md += "- One-addition max stable tokens: $($payload.comparison.oneAdditionMaxStableTokens)"
$md += "- Uplift tokens: $($payload.comparison.upliftTokens)"
$md += "- Uplift percent: $($payload.comparison.upliftPercent)%"
$md += "- One addition: $($payload.comparison.oneAdditionDescription)"
$md += ""
$md += "## Sweep Points"
$md += "| Lane | Tokens | Stable | Successes | Median TPS |"
$md += "| --- | ---: | :---: | ---: | ---: |"
foreach ($lane in @($baseline, $oneAddition)) {
    foreach ($p in $lane.points) {
        $md += "| $($lane.lane) | $($p.tokensRequested) | $($p.stable) | $($p.successCount)/$($p.runCount) | $($p.medianTps) |"
    }
}

$md -join "`r`n" | Set-Content -LiteralPath $OutMarkdown -Encoding UTF8

Write-Host ("BENCHMARK_JSON={0}" -f $OutJson)
Write-Host ("BENCHMARK_MD={0}" -f $OutMarkdown)
Write-Host ("BASELINE_MAX_STABLE={0}" -f $baseline.maxStableTokens)
Write-Host ("ONE_ADDITION_MAX_STABLE={0}" -f $oneAddition.maxStableTokens)
Write-Host ("UPLIFT_TOKENS={0}" -f $upliftTokens)
