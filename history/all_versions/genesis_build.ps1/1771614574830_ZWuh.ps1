<#
.SYNOPSIS
  RawrXD Final Link (Reverse-Linking Architecture)

.DESCRIPTION
  This is the *final* build step that produces a single PE32+ executable from
  already-built x64 COFF .obj files (one per .asm module) + optional static libs
  + optional .res.

  Key rule (your requirement):
    LINK = no need to assemble

  So this script NEVER calls ml64/cl/clang and NEVER recompiles anything.
  It only performs the final link.

.PARAMETER Mode
  monolithic   : single RawrXD.exe (recommended)
  exe+dlls     : placeholder wiring for DLL split (still link-only)
  hybrid       : single RawrXD.exe with CLI modes via args (still link-only)

.PARAMETER ObjectsRoot
  Directory containing .obj files (recursively scanned) OR provide -ObjectsFile.

.PARAMETER ObjectsFile
  Text file listing .obj paths (one per line). Lines starting with '#' ignored.

.PARAMETER LibDir
  Directory containing static libs like rawrxd_core.lib/rawrxd_gpu.lib.

.PARAMETER ResFile
  Optional .res (version info, manifest, icon). Included if present.

.PARAMETER LinkExe
  Linker executable path. Defaults to link.exe on PATH.

.EXAMPLE
  pwsh -File .\genesis_build.ps1 -Mode monolithic -DisableRecompile

.EXAMPLE
  pwsh -File .\genesis_build.ps1 -ObjectsRoot "$env:LOCALAPPDATA\RawrXD\build" -LibDir "$env:LOCALAPPDATA\RawrXD\lib" -ResFile "$env:LOCALAPPDATA\RawrXD\rawrxd.res" -OutExe "$env:LOCALAPPDATA\RawrXD\bin\RawrXD.exe"
#>

[CmdletBinding()]
param(
  [ValidateSet('monolithic','exe+dlls','hybrid')]
  [string]$Mode = 'monolithic',

  # Link-only guard (kept for clarity; this script is always link-only)
  [switch]$DisableRecompile = $true,

  # Inputs
  [string]$ObjectsRoot = "$env:LOCALAPPDATA\RawrXD\build",
  [string]$ObjectsFile = "",
  [string]$LibDir = "$env:LOCALAPPDATA\RawrXD\lib",
  [string]$ResFile = "$env:LOCALAPPDATA\RawrXD\rawrxd.res",

  # Output
  [string]$OutExe = "$env:LOCALAPPDATA\RawrXD\bin\RawrXD.exe",

  # Link config
  [ValidateSet('WINDOWS','CONSOLE')]
  [string]$Subsystem = 'WINDOWS',
  [string]$Entry = 'WinMain',
  [string]$Machine = 'X64',

  # Tooling
  [string]$LinkExe = ''
)

$ErrorActionPreference = 'Stop'

function Get-ObjListFromFile([string]$path) {
  if (!(Test-Path -LiteralPath $path)) { throw "ObjectsFile not found: $path" }
  $objs = Get-Content -LiteralPath $path -ErrorAction Stop |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_ -and -not $_.StartsWith('#') }
  return ,$objs
}

function Get-ObjListFromRoot([string]$root) {
  if (!(Test-Path -LiteralPath $root)) { throw "ObjectsRoot not found: $root" }
  $objs = Get-ChildItem -LiteralPath $root -Recurse -File -Filter *.obj -ErrorAction Stop |
    Select-Object -ExpandProperty FullName
  return ,$objs
}

function Resolve-LinkExe([string]$hint) {
  if ($hint -and (Test-Path -LiteralPath $hint)) { return (Resolve-Path -LiteralPath $hint).Path }
  $cmd = Get-Command link.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  throw "link.exe not found. Provide -LinkExe <path> (Windows SDK/VS Build Tools)."
}

function Ensure-Dir([string]$path) {
  if (!(Test-Path -LiteralPath $path)) {
    New-Item -ItemType Directory -Path $path -Force | Out-Null
  }
}

Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "  RawrXD Final Link (Reverse-Linking)" -ForegroundColor Cyan
Write-Host "  LINK ONLY: .obj/.lib/.res -> RawrXD.exe" -ForegroundColor Magenta
Write-Host "  DisableRecompile: $DisableRecompile" -ForegroundColor Gray
Write-Host "======================================================" -ForegroundColor Cyan

if (-not $DisableRecompile) {
  Write-Host "[WARN] DisableRecompile is OFF, but this script still does LINK ONLY." -ForegroundColor Yellow
}

$link = Resolve-LinkExe $LinkExe

# Collect object list
$objList = @()
if ($ObjectsFile) {
  $objList = @(Get-ObjListFromFile $ObjectsFile)
} else {
  $objList = @(Get-ObjListFromRoot $ObjectsRoot)
}

if ($objList.Count -lt 1) {
  throw "No .obj files found. Provide -ObjectsFile or ensure -ObjectsRoot contains .obj outputs."
}

# Validate inputs exist
$missing = $objList | Where-Object { -not (Test-Path -LiteralPath $_) }
if ($missing.Count -gt 0) {
  throw "Missing .obj inputs (first 10): $((@($missing) | Select-Object -First 10) -join '; ')"
}

# Gather static libs (optional)
$staticLibs = @()
if (Test-Path -LiteralPath $LibDir) {
  foreach ($n in @('rawrxd_core.lib','rawrxd_gpu.lib','rawrxd_agent.lib','rawrxd_ui.lib')) {
    $p = Join-Path $LibDir $n
    if (Test-Path -LiteralPath $p) { $staticLibs += $p }
  }
  # Also include any other .lib present (optional)
  $extra = Get-ChildItem -LiteralPath $LibDir -File -Filter *.lib -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty FullName
  foreach ($p in $extra) {
    if ($staticLibs -notcontains $p) { $staticLibs += $p }
  }
}

# Resource (optional)
$useRes = $false
if ($ResFile -and (Test-Path -LiteralPath $ResFile)) { $useRes = $true }

# Ensure output dir
Ensure-Dir (Split-Path -Parent $OutExe)

# Mode -> entry/subsystem
switch ($Mode) {
  'monolithic' {
    # keep provided Entry/Subsystem
  }
  'hybrid' {
    # still one binary; keep Entry/Subsystem
  }
  'exe+dlls' {
    # link-only placeholder; still produces single exe unless you pass /DLL targets separately
    Write-Host "[INFO] Mode exe+dlls selected: this script links the EXE only. DLL linking is a separate call." -ForegroundColor Yellow
  }
}

# Build linker args (single final PE32+ executable)
$linkArgs = @(
  '/NOLOGO',
  "/OUT:$OutExe",
  "/SUBSYSTEM:$Subsystem",
  "/ENTRY:$Entry",
  "/MACHINE:$Machine",
  '/LARGEADDRESSAWARE',
  '/FIXED:NO',
  '/DYNAMICBASE',
  '/NXCOMPAT',
  '/INCREMENTAL:NO',
  '/NODEFAULTLIB:libcmt',
  '/MERGE:.rdata=.text'
)

# Inputs: objs + libs + res
$inputs = @()
$inputs += $objList
$inputs += $staticLibs
if ($useRes) { $inputs += $ResFile }

# System libs (expand as needed)
$sysLibs = @('kernel32.lib','user32.lib','gdi32.lib','shell32.lib','ole32.lib','advapi32.lib')

Write-Host "[LINK] $OutExe" -ForegroundColor White
Write-Host "  link.exe : $link" -ForegroundColor Gray
Write-Host "  objs     : $($objList.Count)" -ForegroundColor Gray
Write-Host "  libs     : $($staticLibs.Count)" -ForegroundColor Gray
Write-Host "  res      : $useRes" -ForegroundColor Gray

# Invoke link
& $link @linkArgs @inputs @sysLibs
if ($LASTEXITCODE -ne 0) { throw "Link failed (exit $LASTEXITCODE)" }

if (!(Test-Path -LiteralPath $OutExe)) {
  throw "Link reported success but output not found: $OutExe"
}

$sizeMb = (Get-Item -LiteralPath $OutExe).Length / 1MB
Write-Host "[OK] Built single executable: $OutExe" -ForegroundColor Green
Write-Host ("[SIZE] {0:N2} MB" -f $sizeMb) -ForegroundColor DarkGray
