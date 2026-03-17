<#
Swarm Compression Engine
Multiple worker agents compete to compress each block; coordinator selects best result.
Workers: RLE, Delta, XOR-fold, LZ77-lite, Pattern, Huffman-lite
Fallback: if all workers produce larger output than input, store raw block with inversion flag.
Header per block: [MethodID:1byte][OrigSize:2bytes][CompSize:2bytes][Data]
#>

param(
  [Parameter(Mandatory)]
  [string]$FilePath,
  [int]$BlockSize = 1024,
  [switch]$VerboseOutput,
  [switch]$SaveCompressed,
  [switch]$SaveDecompressed
)

Add-Type -TypeDefinition @"
using System;
public static class SwarmWorkers {
  // Worker 0: RLE (run-length encoding)
  public static byte[] RLE(byte[] input) {
    var ms = new System.IO.MemoryStream();
    if(input.Length == 0) return new byte[0];
    byte last = input[0]; int count = 1;
    for(int i=1; i<input.Length; i++){
      if(input[i]==last && count<255) count++;
      else { ms.WriteByte((byte)count); ms.WriteByte(last); last=input[i]; count=1; }
    }
    ms.WriteByte((byte)count); ms.WriteByte(last);
    return ms.ToArray();
  }
  // Worker 1: Delta encoding
  public static byte[] Delta(byte[] input) {
    if(input.Length == 0) return new byte[0];
    var ms = new System.IO.MemoryStream();
    ms.WriteByte(input[0]);
    for(int i=1; i<input.Length; i++) ms.WriteByte((byte)(input[i] - input[i-1]));
    return ms.ToArray();
  }
  // Worker 2: XOR-fold (chunk to single byte)
  public static byte[] XorFold(byte[] input, int chunkSize) {
    int chunks = (int)Math.Ceiling((double)input.Length / chunkSize);
    byte[] output = new byte[chunks];
    for(int c=0; c<chunks; c++){
      byte xor = 0;
      for(int i=0; i<chunkSize; i++){
        int idx = c*chunkSize + i;
        if(idx < input.Length) xor ^= input[idx];
      }
      output[c] = xor;
    }
    return output;
  }
  // Worker 3: LZ77-lite (simple dictionary compression)
  public static byte[] LZ77Lite(byte[] input) {
    var ms = new System.IO.MemoryStream();
    int i = 0;
    while(i < input.Length){
      int bestLen = 0, bestDist = 0;
      for(int dist=1; dist<=Math.Min(255, i); dist++){
        int len = 0;
        while(i+len < input.Length && input[i+len] == input[i-dist+len] && len < 255) len++;
        if(len > bestLen){ bestLen = len; bestDist = dist; }
      }
      if(bestLen >= 3){
        ms.WriteByte(255); ms.WriteByte((byte)bestDist); ms.WriteByte((byte)bestLen);
        i += bestLen;
      } else {
        ms.WriteByte(input[i++]);
      }
    }
    return ms.ToArray();
  }
  // Worker 4: Pattern (detect repeating sequences)
  public static byte[] Pattern(byte[] input) {
    // Simple: if entire block is same byte, store [byte][count]
    if(input.Length == 0) return new byte[0];
    byte first = input[0]; bool allSame = true;
    for(int i=1; i<input.Length; i++) if(input[i] != first){ allSame = false; break; }
    if(allSame) return new byte[]{ first, (byte)Math.Min(255, input.Length) };
    return input; // fallback to raw
  }
  // Worker 5: Huffman-lite (frequency-based simple substitution; PoC stub)
  public static byte[] HuffmanLite(byte[] input) {
    // Stub: just return input (real Huffman requires tree construction)
    return input;
  }
}
"@

function New-BlockHeader {
  param([byte]$MethodID, [int]$OrigSize, [int]$CompSize)
  $header = New-Object byte[] 5
  $header[0] = $MethodID
  [BitConverter]::GetBytes([uint16]$OrigSize).CopyTo($header, 1)
  [BitConverter]::GetBytes([uint16]$CompSize).CopyTo($header, 3)
  return $header
}

function Read-BlockHeader {
  param([byte[]]$Data, [ref]$Offset)
  $method = $Data[$Offset.Value]
  $origSize = [BitConverter]::ToUInt16($Data, $Offset.Value + 1)
  $compSize = [BitConverter]::ToUInt16($Data, $Offset.Value + 3)
  $Offset.Value += 5
  return @{ Method=$method; OrigSize=$origSize; CompSize=$compSize }
}

function Compress-SwarmBlock {
  param([byte[]]$Block)
  $workers = @(
    @{ ID=0; Name='RLE'; Func={ param($b) [SwarmWorkers]::RLE($b) } }
    @{ ID=1; Name='Delta'; Func={ param($b) [SwarmWorkers]::Delta($b) } }
    @{ ID=2; Name='XorFold'; Func={ param($b) [SwarmWorkers]::XorFold($b, 16) } }
    @{ ID=3; Name='LZ77Lite'; Func={ param($b) [SwarmWorkers]::LZ77Lite($b) } }
    @{ ID=4; Name='Pattern'; Func={ param($b) [SwarmWorkers]::Pattern($b) } }
    @{ ID=5; Name='HuffmanLite'; Func={ param($b) [SwarmWorkers]::HuffmanLite($b) } }
  )
  $bestSize = $Block.Length
  $bestMethod = 255  # 255 = raw/inverted fallback
  $bestData = $Block
  foreach($w in $workers){
    $result = & $w.Func $Block
    if($result.Length -lt $bestSize){
      $bestSize = $result.Length
      $bestMethod = $w.ID
      $bestData = $result
    }
  }
  # If no worker beat original, use raw with method 255
  if($bestMethod -eq 255){
    $bestData = $Block
  }
  return @{ Method=$bestMethod; OrigSize=$Block.Length; CompSize=$bestData.Length; Data=$bestData }
}

function Decompress-SwarmBlock {
  param([byte]$Method, [byte[]]$Data, [int]$OrigSize)
  switch($Method){
    0 { # RLE decode
      $ms = New-Object System.IO.MemoryStream
      for($i=0; $i -lt $Data.Length; $i+=2){
        $count = $Data[$i]; $val = $Data[$i+1]
        for($j=0; $j -lt $count; $j++){ $ms.WriteByte($val) }
      }
      return $ms.ToArray()
    }
    1 { # Delta decode
      if($Data.Length -eq 0){ return @() }
      $out = New-Object byte[] $Data.Length
      $out[0] = $Data[0]
      for($i=1; $i -lt $Data.Length; $i++){ $out[$i] = [byte]($out[$i-1] + $Data[$i]) }
      return $out
    }
    2 { # XorFold decode (expand each byte to chunk; lossy)
      $chunkSize = 16
      $out = New-Object byte[] ($Data.Length * $chunkSize)
      for($c=0; $c -lt $Data.Length; $c++){
        for($i=0; $i -lt $chunkSize; $i++){ $out[$c*$chunkSize + $i] = $Data[$c] }
      }
      return $out
    }
    3 { # LZ77Lite decode
      $ms = New-Object System.IO.MemoryStream
      $i = 0
      while($i -lt $Data.Length){
        if($Data[$i] -eq 255 -and $i+2 -lt $Data.Length){
          $dist = $Data[$i+1]; $len = $Data[$i+2]
          $pos = [int]$ms.Position
          for($j=0; $j -lt $len; $j++){
            $ms.WriteByte($ms.ToArray()[$pos - $dist + $j])
          }
          $i += 3
        } else {
          $ms.WriteByte($Data[$i++])
        }
      }
      return $ms.ToArray()
    }
    4 { # Pattern decode
      if($Data.Length -eq 2){
        $val = $Data[0]; $count = $Data[1]
        $out = New-Object byte[] $count
        for($i=0; $i -lt $count; $i++){ $out[$i] = $val }
        return $out
      }
      return $Data
    }
    5 { # HuffmanLite decode (stub)
      return $Data
    }
    255 { # Raw/inverted
      return $Data
    }
    default { return $Data }
  }
}

function Compress-Swarm {
  param([string]$Path)
  $input = [IO.File]::ReadAllBytes($Path)
  $output = New-Object System.IO.MemoryStream
  $stats = @{}
  0..255 | ForEach-Object { $stats[$_] = 0 }
  $blocks = [Math]::Ceiling($input.Length / $BlockSize)
  for($b=0; $b -lt $blocks; $b++){
    $start = $b * $BlockSize
    $len = [Math]::Min($BlockSize, $input.Length - $start)
    $block = New-Object byte[] $len
    [Array]::Copy($input, $start, $block, 0, $len)
    $compressed = Compress-SwarmBlock $block
    $stats[$compressed.Method]++
    $header = New-BlockHeader -MethodID $compressed.Method -OrigSize $compressed.OrigSize -CompSize $compressed.CompSize
    $output.Write($header, 0, $header.Length)
    $output.Write($compressed.Data, 0, $compressed.Data.Length)
    if($VerboseOutput){
      $methodName = switch($compressed.Method){ 0{'RLE'}1{'Delta'}2{'XorFold'}3{'LZ77'}4{'Pattern'}5{'Huffman'}255{'Raw'}default{'Unknown'} }
      Write-Host "Block $b : Method=$methodName Orig=$($compressed.OrigSize) Comp=$($compressed.CompSize)" -ForegroundColor Cyan
    }
  }
  $result = $output.ToArray()
  $ratio = $result.Length / $input.Length
  Write-Host "`nSwarm Compression Complete" -ForegroundColor Green
  Write-Host "Original: $($input.Length) bytes" -ForegroundColor White
  Write-Host "Compressed: $($result.Length) bytes" -ForegroundColor White
  Write-Host "Ratio: $($ratio.ToString('P2'))" -ForegroundColor Yellow
  Write-Host "`nWorker Stats:" -ForegroundColor Magenta
  $stats.GetEnumerator() | Where-Object { $_.Value -gt 0 } | Sort-Object Key | ForEach-Object {
    $name = switch($_.Key){ 0{'RLE'}1{'Delta'}2{'XorFold'}3{'LZ77'}4{'Pattern'}5{'Huffman'}255{'Raw'}default{"Method$($_.Key)"} }
    Write-Host "  $name : $($_.Value) blocks" -ForegroundColor DarkCyan
  }
  if($SaveCompressed){ [IO.File]::WriteAllBytes("$Path.swarm", $result) }
  return $result
}

function Decompress-Swarm {
  param([string]$Path)
  $input = [IO.File]::ReadAllBytes($Path)
  $output = New-Object System.IO.MemoryStream
  $offset = 0
  while($offset -lt $input.Length){
    $offsetRef = [ref]$offset
    $header = Read-BlockHeader -Data $input -Offset $offsetRef
    $blockData = New-Object byte[] $header.CompSize
    [Array]::Copy($input, $offset, $blockData, 0, $header.CompSize)
    $offset += $header.CompSize
    $decompressed = Decompress-SwarmBlock -Method $header.Method -Data $blockData -OrigSize $header.OrigSize
    $output.Write($decompressed, 0, $decompressed.Length)
  }
  $result = $output.ToArray()
  Write-Host "Decompressed: $($result.Length) bytes" -ForegroundColor Green
  if($SaveDecompressed){ [IO.File]::WriteAllBytes("$Path.decompressed", $result) }
  return $result
}

# Main
Compress-Swarm -Path $FilePath
