#Requires -Version 7.0
<#
.SYNOPSIS
  Export Win32IDE linker telemetry and optionally diff against reports/win32ide/link-baseline.json.

.PARAMETER BuildDir
  CMake build directory (default: build-win32).

.PARAMETER ExePath
  Override path to RawrXD-Win32IDE.exe.

.PARAMETER UpdateBaseline
  Rewrite reports/win32ide/link-baseline.json and companion text artifacts.

.PARAMETER FailOnRegression
  Exit non-zero when binary size or import set drifts beyond thresholds vs baseline.
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "build-win32",
    [string]$ExePath = "",
    [switch]$UpdateBaseline,
    [switch]$FailOnRegression
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repoRoot

if (-not $ExePath) {
    $ExePath = Join-Path $repoRoot (Join-Path $BuildDir "bin\RawrXD-Win32IDE.exe")
}
if (-not (Test-Path $ExePath)) {
    throw "Win32IDE binary not found: $ExePath (build RawrXD-Win32IDE first)"
}

$reportDir = Join-Path $repoRoot "reports\win32ide"
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

function Find-DumpBin {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($install) {
            $candidate = Join-Path $install "VC\Tools\MSVC"
            if (Test-Path $candidate) {
                $ver = Get-ChildItem $candidate | Sort-Object Name -Descending | Select-Object -First 1
                $dumpbin = Join-Path $ver.FullName "bin\Hostx64\x64\dumpbin.exe"
                if (Test-Path $dumpbin) { return $dumpbin }
            }
        }
    }
    $fallback = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\dumpbin.exe"
    if (Test-Path $fallback) { return $fallback }
    throw "dumpbin.exe not found (install VS C++ build tools)"
}

$dumpbin = Find-DumpBin
$exeItem = Get-Item $ExePath
$mapPath = [System.IO.Path]::ChangeExtension($ExePath, ".map")

$importsRaw = & $dumpbin /imports $ExePath 2>&1 | Out-String
$summaryRaw = & $dumpbin /summary $ExePath 2>&1 | Out-String

$imports = [System.Collections.Generic.List[string]]::new()
foreach ($line in ($importsRaw -split "`r?`n")) {
    if ($line -match '^\s+([A-Za-z0-9_\-]+\.dll)\s*$') {
        $dll = $Matches[1].ToLowerInvariant()
        if ($imports -notcontains $dll) { $imports.Add($dll) | Out-Null }
    }
}
$imports.Sort()

$sectionSizes = @{}
foreach ($line in ($summaryRaw -split "`r?`n")) {
    if ($line -match '^\s+([0-9A-Fa-f]+)\s+([.#A-Za-z0-9_]+)\s*$') {
        try {
            $sectionSizes[$Matches[2].Trim()] = [Convert]::ToInt64($Matches[1].Trim(), 16)
        } catch { }
    }
}

$probeGates = $true
$cachePath = Join-Path $repoRoot (Join-Path $BuildDir "CMakeCache.txt")
if (Test-Path $cachePath) {
    $cache = Get-Content $cachePath -Raw
    if ($cache -match 'RAWRXD_ENABLE_IDE_PROBE_GATES:BOOL=OFF') { $probeGates = $false }
}

$snapshot = [ordered]@{
    binary_size_bytes   = $exeItem.Length
    import_count        = $imports.Count
    imports             = $imports
    section_sizes       = $sectionSizes
    probe_gates_enabled = $probeGates
    exe_path            = $ExePath
    map_present         = (Test-Path $mapPath)
    timestamp_utc       = (Get-Date).ToUniversalTime().ToString("o")
}

$jsonPath = Join-Path $reportDir "link-baseline.json"
$importsPath = Join-Path $reportDir "imports.txt"
$sectionsPath = Join-Path $reportDir "sections.txt"
$sizePath = Join-Path $reportDir "binary-size.txt"

$imports | ForEach-Object { $_ } | Set-Content -Path $importsPath -Encoding utf8
$sectionLines = @($sectionSizes.GetEnumerator() | Sort-Object Name | ForEach-Object { "{0}={1}" -f $_.Key, $_.Value })
if ($sectionLines.Count -eq 0) { $sectionLines = @("# no sections parsed") }
$sectionLines | Set-Content -Path $sectionsPath -Encoding utf8
Set-Content -Path $sizePath -Value $exeItem.Length -Encoding utf8

if (Test-Path $mapPath) {
    Copy-Item -Force $mapPath (Join-Path $reportDir "symbols.map")
} elseif (Test-Path (Join-Path $repoRoot (Join-Path $BuildDir "bin\RawrXD-Win32IDE.map"))) {
    Copy-Item -Force (Join-Path $repoRoot (Join-Path $BuildDir "bin\RawrXD-Win32IDE.map")) (Join-Path $reportDir "symbols.map")
}

if ($UpdateBaseline) {
    ($snapshot | ConvertTo-Json -Depth 6) | Set-Content -Path $jsonPath -Encoding utf8
    Write-Host "[link-baseline] Updated $jsonPath ($($exeItem.Length) bytes, $($imports.Count) imports)"
    exit 0
}

if (-not (Test-Path $jsonPath)) {
    ($snapshot | ConvertTo-Json -Depth 6) | Set-Content -Path $jsonPath -Encoding utf8
    Write-Host "[link-baseline] Created initial baseline at $jsonPath"
    exit 0
}

$baseline = Get-Content $jsonPath -Raw | ConvertFrom-Json
$exitCode = 0
$sizeDelta = $snapshot.binary_size_bytes - [int64]$baseline.binary_size_bytes
$sizePct = if ($baseline.binary_size_bytes -gt 0) { 100.0 * $sizeDelta / $baseline.binary_size_bytes } else { 0 }

Write-Host "[link-baseline] binary_size=$($snapshot.binary_size_bytes) delta=${sizeDelta} (${sizePct:N2}%) imports=$($snapshot.import_count)"

$baselineImports = @($baseline.imports)
$added = @($imports | Where-Object { $_ -notin $baselineImports })
$removed = @($baselineImports | Where-Object { $_ -notin $imports })

if ($added.Count -gt 0) {
    Write-Host "[link-baseline] NEW imports:" ($added -join ", ")
}
if ($removed.Count -gt 0) {
    Write-Host "[link-baseline] REMOVED imports:" ($removed -join ", ")
}

$suspect = @('cuda', 'vulkan', 'onnx', 'sovereign', 'harness') | ForEach-Object { $added | Where-Object { $_ -like "*$_*" } }
if ($suspect.Count -gt 0) {
    Write-Warning "[link-baseline] Suspect new imports: $($suspect -join ', ')"
}

if ($FailOnRegression) {
  $maxSizePct = 15.0
  if ([math]::Abs($sizePct) -gt $maxSizePct) {
    Write-Error "[link-baseline] Binary size drift ${sizePct:N2}% exceeds ${maxSizePct}% threshold"
    $exitCode = 1
  }
  if ($added.Count -gt 0) {
    Write-Error "[link-baseline] Import table grew by $($added.Count) DLL(s)"
    $exitCode = 1
  }
}

exit $exitCode
