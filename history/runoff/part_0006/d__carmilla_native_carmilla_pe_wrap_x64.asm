; =============================================================================
; Carmilla PE Wrapper — Pure MASM64
;
; Takes a raw blob (stub PIC shellcode + .car container) and wraps it in a
; minimal PE so Windows will load and execute it.
;
; Usage:
;   carmilla_pe_wrap_x64.exe <input.blob> <output.exe>
;
; Output PE layout:
;   [DOS header + stub]          (64 bytes)
;   [PE signature "PE\0\0"]      (4 bytes)
;   [IMAGE_FILE_HEADER]          (20 bytes)
;   [IMAGE_OPTIONAL_HEADER64]    (240 bytes)
;   [IMAGE_SECTION_HEADER .xdata](40 bytes)
;   [padding to 0x200]
;   [blob bytes at file offset 0x200, RVA 0x1000]
;
; The single section is marked IMAGE_SCN_MEM_EXECUTE | READ | WRITE (E0000020)
; so the stub PIC code can self-modify, resolve APIs, decrypt in-place, etc.
;
; Entry point = RVA 0x1000 (start of section = start of blob)
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

; PE constants
PE_HDR_OFFSET   equ 80h         ; e_lfanew value — where PE sig starts
SECTION_ALIGN   equ 1000h       ; SectionAlignment
FILE_ALIGN      equ 200h        ; FileAlignment
HEADERS_SIZE    equ 200h        ; SizeOfHeaders (file-aligned)
SECTION_RVA     equ 1000h       ; VirtualAddress of .xdata section

.data
align 8

szBanner  db 13,10
          db "  Carmilla PE Wrapper v1.0",13,10
          db "  Wraps raw blob in minimal x64 PE",13,10,13,10,0
szUsage   db "  Usage: carmilla_pe_wrap_x64 <blob> <output.exe>",13,10,0
szWrap    db "  [*] Wrapping ",0
szArrow   db " -> ",0
szDone    db "done",13,10,"  [+] Wrote ",0
szBytes   db " bytes",13,10,0
szCRLF    db 13,10,0
szFail    db "  [!] FATAL: ",0
szErrOpen db "cannot open input file",13,10,0
szErrWrt  db "cannot create output file",13,10,0
szErrMem  db "out of memory",13,10,0

hStdOut   dq 0
pArgIn    dq 0
pArgOut   dq 0
hFileIn   dq 0
dwFileSize dd 0
dwBytesRW dd 0
pBlobBuf  dq 0
pOutBuf   dq 0
numBuf    db 20 dup(0)

; Pre-built PE header template (will be copied and patched)
; This is a minimal 64-bit PE with one section
align 8
peTemplate:
; ── DOS header (64 bytes) ──
    dw 5A4Dh          ; e_magic "MZ"
    dw 0090h          ; e_cblp
    dw 0003h          ; e_cp
    dw 0              ; e_crlc
    dw 0004h          ; e_cparhdr
    dw 0              ; e_minalloc
    dw 0FFFFh         ; e_maxalloc
    dw 0              ; e_ss
    dw 00B8h          ; e_sp
    dw 0              ; e_csum
    dw 0              ; e_ip
    dw 0              ; e_cs
    dw 0040h          ; e_lfarlc
    dw 0              ; e_ovno
    dq 0              ; e_res[4]
    dw 0              ; e_oemid
    dw 0              ; e_oeminfo
    dq 0,0            ; e_res2[10] (20 bytes = 2 qwords + 1 dword)
    dd 0              ; e_res2 remainder
    dd PE_HDR_OFFSET  ; e_lfanew

; Pad to e_lfanew offset (0x80 = 128 bytes from start)
; DOS header is 64 bytes, so we need 64 bytes of padding
    db 64 dup(0)

; ── PE signature (at offset 0x80) ──
    dd 00004550h      ; "PE\0\0"

; ── IMAGE_FILE_HEADER (20 bytes) ──
    dw 8664h          ; Machine = AMD64
    dw 1              ; NumberOfSections
    dd 0              ; TimeDateStamp (will be patched)
    dd 0              ; PointerToSymbolTable
    dd 0              ; NumberOfSymbols
    dw 00F0h          ; SizeOfOptionalHeader (240 bytes for PE32+)
    dw 0022h          ; Characteristics: EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE

; ── IMAGE_OPTIONAL_HEADER64 (240 bytes) ──
    dw 020Bh          ; Magic = PE32+
    db 0,0            ; Linker version
    dd 0              ; SizeOfCode (patched)
    dd 0              ; SizeOfInitializedData
    dd 0              ; SizeOfUninitializedData
    dd SECTION_RVA    ; AddressOfEntryPoint = 0x1000
    dd SECTION_RVA    ; BaseOfCode
    dq 0000000140000000h ; ImageBase
    dd SECTION_ALIGN  ; SectionAlignment
    dd FILE_ALIGN     ; FileAlignment
    dw 6, 0           ; OS version 6.0
    dw 0, 0           ; Image version
    dw 6, 0           ; Subsystem version 6.0
    dd 0              ; Win32VersionValue
pe_SizeOfImage:
    dd 0              ; SizeOfImage (patched)
    dd HEADERS_SIZE   ; SizeOfHeaders
    dd 0              ; CheckSum
    dw 3              ; Subsystem = CONSOLE
    dw 8160h          ; DllCharacteristics: DYNAMIC_BASE|NX_COMPAT|TERMINAL_SERVER_AWARE|HIGH_ENTROPY_VA
    dq 00100000h      ; SizeOfStackReserve
    dq 001000h        ; SizeOfStackCommit
    dq 00100000h      ; SizeOfHeapReserve
    dq 001000h        ; SizeOfHeapCommit
    dd 0              ; LoaderFlags
    dd 16             ; NumberOfRvaAndSizes
    ; Data directories (16 entries × 8 bytes = 128 bytes, all zero)
    dq 0,0,0,0,0,0,0,0  ; Export, Import, Resource, Exception
    dq 0,0,0,0,0,0,0,0  ; Security, Reloc, Debug, Architecture, GlobalPtr, TLS, LoadConfig, BoundImport

; ── IMAGE_SECTION_HEADER (40 bytes) ──
pe_section:
    db ".xdata",0,0   ; Name (8 bytes)
pe_VirtualSize:
    dd 0              ; VirtualSize (patched)
    dd SECTION_RVA    ; VirtualAddress = 0x1000
pe_RawDataSize:
    dd 0              ; SizeOfRawData (patched = file-aligned blob size)
    dd FILE_ALIGN     ; PointerToRawData = 0x200
    dd 0              ; PointerToRelocations
    dd 0              ; PointerToLinenumbers
    dw 0              ; NumberOfRelocations
    dw 0              ; NumberOfLinenumbers
    dd 0E0000020h     ; Characteristics: CODE|EXECUTE|READ|WRITE
peTemplateEnd:

PE_TEMPLATE_SIZE equ peTemplateEnd - peTemplate

.code

StrLen proc
    xor eax, eax
@@: cmp byte ptr [rcx+rax], 0
    je @F
    inc eax
    jmp @B
@@: ret
StrLen endp

ConWrite proc
    push rbx
    push rsi
    sub rsp, 40
    mov rsi, rcx
    call StrLen
    mov r8d, eax
    mov rcx, [hStdOut]
    mov rdx, rsi
    lea r9, [dwBytesRW]
    mov qword ptr [rsp+20h], 0
    call WriteConsoleA
    add rsp, 40
    pop rsi
    pop rbx
    ret
ConWrite endp

ConWriteNum proc
    push rbx
    sub rsp, 32
    lea rbx, [numBuf+19]
    mov byte ptr [rbx], 0
    dec rbx
    mov eax, ecx
    test eax, eax
    jnz @cvt
    mov byte ptr [rbx], '0'
    jmp @print
@cvt:
    mov ecx, 10
@@: xor edx, edx
    div ecx
    add dl, '0'
    mov [rbx], dl
    dec rbx
    test eax, eax
    jnz @B
    inc rbx
@print:
    mov rcx, rbx
    call ConWrite
    add rsp, 32
    pop rbx
    ret
ConWriteNum endp

FatalExit proc
    push rbx
    sub rsp, 32
    mov rbx, rcx
    lea rcx, [szFail]
    call ConWrite
    mov rcx, rbx
    call ConWrite
    mov ecx, 1
    call ExitProcess
    add rsp, 32
    pop rbx
    ret
FatalExit endp

; ── Align up to FILE_ALIGN boundary ──
; ecx = value → eax = aligned up
AlignUp proc
    mov eax, ecx
    add eax, FILE_ALIGN - 1
    and eax, NOT (FILE_ALIGN - 1)
    ret
AlignUp endp

; ── Align up to SECTION_ALIGN boundary ──
AlignUpSection proc
    mov eax, ecx
    add eax, SECTION_ALIGN - 1
    and eax, NOT (SECTION_ALIGN - 1)
    ret
AlignUpSection endp

ParseArgs proc
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 40

    call GetCommandLineA
    mov rsi, rax
    xor r12d, r12d

    ; skip exe name
@@: movzx eax, byte ptr [rsi]
    test al, al
    jz @done
    cmp al, ' '
    je @skipWs
    cmp al, 9
    je @skipWs
    cmp al, '"'
    je @skipQ
    inc rsi
    jmp @B
@skipQ:
    inc rsi
@@: movzx eax, byte ptr [rsi]
    test al, al
    jz @done
    cmp al, '"'
    je @endQ
    inc rsi
    jmp @B
@endQ:
    inc rsi
@skipWs:
@@: movzx eax, byte ptr [rsi]
    cmp al, ' '
    je @sw
    cmp al, 9
    je @sw
    jmp @getArgs
@sw: inc rsi
    jmp @B

@getArgs:
    cmp byte ptr [rsi], 0
    je @done
    ; quoted?
    cmp byte ptr [rsi], '"'
    jne @unq
    inc rsi
    mov rdi, rsi
@@: movzx eax, byte ptr [rsi]
    test al, al
    jz @store
    cmp al, '"'
    je @qend
    inc rsi
    jmp @B
@qend:
    mov byte ptr [rsi], 0
    inc rsi
    jmp @store
@unq:
    mov rdi, rsi
@@: movzx eax, byte ptr [rsi]
    test al, al
    jz @store
    cmp al, ' '
    je @uend
    cmp al, 9
    je @uend
    inc rsi
    jmp @B
@uend:
    mov byte ptr [rsi], 0
    inc rsi

@store:
    cmp r12d, 0
    je @sIn
    cmp r12d, 1
    je @sOut
    jmp @nx
@sIn:  mov [pArgIn], rdi
    jmp @nx
@sOut: mov [pArgOut], rdi
@nx:
    inc r12d
    ; skip whitespace
@@: movzx eax, byte ptr [rsi]
    cmp al, ' '
    je @skw
    cmp al, 9
    je @skw
    jmp @getArgs
@skw: inc rsi
    jmp @B

@done:
    cmp r12d, 2
    jb @badArgs
    add rsp, 40
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
@badArgs:
    lea rcx, [szBanner]
    call ConWrite
    lea rcx, [szUsage]
    call ConWrite
    mov ecx, 1
    call ExitProcess
    add rsp, 40
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ParseArgs endp

main proc
    push rbx
    push r12
    push r13
    push r14
    push rsi
    push rdi
    sub rsp, 88                 ; 8+8*6+88=144, 144%16=0 ✓

    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [hStdOut], rax

    lea rcx, [szBanner]
    call ConWrite

    call ParseArgs

    ; ── Print what we're doing ──
    lea rcx, [szWrap]
    call ConWrite
    mov rcx, [pArgIn]
    call ConWrite
    lea rcx, [szArrow]
    call ConWrite
    mov rcx, [pArgOut]
    call ConWrite
    lea rcx, [szCRLF]
    call ConWrite

    ; ── Read input blob ──
    mov rcx, [pArgIn]
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov dword ptr [rsp+20h], OPEN_EXISTING
    mov dword ptr [rsp+28h], FILE_ATTR_NORM
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE
    je @errOpen
    mov r14, rax

    mov rcx, r14
    xor edx, edx
    call GetFileSize
    mov [dwFileSize], eax
    mov ebx, eax                ; ebx = blob size

    xor ecx, ecx
    lea edx, [ebx+4096]
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @errMem
    mov [pBlobBuf], rax

    mov rcx, r14
    mov rdx, [pBlobBuf]
    mov r8d, ebx
    lea r9, [dwBytesRW]
    mov qword ptr [rsp+20h], 0
    call ReadFile

    mov rcx, r14
    call CloseHandle

    ; ── Calculate sizes ──
    ; File-aligned blob size
    mov ecx, ebx
    call AlignUp
    mov r12d, eax               ; r12d = rawDataSize (file-aligned)

    ; Section-aligned blob size
    mov ecx, ebx
    call AlignUpSection
    mov r13d, eax               ; r13d = virtualSize (section-aligned)

    ; Total output size = HEADERS_SIZE + rawDataSize
    mov eax, HEADERS_SIZE
    add eax, r12d
    mov r14d, eax               ; r14d = total output file size

    ; SizeOfImage = SECTION_RVA + section-aligned blob
    mov eax, SECTION_RVA
    add eax, r13d               ; SizeOfImage

    ; ── Allocate output buffer ──
    xor ecx, ecx
    lea edx, [r14d+4096]
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @errMem
    mov [pOutBuf], rax
    mov rdi, rax

    ; Zero-fill entire output
    push rdi
    xor eax, eax
    mov ecx, r14d
    rep stosb
    pop rdi

    ; ── Copy PE template into output ──
    push rdi
    push rsi
    mov rsi, offset peTemplate
    mov ecx, PE_TEMPLATE_SIZE
    rep movsb
    pop rsi
    pop rdi

    ; ── Patch PE fields ──
    ; SizeOfCode at offset PE_HDR_OFFSET + 4 + 20 + 4 = 0x80 + 4 + 20 + 4 = 0x9C
    ; Actually: PE sig(4) + FileHdr(20) + Magic(2) + LinkerVer(2) + SizeOfCode(4)
    ; OptionalHeader starts at 0x80 + 4 + 20 = 0x98
    ; SizeOfCode is at OptHdr+4 = 0x9C
    mov dword ptr [rdi + 09Ch], r12d          ; SizeOfCode = rawDataSize

    ; SizeOfImage at OptHdr + 56 = 0x98 + 0x38 = 0xD0
    mov eax, SECTION_RVA
    add eax, r13d
    mov dword ptr [rdi + 0D0h], eax           ; SizeOfImage

    ; Section header VirtualSize
    ; Section header starts at 0x80 + 4 + 20 + 240 = 0x178
    ; Name(8) + VirtualSize(4) at 0x178 + 8 = 0x180
    mov dword ptr [rdi + 0180h], ebx          ; VirtualSize = actual blob size

    ; SizeOfRawData at 0x178 + 16 = 0x188
    mov dword ptr [rdi + 0188h], r12d         ; SizeOfRawData = file-aligned

    ; ── Copy blob into section ──
    push rdi
    push rsi
    mov rdi, [pOutBuf]
    add rdi, HEADERS_SIZE       ; file offset 0x200
    mov rsi, [pBlobBuf]
    mov ecx, ebx
    rep movsb
    pop rsi
    pop rdi

    ; ── Write output file ──
    mov rcx, [pArgOut]
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    mov dword ptr [rsp+20h], CREATE_ALWAYS
    mov dword ptr [rsp+28h], FILE_ATTR_NORM
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE
    je @errWrt
    mov rbx, rax

    mov rcx, rbx
    mov rdx, [pOutBuf]
    mov r8d, r14d
    lea r9, [dwBytesRW]
    mov qword ptr [rsp+20h], 0
    call WriteFile

    mov rcx, rbx
    call CloseHandle

    ; ── Print result ──
    lea rcx, [szDone]
    call ConWrite
    mov ecx, r14d
    call ConWriteNum
    lea rcx, [szBytes]
    call ConWrite

    ; cleanup
    mov rcx, [pBlobBuf]
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    mov rcx, [pOutBuf]
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree

    xor ecx, ecx
    call ExitProcess

@errOpen:
    lea rcx, [szErrOpen]
    call FatalExit
@errWrt:
    lea rcx, [szErrWrt]
    call FatalExit
@errMem:
    lea rcx, [szErrMem]
    call FatalExit

    add rsp, 88
    pop rdi
    pop rsi
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
main endp

end
