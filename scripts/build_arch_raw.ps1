# build_arch_raw.ps1 - Pure MASM64 RawrXD_Arch.lib + Export Verification
# Builds static lib from critical/optional ASM; when optional are missing, links
# RawrXD_Arch_Stub.asm so IDE can start without hanging. Run from repo root.

param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [switch]$Clean
)

Set-ExecutionPolicy Bypass -Scope Process -Force -ErrorAction SilentlyContinue
$ErrorActionPreference = "Stop"

$Red    = "`e[31m"
$Green  = "`e[32m"
$Yellow = "`e[33m"
$Cyan   = "`e[36m"
$Reset  = "`e[0m"

Write-Host "${Cyan}RawrXD_Arch.lib — Pure MASM64 build + verify${Reset}" -ForegroundColor Cyan
Write-Host "Config: $Config`n"

# ---------------------------------------------------------------------------
# Locate VS2022 tools (ml64, link, dumpbin)
# ---------------------------------------------------------------------------
function Find-VSToolsRaw {
    $searchPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
    )
    foreach ($base in $searchPaths) {
        if (-not (Test-Path $base)) { continue }
        $mvcFolder = Get-ChildItem -Path $base -Directory | Sort-Object Name -Descending | Select-Object -First 1
        if (-not $mvcFolder) { continue }
        $binPath = Join-Path $mvcFolder.FullName "bin\Hostx64\x64"
        $ml64Path = Join-Path $binPath "ml64.exe"
        if (Test-Path $ml64Path) {
            $dumpbin = Join-Path $binPath "dumpbin.exe"
            return @{ ml64 = $ml64Path; link = (Join-Path $binPath "link.exe"); dumpbin = $dumpbin }
        }
    }
    # vswhere fallback (P2: auto-detect VS path)
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $ml64Path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "**/x64/ml64.exe" 2>$null | Select-Object -First 1
        if ($ml64Path -and (Test-Path $ml64Path)) {
            $dir = Split-Path $ml64Path
            return @{ ml64 = $ml64Path; link = (Join-Path $dir "link.exe"); dumpbin = (Join-Path $dir "dumpbin.exe") }
        }
    }
    try {
        $ml64 = (Get-Command ml64.exe -ErrorAction Stop).Source
        $dir = Split-Path $ml64
        return @{ ml64 = $ml64; link = (Join-Path $dir "link.exe"); dumpbin = (Join-Path $dir "dumpbin.exe") }
    } catch {
        return $null
    }
}

$tools = Find-VSToolsRaw
if (-not $tools -or -not (Test-Path $tools.ml64)) {
    Write-Host "${Red}FATAL: ml64.exe not found. Install VS2022 C++ or run from x64 Native Tools prompt.${Reset}"
    exit 1
}
if (-not (Test-Path $tools.dumpbin)) { $tools.dumpbin = $null }

$root = if ($PSScriptRoot) { Split-Path $PSScriptRoot -Parent } else { $PWD.Path }
$srcDir = Join-Path $root "src"
$asmDir = Join-Path $srcDir "asm"
$outDir = Join-Path $root "build" $Config
$stubAsm = Join-Path $srcDir "RawrXD_Arch_Stub.asm"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
if ($Clean) {
    Remove-Item "$outDir\*.obj" -Force -ErrorAction SilentlyContinue
    Remove-Item "$outDir\RawrXD_Arch.lib" -Force -ErrorAction SilentlyContinue
}

function Find-Asm($name) {
    $paths = @(
        (Join-Path $asmDir $name),
        (Join-Path $root "src\asm\kernel_suite" $name)
    )
    foreach ($p in $paths) { if (Test-Path $p) { return $p } }
    return $null
}

$critical = @("memory_patch.asm", "RawrCodex.asm")
$optional = @("kernels.asm", "streaming_dma.asm", "quant_avx2.asm")
$objs = @()
$stubbed = @()

# Critical ASM — must exist
foreach ($asm in $critical) {
    $path = Find-Asm $asm
    if (-not $path) {
        Write-Host "${Red}FATAL: Critical $asm missing${Reset}"
        exit 1
    }
    $obj = Join-Path $outDir ($asm -replace '\.asm$', '.obj')
    & $tools.ml64 /nologo /c /Fo"$obj" "$path" 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "${Red}FATAL: Assemble failed for $asm${Reset}"
        exit 1
    }
    $objs += $obj
}

# Optional ASM — assemble if present, else stub
foreach ($asm in $optional) {
    $path = Find-Asm $asm
    if (-not $path) {
        Write-Host "${Yellow}STUBBED: $asm (not found)${Reset}"
        $stubbed += $asm
        continue
    }
    $obj = Join-Path $outDir ($asm -replace '\.asm$', '.obj')
    & $tools.ml64 /nologo /c /Fo"$obj" "$path" 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $objs += $obj
    } else {
        Write-Host "${Yellow}STUBBED: $asm (assemble failed)${Reset}"
        $stubbed += $asm
    }
}

# When any optional stubbed, add pure MASM stub object
if ($stubbed.Count -gt 0) {
    Set-Content -Path (Join-Path $outDir "optional_stubs.txt") -Value ($stubbed -join "`n")
    $stubObj = Join-Path $outDir "arch_stub.obj"
    if (-not (Test-Path $stubAsm)) {
        Write-Host "${Red}FATAL: RawrXD_Arch_Stub.asm not found at $stubAsm${Reset}"
        exit 1
    }
    & $tools.ml64 /nologo /c /Fo"$stubObj" /DRAWRXD_ASM_STUBBED "$stubAsm" 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        & $tools.ml64 /nologo /c /Fo"$stubObj" "$stubAsm" 2>&1 | Out-Null
    }
    if ($LASTEXITCODE -eq 0) {
        $objs += $stubObj
        Write-Host "${Green}Added MASM stub: arch_stub.obj${Reset}"
    }
}

# Link static library
$libPath = Join-Path $outDir "RawrXD_Arch.lib"
& $tools.link /nologo /OUT:"$libPath" /LIB $objs 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "${Red}Link failed for RawrXD_Arch.lib${Reset}"
    exit 1
}

# Export verification
$critExp = @("RawrXD_MemPatch", "RawrCodex_Disasm")
if ($tools.dumpbin -and (Test-Path $tools.dumpbin)) {
    Write-Host "`n${Cyan}EXPORT VERIFICATION${Reset}"
    $expOut = & $tools.dumpbin /EXPORTS "$libPath" 2>$null
    $expOut | Select-String -Pattern "(RawrXD_MemPatch|RawrCodex_|RawrXD_SGEMM|RawrXD_DMA)" | ForEach-Object { Write-Host "  $_" -ForegroundColor Green }
    foreach ($e in $critExp) {
        if ($expOut -notmatch [regex]::Escape($e)) {
            Write-Host "${Red}FAIL: Critical export $e missing${Reset}"
            exit 1
        }
    }
} else {
    Write-Host "${Yellow}dumpbin not found; skipping export verification${Reset}"
}

Write-Host "`n${Green}BUILD VALIDATED: $($objs.Count) objects, $($stubbed.Count) stubbed → $libPath${Reset}"
