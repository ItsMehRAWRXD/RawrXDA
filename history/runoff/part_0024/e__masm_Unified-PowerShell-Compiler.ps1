param(
  [Parameter(Mandatory=$true)][string]$Source,
  [ValidateSet('masm','nasm')][string]$Tool = 'masm',
  [ValidateSet('windows','console')][string]$SubSystem = 'console',
  [string]$OutDir = "$PSScriptRoot/bin",
  [string]$Entry = '',
  [string]$Runtime
)

$ErrorActionPreference = 'Stop'

function Resolve-MSVC {
  $msvcRoot = 'C:\VS2022Enterprise\VC\Tools\MSVC'
  if (-not (Test-Path $msvcRoot)) { throw "MSVC root not found at $msvcRoot" }
  $msvcVer = Get-ChildItem -Path $msvcRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
  if (-not $msvcVer) { throw "No MSVC versions under $msvcRoot" }
  $bin = Join-Path $msvcVer.FullName 'bin\Hostx64\x64'
  $lib = Join-Path $msvcVer.FullName 'lib\x64'
  $tools = [pscustomobject]@{ ml64 = Join-Path $bin 'ml64.exe'; link = Join-Path $bin 'link.exe'; lib = $lib }
  if (-not (Test-Path $tools.ml64) -or -not (Test-Path $tools.link)) { throw "Missing ml64/link in $bin" }
  return $tools
}

function Resolve-Kits {
  $root = 'C:\Program Files (x86)\Windows Kits\10\Lib'
  if (-not (Test-Path $root)) { throw "Windows Kits lib root not found at $root" }
  $ver = Get-ChildItem -Path $root -Directory | Sort-Object Name -Descending | Select-Object -First 1
  if (-not $ver) { throw "No Windows Kits versions under $root" }
  $ucrt = Join-Path $ver.FullName 'ucrt\x64'
  $um = Join-Path $ver.FullName 'um\x64'
  if (-not (Test-Path $ucrt) -or -not (Test-Path $um)) { throw "Missing ucrt/um lib paths in $($ver.FullName)" }
  return [pscustomobject]@{ ucrt = $ucrt; um = $um }
}

function Ensure-OutDir($dir){ if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null } }

function Compile-MASM($src,$outDir){
  param($Runtime)
  $tools = Resolve-MSVC; $kits = Resolve-Kits
  Ensure-OutDir $outDir
  $obj = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($src) + '.obj')
  $exe = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($src) + '.exe')
  $mlCmd = '"{0}" /c /nologo /Fo "{1}" "{2}"' -f $tools.ml64, $obj, $src
  cmd /c $mlCmd; if ($LASTEXITCODE -ne 0) { throw "ml64 compile failed" }
  $entry = if ($Entry) { $Entry } else { 'WinMain' }
  $sub = if ($SubSystem -eq 'windows') { 'windows' } else { 'console' }
  
  # Build link command with optional runtime
  $linkObjs = "\"$obj\""
  if ($Runtime -and (Test-Path $Runtime)) {
    $linkObjs = "\"$Runtime\" $linkObjs"
  }
  
  $linkCmd = '"{0}" /nologo /subsystem:{1} /entry:{2} {3} /LIBPATH:"{4}" /LIBPATH:"{5}" /LIBPATH:"{6}" kernel32.lib user32.lib gdi32.lib comdlg32.lib comctl32.lib /out:"{7}"' -f $tools.link, $sub, $entry, $linkObjs, $tools.lib, $kits.ucrt, $kits.um, $exe
  cmd /c $linkCmd; if ($LASTEXITCODE -ne 0) { throw "link failed" }
  return $exe
}

function Compile-NASM($src,$outDir){
  param($Runtime)
  $tools = Resolve-MSVC; $kits = Resolve-Kits
  Ensure-OutDir $outDir
  $nasm = 'E:\nasm\nasm-2.16.01\nasm.exe'
  if (-not (Test-Path $nasm)) { throw "NASM not found at $nasm" }
  $obj = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($src) + '.obj')
  $exe = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($src) + '.exe')
  $fmt = 'win64'
  $ncmd = '"{0}" -f {1} "{2}" -o "{3}"' -f $nasm, $fmt, $src, $obj
  cmd /c $ncmd; if ($LASTEXITCODE -ne 0) { throw "nasm assemble failed" }
  $entry = if ($Entry) { $Entry } else { 'start' }
  $sub = if ($SubSystem -eq 'windows') { 'windows' } else { 'console' }
  
  # Build link command with optional runtime
  $linkObjs = "\"$obj\""
  if ($Runtime -and (Test-Path $Runtime)) {
    $linkObjs = "\"$Runtime\" $linkObjs"
  }
  
  $linkCmd = '"{0}" /nologo /subsystem:{1} /entry:{2} {3} /LIBPATH:"{4}" /LIBPATH:"{5}" /LIBPATH:"{6}" kernel32.lib user32.lib /out:"{7}"' -f $tools.link, $sub, $entry, $linkObjs, $tools.lib, $kits.ucrt, $kits.um, $exe
  cmd /c $linkCmd; if ($LASTEXITCODE -ne 0) { throw "link failed" }
  return $exe
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()
try {
  $fullSrc = (Resolve-Path $Source).Path
  Write-Host ("[Compiler] Tool={0} SubSystem={1} Source={2}" -f $Tool,$SubSystem,$fullSrc) -ForegroundColor Cyan
  $result = if ($Tool -eq 'masm') { Compile-MASM $fullSrc $OutDir -Runtime $Runtime } else { Compile-NASM $fullSrc $OutDir -Runtime $Runtime }
  $sw.Stop()
  Write-Host ("[OK] Built: {0} in {1} ms" -f $result, $sw.ElapsedMilliseconds) -ForegroundColor Green
} catch {
  $sw.Stop(); Write-Host ("[ERR] {0}" -f $_) -ForegroundColor Red; exit 1
}
