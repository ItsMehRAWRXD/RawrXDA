[CmdletBinding()]
param(
    [string]$ExePath = "d:/rawrxd/build_ninja/bin/RawrXD-Win32IDE.exe",
    [int]$ColdRuns = 10,
    [int]$WarmRuns = 10,
    [int]$ColdCooldownSeconds = 5,
    [int]$TimeoutSeconds = 45,
    [string]$OutDir = "d:/rawrxd/reports/14day",
    [string]$Tag = "camellia_e0_matrix"
)

$ErrorActionPreference = "Stop"

function Stop-RawrXDProcess {
    Get-Process RawrXD-Win32IDE -ErrorAction SilentlyContinue | ForEach-Object {
        try {
            Stop-Process -Id $_.Id -Force -ErrorAction Stop
        } catch {
        }
    }
}

function Get-TracePath {
    $tmp = [System.IO.Path]::GetTempPath()
    return (Join-Path $tmp "rawrxd_camellia_e0_trace.log")
}

function Read-AppendedTrace {
    param(
        [string]$TracePath,
        [long]$StartLength
    )

    if (-not (Test-Path $TracePath)) {
        return ""
    }

    $fi = Get-Item -LiteralPath $TracePath
    if ($fi.Length -le $StartLength) {
        return ""
    }

    $fs = [System.IO.File]::Open($TracePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        $null = $fs.Seek($StartLength, [System.IO.SeekOrigin]::Begin)
        $sr = New-Object System.IO.StreamReader($fs)
        $text = $sr.ReadToEnd()
        $sr.Dispose()
        return $text
    } finally {
        $fs.Dispose()
    }
}

function Get-LastCamelliaLine {
    param([string]$Text)

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return ""
    }

    $lines = $Text -split "`r?`n"
    for ($i = $lines.Length - 1; $i -ge 0; $i--) {
        if ($lines[$i] -match "\[Camellia\]\[E0") {
            return $lines[$i]
        }
    }
    return ""
}

function Get-HexField {
    param(
        [string]$Line,
        [string]$Name,
        [string]$DefaultValue = "0x00000000"
    )

    $pattern = [regex]::Escape($Name) + "=(0x[0-9a-fA-F]+)"
    $m = [regex]::Match($Line, $pattern)
    if ($m.Success) {
        return $m.Groups[1].Value.ToLowerInvariant()
    }
    return $DefaultValue
}

function Get-TokenField {
    param(
        [string]$Line,
        [string]$Name,
        [string]$DefaultValue = ""
    )

    $pattern = [regex]::Escape($Name) + "=([^\s;]+)"
    $m = [regex]::Match($Line, $pattern)
    if ($m.Success) {
        return $m.Groups[1].Value
    }
    return $DefaultValue
}

function Get-FailureBucket {
    param(
        [string]$ActualHex,
        [string]$Mode,
        [hashtable]$Frequency,
        [int]$Failures,
        [int]$WarmFailures,
        [string]$HmacRc
    )

    if ($ActualHex -eq "0x00000000") {
        return "PASS"
    }

    $sameCodeCount = 0
    if ($Frequency.ContainsKey($ActualHex)) {
        $sameCodeCount = [int]$Frequency[$ActualHex]
    }

    if ($sameCodeCount -ge 3 -and $Failures -ge 3) {
        return "A:deterministic-data-corruption"
    }

    if ($Mode -eq "warm" -and $WarmFailures -ge 2) {
        return "B:timing-race-warm-bias"
    }

    if ($HmacRc -eq "0") {
        return "C:instruction-path-mismatch"
    }

    return "B:timing-race-intermittent"
}

if (-not (Test-Path $ExePath)) {
    throw "Executable not found: $ExePath"
}

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outJson = Join-Path $OutDir ("{0}_{1}.json" -f $Tag, $stamp)
$outCsv = Join-Path $OutDir ("{0}_{1}.csv" -f $Tag, $stamp)

$tracePath = Get-TracePath
if (-not (Test-Path $tracePath)) {
    New-Item -ItemType File -Path $tracePath -Force | Out-Null
}

$results = New-Object System.Collections.Generic.List[object]
$frequency = @{}
$failures = 0
$warmFailures = 0
$runIndex = 0

$phases = @(
    @{ Mode = "cold"; Count = $ColdRuns; Cooldown = $ColdCooldownSeconds },
    @{ Mode = "warm"; Count = $WarmRuns; Cooldown = 0 }
)

foreach ($phase in $phases) {
    for ($i = 1; $i -le [int]$phase.Count; $i++) {
        $runIndex++
        Stop-RawrXDProcess
        if ([int]$phase.Cooldown -gt 0) {
            Start-Sleep -Seconds ([int]$phase.Cooldown)
        }

        $beforeLen = 0
        if (Test-Path $tracePath) {
            $beforeLen = (Get-Item -LiteralPath $tracePath).Length
        }

        $stdoutPath = Join-Path $OutDir ("{0}_{1:00}_{2}_stdout.txt" -f $Tag, $runIndex, $phase.Mode)
        $stderrPath = Join-Path $OutDir ("{0}_{1:00}_{2}_stderr.txt" -f $Tag, $runIndex, $phase.Mode)

        $launchArgs = @("--headless", "--no-server")
        $p = Start-Process -FilePath $ExePath -ArgumentList $launchArgs -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

        $timedOut = -not $p.WaitForExit($TimeoutSeconds * 1000)
        if ($timedOut) {
            try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
        }

        Start-Sleep -Milliseconds 150
        $tail = Read-AppendedTrace -TracePath $tracePath -StartLength $beforeLen
        $line = Get-LastCamelliaLine -Text $tail

        $actualE0 = Get-HexField -Line $line -Name "actual_e0"
        $expectedE0 = Get-HexField -Line $line -Name "expected_e0"
        $hmacRc = Get-TokenField -Line $line -Name "hmac_snapshot_rc" -DefaultValue "-1"

        if (-not $frequency.ContainsKey($actualE0)) {
            $frequency[$actualE0] = 0
        }
        $frequency[$actualE0] = [int]$frequency[$actualE0] + 1

        $result = if ($timedOut) { "TIMEOUT" } elseif ($actualE0 -eq "0x00000000") { "PASS" } else { "FAIL" }
        if ($result -ne "PASS") {
            $failures++
            if ($phase.Mode -eq "warm") {
                $warmFailures++
            }
        }

        $bucket = Get-FailureBucket -ActualHex $actualE0 -Mode $phase.Mode -Frequency $frequency -Failures $failures -WarmFailures $warmFailures -HmacRc $hmacRc
        $delta = ("{0}->{1}" -f $expectedE0, $actualE0)

        $obj = [pscustomobject]@{
            Run = $runIndex
            Mode = $phase.Mode
            Result = $result
            E0_Delta = $delta
            Failure_Bucket = $bucket
            ExitCode = if ($timedOut) { -999999 } else { $p.ExitCode }
            TraceLine = $line
            StdoutPath = $stdoutPath
            StderrPath = $stderrPath
        }
        $results.Add($obj) | Out-Null

        Write-Host ("RUN {0:00} | {1} | {2} | {3} | {4}" -f $runIndex, $phase.Mode.ToUpperInvariant(), $result, $delta, $bucket)
    }
}

$results | Export-Csv -Path $outCsv -NoTypeInformation -Encoding UTF8
$summary = [pscustomobject]@{
    generatedAt = (Get-Date).ToString("o")
    executable = $ExePath
    coldRuns = $ColdRuns
    warmRuns = $WarmRuns
    timeoutSeconds = $TimeoutSeconds
    tracePath = $tracePath
    results = $results
}
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $outJson -Encoding UTF8

Write-Host ""
Write-Host "Run # | Result | E0_Delta | Failure_Bucket"
foreach ($r in $results) {
    Write-Host (("{0,5} | {1,-7} | {2,-24} | {3}") -f $r.Run, $r.Result, $r.E0_Delta, $r.Failure_Bucket)
}
Write-Host ""
Write-Host ("CAMELLIA_MATRIX_COMPLETE json={0} csv={1}" -f $outJson, $outCsv)
