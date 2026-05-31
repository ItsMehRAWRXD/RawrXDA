param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [int]$BlockSize = 1048576
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $InputPath)) {
    throw "Input file does not exist: $InputPath"
}

$inputFile = Get-Item -LiteralPath $InputPath
if ($inputFile.Length -le 0) {
    throw "Input file is empty: $InputPath"
}

if ($BlockSize -lt 4096 -or $BlockSize -gt 67108864) {
    throw "BlockSize must be in [4096, 67108864]"
}

$magic = 0x21584152 # "RXA!"
$version = 0x00010000
$flags = 0
$uncompressedSize = [UInt64]$inputFile.Length
$blockCount = [UInt32][Math]::Ceiling($inputFile.Length / [double]$BlockSize)

$headerSize = 32
$entrySize = 24
$indexSize = [UInt64]$blockCount * [UInt64]$entrySize
$dataOffset = [UInt64]$headerSize + $indexSize

$inStream = [System.IO.File]::Open($InputPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
try {
    $outDir = Split-Path -Parent $OutputPath
    if ($outDir -and -not (Test-Path -LiteralPath $outDir)) {
        New-Item -ItemType Directory -Path $outDir | Out-Null
    }

    $outStream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try {
        $writer = New-Object System.IO.BinaryWriter($outStream)

        # Header
        $writer.Write([UInt32]$magic)
        $writer.Write([UInt32]$version)
        $writer.Write([UInt32]$flags)
        $writer.Write([UInt32]$BlockSize)
        $writer.Write([UInt64]$uncompressedSize)
        $writer.Write([UInt32]$blockCount)
        $writer.Write([UInt32]0)

        # Build and write index entries first
        $entries = New-Object 'System.Collections.Generic.List[object]'
        $cursor = $dataOffset
        $remaining = [UInt64]$uncompressedSize
        for ($i = 0; $i -lt $blockCount; $i++) {
            $chunk = [UInt32][Math]::Min([UInt64]$BlockSize, $remaining)
            $entry = [PSCustomObject]@{
                Offset = [UInt64]$cursor
                CompressedSize = [UInt32]$chunk
                UncompressedSize = [UInt32]$chunk
                Crc32C = [UInt32]0
                Algorithm = [Byte]0
            }
            $entries.Add($entry)
            $cursor += [UInt64]$chunk
            $remaining -= [UInt64]$chunk
        }

        foreach ($entry in $entries) {
            $writer.Write([UInt64]$entry.Offset)
            $writer.Write([UInt32]$entry.CompressedSize)
            $writer.Write([UInt32]$entry.UncompressedSize)
            $writer.Write([UInt32]$entry.Crc32C)
            $writer.Write([Byte]$entry.Algorithm)
            $writer.Write([Byte]0)
            $writer.Write([Byte]0)
            $writer.Write([Byte]0)
        }

        # Stream payload blocks (raw passthrough for minimal overhead)
        $buffer = New-Object byte[] $BlockSize
        $totalWritten = [UInt64]0
        while ($true) {
            $read = $inStream.Read($buffer, 0, $buffer.Length)
            if ($read -le 0) {
                break
            }
            $outStream.Write($buffer, 0, $read)
            $totalWritten += [UInt64]$read
        }

        if ($totalWritten -ne $uncompressedSize) {
            throw "Payload size mismatch: wrote $totalWritten bytes, expected $uncompressedSize"
        }

        $writer.Flush()
    }
    finally {
        $outStream.Dispose()
    }
}
finally {
    $inStream.Dispose()
}

Write-Host "RXA archive created: $OutputPath"
Write-Host "Input bytes:  $uncompressedSize"
Write-Host "Block size:   $BlockSize"
Write-Host "Block count:  $blockCount"
