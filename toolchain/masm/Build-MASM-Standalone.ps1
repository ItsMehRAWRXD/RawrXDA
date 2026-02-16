<#
.SYNOPSIS
    Standalone MASM builder: compile and link one or more .asm files to .exe or .dll (x64/x86).
.DESCRIPTION
    Single entry point for the RawrXD MASM toolchain. Uses Unified-PowerShell-Compiler-RawrXD.ps1
    for single-file builds, or assembles + links multiple .asm files for the chosen architecture.
    Requires: Visual Studio 2022 (or Build Tools) with MASM + Windows SDK. No E: drive dependency.
.PARAMETER Source
    Single .asm file, or directory containing .asm files (all will be assembled and linked).
.PARAMETER Architecture
    x64 (64-bit) or x86 (32-bit). x32 means 32-bit: use x86.
.PARAMETER OutputType
    exe or dll.
.PARAMETER OutDir
    Output directory. Default: toolchain\masm\bin\<arch>.
.PARAMETER SubSystem
    console or windows.
.PARAMETER Entry
    Entry point (default: WinMain for exe, DllMain for dll).
.PARAMETER Clean
    Remove OutDir before build.
.EXAMPLE
    .\Build-MASM-Standalone.ps1 -Source samples\hello_masm.asm -Architecture x64
    .\Build-MASM-Standalone.ps1 -Source src\masm -Architecture x86 -OutputType dll -OutDir build\x86
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Source,
    [ValidateSet('x64','x86')]
    [string]$Architecture = 'x64',
    [ValidateSet('exe','dll')]
    [string]$OutputType = 'exe',
    [string]$OutDir = "",
    [ValidateSet('console','windows')]
    [string]$SubSystem = 'console',
    [string]$Entry = '',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$ScriptDir = $PSScriptRoot
$RawrXD = if ($env:RAWRXD_ROOT) { $env:RAWRXD_ROOT } else { (Resolve-Path (Join-Path $ScriptDir "..\..")).Path }
if (-not $OutDir) { $OutDir = Join-Path $ScriptDir "bin\$Architecture" }

$UnifiedScript = Join-Path $ScriptDir "Unified-PowerShell-Compiler-RawrXD.ps1"
if (-not (Test-Path $UnifiedScript)) { throw "Unified-PowerShell-Compiler-RawrXD.ps1 not found at $UnifiedScript" }

if ($Clean) {
    if (Test-Path $OutDir) {
        Remove-Item -Path $OutDir -Recurse -Force
        Write-Host "Cleaned $OutDir"
    }
    exit 0
}

# Resolve source: single file or directory of .asm
$sources = @()
if (Test-Path $Source -PathType Leaf) {
    if ([IO.Path]::GetExtension($Source) -ne '.asm') { throw "Single source must be .asm: $Source" }
    $sources = @((Resolve-Path $Source).Path)
} elseif (Test-Path $Source -PathType Container) {
    $sources = Get-ChildItem -Path $Source -Filter "*.asm" -File -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
    if ($sources.Count -eq 0) { throw "No .asm files in $Source" }
} else {
    throw "Source not found: $Source"
}

# Single file: delegate to unified compiler
if ($sources.Count -eq 1) {
    $entryParam = if ($Entry) { "-Entry $Entry" } else { "" }
    $args = @(
        '-Source', $sources[0],
        '-Tool', 'masm',
        '-Architecture', $Architecture,
        '-SubSystem', $SubSystem,
        '-OutDir', $OutDir,
        '-OutputType', $OutputType
    )
    if ($Entry) { $args += '-Entry'; $args += $Entry }
    & $UnifiedScript @args
    exit $LASTEXITCODE
}

# Multi-file: resolve MSVC/Kits and build manually (same logic as unified but multiple objs)
function Find-MSVCTools {
    param([string]$Arch)
    $roots = @(
        'D:\VS2022Enterprise\VC\Tools\MSVC',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC'
    )
    $msvcRoot = $null
    foreach ($r in $roots) { if (Test-Path $r) { $msvcRoot = $r; break } }
    if (-not $msvcRoot) {
        $d = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($d) { $msvcRoot = Join-Path $d.FullName "VC\Tools\MSVC" }
    }
    if (-not $msvcRoot -or -not (Test-Path $msvcRoot)) { throw "MSVC not found" }
    $ver = Get-ChildItem $msvcRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
    $hostArch = if ($Arch -eq 'x64') { 'x64' } else { 'x86' }
    $bin = Join-Path $ver.FullName "bin\Hostx64\$hostArch"
    $lib = Join-Path $ver.FullName "lib\$hostArch"
    $asm = if ($Arch -eq 'x64') { 'ml64.exe' } else { 'ml.exe' }
    $ml = Join-Path $bin $asm
    $link = Join-Path $bin 'link.exe'
    if (-not (Test-Path $ml) -or -not (Test-Path $link)) { throw "ml/link not in $bin" }
    return @{ ml = $ml; link = $link; lib = $lib }
}

function Find-Kits {
    param([string]$Arch)
    $root = 'C:\Program Files (x86)\Windows Kits\10\Lib'
    if (-not (Test-Path $root)) { $root = 'D:\Program Files (x86)\Windows Kits\10\Lib' }
    if (-not (Test-Path $root)) { throw "Windows Kits not found" }
    $ver = Get-ChildItem $root -Directory | Sort-Object Name -Descending | Select-Object -First 1
    $a = if ($Arch -eq 'x64') { 'x64' } else { 'x86' }
    return @{
        ucrt = (Join-Path $ver.FullName "ucrt\$a")
        um   = (Join-Path $ver.FullName "um\$a")
    }
}

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$tools = Find-MSVCTools -Arch $Architecture
$kits = Find-Kits -Arch $Architecture
$objs = @()
foreach ($src in $sources) {
    $base = [IO.Path]::GetFileNameWithoutExtension($src)
    $obj = Join-Path $OutDir ($base + '.obj')
    Write-Host "Assembling $base ..."
    & $tools.ml /c /nologo /Fo $obj $src
    if ($LASTEXITCODE -ne 0) { throw "ml failed: $src" }
    $objs += $obj
}

$outName = if ($OutputType -eq 'dll') {
    $n = if ($sources.Count -eq 1) { [IO.Path]::GetFileNameWithoutExtension($sources[0]) } else { "Output" }
    Join-Path $OutDir ($n + '.dll')
} else {
    $n = if ($sources.Count -eq 1) { [IO.Path]::GetFileNameWithoutExtension($sources[0]) } else { "Output" }
    Join-Path $OutDir ($n + '.exe')
}

$entry = if ($Entry) { $Entry } else { if ($OutputType -eq 'dll') { 'DllMain' } else { 'WinMain' } }
$sub = $SubSystem
$libPaths = "/LIBPATH:`"$($tools.lib)`" /LIBPATH:`"$($kits.ucrt)`" /LIBPATH:`"$($kits.um)`""
$libs = "kernel32.lib user32.lib gdi32.lib comdlg32.lib comctl32.lib"
$objStr = ($objs | ForEach-Object { "`"$_`"" }) -join ' '

if ($OutputType -eq 'dll') {
    & $tools.link /nologo /DLL /NOENTRY /subsystem:$sub $objStr $libPaths $libs "/OUT:$outName"
} else {
    & $tools.link /nologo /LARGEADDRESSAWARE:NO /subsystem:$sub /entry:$entry $objStr $libPaths $libs "/OUT:$outName"
}
if ($LASTEXITCODE -ne 0) { throw "link failed" }

Write-Host "[OK] Built: $outName" -ForegroundColor Green
Write-Output $outName
