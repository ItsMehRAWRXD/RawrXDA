# Quick rebuild of MASM kernels with symbol aliases
$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   Rebuild MASM Kernels with Symbol Aliases               ║" -ForegroundColor Cyan
Write-Host "║   Fixing symbol resolution for C orchestrator             ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Paths
$projectRoot = "D:\temp\RawrXD-agentic-ide-production"
$kernelDir = Join-Path $projectRoot "RawrXD-ModelLoader\kernels"
$buildDir = Join-Path $projectRoot "build-sovereign"

Write-Host "[0/2] Initializing Visual Studio 2022 environment..." -ForegroundColor Yellow

# Find vcvars64.bat
$vsPath = "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vsPath)) {
    $vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
}
if (-not (Test-Path $vsPath)) {
    $vsPath = "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
}

if (-not (Test-Path $vsPath)) {
    Write-Host "  ✗ Visual Studio 2022 not found!" -ForegroundColor Red
    exit 1
}

# Run vcvars64 and capture environment
cmd /c "`"$vsPath`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}

Write-Host "  ✓ Visual Studio environment loaded" -ForegroundColor Green
Write-Host ""

# Find ml64.exe location
$vsVersions = @("2022", "2019", "2017")
$ml64Path = ""
foreach ($version in $vsVersions) {
    $searchPath = "C:\Program Files\Microsoft Visual Studio\$version\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
    $found = Get-Item $searchPath -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $ml64Path = $found.FullName
        break
    }
    
    $searchPath = "C:\Program Files\Microsoft Visual Studio\$version\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
    $found = Get-Item $searchPath -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $ml64Path = $found.FullName
        break
    }
    
    $searchPath = "C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe"
    $found = Get-Item $searchPath -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $ml64Path = $found.FullName
        break
    }
}

if (-not $ml64Path) {
    Write-Host "  ✗ ml64.exe not found in VS paths!" -ForegroundColor Red
    exit 1
}

Write-Host "[1/2] Assembling MASM kernels with symbol aliases..." -ForegroundColor Yellow

# Compile MASM files
$masmFiles = @(
    "universal_quant_kernel.asm",
    "beaconism_dispatcher.asm",
    "dimensional_pool.asm"
)

$objFiles = @()

foreach ($asmFile in $masmFiles) {
    $asmPath = Join-Path $kernelDir $asmFile
    $objName = [System.IO.Path]::GetFileNameWithoutExtension($asmFile) + ".obj"
    $objPath = Join-Path $buildDir $objName
    
    if (Test-Path $asmPath) {
        Write-Host "  → Assembling $asmFile..." -NoNewline
        
        $result = & "$ml64Path" /c /Fo"$objPath" "$asmPath" 2>&1
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host " ✓" -ForegroundColor Green
            $objFiles += $objPath
        } else {
            Write-Host " ✗" -ForegroundColor Red
            Write-Host "Error output:" -ForegroundColor Red
            Write-Host $result -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "  ✗ File not found: $asmPath" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "[2/2] Relinking RawrXD-SovereignLoader.dll..." -ForegroundColor Yellow

$cObj = Join-Path $buildDir "sovereign_loader.obj"
$dllPath = Join-Path $buildDir "bin\RawrXD-SovereignLoader.dll"
$libPath = Join-Path $buildDir "bin\RawrXD-SovereignLoader.lib"
$expPath = Join-Path $buildDir "bin\RawrXD-SovereignLoader.exp"

# Remove old artifacts
Remove-Item $dllPath -ErrorAction SilentlyContinue
Remove-Item $libPath -ErrorAction SilentlyContinue
Remove-Item $expPath -ErrorAction SilentlyContinue

Write-Host "  → Linking DLL with updated kernels..." -NoNewline

# Find link.exe
$linkPath = ""
foreach ($version in $vsVersions) {
    $searchPath = "C:\Program Files\Microsoft Visual Studio\$version\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe"
    $found = Get-Item $searchPath -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $linkPath = $found.FullName
        break
    }
    
    $searchPath = "C:\Program Files\Microsoft Visual Studio\$version\Community\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe"
    $found = Get-Item $searchPath -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $linkPath = $found.FullName
        break
    }
}

if (-not $linkPath) {
    Write-Host " ✗" -ForegroundColor Red
    Write-Host "  ✗ link.exe not found in VS paths!" -ForegroundColor Red
    exit 1
}

# Link as DLL with all MASM objects
$allObjs = @($cObj) + $objFiles
$objList = $allObjs -join " "

$result = & "$linkPath" /DLL /OUT:"$dllPath" /IMPLIB:"$libPath" $objList kernel32.lib 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host " ✓" -ForegroundColor Green
} else {
    Write-Host " ✗" -ForegroundColor Red
    Write-Host $result -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "  KERNEL REBUILD SUCCESSFUL" -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host ""
Write-Host "New symbol aliases enabled:" -ForegroundColor Green
Write-Host "  ✓ load_model_beacon -> ManifestVisualIdentity" -ForegroundColor Gray
Write-Host "  ✓ validate_beacon_signature -> ProcessSignal" -ForegroundColor Gray
Write-Host "  ✓ quantize_tensor_zmm -> EncodeToPoints" -ForegroundColor Gray
Write-Host "  ✓ dequantize_tensor_zmm -> DecodeFromPoints" -ForegroundColor Gray
Write-Host "  ✓ dimensional_pool_init -> CreateWeightPool" -ForegroundColor Gray
Write-Host ""
Write-Host "DLL location: $dllPath" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next: Run test_loader.exe to verify symbol resolution" -ForegroundColor White
Write-Host ""
