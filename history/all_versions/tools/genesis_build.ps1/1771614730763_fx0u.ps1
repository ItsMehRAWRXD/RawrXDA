# RawrXD Genesis Build
# Default: strict MASM64 pipeline (assemble required components).
# Optional: Link-only mode that consumes prebuilt COFF inputs and produces a single PE executable.

param(
    [string]$Configuration = "Release",
    [switch]$LinkOnly,
    # If provided, these files are passed directly to the linker (.obj/.lib/.res). Overrides ObjDir/LibDir scanning.
    [string[]]$Inputs = @(),
    # Directory containing prebuilt .obj inputs (used when -Inputs not supplied)
    [string]$ObjDir = "",
    # Directory containing prebuilt .lib inputs (used when -Inputs not supplied)
    [string]$LibDir = "",
    # Optional resource file (.res)
    [string]$ResPath = "",
    # Output exe path
    [string]$OutExe = "",
    [ValidateSet("WINDOWS", "CONSOLE")] [string]$Subsystem = "WINDOWS",
    # Leave blank to omit /ENTRY and rely on defaults embedded in objs/libs.
    [string]$EntryPoint = "",
    [switch]$NoDefaultSystemLibs,
    [string[]]$SystemLibs = @(
        "kernel32.lib",
        "user32.lib",
        "gdi32.lib",
        "advapi32.lib",
        "shell32.lib",
        "ole32.lib",
        "oleaut32.lib",
        "comdlg32.lib",
        "comctl32.lib",
        "shlwapi.lib"
    )
)
$ErrorActionPreference = "Stop"

$rootDir = if ($PSScriptRoot) {
    $scriptDir = $PSScriptRoot
    if (Split-Path -Leaf $scriptDir -eq "tools") { Split-Path $scriptDir } else { $scriptDir }
} else { "D:\rawrxd" }

# Source directory: ASM files reside in src\asm
$Src = Join-Path $rootDir "src\asm"
$outDir = Join-Path $rootDir "build_prod"
$genesisDir = Join-Path $rootDir "genesis"

New-Item -ItemType Directory -Force -Path $outDir -ErrorAction SilentlyContinue | Out-Null
New-Item -ItemType Directory -Force -Path $genesisDir -ErrorAction SilentlyContinue | Out-Null

function Resolve-FirstExistingPath {
    param([string[]]$Candidates)
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }
    return $null
}

function Resolve-LinkTool {
    $link = (Get-Command link.exe -ErrorAction SilentlyContinue).Source
    if ($link) { return $link }
    $link = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
    if ($link) { return $link }
    $link = (Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
    return $link
}

function Invoke-LinkOnly {
    param(
        [string]$LinkExe,
        [string[]]$LinkInputs,
        [string]$OutputExe
    )

    if (-not $LinkExe) {
        Write-Host "ERROR: link.exe not found." -ForegroundColor Red
        exit 1
    }

    if (-not $LinkInputs -or $LinkInputs.Count -eq 0) {
        Write-Host "ERROR: No linker inputs provided (.obj/.lib/.res)." -ForegroundColor Red
        exit 1
    }

    $missing = @($LinkInputs | Where-Object { -not (Test-Path $_) })
    if ($missing.Count -gt 0) {
        Write-Host "ERROR: Missing linker inputs:" -ForegroundColor Red
        $missing | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
        exit 1
    }

    $OutputExe = if ($OutputExe) { $OutputExe } else { (Join-Path $outDir "RawrXD.exe") }
    $outDirLocal = Split-Path -Parent $OutputExe
    if ($outDirLocal) {
        New-Item -ItemType Directory -Force -Path $outDirLocal -ErrorAction SilentlyContinue | Out-Null
    }

    $rspPath = Join-Path $outDir "rawrxd_link_only.rsp"

    $args = @(
        "/NOLOGO",
        "/MACHINE:X64",
        "/INCREMENTAL:NO",
        "/OPT:REF",
        "/OPT:ICF",
        "/OUT:$OutputExe",
        "/SUBSYSTEM:$Subsystem"
    )

    if ($EntryPoint) {
        $args += "/ENTRY:$EntryPoint"
    }

    if (-not $NoDefaultSystemLibs) {
        $args += $SystemLibs
    }

    $rspLines = @()
    $rspLines += $args
    $rspLines += $LinkInputs
    Set-Content -Path $rspPath -Value ($rspLines -join "`r`n") -Encoding ASCII

    Write-Host "[LinkOnly] $OutputExe" -ForegroundColor Cyan
    Write-Host "  link: $LinkExe" -ForegroundColor DarkGray
    Write-Host "  rsp:  $rspPath" -ForegroundColor DarkGray
    & $LinkExe "@$rspPath"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: link.exe failed ($LASTEXITCODE)." -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host "OK: Built $OutputExe" -ForegroundColor Green
}

if ($LinkOnly) {
    Write-Host "GENESIS LINK-ONLY" -ForegroundColor Magenta

    $linkExe = Resolve-LinkTool

    if (-not $OutExe) {
        $OutExe = Join-Path $outDir "RawrXD.exe"
    }

    $resolvedInputs = @()
    if ($Inputs -and $Inputs.Count -gt 0) {
        $resolvedInputs = @($Inputs)
    } else {
        if (-not $ObjDir) {
            $ObjDir = Resolve-FirstExistingPath -Candidates @(
                (Join-Path $env:LOCALAPPDATA "RawrXD\build\lib\bin"),
                (Join-Path $env:LOCALAPPDATA "RawrXD\build\lib"),
                (Join-Path $rootDir "build_prod")
            )
        }
        if (-not $ObjDir -or -not (Test-Path $ObjDir)) {
            Write-Host "ERROR: ObjDir not found. Provide -ObjDir or -Inputs." -ForegroundColor Red
            exit 1
        }

        if (-not $LibDir) {
            $LibDir = $ObjDir
        }

        $objs = Get-ChildItem -Path $ObjDir -Filter "*.obj" -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
        if (-not $objs -or $objs.Count -eq 0) {
            Write-Host "ERROR: No .obj files found in ObjDir: $ObjDir" -ForegroundColor Red
            exit 1
        }

        $coreLib = Join-Path $LibDir "rawrxd_core.lib"
        $gpuLib = Join-Path $LibDir "rawrxd_gpu.lib"
        $libs = @()
        if (Test-Path $coreLib) { $libs += $coreLib }
        if (Test-Path $gpuLib) { $libs += $gpuLib }

        $resolvedInputs = @()
        $resolvedInputs += $objs
        $resolvedInputs += $libs

        if ($ResPath) {
            $resolvedInputs += $ResPath
        } else {
            $candidateRes = Join-Path $ObjDir "rawrxd.res"
            if (Test-Path $candidateRes) {
                $resolvedInputs += $candidateRes
            }
        }
    }

    Invoke-LinkOnly -LinkExe $linkExe -LinkInputs $resolvedInputs -OutputExe $OutExe
    exit 0
}

# Locate ML64 / LINK (VS2022 or Build Tools)
$ml64 = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $ml64) {
    $ml64 = (Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
if (-not $ml64) {
    Write-Host "ERROR: ML64 not found. Install VS2022 (or Build Tools) with C++ x64 tools." -ForegroundColor Red
    exit 1
}

$link = Resolve-LinkTool

# Resolve full path to .asm in $Src (src\asm). No optional skip: missing file will cause compile to fail.
function Get-AsmPath {
    param([string]$FileName)
    $path = Join-Path $Src $FileName
    return $path
}

# Invoke ML64; returns $true on success, $false otherwise. Build must fail fast (exit 1) if this returns $false.
function Invoke-Compile {
    param(
        [string]$Source,
        [string]$Object,
        [string]$Defines = ""
    )
    $srcPath = Get-AsmPath -FileName $Source
    Write-Host "[Compile] $Source -> $Object" -ForegroundColor Cyan
    $args = @("/c", "/W3", "/Zd")
    if ($Defines) { $args += $Defines }
    $args += @("/Fo`"$outDir\$Object`"", "`"$srcPath`"")
    & $ml64 $args
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  FAIL: $Source" -ForegroundColor Red
        return $false
    }
    Write-Host "  OK" -ForegroundColor Green
    return $true
}

Write-Host "GENESIS BUILD (strict)" -ForegroundColor Magenta
Write-Host "  Src: $Src" -ForegroundColor DarkGray
Write-Host "  Out: $outDir" -ForegroundColor DarkGray

# 1. tsconfig_runtime.asm (mandatory)
if (!(Invoke-Compile -Source "tsconfig_runtime.asm" -Object "tsconfig_runtime.obj")) { exit 1 }

# 2. win32ide_main.asm (mandatory)
if (!(Invoke-Compile -Source "win32ide_main.asm" -Object "win32ide_main.obj")) { exit 1 }

# 3. agenticide_main.asm (mandatory)
if (!(Invoke-Compile -Source "agenticide_main.asm" -Object "agenticide_main.obj")) { exit 1 }

# 4. Always-Local Extension
if (!(Invoke-Compile -Source "cursor_always_local.asm" -Object "alwayslocal.obj" -Defines "/DALWAYS_LOCAL")) { exit 1 }

# 5. Extension Host Main
if (!(Invoke-Compile -Source "RawrXD_AgentHost.asm" -Object "exthost.obj" -Defines "/DEXTENSION_HOST")) { exit 1 }

Write-Host ""
Write-Host "All mandatory components built successfully." -ForegroundColor Green
