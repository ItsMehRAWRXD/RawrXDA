param(
    [string]$DllPath = "D:\rawrxd\build-win32\bin\RawrXD_Titan.dll",
    [string]$SampleModelPath = "D:\phi3mini.gguf",
    [uint32]$ChunkIndex = 0,
    [string[]]$ChunkIndices = @(),
    [string[]]$FragmentationPattern = @(),
    [switch]$VerifyExports,
    [string]$DefPath = "D:\rawrxd\src\titan_infer.def"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $DllPath)) {
    throw "DLL not found: $DllPath"
}

if ($VerifyExports -and -not (Test-Path $DefPath)) {
    throw "Definition file not found: $DefPath"
}

$resolvedModelPath = $null
if ($SampleModelPath -and (Test-Path $SampleModelPath)) {
    $resolvedModelPath = (Resolve-Path $SampleModelPath).Path
}

$resolvedChunkIndices = @()
foreach ($rawChunkValue in $ChunkIndices) {
    foreach ($token in ($rawChunkValue -split '[,\s]+' | Where-Object { $_ -ne '' })) {
        $resolvedChunkIndices += [uint32]::Parse($token)
    }
}
if ($resolvedChunkIndices.Count -eq 0) {
    $resolvedChunkIndices = @([uint32]$ChunkIndex)
}

$resolvedFragmentationPattern = @()
foreach ($rawChunkValue in $FragmentationPattern) {
    foreach ($token in ($rawChunkValue -split '[,\s]+' | Where-Object { $_ -ne '' })) {
        $resolvedFragmentationPattern += [uint32]::Parse($token)
    }
}
if ($resolvedFragmentationPattern.Count -eq 0 -and $resolvedChunkIndices.Count -ge 2) {
    $resolvedFragmentationPattern = @($resolvedChunkIndices[0], $resolvedChunkIndices[$resolvedChunkIndices.Count - 1])
}

$interopCode = @"
using System;
using System.Runtime.InteropServices;

public static class TitanApertureNative {
    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Ansi)]
    public static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Ansi)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32", SetLastError=true)]
    public static extern bool FreeLibrary(IntPtr hModule);
}

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDInitializeDelegate();

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDShutdownDelegate();

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDLoadModelDelegate([MarshalAs(UnmanagedType.LPStr)] string modelPath, out ulong modelHandle);

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDUnloadModelDelegate(ulong modelHandle);

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDApertureInitDelegate();

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDMapChunkDelegate(uint chunkIndex, out ulong mappedAddress);

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDUnmapChunkDelegate(uint chunkIndex);

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDGetApertureBaseDelegate(out ulong baseAddress);

[StructLayout(LayoutKind.Sequential)]
public struct RawrXDApertureStatus {
    public ulong aperture_base;
    public ulong aperture_total_bytes;
    public ulong mapped_bytes;
    public ulong unmapped_bytes;
    public uint mapped_chunks;
    public uint fragment_count;
    public float fragmentation_ratio;
}

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDGetApertureUtilizationDelegate(out RawrXDApertureStatus status);

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDGetChunkStatusDelegate(ulong modelHandle, uint chunkIndex, out uint isMapped);

[UnmanagedFunctionPointer(CallingConvention.StdCall)]
public delegate int RawrXDGetVAFragmentationDelegate(out float fragmentation);

public static class TitanApertureSmoke {
    public const int RAWRXD_SUCCESS = 0x00000000;
    public const int RAWRXD_ERROR_NO_MODEL_LOADED = unchecked((int)0x80000004);
    public const ulong RAWRXD_APERTURE_CHUNK_BYTES = 1073741824UL;

    public sealed class Result {
        public uint ChunkIndex;
        public int InitializeRc;
        public int ApertureInitRc;
        public int LoadModelRc = int.MinValue;
        public ulong ModelHandle;
        public int MapChunkRc;
        public ulong MappedAddress;
        public ulong ExpectedMappedAddress;
        public int UnmapChunkRc = int.MinValue;
        public int UnloadModelRc = int.MinValue;
        public int ShutdownRc;
        public int GetApertureBaseBeforeMapRc;
        public ulong ApertureBaseBeforeMap;
        public int GetApertureBaseAfterMapRc;
        public ulong ApertureBaseAfterMap;
        public int GetChunkStatusAfterMapRc = int.MinValue;
        public uint ChunkMappedAfterMap;
        public int GetChunkStatusAfterUnmapRc = int.MinValue;
        public uint ChunkMappedAfterUnmap;
        public int GetUtilizationAfterMapRc = int.MinValue;
        public RawrXDApertureStatus UtilizationAfterMap;
        public int GetUtilizationAfterUnmapRc = int.MinValue;
        public RawrXDApertureStatus UtilizationAfterUnmap;
        public int GetVAFragmentationRc = int.MinValue;
        public float VAFragmentation;
        public bool SampleModelAttempted;
        public bool SampleModelLoaded;
    }

    public sealed class FragmentationResult {
        public uint[] ChunkIndices;
        public int InitializeRc;
        public int ApertureInitRc;
        public int LoadModelRc = int.MinValue;
        public ulong ModelHandle;
        public ulong ApertureBase;
        public int GetApertureBaseRc;
        public int[] MapChunkRcs;
        public ulong[] MappedAddresses;
        public ulong[] ExpectedAddresses;
        public int GetUtilizationRc = int.MinValue;
        public RawrXDApertureStatus Utilization;
        public int GetVAFragmentationRc = int.MinValue;
        public float VAFragmentation;
        public int UnmapChunkRc = int.MinValue;
        public int GetUtilizationAfterUnmapRc = int.MinValue;
        public RawrXDApertureStatus UtilizationAfterUnmap;
        public int UnloadModelRc = int.MinValue;
        public int ShutdownRc;
    }

    public static Result Invoke(string dllPath, string sampleModelPath, uint chunkIndex) {
        IntPtr module = TitanApertureNative.LoadLibrary(dllPath);
        if (module == IntPtr.Zero) {
            throw new InvalidOperationException("LoadLibrary failed: " + dllPath + " (" + Marshal.GetLastWin32Error() + ")");
        }

        try {
            IntPtr initPtr = RequireExport(module, "RawrXD_Initialize");
            IntPtr shutdownPtr = RequireExport(module, "RawrXD_Shutdown");
            IntPtr loadModelPtr = RequireExport(module, "RawrXD_LoadModel");
            IntPtr unloadModelPtr = RequireExport(module, "RawrXD_UnloadModel");
            IntPtr apertureInitPtr = RequireExport(module, "RawrXD_ApertureInit");
            IntPtr mapChunkPtr = RequireExport(module, "RawrXD_MapChunk");
            IntPtr unmapChunkPtr = RequireExport(module, "RawrXD_UnmapChunk");
            IntPtr getApertureBasePtr = RequireExport(module, "RawrXD_GetApertureBase");
            IntPtr getApertureUtilizationPtr = RequireExport(module, "RawrXD_GetApertureUtilization");
            IntPtr getChunkStatusPtr = RequireExport(module, "RawrXD_GetChunkStatus");
            IntPtr getVAFragmentationPtr = RequireExport(module, "RawrXD_GetVAFragmentation");

            var init = (RawrXDInitializeDelegate)Marshal.GetDelegateForFunctionPointer(initPtr, typeof(RawrXDInitializeDelegate));
            var shutdown = (RawrXDShutdownDelegate)Marshal.GetDelegateForFunctionPointer(shutdownPtr, typeof(RawrXDShutdownDelegate));
            var loadModel = (RawrXDLoadModelDelegate)Marshal.GetDelegateForFunctionPointer(loadModelPtr, typeof(RawrXDLoadModelDelegate));
            var unloadModel = (RawrXDUnloadModelDelegate)Marshal.GetDelegateForFunctionPointer(unloadModelPtr, typeof(RawrXDUnloadModelDelegate));
            var apertureInit = (RawrXDApertureInitDelegate)Marshal.GetDelegateForFunctionPointer(apertureInitPtr, typeof(RawrXDApertureInitDelegate));
            var mapChunk = (RawrXDMapChunkDelegate)Marshal.GetDelegateForFunctionPointer(mapChunkPtr, typeof(RawrXDMapChunkDelegate));
            var unmapChunk = (RawrXDUnmapChunkDelegate)Marshal.GetDelegateForFunctionPointer(unmapChunkPtr, typeof(RawrXDUnmapChunkDelegate));
            var getApertureBase = (RawrXDGetApertureBaseDelegate)Marshal.GetDelegateForFunctionPointer(getApertureBasePtr, typeof(RawrXDGetApertureBaseDelegate));
            var getApertureUtilization = (RawrXDGetApertureUtilizationDelegate)Marshal.GetDelegateForFunctionPointer(getApertureUtilizationPtr, typeof(RawrXDGetApertureUtilizationDelegate));
            var getChunkStatus = (RawrXDGetChunkStatusDelegate)Marshal.GetDelegateForFunctionPointer(getChunkStatusPtr, typeof(RawrXDGetChunkStatusDelegate));
            var getVAFragmentation = (RawrXDGetVAFragmentationDelegate)Marshal.GetDelegateForFunctionPointer(getVAFragmentationPtr, typeof(RawrXDGetVAFragmentationDelegate));

            var result = new Result();
            result.ChunkIndex = chunkIndex;
            result.InitializeRc = init();
            if (result.InitializeRc != RAWRXD_SUCCESS) {
                result.ShutdownRc = shutdown();
                return result;
            }

            try {
                result.ApertureInitRc = apertureInit();
                result.GetApertureBaseBeforeMapRc = getApertureBase(out result.ApertureBaseBeforeMap);

                if (!String.IsNullOrEmpty(sampleModelPath)) {
                    result.SampleModelAttempted = true;
                    result.LoadModelRc = loadModel(sampleModelPath, out result.ModelHandle);
                    result.SampleModelLoaded = (result.LoadModelRc == RAWRXD_SUCCESS);
                }

                result.MapChunkRc = mapChunk(chunkIndex, out result.MappedAddress);
                result.GetApertureBaseAfterMapRc = getApertureBase(out result.ApertureBaseAfterMap);
                result.ExpectedMappedAddress = result.ApertureBaseBeforeMap + (chunkIndex * RAWRXD_APERTURE_CHUNK_BYTES);

                if (result.SampleModelLoaded) {
                    result.GetChunkStatusAfterMapRc = getChunkStatus(result.ModelHandle, chunkIndex, out result.ChunkMappedAfterMap);
                }
                result.GetUtilizationAfterMapRc = getApertureUtilization(out result.UtilizationAfterMap);
                result.GetVAFragmentationRc = getVAFragmentation(out result.VAFragmentation);

                if (result.MapChunkRc == RAWRXD_SUCCESS) {
                    result.UnmapChunkRc = unmapChunk(chunkIndex);
                    if (result.SampleModelLoaded) {
                        result.GetChunkStatusAfterUnmapRc = getChunkStatus(result.ModelHandle, chunkIndex, out result.ChunkMappedAfterUnmap);
                    }
                    result.GetUtilizationAfterUnmapRc = getApertureUtilization(out result.UtilizationAfterUnmap);
                }

                if (result.SampleModelLoaded) {
                    result.UnloadModelRc = unloadModel(result.ModelHandle);
                }
            }
            finally {
                result.ShutdownRc = shutdown();
            }

            return result;
        }
        finally {
            TitanApertureNative.FreeLibrary(module);
        }
    }

    public static FragmentationResult InvokeFragmentation(string dllPath, string sampleModelPath, uint[] chunkIndices) {
        IntPtr module = TitanApertureNative.LoadLibrary(dllPath);
        if (module == IntPtr.Zero) {
            throw new InvalidOperationException("LoadLibrary failed: " + dllPath + " (" + Marshal.GetLastWin32Error() + ")");
        }

        try {
            IntPtr initPtr = RequireExport(module, "RawrXD_Initialize");
            IntPtr shutdownPtr = RequireExport(module, "RawrXD_Shutdown");
            IntPtr loadModelPtr = RequireExport(module, "RawrXD_LoadModel");
            IntPtr unloadModelPtr = RequireExport(module, "RawrXD_UnloadModel");
            IntPtr apertureInitPtr = RequireExport(module, "RawrXD_ApertureInit");
            IntPtr mapChunkPtr = RequireExport(module, "RawrXD_MapChunk");
            IntPtr unmapChunkPtr = RequireExport(module, "RawrXD_UnmapChunk");
            IntPtr getApertureBasePtr = RequireExport(module, "RawrXD_GetApertureBase");
            IntPtr getApertureUtilizationPtr = RequireExport(module, "RawrXD_GetApertureUtilization");
            IntPtr getVAFragmentationPtr = RequireExport(module, "RawrXD_GetVAFragmentation");

            var init = (RawrXDInitializeDelegate)Marshal.GetDelegateForFunctionPointer(initPtr, typeof(RawrXDInitializeDelegate));
            var shutdown = (RawrXDShutdownDelegate)Marshal.GetDelegateForFunctionPointer(shutdownPtr, typeof(RawrXDShutdownDelegate));
            var loadModel = (RawrXDLoadModelDelegate)Marshal.GetDelegateForFunctionPointer(loadModelPtr, typeof(RawrXDLoadModelDelegate));
            var unloadModel = (RawrXDUnloadModelDelegate)Marshal.GetDelegateForFunctionPointer(unloadModelPtr, typeof(RawrXDUnloadModelDelegate));
            var apertureInit = (RawrXDApertureInitDelegate)Marshal.GetDelegateForFunctionPointer(apertureInitPtr, typeof(RawrXDApertureInitDelegate));
            var mapChunk = (RawrXDMapChunkDelegate)Marshal.GetDelegateForFunctionPointer(mapChunkPtr, typeof(RawrXDMapChunkDelegate));
            var unmapChunk = (RawrXDUnmapChunkDelegate)Marshal.GetDelegateForFunctionPointer(unmapChunkPtr, typeof(RawrXDUnmapChunkDelegate));
            var getApertureBase = (RawrXDGetApertureBaseDelegate)Marshal.GetDelegateForFunctionPointer(getApertureBasePtr, typeof(RawrXDGetApertureBaseDelegate));
            var getApertureUtilization = (RawrXDGetApertureUtilizationDelegate)Marshal.GetDelegateForFunctionPointer(getApertureUtilizationPtr, typeof(RawrXDGetApertureUtilizationDelegate));
            var getVAFragmentation = (RawrXDGetVAFragmentationDelegate)Marshal.GetDelegateForFunctionPointer(getVAFragmentationPtr, typeof(RawrXDGetVAFragmentationDelegate));

            var result = new FragmentationResult();
            result.ChunkIndices = chunkIndices;
            result.MapChunkRcs = new int[chunkIndices.Length];
            result.MappedAddresses = new ulong[chunkIndices.Length];
            result.ExpectedAddresses = new ulong[chunkIndices.Length];
            for (int i = 0; i < result.MapChunkRcs.Length; ++i) {
                result.MapChunkRcs[i] = int.MinValue;
            }

            result.InitializeRc = init();
            if (result.InitializeRc != RAWRXD_SUCCESS) {
                result.ShutdownRc = shutdown();
                return result;
            }

            try {
                result.ApertureInitRc = apertureInit();
                result.GetApertureBaseRc = getApertureBase(out result.ApertureBase);

                if (!String.IsNullOrEmpty(sampleModelPath)) {
                    result.LoadModelRc = loadModel(sampleModelPath, out result.ModelHandle);
                }

                for (int i = 0; i < chunkIndices.Length; ++i) {
                    result.MapChunkRcs[i] = mapChunk(chunkIndices[i], out result.MappedAddresses[i]);
                    result.ExpectedAddresses[i] = result.ApertureBase + (chunkIndices[i] * RAWRXD_APERTURE_CHUNK_BYTES);
                }

                result.GetUtilizationRc = getApertureUtilization(out result.Utilization);
                result.GetVAFragmentationRc = getVAFragmentation(out result.VAFragmentation);

                if (chunkIndices.Length > 0) {
                    result.UnmapChunkRc = unmapChunk(chunkIndices[chunkIndices.Length - 1]);
                    result.GetUtilizationAfterUnmapRc = getApertureUtilization(out result.UtilizationAfterUnmap);
                }

                if (result.LoadModelRc == RAWRXD_SUCCESS) {
                    result.UnloadModelRc = unloadModel(result.ModelHandle);
                }
            }
            finally {
                result.ShutdownRc = shutdown();
            }

            return result;
        }
        finally {
            TitanApertureNative.FreeLibrary(module);
        }
    }

    private static IntPtr RequireExport(IntPtr module, string name) {
        IntPtr proc = TitanApertureNative.GetProcAddress(module, name);
        if (proc == IntPtr.Zero) {
            throw new InvalidOperationException("Missing export: " + name);
        }
        return proc;
    }
}
"@

Add-Type -TypeDefinition $interopCode -Language CSharp

$sampleModelDisplay = '<not used>'
if (-not [string]::IsNullOrEmpty($resolvedModelPath)) {
    $sampleModelDisplay = $resolvedModelPath
}

$statusNames = @{
    0x00000000 = 'RAWRXD_SUCCESS'
    -2147483644 = 'RAWRXD_ERROR_NO_MODEL_LOADED'
}

function Format-Status([int]$Value) {
    $hex = ('0x{0:X8}' -f ([uint32]$Value))
    $name = $statusNames[$Value]
    if ($name) {
        return "$name ($hex)"
    }
    return $hex
}

function Write-ChunkResult($result) {
    Write-Host "Chunk index:          $($result.ChunkIndex)"
    Write-Host "Initialize:           $(Format-Status $result.InitializeRc)"
    Write-Host "ApertureInit:         $(Format-Status $result.ApertureInitRc)"
    Write-Host "GetBase before map:   $(Format-Status $result.GetApertureBaseBeforeMapRc) / $('0x{0:X}' -f $result.ApertureBaseBeforeMap)"

    if ($result.SampleModelAttempted) {
        Write-Host "LoadModel:            $(Format-Status $result.LoadModelRc) / Handle=$($result.ModelHandle)"
    } else {
        Write-Host "LoadModel:            skipped"
    }

    Write-Host "MapChunk:             $(Format-Status $result.MapChunkRc) / Address=$('0x{0:X}' -f $result.MappedAddress)"
    Write-Host "Expected address:     $('0x{0:X}' -f $result.ExpectedMappedAddress)"
    Write-Host "GetBase after map:    $(Format-Status $result.GetApertureBaseAfterMapRc) / $('0x{0:X}' -f $result.ApertureBaseAfterMap)"

    if ($result.GetChunkStatusAfterMapRc -ne [int]::MinValue) {
        Write-Host "Chunk status mapped:  $(Format-Status $result.GetChunkStatusAfterMapRc) / IsMapped=$($result.ChunkMappedAfterMap)"
    }
    if ($result.GetUtilizationAfterMapRc -ne [int]::MinValue) {
        Write-Host "Util after map:       $(Format-Status $result.GetUtilizationAfterMapRc) / Chunks=$($result.UtilizationAfterMap.mapped_chunks) Bytes=$($result.UtilizationAfterMap.mapped_bytes) Gaps=$($result.UtilizationAfterMap.fragment_count)"
    }
    if ($result.GetVAFragmentationRc -ne [int]::MinValue) {
        Write-Host "VA fragmentation:     $(Format-Status $result.GetVAFragmentationRc) / Value=$($result.VAFragmentation)"
    }

    if ($result.UnmapChunkRc -ne [int]::MinValue) {
        Write-Host "UnmapChunk:           $(Format-Status $result.UnmapChunkRc)"
    } else {
        Write-Host "UnmapChunk:           skipped"
    }
    if ($result.GetChunkStatusAfterUnmapRc -ne [int]::MinValue) {
        Write-Host "Chunk status unmapped:$(Format-Status $result.GetChunkStatusAfterUnmapRc) / IsMapped=$($result.ChunkMappedAfterUnmap)"
    }
    if ($result.GetUtilizationAfterUnmapRc -ne [int]::MinValue) {
        Write-Host "Util after unmap:     $(Format-Status $result.GetUtilizationAfterUnmapRc) / Chunks=$($result.UtilizationAfterUnmap.mapped_chunks) Bytes=$($result.UtilizationAfterUnmap.mapped_bytes) Base=$('0x{0:X}' -f $result.UtilizationAfterUnmap.aperture_base)"
    }

    if ($result.UnloadModelRc -ne [int]::MinValue) {
        Write-Host "UnloadModel:          $(Format-Status $result.UnloadModelRc)"
    } else {
        Write-Host "UnloadModel:          skipped"
    }

    Write-Host "Shutdown:             $(Format-Status $result.ShutdownRc)"
}

function Test-ChunkPass($result) {
    $passed = $true

    if ($result.InitializeRc -ne 0) { $passed = $false }
    if ($result.ApertureInitRc -ne 0) { $passed = $false }
    if ($result.GetApertureBaseBeforeMapRc -ne 0) { $passed = $false }
    if ($result.GetApertureBaseAfterMapRc -ne 0) { $passed = $false }
    if ($result.ShutdownRc -ne 0) { $passed = $false }

    if ($result.SampleModelLoaded) {
        if ($result.MapChunkRc -ne 0) { $passed = $false }
        if ($result.MappedAddress -eq 0) { $passed = $false }
        if ($result.MappedAddress -ne $result.ExpectedMappedAddress) { $passed = $false }
        if ($result.GetChunkStatusAfterMapRc -ne 0) { $passed = $false }
        if ($result.ChunkMappedAfterMap -ne 1) { $passed = $false }
        if ($result.GetUtilizationAfterMapRc -ne 0) { $passed = $false }
        if ($result.UtilizationAfterMap.mapped_chunks -ne 1) { $passed = $false }
        if ($result.UtilizationAfterMap.mapped_bytes -ne [TitanApertureSmoke]::RAWRXD_APERTURE_CHUNK_BYTES) { $passed = $false }
        if ($result.UnmapChunkRc -ne 0) { $passed = $false }
        if ($result.GetChunkStatusAfterUnmapRc -ne 0) { $passed = $false }
        if ($result.ChunkMappedAfterUnmap -ne 0) { $passed = $false }
        if ($result.GetUtilizationAfterUnmapRc -ne 0) { $passed = $false }
        if ($result.UtilizationAfterUnmap.mapped_chunks -ne 0) { $passed = $false }
        if ($result.UtilizationAfterUnmap.mapped_bytes -ne 0) { $passed = $false }
        if ($result.UtilizationAfterUnmap.aperture_base -ne 0) { $passed = $false }
        if ($result.UnloadModelRc -ne 0) { $passed = $false }
    } else {
        if ($result.MapChunkRc -ne [TitanApertureSmoke]::RAWRXD_ERROR_NO_MODEL_LOADED) { $passed = $false }
    }

    return $passed
}

function Write-FragmentationResult($result) {
    Write-Host "Fragmentation pattern: $($result.ChunkIndices -join ', ')"
    Write-Host "Initialize:           $(Format-Status $result.InitializeRc)"
    Write-Host "ApertureInit:         $(Format-Status $result.ApertureInitRc)"
    Write-Host "LoadModel:            $(Format-Status $result.LoadModelRc) / Handle=$($result.ModelHandle)"
    Write-Host "Aperture base:        $(Format-Status $result.GetApertureBaseRc) / $('0x{0:X}' -f $result.ApertureBase)"
    for ($i = 0; $i -lt $result.ChunkIndices.Length; $i++) {
        Write-Host "MapChunk[$($result.ChunkIndices[$i])]:      $(Format-Status $result.MapChunkRcs[$i]) / Address=$('0x{0:X}' -f $result.MappedAddresses[$i]) / Expected=$('0x{0:X}' -f $result.ExpectedAddresses[$i])"
    }
    Write-Host "Util fragmented:      $(Format-Status $result.GetUtilizationRc) / Chunks=$($result.Utilization.mapped_chunks) Bytes=$($result.Utilization.mapped_bytes) Gaps=$($result.Utilization.fragment_count) Ratio=$($result.Utilization.fragmentation_ratio)"
    Write-Host "VA fragmentation:     $(Format-Status $result.GetVAFragmentationRc) / Value=$($result.VAFragmentation)"
    Write-Host "UnmapChunk(last):     $(Format-Status $result.UnmapChunkRc)"
    Write-Host "Util after unmap:     $(Format-Status $result.GetUtilizationAfterUnmapRc) / Chunks=$($result.UtilizationAfterUnmap.mapped_chunks) Bytes=$($result.UtilizationAfterUnmap.mapped_bytes) Base=$('0x{0:X}' -f $result.UtilizationAfterUnmap.aperture_base)"
    Write-Host "UnloadModel:          $(Format-Status $result.UnloadModelRc)"
    Write-Host "Shutdown:             $(Format-Status $result.ShutdownRc)"
}

function Test-FragmentationPass($result) {
    $passed = $true

    if ($result.InitializeRc -ne 0) { $passed = $false }
    if ($result.ApertureInitRc -ne 0) { $passed = $false }
    if ($result.LoadModelRc -ne 0) { $passed = $false }
    if ($result.GetApertureBaseRc -ne 0) { $passed = $false }
    if ($result.GetUtilizationRc -ne 0) { $passed = $false }
    if ($result.GetVAFragmentationRc -ne 0) { $passed = $false }
    if ($result.Utilization.mapped_chunks -ne $result.ChunkIndices.Length) { $passed = $false }
    if ($result.Utilization.fragment_count -lt 1) { $passed = $false }
    if ($result.VAFragmentation -le 0.0) { $passed = $false }
    if ([Math]::Abs($result.VAFragmentation - $result.Utilization.fragmentation_ratio) -gt 0.000001) { $passed = $false }

    for ($i = 0; $i -lt $result.ChunkIndices.Length; $i++) {
        if ($result.MapChunkRcs[$i] -ne 0) { $passed = $false }
        if ($result.MappedAddresses[$i] -ne $result.ExpectedAddresses[$i]) { $passed = $false }
    }

    if ($result.UnmapChunkRc -ne 0) { $passed = $false }
    if ($result.GetUtilizationAfterUnmapRc -ne 0) { $passed = $false }
    if ($result.UtilizationAfterUnmap.mapped_chunks -ne 0) { $passed = $false }
    if ($result.UtilizationAfterUnmap.aperture_base -ne 0) { $passed = $false }
    if ($result.UnloadModelRc -ne 0) { $passed = $false }
    if ($result.ShutdownRc -ne 0) { $passed = $false }

    return $passed
}

function Test-ExportParity {
    $expectedExports = @()
    foreach ($line in Get-Content -LiteralPath $DefPath) {
        if ($line -match '^\s*(RawrXD_[A-Za-z0-9_]+)\s+@\d+') {
            $expectedExports += $matches[1]
        }
    }

    $h = [TitanApertureNative]::LoadLibrary($DllPath)
    if ($h -eq [IntPtr]::Zero) {
        $err = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "LoadLibrary failed during export verification ($err): $DllPath"
    }

    try {
        $missing = @()
        foreach ($name in $expectedExports) {
            $p = [TitanApertureNative]::GetProcAddress($h, $name)
            if ($p -eq [IntPtr]::Zero) {
                $missing += $name
            }
        }

        return [pscustomobject]@{
            Expected = $expectedExports.Count
            Resolved = $expectedExports.Count - $missing.Count
            Missing = $missing
        }
    }
    finally {
        [void][TitanApertureNative]::FreeLibrary($h)
    }
}

Write-Host "Titan aperture smoke test"
Write-Host "DLL: $DllPath"
Write-Host "Sample model: $sampleModelDisplay"
Write-Host "Chunk indices:        $($resolvedChunkIndices -join ', ')"
Write-Host ""

$overallPassed = $true
foreach ($currentChunk in $resolvedChunkIndices) {
    $result = [TitanApertureSmoke]::Invoke($DllPath, $resolvedModelPath, $currentChunk)
    Write-ChunkResult $result
    if (-not (Test-ChunkPass $result)) {
        $overallPassed = $false
    }
    Write-Host ""
}

if ($resolvedModelPath -and $resolvedFragmentationPattern.Count -ge 2) {
    $fragmentationResult = [TitanApertureSmoke]::InvokeFragmentation($DllPath, $resolvedModelPath, [uint32[]]$resolvedFragmentationPattern)
    Write-FragmentationResult $fragmentationResult
    if (-not (Test-FragmentationPass $fragmentationResult)) {
        $overallPassed = $false
    }
    Write-Host ""
}

$exportResult = $null
if ($VerifyExports) {
    $exportResult = Test-ExportParity
    Write-Host "Export regression:    EXPECTED=$($exportResult.Expected) RESOLVED=$($exportResult.Resolved) MISSING=$($exportResult.Missing.Count)"
    if ($exportResult.Missing.Count -gt 0) {
        $overallPassed = $false
        foreach ($missingName in $exportResult.Missing) {
            Write-Host "  Missing export: $missingName" -ForegroundColor Red
        }
    }
    Write-Host ""
}

if (-not $overallPassed) {
    Write-Host ""
    Write-Host "Titan aperture smoke FAILED." -ForegroundColor Red
    exit 1
}

Write-Host ""
if ($resolvedModelPath) {
    Write-Host "Titan aperture smoke PASSED with multi-chunk live map/unmap coverage." -ForegroundColor Green
} else {
    Write-Host "Titan aperture smoke PASSED with guard-path coverage (no sample model loaded)." -ForegroundColor Green
}