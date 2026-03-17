param(
    [string]$BuildDir = "D:\\rawrxd\\build_gold",
    [string]$FinalTarget = "RawrXD-Gold",
    [switch]$StopOnFail
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-Ninja([string]$argString) {
    $ninjaArgs = @()
    if ($argString -and $argString.Trim().Length -gt 0) {
        $ninjaArgs = $argString.Split(" ")
    }
    $output = & ninja -C $BuildDir @ninjaArgs 2>&1
    $code = $LASTEXITCODE
    return ,@($code, ($output -join "`n"))
}

if (-not (Test-Path $BuildDir)) {
    throw "BuildDir not found: $BuildDir"
}

$reportPath = Join-Path $BuildDir "per_file_build_report.txt"
$started = Get-Date

"Per-file build: $started" | Out-File -FilePath $reportPath -Encoding ascii
"BuildDir: $BuildDir" | Out-File -FilePath $reportPath -Encoding ascii -Append
"FinalTarget: $FinalTarget" | Out-File -FilePath $reportPath -Encoding ascii -Append

# Enumerate per-file object/resource outputs from Ninja's target graph.
$targetsText = & ninja -C $BuildDir -t targets all 2>&1
if ($LASTEXITCODE -ne 0) {
    "ERROR: ninja -t targets failed" | Out-File -FilePath $reportPath -Encoding ascii -Append
    $targetsText | Out-File -FilePath $reportPath -Encoding ascii -Append
    exit 2
}

$outputs = New-Object System.Collections.Generic.List[string]
foreach ($line in $targetsText) {
    if ($line -match '^(?<out>.+\.(obj|res))(:|$)') {
        $outputs.Add($Matches['out'])
    }
}
$outputs = $outputs | Sort-Object -Unique

"Per-file outputs: $($outputs.Count)" | Out-File -FilePath $reportPath -Encoding ascii -Append

$failures = New-Object System.Collections.Generic.List[string]
$i = 0
foreach ($out in $outputs) {
    $i++
    Write-Host ("[{0}/{1}] {2}" -f $i, $outputs.Count, $out)
    $result = Invoke-Ninja $out
    $code = $result[0]
    $text = $result[1]
    if ($code -ne 0) {
        $failures.Add($out)
        ("FAIL: {0}" -f $out) | Out-File -FilePath $reportPath -Encoding ascii -Append
        $text | Out-File -FilePath $reportPath -Encoding ascii -Append
        if ($StopOnFail) {
            break
        }
    }
}

# Final link step for the requested target.
Write-Host ("[FINAL] Linking target: {0}" -f $FinalTarget)
$final = Invoke-Ninja $FinalTarget
if ($final[0] -ne 0) {
    $failures.Add("FINAL:$FinalTarget")
    "FAIL: final link" | Out-File -FilePath $reportPath -Encoding ascii -Append
    $final[1] | Out-File -FilePath $reportPath -Encoding ascii -Append
}

$ended = Get-Date
"Completed: $ended" | Out-File -FilePath $reportPath -Encoding ascii -Append
"Failures: $($failures.Count)" | Out-File -FilePath $reportPath -Encoding ascii -Append

if ($failures.Count -gt 0) {
    "Failed outputs:" | Out-File -FilePath $reportPath -Encoding ascii -Append
    $failures | Out-File -FilePath $reportPath -Encoding ascii -Append
    exit 1
}

exit 0
