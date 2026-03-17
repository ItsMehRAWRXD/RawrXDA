# 6666.1× fold / 1.6666× unfold compressor (PoC – highly lossy)
<##
Interpretation of request:
- "use 6666.1 instead of 652" → target an approximate 6666:1 input:output ratio.
  Since we must use whole bytes, we take CHUNK_SIZE = 6666 bytes and XOR-fold them to 1 byte.
  (Fractional 0.1 ignored; cannot have 0.1 of a byte.)
- "1.6666 decoded" → ambiguous. For a PoC we expand each stored byte to a block of 1666 bytes on decode (1.6666 * 1000 ≈ 1666) to demonstrate a different expansion factor.
  This cannot restore original data; purely demonstrative.
Adjust the constants below if you intended different semantics.
##>

param(
  [Parameter(Mandatory)]
  [ValidateSet('compress','decompress')]
  [string]$Action,

  [Parameter(Mandatory)]
  [ValidateScript({Test-Path $_ -PathType Container})]
  [string]$Folder,

  [int]$ChunkSize = 6666,
  [int]$DecodeExpand = 1666
)

Add-Type -TypeDefinition @"
using System;
using System.IO;

public static class Fold6666
{
    public static byte CompressChunk(byte[] buf, int len){
        byte acc = 0; // XOR fold
        for(int i=0;i<len;i++) acc ^= buf[i];
        return acc;
    }
    public static byte[] DecompressByte(byte b, int expand){
        byte[] buf = new byte[expand];
        for(int i=0;i<expand;i++) buf[i] = b;
        return buf;
    }
}
"@

function Compress-6666 {
  param($File)
  $out = $File.FullName + '.6666'
  $inStream  = [System.IO.File]::Open($File.FullName, 'Open', 'Read', 'Read')
  $outStream = [System.IO.File]::Create($out)
  $buf = New-Object byte[] $ChunkSize
  $total = 0
  while(($read = $inStream.Read($buf,0,$ChunkSize)) -gt 0){
    $total += $read
    if($read -lt $ChunkSize){ # last partial chunk → pad zeros (already zero-init remainder)
      # nothing extra needed, buf already sized ChunkSize
    }
    $b = [Fold6666]::CompressChunk($buf, $read)
    $outStream.WriteByte($b)
  }
  $inStream.Close(); $outStream.Close()
  (Get-Item $out).LastWriteTime = $File.LastWriteTime
  $orig = $File.Length; $comp = (Get-Item $out).Length
  Write-Host "Compressed $($File.Name): $orig bytes -> $comp bytes (ratio $(('{0:P6}' -f ($comp/$orig))))" -ForegroundColor Cyan
}

function Decompress-6666 {
  param($File)
  $base = $File.FullName -replace '\.6666$',''
  $inStream  = [System.IO.File]::Open($File.FullName,'Open','Read','Read')
  $outStream = [System.IO.File]::Create($base)
  while($inStream.Position -lt $inStream.Length){
    $b = $inStream.ReadByte()
    if($b -lt 0){ break }
    $chunk = [Fold6666]::DecompressByte([byte]$b, $DecodeExpand)
    $outStream.Write($chunk,0,$chunk.Length)
  }
  $inStream.Close(); $outStream.Close()
  (Get-Item $base).LastWriteTime = $File.LastWriteTime
  $orig = (Get-Item $File.FullName).Length; $dec = (Get-Item $base).Length
  Write-Host "Decompressed $($File.Name): $orig bytes -> $dec bytes (expanded $(('{0:N0}' -f ($dec/$orig)))×)" -ForegroundColor Magenta
}

# ---- main ----
Get-ChildItem $Folder -File | Where-Object Length -GE 2GB | ForEach-Object {
  if($Action -eq 'compress'){ Compress-6666 $_ }
  else { Decompress-6666 $_ }
}

Write-Host "Done."