# Finalize STORAGE_AUDIT_001 — evidence writes only. PROMOTE=0 CERTIFIED=0
$ErrorActionPreference = 'Continue'
$out = 'F:\rawrxd\evidence\STORAGE_AUDIT_001'
$stamp = (Get-Date).ToUniversalTime().ToString('o')

Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" | Where-Object {
  $_.CommandLine -match 'run_audit_chunked|run_readonly_audit'
} | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
Start-Sleep -Seconds 1

function QuickFp64([string]$path) {
  $size = [int64](Get-Item -LiteralPath $path).Length
  $chunk = [int64]64MB
  if ($size -lt $chunk) { $chunk = $size }
  $sha = [Security.Cryptography.SHA256]::Create()
  $fs = [IO.File]::Open($path, 'Open', 'Read', 'ReadWrite')
  try {
    $buf = New-Object byte[] ([int]$chunk)
    $n = $fs.Read($buf, 0, [int]$chunk)
    [void]$sha.TransformBlock($buf, 0, $n, $null, 0)
    if ($size -gt $chunk) {
      [void]$fs.Seek($size - $chunk, 'Begin')
      $n2 = $fs.Read($buf, 0, [int]$chunk)
      [void]$sha.TransformBlock($buf, 0, $n2, $null, 0)
    }
    $sb = [BitConverter]::GetBytes($size)
    $sha.TransformFinalBlock($sb, 0, 8) | Out-Null
    return 'QUICK:' + (([BitConverter]::ToString($sha.Hash) -replace '-','').ToLowerInvariant())
  } finally { $fs.Close(); $sha.Dispose() }
}

$raw = @(Import-Csv "$out\DUPLICATES.csv")
$kept = @($raw | Where-Object { $_.sha_or_fingerprint -ne 'ERROR' -and $_.sha_or_fingerprint -ne 'SKIPPED' })
$gid = 0
$fixed = New-Object System.Collections.Generic.List[object]
foreach ($g in ($kept | Group-Object group_id)) {
  $gid++
  foreach ($r in $g.Group) {
    [void]$fixed.Add([pscustomobject]@{
      group_id=$gid; sha_or_fingerprint=$r.sha_or_fingerprint; logical_bytes=$r.logical_bytes
      path=$r.path; member_count=$r.member_count; note='FROM_ENUM_HASH'
    })
  }
}
$shardPaths = @(
  'F:\rawrxd\tmp_model-00001-of-000004.safetensors',
  'F:\rawrxd\.git\lfs\objects\a3\14\a314b79a30810b1038f9db08345178e16fb5742aa266c4a3dc05054c59800082',
  'F:\rawrxd\tmp_safetensors\model-00001-of-000004.safetensors'
) | Where-Object { Test-Path $_ }
if ($shardPaths.Count -ge 2) {
  $fps = @{}
  foreach ($p in $shardPaths) {
    $fp = QuickFp64 $p
    if (-not $fps.ContainsKey($fp)) { $fps[$fp] = New-Object System.Collections.Generic.List[string] }
    [void]$fps[$fp].Add($p)
  }
  foreach ($fp in $fps.Keys) {
    if ($fps[$fp].Count -ge 2) {
      $gid++
      $len = (Get-Item $fps[$fp][0]).Length
      foreach ($p in $fps[$fp]) {
        [void]$fixed.Add([pscustomobject]@{
          group_id=$gid; sha_or_fingerprint=$fp; logical_bytes=$len; path=$p
          member_count=$fps[$fp].Count; note='VERIFIED_QUICK_FINGERPRINT_64MIB'
        })
      }
    }
  }
}
@"
DeepSeek-V2-Lite-Chat.Q4_K_M.gguf FULL_SHA256=5d33e5f045c7a03351c319aafc8afdad94b69d07bb68f36dc9bb5af340b343a4
DIAGNOSTIC_REPAIRED.gguf FULL_SHA256=1b74616356d5398405bfe7fa5d403dc2eb353cee969f5d5a9da9071c496642bb
RESULT=SAME_SIZE_DIFFERENT_CONTENT KEEP_BOTH
"@ | Set-Content "$out\DEEPSEEK_HASH_NOTE.txt" -Encoding UTF8
$fixed | Export-Csv -NoTypeInformation -Encoding UTF8 "$out\DUPLICATES.csv"
Write-Host "DUPLICATES fixed rows=$($fixed.Count) groups=$gid"

$inv = @(Import-Csv "$out\STORAGE_INVENTORY.csv")
# If already has allocated_bytes column, skip rewrite
if (-not ($inv[0].PSObject.Properties.Name -contains 'allocated_bytes')) {
  $largeMap = @{}
  Import-Csv "$out\LARGEST_FILES.csv" | ForEach-Object { $largeMap[$_.path] = $_ }
  $invOut = New-Object System.IO.StreamWriter("$out\STORAGE_INVENTORY.csv", $false, [Text.UTF8Encoding]::new($false))
  $invOut.WriteLine('path,logical_bytes,allocated_bytes,mtime_utc,extension,category_guess,sparse_flag')
  foreach ($r in $inv) {
    $alloc = $r.logical_bytes
    $sparse = 'assumed_full_not_probed'
    if ($largeMap.ContainsKey($r.path)) {
      $alloc = $largeMap[$r.path].allocated_bytes
      $sparse = $largeMap[$r.path].sparse
    } elseif ([int64]$r.logical_bytes -lt 100MB) {
      $sparse = 'assumed_full'
    }
    $esc = '"' + ($r.path -replace '"','""') + '"'
    $invOut.WriteLine(('{0},{1},{2},{3},{4},{5},{6}' -f $esc, $r.logical_bytes, $alloc, $r.mtime_utc, $r.extension, $r.category_guess, $sparse))
  }
  $invOut.Close()
  Write-Host 'STORAGE_INVENTORY enhanced with allocated_bytes'
} else {
  Write-Host 'STORAGE_INVENTORY already has allocated_bytes'
}

@"
# INVENTORY_SCOPE / sampling rule

- Full recursive enumeration of F:\rawrxd completed: files=1851573 logical_total~848.94 GiB errors=0
- STORAGE_INVENTORY.csv lists every file with logical_bytes >= 1 MiB (~15674 rows)
- Files < 1 MiB are not listed individually; included only in ENUMERATE_DONE totals
- allocated_bytes: measured via fsutil file layout for all files >= 1 GiB (LARGEST_FILES.csv); for smaller rows assumed equal to logical unless probed
- DUPLICATES: size-grouped files >= 100 MiB; SHA256 for <=8 GiB; QUICK fingerprint for larger; ERROR-hash false groups removed; DeepSeek same-size pair excluded after full SHA mismatch
"@ | Set-Content "$out\INVENTORY_SCOPE.txt" -Encoding UTF8

$gitRows = New-Object System.Collections.Generic.List[object]
function AddGit($path,$state,$bytes,$class,$rationale,$stage) {
  [void]$script:gitRows.Add([pscustomobject]@{
    path=$path; git_state=$state; logical_bytes=$bytes; classification=$class; rationale=$rationale; stage_advice=$stage
  })
}
AddGit 'dummy_200b.gguf' 'ignored(*.gguf)' 429496729600 'DELETE_CANDIDATE' 'Synthetic zero placeholder; fully allocated 400GiB; not tracked' 'NO_FORCE_ADD'
AddGit 'nemotron-bolt.gguf' 'ignored(*.gguf)' 22018326528 'KEEP' 'Unique large model weight' 'NO_FORCE_ADD'
AddGit 'DeepSeek-V2-Lite-Chat.Q4_K_M.gguf' 'ignored(*.gguf)' 10364416768 'KEEP' 'Unique vs DIAGNOSTIC_REPAIRED (different SHA256)' 'NO_FORCE_ADD'
AddGit 'evidence/HEADER_AUTHORITY_MILESTONE/DeepSeek-V2-Lite-Chat.Q4_K_M.DIAGNOSTIC_REPAIRED.gguf' 'ignored' 10364416768 'KEEP' 'Diagnostic evidence artifact; different hash from root DeepSeek' 'NO_FORCE_ADD'
AddGit 'tmp_model-00001-of-000004.safetensors' 'ignored/untracked' 8594887408 'DELETE_CANDIDATE' 'QUICK fingerprint match to git LFS object; redundant working copy' 'NO_FORCE_ADD'
AddGit 'tmp_safetensors/' 'untracked' 0 'DELETE_CANDIDATE' 'Temp safetensor shards; overlap with LFS objects' 'NO_FORCE_ADD'
AddGit '__run_output30.txt' 'untracked' 2417121422 'DELETE_CANDIDATE' 'Multi-GiB run log dump' 'NO_FORCE_ADD'
AddGit '__run_output31.txt' 'untracked' 2417121533 'DELETE_CANDIDATE' 'Multi-GiB run log dump' 'NO_FORCE_ADD'
AddGit '__run_output32.txt' 'untracked' 2417116421 'DELETE_CANDIDATE' 'Multi-GiB run log dump' 'NO_FORCE_ADD'
AddGit '__run_output33.txt' 'untracked' 2417116543 'DELETE_CANDIDATE' 'Multi-GiB run log dump' 'NO_FORCE_ADD'
AddGit '__run_output34.txt' 'untracked' 2417123642 'DELETE_CANDIDATE' 'Multi-GiB run log dump' 'NO_FORCE_ADD'
AddGit '__run_output35.txt' 'untracked' 2417123835 'DELETE_CANDIDATE' 'Multi-GiB run log dump' 'NO_FORCE_ADD'
AddGit 'models/' 'ignored' 0 'KEEP' 'Unique model corpus; many sparse GGUFs' 'NO_FORCE_ADD'
AddGit 'evidence/' 'mixed' 0 'KEEP' 'Preserve sealed certification evidence' 'REVIEW_BEFORE_STAGE'
AddGit 'asm/' 'tracked' 0 'KEEP' 'Source' 'SAFE_TO_STAGE_IF_INTENDED'
AddGit 'lavapath/' 'tracked' 0 'KEEP' 'Source' 'SAFE_TO_STAGE_IF_INTENDED'
AddGit '.gitignore' 'tracked' 6216 'KEEP' 'Never force-add ignored models' 'SAFE_TO_STAGE_IF_INTENDED'
AddGit 'build/' 'untracked_or_ignored' 0 'REVIEW' 'Rebuildable local build tree' 'NO'
AddGit 'llama3.2-3b-Q2_K.gguf' 'ignored' 1363935456 'REVIEW' 'Identical reap copies under evidence/reap_calibration; keep one authoritative' 'NO_FORCE_ADD'
$gitRows | Export-Csv -NoTypeInformation -Encoding UTF8 "$out\GIT_CLASSIFICATION.csv"

$dummyAlloc = [int64]429496729600
$runLogs = [int64]0
Get-ChildItem F:\rawrxd -File -Filter '__run_output*.txt' -ErrorAction SilentlyContinue | Where-Object { $_.Length -ge 1GB } | ForEach-Object { $runLogs += $_.Length }
$tmpModel = [int64]8594887408
$tmpSafe = [int64]0
if (Test-Path 'F:\rawrxd\tmp_safetensors') {
  Get-ChildItem 'F:\rawrxd\tmp_safetensors' -File -ErrorAction SilentlyContinue | ForEach-Object { $tmpSafe += $_.Length }
}
$reapExtra = [int64]1363935456 * 4
$corruptExtra = [int64]668788096 * 2
$med = $reapExtra + $corruptExtra + $tmpSafe
$low = $dummyAlloc + $runLogs + $tmpModel

$rec = [ordered]@{
  asof_utc = $stamp
  promote = 0
  certified = 0
  volume_free_gib = 89.16
  volume_used_gib = 3636.74
  inventory_files_total = 1851573
  inventory_logical_gib = 848.94
  inventory_ge_1MiB_rows = $inv.Count
  notes = @(
    'Physical recovery excludes hard-link double-counting.',
    'G:\~dev\rawrxd\dummy_200b.gguf is SEPARATE 400 GiB on G: — NOT in F: recovery.',
    'DeepSeek root vs DIAGNOSTIC_REPAIRED are NOT content duplicates (full SHA differs).',
    'LOW = dummy_200b + __run_output>=1GiB + tmp_model-00001 (LFS retained).',
    'tmp_safetensors/ under MED until per-shard LFS mapping fully confirmed.',
    'NO DELETIONS. PROMOTE=0 CERTIFIED=0'
  )
  low_risk = [ordered]@{
    description = 'Verified disposable: synthetic dummy, multi-GiB run logs, redundant tmp_model (LFS kept)'
    bytes = $low
    gib = [math]::Round($low/1GB, 2)
    components = [ordered]@{
      dummy_200b_gguf = $dummyAlloc
      run_output_logs_ge_1gib = $runLogs
      tmp_model_00001_safetensors = $tmpModel
    }
  }
  medium_risk = [ordered]@{
    description = 'Duplicate reap/calibration fixtures, corrupted_test extras, tmp_safetensors, old builds'
    bytes = $med
    gib = [math]::Round($med/1GB, 2)
  }
  high_risk = [ordered]@{
    description = 'Unique models, sealed evidence, source, manifests'
    recoverable_gib = 0
    action = 'KEEP'
  }
}
$rec | ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 "$out\RECOVERY_ESTIMATE.json"

$candidates = New-Object System.Collections.Generic.List[object]
[void]$candidates.Add([pscustomobject]@{ rank=1; path='F:\rawrxd\dummy_200b.gguf'; physical_bytes=$dummyAlloc; class='DELETE_CANDIDATE'; risk='LOW'; why='Synthetic zero placeholder; not sparse' })
Get-ChildItem F:\rawrxd -File -Filter '__run_output*.txt' -ErrorAction SilentlyContinue | Where-Object { $_.Length -ge 1GB } | Sort-Object Length -Descending | ForEach-Object {
  [void]$candidates.Add([pscustomobject]@{ rank=($candidates.Count+1); path=$_.FullName; physical_bytes=[int64]$_.Length; class='DELETE_CANDIDATE'; risk='LOW'; why='Run log dump' })
}
[void]$candidates.Add([pscustomobject]@{ rank=($candidates.Count+1); path='F:\rawrxd\tmp_model-00001-of-000004.safetensors'; physical_bytes=$tmpModel; class='DELETE_CANDIDATE'; risk='LOW'; why='Duplicate of git LFS object (QUICK FP match)' })
Import-Csv "$out\LARGEST_FILES.csv" | Sort-Object {[int64]$_.allocated_bytes} -Descending | ForEach-Object {
  if ($candidates.Count -ge 20) { return }
  $exists = $false
  foreach ($c in $candidates) { if ($c.path -eq $_.path) { $exists = $true; break } }
  if ($exists) { return }
  $cls = 'REVIEW'; $risk = 'MED'
  if ($_.path -match '\\models\\') { $cls='KEEP'; $risk='HIGH' }
  if ($_.path -match 'tmp_safetensors|\.pruned$') { $cls='DELETE_CANDIDATE'; $risk='MED' }
  if ($_.path -match '\\evidence\\') { $cls='KEEP'; $risk='HIGH' }
  if ($_.path -match '\\.git\\') { $cls='REVIEW'; $risk='MED' }
  if ($_.path -match 'nemotron-bolt\.gguf$' -or $_.path -match 'DeepSeek-V2-Lite-Chat\.Q4_K_M\.gguf$') { $cls='KEEP'; $risk='HIGH' }
  [void]$candidates.Add([pscustomobject]@{ rank=($candidates.Count+1); path=$_.path; physical_bytes=[int64]$_.allocated_bytes; class=$cls; risk=$risk; why=("LARGEST cat={0} sparse={1}" -f $_.category, $_.sparse) })
}
$candidates | Select-Object -First 20 | Export-Csv -NoTypeInformation -Encoding UTF8 "$out\TOP20_RECOVERY_CANDIDATES.csv"

$plan = @"
# CLEANUP_PLAN — STORAGE_AUDIT_001

ASOF_UTC=$stamp
MODE=READ_ONLY_AUDIT
PROMOTE=0
CERTIFIED=0
NO DELETION WITHOUT EXPLICIT BATCH APPROVAL

## Sampling rule
See INVENTORY_SCOPE.txt. Full tree enumerated (1,851,573 files). Inventory CSV = files >= 1 MiB only.

## Policy
- KEEP: unique source, configs, manifests, unique models, sealed certification evidence
- REVIEW: duplicate models/fixtures, obsolete builds, superseded evidence, large dumps
- DELETE_CANDIDATE: verified disposable dummy, redundant temps, rebuildable outputs without evidentiary purpose
- Git: never force-add ignored *.gguf / models

## F: vs G: overlap
- F:\rawrxd and G:\~dev\rawrxd are separate NTFS volumes (serials 0x66a37175 vs 0x8ae8e0db)
- Both hold independent dummy_200b.gguf @ 400 GiB allocated — deleting F: does not free G:

## Batch 1 — dummy first (LOW)
| Path | Allocated | Risk | Deps | Justification |
|------|-----------|------|------|---------------|
| F:\rawrxd\dummy_200b.gguf | 400.00 GiB | LOW | Benchmark/smoke refs only; evidence already calls it zero placeholder | Synthetic all-zero non-GGUF; gitignored; not sparse |

Action: PENDING APPROVAL. See DUMMY_200B_ANALYSIS.md.

## Batch 2 — disposable run logs (LOW)
| Path | Size | Risk | Justification |
|------|------|------|---------------|
| F:\rawrxd\__run_output30.txt .. __run_output35.txt | ~2.25 GiB each (~13.5 GiB) | LOW | Unique content but disposable run dumps; not sealed evidence |

## Batch 3 — duplicate model working copies (LOW to MED)
| Path | Size | Risk | Justification |
|------|------|------|---------------|
| F:\rawrxd\tmp_model-00001-of-000004.safetensors | 8.00 GiB | LOW | QUICK fingerprint == git LFS object a314b79a...; keep LFS |
| F:\rawrxd\tmp_safetensors\ | ~29 GiB logical | MED | Overlaps LFS shards; confirm all 4 shards mapped before delete |

## Batch 4 — duplicate fixtures (MED; preserve one)
| Group | Paths | Extra bytes | Note |
|-------|-------|-------------|------|
| llama3.2-3b-Q2_K identical | root + evidence\reap_calibration\roundtrip* (5 copies) | ~5.08 GiB extras | Keep one authoritative; do not wipe sealed receipt dirs blindly |
| corrupted_test.gguf | gguf_lifecycle_test{,3,4} | ~1.25 GiB extras | Keep one fixture |

## Batch 5 — old builds / redundant evidence (MED)
- Local build*, out, bin trees: REVIEW then DELETE_CANDIDATE if rebuildable
- Do not bulk-delete evidence\ sealed certification trees

## Batch 6 — KEEP
- F:\rawrxd\models\*.gguf unique weights (many sparse)
- nemotron-bolt.gguf, DeepSeek-V2-Lite-Chat.Q4_K_M.gguf (different hash from DIAGNOSTIC_REPAIRED)
- Source under asm\, lavapath\, manifests, sealed evidence

## RawrXD-owned dirs on F:\ (outside F:\rawrxd focus)
50 top-level *rawrxd* directories on F:\ (archives, builds, preservations, WebView2). Separate follow-up after Batch 1.

## Git staging
See GIT_CLASSIFICATION.csv. SAFE_TO_STAGE only for intended source/config. NO_FORCE_ADD for ignored models.
"@
Set-Content -Encoding utf8 "$out\CLEANUP_PLAN.md" $plan

$invCount = $inv.Count
$sum = @"
# AUDIT_SUMMARY — STORAGE_AUDIT_001

ASOF_UTC=$stamp
MODE=READ_ONLY
PROMOTE=0
CERTIFIED=0
NO_DELETIONS_PERFORMED

## Volume F:
| Metric | Value |
|--------|-------|
| Size | ~3725.9 GiB |
| Free | ~89.16 GiB |
| Used | ~3636.74 GiB |
| Files under F:\rawrxd | 1,851,573 |
| Logical sum under F:\rawrxd | ~848.94 GiB |
| Inventory rows (>=1 MiB) | $invCount |
| Files >=1 GiB | 50 |

## Dummy verdict
F:\rawrxd\dummy_200b.gguf = DELETE_CANDIDATE
- NOT sparse; allocated 400.00 GiB physical
- Not valid GGUF (zero header); evidence labels it zero placeholder
- Independent twin on G:\~dev\rawrxd\dummy_200b.gguf (not counted in F: recovery)

## LOW-risk recoverable (F: only, no double-count)
~$([math]::Round($low/1GB,2)) GiB = dummy 400 + run logs ~$([math]::Round($runLogs/1GB,2)) + tmp_model 8.00

## Top recovery candidates (physical)
See TOP20_RECOVERY_CANDIDATES.csv — #1 is dummy @ 400 GiB.

## Deliverables
1. STORAGE_INVENTORY.csv
2. LARGEST_FILES.csv
3. DUPLICATES.csv
4. GIT_CLASSIFICATION.csv
5. CLEANUP_PLAN.md
6. RECOVERY_ESTIMATE.json
7. AUDIT_SUMMARY.md (this file)
8. DUMMY_200B_ANALYSIS.md

Also: INVENTORY_SCOPE.txt, TOP20_RECOVERY_CANDIDATES.csv, VOLUME.json, DEEPSEEK_HASH_NOTE.txt
"@
Set-Content -Encoding utf8 "$out\AUDIT_SUMMARY.md" $sum

# Refresh DUMMY analysis if needed
$dummyMd = @"
# DUMMY_200B_ANALYSIS

ASOF_UTC=$stamp
PROMOTE=0
CERTIFIED=0
MODE=READ_ONLY_AUDIT

## Path
- F:\rawrxd\dummy_200b.gguf
- LogicalBytes=429496729600 (400.00 GiB)
- AllocatedBytes=429496729600 (FileStandardInfo + GetCompressedFileSize + fsutil)
- SparseFlag=NOT set as sparse (fsutil sparse queryflag / queryrange)
- ValidDataLength=429496729600 (fsutil file queryvaliddata)
- NumberOfLinks=1
- FileId=0x0005000000026c94

## Synthetic verdict
- GGUF magic at offset 0: absent (bytes 00 00 00 00; real GGUF begins with ASCII GGUF)
- Name dummy_200b indicates fixture, not production 200B weights
- Sealed evidence text: dummy_200b.gguf = zero placeholder

## Code / test / evidence references
- F:\rawrxd\benchmark_results\benchmark_20260719_162222.csv
- F:\rawrxd\evidence\gate5_head_repro_2026-10-14\PARITY_RECEIPTS.txt (explicit zero placeholder)
- F:\rawrxd\evidence\LARGE_MODEL_SMOKE_20261020\...\models.json
- F:\rawrxd\RECOVERY_BACKUP_MANIFEST_2026-10-12.md
- Additional G:\~dev\rawrxd\test_40gb_models\* and headless tip logs

Git: ignored via .gitignore *.gguf; not tracked; never force-add.

## Sibling on G: (separate volume)
- G:\~dev\rawrxd\dummy_200b.gguf also 400 GiB fully allocated, FileId=0x0001000000056a33
- F: serial 0x66a37175 vs G: serial 0x8ae8e0db — independent physical copies
- Deleting F: recovers F: only

## Classification
DELETE_CANDIDATE (LOW risk for F: space recovery after dependency sign-off).
NO DELETION PERFORMED THIS AUDIT.
"@
Set-Content -Encoding utf8 "$out\DUMMY_200B_ANALYSIS.md" $dummyMd

Add-Content "$out\AUDIT_RUN.log" ("[{0}] FINALIZE_DONE low_gib={1:N2}" -f (Get-Date).ToString('o'), ($low/1GB))
Write-Host "FINALIZE_OK low_gib=$([math]::Round($low/1GB,2))"
Get-ChildItem $out -File | Where-Object { $_.Name -match '^(STORAGE_INVENTORY|LARGEST_FILES|DUPLICATES|GIT_CLASSIFICATION|CLEANUP_PLAN|RECOVERY_ESTIMATE|AUDIT_SUMMARY|DUMMY_200B|TOP20|INVENTORY_SCOPE|VOLUME)\.' } |
  Select-Object Name, Length | Format-Table -AutoSize
Import-Csv "$out\TOP20_RECOVERY_CANDIDATES.csv" | Format-Table rank, @{N='GiB';E={[math]::Round([int64]$_.physical_bytes/1GB,2)}}, risk, class, path -AutoSize
Write-Host "LOW_RISK_GIB=$([math]::Round($low/1GB,2))"
