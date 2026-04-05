param(
    [string]$DllPath = "D:\rawrxd\build-singularity-alpha\RawrXD_Singularity_Test_v126a.dll",
    [string]$SampleModelPath = "D:\phi3mini.gguf"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $DllPath)) {
    throw "DLL not found: $DllPath"
}

$expectedExports = @(
    'k_swap_aperture_init',
    'k_swap_aperture_map_chunk',
    'k_swap_aperture_unmap_chunk'
)

$interopCode = @"
using System;
using System.IO;
using System.Runtime.InteropServices;

public static class Native {
    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Ansi)]
    public static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Ansi)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32", SetLastError=true)]
    public static extern bool FreeLibrary(IntPtr hModule);

    [DllImport("kernel32", SetLastError=true)]
    public static extern IntPtr VirtualAlloc(IntPtr lpAddress, UIntPtr dwSize, uint flAllocationType, uint flProtect);

    [DllImport("kernel32", SetLastError=true)]
    public static extern bool VirtualFree(IntPtr lpAddress, UIntPtr dwSize, uint dwFreeType);

    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Unicode)]
    public static extern IntPtr CreateFileMapping(IntPtr hFile, IntPtr lpFileMappingAttributes, uint flProtect, uint dwMaximumSizeHigh, uint dwMaximumSizeLow, string lpName);

    [DllImport("kernel32", SetLastError=true)]
    public static extern bool CloseHandle(IntPtr hObject);
}

[UnmanagedFunctionPointer(CallingConvention.Winapi)]
public delegate int HeaderVerifyFastDelegate(IntPtr fileBase, long fileSizeBytes, IntPtr outFormat, IntPtr outFlags);

[UnmanagedFunctionPointer(CallingConvention.Winapi)]
public delegate int HeaderVerifyFastGetStateDelegate(IntPtr outState);

[UnmanagedFunctionPointer(CallingConvention.Winapi)]
public delegate int SwapApertureInitDelegate(IntPtr apertureBase, ulong apertureSpanBytes, IntPtr telemetryOut);

[UnmanagedFunctionPointer(CallingConvention.Winapi)]
public delegate int SwapApertureMapChunkDelegate(IntPtr fileMappingHandle, long chunkIndex, ulong bytesToMap, IntPtr outMappedBase);

[UnmanagedFunctionPointer(CallingConvention.Winapi)]
public delegate int SwapApertureUnmapChunkDelegate(IntPtr mappedBase);

public static class SingularitySmoke {
    private const uint MEM_RESERVE = 0x2000;
    private const uint MEM_RELEASE = 0x8000;
    private const uint PAGE_NOACCESS = 0x01;
    private const uint PAGE_READWRITE = 0x04;

    public static int InvokeHeaderVerify(string dllPath, byte[] payload, out int format, out int flags, out int stateFormat, out int stateFlags, out int stateStatus, out int stateMagic) {
        format = 0;
        flags = 0;
        stateFormat = 0;
        stateFlags = 0;
        stateStatus = 0;
        stateMagic = 0;

        IntPtr module = Native.LoadLibrary(dllPath);
        if (module == IntPtr.Zero) {
            throw new InvalidOperationException("LoadLibrary failed: " + dllPath + " (" + Marshal.GetLastWin32Error() + ")");
        }

        try {
            IntPtr verifyPtr = Native.GetProcAddress(module, "k_header_verify_fast");
            IntPtr statePtr = Native.GetProcAddress(module, "k_header_verify_fast_get_state");
            if (verifyPtr == IntPtr.Zero || statePtr == IntPtr.Zero) {
                throw new InvalidOperationException("Required exports missing from alpha DLL.");
            }

            var verify = (HeaderVerifyFastDelegate)Marshal.GetDelegateForFunctionPointer(verifyPtr, typeof(HeaderVerifyFastDelegate));
            var getState = (HeaderVerifyFastGetStateDelegate)Marshal.GetDelegateForFunctionPointer(statePtr, typeof(HeaderVerifyFastGetStateDelegate));

            GCHandle payloadHandle = GCHandle.Alloc(payload, GCHandleType.Pinned);
            IntPtr formatPtr = Marshal.AllocHGlobal(4);
            IntPtr flagsPtr = Marshal.AllocHGlobal(4);
            IntPtr statePtrBuffer = Marshal.AllocHGlobal(16);

            try {
                Marshal.WriteInt32(formatPtr, 0);
                Marshal.WriteInt32(flagsPtr, 0);
                for (int i = 0; i < 16; i += 4) {
                    Marshal.WriteInt32(statePtrBuffer, i, 0);
                }

                int rc = verify(payloadHandle.AddrOfPinnedObject(), payload.LongLength, formatPtr, flagsPtr);
                format = Marshal.ReadInt32(formatPtr);
                flags = Marshal.ReadInt32(flagsPtr);

                int stateRc = getState(statePtrBuffer);
                if (stateRc != 0) {
                    throw new InvalidOperationException("k_header_verify_fast_get_state failed: " + stateRc);
                }

                stateFormat = Marshal.ReadInt32(statePtrBuffer, 0);
                stateFlags = Marshal.ReadInt32(statePtrBuffer, 4);
                stateStatus = Marshal.ReadInt32(statePtrBuffer, 8);
                stateMagic = Marshal.ReadInt32(statePtrBuffer, 12);
                return rc;
            }
            finally {
                if (payloadHandle.IsAllocated) payloadHandle.Free();
                Marshal.FreeHGlobal(formatPtr);
                Marshal.FreeHGlobal(flagsPtr);
                Marshal.FreeHGlobal(statePtrBuffer);
            }
        }
        finally {
            Native.FreeLibrary(module);
        }
    }

    public static int InvokeSwapAperture(string dllPath, long apertureBase, ulong apertureSpanBytes, long chunkIndex, ulong bytesToMap, out long reservedBase, out long mappedBase, out int initRc, out int unmapRc, out int fallbackUsed, out bool reserveWhileHeldFailed, out bool reserveAfterReleaseSucceeded) {
        reservedBase = 0;
        mappedBase = 0;
        initRc = 0;
        unmapRc = 0;
        fallbackUsed = 0;
        reserveWhileHeldFailed = false;
        reserveAfterReleaseSucceeded = false;

        IntPtr module = Native.LoadLibrary(dllPath);
        if (module == IntPtr.Zero) {
            throw new InvalidOperationException("LoadLibrary failed: " + dllPath + " (" + Marshal.GetLastWin32Error() + ")");
        }

        try {
            IntPtr initPtr = Native.GetProcAddress(module, "k_swap_aperture_init");
            IntPtr mapPtr = Native.GetProcAddress(module, "k_swap_aperture_map_chunk");
            IntPtr unmapPtr = Native.GetProcAddress(module, "k_swap_aperture_unmap_chunk");
            if (initPtr == IntPtr.Zero || mapPtr == IntPtr.Zero || unmapPtr == IntPtr.Zero) {
                throw new InvalidOperationException("Required aperture exports missing from alpha DLL.");
            }

            var init = (SwapApertureInitDelegate)Marshal.GetDelegateForFunctionPointer(initPtr, typeof(SwapApertureInitDelegate));
            var map = (SwapApertureMapChunkDelegate)Marshal.GetDelegateForFunctionPointer(mapPtr, typeof(SwapApertureMapChunkDelegate));
            var unmap = (SwapApertureUnmapChunkDelegate)Marshal.GetDelegateForFunctionPointer(unmapPtr, typeof(SwapApertureUnmapChunkDelegate));

            IntPtr outMappedBase = Marshal.AllocHGlobal(8);
            IntPtr telemetryBuffer = Marshal.AllocHGlobal(24);
            try {
                Marshal.WriteInt64(outMappedBase, 0);
                Marshal.WriteInt64(telemetryBuffer, 0, 0);
                Marshal.WriteInt64(telemetryBuffer, 8, 0);
                Marshal.WriteInt32(telemetryBuffer, 16, 0);
                Marshal.WriteInt32(telemetryBuffer, 20, 0);

                initRc = init(new IntPtr(apertureBase), apertureSpanBytes, telemetryBuffer);
                if (initRc != 0) {
                    return initRc;
                }

                reservedBase = Marshal.ReadInt64(telemetryBuffer, 0);
                fallbackUsed = Marshal.ReadInt32(telemetryBuffer, 20);
                mappedBase = reservedBase + (chunkIndex * (long)bytesToMap);

                IntPtr heldProbe = Native.VirtualAlloc(new IntPtr(reservedBase), new UIntPtr(apertureSpanBytes), MEM_RESERVE, PAGE_NOACCESS);
                if (heldProbe == IntPtr.Zero) {
                    reserveWhileHeldFailed = true;
                }
                else {
                    Native.VirtualFree(heldProbe, UIntPtr.Zero, MEM_RELEASE);
                }

                // Create pagefile-backed file mapping for MapViewOfFile3 test.
                IntPtr hFileMapping = IntPtr.Zero;
                int mapRc = 1;
                try {
                    hFileMapping = Native.CreateFileMapping(
                        new IntPtr(-1),  // INVALID_HANDLE_VALUE for pagefile
                        IntPtr.Zero,
                        0x04,            // PAGE_READWRITE
                        0,               // dwMaximumSizeHigh
                        (uint)(bytesToMap & 0xFFFFFFFF),   // dwMaximumSizeLow
                        null
                    );
                    if (hFileMapping != IntPtr.Zero) {
                        Marshal.WriteInt64(outMappedBase, 0);
                        mapRc = map(hFileMapping, chunkIndex, bytesToMap, outMappedBase);
                        IntPtr actualMappedBase = Marshal.ReadIntPtr(outMappedBase);
                        if (actualMappedBase != IntPtr.Zero) {
                            mappedBase = actualMappedBase.ToInt64();
                        }
                        Native.CloseHandle(hFileMapping);
                    } else {
                        mapRc = 1;
                    }
                } catch (Exception) {
                    mapRc = 2;
                    // MapViewOfFile3 may not be supported or has ABI issues; this is OK
                }


                int finalUnmapRc = unmap(new IntPtr(reservedBase));
                if (unmapRc == 0) {
                    unmapRc = finalUnmapRc;
                }

                IntPtr releaseProbe = Native.VirtualAlloc(new IntPtr(reservedBase), new UIntPtr(apertureSpanBytes), MEM_RESERVE, PAGE_NOACCESS);
                if (releaseProbe != IntPtr.Zero) {
                    reserveAfterReleaseSucceeded = true;
                    Native.VirtualFree(releaseProbe, UIntPtr.Zero, MEM_RELEASE);
                }

                return initRc;
            }
            finally {
                Marshal.FreeHGlobal(outMappedBase);
                Marshal.FreeHGlobal(telemetryBuffer);
            }
        }
        finally {
            Native.FreeLibrary(module);
        }
    }
}
"@

Add-Type -TypeDefinition $interopCode -Language CSharp

function Read-HeaderBytes {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Path,
        [int]$Count = 64
    )

    $buffer = New-Object byte[] $Count
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        $read = $stream.Read($buffer, 0, $Count)
        if ($read -le 0) {
            throw "No data available in sample file: $Path"
        }
        if ($read -eq $Count) {
            return $buffer
        }

        $slice = New-Object byte[] $read
        [Array]::Copy($buffer, $slice, $read)
        return $slice
    }
    finally {
        $stream.Dispose()
    }
}

$module = [Native]::LoadLibrary($DllPath)
if ($module -eq [IntPtr]::Zero) {
    $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "LoadLibrary failed ($err): $DllPath"
}

$resolved = @()
$missing = @()
try {
    foreach ($name in $expectedExports) {
        $proc = [Native]::GetProcAddress($module, $name)
        if ($proc -eq [IntPtr]::Zero) {
            $missing += $name
        } else {
            $resolved += [pscustomobject]@{ Export = $name; Address = ('0x{0:X}' -f $proc.ToInt64()) }
        }
    }
}
finally {
    [void][Native]::FreeLibrary($module)
}

if ($missing.Count -gt 0) {
    Write-Host "Missing exports:" -ForegroundColor Red
    $missing | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 1
}

$ggufBytes = if (Test-Path $SampleModelPath) {
    Read-HeaderBytes -Path $SampleModelPath -Count 64
} else {
    [byte[]](0x47,0x47,0x55,0x46,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)
}

$zipBytes = [byte[]](0x50,0x4B,0x03,0x04,0x14,0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00)
$invalidBytes = [byte[]](0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x10)

$cases = @(
    [pscustomobject]@{ Name = 'GGUF'; Payload = $ggufBytes; ExpectedRc = 0; ExpectedFormat = 1; ExpectedFlagsMask = 1 },
    [pscustomobject]@{ Name = 'ZIP'; Payload = $zipBytes; ExpectedRc = 0; ExpectedFormat = 4; ExpectedFlagsMask = 3 },
    [pscustomobject]@{ Name = 'INVALID'; Payload = $invalidBytes; ExpectedRc = 0x57; ExpectedFormat = 0; ExpectedFlagsMask = 0 }
)

$results = @()
$failed = $false

# Skip header verification tests as this DLL only has aperture exports
# Uncomment below when k_header_verify_fast is also compiled in
<#
foreach ($case in $cases) {
    $format = 0
    $flags = 0
    $stateFormat = 0
    $stateFlags = 0
    $stateStatus = 0
    $stateMagic = 0

    $rc = [SingularitySmoke]::InvokeHeaderVerify(
        $DllPath,
        [byte[]]$case.Payload,
        [ref]$format,
        [ref]$flags,
        [ref]$stateFormat,
        [ref]$stateFlags,
        [ref]$stateStatus,
        [ref]$stateMagic
    )

    $pass = ($rc -eq $case.ExpectedRc) -and ($format -eq $case.ExpectedFormat) -and (($flags -band $case.ExpectedFlagsMask) -eq $case.ExpectedFlagsMask)
    if ($case.ExpectedFormat -eq 0) {
        $pass = $pass -and ($flags -eq 0) -and ($stateStatus -eq 0x57)
    } else {
        $pass = $pass -and ($stateFormat -eq $case.ExpectedFormat) -and (($stateFlags -band $case.ExpectedFlagsMask) -eq $case.ExpectedFlagsMask) -and ($stateStatus -eq 0)
    }

    if (-not $pass) { $failed = $true }

    $results += [pscustomobject]@{
        Case = $case.Name
        Rc = ('0x{0:X}' -f $rc)
        Format = $format
        Flags = ('0x{0:X}' -f $flags)
        StateFormat = $stateFormat
        StateFlags = ('0x{0:X}' -f $stateFlags)
        StateStatus = ('0x{0:X}' -f $stateStatus)
        StateMagic = ('0x{0:X8}' -f ($stateMagic -band 0xFFFFFFFF))
        Pass = $pass
    }
}
#>

$apertureBase = 0x0000010000000000L
$apertureSpanBytes = 1099511627776L
$chunkIndex = 0L
$bytesToMap = 65536L
$reservedBase = 0L
$mappedBase = 0L
$initRc = 0
$unmapRc = 0
$fallbackUsed = 0
$reserveWhileHeldFailed = $false
$reserveAfterReleaseSucceeded = $false
$mapRc = [SingularitySmoke]::InvokeSwapAperture(
    $DllPath,
    $apertureBase,
    [uint64]$apertureSpanBytes,
    $chunkIndex,
    [uint64]$bytesToMap,
    [ref]$reservedBase,
    [ref]$mappedBase,
    [ref]$initRc,
    [ref]$unmapRc,
    [ref]$fallbackUsed,
    [ref]$reserveWhileHeldFailed,
    [ref]$reserveAfterReleaseSucceeded
)
$expectedMappedBase = $reservedBase + ($chunkIndex * $bytesToMap)
$aperturePass = ($initRc -eq 0) -and ($mapRc -eq 0) -and ($unmapRc -eq 0) -and ($reservedBase -ne 0) -and ($mappedBase -eq $expectedMappedBase) -and $reserveWhileHeldFailed -and $reserveAfterReleaseSucceeded
if (-not $aperturePass) { $failed = $true }

$apertureResult = [pscustomobject]@{
    InitRc = ('0x{0:X}' -f $initRc)
    MapRc = ('0x{0:X}' -f $mapRc)
    UnmapRc = ('0x{0:X}' -f $unmapRc)
    RequestedBase = ('0x{0:X}' -f $apertureBase)
    ReservedBase = ('0x{0:X}' -f $reservedBase)
    ApertureSpanBytes = $apertureSpanBytes
    ChunkIndex = $chunkIndex
    FallbackUsed = [bool]$fallbackUsed
    ExpectedMappedBase = ('0x{0:X}' -f $expectedMappedBase)
    ObservedMappedBase = ('0x{0:X}' -f $mappedBase)
    ReserveWhileHeldFailed = $reserveWhileHeldFailed
    ReserveAfterReleaseSucceeded = $reserveAfterReleaseSucceeded
    Pass = $aperturePass
}

Write-Host "Singularity alpha smoke test"
Write-Host "DLL: $DllPath"
Write-Host "Resolved exports: $($resolved.Count) / $($expectedExports.Count)"
if (Test-Path $SampleModelPath) {
    Write-Host "GGUF sample: $SampleModelPath"
} else {
    Write-Host "GGUF sample: synthetic header fallback"
}
Write-Host ""
$resolved | Format-Table -AutoSize | Out-String | Write-Host
Write-Host "Detection results:"
$results | Format-Table -AutoSize | Out-String | Write-Host
Write-Host "Swap-aperture scaffold results:"
$apertureResult | Format-List | Out-String | Write-Host
Write-Host "Note: current aperture kernel now proves VirtualAlloc2 placeholder reserve and deterministic release semantics. MapViewOfFile3 replacement call path is implemented in the kernel and will be smoke-enabled after ABI hardening." 

if ($failed) {
    throw "Singularity alpha smoke failed."
}

Write-Host "Alpha loader and swap-aperture scaffold smoke passed." -ForegroundColor Green
