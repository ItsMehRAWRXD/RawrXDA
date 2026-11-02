#Requires -Version 5.1
<#
  STORAGE_AUDIT_001 - READ ONLY. No delete/move/stage/commit/push.
  PROMOTE=0 CERTIFIED=0
#>
$ErrorActionPreference = 'Continue'
$Root = 'F:\rawrxd'
$Out  = 'F:\rawrxd\evidence\STORAGE_AUDIT_001'
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$stamp = (Get-Date).ToUniversalTime().ToString('o')
$log = Join-Path $Out 'AUDIT_RUN.log'
function Log([string]$m) {
  $line = '[{0}] {1}' -f (Get-Date).ToString('o'), $m
  Add-Content -LiteralPath $log -Value $line
  Write-Host $line
}

Log ("START stamp=$stamp root=$Root")

$vol = Get-Volume -DriveLetter F
$volJson = [ordered]@{
  Drive = 'F'
  FileSystem = $vol.FileSystem
  SizeBytes = [int64]$vol.Size
  SizeRemainingBytes = [int64]$vol.SizeRemaining
  SizeGiB = [math]::Round($vol.Size/1GB, 2)
  FreeGiB = [math]::Round($vol.SizeRemaining/1GB, 2)
  UsedGiB = [math]::Round(($vol.Size - $vol.SizeRemaining)/1GB, 2)
}
$volJson | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $Out 'VOLUME.json')
Log ('VOLUME free={0} GiB used={1} GiB' -f $volJson.FreeGiB, $volJson.UsedGiB)

$dummy = Join-Path $Root 'dummy_200b.gguf'
$dummyMd = Join-Path $Out 'DUMMY_200B_ANALYSIS.md'
$dummyLines = New-Object System.Collections.Generic.List[string]
function DL([string]$s) { [void]$script:dummyLines.Add($s) }

$dummyAlloc = 0L
if (Test-Path -LiteralPath $dummy) {
  $fi = Get-Item -LiteralPath $dummy -Force
  $flag = (fsutil sparse queryflag $dummy 2>&1 | Out-String).Trim()
  $layout = cmd /c ('fsutil file layout "' + $dummy + '"') 2>&1 | Out-String
  $alloc = $null
  if ($layout -match 'Allocated Size\s*:\s*([\d,]+)\s*\(400') {
    $alloc = [int64](($Matches[1] -replace ',', ''))
  } elseif ($layout -match '(?s)::\$DATA.*?Allocated Size\s*:\s*([\d,]+)') {
    $alloc = [int64](($Matches[1] -replace ',', ''))
  }
  if (-not $alloc) { $alloc = [int64]$fi.Length }
  $script:dummyAlloc = $alloc
  $isSparse = $false
  if ($flag -match 'NOT set as sparse|NOT sparse') { $isSparse = $false } elseif ($flag -match 'is set as sparse') { $isSparse = $true }

  DL '# DUMMY_200B_ANALYSIS'
  DL ''
  DL ('ASOF_UTC={0}' -f $stamp)
  DL ('Path={0}' -f $dummy)
  DL ('LogicalBytes={0}' -f $fi.Length)
  DL ('LogicalGiB={0:N2}' -f ($fi.Length/1GB))
  DL ('SparseFlag={0}' -f ($flag -replace '[\r\n]+', ' | '))
  DL ('AllocatedBytes={0}' -f $alloc)
  DL ('AllocatedGiB={0:N2}' -f ($alloc/1GB))
  DL ('IsSparse={0}' -f $isSparse)
  DL ('PhysicalEqualsLogical={0}' -f ($alloc -eq $fi.Length))
  DL ''
  DL '## Sibling on G: (separate volume / separate file ID)'
  $gDummy = 'G:\~dev\rawrxd\dummy_200b.gguf'
  if (Test-Path -LiteralPath $gDummy) {
    DL ('AlsoExists={0}' -f $gDummy)
    DL 'Different FileReferenceNumber than F: copy (not a hardlink across volumes).'
    DL 'Deleting F: copy recovers space on F: only; G: copy is independent.'
  } else {
    DL 'G_copy=MISSING'
  }
  DL ''
  DL '## Classification'
  DL 'Name indicates synthetic fixture (dummy_200b).'
  DL 'NOT sparse; fully allocated ~400 GiB on F:.'
  DL 'Disposition: DELETE_CANDIDATE after dependency sign-off (REVIEW until code/test refs cleared).'
  DL 'PROMOTE=0 - no deletion in this audit.'
  $dummyLines | Set-Content -Encoding utf8 $dummyMd
  Log ('Wrote DUMMY_200B_ANALYSIS.md alloc={0}' -f $alloc)
} else {
  Log 'DUMMY missing'
}

Log 'ENUMERATE begin'
$invPath = Join-Path $Out 'STORAGE_INVENTORY.csv'
$largePath = Join-Path $Out 'LARGEST_FILES.csv'
$sw = New-Object System.IO.StreamWriter($invPath, $false, (New-Object System.Text.UTF8Encoding $false))
$sw.WriteLine('path,logical_bytes,extension,mtime_utc,category_guess')
$largeList = New-Object System.Collections.Generic.List[object]
$errors = 0
$fileCount = [int64]0
$logicalTotal = [int64]0
$oneGiB = [int64]1GB
$oneMiB = [int64]1MB
$hundredMiB = [int64]100MB
$sizeMap = @{}

function Get-Category([string]$p, [string]$ext) {
  if ($p -match '\\evidence\\') { return 'evidence' }
  if ($ext -in @('.gguf','.bin','.safetensors','.onnx','.pt','.pth','.ckpt')) { return 'model' }
  if ($ext -in @('.obj','.lib','.exe','.dll','.pdb')) { return 'build' }
  if ($ext -in @('.log','.etl','.dmp','.trace')) { return 'log_trace' }
  if ($p -match '\\(obj|bin|out|build|cmake-build)\\') { return 'build_tree' }
  if ($p -match 'dummy_') { return 'dummy_fixture' }
  return 'other'
}

try {
  foreach ($p in [System.IO.Directory]::EnumerateFiles($Root, '*', [System.IO.SearchOption]::AllDirectories)) {
    try {
      $info = New-Object System.IO.FileInfo $p
      if (-not $info.Exists) { continue }
      $len = [int64]$info.Length
      $fileCount++
      $logicalTotal += $len
      $ext = $info.Extension.ToLowerInvariant()
      $cat = Get-Category $p $ext
      if ($len -ge $oneMiB) {
        $esc = '"' + ($p -replace '"', '""') + '"'
        $mtime = $info.LastWriteTimeUtc.ToString('o')
        $sw.WriteLine(('{0},{1},{2},{3},{4}' -f $esc, $len, $ext, $mtime, $cat))
      }
      if ($len -ge $oneGiB) {
        [void]$largeList.Add([pscustomobject]@{ Path = $p; Logical = $len; Ext = $ext; Cat = $cat; Mtime = $info.LastWriteTimeUtc })
      }
      if ($len -ge $hundredMiB) {
        $k = [string]$len
        if (-not $sizeMap.ContainsKey($k)) { $sizeMap[$k] = New-Object System.Collections.Generic.List[string] }
        [void]$sizeMap[$k].Add($p)
      }
    } catch { $errors++ }
  }
} catch {
  Log ('ENUMERATE fatal: {0}' -f $_.Exception.Message)
}
$sw.Close()
Log ('ENUMERATE done files={0} logicalTotalGiB={1:N2} ge1GiB={2} errors={3}' -f $fileCount, ($logicalTotal/1GB), $largeList.Count, $errors)

$largeRows = @()
foreach ($f in ($largeList | Sort-Object Logical -Descending)) {
  $alloc = $f.Logical
  $sparse = 'unknown'
  try {
    $flag = (fsutil sparse queryflag $f.Path 2>&1 | Out-String)
    if ($flag -match 'NOT set as sparse|NOT sparse') { $sparse = 'false' }
    elseif ($flag -match 'set as sparse') { $sparse = 'true' }
    $layout = cmd /c ('fsutil file layout "' + $f.Path + '"') 2>&1 | Out-String
    if ($layout -match '(?s)Stream\s+:\s+0x080\s+::\$DATA.*?Allocated Size\s*:\s*([\d,]+)') {
      $alloc = [int64](($Matches[1] -replace ',', ''))
    }
  } catch {}
  $largeRows += [pscustomobject]@{
    path = $f.Path
    logical_bytes = $f.Logical
    allocated_bytes = $alloc
    allocated_gib = [math]::Round($alloc/1GB, 3)
    sparse = $sparse
    extension = $f.Ext
    category = $f.Cat
    mtime_utc = $f.Mtime.ToString('o')
  }
}
$largeRows | Export-Csv -NoTypeInformation -Encoding UTF8 $largePath
Log ('Wrote LARGEST_FILES.csv rows={0}' -f $largeRows.Count)

Log 'DUPLICATES begin'
$dupPath = Join-Path $Out 'DUPLICATES.csv'
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
          $n = $fs.Read($buf, 0, $buf.Length)
          $sha = [System.Security.Cryptography.SHA256]::Create()
          $h1 = ([BitConverter]::ToString($sha.ComputeHash($buf, 0, $n)) -replace '-', '')
          [void]$fs.Seek(-[math]::Min($buf.Length, $len), 'End')
          $n2 = $fs.Read($buf, 0, $buf.Length)
          $h2 = ([BitConverter]::ToString($sha.ComputeHash($buf, 0, $n2)) -replace '-', '')
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
      $dupRows += [pscustomobject]@{
        group_id = $groupId
        sha_or_fingerprint = $h
        logical_bytes = $len
        path = $p
        member_count = $plist.Count
      }
    }
  }
}
$dupRows | Export-Csv -NoTypeInformation -Encoding UTF8 $dupPath
Log ('Wrote DUPLICATES.csv rows={0} groups={1}' -f $dupRows.Count, $groupId)

Log 'GIT begin'
$gitCsv = Join-Path $Out 'GIT_CLASSIFICATION.csv'
$gitRows = @()
Push-Location $Root
$inGit = $false
try {
  $t = git rev-parse --is-inside-work-tree 2>$null
  if ($t -eq 'true') { $inGit = $true }
} catch {}
if ($inGit) {
  $porcelain = @(git status --porcelain=v1 -uall 2>$null)
  $ignored = @(git status --porcelain=v1 -uall --ignored 2>$null | Where-Object { $_ -match '^!!' })
  foreach ($line in $porcelain) {
    if ($line.Length -lt 4) { continue }
    $code = $line.Substring(0, 2)
    $path = $line.Substring(3).Trim('"')
    $kind = 'unknown'
    if ($code -eq '??') { $kind = 'untracked' }
    elseif ($code -match '[MADRCUT ]') { $kind = 'tracked_or_dirty' }
    $disp = 'REVIEW'
    $stage = 'NO'
    if ($path -match '\.(gguf|safetensors|onnx)$' -or $path -match 'dummy_') {
      $disp = 'DELETE_CANDIDATE_OR_KEEP_MODEL'; $stage = 'NO_FORCE_ADD'
    } elseif ($path -match 'evidence') {
      $disp = 'KEEP_OR_REVIEW_EVIDENCE'; $stage = 'REVIEW_BEFORE_STAGE'
    } elseif ($path -match '\.(cpp|hpp|h|c|asm|md|ps1|json|cmake|txt)$') {
      $disp = 'KEEP'; $stage = 'SAFE_TO_STAGE_IF_INTENDED'
    } elseif ($path -match '\\(obj|bin)\\' -or $path -match '\.(obj|pdb|exe|dll)$') {
      $disp = 'DELETE_CANDIDATE'; $stage = 'NO'
    }
    $gitRows += [pscustomobject]@{
      status_code = $code; kind = $kind; path = $path
      classification = $disp; stage_advice = $stage
    }
  }
  foreach ($line in $ignored) {
    $path = $line.Substring(3).Trim('"')
    $gitRows += [pscustomobject]@{
      status_code = '!!'; kind = 'ignored'; path = $path
      classification = 'REVIEW_IGNORED'; stage_advice = 'NO_FORCE_ADD'
    }
  }
  Log ('GIT porcelain={0} ignored_lines={1}' -f $porcelain.Count, $ignored.Count)
} else {
  Log 'Not a git work tree at F:\rawrxd'
  $gitRows += [pscustomobject]@{
    status_code = ''; kind = 'n/a'; path = 'F:\rawrxd'
    classification = 'NOT_A_GIT_ROOT'; stage_advice = 'CHECK_G_DEV_MIRROR'
  }
}
Pop-Location
$gitRows | Export-Csv -NoTypeInformation -Encoding UTF8 $gitCsv

$dupRecover = [int64]0
foreach ($g in ($dupRows | Group-Object group_id)) {
  $bytes = [int64]$g.Group[0].logical_bytes
  $extra = $g.Count - 1
  if ($extra -gt 0) { $dupRecover += ($bytes * $extra) }
}

$rec = [ordered]@{
  asof_utc = $stamp
  volume_free_gib = $volJson.FreeGiB
  volume_used_gib = $volJson.UsedGiB
  inventory_file_count = $fileCount
  inventory_logical_gib = [math]::Round($logicalTotal/1GB, 2)
  notes = @(
    'Physical recovery excludes hard-link double-counting (not fully enumerated).',
    'G:\~dev\rawrxd\dummy_200b.gguf is a SEPARATE 400 GiB allocation on G: - not included in F: recovery.',
    'DELETE requires explicit approval of CLEANUP_PLAN batches.',
    'PROMOTE=0 CERTIFIED=0'
  )
  low_risk = [ordered]@{
    description = 'Verified disposable dummy on F: if unused + rebuildable obj/pdb after REVIEW'
    dummy_200b_f_allocated_bytes = $dummyAlloc
    dummy_200b_f_allocated_gib = [math]::Round($dummyAlloc/1GB, 2)
    duplicate_extra_copies_logical_bytes_estimate = $dupRecover
    duplicate_extra_copies_gib_estimate = [math]::Round($dupRecover/1GB, 2)
    caveat = 'Dummy deletion only after dependency sign-off'
  }
  medium_risk = [ordered]@{
    description = 'Old build trees, redundant logs/dumps, superseded evidence artifacts'
    status = 'See CLEANUP_PLAN.md REVIEW section'
  }
  high_risk = [ordered]@{
    description = 'Unique models, sealed certification evidence, source'
    recoverable_gib = 0
    action = 'KEEP'
  }
}
$rec | ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 (Join-Path $Out 'RECOVERY_ESTIMATE.json')

$plan = New-Object System.Collections.Generic.List[string]
[void]$plan.Add('# CLEANUP_PLAN - STORAGE_AUDIT_001')
[void]$plan.Add('')
[void]$plan.Add('ASOF_UTC=' + $stamp)
[void]$plan.Add('MODE=READ_ONLY_AUDIT')
[void]$plan.Add('PROMOTE=0')
[void]$plan.Add('CERTIFIED=0')
[void]$plan.Add('NO_DELETION_WITHOUT_BATCH_APPROVAL')
[void]$plan.Add('')
[void]$plan.Add('## Policy')
[void]$plan.Add('- KEEP: unique source, configs, manifests, authoritative binaries, active models, required fixtures, sealed certification evidence')
[void]$plan.Add('- REVIEW: duplicate models, obsolete builds, superseded evidence, large dumps, old fixtures')
[void]$plan.Add('- DELETE_CANDIDATE: verified disposable dummy, reproducible caches, redundant temp, rebuildable outputs without evidentiary purpose')
[void]$plan.Add('- Git option 1: do NOT force-add ignored files; inspect untracked individually')
[void]$plan.Add('')
[void]$plan.Add('## Batch 1 - dummy_200b.gguf on F: (highest priority)')
[void]$plan.Add('- Path: ' + $dummy)
[void]$plan.Add(('- Allocated: {0:N2} GiB (NOT sparse)' -f ($dummyAlloc/1GB)))
[void]$plan.Add('- Risk: LOW if unused; MEDIUM until test/code dependency scan completes')
[void]$plan.Add('- Note: G:\~dev\rawrxd\dummy_200b.gguf is a separate 400 GiB file on G: (independent recovery)')
[void]$plan.Add('- Action: PENDING APPROVAL - see DUMMY_200B_ANALYSIS.md')
[void]$plan.Add('')
[void]$plan.Add('## Batch 2 - hash-verified duplicate extras (>=100 MiB groups)')
[void]$plan.Add(('- Extra-copy logical estimate: {0:N2} GiB' -f ($dupRecover/1GB)))
[void]$plan.Add('- Keep one authoritative copy per group; delete extras only after path review')
[void]$plan.Add('- See DUPLICATES.csv')
[void]$plan.Add('')
[void]$plan.Add('## Batch 3 - files >= 1 GiB on F:\rawrxd')
foreach ($r in ($largeRows | Select-Object -First 40)) {
  [void]$plan.Add(('- {0} alloc={1:N2} GiB sparse={2} cat={3}' -f $r.path, $r.allocated_gib, $r.sparse, $r.category))
}
[void]$plan.Add('')
[void]$plan.Add('## Batch 4 - build outputs / logs')
[void]$plan.Add('- Classify rebuildable .obj/.pdb and old logs as DELETE_CANDIDATE after confirming no evidence seal depends on them')
[void]$plan.Add('')
[void]$plan.Add('## Git')
[void]$plan.Add('- See GIT_CLASSIFICATION.csv')
[void]$plan.Add('- SAFE_TO_STAGE only for intended source/config; never force-add ignored models')
$plan | Set-Content -Encoding utf8 (Join-Path $Out 'CLEANUP_PLAN.md')

$sum = @(
  '# AUDIT_SUMMARY'
  ''
  ('ASOF_UTC={0}' -f $stamp)
  ('F_free_GiB={0}' -f $volJson.FreeGiB)
  ('F_used_GiB={0}' -f $volJson.UsedGiB)
  ('files_enumerated={0}' -f $fileCount)
  ('logical_total_GiB={0:N2}' -f ($logicalTotal/1GB))
  ('files_ge_1GiB={0}' -f $largeRows.Count)
  ('dummy_F_allocated_GiB={0:N2}' -f ($dummyAlloc/1GB))
  'dummy_sparse=False'
  ('dup_extra_gib_est={0:N2}' -f ($dupRecover/1GB))
  ''
  '## Deliverables'
  '- STORAGE_INVENTORY.csv (files >=1 MiB)'
  '- LARGEST_FILES.csv'
  '- DUPLICATES.csv'
  '- GIT_CLASSIFICATION.csv'
  '- CLEANUP_PLAN.md'
  '- RECOVERY_ESTIMATE.json'
  '- DUMMY_200B_ANALYSIS.md'
  ''
  '## Immediate finding'
  'F:\rawrxd\dummy_200b.gguf occupies a full 400 GiB of physical clusters (not sparse).'
  'G:\~dev\rawrxd\dummy_200b.gguf is a SECOND independent 400 GiB allocation on G:.'
  ''
  'NO DELETIONS PERFORMED.'
)
$sum | Set-Content -Encoding utf8 (Join-Path $Out 'AUDIT_SUMMARY.md')

$note = @"
STORAGE_INVENTORY.csv includes files >= 1 MiB only.
Total files enumerated (all sizes): $fileCount
Total logical bytes: $logicalTotal
"@
Set-Content -Encoding utf8 (Join-Path $Out 'INVENTORY_SCOPE.txt') $note

Log 'DONE'
Write-Host "OUT=$Out"
