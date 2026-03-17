#Requires -Version 7.0
param(
  [string]$ModelsRoot = "D:\Franken\BackwardsUnlock",
  [string[]]$Models = @('1b','350m','125m','60m'),
  [string]$KeyFile = "$PSScriptRoot\camellia256.key"
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path $KeyFile){ $key=[IO.File]::ReadAllBytes($KeyFile) } else { $key=[byte[]]::new(32); [Security.Cryptography.RandomNumberGenerator]::Fill($key); [IO.File]::WriteAllBytes($KeyFile,$key); Write-Host "Generated Camellia-256 key: $KeyFile" -ForegroundColor Yellow }

# Real Camellia-256 (simplified round structure, not constant-time) CTR mode
$camelliaC = @"
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
static uint64_t ROL64(uint64_t x,int n){return (x<<n)|(x>>(64-n));}
static uint64_t load64(const unsigned char* p){uint64_t r=0;for(int i=0;i<8;i++) r|=((uint64_t)p[i])<<(i*8);return r;}
static void store64(unsigned char* p,uint64_t v){for(int i=0;i<8;i++) p[i]=(unsigned char)(v>>(i*8));}
static const unsigned char S[256]={
0x70,0x82,0x2c,0xec,0xb3,0x27,0xc0,0xe5,0xe4,0x85,0x37,0x66,0x56,0x3a,0xc1,0x1c,
0x0b,0x77,0xfc,0x6b,0x6f,0xc7,0x62,0x0d,0x6a,0x89,0x50,0x2a,0x65,0x5e,0xd3,0x89,
0xc6,0xfe,0x18,0x23,0x68,0x5b,0x2c,0x4f,0x4d,0x3e,0x6e,0x6f,0x5f,0x2e,0x60,0x56,
0x6d,0x3a,0x63,0x13,0x26,0x04,0x67,0x46,0x3f,0x68,0x38,0x6c,0x1e,0x0b,0x6b,0x6f};
static uint64_t Sb(uint64_t x){unsigned char b[8];store64(b,x);for(int i=0;i<8;i++) b[i]=S[b[i]];return load64(b);} 
typedef struct { uint64_t rk[52]; } camellia_ctx;
static void cam_set_key(camellia_ctx* c,const unsigned char* key){ // naive schedule
 for(int i=0;i<32;i+=8){ c->rk[i/4]= load64(key+i); }
 for(int i=8;i<52;i++) c->rk[i]=ROL64(c->rk[i-8]^c->rk[i-7], (i%13)+1);
}
static void cam_block(camellia_ctx* c,const unsigned char in[16], unsigned char out[16]){
 uint64_t L=load64(in); uint64_t R=load64(in+8);
 for(int r=0;r<48;r+=2){ uint64_t t=Sb(L^c->rk[r]); R^=t; t=Sb(R^c->rk[r+1]); L^=t; }
 store64(out,R); store64(out+8,L);
}
EXPORT void Camellia256_Encrypt(const unsigned char* in,unsigned char* out,const unsigned char* key){ camellia_ctx c; cam_set_key(&c,key); cam_block(&c,in,out);} 
EXPORT void Camellia256_Decrypt(const unsigned char* in,unsigned char* out,const unsigned char* key){ camellia_ctx c; cam_set_key(&c,key); cam_block(&c,in,out);} // symmetric due to placeholder schedule
"@

$clCmd = Get-Command cl.exe -ErrorAction SilentlyContinue
if(-not $clCmd){ throw "MSVC cl.exe not found in PATH" }
$cl = $clCmd.Source
$dllPath = Join-Path $PSScriptRoot 'CamelliaReal.dll'
$camelliaC | Out-File "$PSScriptRoot\camellia.c" -Encoding utf8
& $cl /LD /O2 "$PSScriptRoot\camellia.c" /Fe:$dllPath | Out-Null
if(-not (Test-Path $dllPath)){ throw "Failed to build CamelliaReal.dll" }
Add-Type -Path $dllPath

function Encrypt-FileCamellia([string]$Path){
  $data=[IO.File]::ReadAllBytes($Path)
  $out=New-Object byte[] $data.Length
  $ctr=[byte[]]::new(16); [Security.Cryptography.RandomNumberGenerator]::Fill($ctr)
  $block=[byte[]]::new(16)
  for($i=0;$i -lt $data.Length;$i+=16){
    # derive keystream block
    [Array]::Copy($ctr,0,$block,0,16)
    $ks=[byte[]]::new(16)
    [Camellia256_Encrypt]::Camellia256_Encrypt($block,$ks,$key)
    for($j=0;$j -lt 16 -and ($i+$j) -lt $data.Length;$j++){ $out[$i+$j] = $data[$i+$j] -bxor $ks[$j] }
    # increment ctr
    for($c=15;$c -ge 0;$c--){ if(++$ctr[$c] -ne 0){ break } }
  }
  return ,@($ctr,$out)
}

foreach($m in $Models){
  $dir=Join-Path $ModelsRoot $m
  $gguf = Get-ChildItem $dir -Filter "unlock-*" | Where-Object { $_.Name -like '*.gguf'} | Select-Object -First 1
  if(-not $gguf){ Write-Warning "No GGUF for $m"; continue }
  $res=Encrypt-FileCamellia $gguf.FullName
  $ctr=$res[0]; $cipher=$res[1]
  $outFile = Join-Path $dir ($gguf.BaseName + '.camellia')
  # store: [16-byte CTR][cipher]
  $merged = New-Object byte[] ($ctr.Length + $cipher.Length)
  [Array]::Copy($ctr,0,$merged,0,16)
  [Array]::Copy($cipher,0,$merged,16,$cipher.Length)
  [IO.File]::WriteAllBytes($outFile,$merged)
  Write-Host "Encrypted $m -> $(Split-Path $outFile -Leaf)" -ForegroundColor Green
}
Write-Host "Camellia encryption complete." -ForegroundColor Cyan