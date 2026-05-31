# =============================================================================
# IpcFramingHelper.ps1 — Symmetrical RAWR/RAWR+1 length-prefixed pipe framing
# Matches Win32IDE_ChatIpcProtocol.h (C++ IDE)
# =============================================================================

$script:IPC_MAGIC_LEGACY   = [uint32]0x52415752   # "RAWR" little-endian
$script:IPC_MAGIC_SEGMENT  = [uint32]0x52415753   # segment continuation magic
$script:LEGACY_HEADER_SIZE = 14
$script:SEGMENT_HEADER_SIZE = 28
$script:WIRE_PREFIX_SIZE   = 4
$script:MAX_PIPE_FRAME     = 65536
$script:MAX_SINGLE_PAYLOAD = $script:MAX_PIPE_FRAME - $script:LEGACY_HEADER_SIZE   # 65522
$script:MAX_CHUNK_PAYLOAD  = $script:MAX_PIPE_FRAME - $script:SEGMENT_HEADER_SIZE # 65508
$script:MAX_LOGICAL_BYTES  = 16MB

function Get-IpcConstants {
    [PSCustomObject]@{
        MagicLegacy       = $script:IPC_MAGIC_LEGACY
        MagicSegment      = $script:IPC_MAGIC_SEGMENT
        LegacyHeaderSize  = $script:LEGACY_HEADER_SIZE
        SegmentHeaderSize = $script:SEGMENT_HEADER_SIZE
        WirePrefixSize    = $script:WIRE_PREFIX_SIZE
        MaxPipeFrame      = $script:MAX_PIPE_FRAME
        MaxSinglePayload  = $script:MAX_SINGLE_PAYLOAD
        MaxChunkPayload   = $script:MAX_CHUNK_PAYLOAD
    }
}

function Get-Crc32 {
    param([byte[]]$Data, [int]$Offset = 0, [int]$Length = -1)
    if ($Length -lt 0) { $Length = $Data.Length - $Offset }
    $crc = [uint32][uint32]::MaxValue
    for ($i = $Offset; $i -lt ($Offset + $Length); $i++) {
        $crc = $crc -bxor $Data[$i]
        for ($j = 0; $j -lt 8; $j++) {
            if (($crc -band 1) -ne 0) {
                $crc = ($crc -shr 1) -bxor 0xEDB88320
            } else {
                $crc = $crc -shr 1
            }
        }
    }
    return ($crc -bxor 0xFFFFFFFF)
}

function New-LegacyFrame {
    param(
        [string]$PayloadUtf8,
        [uint16]$MessageType = 1
    )
    $payloadBytes = [System.Text.Encoding]::UTF8.GetBytes($PayloadUtf8)
    if ($payloadBytes.Length -gt $script:MAX_SINGLE_PAYLOAD) {
        throw "Legacy frame payload exceeds MAX_SINGLE_PAYLOAD ($($script:MAX_SINGLE_PAYLOAD))"
    }

    $frame = New-Object byte[] ($script:LEGACY_HEADER_SIZE + $payloadBytes.Length)
    [BitConverter]::GetBytes([uint32]$script:IPC_MAGIC_LEGACY).CopyTo($frame, 0)
    [BitConverter]::GetBytes([uint16]$MessageType).CopyTo($frame, 4)
    [BitConverter]::GetBytes([uint32]$payloadBytes.Length).CopyTo($frame, 6)
    $crc = Get-Crc32 -Data $payloadBytes
    [BitConverter]::GetBytes([uint32]$crc).CopyTo($frame, 10)
    if ($payloadBytes.Length -gt 0) {
        [Array]::Copy($payloadBytes, 0, $frame, $script:LEGACY_HEADER_SIZE, $payloadBytes.Length)
    }
    return $frame
}

function New-SegmentFrame {
    param(
        [byte[]]$ChunkBytes,
        [uint16]$MessageType,
        [uint16]$Flags,
        [uint32]$TotalLen,
        [uint16]$SegIndex,
        [uint16]$SegCount,
        [uint32]$LogicalCrc
    )
    if ($ChunkBytes.Length -gt $script:MAX_CHUNK_PAYLOAD) {
        throw "Chunk exceeds MAX_CHUNK_PAYLOAD"
    }

    $frame = New-Object byte[] ($script:SEGMENT_HEADER_SIZE + $ChunkBytes.Length)
    [BitConverter]::GetBytes([uint32]$script:IPC_MAGIC_SEGMENT).CopyTo($frame, 0)
    [BitConverter]::GetBytes([uint16]$MessageType).CopyTo($frame, 4)
    [BitConverter]::GetBytes([uint16]$Flags).CopyTo($frame, 6)
    [BitConverter]::GetBytes([uint32]$TotalLen).CopyTo($frame, 8)
    [BitConverter]::GetBytes([uint16]$SegIndex).CopyTo($frame, 12)
    [BitConverter]::GetBytes([uint16]$SegCount).CopyTo($frame, 14)
    [BitConverter]::GetBytes([uint32]$ChunkBytes.Length).CopyTo($frame, 16)
    $chunkCrc = Get-Crc32 -Data $ChunkBytes
    [BitConverter]::GetBytes([uint32]$chunkCrc).CopyTo($frame, 20)
    [BitConverter]::GetBytes([uint32]$LogicalCrc).CopyTo($frame, 24)
    if ($ChunkBytes.Length -gt 0) {
        [Array]::Copy($ChunkBytes, 0, $frame, $script:SEGMENT_HEADER_SIZE, $ChunkBytes.Length)
    }
    return $frame
}

function New-PhysicalFramesFromLogicalPayload {
    param(
        [string]$PayloadUtf8,
        [uint16]$MessageType = 1
    )
    $payloadBytes = [System.Text.Encoding]::UTF8.GetBytes($PayloadUtf8)
    if ($payloadBytes.Length -gt $script:MAX_LOGICAL_BYTES) {
        throw "Logical payload too large"
    }

    $frames = New-Object System.Collections.Generic.List[byte[]]

    if ($payloadBytes.Length -le $script:MAX_SINGLE_PAYLOAD) {
        $frames.Add((New-LegacyFrame -PayloadUtf8 $PayloadUtf8 -MessageType $MessageType))
        return $frames.ToArray()
    }

    $logicalCrc = Get-Crc32 -Data $payloadBytes
    $totalLen = [uint32]$payloadBytes.Length
    $segCount = [uint16][Math]::Ceiling($payloadBytes.Length / $script:MAX_CHUNK_PAYLOAD)

    for ($i = 0; $i -lt $segCount; $i++) {
        $offset = $i * $script:MAX_CHUNK_PAYLOAD
        $len = [Math]::Min($script:MAX_CHUNK_PAYLOAD, $payloadBytes.Length - $offset)
        $chunk = New-Object byte[] $len
        [Array]::Copy($payloadBytes, $offset, $chunk, 0, $len)

        $flags = 0x0004  # SegContinuation
        if ($i -eq 0) { $flags = $flags -bor 0x0001 }       # SegFirst
        if ($i -eq ($segCount - 1)) { $flags = $flags -bor 0x0002 }  # SegLast

        $frames.Add((New-SegmentFrame -ChunkBytes $chunk -MessageType $MessageType -Flags $flags `
            -TotalLen $totalLen -SegIndex $i -SegCount $segCount -LogicalCrc $logicalCrc))
    }

    return $frames.ToArray()
}

function Add-WirePrefix {
    param([byte[]]$PhysicalFrame)
    if ($PhysicalFrame.Length -gt $script:MAX_PIPE_FRAME) {
        throw "Physical frame exceeds MAX_PIPE_FRAME"
    }
    $wire = New-Object byte[] ($script:WIRE_PREFIX_SIZE + $PhysicalFrame.Length)
    [BitConverter]::GetBytes([uint32]$PhysicalFrame.Length).CopyTo($wire, 0)
    [Array]::Copy($PhysicalFrame, 0, $wire, $script:WIRE_PREFIX_SIZE, $PhysicalFrame.Length)
    return $wire
}

function Write-PrefixedFramesToStream {
    param(
        [System.IO.Stream]$Stream,
        [byte[][]]$PhysicalFrames
    )
    foreach ($frame in $PhysicalFrames) {
        $wire = Add-WirePrefix -PhysicalFrame $frame
        $Stream.Write($wire, 0, $wire.Length)
    }
    $Stream.Flush()
}

class MockIpcStreamHandler {
    hidden static [uint32] $MagicLegacy = [uint32]0x52415752
    hidden static [uint32] $MagicSegment = [uint32]0x52415753
    hidden static [int] $LegacyHeaderSize = 14
    hidden static [int] $SegmentHeaderSize = 28
    hidden static [int] $MaxPipeFrame = 65536
    hidden static [int] $MaxSinglePayload = 65522
    hidden static [int] $MaxChunkPayload = 65508

    [System.IO.Stream] $Stream
    [System.Collections.Generic.List[byte]] $Accumulator

    MockIpcStreamHandler([System.IO.Stream]$stream) {
        $this.Stream = $stream
        $this.Accumulator = New-Object 'System.Collections.Generic.List[byte]'
    }

    [void] ReadAvailableBytes([int]$WaitMs = 25) {
        if ($null -eq $this.Stream -or -not $this.Stream.CanRead) { return }
        $buf = New-Object byte[] 8192

        if ($this.Stream -is [System.IO.MemoryStream]) {
            while ($this.Stream.Position -lt $this.Stream.Length) {
                $n = $this.Stream.Read($buf, 0, $buf.Length)
                if ($n -le 0) { break }
                for ($i = 0; $i -lt $n; $i++) {
                    [void]$this.Accumulator.Add($buf[$i])
                }
            }
            return
        }

        while ($true) {
            $readTask = $this.Stream.BeginRead($buf, 0, $buf.Length, $null, $null)
            if (-not $readTask.AsyncWaitHandle.WaitOne($WaitMs)) {
                break
            }
            $n = $this.Stream.EndRead($readTask)
            if ($n -le 0) { break }
            for ($i = 0; $i -lt $n; $i++) {
                [void]$this.Accumulator.Add($buf[$i])
            }
        }
    }

    [byte[]] TryExtractPhysicalFrame() {
        $this.ReadAvailableBytes(0)
        if ($this.Accumulator.Count -lt 4) { return $null }
        $arr = $this.Accumulator.ToArray()
        $frameLen = [BitConverter]::ToUInt32($arr, 0)
        if ($frameLen -eq 0 -or $frameLen -gt [MockIpcStreamHandler]::MaxPipeFrame) {
            $this.Accumulator.Clear()
            return $null
        }
        $total = 4 + [int]$frameLen
        if ($this.Accumulator.Count -lt $total) { return $null }

        $frame = New-Object byte[] $frameLen
        [Array]::Copy($arr, 4, $frame, 0, $frameLen)
        $this.Accumulator.RemoveRange(0, $total)
        return $frame
    }

    [void] WriteLegacyReply([string]$payload, [uint16]$messageType = 4) {
        $frame = New-LegacyFrame -PayloadUtf8 $payload -MessageType $messageType
        $wire = Add-WirePrefix -PhysicalFrame $frame
        $this.Stream.Write($wire, 0, $wire.Length)
        $this.Stream.Flush()
    }

    [void] WriteLogicalReply([string]$payload, [uint16]$messageType = 4) {
        $frames = New-PhysicalFramesFromLogicalPayload -PayloadUtf8 $payload -MessageType $messageType
        Write-PrefixedFramesToStream -Stream $this.Stream -PhysicalFrames $frames
    }
}

class MockIpcReassembler {
    hidden [hashtable] $Slots = @{}

    [bool] FeedPhysicalFrame([byte[]]$frame, [ref]$outType, [ref]$outPayload) {
        if ($null -eq $frame -or $frame.Length -lt [MockIpcStreamHandler]::LegacyHeaderSize) { return $false }

        $magic = [BitConverter]::ToUInt32($frame, 0)
        if ($magic -eq [MockIpcStreamHandler]::MagicLegacy) {
            $type = [BitConverter]::ToUInt16($frame, 4)
            $len = [BitConverter]::ToUInt32($frame, 6)
            $crc = [BitConverter]::ToUInt32($frame, 10)
            if ($len -gt [MockIpcStreamHandler]::MaxSinglePayload) { return $false }
            if (([MockIpcStreamHandler]::LegacyHeaderSize + $len) -gt $frame.Length) { return $false }
            $payloadBytes = New-Object byte[] $len
            [Array]::Copy($frame, [MockIpcStreamHandler]::LegacyHeaderSize, $payloadBytes, 0, $len)
            if ((Get-Crc32 -Data $payloadBytes) -ne $crc) { return $false }
            $outType.Value = $type
            $outPayload.Value = [System.Text.Encoding]::UTF8.GetString($payloadBytes)
            return $true
        }

        if ($magic -ne [MockIpcStreamHandler]::MagicSegment -or $frame.Length -lt [MockIpcStreamHandler]::SegmentHeaderSize) {
            return $false
        }

        $type = [BitConverter]::ToUInt16($frame, 4)
        $totalLen = [BitConverter]::ToUInt32($frame, 8)
        $segIndex = [BitConverter]::ToUInt16($frame, 12)
        $segCount = [BitConverter]::ToUInt16($frame, 14)
        $chunkLen = [BitConverter]::ToUInt32($frame, 16)
        $chunkCrc = [BitConverter]::ToUInt32($frame, 20)
        $logicalCrc = [BitConverter]::ToUInt32($frame, 24)

        if ($chunkLen -gt [MockIpcStreamHandler]::MaxChunkPayload) { return $false }
        if (([MockIpcStreamHandler]::SegmentHeaderSize + $chunkLen) -gt $frame.Length) { return $false }

        $chunk = New-Object byte[] $chunkLen
        [Array]::Copy($frame, [MockIpcStreamHandler]::SegmentHeaderSize, $chunk, 0, $chunkLen)
        if ((Get-Crc32 -Data $chunk) -ne $chunkCrc) { return $false }

        $key = "{0}:{1}:{2}" -f $type, $logicalCrc, $totalLen
        if (-not $this.Slots.ContainsKey($key)) {
            $this.Slots[$key] = @{
                Type = $type
                TotalLen = $totalLen
                LogicalCrc = $logicalCrc
                SegCount = $segCount
                Chunks = New-Object 'object[]' $segCount
                Received = 0
            }
        }

        $slot = $this.Slots[$key]
        if ($null -eq $slot.Chunks[$segIndex]) {
            $slot.Chunks[$segIndex] = $chunk
            $slot.Received++
        }

        if ($slot.Received -lt $slot.SegCount) { return $false }

        $assembled = New-Object System.Collections.Generic.List[byte]
        for ($i = 0; $i -lt $slot.SegCount; $i++) {
            $assembled.AddRange($slot.Chunks[$i])
        }

        if ($assembled.Count -ne $slot.TotalLen) {
            $this.Slots.Remove($key)
            return $false
        }

        $bytes = $assembled.ToArray()
        if ((Get-Crc32 -Data $bytes) -ne $slot.LogicalCrc) {
            $this.Slots.Remove($key)
            return $false
        }

        $outType.Value = $slot.Type
        $outPayload.Value = [System.Text.Encoding]::UTF8.GetString($bytes)
        $this.Slots.Remove($key)
        return $true
    }

}

function Start-ExtensionPipeMockJob {
    param(
        [string]$PipeName = "RawrXDExtensionPipe",
        [int]$TimeoutMs = 120000
    )

    $moduleFile = Join-Path $PSScriptRoot "IpcFramingHelper.ps1"
    $scriptBlock = {
        param($PipeName, $TimeoutMs, $ModuleFile)
        $ErrorActionPreference = "Stop"
        . $ModuleFile

        $pipe = New-Object System.IO.Pipes.NamedPipeServerStream(
            $PipeName,
            [System.IO.Pipes.PipeDirection]::InOut,
            10,
            [System.IO.Pipes.PipeTransmissionMode]::Byte,
            [System.IO.Pipes.PipeOptions]::None)

        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        while ($stopwatch.ElapsedMilliseconds -lt $TimeoutMs) {
            try {
                $pipe.WaitForConnection(500)
            } catch {
                continue
            }

            $handler = [MockIpcStreamHandler]::new($pipe)
            $reasm = [MockIpcReassembler]::new()

            while ($pipe.IsConnected -and $stopwatch.ElapsedMilliseconds -lt $TimeoutMs) {
                $handler.ReadAvailableBytes()

                $msgType = [uint16]0
                $payload = ""
                $popped = $false
                while ($true) {
                    $frame = $handler.TryExtractPhysicalFrame()
                    if ($null -eq $frame) { break }
                    $t = [uint16]0
                    $p = ""
                    if ($reasm.FeedPhysicalFrame($frame, [ref]$t, [ref]$p)) {
                        $msgType = $t
                        $payload = $p
                        $popped = $true
                        break
                    }
                }

                if ($popped) {
                    $ack = '{"type":"response_ack","status":"ok","echo":true}'
                    $handler.WriteLogicalReply($ack, 4)
                }

                Start-Sleep -Milliseconds 10
            }

            try { $pipe.Disconnect() } catch { }
        }

        $pipe.Dispose()
    }

    return Start-Job -ScriptBlock $scriptBlock -ArgumentList $PipeName, $TimeoutMs, $moduleFile
}

function Stop-ExtensionPipeMockJob {
    param($Job)
    if ($Job) {
        Stop-Job -Job $Job -ErrorAction SilentlyContinue
        Remove-Job -Job $Job -Force -ErrorAction SilentlyContinue
    }
}

