<#
.SYNOPSIS
  RawrXD Windows-native "in-house" compiler driver.

.DESCRIPTION
  Implements the CLI shape used by `D:\genesis_build.ps1`:
    /DIRECTLINK ... /OUT_COFF /SOURCE:<file.asm> /OUT:<file.obj>
    /LIBMODE /OUT:<lib> <objs...>
    /LINKMODE /OUT:<exe> <objs/libs/res...> <systemlibs...>

  This is Windows-native and avoids cl/clang. For assembling MASM sources it
  uses `ml64.exe` (MASM) and for link/lib it uses `link.exe` and `lib.exe`.

  If you later replace ML64/link/lib with your own implementations, keep this
  front-end stable and swap the internals.
#>

param(
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$Args
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Resolve-VSTool {
  param(
    [Parameter(Mandatory = $true)][ValidateSet("link", "lib", "ml64")]
    [string]$ToolName
  )

  $cmd = Get-Command "$ToolName.exe" -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }

  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  $fallbackGlobs = @(
    "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\*\\bin\\Hostx64\\x64\\$ToolName.exe",
    "C:\\Program Files\\Microsoft Visual Studio\\2022\\*\\VC\\Tools\\MSVC\\*\\bin\\Hostx64\\x64\\$ToolName.exe",
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\*\\VC\\Tools\\MSVC\\*\\bin\\Hostx64\\x64\\$ToolName.exe"
  )

  if (!(Test-Path $vswhere)) {
    foreach ($g in $fallbackGlobs) {
      $hit = Get-ChildItem -Path $g -File -ErrorAction SilentlyContinue | Select-Object -First 1
      if ($hit) { return $hit.FullName }
    }
    throw "$ToolName.exe not found (PATH) and vswhere.exe missing. Install VS Build Tools (C++ x64) or add it to PATH."
  }

  $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
  if (-not $installPath) {
    foreach ($g in $fallbackGlobs) {
      $hit = Get-ChildItem -Path $g -File -ErrorAction SilentlyContinue | Select-Object -First 1
      if ($hit) { return $hit.FullName }
    }
    throw "VS Build Tools not found via vswhere; cannot locate $ToolName.exe."
  }

  $msvcDirs = Get-ChildItem -LiteralPath (Join-Path $installPath "VC\\Tools\\MSVC") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending

  foreach ($msvc in $msvcDirs) {
    $candidate = Join-Path $msvc.FullName "bin\\Hostx64\\x64\\$ToolName.exe"
    if (Test-Path $candidate) { return $candidate }
  }

  foreach ($g in $fallbackGlobs) {
    $hit = Get-ChildItem -Path $g -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) { return $hit.FullName }
  }

  throw "$ToolName.exe not found under $installPath."
}

function Resolve-VcLibPath {
  param([Parameter(Mandatory = $true)][string]$LinkExe)
  try {
    $linkDir = Split-Path -Parent $LinkExe
    # ...\VC\Tools\MSVC\<ver>\bin\Hostx64\x64\link.exe
    $msvcRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $linkDir))
    $vcLib = Join-Path $msvcRoot "lib\\x64"
    if (Test-Path $vcLib) { return $vcLib }
  } catch {}
  return $null
}

function Resolve-WindowsSdkLibPaths {
  $sdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\\10\\Lib"
  if (-not (Test-Path $sdkRoot)) { return @() }

  $versions = @()
  foreach ($d in (Get-ChildItem -Path $sdkRoot -Directory -ErrorAction SilentlyContinue)) {
    try { $versions += [pscustomobject]@{ Name = $d.Name; Version = [version]$d.Name } } catch {}
  }
  $versions = $versions | Sort-Object Version -Descending
  foreach ($v in $versions) {
    $base = Join-Path $sdkRoot $v.Name
    $um = Join-Path $base "um\\x64"
    $ucrt = Join-Path $base "ucrt\\x64"
    if ((Test-Path $um) -and (Test-Path $ucrt)) { return @($um, $ucrt) }
  }
  return @()
}

function Parse-FlagValue {
  param(
    [Parameter(Mandatory = $true)][string[]]$Tokens,
    [Parameter(Mandatory = $true)][string]$Prefix
  )
  foreach ($t in $Tokens) {
    if ($t -like "$Prefix*") {
      return $t.Substring($Prefix.Length).Trim('"')
    }
  }
  return $null
}

function Unquote([string]$value) {
  if ($null -eq $value) { return $null }
  return $value.Trim().Trim('"')
}

function Parse-Switch {
  param(
    [Parameter(Mandatory = $true)][string[]]$Tokens,
    [Parameter(Mandatory = $true)][string]$Name
  )
  return ($Tokens | Where-Object { $_ -eq $Name } | Measure-Object).Count -gt 0
}

function Fail([string]$msg) {
  Write-Error $msg
  exit 1
}

if (-not $Args -or $Args.Count -eq 0) {
  Fail "No arguments. Expected /DIRECTLINK, /LIBMODE, or /LINKMODE."
}

$mode =
  if (Parse-Switch $Args "/DIRECTLINK") { "DIRECTLINK" }
  elseif (Parse-Switch $Args "/LIBMODE") { "LIBMODE" }
  elseif (Parse-Switch $Args "/LINKMODE") { "LINKMODE" }
  else { "" }

if (-not $mode) {
  Fail "Unknown mode. Provide one of: /DIRECTLINK | /LIBMODE | /LINKMODE"
}

switch ($mode) {
  "DIRECTLINK" {
    $source = Parse-FlagValue $Args "/SOURCE:"
    $outObj = Parse-FlagValue $Args "/OUT:"
    if (-not $source) { Fail "Missing /SOURCE:<file.asm>" }
    if (-not $outObj) { Fail "Missing /OUT:<file.obj>" }
    if (!(Test-Path $source)) { Fail "SOURCE not found: $source" }

    $ml64 = Resolve-VSTool -ToolName ml64
    $outDir = Split-Path -Parent $outObj
    if ($outDir -and !(Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

    # Pass-through: /I:<dir> and /D:<macro> tokens (if present) to ml64.
    $includes = @()
    $defines = @()
    foreach ($t in $Args) {
      if ($t -like "/I:*") { $includes += $t }
      if ($t -like "/D:*") { $defines += $t }
    }

    $mlArgs = @("/nologo", "/c", "/Fo", $outObj) + $defines + $includes + @($source)
    & $ml64 @mlArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    exit 0
  }

  "LIBMODE" {
    $outLib = Parse-FlagValue $Args "/OUT:"
    if (-not $outLib) { Fail "Missing /OUT:<file.lib>" }

    $objs = @()
    foreach ($t in $Args) {
      $p = Unquote $t
      if ($p -match "\.obj$" -and (Test-Path $p)) { $objs += $p }
    }
    if ($objs.Count -eq 0) { Fail "No .obj inputs found for /LIBMODE" }

    $libExe = Resolve-VSTool -ToolName lib
    $outDir = Split-Path -Parent $outLib
    if ($outDir -and !(Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

    $libArgs = @("/NOLOGO", "/MACHINE:X64", "/OUT:$outLib") + $objs
    & $libExe @libArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    exit 0
  }

  "LINKMODE" {
    $outExe = Parse-FlagValue $Args "/OUT:"
    if (-not $outExe) { Fail "Missing /OUT:<file.exe>" }

    $linkExe = Resolve-VSTool -ToolName link
    $outDir = Split-Path -Parent $outExe
    if ($outDir -and !(Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

    $pass = @()
    $pass += "/NOLOGO"
    $pass += "/MACHINE:X64"
    $pass += "/INCREMENTAL:NO"
    $pass += "/OUT:$outExe"

    $vcLib = Resolve-VcLibPath -LinkExe $linkExe
    if ($vcLib) { $pass += "/LIBPATH:$vcLib" }
    foreach ($sdkLib in (Resolve-WindowsSdkLibPaths)) { $pass += "/LIBPATH:$sdkLib" }

    foreach ($t in $Args) {
      $u = Unquote $t
      if ($t -like "/SUBSYSTEM:*") { $pass += $t; continue }
      if ($t -like "/ENTRY:*")     { $pass += $t; continue }
      if ($t -like "/LIBPATH:*")   { $pass += $t; continue }
      if ($u -match "\.(obj|lib|res)$") { $pass += $u; continue }
      if ($u -match "\.lib$") { $pass += $u; continue }
      if ($u -like "*.lib") { $pass += $u; continue }
      if ($u -like "*.obj") { $pass += $u; continue }
      if ($u -like "*.res") { $pass += $u; continue }
      # Keep common linker flags from genesis.
      if ($t -like "/LARGEADDRESSAWARE*" -or
          $t -like "/FIXED:*" -or
          $t -like "/DYNAMICBASE*" -or
          $t -like "/NXCOMPAT*" -or
          $t -like "/NODEFAULTLIB:*" -or
          $t -like "/MERGE:*" -or
          $t -like "/FORCE:*") {
        $pass += $t
        continue
      }
    }

    & $linkExe @pass
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    exit 0
  }
}
