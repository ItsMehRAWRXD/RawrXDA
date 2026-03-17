$ErrorActionPreference = 'Stop'

$w = New-Object -ComObject WScript.Shell
$startMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"

function New-Shortcut {
  param(
    [Parameter(Mandatory=$true)][string]$name,
    [Parameter(Mandatory=$true)][string]$root,
    [Parameter(Mandatory=$true)][string]$pattern
  )
  if (-not (Test-Path -LiteralPath $root)) { Write-Warning "Skip $($name): $root not found"; return }

  $exe = Get-ChildItem -LiteralPath $root -Filter $pattern -File -Recurse -ErrorAction SilentlyContinue |
         Sort-Object -Property Length -Descending |
         Select-Object -First 1
  if (-not $exe) {
    $exe = Get-ChildItem -LiteralPath $root -Filter *.exe -File -Recurse -ErrorAction SilentlyContinue |
           Sort-Object -Property LastWriteTime -Descending |
           Select-Object -First 1
  }
  if (-not $exe -and (Test-Path -LiteralPath 'D:\\AmazonQ-IDE.exe') -and $name -eq 'Amazon Q IDE') {
    $exe = Get-Item -LiteralPath 'D:\\AmazonQ-IDE.exe'
  }
  if (-not $exe) { Write-Warning "Skip $($name): no EXE in $root"; return }

  $lnk = Join-Path $startMenu ("{0}.lnk" -f $name)
  $s = $w.CreateShortcut($lnk)
  $s.TargetPath = $exe.FullName
  $s.WorkingDirectory = $root
  $s.Save()
  Write-Host "Shortcut created: $lnk -> $($exe.Name)"
}

$amazonRoot    = $env:AMAZONQ_HOME;     if (-not $amazonRoot)    { $amazonRoot    = "D:\amazonq-ide" }
$mycopilotRoot = $env:MYCOPILOT_HOME;   if (-not $mycopilotRoot) { $mycopilotRoot = "D:\MyCoPilot-Complete-Portable" }

New-Shortcut -name "Amazon Q IDE"  -root $amazonRoot    -pattern "*AmazonQ*.exe"
New-Shortcut -name "MyCoPilot IDE" -root $mycopilotRoot -pattern "*Copilot*.exe"
