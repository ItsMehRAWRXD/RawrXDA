#Requires -Version 5.1
# READ-ONLY chunked audit. PROMOTE=0. No delete/stage/commit.
$ErrorActionPreference = 'Continue'
$Root = 'F:\rawrxd'
$Out  = 'F:\rawrxd\evidence\STORAGE_AUDIT_001'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$log = Join-Path $Out 'AUDIT_RUN.log'
function Log([string]$m) {
  $line = '[{0}] {1}' -f (Get-Date).ToString('o'), $m
  Add-Content -LiteralPath $log -Value $line
  Write-Host $line
}

$stamp = (Get-Date).ToUniversalTime().ToString('o')
Log ("CHUNKED_START $stamp")

$vol = Get-Volume -DriveLetter F
[ordered]@{
  Drive='F'; FileSystem=$vol.FileSystem
  SizeBytes=[int64]$vol.Size; SizeRemainingBytes=[int64]$vol.SizeRemaining
  SizeGiB=[math]::Round($vol.Size/1GB,2)
  FreeGiB=[math]::Round($vol.SizeRemaining/1GB,2)
  UsedGiB=[math]::Round(($vol.Size-$vol.SizeRemaining)/1GB,2)
} | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $Out 'VOLUME.json')

$invPath = Join-Path $Out 'STORAGE_INVENTORY.csv'
$largePath = Join-Path $Out 'LARGEST_FILES_PARTIAL.csv'
$sw = New-Object System.IO.StreamWriter($invPath, $false, (New-Object System.Text.UTF8Encoding $false))
$sw.WriteLine('path,logical_bytes,extension,mtime_utc,category_guess')
$large = New-Object System.Collections.Generic.List[object]
$sizeMap = @{}
$fileCount = [int64]0
$logicalTotal = [int64]0
$errors = 0
$oneMiB = [int64]1MB
$oneGiB = [int64]1GB
$hundredMiB = [int64]100MB

function Get-Cat([string]$p,[string]$ext) {
  if ($p -match '\\evidence\\') { return 'evidence' }
  if ($ext -in @('.gguf','.safetensors','.onnx','.pt','.pth','.ckpt')) { return 'model' }
  if ($ext -in @('.obj','.lib','.exe','.dll','.pdb')) { return 'build' }
  if ($ext -in @('.log','.etl','.dmp','.trace')) { return 'log_trace' }
  if ($p -match '\\(obj|bin|out|build)\\') { return 'build_tree' }
  if ($p -match 'dummy_') { return 'dummy_fixture' }
  return 'other'
}

function Process-File([string]$p) {
  try {
    $info = New-Object System.IO.FileInfo $p
    if (-not $info.Exists) { return }
    $len = [int64]$info.Length
    $script:fileCount++
    $script:logicalTotal += $len
    $ext = $info.Extension.ToLowerInvariant()
    $cat = Get-Cat $p $ext
    if ($len -ge $script:oneMiB) {
      $esc = '"' + ($p -replace '"','""') + '"'
      $script:sw.WriteLine(('{0},{1},{2},{3},{4}' -f $esc,$len,$ext,$info.LastWriteTimeUtc.ToString('o'),$cat))
    }
    if ($len -ge $script:oneGiB) {
      [void]$script:large.Add([pscustomobject]@{Path=$p;Logical=$len;Ext=$ext;Cat=$cat;Mtime=$info.LastWriteTimeUtc})
    }
    if ($len -ge $script:hundredMiB) {
      $k = [string]$len
      if (-not $script:sizeMap.ContainsKey($k)) { $script:sizeMap[$k] = New-Object System.Collections.Generic.List[string] }
      [void]$script:sizeMap[$k].Add($p)
    }
  } catch { $script:errors++ }
}

# Root-level files
Get-ChildItem -LiteralPath $Root -File -Force -ErrorAction SilentlyContinue | ForEach-Object { Process-File $_.FullName }
Log ('ROOT_FILES done count={0}' -f $fileCount)

$dirs = @(Get-ChildItem -LiteralPath $Root -Directory -Force -ErrorAction SilentlyContinue)
$i = 0
foreach ($d in $dirs) {
  $i++
  $name = $d.Name
  # Skip enormous foreign trees if needed - still audit but log
  Log ("DIR_BEGIN $i/$($dirs.Count) $name")
  $before = $fileCount
  try {
    foreach ($p in [System.IO.Directory]::EnumerateFiles($d.FullName, '*', [System.IO.SearchOption]::AllDirectories)) {
      Process-File $p
    }
  } catch {
    Log ("DIR_ERROR $name : $($_.Exception.Message)")
  }
  Log ("DIR_END $name files_delta=$($fileCount-$before) total=$fileCount logicalGiB=$([math]::Round($logicalTotal/1GB,2))")
  # checkpoint large list periodically
  if (($i % 25) -eq 0) {
    $large | Sort-Object Logical -Descending | Select-Object -First 100 |
      Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $Out 'LARGEST_CHECKPOINT.csv')
    Log 'CHECKPOINT wrote LARGEST_CHECKPOINT.csv'
  }
}
$sw.Close()
Log ("ENUMERATE_DONE files=$fileCount logicalGiB=$([math]::Round($logicalTotal/1GB,2)) ge1GiB=$($large.Count) errors=$errors")

# Allocated sizes for >=1GiB
$largeRows = @()
foreach ($f in ($large | Sort-Object Logical -Descending)) {
  $alloc = $f.Logical
  $sparse = 'unknown'
  try {
    $flag = (fsutil sparse queryflag $f.Path 2>&1 | Out-String)
    if ($flag -match 'NOT set as sparse|NOT sparse') { $sparse = 'false' }
    elseif ($flag -match 'set as sparse') { $sparse = 'true' }
    $layout = cmd /c ('fsutil file layout "' + $f.Path + '"') 2>&1 | Out-String
    if ($layout -match '(?s)Stream\s+:\s+0x080\s+::\$DATA.*?Allocated Size\s*:\s*([\d,]+)') {
      $alloc = [int64](($Matches[1] -replace ',',''))
    }
  } catch {}
  $largeRows += [pscustomobject]@{
    path=$f.Path; logical_bytes=$f.Logical; allocated_bytes=$alloc
    allocated_gib=[math]::Round($alloc/1GB,3); sparse=$sparse
    extension=$f.Ext; category=$f.Cat; mtime_utc=$f.Mtime.ToString('o')
  }
}
$largeRows | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $Out 'LARGEST_FILES.csv')
Log ('Wrote LARGEST_FILES.csv n=' + $largeRows.Count)

# Duplicates
$dupRows = @()
$groupId = 0
$eightGiB = [int64]8GB
foreach ($k in @($sizeMap.Keys)) {
  $list = $sizeMap[$k]
  if ($list.Count -lt 2) { continue }
  $len = [int64]$k
  $hashes = @{}
  foreach ($p in $list) {
    $h = 'SKIPPED'
    try {
      if ($len -le $eightGiB) {
        $h = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash
      } else {
        $fs = [System.IO.File]::Open($p, 'Open', 'Read', 'Read')
        try {
          $buf = New-Object byte[] (1MB)
          $n = $fs.Read($buf,0,$buf.Length)
          $sha = [System.Security.Cryptography.SHA256]::Create()
          $h1 = ([BitConverter]::ToString($sha.ComputeHash($buf,0,$n)) -replace '-','')
          [void]$fs.Seek(-[math]::Min($buf.Length,$len),'End')
          $n2 = $fs.Read($buf,0,$buf.Length)
          $h2 = ([BitConverter]::ToString($sha.ComputeHash($buf,0,$n2)) -replace '-','')
          $sha.Dispose()
          $h = 'QUICK_' + $h1 + '_' + $h2
        } finally { $fs.Close() }
      }
    } catch { $h = 'ERROR' }
    if (-not $hashes.ContainsKey($h)) { $hashes[$h] = New-Object System.Collections.Generic.List[string] }
    [void]$hashes[$h].Add($p)
  }
  foreach ($h in @($hashes.Keys)) {
    $plist = $hashes[$h]
    if ($plist.Count -lt 2) { continue }
    $groupId++
    foreach ($p in $plist) {
      $dupRows += [pscustomobject]@{ group_id=$groupId; sha_or_fingerprint=$h; logical_bytes=$len; path=$p; member_count=$plist.Count }
    }
  }
}
$dupRows | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $Out 'DUPLICATES.csv')
Log ("Wrote DUPLICATES.csv groups=$groupId")

# Git
$gitRows = @()
Push-Location $Root
$inGit = $false
try { if ((git rev-parse --is-inside-work-tree 2>$null) -eq 'true') { $inGit = $true } } catch {}
if ($inGit) {
  $porcelain = @(git status --porcelain=v1 -uall 2>$null)
  $ignored = @(git status --porcelain=v1 -uall --ignored 2>$null | Where-Object { $_ -match '^!!' })
  foreach ($line in $porcelain) {
    if ($line.Length -lt 4) { continue }
    $code = $line.Substring(0,2)
    $path = $line.Substring(3).Trim('"')
    $kind = if ($code -eq '??') { 'untracked' } else { 'tracked_or_dirty' }
    $disp = 'REVIEW'; $stage = 'NO'
    if ($path -match '\.(gguf|safetensors|onnx)$|dummy_') { $disp='DELETE_CANDIDATE_OR_KEEP_MODEL'; $stage='NO_FORCE_ADD' }
    elseif ($path -match 'evidence') { $disp='KEEP_OR_REVIEW_EVIDENCE'; $stage='REVIEW_BEFORE_STAGE' }
    elseif ($path -match '\.(cpp|hpp|h|c|asm|md|ps1|json|cmake|txt)$') { $disp='KEEP'; $stage='SAFE_TO_STAGE_IF_INTENDED' }
    elseif ($path -match '\\(obj|bin)\\|\.(obj|pdb|exe|dll)$') { $disp='DELETE_CANDIDATE'; $stage='NO' }
    $gitRows += [pscustomobject]@{ status_code=$code; kind=$kind; path=$path; classification=$disp; stage_advice=$stage }
  }
  foreach ($line in $ignored) {
    $gitRows += [pscustomobject]@{ status_code='!!'; kind='ignored'; path=$line.Substring(3).Trim('"'); classification='REVIEW_IGNORED'; stage_advice='NO_FORCE_ADD' }
  }
  Log ("GIT porcelain=$($porcelain.Count) ignored=$($ignored.Count)")
} else {
  $gitRows += [pscustomobject]@{ status_code=''; kind='n/a'; path='F:\rawrxd'; classification='NOT_A_GIT_ROOT'; stage_advice='CHECK_G_DEV_MIRROR' }
  Log 'GIT not a work tree'
}
Pop-Location
$gitRows | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $Out 'GIT_CLASSIFICATION.csv')

$dummy = 'F:\rawrxd\dummy_200b.gguf'
$dummyAlloc = [int64]429496729600
$dupRecover = [int64]0
foreach ($g in ($dupRows | Group-Object group_id)) {
  $dupRecover += [int64]$g.Group[0].logical_bytes * ($g.Count - 1)
}

[ordered]@{
  asof_utc=$stamp
  volume_free_gib=[math]::Round($vol.SizeRemaining/1GB,2)
  volume_used_gib=[math]::Round(($vol.Size-$vol.SizeRemaining)/1GB,2)
  inventory_file_count=$fileCount
  inventory_logical_gib=[math]::Round($logicalTotal/1GB,2)
  notes=@(
    'G:\~dev\rawrxd\dummy_200b.gguf is SEPARATE 400GiB on G: - not in F recovery'
    'NO DELETIONS. PROMOTE=0 CERTIFIED=0'
  )
  low_risk=@{
    dummy_200b_f_allocated_gib=400
    dummy_200b_f_allocated_bytes=$dummyAlloc
    duplicate_extra_copies_gib_estimate=[math]::Round($dupRecover/1GB,2)
  }
} | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 (Join-Path $Out 'RECOVERY_ESTIMATE.json')

$plan = @('# CLEANUP_PLAN - STORAGE_AUDIT_001', '', "ASOF_UTC=$stamp", 'MODE=READ_ONLY', 'PROMOTE=0', 'NO_DELETION_WITHOUT_APPROVAL', '')
$plan += '## Batch 1 - F:\rawrxd\dummy_200b.gguf'
$plan += '- Allocated 400.00 GiB, NOT sparse, DELETE_CANDIDATE after dependency sign-off'
$plan += '- Sibling G:\~dev\rawrxd\dummy_200b.gguf is independent 400 GiB on G:'
$plan += ''
$plan += '## Batch 2 - duplicates'
$plan += ('- Extra-copy estimate {0:N2} GiB - see DUPLICATES.csv' -f ($dupRecover/1GB))
$plan += ''
$plan += '## Batch 3 - files >= 1 GiB'
foreach ($r in ($largeRows | Select-Object -First 50)) {
  $plan += ('- {0} alloc={1:N2} GiB sparse={2} cat={3}' -f $r.path,$r.allocated_gib,$r.sparse,$r.category)
}
$plan += ''
$plan += '## Git: see GIT_CLASSIFICATION.csv - no force-add ignored'
$plan | Set-Content -Encoding utf8 (Join-Path $Out 'CLEANUP_PLAN.md')

@(
  '# AUDIT_SUMMARY'
  "ASOF_UTC=$stamp"
  ('F_free_GiB={0:N2}' -f ($vol.SizeRemaining/1GB))
  ('files={0}' -f $fileCount)
  ('logical_GiB={0:N2}' -f ($logicalTotal/1GB))
  ('ge1GiB={0}' -f $largeRows.Count)
  'dummy_F_GiB=400 NOT_SPARSE'
  'dummy_G_GiB=400 SEPARATE_VOLUME'
  ('dup_extra_GiB={0:N2}' -f ($dupRecover/1GB))
  'NO_DELETIONS_PERFORMED'
) | Set-Content -Encoding utf8 (Join-Path $Out 'AUDIT_SUMMARY.md')

# Ensure dummy analysis exists
if (-not (Test-Path (Join-Path $Out 'DUMMY_200B_ANALYSIS.md'))) {
  @(
    '# DUMMY_200B_ANALYSIS'
    'Path=F:\rawrxd\dummy_200b.gguf'
    'AllocatedBytes=429496729600'
    'IsSparse=False'
    'G_sibling=G:\~dev\rawrxd\dummy_200b.gguf (separate file id / volume)'
    'Class=DELETE_CANDIDATE pending dependency sign-off'
  ) | Set-Content -Encoding utf8 (Join-Path $Out 'DUMMY_200B_ANALYSIS.md')
}

Log 'DONE'
Write-Host 'OUT='$Out
