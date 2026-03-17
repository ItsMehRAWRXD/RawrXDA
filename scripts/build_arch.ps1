# build_arch.ps1 - Genesis Build with VS2022 Auto-Detection
# Save as: scripts/build_arch.ps1
# Run from repo root: .\scripts\build_arch.ps1

param(
    [string]$Config = "Release",
    [string]$Target = "RawrXD-AgenticIDE",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Colors
$Red = "`e[31m"
$Green = "`e[32m"
$Yellow = "`e[33m"
$Cyan = "`e[36m"
$Reset = "`e[0m"

Write-Host "${Cyan}🔨 RawrXD Architecture Bridge Builder${Reset}"
Write-Host "Config: $Config | Target: $Target`n"

# ========================================================================
# 1. LOCATE VS2022 TOOLS (Auto-detection)
# ========================================================================

function Find-VSTools {
    $tools = @{
        ml64 = $null
        cl   = $null
        link = $null
    }

    # Search paths for VS2022 (Community, Professional, Enterprise, Build Tools)
    $searchPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
    )

    # Try to find ml64.exe in standard locations
    foreach ($basePath in $searchPaths) {
        if (Test-Path $basePath) {
            # Get latest MSVC version folder (sorted by version number)
            $mvcFolder = Get-ChildItem -Path $basePath -Directory |
                Sort-Object Name -Descending |
                Select-Object -First 1

            if ($mvcFolder) {
                $binPath = Join-Path $mvcFolder.FullName "bin\Hostx64\x64"
                $ml64Path = Join-Path $binPath "ml64.exe"

                if (Test-Path $ml64Path) {
                    $tools.ml64 = $ml64Path
                    $tools.cl = Join-Path $binPath "cl.exe"
                    $tools.link = Join-Path $binPath "link.exe"
                    Write-Host "${Green}✓${Reset} Found VS2022 tools: $($mvcFolder.Name)"
                    return $tools
                }
            }
        }
    }

    # Fallback to PATH
    try {
        $ml64Cmd = Get-Command ml64.exe -ErrorAction Stop
        $tools.ml64 = $ml64Cmd.Source

        # Assume cl and link are in same directory or PATH
        $tools.cl = (Get-Command cl.exe -ErrorAction Stop).Source
        $tools.link = (Get-Command link.exe -ErrorAction Stop).Source

        Write-Host "${Yellow}⚠${Reset} Using ml64 from PATH: $($tools.ml64)"
        return $tools
    }
    catch {
        return $null
    }
}

$tools = Find-VSTools

if (-not $tools -or -not $tools.ml64) {
    Write-Host "${Red}❌ ERROR:${Reset} Cannot find Visual Studio 2022 C++ tools (ml64.exe)" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install one of the following:"
    Write-Host "  • Visual Studio 2022 Community/Pro/Ent with 'Desktop development with C++'"
    Write-Host "  • Visual Studio Build Tools 2022 with 'C++ build tools'"
    Write-Host ""
    Write-Host "Or run this script from 'x64 Native Tools Command Prompt for VS 2022'"
    Write-Host "which sets the correct PATH environment variables."
    exit 1
}

# Verify tools exist
foreach ($tool in @('ml64', 'cl', 'link')) {
    if (-not (Test-Path $tools[$tool])) {
        Write-Host "${Red}❌ ERROR:${Reset} Found ml64 but missing $tool.exe" -ForegroundColor Red
        Write-Host "Expected at: $($tools[$tool])"
        exit 1
    }
}

Write-Host "${Green}✓${Reset} Tools located:"
Write-Host "    ml64:  $($tools.ml64)"
Write-Host "    cl:    $($tools.cl)"
Write-Host "    link:  $($tools.link)"
Write-Host ""

# ========================================================================
# 2. BUILD CONFIGURATION
# ========================================================================

$root = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { $PWD.Path }
$srcDir = Join-Path $root "src"
$asmDir = Join-Path $srcDir "asm"
$win32appDir = Join-Path $srcDir "win32app"
$outDir = Join-Path $root "build" $Config
$includeDir = Join-Path $root "include"

# Create output directory
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

if ($Clean) {
    Write-Host "${Yellow}🧹 Cleaning output directory...${Reset}"
    Remove-Item "$outDir\*.obj" -Force -ErrorAction SilentlyContinue
    Remove-Item "$outDir\*.exe" -Force -ErrorAction SilentlyContinue
}

$objs = @()

# Critical: ASM that provides asm_spengine_init, asm_gguf_loader_init, etc.
# If missing, we must link arch_stub.cpp so init does not hang waiting for symbols.
$criticalAsm = @("rawrxd_link.asm", "rawrxd_scc.asm")
$optionalAsm = @("kernels.asm", "memory_patch.asm", "streaming_dma.asm", "quant_avx2.asm", "RawrCodex.asm")

function Find-SourceFile($dir, $name) {
    $p = Join-Path $dir $name
    return (Test-Path $p), $p
}

# ========================================================================
# 3. ASSEMBLE MASM64 SOURCES (critical first, then optional)
# ========================================================================

$haveCathedralAsm = $false
foreach ($asm in $criticalAsm) {
    $exists, $srcPath = Find-SourceFile $asmDir $asm
    if ($exists) {
        Write-Host "${Cyan}📦 Assembling MASM64 sources (critical)...${Reset}"
        $objPath = Join-Path $outDir ($asm -replace '\.asm$', '.obj')
        Write-Host "  Assembling $asm..." -NoNewline
        & $tools.ml64 /c /Fo"$objPath" "$srcPath" 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "${Green} OK${Reset}"
            $objs += $objPath
            $haveCathedralAsm = $true
            break
        }
        else {
            Write-Host "${Red} FAIL${Reset}"
            Write-Host "${Red}❌ CRITICAL: $asm failed to assemble. IDE init requires these symbols.${Reset}"
            exit 1
        }
    }
}

if (-not $haveCathedralAsm) {
    Write-Host "${Yellow}⚠️  No cathedral ASM (rawrxd_link.asm / rawrxd_scc.asm) — will link arch_stub.cpp so init does not hang.${Reset}"
}

Write-Host "${Cyan}📦 Assembling optional MASM64 sources...${Reset}"
foreach ($asm in $optionalAsm) {
    $exists, $srcPath = Find-SourceFile $asmDir $asm
    if ($exists) {
        $objPath = Join-Path $outDir ($asm -replace '\.asm$', '.obj')
        Write-Host "  Assembling $asm..." -NoNewline
        & $tools.ml64 /c /Fo"$objPath" "$srcPath" 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "${Green} OK${Reset}"
            $objs += $objPath
        }
        else {
            Write-Host "${Red} FAIL${Reset}"
            Write-Host "Failed to assemble: $srcPath"
            exit 1
        }
    }
    else {
        Write-Host "${Yellow}  SKIP${Reset} $asm (not found)"
    }
}

# ========================================================================
# 4. COMPILE C++20 SOURCES
# ========================================================================
# When cathedral ASM is missing, we must link arch_stub.cpp so asm_*_init
# symbols resolve and init does not hang.
$cppFiles = @(
    "main_win32.cpp",
    "Win32IDE_Core.cpp",
    "arch_verify.cpp",
    "arch_bridge.cpp",
    "ModelConnection.cpp",
    "CompletionEngine.cpp"
)
if (-not $haveCathedralAsm) {
    $stubPath = Join-Path $srcDir "arch_stub.cpp"
    if (-not (Test-Path $stubPath)) {
        Write-Host "${Red}❌ CRITICAL: arch_stub.cpp not found. Required when cathedral ASM is not built.${Reset}"
        exit 1
    }
    $cppFiles = @("arch_stub.cpp") + $cppFiles
}

Write-Host "`n${Cyan}🔤 Compiling C++20 sources...${Reset}"

$cppFlags = @(
    "/std:c++20",
    "/O2",                    # Optimize for speed
    "/EHsc",                  # Exception handling
    "/W4",                    # Warning level 4
    "/I$includeDir",
    "/I$srcDir",
    "/I$win32appDir",
    "/DUNICODE",
    "/D_UNICODE",
    "/DNDEBUG",               # Release define
    "/c"                      # Compile only
)

if ($Config -eq "Debug") {
    $cppFlags = @(
        "/std:c++20",
        "/Od",
        "/Zi",
        "/EHsc",
        "/W4",
        "/I$includeDir",
        "/I$srcDir",
        "/I$win32appDir",
        "/DUNICODE",
        "/D_UNICODE",
        "/c"
    )
}

foreach ($cpp in $cppFiles) {
    # Look in win32app first for IDE sources, then src
    $srcPath = Join-Path $win32appDir $cpp
    if (-not (Test-Path $srcPath)) {
        $srcPath = Join-Path $srcDir $cpp
    }
    if (Test-Path $srcPath) {
        $objPath = Join-Path $outDir ($cpp -replace '\.cpp$', '.obj')
        Write-Host "  Compiling $cpp..." -NoNewline

        & $tools.cl @cppFlags /Fo"$objPath" "$srcPath" 2>&1 | Out-Null

        if ($LASTEXITCODE -eq 0) {
            Write-Host "${Green} OK${Reset}"
            $objs += $objPath
        }
        else {
            Write-Host "${Red} FAIL${Reset}"
            Write-Host "Failed to compile: $srcPath"
            exit 1
        }
    }
    else {
        Write-Host "${Yellow}  SKIP${Reset} $cpp (not found)"
    }
}

# ========================================================================
# 5. LINK EXECUTABLE
# ========================================================================

if ($objs.Count -eq 0) {
    Write-Host "`n${Red}❌ No objects to link.${Reset}" -ForegroundColor Red
    exit 1
}

Write-Host "`n${Cyan}🔗 Linking $Target.exe...${Reset}"

$linkFlags = @(
    "/OUT:$outDir\$Target.exe",
    "/SUBSYSTEM:WINDOWS",
    "/OPT:REF",               # Eliminate unreferenced functions
    "/OPT:ICF",               # Identical COMDAT folding
    "/MACHINE:X64"
)

if ($Config -eq "Release") {
    $linkFlags += "/LTCG"     # Link-time code generation
}

$libs = @(
    "kernel32.lib",
    "user32.lib",
    "gdi32.lib",
    "winmm.lib",
    "shell32.lib",
    "ole32.lib",
    "winhttp.lib"
)

& $tools.link @linkFlags $objs $libs 2>&1 | Out-Null

if ($LASTEXITCODE -eq 0) {
    $exePath = "$outDir\$Target.exe"
    $exeSize = (Get-Item $exePath).Length / 1KB

    Write-Host "${Green}✅ Build successful!${Reset}"
    Write-Host "   Output: $exePath"
    Write-Host "   Size: $([math]::Round($exeSize, 2)) KB"
    Write-Host "   Objects: $($objs.Count) files"

    # Verify imports (optional check)
    $dumpbin = Join-Path (Split-Path $tools.link) "dumpbin.exe"
    if (Test-Path $dumpbin) {
        & $dumpbin /IMPORTS $exePath 2>&1 | Select-String "RawrXD" | ForEach-Object {
            Write-Host "   Export: $_" -ForegroundColor Gray
        }
    }
}
else {
    Write-Host "${Red}❌ Link failed${Reset}"
    exit 1
}

Write-Host "`n${Green}Done.${Reset} Run with: & '$outDir\$Target.exe'"
