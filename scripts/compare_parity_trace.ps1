# compare_parity_trace.ps1 — Structural diff of CLI vs UI inference traces
#
# Usage:
#   .\compare_parity_trace.ps1 -Cli cli.json -Ui ui.json [-IgnoreTiming]
#
# Compares two trace envelopes emitted by:
#   rawrxd.exe run <model> --prompt "..." --emit-json-trace cli.json
#   $env:RAWRXD_PIPELINE_TRACE="ui.json"; RawrXD-Win32IDE.exe ...
#
# Exit code 0 = structural match, 1 = mismatch.

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)] [string]$Cli,
    [Parameter(Mandatory=$true)] [string]$Ui,
    [switch]$IgnoreTiming
)

if (!(Test-Path $Cli)) { Write-Error "CLI trace not found: $Cli"; exit 2 }
if (!(Test-Path $Ui))  { Write-Error "UI  trace not found: $Ui";  exit 2 }

$a = Get-Content $Cli -Raw | ConvertFrom-Json
$b = Get-Content $Ui  -Raw | ConvertFrom-Json

$mismatches = @()

if ($a.PSObject.Properties.Name -contains 'backend') {
    if ([string]::IsNullOrWhiteSpace($a.backend)) { $mismatches += "cli backend missing" }
    if ([string]::IsNullOrWhiteSpace($b.backend)) { $mismatches += "ui backend missing" }
    if (($a.PSObject.Properties.Name -contains 'backend') -and
        ($b.PSObject.Properties.Name -contains 'backend') -and
        -not [string]::IsNullOrWhiteSpace($a.backend) -and
        -not [string]::IsNullOrWhiteSpace($b.backend) -and
        ($a.backend -ne $b.backend)) {
        $mismatches += "backend: '$($a.backend)' vs '$($b.backend)'"
    }
}

if ($a.PSObject.Properties.Name -contains 'device') {
    if ([string]::IsNullOrWhiteSpace($a.device)) { $mismatches += "cli device missing" }
    if ([string]::IsNullOrWhiteSpace($b.device)) { $mismatches += "ui device missing" }
    if (($a.PSObject.Properties.Name -contains 'device') -and
        ($b.PSObject.Properties.Name -contains 'device') -and
        -not [string]::IsNullOrWhiteSpace($a.device) -and
        -not [string]::IsNullOrWhiteSpace($b.device) -and
        ($a.device -ne $b.device)) {
        $mismatches += "device: '$($a.device)' vs '$($b.device)'"
    }
}

if ($a.model  -ne $b.model)  { $mismatches += "model: '$($a.model)' vs '$($b.model)'" }
if ($a.prompt -ne $b.prompt) { $mismatches += "prompt mismatch" }
if ($a.error  -ne $b.error)  { $mismatches += "error: '$($a.error)' vs '$($b.error)'" }
if ($a.token_count -ne $b.token_count) {
    $mismatches += "token_count: $($a.token_count) vs $($b.token_count)"
}

# Token-by-token comparison (the most important parity signal).
$min = [Math]::Min($a.tokens.Count, $b.tokens.Count)
$divergeAt = -1
for ($i = 0; $i -lt $min; $i++) {
    if ($a.tokens[$i] -ne $b.tokens[$i]) { $divergeAt = $i; break }
}
if ($divergeAt -ge 0) {
    $mismatches += "tokens diverge at index ${divergeAt}: cli=[$($a.tokens[$divergeAt])] ui=[$($b.tokens[$divergeAt])]"
}

# Sequence-number checks: detect async reordering on either side.
if ($a.PSObject.Properties.Name -contains 'seq_monotonic') {
    if (-not $a.seq_monotonic) { $mismatches += "cli seq is non-monotonic (async reordering on CLI side)" }
    if (-not $b.seq_monotonic) { $mismatches += "ui seq is non-monotonic (async reordering on UI side)" }
    if ($a.seq.Count -ne $b.seq.Count) {
        $mismatches += "seq length differs: cli=$($a.seq.Count) ui=$($b.seq.Count)"
    } else {
        $seqMin = $a.seq.Count
        for ($i = 0; $i -lt $seqMin; $i++) {
            if ($a.seq[$i] -ne $b.seq[$i]) {
                $mismatches += "seq diverges at index ${i}: cli=$($a.seq[$i]) ui=$($b.seq[$i])"
                break
            }
        }
    }
}

# Joined-text equality (catches whitespace/encoding drift even if token splits differ).
$cliText = ($a.tokens -join '')
$uiText  = ($b.tokens -join '')
if ($cliText -ne $uiText) {
    $mismatches += "joined text differs (cli=$($cliText.Length) chars, ui=$($uiText.Length) chars)"
}

# Timing report (informational unless -IgnoreTiming is off and they diverge wildly).
$cliFt = $a.first_token_ms; $uiFt = $b.first_token_ms
$cliTot = $a.completed_ms;  $uiTot = $b.completed_ms
Write-Host ""
Write-Host "=== Parity Trace Comparison ==="
Write-Host ("  source       : {0,-12} {1,-12}" -f $a.source, $b.source)
if ($a.PSObject.Properties.Name -contains 'backend') {
    Write-Host ("  backend      : {0,-12} {1,-12}" -f $a.backend, $b.backend)
}
if ($a.PSObject.Properties.Name -contains 'device') {
    Write-Host ("  device       : {0,-12} {1,-12}" -f $a.device, $b.device)
}
Write-Host ("  model        : {0,-12} {1,-12}" -f $a.model,  $b.model)
Write-Host ("  token_count  : {0,-12} {1,-12}" -f $a.token_count, $b.token_count)
Write-Host ("  first_token  : {0,-12} {1,-12} ms" -f $cliFt, $uiFt)
Write-Host ("  completed    : {0,-12} {1,-12} ms" -f $cliTot, $uiTot)
Write-Host ("  joined_chars : {0,-12} {1,-12}" -f $cliText.Length, $uiText.Length)
if ($a.PSObject.Properties.Name -contains 'seq_monotonic') {
    Write-Host ("  seq_monotonic: {0,-12} {1,-12}" -f $a.seq_monotonic, $b.seq_monotonic)
}

# Per-token timing report (token_us, microseconds since started, parallel to tokens[]).
function Get-TokenTimingStats($trace) {
    if (-not ($trace.PSObject.Properties.Name -contains 'token_us')) { return $null }
    $u = $trace.token_us
    if ($null -eq $u -or $u.Count -eq 0) { return $null }
    $deltas = New-Object System.Collections.Generic.List[long]
    [void]$deltas.Add([long]$u[0])  # gap from start to first token
    for ($i = 1; $i -lt $u.Count; $i++) {
        [void]$deltas.Add([long]($u[$i] - $u[$i-1]))
    }
    $sum = 0L; foreach ($d in $deltas) { $sum += $d }
    $sorted = ($deltas | Sort-Object)
    [pscustomobject]@{
        Count   = $u.Count
        First   = [long]$u[0]
        Last    = [long]$u[$u.Count-1]
        SpanUs  = [long]($u[$u.Count-1] - $u[0])
        MinDeltaUs = [long]$sorted[0]
        MaxDeltaUs = [long]$sorted[$sorted.Count-1]
        MeanDeltaUs= [long]([math]::Round($sum / [double]$deltas.Count))
    }
}
$cliT = Get-TokenTimingStats $a
$uiT  = Get-TokenTimingStats $b
if ($cliT -and $uiT) {
    Write-Host ""
    Write-Host "  --- per-token timing (microseconds, steady_clock) ---"
    Write-Host ("  count        : {0,-12} {1,-12}" -f $cliT.Count, $uiT.Count)
    Write-Host ("  span_us      : {0,-12} {1,-12}" -f $cliT.SpanUs, $uiT.SpanUs)
    Write-Host ("  delta_min_us : {0,-12} {1,-12}" -f $cliT.MinDeltaUs, $uiT.MinDeltaUs)
    Write-Host ("  delta_mean_us: {0,-12} {1,-12}" -f $cliT.MeanDeltaUs, $uiT.MeanDeltaUs)
    Write-Host ("  delta_max_us : {0,-12} {1,-12}" -f $cliT.MaxDeltaUs, $uiT.MaxDeltaUs)
    if ($cliT.Count -ne $uiT.Count) {
        $mismatches += "token_us count: cli=$($cliT.Count) ui=$($uiT.Count)"
    }
}
Write-Host ""

if ($mismatches.Count -eq 0) {
    Write-Host "[PARITY OK] CLI and UI traces match structurally." -ForegroundColor Green
    exit 0
}

Write-Host "[PARITY FAIL] $($mismatches.Count) mismatch(es):" -ForegroundColor Red
$mismatches | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
exit 1
