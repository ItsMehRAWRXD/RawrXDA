<#
.SYNOPSIS
    Migrates compiler/linker assets from E: drive to D:\RawrXD (Fortress).
.DESCRIPTION
    Brings back MASM IDE, NASM, assembly sources, DirectX DLLs, build scripts,
    and optional C++ sources from E: into D:\RawrXD\compilers. Updates path
    references so the IDE is fully self-contained on D:.
.NOTES
    Run from RawrXD workspace root or D:\RawrXD.
    E: drive must be accessible.
#>

param(
    [switch]$IncludeCpp,      # Also migrate E:\ root C++ sources
    [switch]$IncludeBuildScripts,  # Migrate build_cli_full.bat, Build-Enterprise-IDE.ps1, etc. (default: yes)
    [switch]$WhatIf           # Dry run only
)

$ErrorActionPreference = 'Stop'
$RawrXD = 'D:\RawrXD'
$E = 'E:\'

function Ensure-Dir { param([string]$Path)
    if (-not (Test-Path $Path)) {
        if (-not $WhatIf) { New-Item -ItemType Directory -Path $Path -Force | Out-Null }
        Write-Host "  mkdir: $Path" -ForegroundColor DarkGray
    }
}

function Copy-ItemSafe { param($Src, $Dst)
    if (-not (Test-Path $Src)) { Write-Warning "Skip (not found): $Src"; return }
    $parent = Split-Path $Dst -Parent
    Ensure-Dir $parent
    if (-not $WhatIf) {
        Copy-Item -Path $Src -Destination $Dst -Force -ErrorAction SilentlyContinue
        if (Test-Path $Dst) { Write-Host "  OK: $Dst" -ForegroundColor Green } else { Write-Warning "  FAIL: $Src -> $Dst" }
    } else { Write-Host "  would copy: $Src -> $Dst" -ForegroundColor Cyan }
}

Write-Host "`n=== E -> RawrXD Fortress Migration ===" -ForegroundColor Cyan
Write-Host "Target: $RawrXD"
if ($WhatIf) { Write-Host "(WhatIf - no changes)" -ForegroundColor Yellow }
Write-Host ""

# 1. MASM IDE
$masmSrc = Join-Path $E 'masm'
$masmDst = Join-Path $RawrXD 'compilers\masm_ide'
if (Test-Path $masmSrc) {
    Write-Host "[1] Migrating E:\masm -> compilers\masm_ide" -ForegroundColor Yellow
    Get-ChildItem $masmSrc -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
        $rel = $_.FullName.Substring($masmSrc.Length).TrimStart('\')
        Copy-ItemSafe $_.FullName (Join-Path $masmDst $rel)
    }
} else { Write-Warning "E:\masm not found" }

# 2. NASM
$nasmSrc = Join-Path $E 'nasm'
$nasmDst = Join-Path $RawrXD 'compilers\nasm'
if (Test-Path $nasmSrc) {
    Write-Host "`n[2] Migrating E:\nasm -> compilers\nasm" -ForegroundColor Yellow
    Get-ChildItem $nasmSrc -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
        $rel = $_.FullName.Substring($nasmSrc.Length).TrimStart('\')
        Copy-ItemSafe $_.FullName (Join-Path $nasmDst $rel)
    }
} else { Write-Warning "E:\nasm not found" }

# 3. Root .asm files
$asmDst = Join-Path $RawrXD 'compilers\assembly_source'
Ensure-Dir $asmDst
Write-Host "`n[3] Migrating E:\*.asm -> compilers\assembly_source" -ForegroundColor Yellow
Get-ChildItem -Path $E -Filter '*.asm' -File -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-ItemSafe $_.FullName (Join-Path $asmDst $_.Name)
}

# 4. Build scripts
$buildDst = Join-Path $RawrXD 'compilers\build_scripts'
Ensure-Dir $buildDst
Write-Host "`n[4] Migrating build scripts" -ForegroundColor Yellow
Copy-ItemSafe (Join-Path $E 'compile_ultimate_ide.bat') (Join-Path $asmDst 'compile_ultimate_ide.bat')
Copy-ItemSafe (Join-Path $E 'compile_ultimate_ide.bat') (Join-Path $buildDst 'compile_ultimate_ide.bat')
if ($IncludeBuildScripts) {
    foreach ($f in 'build_cli_full.bat','test_cli_system.bat','Build-Enterprise-IDE.ps1','build-orchestra.ps1','build.bat') {
        Copy-ItemSafe (Join-Path $E $f) (Join-Path $buildDst $f)
    }
}

# 5. DirectX compiler DLLs (optional)
$dxDst = Join-Path $RawrXD 'compilers\directx'
foreach ($dll in 'dxcompiler.dll','dxil.dll') {
    $p = Join-Path $E $dll
    if (Test-Path $p) { Copy-ItemSafe $p (Join-Path $dxDst $dll) }
}

# 6. Optional C++ sources from E:\ root
if ($IncludeCpp) {
    $cppDst = Join-Path $RawrXD 'compilers\cplusplus_source'
    Ensure-Dir $cppDst
    Write-Host "`n[6] Migrating E:\ root C++ sources -> compilers\cplusplus_source" -ForegroundColor Yellow
    $cppExt = @('*.cpp','*.c','*.h','*.hpp')
    foreach ($ext in $cppExt) {
        Get-ChildItem -Path $E -Filter $ext -File -ErrorAction SilentlyContinue | ForEach-Object {
            Copy-ItemSafe $_.FullName (Join-Path $cppDst $_.Name)
        }
    }
}

# 7. Update Unified-PowerShell-Compiler-Working.ps1 NASM path for D:\RawrXD
$ps1 = Join-Path $masmDst 'Unified-PowerShell-Compiler-Working.ps1'
if (Test-Path $ps1) {
    Write-Host "`n[7] Updating NASM path in Unified-PowerShell-Compiler-Working.ps1" -ForegroundColor Yellow
    if (-not $WhatIf) {
        $c = Get-Content $ps1 -Raw
        $newNasm = "`$nasm = '$RawrXD\compilers\nasm\nasm-2.16.01\nasm.exe'"
        $c = $c -replace "\`$nasm = 'E:\\nasm[^']*'", $newNasm
        $c = $c -replace "`$nasm = 'E:\\nasm[^']*'", $newNasm
        Set-Content -Path $ps1 -Value $c -NoNewline
        Write-Host "  OK: NASM path set to D:\RawrXD" -ForegroundColor Green
    } else { Write-Host "  would update NASM path in $ps1" -ForegroundColor Cyan }
}

# 8. Update compile_ultimate_ide.bat to use Fortress NASM path
$nasmFortress = Join-Path $RawrXD 'compilers\nasm\nasm-2.16.01\nasm.exe'
foreach ($batDir in @($asmDst, $buildDst)) {
    $batPath = Join-Path $batDir 'compile_ultimate_ide.bat'
    if ((Test-Path $batPath) -and -not $WhatIf) {
        $bat = Get-Content $batPath -Raw
        $bat = $bat -replace 'nasm\\nasm-2\.16\.01\\nasm\.exe', $nasmFortress
        Set-Content -Path $batPath -Value $bat -NoNewline
        Write-Host "  OK: $batPath NASM path -> Fortress" -ForegroundColor Green
    }
}

Write-Host "`n=== Migration complete ===" -ForegroundColor Cyan
Write-Host "Verify: D:\RawrXD\compilers\" -ForegroundColor Gray
Write-Host "  - compilers\masm_ide\Unified-PowerShell-Compiler-Working.ps1" -ForegroundColor Gray
Write-Host "  - compilers\nasm\nasm-2.16.01\nasm.exe" -ForegroundColor Gray
Write-Host "  - compilers\assembly_source\*.asm" -ForegroundColor Gray
