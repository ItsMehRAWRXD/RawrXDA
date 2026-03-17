; =============================================================================
; Carmilla Universal Encryptor -- Pure MASM64  v2.0
; Camellia-256-GCM  |  PBKDF2-SHA512  |  Any File  |  Any Platform
;
; Block cipher : Camellia-256  (RFC 3713, pure MASM64 -- no AES-NI)
; Mode         : GCM  (CTR + GHASH, from scratch)
; KDF          : PBKDF2-SHA512  100000 iterations (BCrypt HMAC-SHA512)
; RNG          : BCryptGenRandom (system PRNG)
; =============================================================================

option casemap:none

extern GetCommandLineA:proc
extern GetStdHandle:proc
extern WriteConsoleA:proc
extern ExitProcess:proc
extern CreateFileA:proc
extern ReadFile:proc
extern WriteFile:proc
extern CloseHandle:proc
extern GetFileSize:proc
extern VirtualAlloc:proc
extern VirtualFree:proc
extern BCryptOpenAlgorithmProvider:proc
extern BCryptCloseAlgorithmProvider:proc
extern BCryptDeriveKeyPBKDF2:proc
extern BCryptGenRandom:proc

CAM_KEY_LEN     equ 32
GCM_IV_LEN      equ 12
GCM_TAG_LEN     equ 16
SALT_LEN        equ 32
PBKDF2_ROUNDS   equ 100000
HDR_MAGIC_LEN   equ 8
HDR_VERSION     equ 0001h
HDR_OVERHEAD    equ 76
GENERIC_READ    equ 80000000h
GENERIC_WRITE   equ 40000000h
FILE_SHARE_READ equ 1
OPEN_EXISTING   equ 3
CREATE_ALWAYS   equ 2
FILE_ATTR_NORM  equ 80h
INVALID_HANDLE  equ -1
STD_OUTPUT_HANDLE equ 0FFFFFFF5h
MEM_COMMIT      equ 1000h
MEM_RESERVE     equ 2000h
MEM_RELEASE     equ 8000h
PAGE_READWRITE  equ 4
BCRYPT_HMAC_FLAG equ 8
BCRYPT_SYS_RNG  equ 2
.data
align 8

SBOX1 label byte
db 070h,082h,02Ch,0ECh,0B3h,027h,0C0h,0E5h
db 0E4h,085h,057h,035h,0EAh,00Ch,0AEh,041h
db 023h,0EFh,06Bh,093h,045h,019h,0A5h,021h
db 0EDh,00Eh,04Fh,04Eh,01Dh,065h,092h,0BDh
db 086h,0B8h,0AFh,08Fh,07Ch,0EBh,01Fh,0CEh
db 03Eh,030h,0DCh,05Fh,05Eh,0C5h,00Bh,01Ah
db 0A6h,0E1h,039h,0CAh,0D5h,047h,05Dh,03Dh
db 0D9h,001h,05Ah,0D6h,051h,056h,06Ch,04Dh
db 08Bh,00Dh,09Ah,066h,0FBh,0CCh,0B0h,02Dh
db 074h,012h,02Bh,020h,0F0h,0B1h,084h,099h
db 0DFh,04Ch,0CBh,0C2h,034h,07Eh,076h,005h
db 06Dh,0B7h,0A9h,031h,0D1h,017h,004h,0D7h
db 014h,058h,03Ah,061h,0DEh,01Bh,011h,01Ch
db 032h,00Fh,09Ch,016h,053h,018h,0F2h,022h
db 0FEh,044h,0CFh,0B2h,0C3h,0B5h,07Ah,091h
db 024h,008h,0E8h,0A8h,060h,0FCh,069h,050h
db 0AAh,0D0h,0A0h,07Dh,0A1h,089h,062h,097h
db 054h,05Bh,01Eh,095h,0E0h,0FFh,064h,0D2h
db 010h,0C4h,000h,048h,0A3h,0F7h,075h,0DBh
db 08Ah,003h,0E6h,0DAh,009h,03Fh,0DDh,094h
db 087h,05Ch,083h,002h,0CDh,04Ah,090h,033h
db 073h,067h,0F6h,0F3h,09Dh,07Fh,0BFh,0E2h
db 052h,09Bh,0D8h,026h,0C8h,037h,0C6h,03Bh
db 081h,096h,06Fh,04Bh,013h,0BEh,063h,02Eh
db 0E9h,079h,0A7h,08Ch,09Fh,06Eh,0BCh,08Eh
db 029h,0F5h,0F9h,0B6h,02Fh,0FDh,0B4h,059h
db 078h,098h,006h,06Ah,0E7h,046h,071h,0BAh
db 0D4h,025h,0ABh,042h,088h,0A2h,08Dh,0FAh
db 072h,007h,0B9h,055h,0F8h,0EEh,0ACh,00Ah
db 036h,049h,02Ah,068h,03Ch,038h,0F1h,0A4h
db 040h,028h,0D3h,07Bh,0BBh,0C9h,043h,0C1h
db 015h,0E3h,0ADh,0F4h,077h,0C7h,080h,09Eh

SBOX2 db 256 dup(0)
SBOX3 db 256 dup(0)
SBOX4 db 256 dup(0)

align 8
Sigma1 dq 0A09E667F3BCC908Bh
Sigma2 dq 0B67AE8584CAA73B2h
Sigma3 dq 0C6EF372FE94F82BEh
Sigma4 dq 054FF53A5F1D36F1Ch
Sigma5 dq 010E527FADE682D1Dh
Sigma6 dq 0B05688C2B3E6C1FDh

align 8
camEK dq 34 dup(0)
camDK dq 34 dup(0)

align 16
gcmH   db 16 dup(0)
gcmJ0  db 16 dup(0)
gcmCtr db 16 dup(0)
gcmTag db 16 dup(0)

align 16
camKey db 32 dup(0)

align 8
bSalt  db SALT_LEN   dup(0)
bIV    db GCM_IV_LEN dup(0)

hAlgHMAC dq 0

szBanner  db 13,10
          db "  Carmilla v2.0 -- Camellia-256-GCM | PBKDF2-SHA512",13,10
          db "  Pure MASM64 -- encrypts any file for any platform",13,10,13,10,0
szUsage   db "  Usage:",13,10
          db "    carmilla_x64 encrypt <in> <out> [pass]",13,10
          db "    carmilla_x64 decrypt <in> <out> [pass]",13,10
          db "    carmilla_x64 pack <stub.bin> <container.car> <out>",13,10,0
szEnc     db "  [*] Encrypting ... ",0
szDec     db "  [*] Decrypting ... ",0
szPack    db "  [*] Packing stub + container ...",13,10,0
szPackOk  db "  [+] Packed -> ",0
szPackSz  db " bytes (stub=",0
szPackSz2 db " + car=",0
szPackSz3 db ")",13,10,0
szPackBad db "not a valid CSTB stub file",13,10,0
szDone    db "done",13,10,"  [+] Wrote ",0
szBytes   db " bytes",13,10,0
szCRLF    db 13,10,0
szFail    db "  [!] FATAL: ",0
szErrOpen db "cannot open input file",13,10,0
szErrWrt  db "cannot create output file",13,10,0
szErrMem  db "out of memory",13,10,0
szErrCryp db "crypto error NTSTATUS=0x",0
szErrMagic db "not a Carmilla container",13,10,0
szErrAuth db "authentication failed",13,10,0
szEncWord db "encrypt",0
szDecWord db "decrypt",0
szPackWord db "pack",0
szMagic   db "CARMILLA"
szCSTBMagic db "CSTB"
wszSHA512 dw 'S','H','A','5','1','2',0
szDefPass db "carmilla-default-key",0
DEF_PASS_LEN equ 20

hStdOut   dq 0
pArgMode  dq 0
pArgIn    dq 0
pArgOut   dq 0
pArgPass  dq 0
dwPassLen dd 0
bDecrypt  db 0
bPack     db 0
pArgStub  dq 0          ; pack mode: path to stub .bin
hFileIn   dq 0
dwFileSize dd 0
dwBytesRW dd 0
pFileBuf  dq 0
pOutBuf   dq 0
numBuf    db 20 dup(0)
hexBuf    db 9  dup(0)

.code

CamInitSboxes proc
    push rbx
    sub  rsp, 32
    xor  rbx, rbx
@cis_loop:
    lea  rcx, [SBOX1]
    movzx eax, byte ptr [rcx + rbx]
    mov  edx, eax
    shl  edx, 1
    mov  ecx, eax
    shr  ecx, 7
    or   edx, ecx
    and  edx, 0FFh
    lea  rcx, [SBOX2]
    mov  byte ptr [rcx + rbx], dl
    mov  edx, eax
    shr  edx, 1
    mov  ecx, eax
    shl  ecx, 7
    or   edx, ecx
    and  edx, 0FFh
    lea  rcx, [SBOX3]
    mov  byte ptr [rcx + rbx], dl
    mov  ecx, ebx
    shl  ecx, 1
    mov  edx, ebx
    shr  edx, 7
    or   ecx, edx
    and  ecx, 0FFh
    lea  rdx, [SBOX1]
    movzx ecx, byte ptr [rdx + rcx]
    lea  rdx, [SBOX4]
    mov  byte ptr [rdx + rbx], cl
    inc  rbx
    cmp  rbx, 256
    jb   @cis_loop
    add  rsp, 32
    pop  rbx
    ret
CamInitSboxes endp

CamF proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rsi
    sub  rsp, 40
    xor  rcx, rdx
    lea  r8,  [SBOX1]
    lea  r9,  [SBOX2]
    lea  r10, [SBOX3]
    lea  r11, [SBOX4]
    mov  rax, rcx
    shr  rax, 56
    movzx ebx,  byte ptr [r8  + rax]
    mov  rax, rcx
    shr  rax, 48
    and  rax, 0FFh
    movzx r12d, byte ptr [r9  + rax]
    mov  rax, rcx
    shr  rax, 40
    and  rax, 0FFh
    movzx r13d, byte ptr [r10 + rax]
    mov  rax, rcx
    shr  rax, 32
    and  rax, 0FFh
    movzx r14d, byte ptr [r11 + rax]
    mov  rax, rcx
    shr  rax, 24
    and  rax, 0FFh
    movzx r15d, byte ptr [r9  + rax]
    mov  rax, rcx
    shr  rax, 16
    and  rax, 0FFh
    movzx esi,  byte ptr [r10 + rax]
    mov  rax, rcx
    shr  rax, 8
    and  rax, 0FFh
    movzx edx,  byte ptr [r11 + rax]
    mov  rax, rcx
    and  rax, 0FFh
    movzx ecx,  byte ptr [r8  + rax]
    mov  eax, ebx
    xor  eax, r13d
    xor  eax, r14d
    xor  eax, esi
    xor  eax, edx
    xor  eax, ecx
    and  eax, 0FFh
    shl  rax, 56
    mov  r11, rax
    mov  eax, ebx
    xor  eax, r12d
    xor  eax, r14d
    xor  eax, r15d
    xor  eax, edx
    xor  eax, ecx
    and  eax, 0FFh
    shl  rax, 48
    or   r11, rax
    mov  eax, ebx
    xor  eax, r12d
    xor  eax, r13d
    xor  eax, r15d
    xor  eax, esi
    xor  eax, ecx
    and  eax, 0FFh
    shl  rax, 40
    or   r11, rax
    mov  eax, r12d
    xor  eax, r13d
    xor  eax, r14d
    xor  eax, r15d
    xor  eax, esi
    xor  eax, edx
    and  eax, 0FFh
    shl  rax, 32
    or   r11, rax
    mov  eax, ebx
    xor  eax, r12d
    xor  eax, esi
    xor  eax, edx
    xor  eax, ecx
    and  eax, 0FFh
    shl  rax, 24
    or   r11, rax
    mov  eax, r12d
    xor  eax, r13d
    xor  eax, r15d
    xor  eax, edx
    xor  eax, ecx
    and  eax, 0FFh
    shl  rax, 16
    or   r11, rax
    mov  eax, r13d
    xor  eax, r14d
    xor  eax, r15d
    xor  eax, esi
    xor  eax, ecx
    and  eax, 0FFh
    shl  rax, 8
    or   r11, rax
    mov  eax, ebx
    xor  eax, r14d
    xor  eax, r15d
    xor  eax, esi
    xor  eax, edx
    and  eax, 0FFh
    or   r11, rax
    mov  rax, r11
    add  rsp, 40
    pop  rsi
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
CamF endp
CamFL proc
    mov  r8,  rcx
    shr  r8,  32
    mov  r8d, r8d
    mov  r9d, ecx
    mov  r10, rdx
    shr  r10, 32
    mov  r10d, r10d
    mov  r11d, edx
    mov  eax, r8d
    and  eax, r10d
    rol  eax, 1
    xor  r9d, eax
    mov  eax, r9d
    or   eax, r11d
    xor  r8d, eax
    xor  rax, rax
    mov  eax, r9d
    mov  ecx, r8d
    shl  rcx, 32
    or   rax, rcx
    ret
CamFL endp

CamFLINV proc
    mov  r8,  rcx
    shr  r8,  32
    mov  r8d, r8d
    mov  r9d, ecx
    mov  r10, rdx
    shr  r10, 32
    mov  r10d, r10d
    mov  r11d, edx
    mov  eax, r9d
    or   eax, r11d
    xor  r8d, eax
    mov  eax, r8d
    and  eax, r10d
    rol  eax, 1
    xor  r9d, eax
    xor  rax, rax
    mov  eax, r9d
    mov  ecx, r8d
    shl  rcx, 32
    or   rax, rcx
    ret
CamFLINV endp

Rot128 proc
    cmp  ecx, 64
    jb   @r128s
    xchg r8, r9
    sub  ecx, 64
@r128s:
    test ecx, ecx
    jz   @r128z
    mov  r10, r8
    shl  r10, cl
    mov  rax, r9
    push rcx
    neg  ecx
    add  ecx, 64
    shr  rax, cl
    pop  rcx
    or   r10, rax
    mov  r11, r9
    shl  r11, cl
    push rcx
    neg  ecx
    add  ecx, 64
    mov  rax, r8
    shr  rax, cl
    pop  rcx
    or   r11, rax
    ret
@r128z:
    mov  r10, r8
    mov  r11, r9
    ret
Rot128 endp
CamKeySchedule proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rsi
    push rdi
    sub  rsp, 96
    lea  rsi, [camKey]
    mov  r12, [rsi]
    bswap r12
    mov  r13, [rsi+8]
    bswap r13
    mov  r14, [rsi+16]
    bswap r14
    mov  r15, [rsi+24]
    bswap r15
    mov  rdi, r12
    xor  rdi, r14
    mov  rbx, r13
    xor  rbx, r15
    mov  rcx, rdi
    mov  rdx, [Sigma1]
    call CamF
    xor  rbx, rax
    mov  rcx, rbx
    mov  rdx, [Sigma2]
    call CamF
    xor  rdi, rax
    xor  rdi, r12
    xor  rbx, r13
    mov  rcx, rdi
    mov  rdx, [Sigma3]
    call CamF
    xor  rbx, rax
    mov  rcx, rbx
    mov  rdx, [Sigma4]
    call CamF
    xor  rdi, rax
    mov  [rsp+32], rdi
    mov  [rsp+40], rbx
    mov  rdi, [rsp+32]
    xor  rdi, r14
    mov  rbx, [rsp+40]
    xor  rbx, r15
    mov  rcx, rdi
    mov  rdx, [Sigma5]
    call CamF
    xor  rbx, rax
    mov  rcx, rbx
    mov  rdx, [Sigma6]
    call CamF
    xor  rdi, rax
    mov  [rsp+48], rdi
    mov  [rsp+56], rbx
    lea  rsi, [camEK]
    mov  [rsi+ 0*8], r12
    mov  [rsi+ 1*8], r13
    mov  rax, [rsp+48]
    mov  [rsi+ 2*8], rax
    mov  rax, [rsp+56]
    mov  [rsi+ 3*8], rax
    mov  r8, r14
    mov  r9, r15
    mov  ecx, 15
    call Rot128
    mov  [rsi+ 4*8], r10
    mov  [rsi+ 5*8], r11
    mov  r8, [rsp+32]
    mov  r9, [rsp+40]
    mov  ecx, 15
    call Rot128
    mov  [rsi+ 6*8], r10
    mov  [rsi+ 7*8], r11
    mov  r8, r14
    mov  r9, r15
    mov  ecx, 30
    call Rot128
    mov  [rsi+ 8*8], r10
    mov  [rsi+ 9*8], r11
    mov  r8, [rsp+48]
    mov  r9, [rsp+56]
    mov  ecx, 30
    call Rot128
    mov  [rsi+10*8], r10
    mov  [rsi+11*8], r11
    mov  r8, r12
    mov  r9, r13
    mov  ecx, 45
    call Rot128
    mov  [rsi+12*8], r10
    mov  [rsi+13*8], r11
    mov  r8, [rsp+32]
    mov  r9, [rsp+40]
    mov  ecx, 45
    call Rot128
    mov  [rsi+14*8], r10
    mov  [rsi+15*8], r11
    mov  r8, r12
    mov  r9, r13
    mov  ecx, 60
    call Rot128
    mov  [rsi+16*8], r10
    mov  [rsi+17*8], r11
    mov  r8, r14
    mov  r9, r15
    mov  ecx, 60
    call Rot128
    mov  [rsi+18*8], r10
    mov  [rsi+19*8], r11
    mov  r8, [rsp+48]
    mov  r9, [rsp+56]
    mov  ecx, 60
    call Rot128
    mov  [rsi+20*8], r10
    mov  [rsi+21*8], r11
    mov  r8, r12
    mov  r9, r13
    mov  ecx, 77
    call Rot128
    mov  [rsi+22*8], r10
    mov  [rsi+23*8], r11
    mov  r8, [rsp+32]
    mov  r9, [rsp+40]
    mov  ecx, 77
    call Rot128
    mov  [rsi+24*8], r10
    mov  [rsi+25*8], r11
    mov  r8, r14
    mov  r9, r15
    mov  ecx, 94
    call Rot128
    mov  [rsi+26*8], r10
    mov  [rsi+27*8], r11
    mov  r8, [rsp+32]
    mov  r9, [rsp+40]
    mov  ecx, 94
    call Rot128
    mov  [rsi+28*8], r10
    mov  [rsi+29*8], r11
    mov  r8, r12
    mov  r9, r13
    mov  ecx, 111
    call Rot128
    mov  [rsi+30*8], r10
    mov  [rsi+31*8], r11
    mov  r8, [rsp+48]
    mov  r9, [rsp+56]
    mov  ecx, 111
    call Rot128
    mov  [rsi+32*8], r10
    mov  [rsi+33*8], r11
    lea  rdi, [camDK]
    lea  rsi, [camEK + 33*8]
    mov  ecx, 34
@dkr:
    mov  rax, [rsi]
    mov  [rdi], rax
    add  rdi, 8
    sub  rsi, 8
    dec  ecx
    jnz  @dkr
    add  rsp, 96
    pop  rdi
    pop  rsi
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
CamKeySchedule endp
CamEncryptBlock proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rsi
    push rdi
    sub  rsp, 56
    mov  r14, rdx
    mov  rax, [rcx]
    bswap rax
    mov  r12, rax
    mov  rax, [rcx+8]
    bswap rax
    mov  r13, rax
    lea  rsi, [camEK]
    xor  r12, [rsi+ 0*8]
    xor  r13, [rsi+ 1*8]
    mov  rcx, r12
    mov  rdx, [rsi+ 2*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+ 3*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+ 4*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+ 5*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+ 6*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+ 7*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+ 8*8]
    call CamFL
    mov  r12, rax
    mov  rcx, r13
    mov  rdx, [rsi+ 9*8]
    call CamFLINV
    mov  r13, rax
    mov  rcx, r12
    mov  rdx, [rsi+10*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+11*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+12*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+13*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+14*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+15*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+16*8]
    call CamFL
    mov  r12, rax
    mov  rcx, r13
    mov  rdx, [rsi+17*8]
    call CamFLINV
    mov  r13, rax
    mov  rcx, r12
    mov  rdx, [rsi+18*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+19*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+20*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+21*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+22*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+23*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+24*8]
    call CamFL
    mov  r12, rax
    mov  rcx, r13
    mov  rdx, [rsi+25*8]
    call CamFLINV
    mov  r13, rax
    mov  rcx, r12
    mov  rdx, [rsi+26*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+27*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+28*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+29*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+30*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+31*8]
    call CamF
    xor  r12, rax
    xor  r13, [rsi+32*8]
    xor  r12, [rsi+33*8]
    bswap r13
    mov  [r14],   r13
    bswap r12
    mov  [r14+8], r12
    add  rsp, 56
    pop  rdi
    pop  rsi
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
CamEncryptBlock endp
CamDecryptBlock proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rsi
    push rdi
    sub  rsp, 56
    mov  r14, rdx
    mov  rax, [rcx]
    bswap rax
    mov  r13, rax
    mov  rax, [rcx+8]
    bswap rax
    mov  r12, rax
    lea  rsi, [camDK]
    xor  r12, [rsi+ 0*8]
    xor  r13, [rsi+ 1*8]
    mov  rcx, r12
    mov  rdx, [rsi+ 2*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+ 3*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+ 4*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+ 5*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+ 6*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+ 7*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+ 8*8]
    call CamFLINV
    mov  r12, rax
    mov  rcx, r13
    mov  rdx, [rsi+ 9*8]
    call CamFL
    mov  r13, rax
    mov  rcx, r12
    mov  rdx, [rsi+10*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+11*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+12*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+13*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+14*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+15*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+16*8]
    call CamFLINV
    mov  r12, rax
    mov  rcx, r13
    mov  rdx, [rsi+17*8]
    call CamFL
    mov  r13, rax
    mov  rcx, r12
    mov  rdx, [rsi+18*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+19*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+20*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+21*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+22*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+23*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+24*8]
    call CamFLINV
    mov  r12, rax
    mov  rcx, r13
    mov  rdx, [rsi+25*8]
    call CamFL
    mov  r13, rax
    mov  rcx, r12
    mov  rdx, [rsi+26*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+27*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+28*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+29*8]
    call CamF
    xor  r12, rax
    mov  rcx, r12
    mov  rdx, [rsi+30*8]
    call CamF
    xor  r13, rax
    mov  rcx, r13
    mov  rdx, [rsi+31*8]
    call CamF
    xor  r12, rax
    xor  r13, [rsi+32*8]
    xor  r12, [rsi+33*8]
    bswap r12
    mov  [r14],   r12
    bswap r13
    mov  [r14+8], r13
    add  rsp, 56
    pop  rdi
    pop  rsi
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
CamDecryptBlock endp
GF128Mul proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rsi
    push rdi
    sub  rsp, 48
    mov  r12, [rcx]
    mov  r13, [rcx+8]
    mov  r14, [rdx]
    mov  r15, [rdx+8]
    xor  rsi, rsi
    xor  rdi, rdi
    mov  ebx, 128
@gfl:
    test r14, r14
    jns  @gnx
    xor  rsi, r12
    xor  rdi, r13
@gnx:
    add  r15, r15
    adc  r14, r14
    mov  rax, r13
    and  rax, 1
    shr  r12, 1
    rcr  r13, 1
    test rax, rax
    jz   @gnr
    mov  rax, 0E100000000000000h
    xor  r12, rax
@gnr:
    dec  ebx
    jnz  @gfl
    mov  [r8],   rsi
    mov  [r8+8], rdi
    add  rsp, 48
    pop  rdi
    pop  rsi
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
GF128Mul endp

GcmInit proc
    push rbx
    push rsi
    sub  rsp, 48
    mov  rbx, rcx
    xor  rax, rax
    lea  rsi, [gcmH]
    mov  [rsi],   rax
    mov  [rsi+8], rax
    mov  rcx, rsi
    mov  rdx, rsi
    call CamEncryptBlock
    lea  rsi, [gcmJ0]
    mov  rax, [rbx]
    mov  [rsi], rax
    mov  eax, dword ptr [rbx+8]
    mov  dword ptr [rsi+8], eax
    mov  dword ptr [rsi+12], 01000000h
    mov  rax, [rsi]
    lea  rsi, [gcmCtr]
    mov  [rsi], rax
    lea  rdi, [gcmJ0]
    mov  rax, [rdi+8]
    mov  [rsi+8], rax
    add  rsp, 48
    pop  rsi
    pop  rbx
    ret
GcmInit endp

GcmCtrInc proc
    mov  eax, dword ptr [gcmCtr+12]
    bswap eax
    inc  eax
    bswap eax
    mov  dword ptr [gcmCtr+12], eax
    ret
GcmCtrInc endp
GcmEncryptBytes proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rsi
    push rdi
    sub  rsp, 128
    mov  r12, rcx
    mov  r13d, edx
    mov  r14, r8
    xor  rax, rax
    mov  [rsp+48], rax
    mov  [rsp+56], rax
    xor  r15d, r15d
@gcmEL:
    cmp  r15d, r13d
    jge  @gcmED
    call GcmCtrInc
    lea  rcx, [gcmCtr]
    lea  rdx, [rsp+32]
    call CamEncryptBlock
    mov  ebx, r13d
    sub  ebx, r15d
    cmp  ebx, 16
    jle  @gcmEsz
    mov  ebx, 16
@gcmEsz:
    xor  edi, edi
@gcmExr:
    cmp  edi, ebx
    jge  @gcmExd
    movsxd rax, edi
    mov  rbp, r12
    add  rbp, r15
    movzx ecx, byte ptr [rbp + rax]
    xor  cl, byte ptr [rsp+32 + rax]
    mov  rbp, r14
    add  rbp, r15
    mov  byte ptr [rbp + rax], cl
    inc  edi
    jmp  @gcmExr
@gcmExd:
    xor  rax, rax
    mov  [rsp+64], rax
    mov  [rsp+72], rax
    xor  edi, edi
@gcmEcp:
    cmp  edi, ebx
    jge  @gcmEgh
    movsxd rax, edi
    mov  rbp, r14
    add  rbp, r15
    movzx ecx, byte ptr [rbp + rax]
    mov  byte ptr [rsp+64 + rax], cl
    inc  edi
    jmp  @gcmEcp
@gcmEgh:
    mov  rax, [rsp+48]
    xor  rax, [rsp+64]
    mov  [rsp+48], rax
    mov  rax, [rsp+56]
    xor  rax, [rsp+72]
    mov  [rsp+56], rax
    lea  rcx, [rsp+48]
    lea  rdx, [gcmH]
    lea  r8,  [rsp+48]
    call GF128Mul
    add  r15d, ebx
    jmp  @gcmEL
@gcmED:
    xor  rax, rax
    mov  [rsp+64], rax
    mov  rax, r13
    shl  rax, 3
    bswap rax
    mov  [rsp+72], rax
    mov  rax, [rsp+48]
    xor  rax, [rsp+64]
    mov  [rsp+48], rax
    mov  rax, [rsp+56]
    xor  rax, [rsp+72]
    mov  [rsp+56], rax
    lea  rcx, [rsp+48]
    lea  rdx, [gcmH]
    lea  r8,  [rsp+48]
    call GF128Mul
    lea  rcx, [gcmJ0]
    lea  rdx, [gcmTag]
    call CamEncryptBlock
    mov  rax, [rsp+48]
    xor  qword ptr [gcmTag],   rax
    mov  rax, [rsp+56]
    xor  qword ptr [gcmTag+8], rax
    add  rsp, 128
    pop  rdi
    pop  rsi
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
GcmEncryptBytes endp
GcmDecryptBytes proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rsi
    push rdi
    sub  rsp, 128
    mov  r12, rcx
    mov  r13d, edx
    mov  r14, r8
    mov  r15, r9
    xor  rax, rax
    mov  [rsp+48], rax
    mov  [rsp+56], rax
    xor  ebx, ebx
@gcmDL:
    cmp  ebx, r13d
    jge  @gcmDD
    mov  esi, r13d
    sub  esi, ebx
    cmp  esi, 16
    jle  @gcmDsz
    mov  esi, 16
@gcmDsz:
    xor  rax, rax
    mov  [rsp+64], rax
    mov  [rsp+72], rax
    xor  edi, edi
@gcmDcp:
    cmp  edi, esi
    jge  @gcmDgh
    movsxd rax, edi
    mov  rbp, r12
    add  rbp, rbx
    movzx ecx, byte ptr [rbp + rax]
    mov  byte ptr [rsp+64 + rax], cl
    inc  edi
    jmp  @gcmDcp
@gcmDgh:
    mov  rax, [rsp+48]
    xor  rax, [rsp+64]
    mov  [rsp+48], rax
    mov  rax, [rsp+56]
    xor  rax, [rsp+72]
    mov  [rsp+56], rax
    lea  rcx, [rsp+48]
    lea  rdx, [gcmH]
    lea  r8,  [rsp+48]
    call GF128Mul
    call GcmCtrInc
    lea  rcx, [gcmCtr]
    lea  rdx, [rsp+32]
    call CamEncryptBlock
    xor  edi, edi
@gcmDxr:
    cmp  edi, esi
    jge  @gcmDxd
    movsxd rax, edi
    mov  rbp, r12
    add  rbp, rbx
    movzx ecx, byte ptr [rbp + rax]
    xor  cl, byte ptr [rsp+32 + rax]
    mov  rbp, r14
    add  rbp, rbx
    mov  byte ptr [rbp + rax], cl
    inc  edi
    jmp  @gcmDxr
@gcmDxd:
    add  ebx, esi
    jmp  @gcmDL
@gcmDD:
    xor  rax, rax
    mov  [rsp+64], rax
    mov  rax, r13
    shl  rax, 3
    bswap rax
    mov  [rsp+72], rax
    mov  rax, [rsp+48]
    xor  rax, [rsp+64]
    mov  [rsp+48], rax
    mov  rax, [rsp+56]
    xor  rax, [rsp+72]
    mov  [rsp+56], rax
    lea  rcx, [rsp+48]
    lea  rdx, [gcmH]
    lea  r8,  [rsp+48]
    call GF128Mul
    lea  rcx, [gcmJ0]
    lea  rdx, [gcmTag]
    call CamEncryptBlock
    mov  rax, [rsp+48]
    xor  qword ptr [gcmTag],   rax
    mov  rax, [rsp+56]
    xor  qword ptr [gcmTag+8], rax
    mov  rax, qword ptr [gcmTag]
    xor  rax, qword ptr [r15]
    mov  rcx, qword ptr [gcmTag+8]
    xor  rcx, qword ptr [r15+8]
    or   rax, rcx
    mov  eax, 0
    jz   @gcmDok
    mov  eax, 1
@gcmDok:
    add  rsp, 128
    pop  rdi
    pop  rsi
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
GcmDecryptBytes endp
StrLen proc
    xor  eax, eax
@sl:
    cmp  byte ptr [rcx+rax], 0
    je   @sld
    inc  eax
    jmp  @sl
@sld:
    ret
StrLen endp

ConWrite proc
    push rbx
    push rsi
    sub  rsp, 40
    mov  rsi, rcx
    call StrLen
    mov  r8d, eax
    mov  rcx, [hStdOut]
    mov  rdx, rsi
    lea  r9,  [dwBytesRW]
    mov  qword ptr [rsp+20h], 0
    call WriteConsoleA
    add  rsp, 40
    pop  rsi
    pop  rbx
    ret
ConWrite endp

ConWriteNum proc
    push rbx
    sub  rsp, 32
    lea  rbx, [numBuf+19]
    mov  byte ptr [rbx], 0
    dec  rbx
    mov  eax, ecx
    test eax, eax
    jnz  @cnv
    mov  byte ptr [rbx], '0'
    jmp  @cnp
@cnv:
    mov  ecx, 10
@cnd:
    xor  edx, edx
    div  ecx
    add  dl, '0'
    mov  byte ptr [rbx], dl
    dec  rbx
    test eax, eax
    jnz  @cnd
    inc  rbx
@cnp:
    mov  rcx, rbx
    call ConWrite
    add  rsp, 32
    pop  rbx
    ret
ConWriteNum endp

ConWriteHex32 proc
    push rbx
    push rdi
    sub  rsp, 40
    lea  rbx, [hexBuf]
    mov  byte ptr [rbx+8], 0
    mov  eax, ecx
    lea  rdi, [rbx+7]
    mov  ecx, 8
@hxl:
    mov  edx, eax
    and  edx, 0Fh
    cmp  dl, 9
    jbe  @hxd
    add  dl, 7
@hxd:
    add  dl, '0'
    mov  [rdi], dl
    shr  eax, 4
    dec  rdi
    dec  ecx
    jnz  @hxl
    lea  rcx, [hexBuf]
    call ConWrite
    add  rsp, 40
    pop  rdi
    pop  rbx
    ret
ConWriteHex32 endp

FatalExit proc
    push rbx
    sub  rsp, 32
    mov  rbx, rcx
    lea  rcx, [szFail]
    call ConWrite
    mov  rcx, rbx
    call ConWrite
    mov  ecx, 1
    call ExitProcess
    add  rsp, 32
    pop  rbx
    ret
FatalExit endp

StrICmp7 proc
    push rsi
    push rdi
    mov  rsi, rcx
    mov  rdi, rdx
    xor  ecx, ecx
@sic:
    movzx eax, byte ptr [rsi+rcx]
    movzx edx, byte ptr [rdi+rcx]
    test al, al
    jz   @sicZ
    test dl, dl
    jz   @sicn
    or   al, 20h
    or   dl, 20h
    cmp  al, dl
    jne  @sicn
    inc  ecx
    jmp  @sic
@sicZ:
    test dl, dl
    jnz  @sicn
@sice:
    xor  eax, eax
    pop  rdi
    pop  rsi
    ret
@sicn:
    mov  eax, 1
    pop  rdi
    pop  rsi
    ret
StrICmp7 endp
ParseArgs proc
    push rbx
    push rsi
    push rdi
    push r12
    sub  rsp, 40
    call GetCommandLineA
    mov  rsi, rax
    xor  r12d, r12d
@pas:
    movzx eax, byte ptr [rsi]
    cmp  al, ' '
    je   @pai
    cmp  al, 9
    je   @pai
    jmp  @pat
@pai:
    inc  rsi
    jmp  @pas
@pat:
    cmp  byte ptr [rsi], 0
    je   @pad
    cmp  byte ptr [rsi], '"'
    jne  @pau
    inc  rsi
    mov  rdi, rsi
@paqL:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   @paS
    cmp  al, '"'
    je   @paqE
    inc  rsi
    jmp  @paqL
@paqE:
    mov  byte ptr [rsi], 0
    inc  rsi
    jmp  @paS
@pau:
    mov  rdi, rsi
@pauL:
    movzx eax, byte ptr [rsi]
    test al, al
    jz   @paS
    cmp  al, ' '
    je   @pauE
    cmp  al, 9
    je   @pauE
    inc  rsi
    jmp  @pauL
@pauE:
    mov  byte ptr [rsi], 0
    inc  rsi
@paS:
    cmp  r12d, 1
    je   @pam
    cmp  r12d, 2
    je   @pain
    cmp  r12d, 3
    je   @paou
    cmp  r12d, 4
    je   @pap
    jmp  @pan
@pam:
    mov  [pArgMode], rdi
    jmp  @pan
@pain:
    mov  [pArgIn], rdi
    jmp  @pan
@paou:
    mov  [pArgOut], rdi
    jmp  @pan
@pap:
    mov  [pArgPass], rdi
@pan:
    inc  r12d
    jmp  @pas
@pad:
    ; Check for pack mode first (needs different arg count)
    cmp  r12d, 2
    jb   @paerr
    mov  rcx, [pArgMode]
    lea  rdx, [szPackWord]
    call StrICmp7
    test eax, eax
    jz   @packMode
    ; encrypt/decrypt need >=4 args
    cmp  r12d, 4
    jb   @paerr
    mov  rcx, [pArgMode]
    lea  rdx, [szDecWord]
    call StrICmp7
    test eax, eax
    jnz  @pand
    mov  byte ptr [bDecrypt], 1
@pand:
    cmp  qword ptr [pArgPass], 0
    jne  @pahp
    lea  rax, [szDefPass]
    mov  [pArgPass], rax
    mov  dword ptr [dwPassLen], DEF_PASS_LEN
    jmp  @paok
@pahp:
    mov  rcx, [pArgPass]
    call StrLen
    mov  [dwPassLen], eax
@paok:
    add  rsp, 40
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    ret
@paerr:
    lea  rcx, [szBanner]
    call ConWrite
    lea  rcx, [szUsage]
    call ConWrite
    mov  ecx, 1
    call ExitProcess
    add  rsp, 40
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    ret
@packMode:
    ; pack mode: args are [1]=mode [2]=stub [3]=car [4]=output
    cmp  r12d, 5
    jb   @paerr
    mov  byte ptr [bPack], 1
    ; pArgIn = stub path, pArgOut reused as car path
    ; pArgPass reused as output path (no passphrase needed)
    mov  rax, [pArgIn]
    mov  [pArgStub], rax       ; stub .bin path
    mov  rax, [pArgOut]
    mov  [pArgIn], rax         ; container .car path  
    mov  rax, [pArgPass]
    mov  [pArgOut], rax        ; output packed file
    jmp  @paok
ParseArgs endp

DeriveKey proc
    sub  rsp, 88
    mov  rcx, [hAlgHMAC]
    mov  rdx, [pArgPass]
    mov  r8d, [dwPassLen]
    lea  r9,  [bSalt]
    mov  dword ptr [rsp+20h], SALT_LEN
    mov  qword ptr [rsp+28h], PBKDF2_ROUNDS
    lea  rax, [camKey]
    mov  [rsp+30h], rax
    mov  dword ptr [rsp+38h], CAM_KEY_LEN
    mov  dword ptr [rsp+40h], 0
    call BCryptDeriveKeyPBKDF2
    test eax, eax
    jnz  @dkf
    add  rsp, 88
    ret
@dkf:
    push rax
    lea  rcx, [szErrCryp]
    call ConWrite
    pop  rcx
    call ConWriteHex32
    lea  rcx, [szCRLF]
    call ConWrite
    mov  ecx, 1
    call ExitProcess
    add  rsp, 88
    ret
DeriveKey endp
ReadInputFile proc
    push rbx
    sub  rsp, 64
    mov  rcx, [pArgIn]
    mov  edx, GENERIC_READ
    mov  r8d, FILE_SHARE_READ
    xor  r9d, r9d
    mov  dword ptr [rsp+20h], OPEN_EXISTING
    mov  dword ptr [rsp+28h], FILE_ATTR_NORM
    mov  qword ptr [rsp+30h], 0
    call CreateFileA
    cmp  rax, INVALID_HANDLE
    je   @rifF
    mov  [hFileIn], rax
    mov  rcx, rax
    xor  edx, edx
    call GetFileSize
    mov  [dwFileSize], eax
    mov  ebx, eax
    xor  ecx, ecx
    lea  edx, [ebx+4096]
    mov  r8d, MEM_COMMIT or MEM_RESERVE
    mov  r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz   @rifM
    mov  [pFileBuf], rax
    mov  rcx, [hFileIn]
    mov  rdx, rax
    mov  r8d, ebx
    lea  r9,  [dwBytesRW]
    mov  qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz   @rifF
    mov  rcx, [hFileIn]
    call CloseHandle
    add  rsp, 64
    pop  rbx
    ret
@rifF:
    lea  rcx, [szErrOpen]
    call FatalExit
@rifM:
    lea  rcx, [szErrMem]
    call FatalExit
    add  rsp, 64
    pop  rbx
    ret
ReadInputFile endp

WriteOutputFile proc
    push rbx
    push r12
    sub  rsp, 56
    mov  r12d, ecx
    mov  rcx, [pArgOut]
    mov  edx, GENERIC_WRITE
    xor  r8d, r8d
    xor  r9d, r9d
    mov  dword ptr [rsp+20h], CREATE_ALWAYS
    mov  dword ptr [rsp+28h], FILE_ATTR_NORM
    mov  qword ptr [rsp+30h], 0
    call CreateFileA
    cmp  rax, INVALID_HANDLE
    je   @wofF
    mov  rbx, rax
    mov  rcx, rbx
    mov  rdx, [pOutBuf]
    mov  r8d, r12d
    lea  r9,  [dwBytesRW]
    mov  qword ptr [rsp+20h], 0
    call WriteFile
    test eax, eax
    jz   @wofF
    mov  rcx, rbx
    call CloseHandle
    add  rsp, 56
    pop  r12
    pop  rbx
    ret
@wofF:
    lea  rcx, [szErrWrt]
    call FatalExit
    add  rsp, 56
    pop  r12
    pop  rbx
    ret
WriteOutputFile endp

FreeBuffers proc
    sub  rsp, 40
    push rdi
    mov  rcx, [pFileBuf]
    test rcx, rcx
    jz   @fb1
    xor  edx, edx
    mov  r8d, MEM_RELEASE
    call VirtualFree
    mov  qword ptr [pFileBuf], 0
@fb1:
    mov  rcx, [pOutBuf]
    test rcx, rcx
    jz   @fb2
    xor  edx, edx
    mov  r8d, MEM_RELEASE
    call VirtualFree
    mov  qword ptr [pOutBuf], 0
@fb2:
    xor  eax, eax
    lea  rdi, [camKey]
    mov  ecx, CAM_KEY_LEN
    rep  stosb
    lea  rdi, [bSalt]
    mov  ecx, SALT_LEN
    rep  stosb
    lea  rdi, [bIV]
    mov  ecx, GCM_IV_LEN
    rep  stosb
    lea  rdi, [camEK]
    mov  ecx, 34*8
    rep  stosb
    lea  rdi, [camDK]
    mov  ecx, 34*8
    rep  stosb
    pop  rdi
    add  rsp, 40
    ret
FreeBuffers endp

; ─── DoPack — prepend polymorphic stub to encrypted container ─────────────────
; Reads a CSTB stub file and a .car container, outputs [stub code][container].
; The stub's CSTB header (12 bytes: magic+flags+size) is stripped — only the
; raw PIC code is prepended.  The result is a self-decrypting executable blob.
;
; Layout of output: [stub PIC shellcode (variable)][CARMILLA .car container]
DoPack proc
    push rbx
    push r12
    push r13
    push r14
    push rsi
    push rdi
    sub  rsp, 72                ; 8+8*6+72=128, 128%16=0 ✓

    lea  rcx, [szPack]
    call ConWrite

    ; ── Read stub file ──
    mov  rcx, [pArgStub]
    mov  edx, GENERIC_READ
    mov  r8d, FILE_SHARE_READ
    xor  r9d, r9d
    mov  dword ptr [rsp+20h], OPEN_EXISTING
    mov  dword ptr [rsp+28h], FILE_ATTR_NORM
    mov  qword ptr [rsp+30h], 0
    call CreateFileA
    cmp  rax, INVALID_HANDLE
    je   @pkErrStub
    mov  r14, rax               ; stub file handle

    mov  rcx, r14
    xor  edx, edx
    call GetFileSize
    mov  r12d, eax              ; stub file size
    cmp  r12d, 12
    jb   @pkBadStub             ; too small for CSTB header

    ; alloc buf for stub file
    xor  ecx, ecx
    mov  edx, r12d
    add  edx, 4096
    mov  r8d, MEM_COMMIT or MEM_RESERVE
    mov  r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz   @pkMem
    mov  rsi, rax               ; rsi = stub buf

    mov  rcx, r14
    mov  rdx, rsi
    mov  r8d, r12d
    lea  r9,  [dwBytesRW]
    mov  qword ptr [rsp+20h], 0
    call ReadFile

    mov  rcx, r14
    call CloseHandle

    ; validate CSTB magic
    mov  eax, dword ptr [rsi]
    cmp  eax, 42545343h         ; "CSTB" LE
    jne  @pkBadStub

    ; extract stub code size from header [8..11]
    mov  ebx, dword ptr [rsi+8] ; stub code size
    test ebx, ebx
    jz   @pkBadStub

    ; stub code starts at offset 12
    lea  r13, [rsi+12]          ; r13 = ptr to stub code

    ; ── Read container file ──
    mov  rcx, [pArgIn]          ; .car path (was repointed in ParseArgs)
    mov  edx, GENERIC_READ
    mov  r8d, FILE_SHARE_READ
    xor  r9d, r9d
    mov  dword ptr [rsp+20h], OPEN_EXISTING
    mov  dword ptr [rsp+28h], FILE_ATTR_NORM
    mov  qword ptr [rsp+30h], 0
    call CreateFileA
    cmp  rax, INVALID_HANDLE
    je   @pkErrCar
    mov  r14, rax               ; car file handle

    mov  rcx, r14
    xor  edx, edx
    call GetFileSize
    mov  r12d, eax              ; car file size

    ; alloc buf for car
    xor  ecx, ecx
    mov  edx, r12d
    add  edx, 4096
    mov  r8d, MEM_COMMIT or MEM_RESERVE
    mov  r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz   @pkMem
    mov  [pFileBuf], rax        ; car buf

    mov  rcx, r14
    mov  rdx, [pFileBuf]
    mov  r8d, r12d
    lea  r9,  [dwBytesRW]
    mov  qword ptr [rsp+20h], 0
    call ReadFile

    mov  rcx, r14
    call CloseHandle

    ; ── Build output: [stub code][car container] ──
    mov  eax, ebx
    add  eax, r12d              ; total output size
    mov  r14d, eax              ; r14d = total size

    xor  ecx, ecx
    mov  edx, r14d
    add  edx, 4096
    mov  r8d, MEM_COMMIT or MEM_RESERVE
    mov  r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz   @pkMem
    mov  [pOutBuf], rax
    mov  rdi, rax

    ; copy stub code
    mov  rcx, rdi               ; dst
    mov  rdx, r13               ; src = stub code (past CSTB header)
    mov  eax, ebx               ; count
    push rdi
    push rsi
    mov  rdi, rcx
    mov  rsi, rdx
    mov  ecx, eax
    rep  movsb
    pop  rsi
    pop  rdi

    ; copy car container after stub
    push rdi
    push rsi
    mov  rdi, [pOutBuf]
    add  rdi, rbx               ; skip past stub code
    mov  rsi, [pFileBuf]
    mov  ecx, r12d
    rep  movsb
    pop  rsi
    pop  rdi

    ; ── Write output file ──
    mov  rcx, [pArgOut]
    mov  edx, GENERIC_WRITE
    xor  r8d, r8d
    xor  r9d, r9d
    mov  dword ptr [rsp+20h], CREATE_ALWAYS
    mov  dword ptr [rsp+28h], FILE_ATTR_NORM
    mov  qword ptr [rsp+30h], 0
    call CreateFileA
    cmp  rax, INVALID_HANDLE
    je   @pkErrWrt
    mov  r14, rax

    mov  eax, ebx
    add  eax, r12d              ; total size
    mov  rcx, r14
    mov  rdx, [pOutBuf]
    mov  r8d, eax
    lea  r9,  [dwBytesRW]
    mov  qword ptr [rsp+20h], 0
    call WriteFile

    mov  rcx, r14
    call CloseHandle

    ; ── Print summary ──
    lea  rcx, [szPackOk]
    call ConWrite
    mov  rcx, [pArgOut]
    call ConWrite

    ; total size
    lea  rcx, [szCRLF]
    call ConWrite
    lea  rcx, [szDone]
    call ConWrite
    mov  eax, ebx
    add  eax, r12d
    mov  ecx, eax
    call ConWriteNum
    lea  rcx, [szPackSz]
    call ConWrite
    mov  ecx, ebx
    call ConWriteNum
    lea  rcx, [szPackSz2]
    call ConWrite
    mov  ecx, r12d
    call ConWriteNum
    lea  rcx, [szPackSz3]
    call ConWrite

    ; free stub buf (rsi)
    mov  rcx, rsi
    xor  edx, edx
    mov  r8d, MEM_RELEASE
    call VirtualFree

    add  rsp, 72
    pop  rdi
    pop  rsi
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret

@pkErrStub:
    lea  rcx, [szErrOpen]
    call FatalExit
@pkErrCar:
    lea  rcx, [szErrOpen]
    call FatalExit
@pkErrWrt:
    lea  rcx, [szErrWrt]
    call FatalExit
@pkBadStub:
    mov  rcx, r14
    call CloseHandle
    lea  rcx, [szPackBad]
    call FatalExit
@pkMem:
    lea  rcx, [szErrMem]
    call FatalExit
    add  rsp, 72
    pop  rdi
    pop  rsi
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    ret
DoPack endp

DoEncrypt proc
    push rbx
    push r12
    push r13
    push rsi
    push rdi
    sub  rsp, 80
    lea  rcx, [szEnc]
    call ConWrite
    mov  ebx, [dwFileSize]
    xor  ecx, ecx
    lea  rdx, [bSalt]
    mov  r8d, SALT_LEN
    mov  r9d, BCRYPT_SYS_RNG
    call BCryptGenRandom
    test eax, eax
    jnz  @encCF
    xor  ecx, ecx
    lea  rdx, [bIV]
    mov  r8d, GCM_IV_LEN
    mov  r9d, BCRYPT_SYS_RNG
    call BCryptGenRandom
    test eax, eax
    jnz  @encCF
    call DeriveKey
    call CamKeySchedule
    lea  rcx, [bIV]
    call GcmInit
    mov  r12d, ebx
    add  r12d, HDR_OVERHEAD
    xor  ecx, ecx
    lea  edx, [r12d+4096]
    mov  r8d, MEM_COMMIT or MEM_RESERVE
    mov  r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz   @encMF
    mov  [pOutBuf], rax
    mov  r13, rax
    lea  rsi, [szMagic]
    mov  rdi, r13
    movsq
    mov  word  ptr [r13+ 8], HDR_VERSION
    mov  byte  ptr [r13+10], 0
    mov  byte  ptr [r13+11], 0
    lea  rsi, [bSalt]
    lea  rdi, [r13+12]
    mov  ecx, SALT_LEN
    rep  movsb
    lea  rsi, [bIV]
    lea  rdi, [r13+44]
    mov  ecx, GCM_IV_LEN
    rep  movsb
    mov  dword ptr [r13+56], ebx
    mov  rcx, [pFileBuf]
    mov  edx, ebx
    lea  r8,  [r13+60]
    call GcmEncryptBytes
    lea  rsi, [gcmTag]
    lea  rdi, [r13+60]
    mov  eax, ebx
    add  rdi, rax
    mov  ecx, GCM_TAG_LEN
    rep  movsb
    mov  ecx, r12d
    call WriteOutputFile
    lea  rcx, [szDone]
    call ConWrite
    mov  ecx, r12d
    call ConWriteNum
    lea  rcx, [szBytes]
    call ConWrite
    add  rsp, 80
    pop  rdi
    pop  rsi
    pop  r13
    pop  r12
    pop  rbx
    ret
@encCF:
    push rax
    lea  rcx, [szErrCryp]
    call ConWrite
    pop  rcx
    call ConWriteHex32
    lea  rcx, [szCRLF]
    call ConWrite
    mov  ecx, 1
    call ExitProcess
@encMF:
    lea  rcx, [szErrMem]
    call FatalExit
    add  rsp, 80
    pop  rdi
    pop  rsi
    pop  r13
    pop  r12
    pop  rbx
    ret
DoEncrypt endp

DoDecrypt proc
    push rbx
    push r12
    push r13
    push rsi
    push rdi
    sub  rsp, 80
    lea  rcx, [szDec]
    call ConWrite
    mov  r13, [pFileBuf]
    lea  rsi, [szMagic]
    mov  rdi, r13
    mov  ecx, HDR_MAGIC_LEN
    repe cmpsb
    jnz  @decMag
    lea  rsi, [r13+12]
    lea  rdi, [bSalt]
    mov  ecx, SALT_LEN
    rep  movsb
    lea  rsi, [r13+44]
    lea  rdi, [bIV]
    mov  ecx, GCM_IV_LEN
    rep  movsb
    mov  ebx, dword ptr [r13+56]
    call DeriveKey
    call CamKeySchedule
    lea  rcx, [bIV]
    call GcmInit
    xor  ecx, ecx
    lea  edx, [ebx+4096]
    mov  r8d, MEM_COMMIT or MEM_RESERVE
    mov  r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz   @decMF
    mov  [pOutBuf], rax
    mov  r12, rax
    mov  rcx, r13
    add  rcx, 60
    mov  edx, ebx
    mov  r8,  r12
    mov  r9,  r13
    add  r9,  60
    add  r9,  rbx
    call GcmDecryptBytes
    test eax, eax
    jnz  @decAuth
    mov  ecx, ebx
    call WriteOutputFile
    lea  rcx, [szDone]
    call ConWrite
    mov  ecx, ebx
    call ConWriteNum
    lea  rcx, [szBytes]
    call ConWrite
    add  rsp, 80
    pop  rdi
    pop  rsi
    pop  r13
    pop  r12
    pop  rbx
    ret
@decMag:
    lea  rcx, [szErrMagic]
    call FatalExit
@decMF:
    lea  rcx, [szErrMem]
    call FatalExit
@decAuth:
    lea  rcx, [szErrAuth]
    call FatalExit
    add  rsp, 80
    pop  rdi
    pop  rsi
    pop  r13
    pop  r12
    pop  rbx
    ret
DoDecrypt endp

main proc
    sub  rsp, 40
    mov  ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov  [hStdOut], rax
    lea  rcx, [szBanner]
    call ConWrite
    call CamInitSboxes
    call ParseArgs
    ; Check pack mode first — no crypto needed
    cmp  byte ptr [bPack], 1
    je   @mainPack
    lea  rcx, [hAlgHMAC]
    lea  rdx, [wszSHA512]
    xor  r8d, r8d
    mov  r9d, BCRYPT_HMAC_FLAG
    call BCryptOpenAlgorithmProvider
    test eax, eax
    jnz  @mainBF
    call ReadInputFile
    cmp  byte ptr [bDecrypt], 0
    jne  @mainDec
    call DoEncrypt
    jmp  @mainOut
@mainDec:
    call DoDecrypt
    jmp  @mainOut
@mainPack:
    call DoPack
    jmp  @mainFree
@mainOut:
    mov  rcx, [hAlgHMAC]
    test rcx, rcx
    jz   @mainFree
    xor  edx, edx
    call BCryptCloseAlgorithmProvider
@mainFree:
    call FreeBuffers
    xor  ecx, ecx
    call ExitProcess
    add  rsp, 40
    ret
@mainBF:
    push rax
    lea  rcx, [szErrCryp]
    call ConWrite
    pop  rcx
    call ConWriteHex32
    lea  rcx, [szCRLF]
    call ConWrite
    mov  ecx, 1
    call ExitProcess
    add  rsp, 40
    ret
main endp

end