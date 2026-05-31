param(
    [ValidateSet('Release','Debug')]
    [string]$Configuration = 'Release',
    [switch]$EnableAVX512,
    [int]$ModelSize = 120,
    [switch]$SkipExportSmoke
)

$ErrorActionPreference = 'Stop'

$Ml64 = 'C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe'
$LinkExe = 'C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe'
$SrcDir = 'D:\rawrxd\src'
$KernelDir = Join-Path $SrcDir 'kernels'
$MemoryDir = Join-Path $SrcDir 'memory'
$BuildDir = 'D:\rawrxd\build-sovereign'
$OutputDir = Join-Path $BuildDir $Configuration
$ExportSmokeScript = Join-Path $PSScriptRoot 'Test-RawrXD-SovereignExports.ps1'

if (!(Test-Path $Ml64)) { throw "ml64 not found: $Ml64" }
if (!(Test-Path $LinkExe)) { throw "link.exe not found: $LinkExe" }
if (-not $SkipExportSmoke -and !(Test-Path $ExportSmokeScript)) {
    throw "Export smoke script not found: $ExportSmokeScript"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$Defines = @('WIN64','_AMD64_','RAWRXD_SOVEREIGN=1')
if ($EnableAVX512) { $Defines += 'RAWRXD_AVX512=1' }
if ($ModelSize -eq 800) { $Defines += 'RAWRXD_TARGET_800B=1' } else { $Defines += 'RAWRXD_TARGET_120B=1' }

$Sources = @(
    (Join-Path $SrcDir 'RawrXD_SovereignEnhancements_Entry.asm'),
    (Join-Path $MemoryDir 'RawrXD_Enh1_TieredMemoryOrchestrator.asm'),
    (Join-Path $KernelDir 'RawrXD_Enh2_DynamicQuantizationHotpatcher.asm'),
    (Join-Path $KernelDir 'RawrXD_Enh3_SpeculativeDecodingEngine.asm'),
    (Join-Path $KernelDir 'RawrXD_Enh4_KVCacheCompression.asm'),
    (Join-Path $KernelDir 'RawrXD_Enh5_ContinuousBatching.asm'),
    (Join-Path $KernelDir 'RawrXD_Enh6_MoEExpertRouting.asm'),
    (Join-Path $KernelDir 'RawrXD_Enh7_AsyncStreamingLoader.asm'),
    (Join-Path $KernelDir 'RawrXD_Enh8_ThermalAwareThrottling.asm')
)

$ObjFiles = @()
foreach ($src in $Sources) {
    if (!(Test-Path $src)) { throw "Missing source: $src" }

    $obj = Join-Path $OutputDir (([IO.Path]::GetFileNameWithoutExtension($src)) + '.obj')
    $args = @('/nologo','/c',"/Fo$obj")
    foreach ($define in $Defines) { $args += @('/D',$define) }
    $args += $src

    Write-Host "Compiling $([IO.Path]::GetFileName($src))"
    & $Ml64 @args
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $src" }

    $ObjFiles += $obj
}

$DllPath = Join-Path $OutputDir 'RawrXD_Sovereign.dll'
$LibPath = Join-Path $OutputDir 'RawrXD_Sovereign.lib'

$Exports = @(
    'SovereignEnhancements_InitializeAll',
    'SovereignEnhancements_InferenceStep',
    'TieredOrchestrator_Initialize',
    'TieredOrchestrator_MigratePage',
    'DynamicQuant_Initialize',
    'DynamicQuant_HotPatchLevel',
    'DynamicQuant_AdaptiveLayer',
    'SpeculativeDecoding_Initialize',
    'SpeculativeDecoding_GenerateDrafts',
    'SpeculativeDecoding_VerifyDrafts',
    'KVCacheCompression_Initialize',
    'KVCacheCompression_UpdateScores',
    'KVCacheCompression_CompressTier',
    'ContinuousBatching_Initialize',
    'ContinuousBatching_ScheduleRequest',
    'ContinuousBatching_Step',
    'PagedKV_AllocatePages',
    'MoE_Initialize',
    'MoE_RouteTokens',
    'MoE_LoadBalance',
    'MoE_SparseActivate',
    'AsyncStreamingLoader_Initialize',
    'AsyncStreamingLoader_BeginStream',
    'AsyncIO_WorkerThread',
    'AsyncStreamingLoader_Cancel',
    'ThermalAwareThrottling_Initialize',
    'ThermalMonitor_Thread',
    'ThermalAwareThrottling_GetCurrentTarget'
)

$linkArgs = @('/NOLOGO','/DLL','/NOENTRY','/MACHINE:X64',"/OUT:$DllPath", "/IMPLIB:$LibPath")
foreach ($exportName in $Exports) { $linkArgs += "/EXPORT:$exportName" }
$linkArgs += $ObjFiles

Write-Host 'Linking RawrXD_Sovereign.dll'
& $LinkExe @linkArgs
if ($LASTEXITCODE -ne 0) { throw 'Link failed for RawrXD_Sovereign.dll' }

$ExportSmokePassed = $false
if (-not $SkipExportSmoke) {
    Write-Host 'Running export smoke validation'
    & powershell -NoProfile -ExecutionPolicy Bypass -File $ExportSmokeScript -DllPath $DllPath
    if ($LASTEXITCODE -ne 0) { throw 'Export smoke validation failed' }
    $ExportSmokePassed = $true
}

$Manifest = [ordered]@{
    version = '1.3.0-sovereign'
    configuration = $Configuration
    model_size_target = $ModelSize
    avx512 = [bool]$EnableAVX512
    dll = $DllPath
    lib = $LibPath
    export_smoke_passed = $ExportSmokePassed
    exports = $Exports
    objects = $ObjFiles
}

$ManifestPath = Join-Path $OutputDir 'sovereign_manifest.json'
$Manifest | ConvertTo-Json -Depth 4 | Set-Content -Encoding ascii -Path $ManifestPath

Write-Host 'Build complete'
Write-Host "Manifest: $ManifestPath"
