# RawrXD Build & Automatic Wiring Script
# Automates compilation of production components and deployment to C:\RawrXD

param(
    [string]$SourceDir = "D:\RawrXD-Compilers",
    [string]$TargetDir = "C:\RawrXD",
    [switch]$BuildFull,
    [switch]$SkipLink,
    [switch]$IncludeAll,
    [switch]$ContinueOnError
)

$ErrorActionPreference = "Stop"

function Get-ToolPath {
    param([string]$ToolName, [string]$Architecture = "x64")
    $cmd = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    # Check known VS2022 Enterprise installation
    $knownPaths = @(
        "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\$ToolName",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\$ToolName",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\*\bin\Hostx64\x64\$ToolName"
    )
    
    foreach ($pattern in $knownPaths) {
        $resolved = Get-Item $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($resolved) { return $resolved.FullName }
    }

    $vswhere = "$Env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsroot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.${Architecture} -property installationPath 2>$null
        if ($vsroot) {
            $msvcRoot = Join-Path $vsroot "VC\Tools\MSVC"
            $tool = Get-ChildItem -Path $msvcRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
            if ($tool) {
                $bin = Join-Path $tool.FullName "bin\Host${Architecture}\${Architecture}"
                $candidate = Join-Path $bin $ToolName
                if (Test-Path $candidate) { return $candidate }
            }
        }
    }

    throw "Tool '$ToolName' not found. Run from VS Developer PowerShell or add it to PATH."
}

function Invoke-Checked {
    param([string]$Command, [string[]]$Arguments)
    $filteredArgs = $Arguments | Where-Object { $_ -ne $null -and $_ -ne "" }
    Write-Host "[*] $Command $($filteredArgs -join ' ')" -ForegroundColor DarkGray
    & $Command $filteredArgs
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $null) { 
        throw "Command failed with exit code $LASTEXITCODE - $Command" 
    }
}

$ml64 = Get-ToolPath -ToolName "ml64.exe"
$link = Get-ToolPath -ToolName "link.exe"
$lib  = Get-ToolPath -ToolName "lib.exe"

$srcDir = (Resolve-Path $SourceDir).Path
$targetDir = (Resolve-Path $TargetDir).Path
$binDir = Join-Path $srcDir "bin"
$objDir = Join-Path $srcDir "obj"
$libDir = Join-Path $srcDir "lib"

if (!(Test-Path $binDir)) { New-Item -ItemType Directory -Path $binDir | Out-Null }
if (!(Test-Path $objDir)) { New-Item -ItemType Directory -Path $objDir | Out-Null }

Write-Host "`n[1/5] Assembling Production Components..." -ForegroundColor Cyan

$asmFiles = @(
    "instruction_encoder_production.asm",
    "x64_encoder_production.asm",
    "RawrXD_PE_Generator_PROD.asm",
    "rawrxd_pe_generator_encoder.asm",
    "macro_substitution_engine.asm",
    "pe_generator_production.asm",
    "assembler_loop_production.asm",
    "RawrXD_ReverseAssemblerLoop.asm",
    "assembler_core_v3.asm"
)

if ($IncludeAll) {
    # Include all other supporting modules if requested
    $extraAsms = Get-ChildItem -Path $srcDir -Filter "*.asm" | Select-Object -ExpandProperty Name
    foreach ($extra in $extraAsms) {
        if ($asmFiles -notcontains $extra) {
            $asmFiles += $extra
        }
    }
}

foreach ($file in $asmFiles) {
    $fullPath = Join-Path $srcDir $file
    if (!(Test-Path $fullPath)) { 
        Write-Warning "Skipping missing file: $file"
        continue
    }
    $objPath = Join-Path $objDir ($file.Replace(".asm", ".obj"))
    try {
        Invoke-Checked -Command $ml64 -Arguments @("/c","/nologo","/Zi","/W3","/Fo",$objPath,$fullPath)
    } catch {
        if ($ContinueOnError) {
            Write-Warning "Failed to assemble $file - continuing"
            continue
        }
        throw
    }
}

if ($BuildFull) {
    $fullAsm = Join-Path $srcDir "RawrXD_PE_Generator_FULL.asm"
    if (Test-Path $fullAsm) {
        Write-Host "[*] Building FULL PE generator (best-effort)" -ForegroundColor Yellow
        $fullObj = Join-Path $objDir "RawrXD_PE_Generator_FULL.obj"
        try {
                Invoke-Checked -Command $ml64 -Arguments @("/c","/nologo","/Zi","/W3","/Fo",$fullObj,$fullAsm)
        } catch {
            Write-Warning "FULL build failed; continuing with PROD only."
        }
    }
}

Write-Host "`n[2/5] Creating Static Libraries..." -ForegroundColor Cyan

# Create lib directory if needed
if (!(Test-Path $libDir)) { New-Item -ItemType Directory -Path $libDir | Out-Null }

$encoderObj = Join-Path $objDir "instruction_encoder_production.obj"
$x64Obj = Join-Path $objDir "x64_encoder_production.obj"
$peProdObj = Join-Path $objDir "RawrXD_PE_Generator_PROD.obj"
$peAdvObj = Join-Path $objDir "rawrxd_pe_generator_encoder.obj"
$peFullObj = Join-Path $objDir "RawrXD_PE_Generator_FULL.obj"
$macroObj  = Join-Path $objDir "macro_substitution_engine.obj"
$asmLoopObj = Join-Path $objDir "assembler_loop_production.obj"
$asmCoreObj = Join-Path $objDir "assembler_core_v3.obj"

if ((Test-Path $encoderObj) -and (Test-Path $x64Obj)) {
    Invoke-Checked -Command $lib -Arguments @("/NOLOGO","/OUT:$libDir\rawrxd_encoder.lib",
        $encoderObj,
        $x64Obj)
} else {
    Write-Warning "Missing encoder objects; skipping rawrxd_encoder.lib"
}

if (Test-Path $peProdObj) {
    Invoke-Checked -Command $lib -Arguments @("/NOLOGO","/OUT:$libDir\rawrxd_pe_gen.lib",
        $peProdObj)
}

if ((Test-Path $macroObj) -or (Test-Path $asmLoopObj) -or (Test-Path $asmCoreObj)) {
    $asmArgs = @("/NOLOGO","/OUT:$libDir\rawrxd_asm_core.lib")
    if (Test-Path $macroObj) { $asmArgs += $macroObj }
    if (Test-Path $asmLoopObj) { $asmArgs += $asmLoopObj }
    if (Test-Path $asmCoreObj) { $asmArgs += $asmCoreObj }
    Invoke-Checked -Command $lib -Arguments $asmArgs
}

if (Test-Path $peAdvObj) {
    Invoke-Checked -Command $lib -Arguments @("/NOLOGO","/OUT:$libDir\rawrxd_pe_gen_adv.lib",
        $peAdvObj)
}

if (Test-Path $peFullObj) {
    Invoke-Checked -Command $lib -Arguments @("/NOLOGO","/OUT:$libDir\rawrxd_pe_gen_full.lib",
        $peFullObj)
}

if (-not $SkipLink) {
    Write-Host "`n[3/5] Linking Production PE Generator EXE..." -ForegroundColor Cyan

    $msvcLib = Split-Path (Split-Path $link -Parent) -Parent
    $msvcLib = Join-Path $msvcLib "lib\x64"
    $winKitLib = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0"
    $umLib = Join-Path $winKitLib "um\x64"
    $ucrtLib = Join-Path $winKitLib "ucrt\x64"

    $peProdExeObj = Join-Path $objDir "pe_generator_production.obj"
    $peGenLib = Join-Path $libDir "rawrxd_pe_gen.lib"
    $encoderLib = Join-Path $libDir "rawrxd_encoder.lib"

    if ((Test-Path $peProdExeObj) -and (Test-Path $peGenLib) -and (Test-Path $encoderLib)) {
        Invoke-Checked -Command $link -Arguments @(
            "/NOLOGO",
            "/SUBSYSTEM:CONSOLE",
            "/ENTRY:main",
            "/OUT:$binDir\pe_generator.exe",
            $peProdExeObj,
            $peGenLib,
            $encoderLib,
            "/LIBPATH:$msvcLib",
            "/LIBPATH:$umLib",
            "/LIBPATH:$ucrtLib",
            "kernel32.lib",
            "ntdll.lib",
            "user32.lib"
        )
    } else {
        Write-Warning "Missing inputs for pe_generator.exe; skipping link"
    }
}

Write-Host "`n[4/5] Automatic Wiring to C:\RawrXD..." -ForegroundColor Cyan

$wiringManifest = @(
    @{ Src = "$libDir\rawrxd_encoder.lib"; Dest = "$targetDir\Libraries\rawrxd_encoder.lib" },
    @{ Src = "$libDir\rawrxd_pe_gen.lib";   Dest = "$targetDir\Libraries\rawrxd_pe_gen.lib" },
    @{ Src = "$libDir\rawrxd_pe_gen_adv.lib"; Dest = "$targetDir\Libraries\rawrxd_pe_gen_adv.lib" },
    @{ Src = "$libDir\rawrxd_asm_core.lib"; Dest = "$targetDir\Libraries\rawrxd_asm_core.lib" },
    @{ Src = "$srcDir\RawrXD_PE_Generator.h"; Dest = "$targetDir\Headers\RawrXD_PE_Generator.h" },
    @{ Src = "$srcDir\pe_generator.h";        Dest = "$targetDir\Headers\pe_generator.h" },
    @{ Src = "$binDir\pe_generator.exe";      Dest = "$targetDir\pe_generator.exe" },
    @{ Src = "$srcDir\PeGen_Examples.cpp";    Dest = "$targetDir\PeGen_Examples.cpp" }
)

foreach ($item in $wiringManifest) {
    $src = $item.Src
    $dest = $item.Dest

    if (Test-Path $src) {
        # Ensure destination directory exists
        $destDir = Split-Path $dest -Parent
        if (!(Test-Path $destDir)) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }

        Copy-Item $src $dest -Force
        Write-Host "   ✓ Wired $src to $dest" -ForegroundColor Green
    } else {
        Write-Warning "Source item $src not found; skipping wiring."
    }
}

Write-Host "`n[5/5] Verification..." -ForegroundColor Cyan

$checks = @(
    "Libraries\rawrxd_pe_gen.lib",
    "Libraries\rawrxd_pe_gen_adv.lib",
    "Libraries\rawrxd_encoder.lib",
    "Headers\RawrXD_PE_Generator.h",
    "pe_generator.exe"
)

foreach ($check in $checks) {
    $p = Join-Path $targetDir $check
    if (Test-Path $p) {
        $size = (Get-Item $p).Length / 1KB
        Write-Host "   ✓ $check exists ($($size.ToString('F2')) KB)" -ForegroundColor Green
    } else {
        Write-Error "   ✗ $check missing!"
    }
}

Write-Host "`n✨ RawrXD TOOLCHAIN BUILT AND WIRED SUCCESSFULLY! ✨" -ForegroundColor Green
Write-Host "Ready for integration into the IDE.`n"
