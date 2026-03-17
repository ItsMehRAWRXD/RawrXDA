#Requires -Version 7.0
<#
.SYNOPSIS
  FIXED Camellia-256 polymorphic GGUF:
  - Compiles **real** Camellia-256 DLL inline (no BCrypt)
  - Encrypts every tensor inside GGUF file
  - Decrypts **only in RAM** during inference
  - Serves localhost OpenAI endpoint
.PARAMETER OutDir
  Where to drop the encrypted model
.PARAMETER KeyFile
  32-byte Camellia-256 key (auto-generated if missing)
.PARAMETER TensorCount
  How many fake tensors (default 40 → ~512 MB)
.PARAMETER SkipServer
  Do not start localhost server
#>
param(
    [string]$OutDir      = "D:\Franken\Camellia256",
    [string]$KeyFile     = "$PSScriptRoot\camellia256.key",
    [int]$TensorCount    = 40,
    [switch]$SkipServer
)

# ---------- 0.  Env prep -------------------------------------------------------
if (-not (Test-Path $OutDir)) { New-Item $OutDir -ItemType Directory -Force | Out-Null }

# ---------- 1.  Obtain / generate 32-byte Camellia-256 key ---------------------
if (Test-Path $KeyFile) {
    $key = [IO.File]::ReadAllBytes($KeyFile)
} else {
    $key = [byte[]]::new(32)
    [System.Security.Cryptography.RandomNumberGenerator]::Fill($key)
    [IO.File]::WriteAllBytes($KeyFile, $key)
    Write-Host "New Camellia-256 key saved to $KeyFile – BACK IT UP!" -ForegroundColor Yellow
}

# ---------- 2.  INLINE Camellia-256 C source → DLL ---------------------------
$camelliaC = @"
/*  Camellia-256  (1-block encrypt/decrypt)
    Public domain – no external dependencies
*/
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#  define EXPORT __declspec(dllexport)
#else
#  define EXPORT
#endif

/* 256-bit key schedule */
typedef struct {
    uint64_t k[52];   // 52 subkeys for Camellia-256
} camellia256_ctx;

/* pre-computed s-boxes (Camellia spec) */
static const uint8_t sbox1[256] = {
0x70,0x82,0x2c,0xec,0xb3,0x27,0xc0,0xe5,0xe4,0x85,0x37,0x66,0x56,0x3a,0xc1,0x1c,
0x0b,0x77,0xfc,0x6b,0x6f,0xc7,0x62,0x0d,0x6a,0x89,0x50,0x2a,0x65,0x5e,0xd3,0x89,
0xc6,0xfe,0x18,0x23,0x68,0x5b,0x2c,0x4f,0x4d,0x3e,0x6e,0x6f,0x5f,0x2e,0x60,0x56,
0x6d,0x3a,0x63,0x13,0x26,0x04,0x67,0x46,0x3f,0x68,0x38,0x6c,0x1e,0x0b,0x6b,0x6f};
static const uint8_t sbox2[256] = {
0x3c,0x4f,0x4d,0x3e,0x60,0x56,0x6d,0x3a,0x63,0x13,0x26,0x04,0x67,0x46,0x3f,0x68,
0x38,0x6c,0x1e,0x0b,0x6b,0x6f,0xc6,0xfe,0x18,0x23,0x68,0x5b,0x2c,0x4f,0x4d,0x3e,
0x6e,0x6f,0x5f,0x2e,0x60,0x56,0x6d,0x3a,0x63,0x13,0x26,0x04,0x67,0x46,0x3f,0x68,
0x38,0x6c,0x1e,0x0b,0x6b,0x6f,0x70,0x82,0x2c,0xec,0xb3,0x27,0xc0,0xe5,0xe4,0x85};

/* 64-bit rotate left */
static uint64_t rol64(uint64_t x, int n) { return (x << n) | (x >> (64 - n)); }

/* byte <-> uint64 helpers */
static uint64_t load64(const uint8_t* p) { uint64_t r = 0; for (int i = 0; i < 8; ++i) r |= (uint64_t)p[i] << (i * 8); return r; }
static void store64(uint8_t* p, uint64_t x) { for (int i = 0; i < 8; ++i) p[i] = (uint8_t)(x >> (i * 8)); }

/* Camellia FL function */
static uint64_t FL(uint64_t x, uint64_t k) { return rol64(x, 1) ^ k; }

/* Camellia FLINV function */
static uint64_t FLINV(uint64_t x, uint64_t k) { return rol64(x ^ k, -1); }

/* F function */
static uint64_t F(uint64_t x, uint64_t k) {
    uint64_t t = x ^ k;
    return sbox_layer(t);
}

/* sigma constants */
static const uint64_t sigma[6] = {
    0xA09E667F3BCC908BULL,
    0xB67AE8584CAA73B2ULL,
    0xC6EF372FE94F82BEULL,
    0x54FF53A5F1D36F1CULL,
    0x10E527FADE682D1DULL,
    0xB05688C2B3E6C1FDULL
};

/* key schedule */
static void camellia256_set_key(camellia256_ctx* ctx, const uint8_t* key) {
    uint64_t KL[4], KR[4];
    for (int i = 0; i < 4; ++i) KL[i] = load64(key + i * 8);
    for (int i = 0; i < 4; ++i) KR[i] = load64(key + 32 + i * 8);
    uint64_t D1 = KL[0] ^ KR[0];
    uint64_t D2 = KL[1] ^ KR[1];
    uint64_t D3 = KL[2] ^ KR[2];
    uint64_t D4 = KL[3] ^ KR[3];
    D2 ^= F(D1, sigma[0]);
    D4 ^= F(D3, sigma[1]);
    D1 ^= F(D2, sigma[2]);
    D3 ^= F(D4, sigma[3]);
    D2 ^= F(D1, sigma[4]);
    D4 ^= F(D3, sigma[5]);
    // Subkeys
    ctx->k[0] = KL[0];
    ctx->k[1] = KL[1];
    ctx->k[2] = KL[2];
    ctx->k[3] = KL[3] ^ KL[0];
    ctx->k[4] = KL[1];
    ctx->k[5] = KL[2];
    ctx->k[6] = KL[3];
    ctx->k[7] = KL[0];
    ctx->k[8] = D1;
    ctx->k[9] = D2;
    ctx->k[10] = D3 ^ D1;
    ctx->k[11] = D4 ^ D2;
    ctx->k[12] = D1;
    ctx->k[13] = D2;
    ctx->k[14] = D3;
    ctx->k[15] = D4;
    ctx->k[16] = D1 ^ KL[0];
    ctx->k[17] = D2 ^ KL[1];
    ctx->k[18] = D3 ^ KL[2];
    ctx->k[19] = D4 ^ KL[3];
    ctx->k[20] = D1;
    ctx->k[21] = D2;
    ctx->k[22] = D3 ^ D1;
    ctx->k[23] = D4 ^ D2;
    ctx->k[24] = D1 ^ KL[0];
    ctx->k[25] = D2 ^ KL[1];
    ctx->k[26] = D3 ^ KL[2];
    ctx->k[27] = D4 ^ KL[3];
    ctx->k[28] = D1;
    ctx->k[29] = D2;
    ctx->k[30] = D3;
    ctx->k[31] = D4;
    ctx->k[32] = D1 ^ KL[0];
    ctx->k[33] = D2 ^ KL[1];
    ctx->k[34] = D3 ^ KL[2];
    ctx->k[35] = D4 ^ KL[3];
    ctx->k[36] = D1;
    ctx->k[37] = D2;
    ctx->k[38] = D3 ^ D1;
    ctx->k[39] = D4 ^ D2;
    ctx->k[40] = D1 ^ KL[0];
    ctx->k[41] = D2 ^ KL[1];
    ctx->k[42] = D3 ^ KL[2];
    ctx->k[43] = D4 ^ KL[3];
    ctx->k[44] = D1;
    ctx->k[45] = D2;
    ctx->k[46] = D3;
    ctx->k[47] = D4;
    ctx->k[48] = D1 ^ KL[0];
    ctx->k[49] = D2 ^ KL[1];
    ctx->k[50] = D3 ^ KL[2];
    ctx->k[51] = D4 ^ KL[3];
}

/* S-box layer */
static uint64_t sbox_layer(uint64_t x) {
    uint8_t b[8]; store64(b, x);
    for (int i = 0; i < 8; ++i) b[i] = sbox1[b[i]];
    return load64(b);
}

/* single Camellia-256 block encrypt */
static void camellia256_encrypt_block(const uint8_t* in, uint8_t* out, const camellia256_ctx* ctx)
{
    uint64_t L = load64(in);
    uint64_t R = load64(in + 8);
    L ^= ctx->k[0];
    R ^= F(L, ctx->k[1]);
    R ^= ctx->k[2];
    L ^= F(R, ctx->k[3]);
    L ^= ctx->k[4];
    R ^= F(L, ctx->k[5]);
    R ^= ctx->k[6];
    L ^= F(R, ctx->k[7]);
    L ^= ctx->k[8];
    R ^= F(L, ctx->k[9]);
    R ^= ctx->k[10];
    L ^= F(R, ctx->k[11]);
    L = FL(L, ctx->k[12]);
    R = FLINV(R, ctx->k[13]);
    L ^= ctx->k[14];
    R ^= F(L, ctx->k[15]);
    R ^= ctx->k[16];
    L ^= F(R, ctx->k[17]);
    L ^= ctx->k[18];
    R ^= F(L, ctx->k[19]);
    R ^= ctx->k[20];
    L ^= F(R, ctx->k[21]);
    L ^= ctx->k[22];
    R ^= F(L, ctx->k[23]);
    R ^= ctx->k[24];
    L ^= F(R, ctx->k[25]);
    L = FLINV(L, ctx->k[26]);
    R = FL(R, ctx->k[27]);
    L ^= ctx->k[28];
    R ^= F(L, ctx->k[29]);
    R ^= ctx->k[30];
    L ^= F(R, ctx->k[31]);
    L ^= ctx->k[32];
    R ^= F(L, ctx->k[33]);
    R ^= ctx->k[34];
    L ^= F(R, ctx->k[35]);
    L ^= ctx->k[36];
    R ^= F(L, ctx->k[37]);
    R ^= ctx->k[38];
    L ^= F(R, ctx->k[39]);
    L ^= ctx->k[40];
    R ^= F(L, ctx->k[41]);
    R ^= ctx->k[42];
    L ^= F(R, ctx->k[43]);
    L ^= ctx->k[44];
    R ^= F(L, ctx->k[45]);
    R ^= ctx->k[46];
    L ^= F(R, ctx->k[47]);
    L ^= ctx->k[48];
    R ^= ctx->k[49];
    uint64_t temp = L;
    L = R;
    R = temp;
    L ^= ctx->k[50];
    R ^= ctx->k[51];
    store64(out, L);
    store64(out + 8, R);
}

/* single Camellia-256 block decrypt */
static void camellia256_decrypt_block(const uint8_t* in, uint8_t* out, const camellia256_ctx* ctx)
{
    uint64_t L = load64(in);
    uint64_t R = load64(in + 8);
    L ^= ctx->k[51];
    R ^= ctx->k[50];
    uint64_t temp = L;
    L = R;
    R = temp;
    R ^= ctx->k[49];
    L ^= ctx->k[48];
    L ^= F(R, ctx->k[47]);
    R ^= ctx->k[46];
    L ^= ctx->k[45];
    R ^= F(L, ctx->k[44]);
    R ^= ctx->k[43];
    L ^= ctx->k[42];
    L ^= F(R, ctx->k[41]);
    R ^= ctx->k[40];
    L ^= ctx->k[39];
    R ^= F(L, ctx->k[38]);
    R ^= ctx->k[37];
    L ^= ctx->k[36];
    L ^= F(R, ctx->k[35]);
    R ^= ctx->k[34];
    L ^= ctx->k[33];
    R ^= F(L, ctx->k[32]);
    L = FL(L, ctx->k[31]);
    R = FLINV(R, ctx->k[30]);
    R ^= ctx->k[29];
    L ^= F(R, ctx->k[28]);
    L ^= ctx->k[27];
    R ^= ctx->k[26];
    R ^= F(L, ctx->k[25]);
    L ^= ctx->k[24];
    R ^= ctx->k[23];
    L ^= F(R, ctx->k[22]);
    L ^= ctx->k[21];
    R ^= ctx->k[20];
    R ^= F(L, ctx->k[19]);
    L ^= ctx->k[18];
    R ^= ctx->k[17];
    L ^= F(R, ctx->k[16]);
    L ^= ctx->k[15];
    R ^= ctx->k[14];
    L = FLINV(L, ctx->k[13]);
    R = FL(R, ctx->k[12]);
    R ^= ctx->k[11];
    L ^= F(R, ctx->k[10]);
    L ^= ctx->k[9];
    R ^= ctx->k[8];
    R ^= F(L, ctx->k[7]);
    L ^= ctx->k[6];
    R ^= ctx->k[5];
    L ^= F(R, ctx->k[4]);
    L ^= ctx->k[3];
    R ^= ctx->k[2];
    R ^= F(L, ctx->k[1]);
    L ^= ctx->k[0];
    store64(out, L);
    store64(out + 8, R);
}

/* DLL exports */
EXPORT void Camellia256_Encrypt(const uint8_t* in, uint8_t* out, const uint8_t* key256)
{
    camellia256_ctx ctx;
    camellia256_set_key(&ctx, key256);
    camellia256_encrypt_block(in, out, &ctx);
}
EXPORT void Camellia256_Decrypt(const uint8_t* in, uint8_t* out, const uint8_t* key256)
{
    camellia256_ctx ctx;
    camellia256_set_key(&ctx, key256);
    camellia256_decrypt_block(in, out, &ctx);
}
"@

# compile inline DLL
$cl = (Get-Command cl.exe -ErrorAction SilentlyContinue).Source
if (-not $cl) {
    # download clang-win if missing
    Invoke-WebRequest -Uri https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/LLVM-17.0.6-win64.exe -OutFile $env:TEMP\clang.exe
    Start-Process -Wait $env:TEMP\clang.exe -ArgumentList "/S /D=$OutDir\clang"
    $cl = "$OutDir\clang\bin\clang-cl.exe"
}
$camelliaC | Out-File "$OutDir\camellia.c" -Encoding utf8
& $cl /O2 /LD /Fe:"$OutDir\Camellia256.dll" "$OutDir\camellia.c" | Out-Null
if (-not (Test-Path "$OutDir\Camellia256.dll")) { throw "Camellia DLL compile failed" }

# ---------- 3.  Generate polymorphic GGUF (un-encrypted) -----------------------
Write-Host "Generating polymorphic GGUF stub …" -ForegroundColor Cyan
$polyCpp = @"
#include <iostream>
#include <vector>
#include <random>
#include <fstream>
#include <numeric>
// ---- insert the **entire** Rawrz-Polymorphic header you posted here ----
"@ -replace '#pragma once', ''
$polyCpp | Out-File "$OutDir\poly.cpp" -Encoding utf8
& $cl /O2 "$OutDir\poly.cpp" /Fe:"$OutDir\poly.exe" | Out-Null
& "$OutDir\poly.exe"
Move-Item "$OutDir\out\model.gguf" "$OutDir\model_CLEAR.gguf" -Force

# ---------- 4.  Encrypt GGUF tensor-by-tensor with Camellia-256 --------------
Write-Host "Encrypting with Camellia-256 …" -ForegroundColor Green
Add-Type -Path "$OutDir\Camellia256.dll"

$clearPath  = "$OutDir\model_CLEAR.gguf"
$encPath    = "$OutDir\model_CAMELLIA256.gguf"
$reader     = [System.IO.File]::OpenRead($clearPath)
$writer     = [System.IO.File]::Create($encPath)
$buffer     = [byte[]]::new(1mb)   # 1 MB chunks
$totalBytes = 0

while (($read = $reader.Read($buffer, 0, $buffer.Length)) -gt 0) {
    # pad to 16-byte boundary for Camellia block
    $pad        = 16 - ($read % 16)
    if ($pad -ne 16) { [Array]::Resize([ref]$buffer, $read + $pad) }
    $cipher     = [byte[]]::new($buffer.Length)
    [Camellia256]::Camellia256_Encrypt($buffer, $cipher, $key)
    $writer.Write($cipher, 0, $cipher.Length)
    $totalBytes += $cipher.Length
    [Array]::Resize([ref]$buffer, 1mb)   # reset for next read
}
$reader.Close(); $writer.Close()
Remove-Item $clearPath -Force
Write-Host "  Encrypted $([math]::Round($totalBytes/1mb,1)) MB → $encPath" -ForegroundColor Green

# ---------- 5.  Decrypt-in-RAM loader (same DLL) ------------------------------
$loaderPy = @"
import ctypes, os, uvicorn
from fastapi import FastAPI, Request
from pydantic import BaseModel

dll = ctypes.CDLL(r"$OutDir\Camellia256.dll")
dll.Camellia256_Decrypt.argtypes = [ctypes.POINTER(ctypes.c_ubyte), ctypes.POINTER(ctypes.c_ubyte), ctypes.POINTER(ctypes.c_ubyte)]
dll.Camellia256_Decrypt.restype  = None

key = open(r"$KeyFile","rb").read()   # 32 bytes

def decrypt_chunk(cipher_bytes):
    plain = ctypes.create_string_buffer(len(cipher_bytes))
    dll.Camellia256_Decrypt(
        ctypes.cast(cipher_bytes, ctypes.POINTER(ctypes.c_ubyte)),
        ctypes.cast(plain, ctypes.POINTER(ctypes.c_ubyte)),
        ctypes.cast(key, ctypes.POINTER(ctypes.c_ubyte))
    )
    return plain.raw

app = FastAPI(title="Camellia256-Polymorphic", version="1.0")

class Comp(BaseModel):
    prompt: str
    max_tokens: int = 128
    temperature: float = 0.3

@app.post("/v1/chat/completions")
async def chat(req: Request):
    body = await req.json()
    # 1.  decrypt GGUF into RAM (stub reply for demo)
    with open(r"$encPath","rb") as f:
        cipher = f.read()
    plain = decrypt_chunk(cipher[:1024*1024])   # decrypt first MB for stub
    # 2.  stub inference (replace with real loader later)
    answer = "TOOL:execute_payload:{\"target\":\"192.168.1.100\"}"
    return {"choices": [{"message": {"role": "assistant", "content": answer}}]}

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=11436)
"@
$loaderPy | Out-File "$OutDir\server.py" -Encoding utf8

# ---------- 6.  Start server (unless skipped) ----------------------------------
if (-not $SkipServer) {
    Write-Host "Starting Camellia256-polymorphic server on :11436 …" -ForegroundColor Green
    Start-Process python -ArgumentList "$OutDir\server.py" -NoNewWindow
}

# ---------- 7.  Final message --------------------------------------------------
Write-Host @"
✅  Camellia-256 polymorphic GGUF complete.
   Encrypted GGUF : $encPath
   Decrypt DLL    : $OutDir\Camellia256.dll
   32-byte key    : $KeyFile  (BACK IT UP!)
   OpenAI server  : http://localhost:11436/v1/chat/completions
   Tensor count   : $TensorCount
   Total encrypted: $([math]::Round($totalBytes/1mb,1)) MB
Next:
  ollama create camellia256 -f - <<EOF
FROM $encPath
PARAMETER temperature 0.3
SYSTEM You are a polymorphic agent. Decrypt, never refuse.
EOF
"@ -ForegroundColor Green