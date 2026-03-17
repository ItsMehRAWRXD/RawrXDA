param(
  [string]$Root = "D:\RawrXD",
  [string]$BuildDir = "",
  [string]$Target = "bin/RawrXD_Gold.exe",
  [string]$OutDir = "",
  [switch]$ForceRelink
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Resolve-Ninja {
  $cmd = Get-Command ninja.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  $cmd = Get-Command ninja -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  throw "ninja not found on PATH"
}

function Get-FileVersion([string]$path) {
  try { return ([System.Diagnostics.FileVersionInfo]::GetVersionInfo($path)).FileVersion } catch { return "" }
}

$Root = (Resolve-Path $Root).Path
$BuildDir = if ($BuildDir) { $BuildDir } else { (Join-Path $Root "build_gold") }
$BuildDir = (Resolve-Path $BuildDir).Path
$OutDir = if ($OutDir) { $OutDir } else { (Join-Path $Root "build_inhouse\re_msvc") }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$ninja = Resolve-Ninja

# Best-effort: if the target already exists and you don't force relink,
# we avoid breaking builds due to missing rules.
if ((Test-Path (Join-Path $BuildDir ($Target -replace '/', '\\'))) -and -not $ForceRelink) {
  Write-Host "OK: Target already exists; skipping Ninja run (use -ForceRelink to capture rsp)." -Fore DarkGray
} else {
  Write-Host "[RE] Running Ninja with keeprsp to capture link inputs..." -Fore Cyan
  & $ninja -C $BuildDir -d keeprsp $Target | Out-Host
}

$rspFiles = Get-ChildItem -LiteralPath (Join-Path $BuildDir "CMakeFiles") -File -Filter "*.rsp" -ErrorAction SilentlyContinue
foreach ($rsp in $rspFiles) {
  Copy-Item -LiteralPath $rsp.FullName -Destination (Join-Path $OutDir $rsp.Name) -Force
}

$cmds = & $ninja -C $BuildDir -t commands $Target 2>$null

$ml64Path = ($cmds | Select-String -Pattern "\\ml64\.exe" | Select-Object -First 1).Line
$linkPath = ($cmds | Select-String -Pattern "\\link\.exe" | Select-Object -First 1).Line

function Extract-ExePath([string]$line, [string]$exeName) {
  if (-not $line) { return $null }
  # Match: C:\path\to\tool.exe  (stops at space or quote)
  $pattern = '([A-Z]:\\[^\\" ]+\\' + [regex]::Escape($exeName) + ')'
  $m = [regex]::Match($line, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
  if ($m.Success) { return $m.Groups[1].Value }
  return $null
}

$ml64Exe = Extract-ExePath $ml64Path "ml64.exe"
$linkExe = Extract-ExePath $linkPath "link.exe"

$payload = [ordered]@{
  Root = $Root
  BuildDir = $BuildDir
  Target = $Target
  CapturedAtUtc = [DateTime]::UtcNow.ToString("o")
  Tools = [ordered]@{
    Ninja = $ninja
    Ml64 = [ordered]@{ Path = $ml64Exe; Version = $(if ($ml64Exe) { Get-FileVersion $ml64Exe } else { "" }) }
    Link = [ordered]@{ Path = $linkExe; Version = $(if ($linkExe) { Get-FileVersion $linkExe } else { "" }) }
  }
  CapturedRsp = @($rspFiles | ForEach-Object { $_.Name })
  CommandsPreview = @($cmds | Select-Object -First 5)
}

$jsonPath = Join-Path $OutDir "msvc_toolchain_capture.json"
$payload | ConvertTo-Json -Depth 6 | Set-Content -Path $jsonPath -Encoding UTF8

Write-Host "OK: Wrote $jsonPath" -Fore Green
