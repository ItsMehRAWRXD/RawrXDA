# Unified MASM/NASM compiler for RawrXD IDE.
param(
  [Parameter(Mandatory = $true)][string]$Source,
  [ValidateSet('masm', 'nasm')][string]$Tool = 'masm',
  [ValidateSet('x64', 'x86')][string]$Architecture = 'x64',
  [ValidateSet('windows', 'console')][string]$SubSystem = 'console',
  [ValidateSet('exe', 'dll')][string]$OutputType = 'exe',
  [string]$OutDir = "",
  [string]$Entry = "",
  [string]$Runtime,
  [switch]$UseExternalToolchain
)

$ErrorActionPreference = 'Stop'
$ScriptDir = $PSScriptRoot
$RawrXD = if ($env:RAWRXD_ROOT) { $env:RAWRXD_ROOT } else { (Resolve-Path (Join-Path $ScriptDir "..\..")).Path }
if (-not $OutDir) { $OutDir = Join-Path $ScriptDir "bin\$Architecture" }

function Resolve-NASM {
  $candidates = @(
    (Join-Path $RawrXD "toolchain\nasm\nasm.exe"),
    (Join-Path $RawrXD "compilers\nasm\nasm-2.16.01\nasm.exe"),
    "D:\RawrXD\toolchain\nasm\nasm.exe",
    "D:\RawrXD\compilers\nasm\nasm-2.16.01\nasm.exe",
    "E:\nasm\nasm-2.16.01\nasm.exe"
  )
  foreach ($path in $candidates) {
    if ($path -and (Test-Path $path)) { return (Resolve-Path $path).Path }
  }
  $cmd = Get-Command nasm -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  throw "NASM not found. Checked: $($candidates -join ', ') and PATH."
}

function Resolve-MSVC {
  param([ValidateSet('x64', 'x86')][string]$Arch)

  $roots = @(
    'D:\VS2022Enterprise\VC\Tools\MSVC',
    'C:\VS2022Enterprise\VC\Tools\MSVC',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC'
  )

  $msvcRoot = $null
  foreach ($r in $roots) {
    if (Test-Path $r) { $msvcRoot = $r; break }
  }
  if (-not $msvcRoot) { throw "MSVC root not found." }

  $msvcVer = Get-ChildItem -Path $msvcRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
  if (-not $msvcVer) { throw "No MSVC versions under $msvcRoot" }

  $target = if ($Arch -eq 'x64') { 'x64' } else { 'x86' }
  $bin = Join-Path $msvcVer.FullName "bin\Hostx64\$target"
  $lib = Join-Path $msvcVer.FullName "lib\$target"
  $mlName = if ($Arch -eq 'x64') { 'ml64.exe' } else { 'ml.exe' }
  $ml = Join-Path $bin $mlName
  $link = Join-Path $bin 'link.exe'

  if (-not (Test-Path $ml) -or -not (Test-Path $link)) { throw "Missing $mlName/link in $bin" }
  return [pscustomobject]@{ ml = $ml; link = $link; lib = $lib }
}

function Resolve-Kits {
  param([ValidateSet('x64', 'x86')][string]$Arch)

  $roots = @(
    'C:\Program Files (x86)\Windows Kits\10\Lib',
    'D:\Program Files (x86)\Windows Kits\10\Lib'
  )
  $root = $null
  foreach ($r in $roots) {
    if (Test-Path $r) { $root = $r; break }
  }
  if (-not $root) { throw "Windows Kits lib root not found." }

  $archLeaf = if ($Arch -eq 'x64') { 'x64' } else { 'x86' }
  $versions = Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
  foreach ($ver in $versions) {
    $ucrt = Join-Path $ver.FullName "ucrt\$archLeaf"
    $um = Join-Path $ver.FullName "um\$archLeaf"
    if ((Test-Path $ucrt) -and (Test-Path $um)) {
      return [pscustomobject]@{ ucrt = $ucrt; um = $um; version = $ver.Name }
    }
  }
  throw "No Windows Kits version with ucrt+um for $Arch under $root"
}

function Resolve-RawrXDTools {
  $asm = Join-Path $RawrXD "toolchain\from_scratch\phase1_assembler\build\rawrxd_asm.exe"
  $linker = Join-Path $RawrXD "tools\inhouse\linker\bin\Debug\net8.0\rawrxd_linker.exe"
  
  if (-not (Test-Path $asm)) { Write-Warning "rawrxd_asm not found at $asm"; return $null }
  if (-not (Test-Path $linker)) { Write-Warning "rawrxd_linker not found at $linker"; return $null }
  return [pscustomobject]@{ asm = $asm; linker = $linker }
}

function Ensure-OutDir([string]$Dir) {
  if (-not (Test-Path $Dir)) { New-Item -ItemType Directory -Path $Dir -Force | Out-Null }
}

function Get-OutputPath([string]$Src, [string]$Dir, [string]$Kind) {
  $base = [IO.Path]::GetFileNameWithoutExtension($Src)
  $ext = if ($Kind -eq 'dll') { '.dll' } else { '.exe' }
  return (Join-Path $Dir ($base + $ext))
}

function Resolve-EntryDefault {
  if ($Entry) { return $Entry }
  if ($OutputType -eq 'dll') { return 'DllMain' }
  if ($Tool -eq 'nasm') { return 'start' }
  return 'WinMain'
}

function Normalize-Entry([string]$Symbol) {
  if ($Architecture -eq 'x86' -and $Tool -eq 'nasm' -and $Symbol.StartsWith('_')) {
    return $Symbol.TrimStart('_')
  }
  return $Symbol
}

function Compile-RawrXD {
  param([string]$Src, [string]$OutputDir)

  $rawrxdTools = Resolve-RawrXDTools
  if (-not $rawrxdTools) { throw "Fallback toggle not set, but RawrXD internal tools missing. Use -UseExternalToolchain or compile them." }
  
  $msvc = Resolve-MSVC -Arch $Architecture
  $kits = Resolve-Kits -Arch $Architecture
  
  Ensure-OutDir $OutputDir

  $obj = Join-Path $OutputDir ([IO.Path]::GetFileNameWithoutExtension($Src) + '.obj')
  $outFile = Get-OutputPath -Src $Src -Dir $OutputDir -Kind $OutputType

  $asmArgs = @($Src, "-o", $obj)
  $null = & $rawrxdTools.asm @asmArgs
  if ($LASTEXITCODE -ne 0) { throw "RawrXD Assembler failed for $Src" }

  $machine = if ($Architecture -eq 'x64') { '/MACHINE:X64' } else { '/MACHINE:X86' }
  $entry = Normalize-Entry (Resolve-EntryDefault)
  
  $linkArgs = @('/nologo', $machine, "/subsystem:$SubSystem")
  if ($OutputType -eq 'dll') {
    $linkArgs += '/DLL'
    if ($Entry) { $linkArgs += "/entry:$entry" } else { $linkArgs += '/NOENTRY' }
  } else {
    $linkArgs += "/entry:$entry"
  }

  if ($Runtime -and (Test-Path $Runtime)) { $linkArgs += (Resolve-Path $Runtime).Path }
  
  $linkArgs += $obj
  $linkArgs += @("/LIBPATH:$($msvc.lib)", "/LIBPATH:$($kits.ucrt)", "/LIBPATH:$($kits.um)")
  $linkArgs += @('kernel32.lib', 'user32.lib', 'gdi32.lib', 'comdlg32.lib', 'comctl32.lib')
  $linkArgs += "/out:$outFile"

  $null = & $rawrxdTools.linker @linkArgs
  if ($LASTEXITCODE -ne 0) { throw "RawrXD Linker failed for $Src" }
  return $outFile
}

function Compile-MASM {
  param([string]$Src, [string]$OutputDir)

  $tools = Resolve-MSVC -Arch $Architecture
  $kits = Resolve-Kits -Arch $Architecture
  Ensure-OutDir $OutputDir

  $obj = Join-Path $OutputDir ([IO.Path]::GetFileNameWithoutExtension($Src) + '.obj')
  $outFile = Get-OutputPath -Src $Src -Dir $OutputDir -Kind $OutputType
  $mlArgs = @('/c', '/nologo')
  if ($Architecture -eq 'x86') { $mlArgs += '/coff' }
  $mlArgs += @("/Fo$obj", $Src)
  $null = & $tools.ml @mlArgs
  if ($LASTEXITCODE -ne 0) { throw "MASM assemble failed for $Src" }

  $machine = if ($Architecture -eq 'x64') { '/MACHINE:X64' } else { '/MACHINE:X86' }
  $entry = Normalize-Entry (Resolve-EntryDefault)
  $linkArgs = @('/nologo', $machine, "/subsystem:$SubSystem")
  if ($OutputType -eq 'dll') {
    $linkArgs += '/DLL'
    if ($Entry) { $linkArgs += "/entry:$entry" } else { $linkArgs += '/NOENTRY' }
  } else {
    $linkArgs += "/entry:$entry"
  }

  if ($Runtime -and (Test-Path $Runtime)) { $linkArgs += (Resolve-Path $Runtime).Path }
  $linkArgs += $obj
  $linkArgs += @("/LIBPATH:$($tools.lib)", "/LIBPATH:$($kits.ucrt)", "/LIBPATH:$($kits.um)")
  $linkArgs += @('kernel32.lib', 'user32.lib', 'gdi32.lib', 'comdlg32.lib', 'comctl32.lib')
  $linkArgs += "/out:$outFile"

  $null = & $tools.link @linkArgs
  if ($LASTEXITCODE -ne 0) { throw "Link failed for $Src" }
  return $outFile
}

function Compile-NASM {
  param([string]$Src, [string]$OutputDir)

  $tools = Resolve-MSVC -Arch $Architecture
  $kits = Resolve-Kits -Arch $Architecture
  $nasmExe = Resolve-NASM
  Ensure-OutDir $OutputDir

  $obj = Join-Path $OutputDir ([IO.Path]::GetFileNameWithoutExtension($Src) + '.obj')
  $outFile = Get-OutputPath -Src $Src -Dir $OutputDir -Kind $OutputType
  $fmt = if ($Architecture -eq 'x64') { 'win64' } else { 'win32' }
  $null = & $nasmExe '-f' $fmt $Src '-o' $obj
  if ($LASTEXITCODE -ne 0) { throw "NASM assemble failed for $Src" }

  $machine = if ($Architecture -eq 'x64') { '/MACHINE:X64' } else { '/MACHINE:X86' }
  $entry = Normalize-Entry (Resolve-EntryDefault)
  $linkArgs = @('/nologo', $machine, "/subsystem:$SubSystem")
  if ($OutputType -eq 'dll') {
    $linkArgs += '/DLL'
    if ($Entry) { $linkArgs += "/entry:$entry" } else { $linkArgs += '/NOENTRY' }
  } else {
    $linkArgs += "/entry:$entry"
  }

  if ($Runtime -and (Test-Path $Runtime)) { $linkArgs += (Resolve-Path $Runtime).Path }
  $linkArgs += $obj
  $linkArgs += @("/LIBPATH:$($tools.lib)", "/LIBPATH:$($kits.ucrt)", "/LIBPATH:$($kits.um)")
  $linkArgs += @('kernel32.lib', 'user32.lib')
  $linkArgs += "/out:$outFile"

  $null = & $tools.link @linkArgs
  if ($LASTEXITCODE -ne 0) { throw "Link failed for $Src" }
  return $outFile
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()
try {
  $fullSrc = (Resolve-Path $Source).Path
  $mode = if ($UseExternalToolchain) { "External ($Tool)" } else { "RawrXD Internal" }
  Write-Host ("[Compiler] Mode={0} Arch={1} Type={2} SubSystem={3} Source={4}" -f $mode, $Architecture, $OutputType, $SubSystem, $fullSrc) -ForegroundColor Cyan
  
  $result = if (-not $UseExternalToolchain) {
    Compile-RawrXD -Src $fullSrc -OutputDir $OutDir
  } elseif ($Tool -eq 'masm') {
    Compile-MASM -Src $fullSrc -OutputDir $OutDir
  } else {
    Compile-NASM -Src $fullSrc -OutputDir $OutDir
  }
  
  $sw.Stop()
  Write-Host ("[OK] Built: {0} in {1} ms" -f $result, $sw.ElapsedMilliseconds) -ForegroundColor Green
  Write-Output $result
} catch {
  $sw.Stop()
  Write-Host ("[ERR] {0}" -f $_) -ForegroundColor Red
  exit 1
}
