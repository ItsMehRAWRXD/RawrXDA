param(
    [Parameter(Mandatory = $false)]
    [string]$Line,

    [Parameter(Mandatory = $false)]
    [switch]$StrictHeadlessLine
)

$strictPattern = '^\[SOVEREIGN_SUCCESS\]\s+0x1751431337\s+TPS=\d+(\.\d+)?\s*$'
$legacyPattern = '^\[SOVEREIGN_SUCCESS\]\s+0x1751431337\s+TPS=\d+(\.\d+)?(\s+TOKENS=\d+\s+SCHEMA=\S+)?\s*$'

function Test-SovereignHeadlessLine {
    param(
        [Parameter(Mandatory = $true)]
        [string]$InputLine,

        [Parameter(Mandatory = $false)]
        [switch]$Strict
    )

    if ($Strict) {
        return [bool]($InputLine -match $strictPattern)
    }

    return [bool]($InputLine -match $legacyPattern)
}

if ($PSBoundParameters.ContainsKey('Line')) {
    $ok = Test-SovereignHeadlessLine -InputLine $Line -Strict:$StrictHeadlessLine
    if ($ok) {
        Write-Host "PASS"
        exit 0
    }

    Write-Host "FAIL"
    Write-Host "Input: $Line"
    if ($StrictHeadlessLine) {
        Write-Host "Expected strict regex: $strictPattern"
    } else {
        Write-Host "Expected compatible regex: $legacyPattern"
    }
    exit 1
}

# Pester-less smoke checks for CI and local validation
$strictSample = '[SOVEREIGN_SUCCESS] 0x1751431337 TPS=42.50'
$legacySample = '[SOVEREIGN_SUCCESS] 0x1751431337 TPS=42.50 TOKENS=128 SCHEMA=HEADLESS_STREAM'

if (-not (Test-SovereignHeadlessLine -InputLine $strictSample -Strict)) {
    throw "Strict sample did not match strict regex"
}

if (-not (Test-SovereignHeadlessLine -InputLine $strictSample)) {
    throw "Strict sample did not match compatible regex"
}

if (-not (Test-SovereignHeadlessLine -InputLine $legacySample)) {
    throw "Legacy sample did not match compatible regex"
}

Write-Host "Sovereign test cases passed"
exit 0
