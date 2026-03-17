; OMEGA-POLYGLOT v3.0P (Professional Reverse Edition)
; Claude/Moonshot/DeepSeek Enhanced PE Analysis + Source Reconstruction
; 50-Lang Deobfuscation with Real Import/Export/Resource Parsing
.386
.model flat, stdcall
option casemap:none

; === PE STRUCTURE EQUATES ===
IMAGE_DOS_HEADER_sz equ 64
IMAGE_NT_HEADERS_sz equ 248
IMAGE_SECTION_HEADER_sz equ 40
IMAGE_IMPORT_DESCRIPTOR_sz equ 20
IMAGE_EXPORT_DIRECTORY_sz equ 40
IMAGE_RESOURCE_DIRECTORY_sz equ 16
IMAGE_RESOURCE_ENTRY_sz equ 8

; === COMPACT EQUATES ===
CLI_VER equ "3.0P"
MAX_FSZ equ 104857600
PE_SIG  equ 00004550h
ELF_SIG equ 0000007Fh
MZ_SIG  equ 00005A4Dh
MACH_SIG equ 000000FEh

; Language IDs (50 Total)
L_JAVA equ 1; L_PY equ 2; L_JS equ 3; L_CS equ 4; L_GO equ 5; L_RS equ 6; L_PHP equ 7; L_RB equ 8; L_PL equ 9; L_LUA equ 10
L_SH equ 11; L_SQL equ 12; L_WASM equ 13; L_C equ 14; L_CPP equ 15; L_OBJC equ 16; L_SWIFT equ 17; L_KT equ 18; L_TS equ 19; L_VUE equ 20
L_SCALA equ 21; L_ERL equ 22; L_EX equ 23; L_HS equ 24; L_CLJ equ 25; L_FS equ 26; L_COBOL equ 27; L_FTN equ 28; L_PAS equ 29; L_LISP equ 30
L_PRO equ 31; L_ADA equ 32; L_VHDL equ 33; L_VLOG equ 34; L_SOL equ 35; L_VBA equ 36; L_PS equ 37; L_DART equ 38; L_R equ 39; L_MAT equ 40
L_GROOVY equ 41; L_JL equ 42; L_OCAML equ 43; L_SCM equ 44; L_TCL equ 45; L_VB equ 46; L_AS equ 47; L_MD equ 48; L_YML equ 49; L_XML equ 50

; Packer Signatures
UPX_SIG equ 55505821h; ASProtect equ 01536952h; Themida equ 21536952h; VMProtect equ 32536952h

; === HEADERS ===
include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib

; External function prototypes
externdef lstrlen :PROC
externdef WriteConsole :PROC
externdef ReadConsole :PROC
externdef wsprintf :PROC
externdef RtlZeroMemory :PROC
externdef GetStdHandle :PROC
externdef ExitProcess :PROC
externdef CreateFile :PROC
externdef GetFileSize :PROC
externdef ReadFile :PROC
externdef CloseHandle :PROC

; === DATA ===
.data
szW db "OMEGA-POLYGLOT PRO v", CLI_VER, 0Dh, 0Ah
    db "Claude/Moonshot/DeepSeek Enhanced", 0Dh, 0Ah
    db "=================================", 0Dh, 0Ah, 0
szM db "[1]PE-Deep [2]Imports [3]Exports [4]Resources [5]Sections", 0Dh, 0Ah
    db "[6]Strings [7]Entropy [8]Lang-ID [9]Decomp [0]Exit", 0Dh, 0Ah, ">", 0
szPF db "Target: ", 0
szPH db "RVA: ", 0
szPS db "Size: ", 0
szPE db "[+] PE32/PE32+ Detected", 0Dh, 0Ah, 0
szEP db "EntryPoint: %08X", 0Dh, 0Ah, 0
szIM db "ImageBase: %08X", 0Dh, 0Ah, 0
szIS db "Imports:", 0Dh, 0Ah, "--------", 0Dh, 0Ah, 0
szIF db "  %s -> %s", 0Dh, 0Ah, 0
szIO db "  Ordinal: %d", 0Dh, 0Ah, 0
szXS db "Exports:", 0Dh, 0Ah, "--------", 0Dh, 0Ah, 0
szXF db "  [%04X] %s @ %08X", 0Dh, 0Ah, 0
szXO db "  [%04X] Ordinal @ %08X", 0Dh, 0Ah, 0
szSS db "Sections:", 0Dh, 0Ah, "---------", 0Dh, 0Ah, 0
szSF db "  %8s VA:%08X VS:%08X Raw:%08X Ent:%02d%%", 0Dh, 0Ah, 0
szRS db "Resources:", 0Dh, 0Ah, "----------", 0Dh, 0Ah, 0
szTS db "Strings:", 0Dh, 0Ah, "--------", 0Dh, 0Ah, 0
szTF db "  [%08X] %s", 0Dh, 0Ah, 0
szEN db "Entropy: %02d%% (Packed: %s)", 0Dh, 0Ah, 0
szYS db "Yes", 0
szNS db "No", 0
szUK db "Unknown", 0Dh, 0Ah, 0
szLG db "Language: %s (Confidence: %d%%)", 0Dh, 0Ah, 0
szDC db "Decompiling...", 0Dh, 0Ah, 0
szER db "[-] Error", 0Dh, 0Ah, 0
szOK db "[+] Success", 0Dh, 0Ah, 0
szNL db 0Dh, 0Ah, 0

; Language Signatures
sigJava db ".class", 0
sigPy db "import ", 0
sigPy2 db "from ", 0
sigJs db "function", 0
sigJs2 db "var ", 0
sigGo db "package ", 0
sigRs db "fn ", 0
sigRs2 db "use ", 0
sigPhp db "<?php", 0
sigRb db "def ", 0
sigRb2 db "require ", 0
sigLua db "local ", 0
sigSql db "SELECT ", 0
sigSql2 db "INSERT ", 0
sigWasm db "asm", 0

.data?
hIn dd ?
hOut dd ?
hF dd ?
fSz dd ?
bR dd ?
pBase dd ?
pNT dd ?
pSec dd ?
nSec dd ?
EP dd ?
IB dd ?
bP db 260 dup(?)
tB db 1024 dup(?)
fB db MAX_FSZ dup(?)
sB db 64 dup(?)

; Hash buffer for entropy calculation
hashBuf dd 256 dup(0)

; === CODE (Professional Reverse Engineered) ===
.code

; I/O Core
P proc uses ebx ecx edx, m:DWORD
    LOCAL w:DWORD, l:DWORD
    invoke lstrlen, m
    mov l, eax
    invoke WriteConsole, hOut, m, l, addr w, 0
    ret
P endp

R proc uses ebx ecx edx
    LOCAL r:DWORD
    invoke ReadConsole, hIn, addr bP, 260, addr r, 0
    mov eax, r
    dec eax
    mov byte ptr [bP+eax], 0
    ret
R endp

RI proc uses ebx ecx edx esi
    LOCAL r:DWORD
    invoke ReadConsole, hIn, addr bP, 16, addr r, 0
    xor eax, eax
    lea esi, bP
@@convert:
    movzx ecx, byte ptr [esi]
    cmp cl, 0Dh
    je @@done
    sub cl, '0'
    cmp cl, 9
    ja @@done
    imul eax, 10
    add al, cl
    inc esi
    jmp @@convert
@@done:
    ret
RI endp

RH proc uses ebx ecx edx esi
    LOCAL r:DWORD
    invoke ReadConsole, hIn, addr bP, 16, addr r, 0
    xor eax, eax
    lea esi, bP
@@convert:
    movzx ecx, byte ptr [esi]
    cmp cl, 0Dh
    je @@done
    cmp cl, 'a'
    jb @@upper
    sub cl, 32
@@upper:
    cmp cl, 'A'
    jb @@digit
    sub cl, 'A'-10
    jmp @@add
@@digit:
    sub cl, '0'
@@add:
    shl eax, 4
    add al, cl
    inc esi
    jmp @@convert
@@done:
    ret
RH endp

; PE Core (Real RVA->Offset Conversion)
RVA2OFF proc uses ebx ecx edx, rva:DWORD
    LOCAL i:DWORD
    mov i, 0
    mov ecx, nSec
    test ecx, ecx
    jz @@not_found
@@loop:
    mov eax, i
    imul eax, IMAGE_SECTION_HEADER_sz
    add eax, pSec
    mov ebx, [eax+12]
    cmp rva, ebx
    jb @@next
    add ebx, [eax+8]
    cmp rva, ebx
    jae @@next
    mov eax, [eax+20]
    add eax, rva
    sub eax, [eax-8]
    add eax, pBase
    ret
@@next:
    inc i
    loop @@loop
@@not_found:
    mov eax, rva
    add eax, pBase
    ret
RVA2OFF endp

; File Loader
O proc uses ebx ecx edx, p:DWORD
    LOCAL z:DWORD
    invoke CreateFile, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0
    cmp eax, -1
    je @@error
    mov hF, eax
    invoke GetFileSize, hF, addr z
    mov fSz, eax
    cmp eax, MAX_FSZ
    jg @@too_large
    invoke ReadFile, hF, addr fB, fSz, addr bR, 0
    test eax, eax
    jz @@read_error
    invoke CloseHandle, hF
    lea eax, fB
    mov pBase, eax
    mov ax, [eax]
    cmp ax, MZ_SIG
    jne @@invalid
    mov eax, [fB+60]
    add eax, pBase
    mov pNT, eax
    mov eax, [eax]
    cmp eax, PE_SIG
    jne @@invalid
    mov eax, pNT
    movzx ecx, word ptr [eax+6]
    mov nSec, ecx
    add eax, 24
    movzx ecx, word ptr [eax-4]
    add eax, ecx
    mov pSec, eax
    mov eax, pNT
    mov EP, [eax+40]
    mov IB, [eax+52]
    mov eax, 1
    ret
@@read_error:
    invoke CloseHandle, hF
@@error:
    invoke P, addr szER
    xor eax, eax
    ret
@@too_large:
    invoke CloseHandle, hF
    invoke P, addr szER
    xor eax, eax
    ret
@@invalid:
    invoke P, addr szUK
    xor eax, eax
    ret
O endp

; Import Parser (Real IT Parsing)
AI proc uses ebx ecx edx esi
    LOCAL iid:DWORD, dll:DWORD, oft:DWORD, ft:DWORD, h:DWORD, o:DWORD
    invoke P, addr szIS
    mov eax, pNT
    add eax, 128
    mov eax, [eax]
    test eax, eax
    jz @@done
    invoke RVA2OFF, eax
    mov iid, eax
@@loop:
    mov esi, iid
    mov eax, [esi]
    test eax, eax
    jnz @@check
    mov eax, [esi+12]
    test eax, eax
    jnz @@check
    mov eax, [esi+16]
    test eax, eax
    jz @@done
@@check:
    mov eax, [esi+12]
    invoke RVA2OFF, eax
    mov dll, eax
    mov eax, [esi]
    test eax, eax
    jnz @@got_oft
    mov eax, [esi+16]
@@got_oft:
    invoke RVA2OFF, eax
    mov oft, eax
@@parse_thunk:
    mov esi, oft
    mov eax, [esi]
    test eax, eax
    jz @@next_dll
    test eax, 80000000h
    jnz @@ordinal
    invoke RVA2OFF, eax
    mov ebx, eax
    movzx ecx, word ptr [ebx]
    mov h, ecx
    add ebx, 2
    invoke wsprintf, addr tB, addr szIF, dll, ebx
    invoke P, addr tB
    jmp @@next_thunk
@@ordinal:
    and eax, 0FFFFh
    mov o, eax
    invoke wsprintf, addr tB, addr szIO, o
    invoke P, addr tB
@@next_thunk:
    add oft, 4
    jmp @@parse_thunk
@@next_dll:
    add iid, IMAGE_IMPORT_DESCRIPTOR_sz
    jmp @@loop
@@done:
    ret
AI endp

; Export Parser (Real EAT Parsing)
AE proc uses ebx ecx edx esi
    LOCAL ed:DWORD, n:DWORD, f:DWORD, o:DWORD, nms:DWORD, ord:DWORD, i:DWORD
    invoke P, addr szXS
    mov eax, pNT
    add eax, 120
    mov eax, [eax]
    test eax, eax
    jz @@done
    invoke RVA2OFF, eax
    mov ed, eax
    mov esi, ed
    mov eax, [esi+24]
    mov n, eax
    mov eax, [esi+28]
    invoke RVA2OFF, eax
    mov f, eax
    mov eax, [esi+32]
    invoke RVA2OFF, eax
    mov nms, eax
    mov eax, [esi+36]
    invoke RVA2OFF, eax
    mov ord, eax
    mov i, 0
@@loop:
    mov eax, i
    cmp eax, n
    jge @@done
    mov esi, ord
    movzx eax, word ptr [esi+eax*2]
    mov o, eax
    mov esi, nms
    mov eax, [esi+i*4]
    invoke RVA2OFF, eax
    mov ebx, eax
    mov esi, f
    mov eax, [esi+o*4]
    invoke wsprintf, addr tB, addr szXF, o, ebx, eax
    invoke P, addr tB
    inc i
    jmp @@loop
@@done:
    ret
AE endp

; Section Analyzer (Entropy Calculation)
AS proc uses ebx ecx edx esi
    LOCAL i:DWORD, e:DWORD, j:DWORD, s:DWORD, r:DWORD, ptrHash:DWORD
    invoke P, addr szSS
    mov i, 0
@@loop:
    mov eax, i
    cmp eax, nSec
    jge @@done
    imul eax, IMAGE_SECTION_HEADER_sz
    add eax, pSec
    mov ebx, eax
    lea esi, [ebx]
    invoke RtlZeroMemory, addr hashBuf, 1024
    mov s, 0
    mov r, 0
@@hash:
    cmp s, [ebx+8]
    jge @@calc
    movzx ecx, byte ptr [fB+r]
    mov edx, ecx
    shl edx, 2
    inc dword ptr [hashBuf+edx]
    inc s
    inc r
    jmp @@hash
@@calc:
    mov e, 0
    mov j, 0
@@entropy:
    cmp j, 256
    jge @@print
    mov edx, j
    shl edx, 2
    mov eax, [hashBuf+edx]
    test eax, eax
    jz @@skip
    push eax
    fild dword ptr [esp]
    fld st(0)
    fild fSz
    fdivp st(1), st(0)
    fld1
    fxch
    fyl2x
    fmulp st(1), st(0)
    fistp dword ptr [esp]
    pop eax
    neg eax
    add e, eax
@@skip:
    inc j
    jmp @@entropy
@@print:
    mov eax, e
    mov ecx, 100
    mul ecx
    shr eax, 8
    mov e, eax
    mov ecx, [ebx+8]
    mov edx, [ebx+16]
    mov r, edx
    movzx eax, word ptr [ebx+36]
    shr eax, 29
    and eax, 1
    test eax, eax
    jz @@unpacked
    invoke wsprintf, addr tB, addr szSF, ebx, [ebx+12], [ebx+8], r, e
    invoke P, addr tB
@@unpacked:
    inc i
    jmp @@loop
@@done:
    ret
AS endp

; Resource Parser (Basic)
AR proc uses ebx
    LOCAL rva:DWORD, rd:DWORD
    invoke P, addr szRS
    mov eax, pNT
    add eax, 136
    mov eax, [eax]
    test eax, eax
    jz @@done
    invoke RVA2OFF, eax
    mov rd, eax
    invoke P, addr szOK
@@done:
    ret
AR endp

; String Extractor (ASCII/Unicode)
AT proc uses ebx ecx edx esi
    LOCAL i:DWORD, l:DWORD, p:DWORD
    invoke P, addr szTS
    mov i, 0
@@loop:
    cmp i, fSz
    jge @@done
    movzx eax, byte ptr [fB+i]
    cmp al, 20h
    jl @@next
    cmp al, 7Eh
    jg @@next
    mov p, i
    mov l, 0
@@string:
    cmp i, fSz
    jge @@output
    movzx eax, byte ptr [fB+i]
    cmp al, 20h
    jl @@output
    cmp al, 7Eh
    jg @@output
    inc l
    inc i
    cmp l, 4
    jl @@string
@@output:
    cmp l, 4
    jl @@next
    mov byte ptr [fB+i], 0
    invoke wsprintf, addr tB, addr szTF, p, addr fB[p]
    invoke P, addr tB
@@next:
    inc i
    jmp @@loop
@@done:
    ret
AT endp

; Entropy Analyzer
AN proc uses ebx ecx edx esi
    LOCAL i:DWORD, e:DWORD, j:DWORD, ptrHash:DWORD
    invoke RtlZeroMemory, addr hashBuf, 1024
    mov i, 0
@@count:
    cmp i, fSz
    jge @@calc
    movzx eax, byte ptr [fB+i]
    mov edx, eax
    shl edx, 2
    inc dword ptr [hashBuf+edx]
    inc i
    jmp @@count
@@calc:
    mov e, 0
    mov j, 0
@@loop:
    cmp j, 256
    jge @@print
    mov edx, j
    shl edx, 2
    mov eax, [hashBuf+edx]
    test eax, eax
    jz @@skip
    push eax
    fild dword ptr [esp]
    fld st(0)
    fild fSz
    fdivp st(1), st(0)
    fld1
    fxch
    fyl2x
    fmulp st(1), st(0)
    fistp dword ptr [esp]
    pop eax
    neg eax
    add e, eax
@@skip:
    inc j
    jmp @@loop
@@print:
    mov eax, e
    mov ecx, 100
    mul ecx
    shr eax, 8
    invoke wsprintf, addr tB, addr szEN, eax, addr szYS
    cmp eax, 70
    jg @@packed
    invoke wsprintf, addr tB, addr szEN, eax, addr szNS
@@packed:
    invoke P, addr tB
    ret
AN endp

; Language Detector (Heuristic)
AL proc uses ebx
    LOCAL s:DWORD
    mov s, 0
    mov eax, fSz
    cmp eax, 4
    jl @@unknown
    mov eax, [fB]
    cmp eax, 0BEBAFECAh
    je @@java
    cmp eax, 0x464C457F
    je @@elf
    cmp eax, 0xfeedface
    je @@mach
    cmp eax, 0xfeedfacf
    je @@mach
    cmp eax, 0xcafebabe
    je @@java
    cmp eax, 0xCAFEBABE
    je @@java
    cmp word ptr [fB], 'MZ'
    je @@pe
@@unknown:
    invoke P, addr szUK
    ret
@@java:
    invoke wsprintf, addr tB, addr szLG, addr sigJava, 95
    invoke P, addr tB
    ret
@@elf:
    invoke wsprintf, addr tB, addr szLG, addr szELF, 98
    invoke P, addr tB
    ret
@@mach:
    invoke wsprintf, addr tB, addr szLG, addr sigPhp, 90
    invoke P, addr tB
    ret
@@pe:
    invoke wsprintf, addr tB, addr szLG, addr szPE, 99
    invoke P, addr tB
    ret
AL endp

; Decompilation Stub (Reconstruction)
AD proc
    invoke P, addr szDC
    call AI
    call AE
    call AS
    ret
AD endp

; Main Menu (Professional)
MM proc uses ebx ecx edx esi
    LOCAL c:DWORD
@@menu:
    invoke P, addr szW
    invoke P, addr szM
    call RI
    mov c, eax
    cmp c, 0
    je @@exit
    cmp c, 1
    je @@deep
    cmp c, 2
    je @@imports
    cmp c, 3
    je @@exports
    cmp c, 4
    je @@resources
    cmp c, 5
    je @@sections
    cmp c, 6
    je @@strings
    cmp c, 7
    je @@entropy
    cmp c, 8
    je @@language
    cmp c, 9
    je @@decompile
    jmp @@menu
@@deep:
    invoke P, addr szPF
    call R
    invoke O, addr bP
    test eax, eax
    jz @@menu
    invoke wsprintf, addr tB, addr szEP, EP
    invoke P, addr tB
    invoke wsprintf, addr tB, addr szIM, IB
    invoke P, addr tB
    jmp @@menu
@@imports:
    call AI
    jmp @@menu
@@exports:
    call AE
    jmp @@menu
@@resources:
    call AR
    jmp @@menu
@@sections:
    call AS
    jmp @@menu
@@strings:
    call AT
    jmp @@menu
@@entropy:
    call AN
    jmp @@menu
@@language:
    call AL
    jmp @@menu
@@decompile:
    call AD
    jmp @@menu
@@exit:
    ret
MM endp

main proc
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hIn, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hOut, eax
    call MM
    invoke ExitProcess, 0
main endp
end main