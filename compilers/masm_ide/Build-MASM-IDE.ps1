param(
  [string]$Configuration = "Release",
  [switch]$Clean
)

Write-Host "== MASM64 Build (GUI skeleton) ==" -ForegroundColor Cyan

$ErrorActionPreference = "Stop"

# Resolve MSVC and Windows Kits paths (fallback to C:\VS2022Enterprise)
$msvcRoot = "C:\VS2022Enterprise\VC\Tools\MSVC"
if (-not (Test-Path $msvcRoot)) { Write-Host "MSVC root not found at $msvcRoot" -ForegroundColor Red; exit 1 }
$msvcVer = Get-ChildItem -Path $msvcRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $msvcVer) { Write-Host "No MSVC versions found under $msvcRoot" -ForegroundColor Red; exit 1 }
$msvcBin = Join-Path $msvcVer.FullName "bin\Hostx64\x64"
$msvcLib = Join-Path $msvcVer.FullName "lib\x64"
$ml64 = Join-Path $msvcBin "ml64.exe"
$linkExe = Join-Path $msvcBin "link.exe"
if (-not (Test-Path $ml64) -or -not (Test-Path $linkExe)) { Write-Host "MSVC binaries not found in $msvcBin" -ForegroundColor Red; exit 1 }

$kitsRoot = "C:\Program Files (x86)\Windows Kits\10\Lib"
if (-not (Test-Path $kitsRoot)) { Write-Host "Windows Kits Lib root not found at $kitsRoot" -ForegroundColor Red; exit 1 }
$kitsVer = Get-ChildItem -Path $kitsRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
if (-not $kitsVer) { Write-Host "No Windows Kits versions found under $kitsRoot" -ForegroundColor Red; exit 1 }
$kitsUcrt = Join-Path $kitsVer.FullName "ucrt\x64"
$kitsUm = Join-Path $kitsVer.FullName "um\x64"
if (-not (Test-Path $kitsUcrt) -or -not (Test-Path $kitsUm)) { Write-Host "Windows Kits lib paths missing (ucrt/um) under $($kitsVer.FullName)" -ForegroundColor Red; exit 1 }

# Load VS environment into current process
cmd /c """$vcvars"" && set" | ForEach-Object {
  if ($_ -match "^(.*?)=(.*)$") {
    Set-Item -Path "env:$($matches[1])" -Value $matches[2]
  }
}

# Prepare output
$bin = Join-Path $PSScriptRoot "bin"
if ($Clean -and (Test-Path $bin)) { Remove-Item $bin -Recurse -Force }
New-Item -ItemType Directory -Path $bin -Force | Out-Null

# Compile
$asm = Join-Path $PSScriptRoot "working_ide_masm.asm"
$obj = Join-Path $bin "working_ide_masm.obj"

Write-Host "Compiling: $asm" -ForegroundColor Yellow
cmd /c $mlCmd
$mlCmd = ('"{0}" /c /nologo /Fo "{1}" "{2}"' -f $ml64, $obj, $asm)
cmd /c $mlCmd
cmd /c $mlCmd
if ($LASTEXITCODE -ne 0) { Write-Host "ml64 compile failed" -ForegroundColor Red; exit 1 }

# Link
$exe = Join-Path $bin "working_ide_masm.exe"
Write-Host "Linking: $exe" -ForegroundColor Yellow
cmd /c $linkCmd
$linkCmd = ('"{0}" /nologo /subsystem:windows /entry:WinMain "{1}" /LIBPATH:"{2}" /LIBPATH:"{3}" /LIBPATH:"{4}" user32.lib kernel32.lib gdi32.lib comdlg32.lib comctl32.lib /out:"{5}"' -f $linkExe, $obj, $msvcLib, $kitsUcrt, $kitsUm, $exe)
cmd /c $linkCmd
cmd /c $linkCmd
if ($LASTEXITCODE -ne 0) { Write-Host "link failed" -ForegroundColor Red; exit 1 }

Write-Host "Success: $exe" -ForegroundColor Green
