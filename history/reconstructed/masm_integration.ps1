# masm_integration.ps1
# Assembles MASM files and patches CMakeLists.txt for full integration
# Run from repo root: .\masm_integration.ps1

param([string]$Config = "Release")
$ErrorActionPreference = "Stop"

# Colors
$Green = "`e[32m"; $Yellow = "`e[33m"; $Cyan = "`e[36m"; $Red = "`e[31m"; $Reset = "`e[0m"

Write-Host "${Cyan}╔═══════════════════════════════════════════════════════╗${Reset}"
Write-Host "${Cyan}║  RawrXD MASM64 Integration Module                   ║${Reset}"
Write-Host "${Cyan}║  Vulkan Compute + Circular Beacon (Pure ASM)        ║${Reset}"
Write-Host "${Cyan}╚═══════════════════════════════════════════════════════╝${Reset}"

# 1. Find ML64.exe
$ml64 = $null
$vsPaths = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
)

foreach ($path in $vsPaths) {
    if (Test-Path $path) {
        $latest = Get-ChildItem $path -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if ($latest) {
            $candidate = Join-Path $latest.FullName "bin\Hostx64\x64\ml64.exe"
            if (Test-Path $candidate) {
                $ml64 = $candidate
                break
            }
        }
    }
}

if (-not (Test-Path $ml64)) {
    Write-Host "${Red}❌ ml64.exe not found${Reset}"
    exit 1
}

Write-Host "${Green}✓${Reset} ml64.exe: $(Split-Path $ml64 -Leaf)"

# 2. Create build output directory
$buildDir = "build\MASM64"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# 3. Assemble MASM files
$asmFiles = @(
    @{Src="src\asm\vulkan_compute.asm"; Obj="$buildDir\vulkan_compute.obj"},
    @{Src="src\asm\beacon_integration.asm"; Obj="$buildDir\beacon_integration.obj"}
)

$objects = @()

foreach ($file in $asmFiles) {
    $src = $file.Src
    $obj = $file.Obj
    
    if (-not (Test-Path $src)) {
        Write-Host "${Red}✗${Reset} Source not found: $src"
        continue
    }
    
    Write-Host "  Assembling $(Split-Path $src -Leaf)..." -NoNewline
    & $ml64 /c /Fo"$obj" "$src" 2>&1 | Out-Null
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host " ${Green}OK${Reset}"
        $objects += $obj
    } else {
        Write-Host " ${Red}FAIL${Reset}"
        exit 1
    }
}

Write-Host "`n${Green}✅ Assembled:${Reset}"
$objects | ForEach-Object { 
    Write-Host "   $_"
}

# 4. Patch CMakeLists.txt to link MASM objects
$cmakePath = "CMakeLists.txt"
if (Test-Path $cmakePath) {
    Write-Host "`n${Yellow}→${Reset} Patching CMakeLists.txt..."
    $content = Get-Content $cmakePath -Raw
    
    # Check if already patched
    if ($content -notmatch "MASM64.*vulkan_compute") {
        # Find the add_executable line and add our objects before the link libraries
        $patch = @'

# MASM64 Integration (Vulkan Compute + Beacon)
target_sources(RawrXD PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/build/MASM64/vulkan_compute.obj
    ${CMAKE_CURRENT_SOURCE_DIR}/build/MASM64/beacon_integration.obj
)
'@
        
        # Insert after find_package(Vulkan)
        if ($content -match "find_package\(Vulkan\s+REQUIRED\)") {
            $content = $content -replace "(find_package\(Vulkan\s+REQUIRED\))", "`$1$patch"
        } else {
            # Fallback: add after project() declaration
            $content = $content -replace "(project\([^)]+\))", "`$1$patch"
        }
        
        $content | Set-Content $cmakePath -Force
        Write-Host "${Green}✓${Reset} CMakeLists.txt patched"
    } else {
        Write-Host "${Green}✓${Reset} CMakeLists.txt already patched"
    }
}

# 5. Verify objects exist
Write-Host "`n${Cyan}Verification:${Reset}"
foreach ($obj in $objects) {
    if (Test-Path $obj) {
        $size = (Get-Item $obj).Length
        Write-Host "${Green}✓${Reset} $obj ($([Math]::Round($size/1KB, 1))KB)"
    } else {
        Write-Host "${Red}✗${Reset} $obj not found"
    }
}

Write-Host "`n${Green}✅ Integration Complete!${Reset}"
Write-Host "Next: Run ${Cyan}cmake -B build${Reset} then ${Cyan}cmake --build build${Reset}"
