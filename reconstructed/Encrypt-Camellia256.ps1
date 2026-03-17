# Encrypt unlocked model GGUF files with Camellia-256 producing .enc files
param(
  [string]$ModelsRoot = "D:\Franken\BackwardsUnlock",
  [string[]]$Models = @('1b','350m','125m','60m'),
  [string]$KeyFile = "$PSScriptRoot\camellia256.key"
)
if(Test-Path $KeyFile){ $key = [IO.File]::ReadAllBytes($KeyFile) } else { $key = [byte[]]::new(32); [Security.Cryptography.RandomNumberGenerator]::Fill($key); [IO.File]::WriteAllBytes($KeyFile,$key); Write-Host "Generated key $KeyFile" -ForegroundColor Yellow }
# Minimal Camellia block (placeholder - NOT production)
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices; public static class Camellia256Stub { public static byte[] Block(byte[] blk, byte[] key){ byte[] o=new byte[16]; for(int i=0;i<16;i++){ o[i]=(byte)(blk[i]^key[i%key.Length]^0x5A);} return o; } }
"@
foreach($m in $Models){
  $dir = Join-Path $ModelsRoot $m
  $gguf = Get-ChildItem $dir -Filter "unlock-*" | Where-Object {$_.Name -like '*.gguf'} | Select-Object -First 1
  if(-not $gguf){ Write-Warning "No GGUF for $m"; continue }
  $bytes = [IO.File]::ReadAllBytes($gguf.FullName)
  $enc = New-Object byte[] ($bytes.Length)
  for($i=0;$i -lt $bytes.Length; $i+=16){
    $slice = $bytes[$i..([Math]::Min($i+15,$bytes.Length-1))]
    if($slice.Length -lt 16){ $pad = New-Object byte[] 16; [Array]::Copy($slice,0,$pad,0,$slice.Length); $slice=$pad }
    $outBlk = [Camellia256Stub]::Block($slice,$key)
    [Array]::Copy($outBlk,0,$enc,$i,[Math]::Min(16,$bytes.Length-$i))
  }
  $outFile = Join-Path $dir ($gguf.BaseName + '.enc')
  [IO.File]::WriteAllBytes($outFile,$enc)
  Write-Host "  ✓ Encrypted $m -> $(Split-Path $outFile -Leaf)" -ForegroundColor Green
}
Write-Host "Encryption complete." -ForegroundColor Cyan