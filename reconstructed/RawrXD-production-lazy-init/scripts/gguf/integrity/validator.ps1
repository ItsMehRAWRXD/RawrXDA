param(
    [Parameter(Mandatory=$true)]$ModelPath,
    [switch]$AggressiveChecks,
    [int]$MaxRamGB = 64
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.IO;
using System.Runtime.InteropServices;

public class GGUFDiagnostic {
    public const uint MAGIC = 0x46554747;
    
    [StructLayout(LayoutKind.Sequential)]
    public struct GGUFHeader {
        public uint Magic;
        public uint Version;
        public ulong TensorCount;
        public ulong KvCount;
    }
    
    public static unsafe (bool valid, string error) ValidateHeader(string path) {
        using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read)) {
            if (fs.Length < 64) return (false, "File too small");
            
            byte[] buffer = new byte[24];
            fs.Read(buffer, 0, 24);
            
            fixed (byte* p = buffer) {
                var hdr = (GGUFHeader*)p;
                if (hdr->Magic != MAGIC) return (false, $"Bad magic: 0x{hdr->Magic:X8}");
                if (hdr->Version > 3) return (false, $"Bad version: {hdr->Version}");
                if (hdr->TensorCount > 100000) return (false, "Suspicious tensor count");
                
                return (true, "OK");
            }
        }
    }
    
    public static long GetPeakWorkingSet() {
        var proc = System.Diagnostics.Process.GetCurrentProcess();
        proc.Refresh();
        return proc.PeakWorkingSet64 / (1024 * 1024); // MB
    }
}
"@

function Test-GGUFCorruption {
    param($Path)
    
    $file = Get-Item $Path
    Write-Host "[+] Scanning: $($file.Name) ($([math]::Round($file.Length/1GB,2)) GB)"
    
    # Binary header check
    $result = [GGUFDiagnostic]::ValidateHeader($Path)
    if (-not $result.valid) {
        Write-Error "[FAIL] Header validation: $($result.error)"
        return $false
    }
    Write-Host "    [OK] Header integrity verified"
    
    # String table corruption scan (fast)
    $stream = [FileStream]::new($Path, [FileMode]::Open, [FileAccess]::Read, [FileShare]::Read, 8192, [FileOptions]::SequentialScan)
    $reader = [BinaryReader]::new($stream)
    
    try {
        $stream.Position = 24 # Skip header
        
        $kvCount = $reader.ReadUInt64()
        $suspiciousStrings = 0
        
        for ($i = 0; $i -lt [Math]::Min($kvCount, 100); $i++) {
            try {
                # Read key length and key
                $keyLen = $reader.ReadUInt64()
                if ($keyLen -gt 1024 -or $keyLen -eq 0) {
                    $suspiciousStrings++
                    if ($keyLen -gt 1GB) { 
                        Write-Error "[CRITICAL] Corruption detected at KV pair $i - impossible key length $keyLen"
                        return $false
                    }
                    continue
                }
                
                $stream.Position += $keyLen # Skip key bytes
                
                $type = $reader.ReadUInt32()
                if ($type -eq 8) { # String value
                    $valLen = $reader.ReadUInt64()
                    if ($valLen -gt 100MB) {
                        Write-Host "    [WARN] Large string detected at pair $i (${valLen} bytes) - possible chat_template bloat"
                        $stream.Position += $valLen
                    } else {
                        $stream.Position += $valLen
                    }
                } elseif ($type -eq 9) { # Array
                    $arrType = $reader.ReadUInt32()
                    $arrCount = $reader.ReadUInt64()
                    if ($arrCount -gt 10MB) {
                        Write-Host "    [WARN] Large array detected ($arrCount elements)"
                    }
                    # Rough skip estimate
                    $elemSize = if ($arrType -eq 8) { 8 } else { 4 }
                    $stream.Position += $arrCount * $elemSize
                } else {
                    # Skip scalar based on type
                    $skip = switch ($type) {
                        4 { 4 } # UINT32
                        5 { 4 } # INT32
                        6 { 4 } # FLOAT32
                        7 { 1 } # BOOL
                        10 { 8 } # UINT64
                        default { 4 }
                    }
                    $stream.Position += $skip
                }
                
            } catch {
                Write-Error "[FAIL] Corruption detected at pair $i : $_"
                return $false
            }
        }
        
        Write-Host "    [OK] String table scan passed ($suspiciousStrings anomalies)"
        return $true
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

function Measure-GGUFMemoryPressure {
    param($Path, $MaxRamMB = ($MaxRamGB * 1024))
    
    Write-Host "[+] Memory pressure analysis..."
    
    $fileSizeMB = (Get-Item $Path).Length / 1MB
    $available = (Get-CimInstance Win32_PhysicalMemory | Measure-Object Capacity -Sum).Sum / 1MB
    $committed = [GGUFDiagnostic]::GetPeakWorkingSet()
    
    Write-Host "    File: $([math]::Round($fileSizeMB,0)) MB"
    Write-Host "    System RAM: $([math]::Round($available,0)) MB"
    Write-Host "    Current Commit: $([math]::Round($committed,0)) MB"
    
    if ($fileSizeMB -gt ($MaxRamMB * 0.8)) {
        Write-Warning "File exceeds 80% of max RAM - lazy loading mandatory"
        return $false
    }
    
    if ($fileSizeMB -gt ($available * 0.7)) {
        Write-Warning "File exceeds 70% of physical RAM - expect paging"
        return $false
    }
    
    return $true
}

# Execution
$Result = New-Object PSObject -Property @{
    File = $ModelPath
    HeaderValid = $false
    CorruptionFree = $false
    MemorySafe = $false
    SafeToLoad = $false
}

Write-Host "`n=== RAWRXD GGUF VALIDATOR ===" -ForegroundColor Cyan

$Result.HeaderValid = ([GGUFDiagnostic]::ValidateHeader($ModelPath)).valid
$Result.CorruptionFree = Test-GGUFCorruption -Path $ModelPath
$Result.MemorySafe = Measure-GGUFMemoryPressure -Path $ModelPath

$Result.SafeToLoad = $Result.HeaderValid -and $Result.CorruptionFree -and $Result.MemorySafe

if ($Result.SafeToLoad) {
    Write-Host "`n[PASS] Model safe for RawrXD loading" -ForegroundColor Green
} else {
    Write-Host "`n[FAIL] Load aborted - fix corruption or enable lazy paging" -ForegroundColor Red
}

$Result
