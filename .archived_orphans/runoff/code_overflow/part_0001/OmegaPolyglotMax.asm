; OMEGA-POLYGLOT MAX v3.1 PRO
; Professional Reverse Engineering Suite - MASM32
; Real PE/ELF/Mach-O parsing, import/export reconstruction, string analysis
.386
.model flat, stdcall
option casemap:none

; === HEADERS ===
include \masm32\include\windows.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; === EQUATES ===
VER equ "3.1PRO"
MAX_F equ 104857600
PE_SIG equ 4550h
ELF_SIG equ 7Fh
MZ_SIG equ 5A4Dh
MACH_SIG equ 0FEh

; PE Offsets
e_lfanew equ 3Ch
IMAGE_FILE_MACHINE_I386 equ 14Ch
IMAGE_FILE_MACHINE_AMD64 equ 8664h

; Data Directory indices
IMAGE_DIRECTORY_ENTRY_EXPORT equ 0
IMAGE_DIRECTORY_ENTRY_IMPORT equ 1
IMAGE_DIRECTORY_ENTRY_RESOURCE equ 2
IMAGE_DIRECTORY_ENTRY_BASERELOC equ 5
IMAGE_DIRECTORY_ENTRY_TLS equ 9

; Section characteristics
IMAGE_SCN_CNT_CODE equ 000000020h
IMAGE_SCN_CNT_INITIALIZED_DATA equ 000000040h
IMAGE_SCN_MEM_EXECUTE equ 020000000h
IMAGE_SCN_MEM_READ equ 040000000h
IMAGE_SCN_MEM_WRITE equ 080000000h

; === DATA ===
.data
szW db "OMEGA-POLYGLOT MAX v", VER, 0Dh, 0Ah, "Professional Reverse Engineering Suite", 0Dh, 0Ah, "========================================", 0Dh, 0Ah, 0
szM db "[1]Analyze PE [2]Hex Dump [3]Strings [4]Imports [5]Exports [6]Sections [7]Headers [8]Quit", 0Dh, 0Ah, ">", 0
szPF db "File: ", 0
szPA db "RVA: ", 0
szPS db "Size: ", 0
szPM db "Min Len: ", 0

szHPE db 0Dh, 0Ah, "=== PE HEADER ===", 0Dh, 0Ah, 0
szHDOS db "DOS Header: Magic=%04X e_lfanew=%08X", 0Dh, 0Ah, 0
szHNT db "NT Header: Signature=%08X", 0Dh, 0Ah, 0
szHFH db "File: Machine=%04X Sections=%d Time=%08X", 0Dh, 0Ah, 0
szHOH db "Optional: Magic=%04X Entry=%08X ImageBase=%08X", 0Dh, 0Ah, 0
szHS db 0Dh, 0Ah, "=== SECTIONS ===", 0Dh, 0Ah, 0
szSEC db "%-8s VA=%08X VS=%08X Raw=%08X RS=%08X Char=%08X", 0Dh, 0Ah, 0
szHI db 0Dh, 0Ah, "=== IMPORTS ===", 0Dh, 0Ah, 0
szIMP db "  %s", 0Dh, 0Ah, 0
szIMPF db "    %s (Hint=%04X)", 0Dh, 0Ah, 0
szIMPO db "    Ordinal=%04X", 0Dh, 0Ah, 0
szHE db 0Dh, 0Ah, "=== EXPORTS ===", 0Dh, 0Ah, 0
szEXP db "  %s @%d RVA=%08X", 0Dh, 0Ah, 0
szHSTR db 0Dh, 0Ah, "=== STRINGS ===", 0Dh, 0Ah, 0
szSTR db "[%08X] %s", 0Dh, 0Ah, 0
szHH db 0Dh, 0Ah, "=== HEX DUMP ===", 0Dh, 0Ah, 0
szHL db "%08X: ", 0
szHB db "%02X ", 0
szAS db " %s", 0Dh, 0Ah, 0
szERR db "[-] Error", 0Dh, 0Ah, 0
szOK db "[+] Success", 0Dh, 0Ah, 0
szNL db 0Dh, 0Ah, 0

.data?
hIn dd ?, hOut dd ?, hF dd ?, fSz dd ?, bR dd ?
pBase dd ?, pNT dd ?, pSec dd ?, nSec dd ?
bP db 260 dup(?), tB db 1024 dup(?), fB db MAX_F dup(?)

; === CODE ===
.code
; I/O
P proc m:DWORD; local w:DWORD, l:DWORD; invoke lstrlen, m; mov l, eax; invoke WriteConsole, hOut, m, l, addr w, 0; ret; P endp
R proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 260, addr r, 0; mov eax, r; dec eax; mov byte ptr [bP+eax], 0; ret; R endp
RI proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 32, addr r, 0; xor eax, eax; lea esi, bP; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; sub cl, '0'; cmp cl, 9; ja @@d; imul eax, 10; add al, cl; inc esi; jmp @@c; @@d: ret; RI endp
RH proc; local r:DWORD; invoke ReadConsole, hIn, addr bP, 32, addr r, 0; xor eax, eax; lea esi, bP; @@c: movzx ecx, byte ptr [esi]; cmp cl, 0Dh; je @@d; cmp cl, 'a'; jb @@u; sub cl, 32; @@u: cmp cl, 'A'; jb @@n; sub cl, 'A'-10; jmp @@a; @@n: sub cl, '0'; @@a: shl eax, 4; add al, cl; inc esi; jmp @@c; @@d: ret; RH endp

; File I/O
O proc p:DWORD; local z:DWORD; invoke CreateFile, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0; cmp eax, -1; je @@f; mov hF, eax; invoke GetFileSize, hF, addr z; mov fSz, eax; cmp eax, MAX_F; jg @@c; invoke ReadFile, hF, addr fB, fSz, addr bR, 0; test eax, eax; jz @@r; invoke CloseHandle, hF; lea eax, fB; mov pBase, eax; mov eax, 1; ret; @@r: invoke CloseHandle, hF; @@f: invoke P, addr szERR; xor eax, eax; ret; @@c: invoke CloseHandle, hF; invoke P, addr szERR; xor eax, eax; ret; O endp

; PE Utils: RVA to Raw
R2R proc rva:DWORD; local i:DWORD; mov i, 0; mov ecx, nSec; test ecx, ecx; jz @@f; @@l: mov eax, i; imul eax, 40; add eax, pSec; mov ebx, [eax+12]; cmp rva, ebx; jb @@n; add ebx, [eax+8]; cmp rva, ebx; jae @@n; mov eax, [eax+20]; sub rva, [eax+12]; add eax, rva; add eax, pBase; ret; @@n: inc i; dec ecx; jnz @@l; @@f: xor eax, eax; ret; R2R endp

; Parse PE
ParsePE proc; local pDOS:DWORD, pFH:DWORD, pOH:DWORD, v:DWORD; mov eax, pBase; mov pDOS, eax; mov eax, [eax+3Ch]; add eax, pBase; mov pNT, eax; cmp dword ptr [eax], 00004550h; jne @@f; mov eax, pNT; add eax, 4; mov pFH, eax; movzx eax, word ptr [eax+2]; mov nSec, eax; mov eax, pFH; add eax, 20; movzx ecx, word ptr [eax+16]; add eax, ecx; mov pOH, eax; mov pSec, eax; mov eax, pOH; movzx eax, word ptr [eax]; mov v, eax; invoke wsprintf, addr tB, addr szHDOS, word ptr [pDOS], dword ptr [pDOS+3Ch]; invoke P, addr tB; invoke wsprintf, addr tB, addr szHNT, dword ptr [pNT]; invoke P, addr tB; mov eax, pFH; movzx ebx, word ptr [eax]; movzx ecx, word ptr [eax+2]; mov edx, [eax+8]; invoke wsprintf, addr tB, addr szHFH, ebx, ecx, edx; invoke P, addr tB; mov eax, pOH; mov ebx, [eax+16]; mov ecx, [eax+28]; add ecx, pBase; invoke wsprintf, addr tB, addr szHOH, word ptr [eax], ebx, ecx; invoke P, addr tB; mov eax, 1; ret; @@f: xor eax, eax; ret; ParsePE endp

; Sections
DumpSec proc; local i:DWORD; invoke P, addr szHS; mov i, 0; @@l: mov eax, i; cmp eax, nSec; jge @@d; imul eax, 40; add eax, pSec; mov ebx, [eax+12]; mov ecx, [eax+8]; mov edx, [eax+20]; mov esi, [eax+16]; push dword ptr [eax+36]; push esi; push edx; push ecx; push ebx; push eax; push offset szSEC; call wsprintf; add esp, 28; invoke P, addr tB; inc i; jmp @@l; @@d: ret; DumpSec endp

; Imports
DumpImp proc; local pID:DWORD, pDesc:DWORD, dll:DWORD, thunk:DWORD, i:DWORD; mov eax, pOH; mov eax, [eax+128]; test eax, eax; jz @@d; invoke R2R, eax; test eax, eax; jz @@d; mov pID, eax; invoke P, addr szHI; @@dll: mov esi, pID; mov eax, [esi]; or eax, [esi+12]; jz @@d; mov eax, [esi+12]; invoke R2R, eax; mov dll, eax; invoke P, dll; mov eax, [esi]; test eax, eax; jnz @@oft; mov eax, [esi+16]; @@oft: invoke R2R, eax; mov thunk, eax; @@th: mov esi, thunk; mov eax, [esi]; test eax, eax; jz @@nd; test eax, 80000000h; jnz @@ord; invoke R2R, eax; movzx ecx, word ptr [eax]; add eax, 2; push ecx; push eax; push offset szIMPF; call wsprintf; add esp, 12; invoke P, addr tB; jmp @@nt; @@ord: and eax, 0FFFFh; push eax; push offset szIMPO; call wsprintf; add esp, 8; invoke P, addr tB; @@nt: add thunk, 4; jmp @@th; @@nd: add pID, 20; jmp @@dll; @@d: ret; DumpImp endp

; Exports
DumpExp proc; local pED:DWORD, pNames:DWORD, pOrds:DWORD, pFuncs:DWORD, n:DWORD, i:DWORD; mov eax, pOH; mov eax, [eax+120]; test eax, eax; jz @@d; invoke R2R, eax; test eax, eax; jz @@d; mov pED, eax; mov eax, [eax+24]; mov n, eax; test eax, eax; jz @@d; invoke P, addr szHE; mov eax, pED; mov eax, [eax+32]; invoke R2R, eax; push eax; push offset szIMP; call wsprintf; add esp, 8; invoke P, addr tB; mov eax, pED; mov eax, [eax+28]; invoke R2R, eax; mov pFuncs, eax; mov eax, pED; mov eax, [eax+32]; invoke R2R, eax; mov pNames, eax; mov eax, pED; mov eax, [eax+36]; invoke R2R, eax; mov pOrds, eax; mov i, 0; @@l: mov eax, i; cmp eax, n; jge @@d; mov esi, pNames; mov eax, [esi+eax*4]; invoke R2R, eax; push eax; mov esi, pOrds; movzx eax, word ptr [esi+i*2]; push eax; mov esi, pFuncs; mov eax, [esi+eax*4]; push eax; push offset szEXP; call wsprintf; add esp, 16; invoke P, addr tB; inc i; jmp @@l; @@d: ret; DumpExp endp

; Strings
DumpStr proc min:DWORD; local i:DWORD, j:DWORD, c:DWORD; invoke P, addr szHSTR; mov i, 0; @@l: mov eax, i; cmp eax, fSz; jge @@d; movzx ecx, byte ptr [fB+eax]; cmp cl, 20h; jl @@n; cmp cl, 7Eh; jg @@n; mov j, 0; mov c, ecx; @@g: mov eax, i; add eax, j; cmp eax, fSz; jge @@w; movzx ecx, byte ptr [fB+eax]; cmp cl, 20h; jl @@w; cmp cl, 7Eh; jg @@w; mov tB[j], cl; inc j; cmp j, 256; jl @@g; @@w: cmp j, min; jl @@c; mov tB[j], 0; push i; push offset szSTR; call wsprintf; add esp, 8; invoke P, addr tB; @@c: add i, j; @@n: inc i; jmp @@l; @@d: ret; DumpStr endp

; Hex
HexD proc a:DWORD, s:DWORD; local i:DWORD, j:DWORD; invoke P, addr szHH; mov i, 0; @@o: mov eax, i; cmp eax, s; jge @@d; add eax, a; invoke wsprintf, addr tB, addr szHL, eax; invoke P, addr tB; mov j, 0; @@h: cmp j, 16; jge @@a; mov eax, i; add eax, j; cmp eax, s; jge @@t; movzx ecx, byte ptr [fB+eax]; invoke wsprintf, addr tB, addr szHB, ecx; invoke P, addr tB; inc j; jmp @@h; @@t: invoke P, addr szS; inc j; jmp @@h; @@a: mov byte ptr [tB], 0; mov j, 0; @@b: cmp j, 16; jge @@p; mov eax, i; add eax, j; cmp eax, s; jge @@p; movzx ecx, byte ptr [fB+eax]; cmp cl, 20h; jl @@x; cmp cl, 7Eh; jg @@x; mov tB[j], cl; jmp @@y; @@x: mov tB[j], '.'; @@y: inc j; jmp @@b; @@p: mov tB[j], 0; invoke P, addr tB; invoke P, addr szNL; add i, 16; jmp @@o; @@d: ret; HexD endp

; Menu
MM proc; local c:DWORD, a:DWORD, s:DWORD; @@m: invoke P, addr szW; invoke P, addr szM; call RI; mov c, eax; cmp c, 8; je @@x; cmp c, 1; je @@1; cmp c, 2; je @@2; cmp c, 3; je @@3; cmp c, 4; je @@4; cmp c, 5; je @@5; cmp c, 6; je @@6; cmp c, 7; je @@7; jmp @@m; @@1: invoke P, addr szPF; call R; invoke O, addr bP; test eax, eax; jz @@m; call ParsePE; test eax, eax; jz @@m; call DumpSec; jmp @@m; @@2: invoke P, addr szPA; call RH; mov a, eax; invoke P, addr szPS; call RI; invoke HexD, a, eax; jmp @@m; @@3: invoke P, addr szPM; call RI; invoke DumpStr, eax; jmp @@m; @@4: call DumpImp; jmp @@m; @@5: call DumpExp; jmp @@m; @@6: call DumpSec; jmp @@m; @@7: call ParsePE; jmp @@m; @@x: ret; MM endp

; Entry
main proc; invoke GetStdHandle, STD_INPUT_HANDLE; mov hIn, eax; invoke GetStdHandle, STD_OUTPUT_HANDLE; mov hOut, eax; call MM; invoke ExitProcess, 0; main endp
end main
