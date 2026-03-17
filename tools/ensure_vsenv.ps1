# Ensure MSVC/NMake tools are available on PATH (and other env vars) for this process.
# This avoids relying on Launch-VsDevShell.ps1 behavior, and works with NMake Makefiles builds.

[CmdletBinding()]
param(
    [ValidateSet("amd64")][string]$Arch = "amd64"
)

$ErrorActionPreference = "SilentlyContinue"

function Test-Tool($name) {
    return [bool](Get-Command $name -ErrorAction SilentlyContinue)
}

function Test-LibHasFile {
    param(
        [AllowNull()][AllowEmptyString()][string]$LibEnv,
        [Parameter(Mandatory = $true)][string]$FileName
    )
    if ([string]::IsNullOrWhiteSpace($LibEnv)) { return $false }
    foreach ($p in ($LibEnv -split ";")) {
        if ([string]::IsNullOrWhiteSpace($p)) { continue }
        $p2 = $p.Trim()
        try {
            if (Test-Path -LiteralPath (Join-Path $p2 $FileName)) { return $true }
        } catch {
            continue
        }
    }
    return $false
}

function Test-IncludeHasFile {
    param(
        [AllowNull()][AllowEmptyString()][string]$IncludeEnv,
        [Parameter(Mandatory = $true)][string]$FileName
    )
    if ([string]::IsNullOrWhiteSpace($IncludeEnv)) { return $false }
    foreach ($p in ($IncludeEnv -split ";")) {
        if ([string]::IsNullOrWhiteSpace($p)) { continue }
        $p2 = $p.Trim()
        try {
            if (Test-Path -LiteralPath (Join-Path $p2 $FileName)) { return $true }
        } catch {
            continue
        }
    }
    return $false
}

# "Tool exists on PATH" is not enough for CMake/NMake builds; we also need LIB *and* INCLUDE
# populated (Windows.h + CRT headers) or cl.exe will fail even if linking works.
if ((Test-Tool "nmake.exe") -and (Test-Tool "cl.exe") -and (Test-Tool "link.exe") -and (Test-Tool "rc.exe") -and (Test-Tool "mt.exe") -and
    (Test-LibHasFile -LibEnv $env:LIB -FileName "kernel32.lib") -and
    (Test-IncludeHasFile -IncludeEnv $env:INCLUDE -FileName "Windows.h") -and
    (Test-IncludeHasFile -IncludeEnv $env:INCLUDE -FileName "stdio.h")) {
    return
}

function Import-EnvFromBatch {
    param(
        [Parameter(Mandatory = $true)][string]$BatPath,
        [string]$Args = ""
    )

    if (-not (Test-Path -LiteralPath $BatPath)) { return $false }

    # Use `call` so this works whether the target is a .bat or .cmd and whether it uses setlocal.
    $cmd = "call `"$BatPath`" $Args >nul && set"
    $envDump = & cmd.exe /c $cmd 2>$null
    if (-not $envDump) { return $false }

    foreach ($line in $envDump) {
        $idx = $line.IndexOf("=")
        if ($idx -le 0) { continue }
        $name = $line.Substring(0, $idx)
        $val = $line.Substring($idx + 1)
        if ($name) {
            Set-Item -Path ("Env:{0}" -f $name) -Value $val | Out-Null
        }
    }
    return $true
}

# Prefer vcvars64.bat: it sets INCLUDE/LIB/LIBPATH (VsDevCmd.bat isn't always sufficient on custom installs).
$vcvarsCandidates = @(
    "C:\\VS2022Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat",
    "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat",
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat",
    "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat",
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat"
)

$vsDevCmdCandidates = @(
    "C:\\VS2022Enterprise\\Common7\\Tools\\VsDevCmd.bat",
    "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\Common7\\Tools\\VsDevCmd.bat",
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Enterprise\\Common7\\Tools\\VsDevCmd.bat",
    "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\Tools\\VsDevCmd.bat",
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\Tools\\VsDevCmd.bat"
)

  $vcvars64 = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $vcvars64) {
      # Fast path: use vswhere (avoids slow recursive filesystem scans).
      $vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe"
      if (Test-Path -LiteralPath $vswhere) {
          $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
          if (-not [string]::IsNullOrWhiteSpace($installPath)) {
              $candidate = Join-Path $installPath "VC\\Auxiliary\\Build\\vcvars64.bat"
              if (Test-Path -LiteralPath $candidate) { $vcvars64 = $candidate }
          }
      }
  }

  if (-not $vcvars64) {
      foreach ($root in @("C:\\VS2022Enterprise", "C:\\Program Files\\Microsoft Visual Studio\\2022", "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022")) {
          if (-not (Test-Path $root)) { continue }
          $hit = Get-ChildItem -Path $root -Recurse -Filter vcvars64.bat -ErrorAction SilentlyContinue | Select-Object -First 1
          if ($hit) { $vcvars64 = $hit.FullName; break }
      }
  }

if ($vcvars64) {
    [void](Import-EnvFromBatch -BatPath $vcvars64 -Args "")
}

# Fallback: VsDevCmd.bat (better than nothing for PATH-only scenarios).
  $vsDevCmd = $vsDevCmdCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
  if (-not $vsDevCmd) {
      # Fast path: use vswhere (avoids slow recursive filesystem scans).
      $vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe"
      if (Test-Path -LiteralPath $vswhere) {
          $installPath = & $vswhere -latest -products * -property installationPath 2>$null
          if (-not [string]::IsNullOrWhiteSpace($installPath)) {
              $candidate = Join-Path $installPath "Common7\\Tools\\VsDevCmd.bat"
              if (Test-Path -LiteralPath $candidate) { $vsDevCmd = $candidate }
          }
      }
  }

  if (-not $vsDevCmd) {
      # Last resort: search common roots (can be slow on some machines).
      foreach ($root in @("C:\\VS2022Enterprise", "C:\\Program Files\\Microsoft Visual Studio\\2022", "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022")) {
          if (-not (Test-Path $root)) { continue }
          $hit = Get-ChildItem -Path $root -Recurse -Filter VsDevCmd.bat -ErrorAction SilentlyContinue | Select-Object -First 1
          if ($hit) { $vsDevCmd = $hit.FullName; break }
      }
  }

if (-not $vsDevCmd) {
    if (-not $vcvars64) {
        Write-Host "⚠️  VS toolchain scripts not found (vcvars64.bat / VsDevCmd.bat); cannot auto-configure MSVC environment" -ForegroundColor Yellow
        return
    }
} else {
    # If vcvars64.bat didn't run (or didn't populate LIB), try VsDevCmd too.
    if (-not (Test-LibHasFile -LibEnv $env:LIB -FileName "kernel32.lib")) {
        [void](Import-EnvFromBatch -BatPath $vsDevCmd -Args "-arch=$Arch -host_arch=$Arch")
    }
}

# Sanity check and minimal remediation: if nmake is still missing, prepend the MSVC bin dir.
if (-not (Test-Tool "nmake.exe")) {
    $nmake = Get-ChildItem "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\*\\bin\\Hostx64\\x64\\nmake.exe" -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if (-not $nmake) {
        $nmake = Get-ChildItem "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC\\*\\bin\\Hostx64\\x64\\nmake.exe" -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1
    }
    if ($nmake) {
        $bin = Split-Path $nmake.FullName -Parent
        if (-not ($env:Path -split ";" | Where-Object { $_ -ieq $bin })) {
            $env:Path = "$bin;$env:Path"
        }
    }
}

if (-not (Test-Tool "nmake.exe")) {
    Write-Host "⚠️  MSVC environment loaded, but nmake.exe still not found on PATH" -ForegroundColor Yellow
}

# If LIB still can't resolve kernel32.lib, add Windows SDK lib paths manually (CMake compiler tests need this).
if (-not (Test-LibHasFile -LibEnv $env:LIB -FileName "kernel32.lib")) {
    Write-Verbose "LIB missing kernel32.lib; attempting Windows SDK LIB injection..."
    $sdkLibUm = $null
    $sdkLibUcrt = $null
    foreach ($root in @("C:\\Program Files (x86)\\Windows Kits\\10\\Lib", "C:\\Program Files\\Windows Kits\\10\\Lib")) {
        if (-not (Test-Path $root)) { continue }
        $verDirs = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
            Sort-Object Name -Descending
        foreach ($vd in $verDirs) {
            $um = Join-Path $vd.FullName "um\\x64".Replace("\\", "\")
            $ucrt = Join-Path $vd.FullName "ucrt\\x64".Replace("\\", "\")
            if ((Test-Path -LiteralPath (Join-Path $um "kernel32.lib")) -and (Test-Path -LiteralPath (Join-Path $ucrt "ucrt.lib"))) {
                $sdkLibUm = $um
                $sdkLibUcrt = $ucrt
                break
            }
        }
        if ($sdkLibUm) { break }
    }
    if ($sdkLibUm) {
        $add = @($sdkLibUm)
        if ($sdkLibUcrt) { $add += $sdkLibUcrt }
        $prefix = ($add -join ";")
        if ([string]::IsNullOrWhiteSpace($env:LIB)) { $env:LIB = $prefix } else { $env:LIB = "$prefix;$env:LIB" }
        Write-Verbose "Added Windows SDK LIB paths: $prefix"
    } else {
        Write-Verbose "No suitable Windows SDK lib folder found containing both kernel32.lib and ucrt.lib"
    }
}

# Ensure Windows SDK tools (rc.exe, mt.exe) are available for CMake try-compile/link manifests.
if (-not (Test-Tool "rc.exe") -or -not (Test-Tool "mt.exe")) {
    $kitBin = $null
    foreach ($root in @("C:\\Program Files (x86)\\Windows Kits\\10\\bin", "C:\\Program Files\\Windows Kits\\10\\bin")) {
        if (-not (Test-Path $root)) { continue }

        $verDirs = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
            # Version folder names look like: 10.0.22621.0
            # NOTE: regex should be \d (digit). Don't over-escape it as \\d, which would match a literal "\d".
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
            Sort-Object Name -Descending

        # Some installs can also have tools directly under ...\bin\x64 (no version folder).
        if (-not $verDirs -or $verDirs.Count -eq 0) {
            $x64Direct = Join-Path $root "x64"
            if (Test-Path $x64Direct) {
                if ((Test-Path (Join-Path $x64Direct "rc.exe")) -and (Test-Path (Join-Path $x64Direct "mt.exe"))) {
                    $kitBin = $x64Direct
                    break
                }
            }
        }

        foreach ($vd in $verDirs) {
            $x64 = Join-Path $vd.FullName "x64"
            if (-not (Test-Path $x64)) { continue }
            if ((Test-Path (Join-Path $x64 "rc.exe")) -and (Test-Path (Join-Path $x64 "mt.exe"))) {
                $kitBin = $x64
                break
            }
        }

        if ($kitBin) { break }
    }

    if ($kitBin) {
        if (-not ($env:Path -split ";" | Where-Object { $_ -ieq $kitBin })) {
            $env:Path = "$kitBin;$env:Path"
        }
    }
}

# -----------------------------------------------------------------------------
# Final fallback: some installs have cl/link on disk (or already on PATH) but do
# not ship vcvarsall.bat, and VsDevCmd.bat may not populate LIB/INCLUDE. CMake,
# NMake, and link.exe need these to resolve kernel32.lib/ucrt.
# -----------------------------------------------------------------------------
  function Get-MsvcToolsRoot {
      $cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
      if (-not $cl) { return $null }
      $binDir = Split-Path $cl.Source -Parent  # ...\bin\Hostx64\x64
      if (-not $binDir) { return $null }
      $p = $binDir
      for ($i = 0; $i -lt 3; $i++) { $p = Split-Path $p -Parent } # -> ...\VC\Tools\MSVC\<ver>
      if ($p -and (Test-Path $p)) { return $p }
      return $null
  }

  function Find-MsvcToolsRootOnDisk {
      # Handles installs where vcvarsall/vcvars64 are missing and cl.exe isn't on PATH yet.
      $roots = @(
          "C:\\VS2022Enterprise\\VC\\Tools\\MSVC",
          "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\MSVC",
          "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\MSVC",
          "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC",
          "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Tools\\MSVC"
      )

      foreach ($root in $roots) {
          if (-not (Test-Path $root)) { continue }
          $verDirs = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending
          foreach ($vd in $verDirs) {
              $bin = Join-Path $vd.FullName "bin\\Hostx64\\x64\\cl.exe"
              if (Test-Path $bin) { return $vd.FullName }
          }
      }
      return $null
  }

function Get-WindowsKitsVersionDirs {
    param([Parameter(Mandatory=$true)][string[]]$Roots)
    $dirs = @()
    foreach ($root in $Roots) {
        if (-not (Test-Path $root)) { continue }
        $dirs += Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' }
    }
    return ($dirs | Sort-Object Name -Descending)
}

  # Always add MSVC bin/lib/include if we can locate tools (even if kernel32 is already resolvable via SDK).
  $msvcRoot = Get-MsvcToolsRoot
  if (-not $msvcRoot) {
      $msvcRoot = Find-MsvcToolsRootOnDisk
      if ($msvcRoot) {
          $msvcBin = Join-Path $msvcRoot "bin\\Hostx64\\x64"
          if (Test-Path $msvcBin) {
              if (-not ($env:Path -split ";" | Where-Object { $_ -ieq $msvcBin })) {
                  $env:Path = "$msvcBin;$env:Path"
              }
          }
      }
  }
  if ($msvcRoot) {
      Write-Verbose "Detected MSVC tools root: $msvcRoot"
      if (-not $env:VCToolsInstallDir) { $env:VCToolsInstallDir = ($msvcRoot.TrimEnd("\") + "\") }

$msvcLib = Join-Path $msvcRoot "lib\\x64".Replace("\\", "\")
$msvcAtlLib = Join-Path $msvcRoot "atlmfc\\lib\\x64".Replace("\\", "\")
    $msvcInc = Join-Path $msvcRoot "include"
    $msvcAtlInc = Join-Path $msvcRoot "atlmfc\\include"

    $addLib = @()
    if (Test-Path $msvcLib) { $addLib += $msvcLib }
    if (Test-Path $msvcAtlLib) { $addLib += $msvcAtlLib }

    if ($addLib.Count -gt 0) {
        $prefix = ($addLib -join ";")
        if ([string]::IsNullOrWhiteSpace($env:LIB)) { $env:LIB = $prefix } else { $env:LIB = "$prefix;$env:LIB" }
        Write-Verbose "Added MSVC LIB paths: $prefix"
    }

    $addInc = @()
    if (Test-Path $msvcInc) { $addInc += $msvcInc }
    if (Test-Path $msvcAtlInc) { $addInc += $msvcAtlInc }

    if ($addInc.Count -gt 0) {
        $prefix = ($addInc -join ";")
        if ([string]::IsNullOrWhiteSpace($env:INCLUDE)) { $env:INCLUDE = $prefix } else { $env:INCLUDE = "$prefix;$env:INCLUDE" }
        Write-Verbose "Added MSVC INCLUDE paths: $prefix"
    }
}

if (-not (Test-LibHasFile -LibEnv $env:LIB -FileName "kernel32.lib")) {
    Write-Verbose "LIB still missing kernel32.lib after vcvars/VsDevCmd/SDK pass; applying manual SDK fallback..."
}

# Add Windows SDK LIB if still missing.
if (-not (Test-LibHasFile -LibEnv $env:LIB -FileName "kernel32.lib")) {
    $libRoots = @("C:\\Program Files (x86)\\Windows Kits\\10\\Lib", "C:\\Program Files\\Windows Kits\\10\\Lib")
    $verDirs = Get-WindowsKitsVersionDirs -Roots $libRoots
    foreach ($vd in $verDirs) {
        $um = Join-Path $vd.FullName "um\\x64".Replace("\\", "\")
        $ucrt = Join-Path $vd.FullName "ucrt\\x64".Replace("\\", "\")
        if (-not (Test-Path (Join-Path $um "kernel32.lib"))) { continue }
        # Some kit versions may not include UCRT libs; accept UM-only and let UCRT
        # be satisfied by another version below.
        $add = @($um)
        if (Test-Path (Join-Path $ucrt "ucrt.lib")) { $add += $ucrt }
        $prefix = ($add -join ";")
        if ([string]::IsNullOrWhiteSpace($env:LIB)) { $env:LIB = $prefix } else { $env:LIB = "$prefix;$env:LIB" }
        break
    }

    # If we still don't have ucrt.lib in LIB, try to find it in any kit version.
    if (-not (Test-LibHasFile -LibEnv $env:LIB -FileName "ucrt.lib")) {
        foreach ($vd in $verDirs) {
            $ucrt = Join-Path $vd.FullName "ucrt\\x64".Replace("\\", "\")
            if (Test-Path (Join-Path $ucrt "ucrt.lib")) {
                if ([string]::IsNullOrWhiteSpace($env:LIB)) { $env:LIB = $ucrt } else { $env:LIB = "$ucrt;$env:LIB" }
                break
            }
        }
    }
}

# Add Windows SDK INCLUDE if still missing (common when vcvars/VsDevCmd set LIB but not INCLUDE).
if (-not (Test-IncludeHasFile -IncludeEnv $env:INCLUDE -FileName "Windows.h") -or -not (Test-IncludeHasFile -IncludeEnv $env:INCLUDE -FileName "stdio.h")) {
    $incRoots = @("C:\\Program Files (x86)\\Windows Kits\\10\\Include", "C:\\Program Files\\Windows Kits\\10\\Include")
    $incVerDirs = Get-WindowsKitsVersionDirs -Roots $incRoots
    foreach ($vd in $incVerDirs) {
        $um = Join-Path $vd.FullName "um"
        $shared = Join-Path $vd.FullName "shared"
        $ucrt = Join-Path $vd.FullName "ucrt"
        if (-not (Test-Path (Join-Path $um "Windows.h"))) { continue }
        if (-not (Test-Path (Join-Path $ucrt "stdio.h"))) { continue }

        $add = @($um, $shared, $ucrt)
        $winrt = Join-Path $vd.FullName "winrt"
        $cppwinrt = Join-Path $vd.FullName "cppwinrt"
        if (Test-Path $winrt) { $add += $winrt }
        if (Test-Path $cppwinrt) { $add += $cppwinrt }

        $prefix = ($add -join ";")
        if ([string]::IsNullOrWhiteSpace($env:INCLUDE)) { $env:INCLUDE = $prefix } else { $env:INCLUDE = "$prefix;$env:INCLUDE" }
        break
    }
}

if (-not (Test-LibHasFile -LibEnv $env:LIB -FileName "kernel32.lib")) {
    Write-Host "⚠️  MSVC tools found, but LIB still cannot resolve kernel32.lib; build/link may fail. (VS env scripts likely incomplete on this machine.)" -ForegroundColor Yellow
}

if (-not (Test-IncludeHasFile -IncludeEnv $env:INCLUDE -FileName "Windows.h")) {
    Write-Host "⚠️  MSVC tools found, but INCLUDE still cannot resolve Windows.h; compile may fail. (Windows SDK include paths missing.)" -ForegroundColor Yellow
}
