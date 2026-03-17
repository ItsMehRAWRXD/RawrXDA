param(
    [Parameter(Mandatory = $true)]
    [string]$Dll,

    [ValidateSet("x64", "arm64", "any")]
    [string]$ExpectedMachine = "x64",

    [string[]]$RequiredExports = @(
        "RawrXD_Backend_Init",
        "RawrXD_EnqueueMatmul",
        "RawrXD_SubmitQueue"
    ),

    [switch]$SkipDependents
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Fail([string]$msg) {
    Write-Host "[INTERCONNECT PREFLIGHT] FAIL: $msg" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path -LiteralPath $Dll)) {
    Fail "DLL not found: $Dll"
}

$resolvedDll = (Resolve-Path -LiteralPath $Dll).Path
Write-Host "[INTERCONNECT PREFLIGHT] DLL: $resolvedDll"

$bytes = [System.IO.File]::ReadAllBytes($resolvedDll)
if ($bytes.Length -lt 0x100) {
    Fail "File too small to be a valid PE image."
}

if ($bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) {
    Fail "Missing MZ header."
}

$peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
if ($peOffset -lt 0 -or ($peOffset + 6) -ge $bytes.Length) {
    Fail "Invalid PE header offset."
}

if ($bytes[$peOffset] -ne 0x50 -or $bytes[$peOffset + 1] -ne 0x45 -or
    $bytes[$peOffset + 2] -ne 0x00 -or $bytes[$peOffset + 3] -ne 0x00) {
    Fail "Missing PE signature."
}

$machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
$machineName = switch ($machine) {
    0x8664 { "x64" }
    0xAA64 { "arm64" }
    default { ("0x{0:X4}" -f $machine) }
}

Write-Host "[INTERCONNECT PREFLIGHT] Machine: $machineName"
if ($ExpectedMachine -ne "any" -and $machineName -ne $ExpectedMachine) {
    Fail "Architecture mismatch. Expected $ExpectedMachine, got $machineName."
}

$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if (-not $dumpbin) {
    Write-Warning "[INTERCONNECT PREFLIGHT] dumpbin.exe not found on PATH; skipping export/dependency checks."
    Write-Host "[INTERCONNECT PREFLIGHT] PASS (header/arch checks only)" -ForegroundColor Green
    exit 0
}

$exportsText = & $dumpbin.Path /exports $resolvedDll 2>&1 | Out-String
foreach ($exportName in $RequiredExports) {
    if ($exportsText -notmatch ("(^|\s){0}(\s|$)" -f [Regex]::Escape($exportName))) {
        Fail "Missing required export: $exportName"
    }
}
Write-Host ("[INTERCONNECT PREFLIGHT] Exports OK: {0}" -f ($RequiredExports -join ", "))

if (-not $SkipDependents) {
    $depsText = & $dumpbin.Path /dependents $resolvedDll 2>&1 | Out-String
    Write-Host "[INTERCONNECT PREFLIGHT] Dependents:"
    ($depsText -split "`r?`n") |
        Where-Object { $_ -match '\.dll' } |
        ForEach-Object { Write-Host ("  " + $_.Trim()) }
}

Write-Host "[INTERCONNECT PREFLIGHT] PASS" -ForegroundColor Green
exit 0
