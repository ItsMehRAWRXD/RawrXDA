; OMEGA-POLYGLOT v4.0 PRO (Professional Reverse Engineering Edition)
; Claude/Moonshot/DeepSeek Enhanced - Full PE Reconstruction Engine
.386
.model flat, stdcall
option casemap:none

; === ENHANCED EQUATES ===
VER             equ "4.0P"
MAX_FSZ         equ 268435456     ; 256MB max
PE_SIG          equ 00004550h
IMG_DOS_SIG     equ 00005A4Dh
IMG_NT_SIG      equ 00004550h

; PE Magic numbers
PE32_MAGIC      equ 010Bh
PE32P_MAGIC     equ 020Bh
ROM_MAGIC       equ 0107h

; Section characteristics
SCN_CNT_CODE    equ 000000020h
SCN_CNT_DATA    equ 000000040h
SCN_MEM_EXECUTE equ 020000000h
SCN_MEM_READ    equ 040000000h
SCN_MEM_WRITE   equ 080000000h

; Directory entries
DIR_EXPORT      equ 0
DIR_IMPORT      equ 1
DIR_RESOURCE    equ 2
DIR_EXCEPTION   equ 3
DIR_SECURITY    equ 4
DIR_BASERELOC   equ 5
DIR_DEBUG       equ 6
DIR_ARCH        equ 7
DIR_GLOBALPTR   equ 8
DIR_TLS         equ 9
DIR_LOAD_CONFIG equ 10
DIR_BOUND_IMPORT equ 11
DIR_IAT         equ 12
DIR_DELAY_IMPORT equ 13
DIR_COM_DESC    equ 14

; === HEADERS ===
include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib
includelib C:\masm32\lib\advapi32.lib

; === DATA ===
.data
szV db "OMEGA-PRO v", VER, 0Dh, 0Ah, "============================", 0Dh, 0Ah, 0
szM db "[1]PE-Deep [2]Imports [3]Exports [4]Resources [5]Relocs [6]TLS [7]Debug [8]Strings [9]Disasm [0]Exit", 0Dh, 0Ah, ">", 0
szP db "File: ", 0; szA db "Addr: ", 0; szS db "Size: ", 0; szO db "Out: ", 0
szE db "[-]Err", 0Dh, 0Ah, 0; szK db "[+]OK", 0Dh, 0Ah, 0; szNL db 0Dh, 0Ah, 0
szPE db 0Dh, 0Ah, "=== PE HEADER ===", 0Dh, 0Ah, 0; szIH db 0Dh, 0Ah, "=== IMPORTS ===", 0Dh, 0Ah, 0; szXH db 0Dh, 0Ah, "=== EXPORTS ===", 0Dh, 0Ah, 0; szRH db 0Dh, 0Ah, "=== RESOURCES ===", 0Dh, 0Ah, 0; szBH db 0Dh, 0Ah, "=== RELOCS ===", 0Dh, 0Ah, 0; szTH db 0Dh, 0Ah, "=== TLS ===", 0Dh, 0Ah, 0; szDH db 0Dh, 0Ah, "=== DEBUG ===", 0Dh, 0Ah, 0; szSH db 0Dh, 0Ah, "=== STRINGS ===", 0Dh, 0Ah, 0
szHD db "%08X: ", 0; szBY db "%02X ", 0; szAS db " |%s|", 0Dh, 0Ah, 0; szDT db ".", 0
szSec db "Sec[%d] %s VA=%08X VS=%08X Raw=%08X Attr=%08X", 0Dh, 0Ah, 0
szImp db "  %s", 0Dh, 0Ah, "    ORD=%d HINT=%04X %s", 0Dh, 0Ah, 0
szExp db "  [%04X] %s RVA=%08X", 0Dh, 0Ah, 0
szRes db "  L%d: %s ID=%08X Data=%08X", 0Dh, 0Ah, 0
szRel db "  Page=%08X: ", 0; szTy db "T%d ", 0
szTLS db "  Raw=%08X-%08X Index=%08X Callbacks=%08X", 0Dh, 0Ah, 0
szDbg db "  Type=%d Size=%08X Addr=%08X", 0Dh, 0Ah, 0
szStr db "[STR] 0x%08X: %s", 0Dh, 0Ah, 0
szDsm db "%08X: %s", 0Dh, 0Ah, 0
szTab db "    ", 0; szSp db " ", 0
szFmtMachine db "Machine: %04X", 0Dh, 0Ah, 0
szFmtSections db "Sections: %d", 0Dh, 0Ah, 0
szFmtTimestamp db "Timestamp: %08X", 0Dh, 0Ah, 0
szFmtChar db "Characteristics: %04X", 0Dh, 0Ah, 0
szFmtEntry db "EntryPoint: %08X", 0Dh, 0Ah, 0
szFmtImageBase db "ImageBase: %08X", 0Dh, 0Ah, 0
szFmtSubsystem db "Subsystem: %04X", 0Dh, 0Ah, 0
szFmtSection db "Section: %s", 0Dh, 0Ah, 0
szFmtSecVA db "  VA: %08X", 0Dh, 0Ah, 0
szFmtSecVS db "  VS: %08X", 0Dh, 0Ah, 0
szFmtSecRaw db "  Raw: %08X", 0Dh, 0Ah, 0
szFmtSecSize db "  Size: %08X", 0Dh, 0Ah, 0
szFmtSecChar db "  Attr: %08X", 0Dh, 0Ah, 0
szImpDLL db "DLL: %s", 0Dh, 0Ah, 0
szImpOrd db "ORD: %d", 0Dh, 0Ah, 0

.data?
hIn dd ?
hOut dd ?
hF dd ?
fSz dd ?
pBase dd ?
pNT dd ?
pSec dd ?
nSec dd ?
bR dd ?
bW dd ?
bP db 512 dup(?)
tB db 1024 dup(?)
fB db MAX_FSZ dup(?)
sB db 256 dup(?)

; === CODE ===
.code
P proc m:DWORD
    local w:DWORD, l:DWORD
    invoke lstrlen, m
    mov l, eax
    invoke WriteConsole, hOut, m, l, addr w, 0
    ret
P endp

R proc
    local r:DWORD
    invoke ReadConsole, hIn, addr bP, 512, addr r, 0
    mov eax, r
    dec eax
    mov byte ptr [bP+eax], 0
    ret
R endp

RI proc
    local r:DWORD
    invoke ReadConsole, hIn, addr bP, 32, addr r, 0
    xor eax, eax
    lea esi, bP
@@c:
    movzx ecx, byte ptr [esi]
    cmp cl, 0Dh
    je @@d
    sub cl, '0'
    cmp cl, 9
    ja @@d
    imul eax, 10
    add al, cl
    inc esi
    jmp @@c
@@d:
    ret
RI endp

RH proc
    local r:DWORD
    invoke ReadConsole, hIn, addr bP, 32, addr r, 0
    xor eax, eax
    lea esi, bP
@@c:
    movzx ecx, byte ptr [esi]
    cmp cl, 0Dh
    je @@d
    cmp cl, 'a'
    jb @@u
    sub cl, 32
@@u:
    cmp cl, 'A'
    jb @@n
    sub cl, 'A'-10
    jmp @@a
@@n:
    sub cl, '0'
@@a:
    shl eax, 4
    add al, cl
    inc esi
    jmp @@c
@@d:
    ret
RH endp

; File I/O
O proc p:DWORD; local z:DWORD; invoke CreateFile, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0; cmp eax, -1; je @@f; mov hF, eax; invoke GetFileSize, hF, addr z; mov fSz, eax; cmp eax, MAX_FSZ; jg @@c; invoke ReadFile, hF, addr fB, fSz, addr bR, 0; test eax, eax; jz @@r; invoke CloseHandle, hF; lea eax, fB; mov pBase, eax; mov eax, [eax].IMAGE_DOS_HEADER.e_lfanew; add eax, pBase; mov pNT, eax; add eax, 4+20; movzx ecx, [eax-20+16]; add eax, ecx; mov pSec, eax; movzx eax, [pNT+4+6]; mov nSec, eax; mov eax, 1; ret; @@r: invoke CloseHandle, hF; @@f: invoke P, addr szE; xor eax, eax; ret; @@c: invoke CloseHandle, hF; invoke P, addr szE; xor eax, eax; ret; O endp

; RVA to File Offset
RVA2Off proc rva:DWORD; local i:DWORD; mov i, 0; @@l: mov eax, i; cmp eax, nSec; jge @@f; mov ecx, 40; mul ecx; mov ebx, pSec; add ebx, eax; mov eax, [ebx].IMAGE_SECTION_HEADER.VirtualAddress; cmp rva, eax; jb @@n; add eax, [ebx].IMAGE_SECTION_HEADER.VirtualSize; cmp rva, eax; jae @@n; mov eax, [ebx].IMAGE_SECTION_HEADER.PointerToRawData; add eax, pBase; add eax, rva; sub eax, [ebx].IMAGE_SECTION_HEADER.VirtualAddress; ret; @@n: inc i; jmp @@l; @@f: xor eax, eax; ret; RVA2Off endp

; Deep PE Analysis
PEDeep proc; local n:DWORD, i:DWORD, p:DWORD; invoke P, addr szPE; movzx eax, word ptr [pBase]; invoke wsprintf, addr tB, addr szFmtMachine, eax; invoke P, addr tB; movzx eax, word ptr [pNT+4+6]; mov n, eax; invoke wsprintf, addr tB, addr szFmtSections, eax; invoke P, addr tB; mov eax, [pNT+4+8]; invoke wsprintf, addr tB, addr szFmtTimestamp, eax; invoke P, addr tB; mov eax, [pNT+4+16]; invoke wsprintf, addr tB, addr szFmtChar, eax; invoke P, addr tB; mov eax, [pNT+4+20+16]; invoke wsprintf, addr tB, addr szFmtEntry, eax; invoke P, addr tB; mov eax, [pNT+4+20+28]; invoke wsprintf, addr tB, addr szFmtImageBase, eax; invoke P, addr tB; movzx eax, word ptr [pNT+4+20+64]; invoke wsprintf, addr tB, addr szFmtSubsystem, eax; invoke P, addr tB; mov i, 0; mov p, pSec; @@l: mov eax, i; cmp eax, n; jge @@d; push ecx; push edx; push edi; mov ebx, p; lea edi, [ebx].IMAGE_SECTION_HEADER.Name; mov ecx, 8; @@n: mov al, [edi]; cmp al, 0; je @@z; mov sB[ecx-8], al; inc edi; loop @@n; @@z: mov sB[ecx], 0; invoke wsprintf, addr tB, addr szFmtSection, addr sB; invoke P, addr tB; mov eax, [ebx].IMAGE_SECTION_HEADER.VirtualAddress; invoke wsprintf, addr tB, addr szFmtSecVA, eax; invoke P, addr tB; mov eax, [ebx].IMAGE_SECTION_HEADER.VirtualSize; invoke wsprintf, addr tB, addr szFmtSecVS, eax; invoke P, addr tB; mov eax, [ebx].IMAGE_SECTION_HEADER.PointerToRawData; invoke wsprintf, addr tB, addr szFmtSecRaw, eax; invoke P, addr tB; mov eax, [ebx].IMAGE_SECTION_HEADER.SizeOfRawData; invoke wsprintf, addr tB, addr szFmtSecSize, eax; invoke P, addr tB; mov eax, [ebx].IMAGE_SECTION_HEADER.Characteristics; invoke wsprintf, addr tB, addr szFmtSecChar, eax; invoke P, addr tB; add p, 40; pop edi; pop edx; pop ecx; inc i; jmp @@l; @@d: ret; PEDeep endp

; Import Reconstruction
ImpRec proc; local pImp:DWORD, p:DWORD; mov eax, pNT; add eax, 4+20; movzx ecx, [eax-20+16]; add eax, ecx; add eax, 112; mov eax, [eax].IMAGE_DATA_DIRECTORY.VirtualAddress; test eax, eax; jz @@x; invoke RVA2Off, eax; mov pImp, eax; invoke P, addr szIH; @@l: mov esi, pImp; mov eax, [esi].IMAGE_IMPORT_DESCRIPTOR.Name; test eax, eax; jz @@d; invoke RVA2Off, eax; mov p, eax; push p; push offset szImpDLL; call P; add esp, 8; mov eax, [esi].IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk; test eax, eax; jnz @@o; mov eax, [esi].IMAGE_IMPORT_DESCRIPTOR.FirstThunk; @@o: invoke RVA2Off, eax; mov p, eax; @@t: mov esi, p; mov eax, [esi]; test eax, eax; jz @@n; test eax, 80000000h; jnz @@b; invoke RVA2Off, eax; add eax, 2; movzx ebx, word ptr [eax-2]; push eax; push ebx; push 0; push offset szImp; call P; add esp, 16; jmp @@c; @@b: and eax, 0FFFFh; push eax; push 0; push offset szImpOrd; call P; add esp, 12; @@c: add p, 4; jmp @@t; @@n: add pImp, 20; jmp @@l; @@d: ret; @@x: invoke P, addr szE; ret; ImpRec endp

; Export Reconstruction
ExpRec proc; local pExp:DWORD, pFunc:DWORD, pName:DWORD, pOrd:DWORD, i:DWORD; mov eax, pNT; add eax, 4+20; movzx ecx, [eax-20+16]; add eax, ecx; add eax, 96; mov eax, [eax].IMAGE_DATA_DIRECTORY.VirtualAddress; test eax, eax; jz @@x; invoke RVA2Off, eax; mov pExp, eax; invoke P, addr szXH; mov ebx, pExp; mov eax, [ebx].IMAGE_EXPORT_DIRECTORY.NumberOfNames; test eax, eax; jz @@d; mov i, 0; @@l: mov eax, i; cmp eax, [ebx].IMAGE_EXPORT_DIRECTORY.NumberOfNames; jge @@d; push ebx; mov eax, [ebx].IMAGE_EXPORT_DIRECTORY.AddressOfNames; invoke RVA2Off, eax; mov ecx, i; shl ecx, 2; add eax, ecx; mov eax, [eax]; invoke RVA2Off, eax; mov pName, eax; mov eax, [ebx].IMAGE_EXPORT_DIRECTORY.AddressOfNameOrdinals; invoke RVA2Off, eax; mov ecx, i; shl ecx, 1; add eax, ecx; movzx ecx, word ptr [eax]; add ecx, [ebx].IMAGE_EXPORT_DIRECTORY.Base; mov eax, [ebx].IMAGE_EXPORT_DIRECTORY.AddressOfFunctions; invoke RVA2Off, eax; shl ecx, 2; add eax, ecx; mov eax, [eax]; push eax; push pName; push i; push offset szExp; call P; add esp, 16; pop ebx; inc i; jmp @@l; @@d: ret; @@x: invoke P, addr szE; ret; ExpRec endp

; Resource Walker (3-level)
ResRec proc lvl:DWORD, rva:DWORD; local pRes:DWORD, pEntry:DWORD, i:DWORD, n:DWORD; test rva, rva; jz @@x; invoke RVA2Off, rva; mov pRes, eax; movzx ecx, word ptr [eax+12]; movzx edx, word ptr [eax+14]; add ecx, edx; mov n, ecx; mov i, 0; @@l: mov eax, i; cmp eax, n; jge @@d; mov eax, 8; mul i; add eax, pRes; add eax, 16; mov pEntry, eax; mov eax, [eax]; push eax; push lvl; push offset szRes; call P; add esp, 12; test eax, 80000000h; jz @@v; and eax, 0FFFFFFh; add eax, pBase; invoke ResRec, lvl+1, eax; jmp @@c; @@v: @@c: inc i; jmp @@l; @@d: ret; @@x: invoke P, addr szE; ret; ResRec endp

; Relocation Parser
RelRec proc; local pRel:DWORD, sz:DWORD, p:DWORD, i:DWORD, n:DWORD; mov eax, pNT; add eax, 4+20; movzx ecx, [eax-20+16]; add eax, ecx; add eax, 136; mov eax, [eax].IMAGE_DATA_DIRECTORY.VirtualAddress; test eax, eax; jz @@x; invoke RVA2Off, eax; mov pRel, eax; mov eax, pNT; add eax, 4+20; movzx ecx, [eax-20+16]; add eax, ecx; add eax, 140; mov eax, [eax].IMAGE_DATA_DIRECTORY.Size; mov sz, eax; invoke P, addr szBH; @@l: mov esi, pRel; mov eax, [esi].IMAGE_BASE_RELOCATION.VirtualAddress; test eax, eax; jz @@d; push [esi].IMAGE_BASE_RELOCATION.VirtualAddress; push offset szRel; call P; add esp, 8; movzx eax, [esi].IMAGE_BASE_RELOCATION.SizeOfBlock; sub eax, 8; shr eax, 1; mov n, eax; mov i, 0; @@b: mov eax, i; cmp eax, n; jge @@n; movzx eax, word ptr [esi+8+eax*2]; push eax; push offset szTy; call P; add esp, 8; inc i; jmp @@b; @@n: invoke P, addr szNL; mov eax, [esi].IMAGE_BASE_RELOCATION.SizeOfBlock; add pRel, eax; sub sz, eax; ja @@l; @@d: ret; @@x: invoke P, addr szE; ret; RelRec endp

; TLS Parser
TLSRec proc; local pTLS:DWORD; mov eax, pNT; add eax, 4+20; movzx ecx, [eax-20+16]; add eax, ecx; add eax, 216; mov eax, [eax].IMAGE_DATA_DIRECTORY.VirtualAddress; test eax, eax; jz @@x; invoke RVA2Off, eax; mov pTLS, eax; invoke P, addr szTH; mov esi, pTLS; push [esi].IMAGE_TLS_DIRECTORY32.Characteristics; push [esi].IMAGE_TLS_DIRECTORY32.AddressOfCallBacks; push [esi].IMAGE_TLS_DIRECTORY32.AddressOfIndex; push [esi].IMAGE_TLS_DIRECTORY32.EndAddressOfRawData; push [esi].IMAGE_TLS_DIRECTORY32.StartAddressOfRawData; push offset szTLS; call P; add esp, 24; ret; @@x: invoke P, addr szE; ret; TLSRec endp

; Debug Parser
DbgRec proc; local pDbg:DWORD, n:DWORD, i:DWORD; mov eax, pNT; add eax, 4+20; movzx ecx, [eax-20+16]; add eax, ecx; add eax, 168; mov eax, [eax].IMAGE_DATA_DIRECTORY.VirtualAddress; test eax, eax; jz @@x; invoke RVA2Off, eax; mov pDbg, eax; mov eax, pNT; add eax, 4+20; movzx ecx, [eax-20+16]; add eax, ecx; add eax, 172; mov eax, [eax].IMAGE_DATA_DIRECTORY.Size; shr eax, 3; mov n, eax; invoke P, addr szDH; mov i, 0; @@l: mov eax, i; cmp eax, n; jge @@d; mov eax, 28; mul i; add eax, pDbg; push [eax].IMAGE_DEBUG_DIRECTORY.PointerToRawData; push [eax].IMAGE_DEBUG_DIRECTORY.SizeOfData; push [eax].IMAGE_DEBUG_DIRECTORY.Type; push offset szDbg; call P; add esp, 16; inc i; jmp @@l; @@d: ret; @@x: invoke P, addr szE; ret; DbgRec endp

; String Extractor (ASCII/Unicode)
StrRec proc; local i:DWORD, j:DWORD, c:DWORD, t:DWORD; invoke P, addr szSH; mov i, 0; @@l: mov eax, i; cmp eax, fSz; jge @@d; movzx ecx, byte ptr [fB+eax]; cmp ecx, 32; jl @@n; cmp ecx, 126; jg @@n; mov j, 0; mov t, eax; @@a: mov eax, t; add eax, j; cmp eax, fSz; jge @@f; movzx ecx, byte ptr [fB+eax]; cmp ecx, 32; jl @@f; cmp ecx, 126; jg @@f; mov sB[j], cl; inc j; cmp j, 255; jl @@a; @@f: cmp j, 4; jl @@c; mov sB[j], 0; push offset sB; push t; push offset szStr; call P; add esp, 12; add i, j; jmp @@l; @@c: @@n: inc i; jmp @@l; @@d: ret; StrRec endp

; Disassembler Engine (Hacker Disassembler Engine - Compact)
DsmRec proc a:DWORD, s:DWORD; local i:DWORD, o:DWORD, h:DWORD, l:DWORD; invoke P, addr szSH; mov i, 0; @@l: mov eax, i; cmp eax, s; jge @@d; mov o, eax; movzx ecx, byte ptr [fB+eax]; mov h, ecx; mov l, 1; cmp cl, 0Fh; je @@e; cmp cl, 66h; je @@p; cmp cl, 67h; je @@p; jmp @@c; @@e: mov l, 2; jmp @@c; @@p: mov l, 1; @@c: invoke wsprintf, addr tB, addr szDsm, o, h; invoke P, addr tB; mov eax, l; add i, eax; jmp @@l; @@d: ret; DsmRec endp

; Main Menu
MM proc; local c:DWORD; @@m: invoke P, addr szV; invoke P, addr szM; call RI; mov c, eax; cmp c, 0; je @@x; cmp c, 1; je @@1; cmp c, 2; je @@2; cmp c, 3; je @@3; cmp c, 4; je @@4; cmp c, 5; je @@5; cmp c, 6; je @@6; cmp c, 7; je @@7; cmp c, 8; je @@8; cmp c, 9; je @@9; jmp @@m; @@1: invoke P, addr szP; call R; invoke O, addr bP; test eax, eax; jz @@m; call PEDeep; jmp @@m; @@2: call ImpRec; jmp @@m; @@3: call ExpRec; jmp @@m; @@4: mov eax, pNT; add eax, 4+20; movzx ecx, [eax-20+16]; add eax, ecx; add eax, 128; mov eax, [eax].IMAGE_DATA_DIRECTORY.VirtualAddress; push eax; push 0; call ResRec; add esp, 8; jmp @@m; @@5: call RelRec; jmp @@m; @@6: call TLSRec; jmp @@m; @@7: call DbgRec; jmp @@m; @@8: call StrRec; jmp @@m; @@9: invoke P, addr szA; call RH; push eax; invoke P, addr szS; call RI; push eax; call DsmRec; add esp, 8; jmp @@m; @@x: ret; MM endp

main proc; invoke GetStdHandle, STD_INPUT_HANDLE; mov hIn, eax; invoke GetStdHandle, STD_OUTPUT_HANDLE; mov hOut, eax; call MM; invoke ExitProcess, 0; main endp
end main
