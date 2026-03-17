;============================================================================
; DumpBin v6.0E (Elegant) - One-Liner Enhanced
; Ultra-compact universal binary analyzer
;============================================================================

.386
.model flat, stdcall
option casemap:none

; === INCLUDES ===
include \masm32\include\windows.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; === COMPACT DATA ===
.data
szW db "DumpBin v6.0E",13,10,"================",13,10,0
szM db "[1]Analyze [2]Hex [3]Disasm [4]Conv [5]Ext [6]Imp/Exp [7]Head [8]Exit",13,10,">",0
szP db "File: ",0, szA db "Addr: ",0, szS db "Size: ",0, szO db "Out: ",0
szF db "PE",13,10,0, szE db "ELF",13,10,0, szC db "Mach-O",13,10,0, szU db "Unknown",13,10,0
szH db 13,10,"=== HEADER ===",13,10,0, szI db "Imports",13,10,0, szX db "Exports",13,10,0
szB db "%02X ",0, szN db 13,10,0, szD db "%d",0, szG db "%08X: ",0
szR db "[-] Error",13,10,0, szK db "[+] Done",13,10,0
szSec db "Sec %d: VA=%08X VS=%08X Raw=%08X",13,10,0
fB db 104857600 dup(0), hI dd 0, hO dd 0, fS dd 0, bP db 260 dup(0), tB db 512 dup(0)

; === ELEGANT CODE ===
.code
start: 
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hI, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hO, eax
    call Main
    invoke ExitProcess, eax

Print proc s:DWORD
    local w:DWORD
    invoke lstrlen, s
    invoke WriteConsole, hO, s, eax, addr w, 0
    ret
Print endp

Read proc
    local r:DWORD
    invoke ReadConsole, hI, addr bP, 260, addr r, 0
    mov eax, r
    dec eax
    mov byte ptr [bP+eax], 0
    ret
Read endp

GetInt proc
    call Read
    xor eax, eax
    lea esi, bP
@@l:
    movzx ecx, byte ptr [esi]
    cmp cl, 13
    je @@d
    sub cl, '0'
    cmp cl, 9
    ja @@d
    imul eax, 10
    add eax, ecx
    inc esi
    jmp @@l
@@d:
    ret
GetInt endp

GetHex proc
    call Read
    xor eax, eax
    lea esi, bP
@@l:
    movzx ecx, byte ptr [esi]
    cmp cl, 13
    je @@d
    cmp cl, 'a'
    jb @@u
    sub cl, 32
@@u:
    cmp cl, 'A'
    jb @@n
    sub cl, 'A'-10
    jmp @@c
@@n:
    sub cl, '0'
@@c:
    shl eax, 4
    add eax, ecx
    inc esi
    jmp @@l
@@d:
    ret
GetHex endp

Open proc p:DWORD
    invoke CreateFile, p, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0
    cmp eax, -1
    je @@f
    mov hI, eax
    invoke GetFileSize, hI, 0
    mov fS, eax
    cmp eax, 104857600
    jg @@c
    invoke ReadFile, hI, addr fB, fS, addr tB, 0
    push eax
    invoke CloseHandle, hI
    pop eax
    ret
@@c:
    invoke CloseHandle, hI
@@f:
    invoke Print, addr szR
    xor eax, eax
    ret
Open endp

Fmt proc
    cmp fS, 4
    jl @@u
    mov ax, word ptr [fB]
    cmp ax, 'ZM'
    je @@p
    mov al, byte ptr [fB]
    cmp al, 7Fh
    je @@e
    cmp al, 0FEh
    je @@m
@@u:
    invoke Print, addr szU
    xor eax, eax
    ret
@@p:
    cmp fS, 64
    jl @@z
    mov eax, [fB+60]
    cmp eax, fS
    jge @@z
    mov ebx, eax
    mov eax, [fB+ebx]
    cmp eax, 'EP'
    je @@pe
@@z:
    invoke Print, addr szF
    mov eax, 4
    ret
@@pe:
    invoke Print, addr szF
    mov eax, 1
    ret
@@e:
    invoke Print, addr szE
    mov eax, 2
    ret
@@m:
    invoke Print, addr szC
    mov eax, 3
    ret
Fmt endp

DumpPE proc
    invoke Print, addr szH
    mov eax, [fB+60]
    lea ebx, fB
    add eax, ebx
    movzx ecx, word ptr [eax+6]
    push ecx
    invoke wsprintf, addr tB, addr szD, ecx
    invoke Print, addr tB
    pop ecx
    add eax, 24
    movzx edx, word ptr [eax-4]
    add eax, edx
    mov edx, ecx
@@l:
    push edx
    push eax
    mov ebx, [eax+12]
    mov edx, [eax+8]
    mov esi, [eax+20]
    push esi
    push edx
    push ebx
    push edx
    invoke wsprintf, addr tB, addr szSec, edx, ebx, edx, esi
    invoke Print, addr tB
    pop edx
    add eax, 40
    pop edx
    dec edx
    jnz @@l
    ret
DumpPE endp

Hex proc a:DWORD, s:DWORD
    invoke Print, addr szH
    mov esi, a
    mov ecx, s
@@l:
    push ecx
    push esi
    invoke wsprintf, addr tB, addr szG, esi
    invoke Print, addr tB
    mov ecx, 16
@@b:
    cmp esi, fS
    jge @@p
    movzx eax, byte ptr [fB+esi]
    push ecx
    invoke wsprintf, addr tB, addr szB, eax
    invoke Print, addr tB
    pop ecx
    inc esi
    loop @@b
@@p:
    invoke Print, addr szN
    pop esi
    add esi, 16
    pop ecx
    sub ecx, 16
    jg @@l
    ret
Hex endp

Dis proc a:DWORD, s:DWORD
    invoke Print, addr szH
    mov esi, a
    mov ecx, s
@@l:
    push ecx
    push esi
    invoke wsprintf, addr tB, addr szG, esi
    invoke Print, addr tB
    movzx eax, byte ptr [fB+esi]
    invoke wsprintf, addr tB, addr szB, eax
    invoke Print, addr tB
    invoke Print, addr szN
    pop esi
    inc esi
    pop ecx
    loop @@l
    ret
Dis endp

Main proc
@@m:
    invoke Print, addr szW
    invoke Print, addr szM
    call GetInt
    cmp eax, 8
    je @@x
    cmp eax, 1
    je @@1
    cmp eax, 2
    je @@2
    cmp eax, 3
    je @@3
    jmp @@m
@@1:
    invoke Print, addr szP
    call Read
    invoke Open, addr bP
    test eax, eax
    jz @@m
    call Fmt
    cmp eax, 1
    jne @@m
    call DumpPE
    jmp @@m
@@2:
    invoke Print, addr szA
    call GetHex
    push eax
    invoke Print, addr szS
    call GetInt
    pop ebx
    invoke Hex, ebx, eax
    jmp @@m
@@3:
    invoke Print, addr szA
    call GetHex
    push eax
    invoke Print, addr szS
    call GetInt
    pop ebx
    invoke Dis, ebx, eax
    jmp @@m
@@x:
    xor eax, eax
    ret
Main endp

END start
