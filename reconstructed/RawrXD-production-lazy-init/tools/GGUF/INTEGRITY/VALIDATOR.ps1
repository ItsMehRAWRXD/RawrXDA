param(
    [Parameter(Mandatory=$true)]$ModelPath,
    [switch]$AggressiveChecks,
    [int]$MaxRamGB = 64
)

$ErrorActionPreference = 'Stop'

Add-Type @"
using System;
using System.IO;
using System.Runtime.InteropServices;

public class GGUFDiagnostic {
    public const uint MAGIC = 0x46554747;

    [StructLayout(LayoutKind.Sequential)]
    public struct Header {
        public uint Magic;
        public uint Version;
        public ulong TensorCount;
        public ulong KvCount;
    }

    public static unsafe (bool valid, string error) ValidateHeader(string path) {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read);
        if (stream.Length < 64) return (false, "File too small");

        Span<byte> buffer = stackalloc byte[24];
        stream.Read(buffer);
        fixed (byte* p = buffer) {
            var hdr = (Header*)p;
            if (hdr->Magic != MAGIC) return (false, $"Bad magic: 0x{hdr->Magic:X8}");
            if (hdr->Version > 3) return (false, $"Bad version: {hdr->Version}");
            if (hdr->TensorCount > 100000) return (false, "Suspicious tensor count");
            return (true, "OK");
        }
    }

    public static long GetWorkingSetMB() {
        var proc = System.Diagnostics.Process.GetCurrentProcess();
        proc.Refresh();
        return proc.PeakWorkingSet64 / (1024 * 1024);
    }
}
"@

function Test-GGUFCorruption {
    param($Path)

    $stream = [IO.File]::Open($Path, 'Open', 'Read', 'Read');
    $reader = [IO.BinaryReader]::new($stream);

    try {
        $stream.Position = 24
        $kvCount = $reader.ReadUInt64()
        $suspicious = 0

        for ($i = 0; $i -lt [Math]::Min($kvCount, 100); $i++) {
            try {
                $keyLen = $reader.ReadUInt64()
                if ($keyLen -gt 1024 -or $keyLen -eq 0) {
                    $suspicious++
                    if ($keyLen -gt 1GB) {
                        Write-Error "[CRITICAL] Corruption at KV pair $i - impossible key length $keyLen"
                        return $false
                    }
                    continue
                }

                $stream.Position += $keyLen
                $type = $reader.ReadUInt32()

                switch ($type) {
                    8 {
                        $valLen = $reader.ReadUInt64()
                        if ($valLen -gt 100MB) {
                            Write-Host "    [WARN] Large string at pair $i ($valLen bytes)"
                            $stream.Position += $valLen
                        } else {
                            $stream.Position += $valLen
                        }
                    }
                    9 {
                        $arrType = $reader.ReadUInt32()
                        $arrCount = $reader.ReadUInt64()
                        if ($arrCount -gt 10MB) {
                            Write-Host "    [WARN] Large array ($arrCount elements)"
                        }
                        $elemSize = if ($arrType -eq 8) { 8 } else { 4 }
                        $stream.Position += $arrCount * $elemSize
                    }
                    default {
                        $skip = switch ($type) {
                            4 { 4 }
                            5 { 4 }
                            6 { 4 }
                            7 { 1 }
                            10 { 8 }
                            default { 4 }
                        }
                        $stream.Position += $skip
                    }
                }
            } catch {
                Write-Error "[FAIL] Corruption at pair $i : $_"
                return $false
            }
        }
        Write-Host "    [OK] String table scan passed ($suspicious anomalies)"
        return $true
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

function Measure-GGUFMemoryPressure {
    param($Path, $MaxRamMB = ($MaxRamGB * 1024))

    $stats = [Diagnostics.Process]::GetCurrentProcess()
    $stats.Refresh()
    $fileMB = (Get-Item $Path).Length / 1MB
    $available = (Get-CimInstance Win32_PhysicalMemory | Measure-Object Capacity -Sum).Sum / 1MB
    $commit = [GGUFDiagnostic]::GetWorkingSetMB()

    Write-Host "    File: $([math]::Round($fileMB,0)) MB"
    Write-Host "    System RAM: $([math]::Round($available,0)) MB"
    Write-Host "    Current Commit: $([math]::Round($commit,0)) MB"

    if ($fileMB -gt ($MaxRamMB * 0.8)) {
        Write-Warning "File exceeds 80% of max RAM - lazy loading required"
        return $false
    }
    if ($fileMB -gt ($available * 0.7)) {
        Write-Warning "File exceeds 70% of physical RAM - expect paging"
        return $false
    }
    return $true
}

Write-Host "`n=== RAWRXD GGUF VALIDATOR ===" -ForegroundColor Cyan
$result = [PSCustomObject]@{
    File = $ModelPath
    HeaderValid = $false
    CorruptionFree = $false
    MemorySafe = $false
    SafeToLoad = $false
}

$result.HeaderValid = ([GGUFDiagnostic]::ValidateHeader($ModelPath)).valid
$result.CorruptionFree = Test-GGUFCorruption -Path $ModelPath
$result.MemorySafe = Measure-GGUFMemoryPressure -Path $ModelPath
$result.SafeToLoad = $result.HeaderValid -and $result.CorruptionFree -and $result.MemorySafe

if ($result.SafeToLoad) {
    Write-Host "`n[PASS] Model safe for RawrXD loading" -ForegroundColor Green
} else {
    Write-Host "`n[FAIL] Load aborted - fix corruption or enable lazy paging" -ForegroundColor Red
}

$result
