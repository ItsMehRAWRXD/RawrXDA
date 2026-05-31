#Requires -Version 5.1
<#!
.SYNOPSIS
  Benchmarks the maximum real GGUF streamability of the current RawrXD loader and compares it to one targeted addition.

.DESCRIPTION
  Builds and runs RawrXD-StreamabilityBenchmark against every .gguf under the supplied roots.
  The native benchmark uses src/streaming_gguf_loader.cpp directly, probes real zones, and emits machine JSON.
  Optional TPS correlation reuses RawrXD-TpsSmoke so the report can show both streamability ceiling and decode viability.

  "Current" means the shipping whole-zone loader.
  "One addition" means replacing whole-zone copies with a mapped sliding window sized by -MappedWindowMB.
#>
param(
    [string[]]$ModelRoots = @("D:\"),
    [switch]$NoRecurse,
    [double]$MaxFileSizeGB = 0,
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [switch]$SkipBuild,
    [switch]$RunTps,
    [double]$TpsMaxFileSizeGB = 4,
    [int]$MaxTokens = 8,
    [int]$MappedWindowMB = 64,
    [int[]]$OneAdditionWindowSweepMB = @(),
    [double]$MinFileSizeMB = 1,
    [switch]$FastProbe,
    [string]$OutputDir = "",
    [int]$PerModelTimeoutSec = 300
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    $root = Split-Path -Parent $PSScriptRoot
    if (Test-Path (Join-Path $root "CMakeLists.txt")) { return $root }
    $walk = $PSScriptRoot
    while ($walk -and -not (Test-Path (Join-Path $walk "CMakeLists.txt"))) {
        $walk = Split-Path -Parent $walk
    }
    if (-not $walk) { throw "Could not resolve repository root." }
    return $walk
}

function Resolve-FirstExistingPath {
    param([string[]]$Candidates)
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) { return $candidate }
    }
    return $null
}

function Resolve-BuildDirectory {
    param([string]$RepoRoot, [string]$Preferred)
    if ($Preferred -and (Test-Path -LiteralPath (Join-Path $Preferred "CMakeCache.txt"))) {
        return (Resolve-Path -LiteralPath $Preferred).Path
    }

    $resolver = Join-Path $RepoRoot "scripts\Resolve-BuildDir.ps1"
    if (Test-Path -LiteralPath $resolver) {
        try {
            $resolved = & pwsh -NoProfile -File $resolver -RepoRoot $RepoRoot -Prefer $Preferred
            if ($LASTEXITCODE -eq 0 -and $resolved) { return ($resolved | Select-Object -Last 1) }
        }
        catch {
        }
    }

    foreach ($candidate in @(
            (Join-Path $RepoRoot "build-win32"),
            (Join-Path $RepoRoot "build-ninja"),
            (Join-Path $RepoRoot "build-ninja-ctx2"),
            (Join-Path $RepoRoot "build_smoke_auto"),
            (Join-Path $RepoRoot "build"))) {
        if (Test-Path -LiteralPath (Join-Path $candidate "CMakeCache.txt")) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return (Join-Path $RepoRoot "build-win32")
}

function Convert-ToArgumentString {
    param([string[]]$Arguments)
    if (-not $Arguments -or $Arguments.Count -eq 0) { return "" }

    $encoded = foreach ($arg in $Arguments) {
        if ($null -eq $arg) { '""'; continue }
        $text = [string]$arg
        if ($text -match '[\s"]') {
            '"' + ($text -replace '"', '\\"') + '"'
        }
        else {
            $text
        }
    }
    return ($encoded -join ' ')
}

function Get-ProcessResult {
    param(
        [Parameter(Mandatory = $true)][string]$FileName,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [hashtable]$Environment = @{},
        [int]$TimeoutSec = 0
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FileName
    $psi.Arguments = Convert-ToArgumentString -Arguments $Arguments
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    foreach ($key in $Environment.Keys) { $psi.Environment[$key] = [string]$Environment[$key] }

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    [void]$proc.Start()

    if ($TimeoutSec -gt 0) {
        if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
            try { $proc.Kill() } catch { }
            try { $proc.WaitForExit() } catch { }
            $stdout = ""
            $stderr = "process_timeout_after_${TimeoutSec}s"
            return [pscustomobject]@{ ExitCode = 124; StdOut = $stdout; StdErr = $stderr; TimedOut = $true }
        }
    }
    else {
        $proc.WaitForExit()
    }

    $stdout = $proc.StandardOutput.ReadToEnd()
    $stderr = $proc.StandardError.ReadToEnd()

    return [pscustomobject]@{ ExitCode = $proc.ExitCode; StdOut = $stdout; StdErr = $stderr; TimedOut = $false }
}

function Convert-BytesToGiB {
    param([UInt64]$Bytes)
    return [Math]::Round(($Bytes / 1GB), 3)
}

function Get-JsonLinePayload {
    param([string]$CombinedText, [string]$Prefix)
    $line = $CombinedText -split "`r?`n" | Where-Object { $_ -match ("^\s*" + [regex]::Escape($Prefix)) } | Select-Object -Last 1
    if (-not $line) { return $null }
    $payload = ($line -replace ("^\s*" + [regex]::Escape($Prefix)), "").Trim()
    if (-not $payload) { return $null }
    try { return $payload | ConvertFrom-Json } catch { return $null }
}

function Get-DefaultWindowSweepMB {
    param([int]$MappedWindowMB)
    $seed = @($MappedWindowMB, 16, 32, 64, 96, 128, 192, 256, 384, 512, 768, 1024)
    return @($seed | Where-Object { $_ -gt 0 } | Sort-Object -Unique)
}

function Get-BestOneAdditionEstimate {
    param(
        [pscustomobject]$StreamJson,
        [int[]]$WindowSweepMB,
        [int]$MappedWindowMB
    )

    if (-not $StreamJson) {
        return [pscustomobject]@{
            BestWindowMB       = $MappedWindowMB
            BestMaxBytes       = 0
            BestMaxGiB         = 0
            BestDeltaBytes     = 0
            BestDeltaGiB       = 0
            SweepEvaluatedMB   = @()
            EffectiveWorkingSet = 0
        }
    }

    $fileBytes = [UInt64]$StreamJson.file_bytes
    $workingBudgetBytes = [UInt64]$StreamJson.working_budget_bytes
    $largestZoneBytes = [UInt64]$StreamJson.largest_zone_bytes
    $baselineOverheadBytes = [UInt64]$StreamJson.baseline_overhead_bytes
    $observedPeakBytes = [UInt64]$StreamJson.observed_peak_bytes
    $estimatedCurrentMaxBytes = [UInt64]$StreamJson.estimated_current_max_bytes

    $observedDeltaBytes = if ($observedPeakBytes -gt $baselineOverheadBytes) { [UInt64]($observedPeakBytes - $baselineOverheadBytes) } else { [UInt64]0 }
    $effectiveCurrentWorkingSet = [UInt64][Math]::Max(1.0, [Math]::Max([double]$largestZoneBytes, [double]$observedDeltaBytes))

    $sweep = @(
        if ($WindowSweepMB -and $WindowSweepMB.Count -gt 0) {
            @($WindowSweepMB | Where-Object { $_ -gt 0 } | Sort-Object -Unique)
        }
        else {
            @(Get-DefaultWindowSweepMB -MappedWindowMB $MappedWindowMB)
        }
    )
    if (-not $sweep -or $sweep.Length -eq 0) { $sweep = @($MappedWindowMB) }

    $bestWindowMB = [int]$sweep[0]
    $bestMaxBytes = [UInt64]0

    foreach ($windowMB in $sweep) {
        $windowBytes = [UInt64]([uint64]$windowMB * 1MB)
        $effectiveOneAdditionWorkingSet = [UInt64][Math]::Min([double]$effectiveCurrentWorkingSet, [double][Math]::Max(1.0, [double]$windowBytes))

        $estimateBytes = [UInt64]0
        if ($workingBudgetBytes -gt 0 -and $fileBytes -gt 0 -and $effectiveOneAdditionWorkingSet -gt 0) {
            $estimateBytes = [UInt64](($fileBytes * [double]$workingBudgetBytes) / [double]$effectiveOneAdditionWorkingSet)
        }

        if ($estimateBytes -gt $bestMaxBytes) {
            $bestMaxBytes = $estimateBytes
            $bestWindowMB = [int]$windowMB
        }
    }

    $bestDeltaBytes = if ($bestMaxBytes -gt $estimatedCurrentMaxBytes) { [UInt64]($bestMaxBytes - $estimatedCurrentMaxBytes) } else { [UInt64]0 }

    return [pscustomobject]@{
        BestWindowMB       = $bestWindowMB
        BestMaxBytes       = $bestMaxBytes
        BestMaxGiB         = Convert-BytesToGiB -Bytes $bestMaxBytes
        BestDeltaBytes     = $bestDeltaBytes
        BestDeltaGiB       = Convert-BytesToGiB -Bytes $bestDeltaBytes
        SweepEvaluatedMB   = $sweep
        EffectiveWorkingSet = $effectiveCurrentWorkingSet
    }
}

$repoRoot = Resolve-RepoRoot
$BuildDir = Resolve-BuildDirectory -RepoRoot $repoRoot -Preferred $BuildDir

if (-not $OutputDir) { $OutputDir = Join-Path $repoRoot "reports\14day" }
if (-not (Test-Path -LiteralPath $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$streamExeCandidates = @(
    (Join-Path $BuildDir "bin\$Config\RawrXD-StreamabilityBenchmark.exe"),
    (Join-Path $BuildDir "bin\RawrXD-StreamabilityBenchmark.exe"),
    (Join-Path $BuildDir "RawrXD-StreamabilityBenchmark.exe")
)
$tpsExeCandidates = @(
    (Join-Path $BuildDir "bin\$Config\RawrXD-TpsSmoke.exe"),
    (Join-Path $BuildDir "bin\RawrXD-TpsSmoke.exe"),
    (Join-Path $BuildDir "RawrXD-TpsSmoke.exe")
)

if (-not $SkipBuild) {
    if (-not (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
        cmake -S $repoRoot -B $BuildDir
        if ($LASTEXITCODE -ne 0) { throw "cmake configure failed for $BuildDir" }
    }
    cmake --build $BuildDir --config $Config --target RawrXD-StreamabilityBenchmark
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed for RawrXD-StreamabilityBenchmark" }
    if ($RunTps) {
        cmake --build $BuildDir --config $Config --target RawrXD-TpsSmoke
        if ($LASTEXITCODE -ne 0) { throw "cmake build failed for RawrXD-TpsSmoke" }
    }
}

$streamExe = Resolve-FirstExistingPath -Candidates $streamExeCandidates
if (-not $streamExe) { throw "RawrXD-StreamabilityBenchmark.exe not found under $BuildDir" }
$tpsExe = Resolve-FirstExistingPath -Candidates $tpsExeCandidates

$maxBytes = 0L
if ($MaxFileSizeGB -gt 0) { $maxBytes = [long]($MaxFileSizeGB * 1GB) }
$minBytes = [long]([Math]::Max(0, $MinFileSizeMB * 1MB))
$tpsMaxBytes = [long]($TpsMaxFileSizeGB * 1GB)

$models = New-Object System.Collections.Generic.List[System.IO.FileInfo]
foreach ($root in $ModelRoots) {
    if (-not (Test-Path -LiteralPath $root)) {
        Write-Warning "Skipping missing model root: $root"
        continue
    }
    $gciArgs = @{ LiteralPath = $root; Filter = "*.gguf"; File = $true; ErrorAction = "SilentlyContinue" }
    if (-not $NoRecurse) { $gciArgs["Recurse"] = $true }
    foreach ($file in (Get-ChildItem @gciArgs)) {
        if ($file.Length -lt $minBytes) { continue }
        if ($maxBytes -gt 0 -and $file.Length -gt $maxBytes) { continue }
        $models.Add($file) | Out-Null
    }
}

$models = @($models | Sort-Object Length, FullName)
if ($models.Count -eq 0) { throw "No .gguf files found under the provided roots." }

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$jsonPath = Join-Path $OutputDir "max_streamable_benchmark_$stamp.json"
$csvPath = Join-Path $OutputDir "max_streamable_benchmark_$stamp.csv"
$mdPath = Join-Path $OutputDir "max_streamable_benchmark_$stamp.md"

$effectiveWindowSweepMB = if ($OneAdditionWindowSweepMB -and $OneAdditionWindowSweepMB.Count -gt 0) {
    @($OneAdditionWindowSweepMB | Where-Object { $_ -gt 0 } | Sort-Object -Unique)
}
else {
    Get-DefaultWindowSweepMB -MappedWindowMB $MappedWindowMB
}

$rows = New-Object System.Collections.Generic.List[object]
for ($i = 0; $i -lt $models.Count; $i++) {
    $model = $models[$i]
    Write-Host (("[{0}/{1}] STREAM: {2}" -f ($i + 1), $models.Count, $model.FullName)) -ForegroundColor Cyan

    $streamArgs = @($model.FullName, "--window-mb", "$MappedWindowMB")
    if ($FastProbe) { $streamArgs += "--first-zone-only" }

    $streamResult = Get-ProcessResult -FileName $streamExe -Arguments $streamArgs -WorkingDirectory $repoRoot -Environment @{ "RAWRXD_MAX_STREAM_MACHINE_JSON" = "1" } -TimeoutSec $PerModelTimeoutSec
    $streamJson = Get-JsonLinePayload -CombinedText ($streamResult.StdErr + "`n" + $streamResult.StdOut) -Prefix "RAWRXD_MAX_STREAM_JSON="
    $bestOneAddition = Get-BestOneAdditionEstimate -StreamJson $streamJson -WindowSweepMB $effectiveWindowSweepMB -MappedWindowMB $MappedWindowMB

    $tpsJson = $null
    $tpsResult = $null
    if ($RunTps -and $tpsExe -and $streamJson -and $streamJson.streamable -and $model.Length -le $tpsMaxBytes) {
        Write-Host (("[{0}/{1}] TPS:    {2}" -f ($i + 1), $models.Count, $model.Name)) -ForegroundColor DarkCyan
        $tpsResult = Get-ProcessResult -FileName $tpsExe -Arguments @($model.FullName, "$MaxTokens") -WorkingDirectory $repoRoot -Environment @{ "RAWRXD_TPS_MACHINE_JSON" = "1"; "RAWRXD_TPS_REF" = "239" } -TimeoutSec $PerModelTimeoutSec
        $tpsJson = Get-JsonLinePayload -CombinedText ($tpsResult.StdErr + "`n" + $tpsResult.StdOut) -Prefix "RAWRXD_TPS_JSON="
    }

    $rows.Add([pscustomobject]@{
            File                         = $model.FullName
            SizeBytes                    = [UInt64]$model.Length
            SizeGiB                      = Convert-BytesToGiB -Bytes ([UInt64]$model.Length)
            StreamExit                   = if ($streamJson) { [int]$streamJson.exit } else { [int]$streamResult.ExitCode }
            StreamPhase                  = if ($streamJson) { [string]$streamJson.phase } elseif ($streamResult.TimedOut) { "timeout" } else { "no_json" }
            Streamable                   = if ($streamJson) { [bool]$streamJson.streamable } else { $false }
            Arch                         = if ($streamJson) { [string]$streamJson.arch } else { "" }
            Layers                       = if ($streamJson) { [int]$streamJson.layers } else { 0 }
            ContextLength                = if ($streamJson) { [int]$streamJson.context_length } else { 0 }
            Embed                        = if ($streamJson) { [int]$streamJson.embed } else { 0 }
            Vocab                        = if ($streamJson) { [UInt64]$streamJson.vocab } else { 0 }
            ZoneCount                    = if ($streamJson) { [int]$streamJson.zone_count } else { 0 }
            LargestZoneBytes             = if ($streamJson) { [UInt64]$streamJson.largest_zone_bytes } else { 0 }
            LargestZoneGiB               = if ($streamJson) { Convert-BytesToGiB -Bytes ([UInt64]$streamJson.largest_zone_bytes) } else { 0 }
            LargestZoneName              = if ($streamJson) { [string]$streamJson.largest_zone_name } else { "" }
            LargestTensorBytes           = if ($streamJson) { [UInt64]$streamJson.largest_tensor_bytes } else { 0 }
            LargestTensorGiB             = if ($streamJson) { Convert-BytesToGiB -Bytes ([UInt64]$streamJson.largest_tensor_bytes) } else { 0 }
            ObservedPeakBytes            = if ($streamJson) { [UInt64]$streamJson.observed_peak_bytes } else { 0 }
            ObservedPeakGiB              = if ($streamJson) { Convert-BytesToGiB -Bytes ([UInt64]$streamJson.observed_peak_bytes) } else { 0 }
            WorkingBudgetBytes           = if ($streamJson) { [UInt64]$streamJson.working_budget_bytes } else { 0 }
            WorkingBudgetGiB             = if ($streamJson) { Convert-BytesToGiB -Bytes ([UInt64]$streamJson.working_budget_bytes) } else { 0 }
            EstimatedCurrentMaxBytes     = if ($streamJson) { [UInt64]$streamJson.estimated_current_max_bytes } else { 0 }
            EstimatedCurrentMaxGiB       = if ($streamJson) { Convert-BytesToGiB -Bytes ([UInt64]$streamJson.estimated_current_max_bytes) } else { 0 }
            OneAddition                  = if ($streamJson) { [string]$streamJson.one_addition } else { "mapped_window_view" }
            OneAdditionWindowBytes       = if ($streamJson) { [UInt64]$streamJson.one_addition_window_bytes } else { 0 }
            OneAdditionWindowGiB         = if ($streamJson) { Convert-BytesToGiB -Bytes ([UInt64]$streamJson.one_addition_window_bytes) } else { 0 }
            EstimatedOneAdditionMaxBytes = if ($streamJson) { [UInt64]$streamJson.estimated_one_addition_max_bytes } else { 0 }
            EstimatedOneAdditionMaxGiB   = if ($streamJson) { Convert-BytesToGiB -Bytes ([UInt64]$streamJson.estimated_one_addition_max_bytes) } else { 0 }
            EstimatedDeltaBytes          = if ($streamJson) { [UInt64]$streamJson.estimated_delta_bytes } else { 0 }
            EstimatedDeltaGiB            = if ($streamJson) { Convert-BytesToGiB -Bytes ([UInt64]$streamJson.estimated_delta_bytes) } else { 0 }
            BestOneAdditionWindowMB      = [int]$bestOneAddition.BestWindowMB
            BestOneAdditionMaxBytes      = [UInt64]$bestOneAddition.BestMaxBytes
            BestOneAdditionMaxGiB        = [double]$bestOneAddition.BestMaxGiB
            BestOneAdditionDeltaBytes    = [UInt64]$bestOneAddition.BestDeltaBytes
            BestOneAdditionDeltaGiB      = [double]$bestOneAddition.BestDeltaGiB
            OneAdditionSweepMB           = (($bestOneAddition.SweepEvaluatedMB | ForEach-Object { [string]$_ }) -join ",")
            OpenMs                       = if ($streamJson) { [double]$streamJson.open_ms } else { 0.0 }
            ProbeMs                      = if ($streamJson) { [double]$streamJson.probe_ms } else { 0.0 }
            StreamDetail                 = if ($streamJson) { [string]$streamJson.detail } else { (($streamResult.StdErr + " " + $streamResult.StdOut).Trim()) }
            TpsExit                      = if ($tpsJson) { [int]$tpsJson.exit } else { "" }
            TpsPhase                     = if ($tpsJson) { [string]$tpsJson.phase } elseif ($tpsResult -and $tpsResult.TimedOut) { "timeout" } else { "" }
            Tps                          = if ($tpsJson) { [double]$tpsJson.tps } else { "" }
            TpsWallS                     = if ($tpsJson) { [double]$tpsJson.wall_s } else { "" }
            TpsSteps                     = if ($tpsJson) { [int]$tpsJson.steps } else { "" }
            TpsDetail                    = if ($tpsJson) { [string]$tpsJson.detail } else { "" }
        }) | Out-Null
}

$rowArray = @($rows.ToArray())
$successful = @($rowArray | Where-Object { $_.Streamable })
$minMeaningfulZoneBytes = 1MB
$meaningful = @(
    $successful | Where-Object {
        [UInt64]$_.LargestZoneBytes -ge [UInt64]$minMeaningfulZoneBytes -and
        [UInt64]$_.EstimatedCurrentMaxBytes -gt 0
    }
)
$summaryPool = @(
    if ($meaningful.Count -gt 0) { $meaningful } else { $successful }
)
$largestActual = $successful | Sort-Object SizeBytes -Descending | Select-Object -First 1
$largestEstimatedCurrent = $summaryPool | Sort-Object EstimatedCurrentMaxBytes -Descending | Select-Object -First 1
$largestEstimatedOneAddition = $summaryPool | Sort-Object EstimatedOneAdditionMaxBytes -Descending | Select-Object -First 1
$largestBestOneAddition = $summaryPool | Sort-Object BestOneAdditionMaxBytes -Descending | Select-Object -First 1

$summary = [ordered]@{
    timestamp_utc = [DateTime]::UtcNow.ToString("o")
    repo_root = $repoRoot
    build_dir = $BuildDir
    model_roots = $ModelRoots
    scanned_models = $rowArray.Count
    successful_models = $successful.Count
    meaningful_models = $summaryPool.Count
    meaningful_zone_floor_bytes = [UInt64]$minMeaningfulZoneBytes
    mapped_window_mb = $MappedWindowMB
    one_addition_window_sweep_mb = $effectiveWindowSweepMB
    actual_max_streamed = if ($largestActual) { [ordered]@{ file = $largestActual.File; size_bytes = [UInt64]$largestActual.SizeBytes; size_gib = [double]$largestActual.SizeGiB; largest_zone_bytes = [UInt64]$largestActual.LargestZoneBytes; largest_zone_gib = [double]$largestActual.LargestZoneGiB; observed_peak_bytes = [UInt64]$largestActual.ObservedPeakBytes; observed_peak_gib = [double]$largestActual.ObservedPeakGiB; context_length = [int]$largestActual.ContextLength; tps = $largestActual.Tps } } else { $null }
    estimated_current_ceiling = if ($largestEstimatedCurrent) { [ordered]@{ file = $largestEstimatedCurrent.File; estimated_max_bytes = [UInt64]$largestEstimatedCurrent.EstimatedCurrentMaxBytes; estimated_max_gib = [double]$largestEstimatedCurrent.EstimatedCurrentMaxGiB } } else { $null }
    estimated_one_addition_ceiling = if ($largestEstimatedOneAddition) { [ordered]@{ file = $largestEstimatedOneAddition.File; estimated_max_bytes = [UInt64]$largestEstimatedOneAddition.EstimatedOneAdditionMaxBytes; estimated_max_gib = [double]$largestEstimatedOneAddition.EstimatedOneAdditionMaxGiB; delta_bytes = [UInt64]$largestEstimatedOneAddition.EstimatedDeltaBytes; delta_gib = [double]$largestEstimatedOneAddition.EstimatedDeltaGiB; one_addition = $largestEstimatedOneAddition.OneAddition; mapped_window_gib = [double]$largestEstimatedOneAddition.OneAdditionWindowGiB } } else { $null }
    estimated_best_one_addition_ceiling = if ($largestBestOneAddition) { [ordered]@{ file = $largestBestOneAddition.File; estimated_max_bytes = [UInt64]$largestBestOneAddition.BestOneAdditionMaxBytes; estimated_max_gib = [double]$largestBestOneAddition.BestOneAdditionMaxGiB; delta_bytes = [UInt64]$largestBestOneAddition.BestOneAdditionDeltaBytes; delta_gib = [double]$largestBestOneAddition.BestOneAdditionDeltaGiB; one_addition = $largestBestOneAddition.OneAddition; best_window_mb = [int]$largestBestOneAddition.BestOneAdditionWindowMB } } else { $null }
}

[pscustomobject]@{ summary = $summary; rows = $rowArray } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding utf8
$rowArray | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Max Streamable Benchmark") | Out-Null
$md.Add("") | Out-Null
$md.Add(("- Generated UTC: {0}" -f $summary.timestamp_utc)) | Out-Null
$md.Add(("- Scanned models: {0}" -f $summary.scanned_models)) | Out-Null
$md.Add(("- Successful stream probes: {0}" -f $summary.successful_models)) | Out-Null
$md.Add(("- One addition: mapped window view ({0} MB default)" -f $MappedWindowMB)) | Out-Null
$md.Add(("- One-addition sweep windows MB: {0}" -f (($effectiveWindowSweepMB | ForEach-Object { [string]$_ }) -join ", "))) | Out-Null
$md.Add(("- Probe mode: {0}" -f ($(if ($FastProbe) { "first-zone-only (fast)" } else { "all-zones (full)" }))) ) | Out-Null
$md.Add("") | Out-Null
if ($largestActual) {
    $md.Add("## Largest Actual Streamed") | Out-Null
    $md.Add(("- File: {0}" -f $largestActual.File)) | Out-Null
    $md.Add(("- Size: {0} GiB" -f $largestActual.SizeGiB)) | Out-Null
    $md.Add(("- Largest zone: {0} GiB ({1})" -f $largestActual.LargestZoneGiB, $largestActual.LargestZoneName)) | Out-Null
    $md.Add(("- Observed peak resident: {0} GiB" -f $largestActual.ObservedPeakGiB)) | Out-Null
    if ($largestActual.Tps -ne "") { $md.Add(("- TPS: {0}" -f $largestActual.Tps)) | Out-Null }
    $md.Add("") | Out-Null
}
if ($largestEstimatedCurrent) {
    $md.Add("## Estimated Ceiling") | Out-Null
    $md.Add(("- Current whole-zone loader ceiling: {0} GiB" -f $largestEstimatedCurrent.EstimatedCurrentMaxGiB)) | Out-Null
    $md.Add(("- Based on model: {0}" -f $largestEstimatedCurrent.File)) | Out-Null
    $md.Add(("- One-addition mapped-window ceiling: {0} GiB" -f $largestEstimatedOneAddition.EstimatedOneAdditionMaxGiB)) | Out-Null
    $md.Add(("- Delta: {0} GiB" -f $largestEstimatedOneAddition.EstimatedDeltaGiB)) | Out-Null
    if ($largestBestOneAddition) {
        $md.Add(("- Best one-addition ceiling across sweep: {0} GiB" -f $largestBestOneAddition.BestOneAdditionMaxGiB)) | Out-Null
        $md.Add(("- Best one-addition window: {0} MB" -f $largestBestOneAddition.BestOneAdditionWindowMB)) | Out-Null
        $md.Add(("- Best one-addition delta: {0} GiB" -f $largestBestOneAddition.BestOneAdditionDeltaGiB)) | Out-Null
    }
    $md.Add("") | Out-Null
}
$md.Add("## Top Results") | Out-Null
$md.Add("") | Out-Null
$md.Add("| Model | Size GiB | Streamable | Largest Zone GiB | Peak GiB | Estimated Current GiB | Estimated One-Addition GiB | Best One-Addition GiB | Best Window MB | TPS |") | Out-Null
$md.Add("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|") | Out-Null
foreach ($row in ($rowArray | Sort-Object SizeBytes -Descending | Select-Object -First 12)) {
    $md.Add(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} | {9} |" -f [IO.Path]::GetFileName($row.File), $row.SizeGiB, $row.Streamable, $row.LargestZoneGiB, $row.ObservedPeakGiB, $row.EstimatedCurrentMaxGiB, $row.EstimatedOneAdditionMaxGiB, $row.BestOneAdditionMaxGiB, $row.BestOneAdditionWindowMB, $row.Tps)) | Out-Null
}
$md | Set-Content -LiteralPath $mdPath -Encoding utf8

Write-Host ""
Write-Host "=== Max Streamable Benchmark ===" -ForegroundColor Green
if ($largestActual) { Write-Host (("Largest actual streamed: {0} ({1} GiB)" -f $largestActual.File, $largestActual.SizeGiB)) -ForegroundColor Cyan }
if ($largestEstimatedCurrent) {
    Write-Host (("Estimated current ceiling: {0} GiB" -f $largestEstimatedCurrent.EstimatedCurrentMaxGiB)) -ForegroundColor Cyan
    Write-Host (("Estimated one-addition ceiling: {0} GiB" -f $largestEstimatedOneAddition.EstimatedOneAdditionMaxGiB)) -ForegroundColor Cyan
    if ($largestBestOneAddition) {
        Write-Host (("Estimated best one-addition ceiling across sweep: {0} GiB" -f $largestBestOneAddition.BestOneAdditionMaxGiB)) -ForegroundColor Cyan
        Write-Host (("Best one-addition window: {0} MB" -f $largestBestOneAddition.BestOneAdditionWindowMB)) -ForegroundColor Cyan
    }
}
Write-Host (("JSON: {0}" -f $jsonPath)) -ForegroundColor DarkGray
Write-Host (("CSV:  {0}" -f $csvPath)) -ForegroundColor DarkGray
Write-Host (("MD:   {0}" -f $mdPath)) -ForegroundColor DarkGray
