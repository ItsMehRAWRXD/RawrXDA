param(
  [string]$ExePath = "D:\\rawrxd\\RawrXD_Gold.exe",
  [string]$OutDir = "D:\\rawrxd\\build_inhouse\\re_backend\\pe_gold",
  [string]$DumpbinExe = ""
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

if (-not (Test-Path $ExePath)) { throw "Exe not found: $ExePath" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

function Resolve-Dumpbin {
  param([string]$Hint)
  if ($Hint -and (Test-Path $Hint)) { return (Resolve-Path $Hint).Path }
  $cmd = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  $hits = Get-ChildItem "${env:ProgramFiles(x86)}\\Microsoft Visual Studio\\2022\\*\\VC\\Tools\\MSVC" -Recurse -Filter dumpbin.exe -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -like "*\\bin\\Hostx64\\x64\\dumpbin.exe" } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
  if ($hits) { return $hits.FullName }
  throw "dumpbin.exe not found"
}

$dumpbin = Resolve-Dumpbin $DumpbinExe

$base = [IO.Path]::GetFileNameWithoutExtension($ExePath)
$headersPath = Join-Path $OutDir "$base.headers.txt"
$importsPath = Join-Path $OutDir "$base.imports.txt"
$loadCfgPath = Join-Path $OutDir "$base.loadconfig.txt"
$relocPath = Join-Path $OutDir "$base.reloc.txt"

& $dumpbin /nologo /headers $ExePath | Set-Content -Path $headersPath -Encoding UTF8
& $dumpbin /nologo /imports $ExePath | Set-Content -Path $importsPath -Encoding UTF8
& $dumpbin /nologo /loadconfig $ExePath | Set-Content -Path $loadCfgPath -Encoding UTF8
& $dumpbin /nologo /relocations $ExePath | Set-Content -Path $relocPath -Encoding UTF8

$headers = Get-Content $headersPath

function Find-Value([string]$pattern) {
  ($headers | Select-String -Pattern $pattern | Select-Object -First 1).Line
}

$machineLine = Find-Value "machine \("
$subsystemLine = Find-Value "subsystem"
$imageBaseLine = Find-Value "image base"
$entryLine = Find-Value "entry point"
$dllCharsLine = Find-Value "DLL characteristics"

$importsText = Get-Content $importsPath -Raw
$dlls = @()
foreach ($m in [regex]::Matches($importsText, '(?im)^\\s+([A-Z0-9_\\-]+\\.dll)\\s*$')) {
  $dlls += $m.Groups[1].Value
}
$dlls = $dlls | Sort-Object -Unique

$manifest = [ordered]@{
  generatedAtUtc = [DateTime]::UtcNow.ToString("o")
  exePath = (Resolve-Path $ExePath).Path
  dumpbin = $dumpbin
  machine = ($machineLine -replace '^.*machine \\(([^\\)]+)\\).*$','$1').Trim()
  subsystem = ($subsystemLine -replace '^.*subsystem\\s+','').Trim()
  imageBase = ($imageBaseLine -replace '^.*image base\\s+','').Trim()
  entryPoint = ($entryLine -replace '^.*entry point\\s+','').Trim()
  dllCharacteristics = ($dllCharsLine -replace '^.*DLL characteristics\\s+','').Trim()
  importedDlls = $dlls
  artifacts = [ordered]@{
    headers = $headersPath
    imports = $importsPath
    loadconfig = $loadCfgPath
    relocations = $relocPath
  }
}

$jsonPath = Join-Path $OutDir "$base.pe_manifest.json"
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Path $jsonPath -Encoding UTF8
Write-Host "OK: wrote $jsonPath" -Fore Green

