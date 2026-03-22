#Requires -Version 5.1
<#
.SYNOPSIS
  Run RawrXD-TpsSmoke against every .gguf under a folder (default F:\OllamaModels) and write a CSV report.

.DESCRIPTION
  Sets RAWRXD_TPS_MACHINE_JSON=1 so each run prints one RAWRXD_TPS_JSON={...} line for reliable parsing.
  Large checkpoints can take a very long time per token; use -MaxTokens 4..16 for sweeps and -MaxFileSizeGB to skip huge files.
  If CSV shows exit like -1073741819, that is 0xC0000005 (access violation): RawrXD-TpsSmoke crashed — not a missing JSON feature.

.PARAMETER ModelsRoot
  Root directory to scan (default: F:\OllamaModels).

.PARAMETER NoRecurse
  Only scan the root folder, not subfolders.

.PARAMETER MaxTokens
  Passed to TpsSmoke (default: 8). Each step is a full multi-layer forward.

.PARAMETER MaxFileSizeGB
  Skip files larger than this (0 = no limit). Example: 8 to skip 25GB+ monsters during quick sweeps.

.PARAMETER BuildDir
  CMake build dir (default: build_smoke_auto under repo root).

.PARAMETER Config
  MSBuild config (default: Release).

.PARAMETER OutputCsv
  CSV path (default: logs\tps_benchmark_<timestamp>.csv under repo).

.PARAMETER DecodeProgress
  Sets RAWRXD_GGUF_DECODE_PROGRESS=1 for per-token logs (very verbose).

.PARAMETER SkipBuild
  Do not invoke cmake build; use existing exe.

.PARAMETER RequireBeat
  Sets RAWRXD_TPS_REQUIRE_BEAT=1 (exit 4 fails a model run — usually avoid for batch sweeps).

.EXAMPLE
  .\scripts\Benchmark-TpsSmoke-Models.ps1 -ModelsRoot "F:\OllamaModels" -MaxTokens 8 -MaxFileSizeGB 12

.EXAMPLE
  .\scripts\Benchmark-TpsSmoke-Models.ps1 -NoRecurse -DecodeProgress
#>
param(
    [string]$ModelsRoot = "F:\OllamaModels",
    [switch]$NoRecurse,
    [int]$MaxTokens = 8,
    [double]$MaxFileSizeGB = 0,
    [string]$BuildDir = "",
    [string]$Config = "Release",
    [string]$OutputCsv = "",
    [switch]$DecodeProgress,
    [switch]$SkipBuild,
    [switch]$RequireBeat
)

$ErrorActionPreference = "Stop"

function Get-TpsSmokeExitDetail {
    <#
    .NOTES
      Windows often reports crashes as negative ExitCode (e.g. -1073741819 == 0xC0000005 STATUS_ACCESS_VIOLATION).
      That is not "missing RAWRXD_TPS_MACHINE_JSON" — the process died before emitting the JSON line.
    #>
    param([int]$ExitCode)

    switch ($ExitCode) {
        0 { return 'ok' }
        1 { return 'path_invalid_or_loadModel_failed' }
        2 { return 'runInference_failed' }
        3 { return 'bad_alloc_or_exception' }
        4 { return 'below_ref_with_REQUIRE_BEAT' }
    }

    $u = [uint32](([int64]$ExitCode) -band 0xFFFFFFFFL)
    $hex = '0x{0:X8}' -f $u
    $nt = @{
        [uint32]0xC0000005 = 'STATUS_ACCESS_VIOLATION (null deref / bad pointer — GGUFRunner/TpsSmoke crashed)'
        [uint32]0xC00000FD = 'STATUS_STACK_OVERFLOW'
        [uint32]0xC0000409 = 'STATUS_STACK_BUFFER_OVERRUN'
        [uint32]0xC000001D = 'STATUS_ILLEGAL_INSTRUCTION'
        [uint32]0xC0000094 = 'STATUS_INTEGER_DIVIDE_BY_ZERO'
        [uint32]0xC000013A = 'STATUS_CONTROL_C_EXIT'
        [uint32]0xC0000142 = 'STATUS_DLL_INIT_FAILED'
    }
    $name = $nt[$u]
    if (-not $name) {
        $name = 'abnormal_termination (NTSTATUS)'
    }
    return "exit=$ExitCode unsigned=$hex $name — no JSON because process crashed/exited abnormally (rebuilding for MACHINE_JSON alone will not fix this)"
}

function Test-TpsSmokeLikelyCrashed {
    <#
    .NOTES
      Process exit codes may surface as [int] -1073741819, as a string from CSV re-import, or (rarely) as a large
      positive UInt32 bitmask. Treat NTSTATUS high-bit failures as crashes so Skipped=crashed (not no_json_line).
    #>
    param($ExitCode)
    $v = 0L
    if ($null -eq $ExitCode) { return $false }
    if ($ExitCode -is [string]) {
        $t = $ExitCode.Trim()
        if (-not [int64]::TryParse($t, [ref]$v)) { return $false }
    }
    else {
        $v = [int64]$ExitCode
    }
    if ($v -lt 0) { return $true }
    $u = [uint32]($v -band 0xFFFFFFFFL)
    return ($u -ge 0xC0000000)
}

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $repoRoot "CMakeLists.txt"))) {
    $walk = $PSScriptRoot
    while ($walk -and -not (Test-Path (Join-Path $walk "CMakeLists.txt"))) {
        $walk = Split-Path -Parent $walk
    }
    $repoRoot = $walk
}
if (-not $repoRoot -or -not (Test-Path (Join-Path $repoRoot "CMakeLists.txt"))) {
    throw "Could not find repository root (CMakeLists.txt)."
}

if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build_smoke_auto"
}

$candidates = @(
    (Join-Path $BuildDir "bin\$Config\RawrXD-TpsSmoke.exe"),
    (Join-Path $BuildDir "bin\RawrXD-TpsSmoke.exe"),
    (Join-Path $BuildDir "RawrXD-TpsSmoke.exe")
)

if (-not $SkipBuild) {
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
        cmake -S $repoRoot -B $BuildDir
    }
    cmake --build $BuildDir --config $Config --target RawrXD-TpsSmoke
}

$exe = $null
foreach ($c in $candidates) {
    if (Test-Path $c) { $exe = $c; break }
}
if (-not $exe) {
    throw "RawrXD-TpsSmoke not found under $BuildDir (build: cmake --build '$BuildDir' --config $Config --target RawrXD-TpsSmoke)."
}

if (-not (Test-Path -LiteralPath $ModelsRoot)) {
    throw "ModelsRoot does not exist: $ModelsRoot"
}

$logsDir = Join-Path $repoRoot "logs"
if (-not (Test-Path $logsDir)) {
    New-Item -ItemType Directory -Path $logsDir | Out-Null
}
if (-not $OutputCsv) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputCsv = Join-Path $logsDir "tps_benchmark_$stamp.csv"
}

$maxBytes = 0L
if ($MaxFileSizeGB -gt 0) {
    $maxBytes = [long]([double]$MaxFileSizeGB * 1024.0 * 1024.0 * 1024.0)
}

$gciArgs = @{
    Path        = $ModelsRoot
    Filter      = "*.gguf"
    File        = $true
    ErrorAction = "SilentlyContinue"
}
if (-not $NoRecurse) {
    $gciArgs["Recurse"] = $true
}

$models = @(Get-ChildItem @gciArgs | Sort-Object FullName)
if ($models.Count -eq 0) {
    Write-Warning "No .gguf files found under $ModelsRoot$(if ($NoRecurse) { '' } else { ' (recursive)' })."
    exit 0
}

# Automation: one JSON line per run on stderr
$env:RAWRXD_TPS_MACHINE_JSON = "1"
$env:RAWRXD_TPS_REF = if ($env:RAWRXD_TPS_REF) { $env:RAWRXD_TPS_REF } else { "239" }
if ($RequireBeat) {
    $env:RAWRXD_TPS_REQUIRE_BEAT = "1"
} else {
    Remove-Item Env:\RAWRXD_TPS_REQUIRE_BEAT -ErrorAction SilentlyContinue
}
if ($DecodeProgress) {
    $env:RAWRXD_GGUF_DECODE_PROGRESS = "1"
} else {
    Remove-Item Env:\RAWRXD_GGUF_DECODE_PROGRESS -ErrorAction SilentlyContinue
}

$rows = New-Object System.Collections.Generic.List[object]
$i = 0
foreach ($m in $models) {
    $i++
    $len = $m.Length
    if ($maxBytes -gt 0 -and $len -gt $maxBytes) {
        $rows.Add([pscustomobject]@{
                Index       = $i
                File        = $m.FullName
                SizeBytes   = $len
                Skipped     = "size_over_cap"
                Exit        = ""
                Phase       = ""
                Steps       = ""
                WallS       = ""
                Tps         = ""
                Arch        = ""
                Layers      = ""
                Embed       = ""
                Vocab       = ""
                Detail      = "MaxFileSizeGB=$MaxFileSizeGB"
            }) | Out-Null
        Write-Host "[$i/$($models.Count)] SKIP (too large): $($m.Name)"
        continue
    }

    Write-Host "[$i/$($models.Count)] RUN: $($m.FullName)"
    $out = ""
    $err = ""
    try {
        $pinfo = New-Object System.Diagnostics.ProcessStartInfo
        $pinfo.FileName = $exe
        $pinfo.Arguments = "`"$($m.FullName)`" $MaxTokens"
        $pinfo.WorkingDirectory = $repoRoot
        $pinfo.UseShellExecute = $false
        $pinfo.RedirectStandardOutput = $true
        $pinfo.RedirectStandardError = $true
        $pinfo.CreateNoWindow = $true
        $proc = New-Object System.Diagnostics.Process
        $proc.StartInfo = $pinfo
        [void]$proc.Start()
        $out = $proc.StandardOutput.ReadToEnd()
        $err = $proc.StandardError.ReadToEnd()
        $proc.WaitForExit()
        $code = $proc.ExitCode
    } catch {
        $rows.Add([pscustomobject]@{
                Index       = $i
                File        = $m.FullName
                SizeBytes   = $len
                Skipped     = "launch_error"
                Exit        = ""
                Phase       = ""
                Steps       = ""
                WallS       = ""
                Tps         = ""
                Arch        = ""
                Layers      = ""
                Embed       = ""
                Vocab       = ""
                Detail      = $_.Exception.Message
            }) | Out-Null
        Write-Warning "Launch failed: $($_.Exception.Message)"
        continue
    }

    $jsonLine = ($err + "`n" + $out) -split "`n" | Where-Object { $_ -match '^\s*RAWRXD_TPS_JSON=' } | Select-Object -Last 1
    $parsed = $null
    if ($jsonLine) {
        $payload = ($jsonLine -replace '^\s*RAWRXD_TPS_JSON=', '').Trim()
        try {
            $parsed = $payload | ConvertFrom-Json
        } catch {
            $parsed = $null
        }
    }

    if ($parsed) {
        $rows.Add([pscustomobject]@{
                Index       = $i
                File        = $m.FullName
                SizeBytes   = $len
                Skipped     = ""
                Exit        = $parsed.exit
                Phase       = $parsed.phase
                Steps       = $parsed.steps
                WallS       = $parsed.wall_s
                Tps         = $parsed.tps
                Arch        = $parsed.arch
                Layers      = $parsed.layers
                Embed       = $parsed.embed
                Vocab       = $parsed.vocab
                Detail      = $parsed.detail
            }) | Out-Null
    } else {
        $crashed = Test-TpsSmokeLikelyCrashed -ExitCode $code
        $skipReason = if ($crashed) { "crashed" } else { "no_json_line" }
        $detail = Get-TpsSmokeExitDetail -ExitCode $code
        if (-not $crashed) {
            $detail += ' | Rebuild TpsSmoke: RAWRXD_TPS_MACHINE_JSON + Win32 crash filter emit RAWRXD_TPS_JSON even on 0xC0000005; check stderr.'
        }
        $rows.Add([pscustomobject]@{
                Index       = $i
                File        = $m.FullName
                SizeBytes   = $len
                Skipped     = $skipReason
                Exit        = $code
                Phase       = ""
                Steps       = ""
                WallS       = ""
                Tps         = ""
                Arch        = ""
                Layers      = ""
                Embed       = ""
                Vocab       = ""
                Detail      = $detail
            }) | Out-Null
    }
}

$rows | Export-Csv -LiteralPath $OutputCsv -NoTypeInformation -Encoding UTF8
Write-Host ""
Write-Host "Wrote $($rows.Count) row(s) -> $OutputCsv"
Write-Host "Done."
