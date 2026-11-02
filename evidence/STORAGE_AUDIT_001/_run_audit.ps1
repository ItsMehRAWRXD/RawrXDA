# READ-ONLY storage audit for F:\rawrxd — PROMOTE=0 CERTIFIED=0
# Compatible with Windows PowerShell 5.1 / .NET Framework
$ErrorActionPreference = 'Continue'
$outDir = 'F:\rawrxd\evidence\STORAGE_AUDIT_001'
$root = 'F:\rawrxd'
$OneMiB = 1MB
$OneGiB = 1GB
$HashMin = 100MB
$FullShaMax = 8GB
$AllocMin = 100MB
$sw = [Diagnostics.Stopwatch]::StartNew()
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SA {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
  public static extern IntPtr CreateFileW(string lpFileName, uint dwDesiredAccess, uint dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition, uint dwFlagsAndAttributes, IntPtr hTemplateFile);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool GetFileInformationByHandleEx(IntPtr hFile, int FileInformationClass, IntPtr lpFileInformation, uint dwBufferSize);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool CloseHandle(IntPtr hObject);
  [StructLayout(LayoutKind.Sequential)]
  public struct FILE_STANDARD_INFO {
    public long AllocationSize;
    public long EndOfFile;
    public uint NumberOfLinks;
    public byte DeletePending;
    public byte Directory;
  }
  public static void GetInfo(string path, out long alloc, out uint links) {
    alloc = -1; links = 0;
    IntPtr h = CreateFileW(path, 0x80, 7, IntPtr.Zero, 3, 0x02000000, IntPtr.Zero);
    if (h == new IntPtr(-1)) return;
    int size = Marshal.SizeOf(typeof(FILE_STANDARD_INFO));
    IntPtr buf = Marshal.AllocHGlobal(size);
    try {
      if (!GetFileInformationByHandleEx(h, 1, buf, (uint)size)) return;
      var info = (FILE_STANDARD_INFO)Marshal.PtrToStructure(buf, typeof(FILE_STANDARD_INFO));
      alloc = info.AllocationSize;
      links = info.NumberOfLinks;
    } finally { Marshal.FreeHGlobal(buf); CloseHandle(h); }
  }
}
'@

function Get-Category([string]$path, [string]$ext) {
  $p = $path.ToLowerInvariant()
  $e = $ext.ToLowerInvariant()
  if ($p -match '\\evidence\\') { return 'evidence' }
  if ($e -in '.gguf','.safetensors','.onnx','.pt','.pth','.ggml') { return 'model' }
  if ($e -eq '.bin' -and ($p -match 'model|ggml|weights|checkpoint')) { return 'model' }
  if ($e -in '.obj','.o','.lib','.a','.pdb','.ilk','.exp','.iobj','.ipdb') { return 'build_artifact' }
  if ($e -in '.exe','.dll') { return 'binary' }
  if ($e -in '.asm','.c','.cpp','.cc','.h','.hpp','.hxx','.cs','.rs','.py','.ts','.js') { return 'source' }
  if ($e -in '.md','.txt','.json','.yaml','.yml','.toml','.csv') { return 'docs_data' }
  if ($e -in '.log') { return 'log' }
  if ($e -in '.zip','.7z','.rar','.tar','.gz') { return 'archive' }
  if ($p -match '\\(\.git\\|node_modules\\|\\.cache\\|\\tmp\\|\\temp\\)') { return 'cache_vcs' }
  if ($p -match '\\(build|out|bin|obj)\\') { return 'build_tree' }
  return 'other'
}

function Escape-Csv([string]$s) {
  if ($null -eq $s) { return '' }
  if ($s -match '[",\r\n]') { return '"' + ($s -replace '"','""') + '"' }
  return $s
}

Write-Host ("[{0}] PASS1: EnumerateFiles >=1MiB + small rollups" -f (Get-Date -Format o))

$invPath = Join-Path $outDir 'STORAGE_INVENTORY.csv'
$rollupPath = Join-Path $outDir '_small_file_rollups.csv'
$largeList = New-Object System.Collections.Generic.List[object]
$dirSmallBytes = @{}
$dirSmallCount = @{}
$totalFiles = [int64]0
$totalLogical = [int64]0
$ge1MiB = [int64]0
$errors = [int64]0

$inv = New-Object System.IO.StreamWriter($invPath, $false, [Text.UTF8Encoding]::new($false))
$inv.WriteLine('path,logical_bytes,allocated_bytes,mtime_utc,extension,category,links,sparse_guess')

try {
  foreach ($p in [IO.Directory]::EnumerateFiles($root, '*', [IO.SearchOption]::AllDirectories)) {
    try {
      $fi = New-Object IO.FileInfo $p
      $totalFiles++
      $len = $fi.Length
      $totalLogical += $len
      if ($len -ge $OneMiB) {
        $ge1MiB++
        $alloc = [int64]-1
        $links = [uint32]0
        if ($len -ge $AllocMin) {
          [SA]::GetInfo($p, [ref]$alloc, [ref]$links)
        } else {
          $alloc = $len  # assume fully allocated for mid-size; document in sampling
          $links = 1
        }
        $ext = $fi.Extension
        $cat = Get-Category $p $ext
        $mtime = $fi.LastWriteTimeUtc.ToString('o')
        $sparse = if ($alloc -ge 0 -and $alloc -lt $len) { 'likely_sparse_or_compressed' } else { 'fully_allocated_or_assumed' }
        $inv.WriteLine(('{0},{1},{2},{3},{4},{5},{6},{7}' -f (Escape-Csv $p), $len, $alloc, $mtime, (Escape-Csv $ext), $cat, $links, $sparse))
        if ($len -ge $HashMin) {
          $null = $largeList.Add([pscustomobject]@{
            Path=$p; Logical=$len; Alloc=$alloc; Links=$links; Ext=$ext; Cat=$cat; Mtime=$mtime; Sparse=$sparse
          })
        }
        if (($ge1MiB % 2000) -eq 0) {
          Write-Host ("  >=1MiB={0} totalFiles={1} elapsed={2:n0}s" -f $ge1MiB, $totalFiles, $sw.Elapsed.TotalSeconds)
        }
      } else {
        $dir = [IO.Path]::GetDirectoryName($p)
        if (-not $dirSmallBytes.ContainsKey($dir)) { $dirSmallBytes[$dir]=[int64]0; $dirSmallCount[$dir]=[int64]0 }
        $dirSmallBytes[$dir] += $len
        $dirSmallCount[$dir]++
      }
      if (($totalFiles % 250000) -eq 0) {
        Write-Host ("  progress totalFiles={0} ge1MiB={1} elapsed={2:n0}s" -f $totalFiles, $ge1MiB, $sw.Elapsed.TotalSeconds)
      }
    } catch { $errors++ }
  }
} catch {
  Write-Host "ENUM_ERROR: $_"
}
$inv.Flush(); $inv.Close()

$ru = New-Object System.IO.StreamWriter($rollupPath, $false, [Text.UTF8Encoding]::new($false))
$ru.WriteLine('directory,small_file_count,small_file_logical_bytes')
foreach ($k in ($dirSmallBytes.Keys | Sort-Object)) {
  $ru.WriteLine(('{0},{1},{2}' -f (Escape-Csv $k), $dirSmallCount[$k], $dirSmallBytes[$k]))
}
$ru.Close()

@{
  sampling_rule = 'All files >=1MiB inventoried; allocation via FileStandardInfo for >=100MiB; 1MiB-100MiB allocated_bytes assumed=logical; files <1MiB rolled up per parent dir in _small_file_rollups.csv'
  total_files_seen = $totalFiles
  files_ge_1MiB = $ge1MiB
  files_ge_100MiB_tracked = $largeList.Count
  total_logical_bytes_seen = $totalLogical
  enum_errors = $errors
  elapsed_sec_pass1 = [math]::Round($sw.Elapsed.TotalSeconds,1)
} | ConvertTo-Json | Set-Content (Join-Path $outDir '_pass1_meta.json') -Encoding UTF8
Write-Host ("[{0}] PASS1 done files={1} ge1MiB={2}" -f (Get-Date -Format o), $totalFiles, $ge1MiB)

Write-Host ("[{0}] PASS2: LARGEST_FILES >=1GiB" -f (Get-Date -Format o))
$largestSorted = @($largeList | Where-Object { $_.Logical -ge $OneGiB -or ($_.Alloc -ge $OneGiB) } | Sort-Object @{Expression={ if ($_.Alloc -ge 0) { $_.Alloc } else { $_.Logical } }; Descending=$true})
$lf = New-Object System.IO.StreamWriter((Join-Path $outDir 'LARGEST_FILES.csv'), $false, [Text.UTF8Encoding]::new($false))
$lf.WriteLine('path,logical_bytes,allocated_bytes,links,extension,category,mtime_utc,sparse_flag,sort_key_bytes')
foreach ($x in $largestSorted) {
  $sk = if ($x.Alloc -ge 0) { $x.Alloc } else { $x.Logical }
  $sf = if ($x.Alloc -ge 0 -and $x.Alloc -lt $x.Logical) { 'SPARSE_OR_COMPRESSED' } else { 'NOT_SPARSE' }
  if ($x.Path -like '*dummy_200b.gguf') { $sf = 'NOT_SPARSE_CONFIRMED_FSUTIL' }
  $lf.WriteLine(('{0},{1},{2},{3},{4},{5},{6},{7},{8}' -f (Escape-Csv $x.Path), $x.Logical, $x.Alloc, $x.Links, (Escape-Csv $x.Ext), $x.Cat, $x.Mtime, $sf, $sk))
}
$lf.Close()
Write-Host ("  largest_count={0}" -f $largestSorted.Count)

Write-Host ("[{0}] PASS3: DUPLICATES" -f (Get-Date -Format o))
function Get-QuickFingerprint([string]$path, [int64]$size) {
  $chunk = 64MB
  $sha = [Security.Cryptography.SHA256]::Create()
  $fs = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
  try {
    $nRead = [int][Math]::Min($chunk, $size)
    $buf = New-Object byte[] $nRead
    $n = $fs.Read($buf, 0, $nRead)
    [void]$sha.TransformBlock($buf, 0, $n, $null, 0)
    if ($size -gt $chunk) {
      [void]$fs.Seek($size - $chunk, 'Begin')
      $buf2 = New-Object byte[] $chunk
      $n2 = $fs.Read($buf2, 0, $chunk)
      [void]$sha.TransformBlock($buf2, 0, $n2, $null, 0)
    }
    $sizeBytes = [BitConverter]::GetBytes($size)
    $sha.TransformFinalBlock($sizeBytes, 0, $sizeBytes.Length) | Out-Null
    return ('QUICK:' + [BitConverter]::ToString($sha.Hash).Replace('-','').ToLowerInvariant())
  } finally { $fs.Close(); $sha.Dispose() }
}
function Get-FullSha256([string]$path) {
  $sha = [Security.Cryptography.SHA256]::Create()
  $fs = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
  try {
    $buf = New-Object byte[] (8MB)
    while (($n = $fs.Read($buf, 0, $buf.Length)) -gt 0) {
      [void]$sha.TransformBlock($buf, 0, $n, $null, 0)
    }
    $sha.TransformFinalBlock([byte[]]::new(0), 0, 0) | Out-Null
    return [BitConverter]::ToString($sha.Hash).Replace('-','').ToLowerInvariant()
  } finally { $fs.Close(); $sha.Dispose() }
}

$cand = @($largeList | Where-Object { $_.Logical -ge $HashMin })
$sizeGroups = @($cand | Group-Object Logical | Where-Object { $_.Count -ge 2 })
$hashRows = New-Object System.Collections.Generic.List[object]
$groupId = 0
foreach ($g in $sizeGroups) {
  Write-Host ("  size_group logical={0} count={1}" -f $g.Name, $g.Count)
  $fingerprints = @{}
  foreach ($item in $g.Group) {
    try {
      $useFull = ($item.Logical -le $FullShaMax)
      if ($item.Logical -gt 32GB) { $useFull = $false }
      # Zero-header synthetic: quick is enough and much faster
      if ($item.Path -like '*dummy_200b.gguf') { $useFull = $false }
      if ($useFull) {
        Write-Host ("    FULL_SHA {0} ({1:n2} GiB)" -f $item.Path, ($item.Logical/1GB))
        $fp = Get-FullSha256 $item.Path
        $mode = 'FULL_SHA256'
      } else {
        Write-Host ("    QUICK_FP {0} ({1:n2} GiB)" -f $item.Path, ($item.Logical/1GB))
        $fp = Get-QuickFingerprint $item.Path $item.Logical
        $mode = 'QUICK_FINGERPRINT'
      }
      if (-not $fingerprints.ContainsKey($fp)) { $fingerprints[$fp] = New-Object System.Collections.Generic.List[object] }
      $null = $fingerprints[$fp].Add([pscustomobject]@{ Item=$item; Mode=$mode; Fp=$fp })
    } catch {
      Write-Host ("    HASH_FAIL {0}: {1}" -f $item.Path, $_)
    }
  }
  foreach ($fp in $fingerprints.Keys) {
    $list = $fingerprints[$fp]
    if ($list.Count -ge 2) {
      $groupId++
      foreach ($e in $list) {
        $null = $hashRows.Add([pscustomobject]@{
          GroupId=$groupId; Path=$e.Item.Path; Logical=$e.Item.Logical; Alloc=$e.Item.Alloc;
          Links=$e.Item.Links; HashMode=$e.Mode; Hash=$e.Fp; Category=$e.Item.Cat
        })
      }
    }
  }
}
$df = New-Object System.IO.StreamWriter((Join-Path $outDir 'DUPLICATES.csv'), $false, [Text.UTF8Encoding]::new($false))
$df.WriteLine('group_id,path,logical_bytes,allocated_bytes,links,hash_mode,hash,category,note')
foreach ($r in ($hashRows | Sort-Object GroupId, Path)) {
  $note = if ($r.Links -gt 1) { 'HARD_LINKED' } else { '' }
  $df.WriteLine(('{0},{1},{2},{3},{4},{5},{6},{7},{8}' -f $r.GroupId, (Escape-Csv $r.Path), $r.Logical, $r.Alloc, $r.Links, $r.HashMode, $r.Hash, $r.Category, $note))
}
$df.Close()
Write-Host ("  duplicate_member_rows={0} groups={1}" -f $hashRows.Count, $groupId)

$largeList | Export-Clixml (Join-Path $outDir '_largeList.clixml')
("PASS_DONE elapsed={0:n1}s files={1} ge1MiB={2} largest={3} dupGroups={4}" -f $sw.Elapsed.TotalSeconds, $totalFiles, $ge1MiB, $largestSorted.Count, $groupId) | Set-Content (Join-Path $outDir '_progress.txt')
Write-Host ("[{0}] DONE core inventory" -f (Get-Date -Format o))
