<#
Fully customizable PoC compressor.
Concepts:
- Each file compressed independently; no cross-file state.
- Ratio can be fractional: e.g. 6666.1. Integer part determines CHUNK_SIZE.
  Decimal part (e.g. .1) acts as a sentinel flag stored in header to signal isolation or special handling.
- Header placed at start of compressed file so decompressor can reconstruct parameters.
  Layout (little-endian where numeric):
    [0..7]  ASCII Magic: RAWRCUST
    [8]     Version (byte) = 1
    [9]     FoldMethod (byte): 0=Xor,1=SumMod256,2=FirstByte,3=Hash8
    [10]    SentinelFlag (byte): 1 if fractional part present else 0
    [11..14] ChunkSize (Int32)
    [15..18] ExpansionFactor (Int32) (used only for decompress if method requires expansion)
    [19..22] OriginalFileSize (Int32 low 32 bits) (best-effort, large files will overflow; PoC only)
- Fold methods (lossy):
    Xor: XOR of chunk bytes
    SumMod256: Sum all bytes modulo 256
    FirstByte: Just take first byte (fast, worst quality)
    Hash8: Simple rolling hash (byte)
- Decompression: expand each stored byte into ExpansionFactor bytes (defaults to ChunkSize if not provided).
  NOTE: Not reversible. For demonstration only.
#>

param(
  [Parameter(Mandatory)]
  [ValidateSet('compress','decompress')]
  [string]$Action,

  [Parameter(Mandatory)]
  [ValidateScript({Test-Path $_ -PathType Container})]
  [string]$Folder,

  [double]$Ratio = 6666.1,
  [string]$FoldMethod = 'Xor',
  [int]$ChunkSize,
  [int]$ExpansionFactor,
  [switch]$VerboseStats
)

# Derive chunk size if not explicitly provided
if(-not $ChunkSize){
  $ChunkSize = [int][Math]::Floor($Ratio) # ignore fractional part for byte sizing
}

# Determine sentinel flag from fractional part
$fractional = $Ratio - [Math]::Floor($Ratio)
$SentinelFlag = if($fractional -gt 0){1}else{0}

if(-not $ExpansionFactor){
  # choose a modest expansion vs original chunk; user can override
  $ExpansionFactor = if($ChunkSize -gt 2048){[int]($ChunkSize/4)} else {$ChunkSize}
}

# Map fold method to id
$foldId = switch($FoldMethod.ToLower()){
  'xor' {0}
  'summod256' {1}
  'firstbyte' {2}
  'hash8' {3}
  default { throw "Unknown FoldMethod '$FoldMethod'" }
}

Add-Type -TypeDefinition @"
using System;using System.IO;using System.Security.Cryptography;
public static class FlexibleFold {
  public static byte Xor(byte[] buf,int len){byte acc=0;for(int i=0;i<len;i++)acc^=buf[i];return acc;}
  public static byte SumMod(byte[] buf,int len){int s=0;for(int i=0;i<len;i++)s+=buf[i];return (byte)(s & 0xFF);}    
  public static byte First(byte[] buf,int len){return buf[0];}
  public static byte Hash8(byte[] buf,int len){unchecked{byte h=0;for(int i=0;i<len;i++)h=(byte)(h*131 ^ buf[i]);return h;}}
  public static byte Apply(byte[] buf,int len,int method){switch(method){case 0:return Xor(buf,len);case 1:return SumMod(buf,len);case 2:return First(buf,len);case 3:return Hash8(buf,len);default:return 0;} }
}
"@

function New-HeaderBytes {
  param($FoldId,$SentinelFlag,$ChunkSize,$ExpansionFactor,$OrigSize)
  $header = New-Object byte[] 23
  $magic = [Text.Encoding]::ASCII.GetBytes('RAWRCUST')
  $magic.CopyTo($header,0)
  $header[8] = 1    # version
  $header[9] = [byte]$FoldId
  $header[10] = [byte]$SentinelFlag
  [BitConverter]::GetBytes([int]$ChunkSize).CopyTo($header,11)
  [BitConverter]::GetBytes([int]$ExpansionFactor).CopyTo($header,15)
  [BitConverter]::GetBytes([int]$OrigSize).CopyTo($header,19)
  return $header
}

function Read-Header {
  param([System.IO.Stream]$Stream)
  $header = New-Object byte[] 23
  $read = $Stream.Read($header,0,23)
  if($read -ne 23){ throw 'Invalid or missing header.' }
  $magic = [Text.Encoding]::ASCII.GetString($header,0,8)
  if($magic -ne 'RAWRCUST'){ throw 'Bad magic in header.' }
  $version = $header[8]
  $foldId = $header[9]
  $sentFlag = $header[10]
  $chunk = [BitConverter]::ToInt32($header,11)
  $expand = [BitConverter]::ToInt32($header,15)
  $origLow = [BitConverter]::ToInt32($header,19)
  [pscustomobject]@{Version=$version;FoldId=$foldId;Sentinel=$sentFlag;ChunkSize=$chunk;Expansion=$expand;OrigLow32=$origLow}
}

function Compress-Custom {
  param($File)
  $out = $File.FullName + '.cust'
  $in  = [System.IO.File]::Open($File.FullName,'Open','Read','Read')
  $outStream = [System.IO.File]::Create($out)
  $header = New-HeaderBytes -FoldId $foldId -SentinelFlag $SentinelFlag -ChunkSize $ChunkSize -ExpansionFactor $ExpansionFactor -OrigSize $File.Length
  $outStream.Write($header,0,$header.Length)
  $buf = New-Object byte[] $ChunkSize
  $totalRead = 0
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  while(($read = $in.Read($buf,0,$ChunkSize)) -gt 0){
    $totalRead += $read
    $b = [FlexibleFold]::Apply($buf,$read,$foldId)
    $outStream.WriteByte($b)
  }
  $sw.Stop()
  $in.Close(); $outStream.Close()
  (Get-Item $out).LastWriteTime = $File.LastWriteTime
  $orig = $File.Length; $comp = (Get-Item $out).Length
  $ratio = $comp / $orig
  Write-Host "Compressed $($File.Name) -> $(Split-Path -Leaf $out) | ratio $(('{0:P6}' -f $ratio)) | time $([int]$sw.Elapsed.TotalSeconds)s" -ForegroundColor Cyan
  if($VerboseStats){ Write-Host "Original bytes: $orig  Compressed bytes: $comp  Chunks processed: $([Math]::Ceiling($orig / $ChunkSize))" -ForegroundColor DarkCyan }
}

function Decompress-Custom {
  param($File)
  $in = [System.IO.File]::Open($File.FullName,'Open','Read','Read')
  $meta = Read-Header -Stream $in
  $base = $File.FullName -replace '\.cust$',''
  $out = [System.IO.File]::Create($base)
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  while($in.Position -lt $in.Length){
    $b = $in.ReadByte(); if($b -lt 0){break}
    $chunk = New-Object byte[] $meta.Expansion
    for($i=0;$i -lt $chunk.Length;$i++){ $chunk[$i] = [byte]$b }
    $out.Write($chunk,0,$chunk.Length)
  }
  $sw.Stop()
  $in.Close(); $out.Close()
  (Get-Item $base).LastWriteTime = $File.LastWriteTime
  $dec = (Get-Item $base).Length
  Write-Host "Decompressed $(Split-Path -Leaf $File.FullName) -> $(Split-Path -Leaf $base) | expanded bytes $dec | time $([int]$sw.Elapsed.TotalSeconds)s" -ForegroundColor Magenta
  if($VerboseStats){ Write-Host "Header: FoldId=$($meta.FoldId) ChunkSize=$($meta.ChunkSize) Expansion=$($meta.Expansion) Sentinel=$($meta.Sentinel) OrigLow32=$($meta.OrigLow32)" -ForegroundColor DarkMagenta }
}

# ---- main ----
Get-ChildItem $Folder -File | Where-Object Length -GE 2GB | ForEach-Object {
  if($Action -eq 'compress') { Compress-Custom $_ }
  else { Decompress-Custom $_ }
}
Write-Host 'Done.'