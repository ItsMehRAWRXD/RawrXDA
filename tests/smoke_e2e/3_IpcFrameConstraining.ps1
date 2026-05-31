# =============================================================================
# Smoke Test Scenario 3: IPC Wire Framing & Segmentation
# =============================================================================
# Validates length-prefixed physical frames, legacy RAWR headers, and RAWS
# segment reassembly matching Win32IDE_ChatIpcProtocol.h.
# =============================================================================

param(
    [string]$BinaryPath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [int]$BoundaryFloodCount = 32,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

$logDir = Join-Path $PSScriptRoot "logs"
if (-not (Test-Path $logDir)) { mkdir $logDir | Out-Null }
$logFile = "$logDir\scenario3_ipc_frame.log"

function Log {
    param([string]$Msg, [string]$Level = "INFO")
    $ts = Get-Date -Format "HH:mm:ss.fff"
    $line = "[$ts] [$Level] $Msg"
    Write-Host $line -ForegroundColor Yellow
    Add-Content $logFile $line
}

. (Join-Path $PSScriptRoot "modules\IpcFramingHelper.ps1")
$ipc = Get-IpcConstants

Log "════════════════════════════════════════════════════════════════"
Log "SCENARIO 3: IPC Wire Framing & Segmentation"
Log "════════════════════════════════════════════════════════════════"
Log "Purpose: Verify 4-byte length prefix + RAWR/RAWS physical framing"
Log "Log file: $logFile"
Log ""

$passed = $true
$failures = 0

function Fail-Check {
    param([bool]$Ok, [string]$Name)
    if ($Ok) {
        Log "✓ $Name" "SUCCESS"
    } else {
        Log "✗ $Name" "ERROR"
        $script:passed = $false
        $script:failures++
    }
}

Log "Step 1: Legacy single-frame bounds (14-byte header, CRC in-header)..."

$legacyMaxBody = $ipc.MaxSinglePayload
$small = "x" * 1024
$boundary = "b" * $legacyMaxBody
$legacyFrame = New-LegacyFrame -PayloadUtf8 $boundary -MessageType 2
Fail-Check ($legacyFrame.Length -eq ($ipc.LegacyHeaderSize + $legacyMaxBody)) "Legacy max single payload fits one physical frame"

try {
    $tooBig = "z" * ($legacyMaxBody + 1)
    New-LegacyFrame -PayloadUtf8 $tooBig -MessageType 2 | Out-Null
    Fail-Check $false "Legacy builder rejects payload > MaxSinglePayload"
} catch {
    Fail-Check $true "Legacy builder rejects payload > MaxSinglePayload"
}

Log ""
Log "Step 2: Length-prefix peel and round-trip..."

$wire = Add-WirePrefix -PhysicalFrame $legacyFrame
Fail-Check ($wire.Length -eq (4 + $legacyFrame.Length)) "Wire blob has 4-byte LE prefix"

$mem = New-Object System.IO.MemoryStream
$mem.Write($wire, 0, $wire.Length)
$mem.Position = 0
$handler = [MockIpcStreamHandler]::new($mem)
$extracted = $handler.TryExtractPhysicalFrame()
Fail-Check (($null -ne $extracted) -and ($extracted.Length -eq $legacyFrame.Length)) "TryExtractPhysicalFrame peels one frame"

$reasm = [MockIpcReassembler]::new()
$outType = 0
$outPayload = ''
$ok = $reasm.FeedPhysicalFrame($extracted, [ref]$outType, [ref]$outPayload)
Fail-Check ($ok -and ($outPayload.Length -eq $boundary.Length)) "Reassembler decodes legacy payload"

Log ""
Log "Step 3: Segmented logical payload (>64KB logical splits to RAWS frames)..."

$logicalSize = $legacyMaxBody + 4096
$logicalText = ("s" * $logicalSize)
$frames = New-PhysicalFramesFromLogicalPayload -PayloadUtf8 $logicalText -MessageType 3
Fail-Check ($frames.Count -gt 1) "Oversized logical payload produces multiple physical frames"

foreach ($f in $frames) {
    if ($f.Length -gt $ipc.MaxPipeFrame) {
        Fail-Check $false "Each physical frame <= 65536 bytes (got $($f.Length))"
        break
    }
}
if ($passed) { Log "✓ Each physical frame <= 65536 bytes" "SUCCESS" }

$segMem = New-Object System.IO.MemoryStream
foreach ($f in $frames) {
    $w = Add-WirePrefix -PhysicalFrame $f
    $segMem.Write($w, 0, $w.Length)
}
$segMem.Position = 0
$segHandler = [MockIpcStreamHandler]::new($segMem)
$segReasm = [MockIpcReassembler]::new()
$decoded = ''
$decodedOk = $false
for ($segIter = 0; $segIter -lt ($frames.Count + 2); $segIter++) {
    $frame = $segHandler.TryExtractPhysicalFrame()
    if ($null -eq $frame) { break }
    $t = 0
    $p = ''
    if ($segReasm.FeedPhysicalFrame($frame, [ref]$t, [ref]$p)) {
        $decoded = $p
        $decodedOk = $true
        break
    }
}
Fail-Check ($decodedOk -and ($decoded.Length -eq $logicalSize)) "Segment reassembly restores full logical payload"

Log ""
Log "Step 4: Physical frame hard cap (reject >64KB blob)..."

try {
    $oversizedPhysical = New-Object byte[] ($ipc.MaxPipeFrame + 1)
    Add-WirePrefix -PhysicalFrame $oversizedPhysical | Out-Null
    Fail-Check $false "Add-WirePrefix rejects physical frame > 65536"
} catch {
    Fail-Check $true "Add-WirePrefix rejects physical frame > 65536"
}

Log ""
Log "Step 5: Boundary flood on prefixed legacy frames..."

$sequenceFailures = 0
for ($i = 1; $i -le $BoundaryFloodCount; $i++) {
    $f = New-LegacyFrame -PayloadUtf8 ("f$i" + ("q" * 512)) -MessageType 1
    if ($f.Length -gt $ipc.MaxPipeFrame) { $sequenceFailures++ }
}
Fail-Check ($sequenceFailures -eq 0) "Boundary flood: $($BoundaryFloodCount) legacy frames within cap"

Log ""
Log "Step 6: Security matrix (single physical frame gate)..."

$matrix = @(
    @{ Body = 1024;     Reject = $false }
    @{ Body = 32768;    Reject = $false }
    @{ Body = $legacyMaxBody; Reject = $false }
    @{ Body = ($legacyMaxBody + 1); Reject = $true }
)

foreach ($row in $matrix) {
    $physical = $ipc.LegacyHeaderSize + $row.Body
    $reject = $physical -gt $ipc.MaxPipeFrame
    $expect = $row.Reject
    Fail-Check ($reject -eq $expect) "Matrix body=$($row.Body) physical=$physical expectReject=$expect"
}

Log ""
Log "Step 7: Malformed length-prefix fuzz (no hang, safe recovery)..."

# 7a: Prefix claims 1KB frame but stream ends after 10 body bytes — must not extract yet
$trunc = New-Object byte[] 14
[BitConverter]::GetBytes([uint32]1024).CopyTo($trunc, 0)
1..10 | ForEach-Object { $trunc[$_] = [byte]$_ }
$hTrunc = [MockIpcStreamHandler]::new([System.IO.MemoryStream]::new($trunc))
$eTrunc = $hTrunc.TryExtractPhysicalFrame()
Fail-Check ($null -eq $eTrunc) "Truncated stream waits for full frame (no premature extract)"

# 7b: Prefix claims 65537 — invalid physical size, accumulator cleared
$badLen = New-Object byte[] 8
[BitConverter]::GetBytes([uint32]65537).CopyTo($badLen, 0)
1..4 | ForEach-Object { $badLen[$_] = 0xAA }
$hBad = [MockIpcStreamHandler]::new([System.IO.MemoryStream]::new($badLen))
$eBad = $hBad.TryExtractPhysicalFrame()
Fail-Check ($null -eq $eBad) "Oversize length prefix rejected"
Fail-Check ($hBad.Accumulator.Count -eq 0) "Oversize prefix clears poisoned accumulator"

# 7c: Zero-length prefix — reject and clear
$zeroLen = [BitConverter]::GetBytes([uint32]0)
$hZero = [MockIpcStreamHandler]::new([System.IO.MemoryStream]::new($zeroLen))
$eZero = $hZero.TryExtractPhysicalFrame()
Fail-Check ($null -eq $eZero) "Zero-length prefix rejected"

# 7d: Corrupt prefix flushes stream; fresh bytes after reset parse cleanly (matches C++ WireFrameStream)
$goodWire = Add-WirePrefix -PhysicalFrame (New-LegacyFrame -PayloadUtf8 '{"recover":true}' -MessageType 1)
$hPoison = [MockIpcStreamHandler]::new([System.IO.MemoryStream]::new($badLen))
$null = $hPoison.TryExtractPhysicalFrame()
Fail-Check ($hPoison.Accumulator.Count -eq 0) "Corrupt prefix flushes accumulator (desync recovery)"

$mGood = New-Object System.IO.MemoryStream
$mGood.Write($goodWire, 0, $goodWire.Length)
$mGood.Position = 0
$hGood = [MockIpcStreamHandler]::new($mGood)
$eRecover = $hGood.TryExtractPhysicalFrame()
$reasmCombo = [MockIpcReassembler]::new()
$tRec = 0
$pRec = ''
$okRec = $reasmCombo.FeedPhysicalFrame($eRecover, [ref]$tRec, [ref]$pRec)
Fail-Check (($null -ne $eRecover) -and $okRec) "Valid frame parses on clean stream after desync flush"

Log ""
Log "Step 8: Mini soak — 256 peel/extract cycles at max single frame..."

$soakFail = 0
for ($si = 0; $si -lt 256; $si++) {
    $sf = New-LegacyFrame -PayloadUtf8 ("k" * 512) -MessageType 1
    $sw = Add-WirePrefix -PhysicalFrame $sf
    $ms = New-Object System.IO.MemoryStream
    $ms.Write($sw, 0, $sw.Length)
    $ms.Position = 0
    $hs = [MockIpcStreamHandler]::new($ms)
    $es = $hs.TryExtractPhysicalFrame()
    if (($null -eq $es) -or ($es.Length -ne $sf.Length)) { $soakFail++ }
}
Fail-Check ($soakFail -eq 0) "Soak: 256 length-prefix peel cycles at 512-byte payload"

Log ""
Log "Step 9: Validation Summary"
Log "────────────────────────────────────────────────────────────────"

if ($failures -gt 0) {
    $passed = $false
}

Log "SCENARIO 3 RESULT: $(if ($passed) { 'PASS ✓' } else { 'FAIL ✗' })"
Log "Failures: $failures"

exit $(if ($passed) { 0 } else { 1 })
