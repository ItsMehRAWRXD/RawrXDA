param(
  [int]$BatchSize = 900,
  [int]$MaxCommits = 10
)

$ErrorActionPreference = "Stop"

Set-Location (Split-Path -Parent $MyInvocation.MyCommand.Path)

$lastMsg = git log -1 --pretty=%s
$part = 1
if ($lastMsg -match "part\s+(\d+)$") {
  $part = [int]$matches[1] + 1
}

$made = 0
while ($made -lt $MaxCommits) {
  $paths = @()
  $paths += (git -c core.quotePath=false ls-files -m)
  $paths += (git -c core.quotePath=false ls-files -d)
  $paths += (git -c core.quotePath=false ls-files --others --exclude-standard)
  $paths = $paths | Where-Object { $_ -and $_.Trim() -ne "" -and $_ -notlike ".codex_*" }

  if (-not $paths -or $paths.Count -eq 0) {
    break
  }

  $take = [Math]::Min($BatchSize, $paths.Count)
  $batch = $paths | Select-Object -First $take
  $batchFile = ".codex_batch_paths.txt"
  $batch | Set-Content $batchFile

  git add --pathspec-from-file=$batchFile
  if ($LASTEXITCODE -ne 0) {
    throw "git add failed"
  }

  git commit -m "sync: batched repository consolidation part $part"
  if ($LASTEXITCODE -ne 0) {
    throw "git commit failed"
  }

  Write-Output "committed_part=$part files=$take remaining_estimate=$($paths.Count - $take)"
  $part++
  $made++
}

Write-Output "commits_made=$made"
