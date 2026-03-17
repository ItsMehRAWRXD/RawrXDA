#requires -Version 7.0
<#
.SYNOPSIS
  Fast, no-recompile monolithic RawrXD.exe build.

.DESCRIPTION
  This is the pragmatic "finish today" path:
  - No compilation.
  - No assembly.
  - Relinks from existing Ninja/CMake build artifacts (build_gold) using the
    generated response file CMake provides.

  Output: a single PE32+ RawrXD.exe (no subagent exe splatter).

.PARAMETER Root
  RawrXD repo root.

.PARAMETER BuildDir
  Existing build directory that already contains objects/libs and the CMake
  response file (defaults to build_gold).

.PARAMETER OutExe
  Output executable path. Default: <Root>\build_inhouse\bin\RawrXD.exe

.PARAMETER UseExistingExe
  Skip relink; just copy the already-built <BuildDir>\bin\RawrXD_Gold.exe to OutExe.
  (Fastest, but not a fresh link step.)

.PARAMETER ForceRelink
  Always relink even if OutExe is newer than the rsp file.
#>

param(
  [string]$Root = "D:\RawrXD",
  [string]$BuildDir = "",
  [string]$OutExe = "",
  [switch]$UseExistingExe,
  [switch]$ForceRelink
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Resolve-LinkExe {
  $cmd = Get-Command link.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }

  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path $vswhere) {
    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($installPath) {
      $candidate = Get-ChildItem -LiteralPath (Join-Path $installPath "VC\Tools\MSVC") -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object {
          $p = Join-Path $_.FullName "bin\Hostx64\x64\link.exe"
          if (Test-Path $p) { return $p }
        } | Select-Object -First 1
      if ($candidate) { return $candidate }
    }
  }

  throw "link.exe not found (PATH or VS Build Tools required)."
}

function Ensure-Dir([string]$path) {
  $dir = Split-Path -Parent $path
  if ($dir -and !(Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
}

$Root = (Resolve-Path $Root).Path
$BuildDir = if ($BuildDir) { $BuildDir } else { (Join-Path $Root "build_gold") }
$BuildDir = (Resolve-Path $BuildDir).Path

$rspCandidates = @(
  (Join-Path $BuildDir "CMakeFiles\RawrXD-Gold.rsp"),
  (Join-Path $BuildDir "CMakeFiles\RawrXD_Gold.rsp")
)
$rsp = $rspCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
$goldExe = Join-Path $BuildDir "bin\RawrXD_Gold.exe"

if (-not $OutExe) {
  $OutExe = Join-Path $Root "build_inhouse\bin\RawrXD.exe"
}

Ensure-Dir $OutExe

Write-Host "============================================================" -Fore Cyan
Write-Host " RAWRXD FAST MONOLITHIC BUILD (NO RECOMPILE)" -Fore Cyan
Write-Host " Root    : $Root" -Fore Gray
Write-Host " Build   : $BuildDir" -Fore Gray
Write-Host " Output  : $OutExe" -Fore Gray
Write-Host "============================================================" -Fore Cyan

if ($UseExistingExe) {
  if (!(Test-Path $goldExe)) { throw "Missing $goldExe (run the CMake/Ninja build once to seed artifacts)." }
  Copy-Item -LiteralPath $goldExe -Destination $OutExe -Force
  Write-Host "OK: Copied $goldExe -> $OutExe" -Fore Green
  exit 0
}

function Ensure-RspFromNinja {
  param(
    [string]$BuildDir,
    [string]$GoldExe,
    [string[]]$RspCandidates
  )

  $rspNow = $RspCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if ($rspNow) { return $rspNow }

  $ninja = (Get-Command ninja.exe -ErrorAction SilentlyContinue).Source
  if (-not $ninja) { $ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source }
  if (-not $ninja) { return $null }

  # Ninja typically deletes response files after the command runs; keeprsp preserves them.
  # To force the link step to run (and thus materialize the .rsp), temporarily remove the exe.
  $bak = "$GoldExe.bak"
  $hadExe = Test-Path $GoldExe
  if ($hadExe) {
    Copy-Item -LiteralPath $GoldExe -Destination $bak -Force
    Remove-Item -LiteralPath $GoldExe -Force
  }

  try {
    & $ninja -C $BuildDir -d keeprsp "bin/RawrXD_Gold.exe" | Out-Host
  } finally {
    # If something went wrong and we lost the exe, restore the backup.
    if (-not (Test-Path $GoldExe) -and (Test-Path $bak)) {
      Copy-Item -LiteralPath $bak -Destination $GoldExe -Force
    }
  }

  return ($RspCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1)
}

if (-not $rsp) {
  if ((Test-Path $goldExe) -and -not $ForceRelink) {
    $rsp = $null
  } else {
    Write-Host "[INFO] CMake/Ninja response file not present; trying to materialize it via Ninja (keeprsp)..." -Fore DarkGray
    $rsp = Ensure-RspFromNinja -BuildDir $BuildDir -GoldExe $goldExe -RspCandidates $rspCandidates
  }
}

if (-not $rsp) {
  if (Test-Path $goldExe) {
    Write-Host "[WARN] No .rsp available; falling back to copying the already-built $goldExe." -Fore Yellow
    Copy-Item -LiteralPath $goldExe -Destination $OutExe -Force
    Write-Host "OK: Copied $goldExe -> $OutExe" -Fore Green
    exit 0
  }
  throw "No .rsp available and $goldExe is missing. Ensure $BuildDir is a completed CMake/Ninja build tree."
}

if ((Test-Path $OutExe) -and -not $ForceRelink) {
  $outTime = (Get-Item -LiteralPath $OutExe).LastWriteTimeUtc
  $rspTime = (Get-Item -LiteralPath $rsp).LastWriteTimeUtc
  if ($outTime -ge $rspTime) {
    Write-Host "SKIP: OutExe is already up-to-date vs rsp (use -ForceRelink to override)." -Fore DarkGray
    exit 0
  }
}

$linkExe = Resolve-LinkExe
$outDir = Split-Path -Parent $OutExe
$pdb = Join-Path $outDir "RawrXD.pdb"
$implib = Join-Path $outDir "RawrXD.lib"

Write-Host "[LinkOnly] Relinking from existing CMake rsp" -Fore Yellow
Write-Host "  link : $linkExe" -Fore DarkGray
Write-Host "  rsp  : $rsp" -Fore DarkGray

Push-Location $BuildDir
try {
  & $linkExe `
    /NOLOGO `
    "@$rsp" `
    "/OUT:$OutExe" `
    "/PDB:$pdb" `
    "/IMPLIB:$implib" `
    /INCREMENTAL:NO

  if ($LASTEXITCODE -ne 0) { throw "link.exe failed (exit $LASTEXITCODE)." }
} finally {
  Pop-Location
}

Write-Host "OK: Built $OutExe" -Fore Green
