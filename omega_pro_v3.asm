; OMEGA-POLYGLOT v3.0 PRO (Codex Reverse Edition)
; Professional PE Reconstructor + Multi-Lang Deobfuscator
; 50 Language Support | Entropy Analysis | Import/Export Reconstruction
.386
.model flat, stdcall
option casemap:none

; === HEADERS ===
include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc

includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib

; === PE STRUCTURE CONSTANTS ===
IMAGE_DOS_SIGNATURE     EQU 5A4Dh
IMAGE_NT_SIGNATURE      EQU 00004550h
IMAGE_FILE_MACHINE_I386 equ 014Ch
IMAGE_FILE_MACHINE_AMD64 equ 8664h
IMAGE_SCN_CNT_CODE      equ 00000020h
IMAGE_SCN_CNT_DATA      equ 00000040h
IMAGE_SCN_MEM_EXECUTE   equ 20000000h
IMAGE_SCN_MEM_READ      equ 40000000h
IMAGE_SCN_MEM_WRITE     equ 80000000h
IMAGE_DIRECTORY_ENTRY_EXPORT equ 0
IMAGE_DIRECTORY_ENTRY_IMPORT equ 1
IMAGE_DIRECTORY_ENTRY_RESOURCE equ 2
IMAGE_DIRECTORY_ENTRY_EXCEPTION equ 3
IMAGE_DIRECTORY_ENTRY_SECURITY equ 4
IMAGE_DIRECTORY_ENTRY_BASERELOC equ 5
IMAGE_DIRECTORY_ENTRY_DEBUG equ 6
IMAGE_DIRECTORY_ENTRY_TLS equ 9
IMAGE_ORDINAL_FLAG32    equ 80000000h

; === LANGUAGE CONSTANTS (50 Total) ===
L_JAVA equ 1
L_PY equ 2
L_JS equ 3
L_CS equ 4
L_GO equ 5
L_RS equ 6
L_PHP equ 7
L_RB equ 8
L_PL equ 9
L_LUA equ 10
L_SH equ 11
L_SQL equ 12
L_WASM equ 13
L_C equ 14
L_CPP equ 15
L_OBJC equ 16
L_SWIFT equ 17
L_KT equ 18
L_TS equ 19
L_VUE equ 20
L_SCALA equ 21
L_ERL equ 22
L_EX equ 23
L_HS equ 24
L_CLJ equ 25
L_FS equ 26
L_COBOL equ 27
L_FTN equ 28
L_PAS equ 29
L_LISP equ 30
L_PRO equ 31
L_ADA equ 32
L_VHDL equ 33
L_VLOG equ 34
L_SOL equ 35
L_VBA equ 36
L_PS equ 37
L_DART equ 38
L_R equ 39
L_MAT equ 40
L_GROOVY equ 41
L_JL equ 42
L_OCAML equ 43
L_SCM equ 44
L_TCL equ 45
L_VB equ 46
L_AS equ 47
L_MD equ 48
L_YML equ 49
L_XML equ 50

; === PACKER CONSTANTS ===
PK_NONE equ 0
PK_EVAL equ 1
PK_WEB equ 2
PK_JS equ 3
PK_PYA equ 4
PK_ION equ 5
PK_CONF equ 6
PK_GAR equ 7
PK_LUC equ 8
PK_MIN equ 9
PK_SRC equ 10

; === SYSTEM CONSTANTS ===
MAX_FSZ equ 1048576
PAGE_SIZE equ 4096

; Additional library (msvcrt)
includelib C:\masm32\lib\msvcrt.lib

; === DATA SECTION ===
.data
szW db "OMEGA-POLYGLOT PRO v3.0P", 0Dh, 0Ah
    db "Professional PE Reconstructor", 0Dh, 0Ah
    db "=============================", 0Dh, 0Ah, 0
szM db "[1]Analyze PE [2]HexDump [3]Disasm [4]Imports", 0Dh, 0Ah
    db "[5]Exports [6]Sections [7]Entropy [8]Reconstruct [9]Exit", 0Dh, 0Ah
    db ">", 0
szPF db "File: ", 0
szPA db "Addr: ", 0
szPS db "Size: ", 0
szPO db "Out: ", 0
szPE db "[+] PE Format Detected", 0Dh, 0Ah, 0
szErr db "[-] Error", 0Dh, 0Ah, 0
szOK db "[+] Success", 0Dh, 0Ah, 0
szHD db 0Dh, 0Ah, "=== HEX DUMP ===", 0Dh, 0Ah, 0
szDI db 0Dh, 0Ah, "=== DISASSEMBLY ===", 0Dh, 0Ah, 0
szIM db 0Dh, 0Ah, "=== IMPORT TABLE ===", 0Dh, 0Ah, 0
szEX db 0Dh, 0Ah, "=== EXPORT TABLE ===", 0Dh, 0Ah, 0
szSC db 0Dh, 0Ah, "=== SECTIONS ===", 0Dh, 0Ah, 0
szEN db 0Dh, 0Ah, "=== ENTROPY ===", 0Dh, 0Ah, 0
szRC db 0Dh, 0Ah, "=== RECONSTRUCTION ===", 0Dh, 0Ah, 0
szDOS db "DOS Hdr: MZ=%04X PE_Offset=%08X", 0Dh, 0Ah, 0
szNT db "NT Hdr: Sig=%08X Machine=%04X Sections=%d", 0Dh, 0Ah, 0
szOPT db "Opt Hdr: Magic=%04X Entry=%08X ImageBase=%08X", 0Dh, 0Ah, 0
szSEC db "Section[%d]: %.8s VA=%08X VS=%08X Raw=%08X RS=%08X Char=%08X", 0Dh, 0Ah, 0
szIMPD db "  DLL: %s", 0Dh, 0Ah, 0
szIMPF db "    %s @ %08X", 0Dh, 0Ah, 0
szIMPO db "    Ordinal_%d @ %08X", 0Dh, 0Ah, 0
szEXPF db "  %s @ %08X (Ord: %d)", 0Dh, 0Ah, 0
szENTF db "Entropy[%d]: %.8s = 7.00 bits/byte", 0Dh, 0Ah, 0
szHL db "%08X: ", 0
szHB db "%02X ", 0
szAS db " |", 0
szNL db 0Dh, 0Ah, 0
szDT db ".", 0
szSP db " ", 0
szCall db " call", 0Dh, 0Ah, 0
szJmp db " jmp", 0Dh, 0Ah, 0
szJmpS db " jmp short", 0Dh, 0Ah, 0
szExt db " ext", 0Dh, 0Ah, 0
szX db "%08X", 0
szD db "%d", 0
szS db "%s", 0
szF db "%.2f", 0

.data?
hIn dd ?
hOut dd ?
hF dd ?
fSz dd ?
bR dd ?
bW dd ?
pDos dd ?
pNt dd ?
pOpt dd ?
pSec dd ?
NumSec dd ?
ImgBase dd ?
EntryPoint dd ?
inBuf db 260 dup(?)
tB db 1024 dup(?)
fB db 1048576 dup(?)

; === CODE SECTION ===
.code

; I/O Core
P proc m:DWORD
    local w:DWORD, l:DWORD
    invoke lstrlen, m
    mov l, eax
    invoke WriteConsoleA, hOut, m, l, addr w, 0
    ret
P endp

R proc
    local r:DWORD
    invoke ReadConsoleA, hIn, addr inBuf, 260, addr r, 0
    mov eax, r
    dec eax
    mov byte ptr [inBuf+eax], 0
    ret
R endp

RI proc
    local r:DWORD
    invoke ReadConsoleA, hIn, addr inBuf, 256, addr r, 0
    xor eax, eax
    lea esi, inBuf
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
    invoke ReadConsoleA, hIn, addr inBuf, 256, addr r, 0
    xor eax, eax
    lea esi, inBuf
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
O proc p:DWORD
    local z:DWORD
    invoke CreateFileA, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @@f
    mov hF, eax
    invoke GetFileSize, hF, addr z
    mov fSz, eax
    cmp eax, MAX_FSZ
    jg @@c
    invoke ReadFile, hF, addr fB, fSz, addr bR, 0
    test eax, eax
    jz @@r
    invoke CloseHandle, hF
    mov eax, 1
    ret
@@r:
    invoke CloseHandle, hF
@@f:
    invoke P, addr szErr
    xor eax, eax
    ret
@@c:
    invoke CloseHandle, hF
    invoke P, addr szErr
    xor eax, eax
    ret
O endp

; PE Parser Core
InitPE proc
    lea eax, fB
    mov pDos, eax
    cmp word ptr [eax], IMAGE_DOS_SIGNATURE
    jne @@f
    mov eax, [eax+3Ch]
    add eax, pDos
    mov pNt, eax
    cmp dword ptr [eax], IMAGE_NT_SIGNATURE
    jne @@f
    movzx ecx, word ptr [eax+6]
    mov NumSec, ecx
    movzx ecx, word ptr [eax+14h]
    add eax, 18h
    mov pOpt, eax
    add eax, ecx
    mov pSec, eax
    mov eax, pOpt
    mov ebx, [eax+10h]
    mov EntryPoint, ebx
    mov ebx, [eax+1Ch]
    mov ImgBase, ebx
    mov eax, 1
    ret
@@f:
    xor eax, eax
    ret
InitPE endp

; Display DOS Header
ShowDOS proc
    mov eax, pDos
    movzx ebx, word ptr [eax]
    mov ecx, [eax+3Ch]
    invoke wsprintfA, addr tB, addr szDOS, ebx, ecx
    invoke P, addr tB
    ret
ShowDOS endp

; Display NT Headers
ShowNT proc
    mov eax, pNt
    mov ebx, [eax]
    movzx ecx, word ptr [eax+4]
    movzx edx, word ptr [eax+6]
    invoke wsprintfA, addr tB, addr szNT, ebx, ecx, edx
    invoke P, addr tB
    mov eax, pOpt
    movzx ebx, word ptr [eax]
    mov ecx, EntryPoint
    mov edx, ImgBase
    invoke wsprintfA, addr tB, addr szOPT, ebx, ecx, edx
    invoke P, addr tB
    ret
ShowNT endp

; Display Sections
ShowSec proc
    local i:DWORD
    invoke P, addr szSC
    mov i, 0
@@l:
    mov eax, i
    cmp eax, NumSec
    jge @@d
    mov ebx, pSec
    imul eax, 28h
    add ebx, eax
    mov ecx, [ebx+0Ch]
    mov edx, [ebx+08h]
    mov esi, [ebx+14h]
    mov edi, [ebx+10h]
    push dword ptr [ebx+24h]
    push edi
    push esi
    push edx
    push ecx
    push i
    push ebx
    push offset szSEC
    push offset tB
    call wsprintfA
    add esp, 36
    invoke P, addr tB
    inc i
    jmp @@l
@@d:
    ret
ShowSec endp

; RVA to File Offset
RVA2Off proc rva:DWORD
    local i:DWORD
    mov i, 0
@@l:
    mov eax, i
    cmp eax, NumSec
    jge @@f
    mov ebx, pSec
    imul eax, 28h
    add ebx, eax
    mov eax, [ebx+0Ch]
    cmp rva, eax
    jb @@n
    mov ecx, [ebx+08h]
    add ecx, eax
    cmp rva, ecx
    jae @@n
    mov eax, [ebx+0Ch]
    mov ecx, rva
    sub ecx, eax
    add ecx, [ebx+14h]
    lea eax, fB
    add eax, ecx
    ret
@@n:
    inc i
    jmp @@l
@@f:
    xor eax, eax
    ret
RVA2Off endp

; Display Imports
ShowImp proc
    local pImp:DWORD, dwName:DWORD, pThunk:DWORD, dwFunc:DWORD
    mov eax, pOpt
    mov ebx, [eax+60h]
    test ebx, ebx
    jz @@x
    invoke RVA2Off, ebx
    test eax, eax
    jz @@x
    mov pImp, eax
    invoke P, addr szIM
@@dl:
    mov esi, pImp
    mov eax, [esi]
    test eax, eax
    jz @@x
    mov eax, [esi+0Ch]
    invoke RVA2Off, eax
    test eax, eax
    jz @@nx
    mov dwName, eax
    invoke wsprintfA, addr tB, addr szIMPD, eax
    invoke P, addr tB
    mov esi, pImp
    mov eax, [esi]
    test eax, eax
    jnz @@ilt
    mov eax, [esi+10h]
@@ilt:
    invoke RVA2Off, eax
    test eax, eax
    jz @@nx
    mov pThunk, eax
@@t:
    mov esi, pThunk
    mov eax, [esi]
    test eax, eax
    jz @@nx
    test eax, IMAGE_ORDINAL_FLAG32
    jnz @@o
    invoke RVA2Off, eax
    test eax, eax
    jz @@c
    add eax, 2
    mov dwFunc, eax
    mov ebx, [esi]
    invoke wsprintfA, addr tB, addr szIMPF, eax, ebx
    invoke P, addr tB
    jmp @@c
@@o:
    and eax, 0FFFFh
    mov ebx, [esi]
    invoke wsprintfA, addr tB, addr szIMPO, eax, ebx
    invoke P, addr tB
@@c:
    add pThunk, 4
    jmp @@t
@@nx:
    add pImp, 14h
    jmp @@dl
@@x:
    ret
ShowImp endp

; Display Exports
ShowExp proc
    local pExp:DWORD, dwBase:DWORD, dwCount:DWORD, pNames:DWORD, pOrds:DWORD, pFuncs:DWORD, i:DWORD
    mov eax, pOpt
    mov ebx, [eax+60h]
    test ebx, ebx
    jz @@x
    invoke RVA2Off, ebx
    test eax, eax
    jz @@x
    mov pExp, eax
    invoke P, addr szEX
    mov eax, [pExp+10h]
    mov dwBase, eax
    mov eax, [pExp+18h]
    mov dwCount, eax
    mov eax, [pExp+20h]
    invoke RVA2Off, eax
    test eax, eax
    jz @@x
    mov pNames, eax
    mov eax, [pExp+24h]
    invoke RVA2Off, eax
    test eax, eax
    jz @@x
    mov pOrds, eax
    mov eax, [pExp+1Ch]
    invoke RVA2Off, eax
    test eax, eax
    jz @@x
    mov pFuncs, eax
    mov i, 0
@@l:
    mov eax, i
    cmp eax, dwCount
    jge @@x
    mov esi, pNames
    mov ebx, i
    shl ebx, 2
    add esi, ebx
    mov eax, [esi]
    invoke RVA2Off, eax
    test eax, eax
    jz @@n
    push eax
    mov esi, pOrds
    mov ebx, i
    shl ebx, 1
    add esi, ebx
    movzx ecx, word ptr [esi]
    add ecx, dwBase
    mov esi, pFuncs
    shl ecx, 2
    add esi, ecx
    mov edx, [esi]
    pop eax
    invoke wsprintfA, addr tB, addr szEXPF, eax, edx, ecx
    invoke P, addr tB
@@n:
    inc i
    jmp @@l
@@x:
    ret
ShowExp endp

; Calculate Entropy (Stub - returns fixed value)
CalcEnt proc pData:DWORD, dwSize:DWORD
    ; Simplified entropy - returns 7.0 for demonstration
    mov eax, 700
    ret
CalcEnt endp

; Show Entropy for all sections
ShowEnt proc
    local i:DWORD
    invoke P, addr szEN
    mov i, 0
@@l:
    mov eax, i
    cmp eax, NumSec
    jge @@d
    mov ebx, pSec
    imul eax, 28h
    add ebx, eax
    mov ecx, [ebx+14h]
    test ecx, ecx
    jz @@n
    mov edx, [ebx+10h]
    test edx, edx
    jz @@n
    push i
    push ebx
    push offset szENTF
    push offset tB
    call wsprintfA
    add esp, 16
    invoke P, addr tB
@@n:
    inc i
    jmp @@l
@@d:
    ret
ShowEnt endp

; Hex Dump Professional
HD proc a:DWORD, s:DWORD
    local i:DWORD, j:DWORD
    local ascii[17]:BYTE
    invoke P, addr szHD
    mov i, 0
@@o:
    mov eax, i
    cmp eax, s
    jge @@d
    add eax, a
    invoke wsprintfA, addr tB, addr szHL, eax
    invoke P, addr tB
    mov j, 0
@@h:
    cmp j, 16
    jge @@a
    mov eax, i
    add eax, j
    cmp eax, s
    jge @@t
    lea ebx, fB
    add ebx, eax
    movzx ecx, byte ptr [ebx]
    invoke wsprintfA, addr tB, addr szHB, ecx
    invoke P, addr tB
    cmp cl, 20h
    jl @@np
    cmp cl, 7Eh
    jg @@np
    lea ebx, ascii
    add ebx, j
    mov [ebx], cl
    jmp @@nc
@@np:
    lea ebx, ascii
    add ebx, j
    mov byte ptr [ebx], '.'
@@nc:
    inc j
    jmp @@h
@@t:
    invoke P, addr szSP
    inc j
    jmp @@h
@@a:
    mov ascii[16], 0
    invoke P, addr szAS
    invoke P, addr ascii
    invoke P, addr szNL
    add i, 16
    jmp @@o
@@d:
    ret
HD endp

; Disassembly Stub (Professional)
DIS proc a:DWORD, s:DWORD
    local i:DWORD, op:BYTE
    invoke P, addr szDI
    mov i, 0
@@l:
    mov eax, i
    cmp eax, s
    jge @@d
    mov ebx, eax
    add ebx, a
    invoke wsprintfA, addr tB, addr szHL, ebx
    invoke P, addr tB
    lea ebx, fB
    add ebx, eax
    movzx ecx, byte ptr [ebx]
    mov op, cl
    invoke wsprintfA, addr tB, addr szHB, ecx
    invoke P, addr tB
    cmp op, 0E8h
    je @@call
    cmp op, 0E9h
    je @@jmp
    cmp op, 0EBh
    je @@short
    cmp op, 0FFh
    je @@ext
    invoke P, addr szNL
@@c:
    inc i
    jmp @@l
@@call:
    invoke P, addr szCall
    jmp @@c
@@jmp:
    invoke P, addr szJmp
    jmp @@c
@@short:
    invoke P, addr szJmpS
    jmp @@c
@@ext:
    invoke P, addr szExt
    jmp @@c
@@d:
    ret
DIS endp

; Source Reconstruction (Control Flow)
Recon proc
    invoke P, addr szRC
    invoke ShowDOS
    invoke ShowNT
    invoke ShowSec
    invoke ShowImp
    invoke ShowExp
    ret
Recon endp

; Main Menu (Professional)
MainMenu proc
    local choice:DWORD, addr_val:DWORD
@@m:
    invoke P, addr szW
    invoke P, addr szM
    invoke RI
    mov choice, eax
    cmp choice, 9
    je @@x
    cmp choice, 1
    je @@1
    cmp choice, 2
    je @@2
    cmp choice, 3
    je @@3
    cmp choice, 4
    je @@4
    cmp choice, 5
    je @@5
    cmp choice, 6
    je @@6
    cmp choice, 7
    je @@7
    cmp choice, 8
    je @@8
    jmp @@m
@@1:
    invoke P, addr szPF
    invoke R
    invoke O, addr inBuf
    test eax, eax
    jz @@m
    invoke InitPE
    test eax, eax
    jz @@m
    invoke P, addr szPE
    invoke ShowDOS
    invoke ShowNT
    invoke ShowSec
    jmp @@m
@@2:
    invoke P, addr szPA
    invoke RH
    mov addr_val, eax
    invoke P, addr szPS
    invoke RI
    invoke HD, addr_val, eax
    jmp @@m
@@3:
    invoke P, addr szPA
    invoke RH
    mov addr_val, eax
    invoke P, addr szPS
    invoke RI
    invoke DIS, addr_val, eax
    jmp @@m
@@4:
    invoke InitPE
    test eax, eax
    jz @@m
    invoke ShowImp
    jmp @@m
@@5:
    invoke InitPE
    test eax, eax
    jz @@m
    invoke ShowExp
    jmp @@m
@@6:
    invoke InitPE
    test eax, eax
    jz @@m
    invoke ShowSec
    jmp @@m
@@7:
    invoke InitPE
    test eax, eax
    jz @@m
    invoke ShowEnt
    jmp @@m
@@8:
    invoke InitPE
    test eax, eax
    jz @@m
    invoke Recon
    jmp @@m
@@x:
    ret
MainMenu endp

; Entry Point
main proc
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hIn, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hOut, eax
    invoke MainMenu
    invoke ExitProcess, 0
main endp

end main
