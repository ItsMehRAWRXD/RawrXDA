param(
    [Parameter(Mandatory=$true, Position=0)] [string[]]$Input,
    [Parameter(Mandatory=$false)] [string]$OutDir = (Join-Path (Get-Location) 'bin/reveng-out'),
    [Parameter(Mandatory=$false)] [string]$Include = '',
    [Parameter(Mandatory=$false)] [string]$Defines = '',
    [Parameter(Mandatory=$false)] [ValidateSet('listings','skeletons','both')] [string]$Mode = 'both',
    [Parameter(Mandatory=$false)] [string]$Std = 'c++20'
)

$ErrorActionPreference = 'Stop'

function Resolve-DevCmd {
  $vsWhere = "$Env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path $vsWhere) {
    $vs = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) {
      $vcvars = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
      if (Test-Path $vcvars) { return $vcvars }
      $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
      if (Test-Path $vcvars) { return $vcvars }
    }
  }
  $candidates = @(
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
  )
  foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
  throw 'Visual Studio DevCmd not found'
}

function Ensure-OutDir($dir) { if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null } }

function Get-IncludeArgs([string]$Include) {
  if (-not $Include) { return '' }
  return ($Include.Split(';') | Where-Object { $_ -ne '' } | ForEach-Object { '/I "' + $_ + '"' }) -join ' '
}

function Get-DefineArgs([string]$Defines) {
  if (-not $Defines) { return '' }
  return ($Defines.Split(';') | Where-Object { $_ -ne '' } | ForEach-Object { '/D' + $_ }) -join ' '
}

function Invoke-CL-ToAsm([string]$src, [string]$outDir) {
  $devCmd = Resolve-DevCmd
  $srcAbs = (Resolve-Path -LiteralPath $src).Path
  $base = [System.IO.Path]::GetFileNameWithoutExtension($srcAbs)
  $asmOut = Join-Path $outDir ($base + '.asm')
  $incArgs = Get-IncludeArgs $Include
  $defArgs = Get-DefineArgs $Defines
  $cmd = '"' + $devCmd + '"' + ' && cl.exe /nologo /c /O2 /Z7 /std:' + $Std + ' /FAcs /Fa:"' + $asmOut + '" ' + $incArgs + ' ' + $defArgs + ' "' + $srcAbs + '"'
  Write-Host "[reveng] DevCmd: $devCmd"
  Write-Host "[reveng] Running: $cmd"
  $proc = Start-Process -FilePath cmd.exe -ArgumentList @('/c', $cmd) -NoNewWindow -Wait -PassThru -RedirectStandardOutput (Join-Path $outDir ($base + '.cl.log')) -RedirectStandardError (Join-Path $outDir ($base + '.cl.err'))
  Write-Host "[reveng] cl.exe ExitCode: $($proc.ExitCode)"
  if ($proc.ExitCode -ne 0) {
    Write-Host "[reveng] cl.exe failed. See logs in $outDir" -ForegroundColor Red
  }
  if (-not (Test-Path $asmOut)) { throw "Assembly listing not produced: $asmOut" }
  return $asmOut
}

function New-MasmSkeletonFromListing([string]$asmPath, [string]$dstPath) {
  $text = Get-Content -Raw -LiteralPath $asmPath
  $lines = $text -split "`r?`n"
  $procs = @()
  foreach ($ln in $lines) {
    if ($ln -match '^\s*([A-Za-z_?@$][A-Za-z0-9_?$@]*)\s+PROC\b') {
      $procs += $matches[1]
    }
  }
  $sb = New-Object System.Text.StringBuilder
  [void]$sb.AppendLine('.code')
  foreach ($p in ($procs | Sort-Object -Unique)) {
    [void]$sb.AppendLine('PUBLIC ' + $p)
    [void]$sb.AppendLine($p + ' PROC')
    [void]$sb.AppendLine('    ; TODO: port body from listing: ' + [System.IO.Path]::GetFileName($asmPath))
    [void]$sb.AppendLine('    ret')
    [void]$sb.AppendLine($p + ' ENDP')
    [void]$sb.AppendLine('')
  }
  [void]$sb.AppendLine('end')
  Set-Content -LiteralPath $dstPath -Value $sb.ToString() -Encoding ASCII
}

function Try-Undname([string]$symbol) {
  $und = 'undname.exe'
  $paths = @(
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\undname.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\undname.exe'
  )
  foreach ($p in $paths) { if (Test-Path $p) { $und = $p; break } }
  try { & $und $symbol } catch { return $symbol }
}

function Write-NamesMap([string]$asmPath, [string]$mapPath) {
  $text = Get-Content -Raw -LiteralPath $asmPath
  $lines = $text -split "`r?`n"
  $pairs = @()
  foreach ($ln in $lines) {
    if ($ln -match '^\s*([A-Za-z_?@$][A-Za-z0-9_?$@]*)\s+PROC\b') {
      $name = $matches[1]
      $dem = Try-Undname $name
      $pairs += [PSCustomObject]@{ Decorated = $name; Demangled = $dem }
    }
  }
  $pairs | ConvertTo-Json -Depth 2 | Set-Content -LiteralPath $mapPath -Encoding UTF8
}

Ensure-OutDir $OutDir
Write-Host "[reveng] OutDir: $OutDir"

$asmPaths = @()
foreach ($f in $Input) {
  Write-Host "[reveng] Source: $f"
  $asm = Invoke-CL-ToAsm -src $f -outDir $OutDir
  $asmPaths += $asm
}

foreach ($asm in $asmPaths) {
  if ($Mode -eq 'listings' -or $Mode -eq 'both') {
    # already in place
    Write-Host "[reveng] Listing: $asm"
  }
  if ($Mode -eq 'skeletons' -or $Mode -eq 'both') {
    $skel = [System.IO.Path]::Combine($OutDir, ([System.IO.Path]::GetFileNameWithoutExtension($asm) + '.skeleton.asm'))
    New-MasmSkeletonFromListing -asmPath $asm -dstPath $skel
    Write-Host "[reveng] Skeleton: $skel"

    $map = [System.IO.Path]::Combine($OutDir, ([System.IO.Path]::GetFileNameWithoutExtension($asm) + '.names.json'))
    Write-NamesMap -asmPath $asm -mapPath $map
    Write-Host "[reveng] Names map: $map"
  }
}
