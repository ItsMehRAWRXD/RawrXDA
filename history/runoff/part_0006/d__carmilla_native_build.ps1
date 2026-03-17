param(
  [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
  [ValidateSet('encryptor','stub_gen','pe_wrap','all')][string]$Target = 'all',
  [switch]$Test
)

$ErrorActionPreference = 'Stop'

function Find-VcVars64 {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (!(Test-Path $vswhere)) { return $null }

  $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (!$vsPath) { return $null }

  $bat = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
  if (Test-Path $bat) { return $bat }
  return $null
}

function Find-WindowsSdkLibPath {
  $base = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Lib'
  if (!(Test-Path $base)) { return $null }

  $versions = Get-ChildItem -Path $base -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
    Sort-Object Name -Descending

  foreach ($v in $versions) {
    $um = Join-Path $v.FullName 'um\x64\kernel32.lib'
    $ucrt = Join-Path $v.FullName 'ucrt\x64\ucrt.lib'
    if ((Test-Path $um) -and (Test-Path $ucrt)) {
      return @(
        (Join-Path $v.FullName 'um\x64'),
        (Join-Path $v.FullName 'ucrt\x64')
      )
    }
  }

  return $null
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here

$targets = @()
if ($Target -eq 'all') {
  $targets = @(
    @{ Asm = 'carmilla_x64.asm'; Out = 'carmilla_x64.exe' },
    @{ Asm = 'carmilla_stub_gen_x64.asm'; Out = 'carmilla_stub_gen_x64.exe' },
    @{ Asm = 'carmilla_pe_wrap_x64.asm'; Out = 'carmilla_pe_wrap_x64.exe' }
  )
} elseif ($Target -eq 'encryptor') {
  $targets = @(@{ Asm = 'carmilla_x64.asm'; Out = 'carmilla_x64.exe' })
} elseif ($Target -eq 'stub_gen') {
  $targets = @(@{ Asm = 'carmilla_stub_gen_x64.asm'; Out = 'carmilla_stub_gen_x64.exe' })
} elseif ($Target -eq 'pe_wrap') {
  $targets = @(@{ Asm = 'carmilla_pe_wrap_x64.asm'; Out = 'carmilla_pe_wrap_x64.exe' })
}

foreach ($t in $targets) {
  $asm = $t.Asm
  $out = $t.Out

  if (!(Test-Path $asm)) { throw "ASM not found: $asm" }

  $vcvars = Find-VcVars64
  if (!$vcvars) {
    throw "Could not find vcvars64.bat. Install VS Build Tools with 'Desktop development with C++' (MSVC x64) and Windows 10/11 SDK."
  }

  $sdkPaths = Find-WindowsSdkLibPath
  if (!$sdkPaths) {
    throw "Could not locate Windows SDK x64 libs (kernel32.lib/ucrt.lib). Install Windows 10/11 SDK."
  }

  $umPath, $ucrtPath = $sdkPaths

  $flags = @('/nologo')
  if ($Configuration -eq 'Debug') {
    $flags += '/Zi'
  }

  Write-Host "[*] Assembling $asm" -ForegroundColor Cyan

  $assemble = "`"$vcvars`" >nul & cd /d `"$here`" & ml64 $($flags -join ' ') /c `"$asm`""
  cmd /d /v:off /c $assemble | Write-Host
  if ($LASTEXITCODE -ne 0) { throw "ml64 failed with exit code $LASTEXITCODE" }

  Write-Host "[*] Linking $out" -ForegroundColor Cyan

  $libs = "kernel32.lib"
  if ($asm -match 'carmilla_x64|carmilla_stub_gen') { $libs += " bcrypt.lib" }
  $link = "`"$vcvars`" >nul & cd /d `"$here`" & link /nologo `"$([IO.Path]::ChangeExtension($asm,'obj'))`" /entry:main /subsystem:console /out:`"$out`" /libpath:`"$umPath`" /libpath:`"$ucrtPath`" $libs"
  cmd /d /v:off /c $link | Write-Host
  if ($LASTEXITCODE -ne 0) { throw "link failed with exit code $LASTEXITCODE" }

  Write-Host "[+] Built: $here\$out" -ForegroundColor Green
}

if ($Test) {
  Write-Host "[*] Running smoke test" -ForegroundColor Cyan
  if ($Target -eq 'all' -or $Target -eq 'encryptor') {
    & "$here\smoke-test.ps1" -Exe "$here\carmilla_x64.exe"
  }
  # Stub gen doesn't have a smoke test yet
}
