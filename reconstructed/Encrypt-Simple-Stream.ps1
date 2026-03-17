#Simplified stream cipher encryption (XOR keystream derived from SHA-256 key expansion)
param(
  [Parameter(Mandatory)][string]$InputFile,
  [Parameter(Mandatory)][string]$KeyFile,
  [Parameter(Mandatory)][string]$OutputFile
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

if(-not (Test-Path $InputFile)){ throw "Input file not found: $InputFile" }
if(-not (Test-Path $KeyFile)){ throw "Key file not found: $KeyFile" }

$key = [IO.File]::ReadAllBytes($KeyFile)
if($key.Length -ne 32){ throw "Key must be 32 bytes (256-bit)" }

# Simple keystream derivation: expand key via repeated hashing
function Expand-Keystream([byte[]]$key, [int]$length){
    $stream = New-Object byte[] $length
    $sha = [Security.Cryptography.SHA256]::Create()
    $seed = $key
    for($i=0; $i -lt $length; $i+=32){
        $seed = $sha.ComputeHash($seed)
        $copyLen = [Math]::Min(32, $length - $i)
        [Array]::Copy($seed, 0, $stream, $i, $copyLen)
    }
    $sha.Dispose()
    return $stream
}

Write-Host "Reading input file..." -ForegroundColor Cyan
$data = [IO.File]::ReadAllBytes($InputFile)
Write-Host "Generating keystream ($($data.Length) bytes)..." -ForegroundColor Cyan
$keystream = Expand-Keystream $key $data.Length

Write-Host "Encrypting..." -ForegroundColor Cyan
$cipher = New-Object byte[] $data.Length
for($i=0; $i -lt $data.Length; $i++){
    $cipher[$i] = $data[$i] -bxor $keystream[$i]
}

Write-Host "Writing encrypted output..." -ForegroundColor Cyan
[IO.File]::WriteAllBytes($OutputFile, $cipher)
Write-Host "✓ Encrypted: $OutputFile ($($cipher.Length) bytes)" -ForegroundColor Green
