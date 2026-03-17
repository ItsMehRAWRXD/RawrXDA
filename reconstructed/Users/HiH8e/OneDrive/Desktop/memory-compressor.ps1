<#
In-memory ("fileless / storageless") demo compressor.
Goal: ~6× compression then ~12× expansion (relative to compressed) without writing intermediate files.
Implementation notes:
- Compress groups every 6 source bytes -> 1 byte (ratio ~1:6). We use XOR fold; padding with 0 for incomplete tail.
- Decompress expands each compressed byte -> 12 bytes (repeat pattern). Result size ≈ 12 * compressedLen = ~2 * originalLen.
- Not reversible. Pure PoC for mechanics.
- No temp files: functions return byte arrays & stats; optional final save if user requests.
#>

param(
  [ValidateSet('compress','decompress','demo')]
  [string]$Action = 'demo',
  [string]$FilePath,
  [int]$GroupSize = 6,
  [switch]$NegativeMode,
  [switch]$SaveCompressed,
  [switch]$SaveExpanded,
  [switch]$Stats
)

function Compress-InMemory {
  param([byte[]]$Input,[int]$GroupSize)
  $inLen = $Input.Length
  if($GroupSize -le 0){
    # Degenerate: no grouping possible; treat as theoretical infinite compression (store zero bytes)
    return [pscustomobject]@{ OriginalBytes=$inLen; CompressedBytes=0; Ratio=0; DisplayRatio='Infinity (GroupSize=0)'; Data=([byte[]]@()) }
  }
  $outLen = [int][Math]::Ceiling($inLen / $GroupSize)
  $out = New-Object byte[] $outLen
  $j = 0
  for($i=0; $i -lt $inLen; $i += $GroupSize){
    $xor = 0
    for($k=0; $k -lt $GroupSize; $k++){
      $idx = $i + $k
      if($idx -lt $inLen){ $xor = $xor -bxor $Input[$idx] }
    }
    $out[$j++] = [byte]$xor
  }
  [pscustomobject]@{ OriginalBytes=$inLen; CompressedBytes=$outLen; Ratio=($outLen / $inLen); DisplayRatio="1:${GroupSize}"; Data=$out }
}

function Decompress-InMemory {
  param([byte[]]$Compressed)
  # Detect negative header mode: first 8 bytes Int64 < 0
  if($Compressed.Length -ge 8){
    $possibleNeg = [BitConverter]::ToInt64($Compressed,0)
    if($possibleNeg -lt 0){
      $origSize = -$possibleNeg
      $out = New-Object byte[] $origSize
      # Fill with 0 pattern to represent reconstructed size placeholder
      # (PoC: original data unrecoverable in negative mode)
      [Array]::Clear($out,0,$out.Length)
      return [pscustomobject]@{ CompressedBytes=$Compressed.Length; ExpandedBytes=$out.Length; ExpansionFactor=($out.Length / $Compressed.Length); Data=$out; NegativeHeader=$true; OriginalSize=$origSize }
    }
  }
  $expandSizePerByte = 12
  $compLen = $Compressed.Length
  $outLen = $compLen * $expandSizePerByte
  $out = New-Object byte[] $outLen
  $j = 0
  foreach($b in $Compressed){
    for($r=0; $r -lt $expandSizePerByte; $r++){ $out[$j++] = $b }
  }
  [pscustomobject]@{ CompressedBytes=$compLen; ExpandedBytes=$outLen; ExpansionFactor=($outLen / $compLen); Data=$out }
}

function Load-FileBytes {
  param($Path)
  if(-not (Test-Path $Path -PathType Leaf)){ throw "File not found: $Path" }
  return [System.IO.File]::ReadAllBytes($Path)
}

if($Action -eq 'compress'){
  if(-not $FilePath){ throw 'Provide -FilePath' }
  $bytes = Load-FileBytes $FilePath
  if($NegativeMode){
    # Header only: Int64 negative original size, followed by normal compressed bytes (optional)
    $core = Compress-InMemory -Input $bytes -GroupSize $GroupSize
    # Build header: 8 bytes Int64 negative original size
    $negHeader = [BitConverter]::GetBytes([int64](-$core.OriginalBytes))
    # For pure negative mode we can choose to omit core.Data and only keep header
    $payload = $negHeader + $core.Data  # keep folded bytes for visibility
    $result = [pscustomobject]@{ OriginalBytes=$core.OriginalBytes; CompressedBytes=$payload.Length; Ratio=($payload.Length / $core.OriginalBytes); DisplayRatio="NegativeHeader"; Data=$payload; HeaderOnly=$false }
    if($SaveCompressed){ [IO.File]::WriteAllBytes("$FilePath.negcomp", $payload) }
  }
  else {
    $result = Compress-InMemory -Input $bytes -GroupSize $GroupSize
    if($SaveCompressed){ [IO.File]::WriteAllBytes("$FilePath.comp", $result.Data) }
  }
  if($Stats){ $result }
}
elseif($Action -eq 'decompress'){
  if(-not $FilePath){ throw 'Provide -FilePath to read compressed source (raw bytes)' }
  $bytes = Load-FileBytes $FilePath
  $expanded = Decompress-InMemory $bytes
  if($SaveExpanded){
    $outName = "$FilePath.expanded"
    [IO.File]::WriteAllBytes($outName, $expanded.Data)
  }
  if($Stats){ $expanded }
}
else { # demo
  if(-not $FilePath){ throw 'Provide -FilePath for demo action' }
  $orig = (Load-FileBytes -Path $FilePath)
  if($NegativeMode){
    $cCore = Compress-InMemory -Input $orig -GroupSize $GroupSize
    $negHeader = [BitConverter]::GetBytes([int64](-$cCore.OriginalBytes))
    $c = [pscustomobject]@{ OriginalBytes=$cCore.OriginalBytes; CompressedBytes=($negHeader.Length + $cCore.CompressedBytes); Ratio=(($negHeader.Length + $cCore.CompressedBytes)/$cCore.OriginalBytes); DisplayRatio='NegativeHeader'; Data=($negHeader + $cCore.Data) }
  } else {
    $c = Compress-InMemory -Input $orig -GroupSize $GroupSize
  }
  $d = if($c.CompressedBytes -gt 0){ Decompress-InMemory -Compressed $c.Data } else { [pscustomobject]@{ CompressedBytes=0; ExpandedBytes=0; ExpansionFactor=0; Data=@(); NegativeHeader=$false; OriginalSize=0 } }
  $report = [pscustomobject]@{
    File = (Split-Path -Leaf $FilePath)
    OriginalBytes = $c.OriginalBytes
    CompressedBytes = $c.CompressedBytes
    CompressRatio = $c.Ratio
  ExpandedBytes = $d.ExpandedBytes
  ExpansionFactor = $d.ExpansionFactor
  ExpandedVsOriginal = if($c.OriginalBytes -gt 0 -and $d.ExpandedBytes -gt 0){ ($d.ExpandedBytes / $c.OriginalBytes) } else { 0 }
  DisplayRatio = $c.DisplayRatio
    NegativeHeaderDetected = $d.NegativeHeader
    NegativeOriginalSize = if($d.NegativeHeader){ $d.OriginalSize } else { $null }
  }
  $report | Format-List
  if($SaveExpanded){ [IO.File]::WriteAllBytes("$FilePath.expanded", $d.Data) }
}
