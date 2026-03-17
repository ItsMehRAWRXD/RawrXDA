param(
  [string]$InDir = "D:\\rawrxd\\build_inhouse\\re_backend\\coff_batches",
  [string]$OutPath = "D:\\rawrxd\\build_inhouse\\re_backend\\link_requirements_summary.json"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$InDir = (Resolve-Path $InDir).Path
$jsonFiles = Get-ChildItem -LiteralPath $InDir -File -Filter "batch_*.json" | Sort-Object Name
if (-not $jsonFiles -or $jsonFiles.Count -eq 0) { throw "No batch_*.json under $InDir" }

$machine = @{}
$defaultLibs = @{}
$exports = @{}
$entrypoints = @{}
$undefCounts = @{}
$sectionChars = @{}
$objCount = 0

function AddCount([hashtable]$h, [string]$k, [int]$n = 1) {
  if ([string]::IsNullOrWhiteSpace($k)) { return }
  if ($h.ContainsKey($k)) { $h[$k] += $n } else { $h[$k] = $n }
}

function Parse-Drectve([string]$d) {
  if (-not $d) { return @() }
  # Keep it simple: split on whitespace, preserve quoted tokens.
  $tokens = @()
  $cur = ""
  $inQuote = $false
  foreach ($ch in $d.ToCharArray()) {
    if ($ch -eq '"') { $inQuote = -not $inQuote; $cur += $ch; continue }
    if (-not $inQuote -and [char]::IsWhiteSpace($ch)) {
      if ($cur.Length) { $tokens += $cur; $cur = "" }
      continue
    }
    $cur += $ch
  }
  if ($cur.Length) { $tokens += $cur }
  return $tokens
}

foreach ($f in $jsonFiles) {
  $batch = Get-Content $f.FullName -Raw | ConvertFrom-Json
  foreach ($o in $batch) {
    $objCount++
    AddCount $machine $o.machine

    foreach ($u in ($o.undefinedExternals | Where-Object { $_ -and $_ -ne "ERROR" })) {
      AddCount $undefCounts $u
    }

    if ($o.drectve) {
      $tokens = Parse-Drectve $o.drectve
      foreach ($t in $tokens) {
        if ($t -match '^/DEFAULTLIB:(.+)$') { AddCount $defaultLibs $Matches[1]; continue }
        if ($t -match '^/EXPORT:(.+)$') { AddCount $exports $Matches[1]; continue }
        if ($t -match '^/ENTRY:(.+)$') { AddCount $entrypoints $Matches[1]; continue }
      }
    }

    foreach ($s in $o.sections) {
      $key = ("{0} 0x{1:X8}" -f $s.name, [uint32]$s.characteristics)
      AddCount $sectionChars $key
    }
  }
}

function TopN([hashtable]$h, [int]$n) {
  $h.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First $n | ForEach-Object {
    [pscustomobject]@{ key = $_.Key; count = [int]$_.Value }
  }
}

$summary = [ordered]@{
  generatedAtUtc = [DateTime]::UtcNow.ToString("o")
  inputDir = $InDir
  objectsAnalyzed = $objCount
  machines = (TopN $machine 20)
  defaultLibs = (TopN $defaultLibs 50)
  entrypoints = (TopN $entrypoints 20)
  exports = (TopN $exports 50)
  topUndefinedExternals = (TopN $undefCounts 200)
  sectionCharacteristics = (TopN $sectionChars 200)
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutPath) | Out-Null
$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $OutPath -Encoding UTF8
Write-Host "OK: wrote $OutPath" -Fore Green

