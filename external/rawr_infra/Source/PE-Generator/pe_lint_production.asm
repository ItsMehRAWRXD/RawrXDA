; =============================================================================
; pe_lint_production.asm
; Minimal PE32+ linter (read-only): validates headers/sections/directories.
; No CRT. Uses kernel32 only.
; Build (example):
;   ml64.exe /c pe_lint_production.asm
;   link.exe /SUBSYSTEM:CONSOLE /ENTRY:main pe_lint_production.obj kernel32.lib
; =============================================================================

OPTION CASEMAP:NONE
OPTION DOTNAME

includelib kernel32.lib

ExitProcess            PROTO :DWORD
GetCommandLineA        PROTO
GetStdHandle           PROTO :DWORD
WriteFile              PROTO :QWORD, :QWORD, :DWORD, :QWORD, :QWORD
CreateFileA            PROTO :QWORD, :DWORD, :DWORD, :QWORD, :DWORD, :DWORD, :QWORD
ReadFile               PROTO :QWORD, :QWORD, :DWORD, :QWORD, :QWORD
GetFileSizeEx          PROTO :QWORD, :QWORD
CloseHandle            PROTO :QWORD
VirtualAlloc           PROTO :QWORD, :QWORD, :DWORD, :DWORD
VirtualFree            PROTO :QWORD, :QWORD, :DWORD

; -----------------------------------------------------------------------------
; Constants
; -----------------------------------------------------------------------------
STD_OUTPUT_HANDLE      EQU -11

GENERIC_READ           EQU 80000000h
FILE_SHARE_READ        EQU 00000001h
OPEN_EXISTING          EQU 00000003h
FILE_ATTRIBUTE_NORMAL  EQU 00000080h

MEM_COMMIT             EQU 00001000h
MEM_RESERVE            EQU 00002000h
MEM_RELEASE            EQU 00008000h
PAGE_READWRITE         EQU 00000004h

PE_DOS_MAGIC            EQU 5A4Dh          ; "MZ"
PE_NT_SIGNATURE         EQU 00004550h      ; "PE\0\0"
PE_MACHINE_AMD64        EQU 8664h
PE_OPTIONAL_MAGIC_PE32P EQU 020Bh          ; PE32+

; IMAGE_OPTIONAL_HEADER64 offsets
IOH_Magic              EQU 00h
IOH_SizeOfCode         EQU 04h
IOH_SizeOfInitData     EQU 08h
IOH_AddressOfEntry     EQU 10h
IOH_ImageBase          EQU 18h
IOH_SectionAlign       EQU 20h
IOH_FileAlign          EQU 24h
IOH_SizeOfImage        EQU 38h
IOH_SizeOfHeaders      EQU 3Ch
IOH_Subsystem          EQU 44h
IOH_DllChars           EQU 46h
IOH_NumberOfRvaSizes   EQU 6Ch
IOH_DataDirectory      EQU 70h

; DataDirectory indices
DIR_IMPORT             EQU 1
DIR_BASERELOC          EQU 5

; IMAGE_SECTION_HEADER size
SEC_HDR_SIZE           EQU 28h

; -----------------------------------------------------------------------------
; Data
; -----------------------------------------------------------------------------
.data
g_hStdout           dq 0
g_bytesWritten      dd 0
g_fileSize          dq 0
g_fileBuf           dq 0
g_exitCode          dd 0

g_pathBuf           db 260 dup(0)

g_hexDigits         db "0123456789ABCDEF", 0
g_tmpHex            db 32 dup(0)

szCRLF              db 13, 10, 0
szUsage             db "Usage: pe_lint_production.exe <file.exe>", 13, 10, 0
szOk                db "OK", 13, 10, 0
szFail              db "FAIL: ", 0

szErrOpen           db "cannot open file", 13, 10, 0
szErrRead           db "cannot read file", 13, 10, 0
szErrSize           db "file too small", 13, 10, 0
szErrMZ             db "bad DOS magic", 13, 10, 0
szErrLfanew         db "bad e_lfanew", 13, 10, 0
szErrPESig          db "bad NT signature", 13, 10, 0
szErrOptMagic       db "not PE32+ optional header", 13, 10, 0
szErrAlign          db "invalid alignment", 13, 10, 0
szErrNoSections     db "NumberOfSections is 0", 13, 10, 0
szErrSubsystem0     db "Subsystem is 0", 13, 10, 0
szErrHdrBounds      db "header table out of bounds", 13, 10, 0
szErrSecBounds      db "section raw data out of bounds", 13, 10, 0
szErrSizeHdr        db "SizeOfHeaders invalid", 13, 10, 0
szErrSizeImg        db "SizeOfImage invalid", 13, 10, 0
szErrEntryOob       db "EntryPoint RVA out of range", 13, 10, 0
szErrEntryNoSec     db "EntryPoint not mapped to a section", 13, 10, 0
szErrImportDir      db "import directory invalid", 13, 10, 0

szDbgFileSize       db "fileSize=", 0
szDbgRawPtr         db "rawPtr=", 0
szDbgRawSize        db "rawSize=", 0
szDbgRawEnd         db "rawEnd=", 0

.code

; -----------------------------------------------------------------------------
; strlenA
; RCX = char*
; RAX = length (bytes, not including NUL)
; -----------------------------------------------------------------------------
strlenA PROC FRAME
    push rdi
    .pushreg rdi
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rdi, rcx
    xor eax, eax
@@:
    cmp byte ptr [rdi+rax], 0
    je  @F
    inc eax
    jmp @B
@@:
    add rsp, 20h
    pop rdi
    ret
strlenA ENDP

; -----------------------------------------------------------------------------
; WriteStdout
; RCX = buffer, EDX = length
; -----------------------------------------------------------------------------
WriteStdout PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    mov r8d, edx
    mov rdx, rcx
    mov rcx, g_hStdout
    lea r9, g_bytesWritten
    mov qword ptr [rsp+20h], 0
    call WriteFile

    add rsp, 28h
    ret
WriteStdout ENDP

; -----------------------------------------------------------------------------
; PrintZ
; RCX = NUL-terminated string
; -----------------------------------------------------------------------------
PrintZ PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rbx, rcx
    call strlenA
    mov edx, eax
    mov rcx, rbx
    call WriteStdout

    add rsp, 20h
    pop rbx
    ret
PrintZ ENDP

; -----------------------------------------------------------------------------
; PrintFail
; RDX = error string (already includes CRLF)
; -----------------------------------------------------------------------------
PrintFail PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    mov rbx, rdx
    lea rcx, szFail
    call PrintZ
    mov rcx, rbx
    call PrintZ
    add rsp, 20h
    pop rbx
    ret
PrintFail ENDP

; -----------------------------------------------------------------------------
; PrintHexU64
; RDX = value
; -----------------------------------------------------------------------------
PrintHexU64 PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rbx, rdx
    lea r8, g_tmpHex

    mov byte ptr [r8+0], '0'
    mov byte ptr [r8+1], 'x'
    lea r9, g_hexDigits
    lea r10, [r8+2]

    mov rax, rbx
    mov ecx, 16
@@:
    mov rdx, rax
    shr rdx, 60
    and edx, 0Fh
    mov dl, byte ptr [r9+rdx]
    mov byte ptr [r10], dl
    inc r10
    shl rax, 4
    dec ecx
    jnz @B

    mov byte ptr [r10+0], 13
    mov byte ptr [r10+1], 10
    mov byte ptr [r10+2], 0

    lea rcx, g_tmpHex
    call PrintZ

    add rsp, 20h
    pop rbx
    ret
PrintHexU64 ENDP

; -----------------------------------------------------------------------------
; IsPowerOfTwoU32
; ECX = value
; EAX = 1 if power-of-two and nonzero, else 0
; -----------------------------------------------------------------------------
IsPowerOfTwoU32 PROC
    test ecx, ecx
    jz   @no
    mov eax, ecx
    dec eax
    and eax, ecx
    setz al
    movzx eax, al
    ret
@no:
    ret
IsPowerOfTwoU32 ENDP

; -----------------------------------------------------------------------------
; AlignUpU32
; EAX = value, ECX = alignment (power of 2)
; EAX = aligned up value
; -----------------------------------------------------------------------------
AlignUpU32 PROC
    dec ecx
    add eax, ecx
    not ecx
    and eax, ecx
    ret
AlignUpU32 ENDP

; -----------------------------------------------------------------------------
; RvaToPtr
; Inputs:
;   ECX = rva
;   RDX = file base
;   R8  = file size (QWORD)
;   R9  = section headers ptr
;   [rsp+28h] = section count (DWORD)
;   [rsp+30h] = SizeOfHeaders (DWORD)
; Returns:
;   RAX = pointer or 0 if unmapped/out-of-bounds
; -----------------------------------------------------------------------------
RvaToPtr PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rbx, rdx        ; base
    mov r12, r8         ; size
    mov rsi, r9         ; sec table
    mov r13d, ecx       ; rva

    mov eax, dword ptr [rsp+80h]    ; SizeOfHeaders
    cmp r13d, eax
    jae @sections

    ; Header region: file offset == rva
    mov eax, r13d
    cmp rax, r12
    jae @fail
    lea rax, [rbx+rax]
    jmp @done

@sections:
    mov edi, dword ptr [rsp+78h]    ; section count
    test edi, edi
    jz   @fail

@loop:
    ; VirtualAddress / sizes / raw mapping
    mov eax, dword ptr [rsi+0Ch]    ; VirtualAddress
    mov edx, dword ptr [rsi+08h]    ; VirtualSize
    mov r9d, dword ptr [rsi+10h]    ; SizeOfRawData
    cmp edx, r9d
    cmovb edx, r9d                  ; size = max(VirtualSize, RawSize)

    cmp r13d, eax
    jb  @next
    lea r10d, [eax+edx]             ; end VA (non-aligned)
    cmp r13d, r10d
    jae @next

    ; file offset = PointerToRawData + (rva - va)
    mov r11d, dword ptr [rsi+14h]   ; PointerToRawData
    mov edx, r13d
    sub edx, eax
    add r11d, edx
    mov eax, r11d
    cmp rax, r12
    jae @fail
    lea rax, [rbx+rax]
    jmp @done

@next:
    add rsi, SEC_HDR_SIZE
    dec edi
    jnz @loop

@fail:
    xor eax, eax

@done:
    add rsp, 20h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RvaToPtr ENDP

; -----------------------------------------------------------------------------
; ValidatePE32Plus
; RCX = file base, RDX = file size (QWORD)
; EAX = 1 ok, 0 fail (prints first failure)
; -----------------------------------------------------------------------------
ValidatePE32Plus PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    .pushreg r14
    .pushreg r15
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    mov rbx, rcx        ; base
    mov r12, rdx        ; size

    cmp r12, 200h
    jb  @fail_size

    mov ax, word ptr [rbx]
    cmp ax, PE_DOS_MAGIC
    jne @fail_mz

    mov r13d, dword ptr [rbx+3Ch]   ; e_lfanew (file offset)
    mov rax, r12
    sub rax, 4 + 14h                ; signature + some header bytes
    cmp r13, rax
    ja  @fail_lfanew

    lea rsi, [rbx+r13]              ; NT signature ptr
    cmp dword ptr [rsi], PE_NT_SIGNATURE
    jne @fail_pesig

    ; IMAGE_FILE_HEADER
    lea rdi, [rsi+4]
    movzx r14d, word ptr [rdi+02h]  ; NumberOfSections
    movzx r15d, word ptr [rdi+10h]  ; SizeOfOptionalHeader

    test r14d, r14d
    jz   @fail_nosec

    ; secTableOff = e_lfanew + 4 + 20 + SizeOfOptionalHeader
    mov eax, r13d
    add eax, 4 + 14h
    add eax, r15d
    mov r13d, eax                   ; r13d = secTableOff

    ; Optional header starts after file header (20 bytes)
    lea rsi, [rdi+14h]
    movzx eax, word ptr [rsi+IOH_Magic]
    cmp ax, PE_OPTIONAL_MAGIC_PE32P
    jne @fail_optmagic

    ; Subsystem must be nonzero for a normal EXE/DLL image.
    movzx eax, word ptr [rsi+IOH_Subsystem]
    test ax, ax
    jz   @fail_subsystem0

    ; Save AddressOfEntryPoint for later section mapping checks.
    mov eax, dword ptr [rsi+IOH_AddressOfEntry]
    mov dword ptr [rsp+00h], eax    ; entryRVA
    mov dword ptr [rsp+04h], 0      ; entryFound

    ; Read alignment values
    mov ecx, dword ptr [rsi+IOH_FileAlign]
    mov edi, ecx                    ; fileAlign
    call IsPowerOfTwoU32
    test eax, eax
    jz   @fail_align

    mov ecx, dword ptr [rsi+IOH_SectionAlign]
    mov r15d, ecx                   ; sectionAlign
    call IsPowerOfTwoU32
    test eax, eax
    jz   @fail_align

    cmp edi, 200h
    jb  @fail_align
    cmp edi, r15d
    ja  @fail_align

    mov eax, r14d
    imul eax, SEC_HDR_SIZE
    add eax, r13d                   ; headersEnd (file offset)
    mov r11d, eax

    cmp r11, r12
    ja  @fail_hdrbounds

    ; Validate SizeOfHeaders
    mov ecx, dword ptr [rsi+IOH_SizeOfHeaders]
    test ecx, ecx
    jz   @fail_sizehdr
    ; must cover the section table end
    cmp ecx, r11d
    jb   @fail_sizehdr
    ; must be file-aligned
    mov eax, ecx
    mov ecx, edi
    call AlignUpU32
    cmp eax, dword ptr [rsi+IOH_SizeOfHeaders]
    jne @fail_sizehdr
    ; must be within file
    cmp dword ptr [rsi+IOH_SizeOfHeaders], r12d
    ja  @fail_sizehdr

    ; Validate section raw bounds and compute max virtual end for SizeOfImage
    xor r9d, r9d                    ; maxEnd
    lea r8, [rbx+r13]               ; sec ptr
    mov ecx, r14d

@sec_loop:
    mov eax, dword ptr [r8+14h]     ; PointerToRawData
    mov edx, dword ptr [r8+10h]     ; SizeOfRawData
    test edx, edx
    jz  @skip_raw

    ; PointerToRawData must be file-aligned
    mov r10d, eax
    test r10d, edi-1
    jnz @fail_secbounds

    mov eax, r10d
    add eax, edx
    cmp rax, r12
    ja  @fail_secbounds

@skip_raw:
    ; virtual end = VA + AlignUp(max(VirtualSize, RawSize), SectionAlignment)
    mov eax, dword ptr [r8+0Ch]     ; VirtualAddress
    mov edx, dword ptr [r8+08h]     ; VirtualSize
    mov r10d, dword ptr [r8+10h]    ; RawSize
    cmp edx, r10d
    cmovb edx, r10d

    ; Track whether AddressOfEntryPoint falls inside this section's RVA span.
    mov r10d, dword ptr [rsp+00h]   ; entryRVA
    cmp r10d, eax
    jb  @entry_skip
    lea r11d, [eax+edx]             ; end RVA (non-aligned)
    cmp r10d, r11d
    jae @entry_skip
    mov dword ptr [rsp+04h], 1
@entry_skip:

    mov r11d, eax                   ; base VA
    add r11d, edx                   ; VA + size
    mov eax, r11d
    mov r10d, ecx                   ; save section loop counter
    mov ecx, r15d
    call AlignUpU32
    mov ecx, r10d                   ; restore section loop counter
    cmp eax, r9d
    cmova r9d, eax

    add r8, SEC_HDR_SIZE
    dec ecx
    jnz @sec_loop

    ; Validate SizeOfImage
    mov eax, dword ptr [rsi+IOH_SizeOfImage]
    test eax, eax
    jz   @fail_sizeimg
    ; must be section-aligned
    mov ecx, r15d
    call AlignUpU32
    cmp eax, dword ptr [rsi+IOH_SizeOfImage]
    jne @fail_sizeimg
    ; must be >= computed max end
    cmp dword ptr [rsi+IOH_SizeOfImage], r9d
    jb  @fail_sizeimg

    ; Validate AddressOfEntryPoint against SizeOfImage and mapping.
    mov eax, dword ptr [rsp+00h]    ; entryRVA
    cmp eax, dword ptr [rsi+IOH_SizeOfImage]
    jae @fail_entry_oob

    mov ecx, dword ptr [rsi+IOH_SizeOfHeaders]
    cmp eax, ecx
    jb  @entry_ok

    cmp dword ptr [rsp+04h], 0
    je  @fail_entry_nosec
@entry_ok:

    ; Validate import directory (if present)
    mov eax, dword ptr [rsi+IOH_NumberOfRvaSizes]
    cmp eax, DIR_IMPORT+1
    jb  @ok

    lea r8, [rsi+IOH_DataDirectory]
    mov ecx, dword ptr [r8 + DIR_IMPORT*8 + 0]  ; Import RVA
    mov r10d, dword ptr [r8 + DIR_IMPORT*8 + 4] ; Import Size
    test ecx, ecx
    jz   @ok
    test r10d, r10d
    jz   @fail_import

    ; Map Import RVA to a file offset and ensure the range exists on disk.
    mov dword ptr [rsp+28h], r14d   ; sec count
    mov r11d, dword ptr [rsi+IOH_SizeOfHeaders]
    mov dword ptr [rsp+30h], r11d   ; SizeOfHeaders
    lea r9, [rbx + r13d]            ; sec table ptr
    call RvaToPtr
    test rax, rax
    jz @fail_import
    mov r9, rbx
    add r9, r12
    lea r11, [rax + r10d]
    cmp r11, r9
    ja @fail_import

@ok:
    mov eax, 1
    jmp @done

@fail_size:
    lea rdx, szErrSize
    call PrintFail
    xor eax, eax
    jmp @done
@fail_mz:
    lea rdx, szErrMZ
    call PrintFail
    xor eax, eax
    jmp @done
@fail_lfanew:
    lea rdx, szErrLfanew
    call PrintFail
    xor eax, eax
    jmp @done
@fail_pesig:
    lea rdx, szErrPESig
    call PrintFail
    xor eax, eax
    jmp @done
@fail_optmagic:
    lea rdx, szErrOptMagic
    call PrintFail
    xor eax, eax
    jmp @done
@fail_align:
    lea rdx, szErrAlign
    call PrintFail
    xor eax, eax
    jmp @done
@fail_nosec:
    lea rdx, szErrNoSections
    call PrintFail
    xor eax, eax
    jmp @done
@fail_subsystem0:
    lea rdx, szErrSubsystem0
    call PrintFail
    xor eax, eax
    jmp @done
@fail_hdrbounds:
    lea rdx, szErrHdrBounds
    call PrintFail
    xor eax, eax
    jmp @done
@fail_secbounds:
    mov r13, r8
    lea rcx, szDbgRawPtr
    call PrintZ
    mov edx, dword ptr [r13+14h]
    call PrintHexU64

    lea rcx, szDbgRawSize
    call PrintZ
    mov edx, dword ptr [r13+10h]
    call PrintHexU64

    lea rcx, szDbgRawEnd
    call PrintZ
    mov eax, dword ptr [r13+14h]
    add eax, dword ptr [r13+10h]
    mov edx, eax
    call PrintHexU64

    lea rdx, szErrSecBounds
    call PrintFail
    xor eax, eax
    jmp @done
@fail_sizehdr:
    lea rdx, szErrSizeHdr
    call PrintFail
    xor eax, eax
    jmp @done
@fail_sizeimg:
    lea rdx, szErrSizeImg
    call PrintFail
    xor eax, eax
    jmp @done
@fail_entry_oob:
    lea rdx, szErrEntryOob
    call PrintFail
    xor eax, eax
    jmp @done
@fail_entry_nosec:
    lea rdx, szErrEntryNoSec
    call PrintFail
    xor eax, eax
    jmp @done
@fail_import:
    lea rdx, szErrImportDir
    call PrintFail
    xor eax, eax

@done:
    add rsp, 20h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ValidatePE32Plus ENDP

; -----------------------------------------------------------------------------
; ParseArg1FromCmdLine
; Returns:
;   RAX = pointer to g_pathBuf, or 0 if missing
; -----------------------------------------------------------------------------
ParseArg1FromCmdLine PROC FRAME
    push rbx
    push rsi
    push rdi
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    sub rsp, 20h
    .allocstack 20h
    .endprolog

    call GetCommandLineA
    mov rsi, rax
    test rsi, rsi
    jz @fail

    ; skip leading spaces
@skip1:
    mov al, byte ptr [rsi]
    cmp al, ' '
    je  @sp1
    cmp al, 9
    jne @exe
@sp1:
    inc rsi
    jmp @skip1

@exe:
    ; skip argv0 (quoted or not)
    cmp byte ptr [rsi], 22h         ; '"'
    jne @exe_unq
    inc rsi
@exe_q:
    mov al, byte ptr [rsi]
    test al, al
    jz @fail
    cmp al, 22h                     ; '"'
    je  @exe_done
    inc rsi
    jmp @exe_q
@exe_done:
    inc rsi
    jmp @after_exe

@exe_unq:
    mov al, byte ptr [rsi]
    test al, al
    jz @fail
    cmp al, ' '
    je  @after_exe
    cmp al, 9
    je  @after_exe
    inc rsi
    jmp @exe_unq

@after_exe:
    ; skip spaces before arg1
@skip2:
    mov al, byte ptr [rsi]
    cmp al, ' '
    je  @sp2
    cmp al, 9
    jne @arg1
@sp2:
    inc rsi
    jmp @skip2

@arg1:
    mov al, byte ptr [rsi]
    test al, al
    jz   @fail

    lea rdi, g_pathBuf
    mov ecx, 259
    xor eax, eax
    rep stosb
    lea rdi, g_pathBuf

    cmp byte ptr [rsi], 22h         ; '"'
    jne @copy_unq
    inc rsi
@copy_q:
    mov al, byte ptr [rsi]
    test al, al
    jz   @done_ok
    cmp al, 22h                     ; '"'
    je  @done_ok
    mov byte ptr [rdi], al
    inc rdi
    inc rsi
    jmp @copy_q

@copy_unq:
    mov al, byte ptr [rsi]
    test al, al
    jz   @done_ok
    cmp al, ' '
    je  @done_ok
    cmp al, 9
    je  @done_ok
    mov byte ptr [rdi], al
    inc rdi
    inc rsi
    jmp @copy_unq

@done_ok:
    lea rax, g_pathBuf
    cmp byte ptr [rax], 0
    jne @done

@fail:
    xor eax, eax

@done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
ParseArg1FromCmdLine ENDP

; -----------------------------------------------------------------------------
; ReadFileToMem
; RCX = path (char*)
; RDX = outSize (QWORD*)
; Returns RAX = buffer ptr or 0
; -----------------------------------------------------------------------------
ReadFileToMem PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    .pushreg r12
    .pushreg r13
    sub rsp, 40h
    .allocstack 40h
    .endprolog

    mov rbx, rcx        ; path
    mov r12, rdx        ; outSize*

    ; h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
    mov rcx, rbx
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9, r9
    mov dword ptr [rsp+20h], OPEN_EXISTING
    mov dword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, -1
    je  @open_fail
    mov r13, rax

    ; GetFileSizeEx(h, &size)
    lea rdx, [rsp+38h]
    mov rcx, r13
    call GetFileSizeEx
    test eax, eax
    jz  @read_fail

    mov rax, qword ptr [rsp+38h]
    test rax, rax
    jz  @read_fail
    mov rdx, rax
    shr rdx, 32
    test edx, edx                   ; arbitrary <4GB guard
    jnz @read_fail

    ; alloc = VirtualAlloc(NULL, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
    xor rcx, rcx
    mov rdx, rax
    mov r8d, MEM_COMMIT or MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz  @read_fail
    mov rdi, rax

    ; ReadFile(h, alloc, size32, &read, NULL) - size is guarded to <4GB, so truncate
    mov rcx, r13
    mov rdx, rdi
    mov eax, dword ptr [rsp+38h]
    mov r8d, eax
    lea r9, [rsp+34h]
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz  @read_fail_free

    mov eax, dword ptr [rsp+34h]
    cmp eax, dword ptr [rsp+38h]
    jne @read_fail_free

    ; close handle
    mov rcx, r13
    call CloseHandle

    ; store outSize and return alloc
    mov rax, qword ptr [rsp+38h]
    mov qword ptr [r12], rax
    mov rax, rdi
    jmp @done

@open_fail:
    lea rdx, szErrOpen
    call PrintFail
    xor eax, eax
    jmp @done

@read_fail_free:
    mov rcx, rdi
    xor rdx, rdx
    mov r8d, MEM_RELEASE
    call VirtualFree
@read_fail:
    mov rcx, r13
    call CloseHandle
    lea rdx, szErrRead
    call PrintFail
    xor eax, eax
    jmp @done

@done:
    add rsp, 40h
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ReadFileToMem ENDP

; -----------------------------------------------------------------------------
; main
; -----------------------------------------------------------------------------
main PROC FRAME
    push rbx
    push rsi
    push rdi
    .pushreg rbx
    .pushreg rsi
    .pushreg rdi
    sub rsp, 30h
    .allocstack 30h
    .endprolog

    ; Initialize state
    mov qword ptr [g_fileBuf], 0
    mov qword ptr [g_fileSize], 0
    mov qword ptr [g_hStdout], 0
    mov dword ptr [g_exitCode], 1
    
    ; Get stdout handle
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov g_hStdout, rax
    cmp rax, -1
    je @exit_no_stdout

    ; Parse command line argument
    call ParseArg1FromCmdLine
    test rax, rax
    jnz @have_arg

    ; No argument provided - show usage
    lea rcx, szUsage
    call PrintZ
    mov dword ptr [g_exitCode], 2
    jmp @exit

@have_arg:
    ; Save path pointer
    mov rbx, rax
    
    ; Read file into memory
    lea rdx, g_fileSize
    mov rcx, rbx
    call ReadFileToMem
    test rax, rax
    jz @exit
    
    ; Store buffer and verify allocation
    mov g_fileBuf, rax
    mov rsi, rax
    mov rdi, qword ptr [g_fileSize]
    
    ; Safety check: verify buffer is readable
    test rsi, rsi
    jz @exit
    test rdi, rdi
    jz @cleanup_and_exit

    ; Validate PE32+ structure
    mov rcx, rsi
    mov rdx, rdi
    call ValidatePE32Plus
    test eax, eax
    jz  @cleanup_and_exit

    ; Validation passed - print success
    lea rcx, szOk
    call PrintZ
    mov dword ptr [g_exitCode], 0

@cleanup_and_exit:
    ; Free allocated memory if present
    mov rbx, qword ptr [g_fileBuf]
    test rbx, rbx
    jz @exit
    
    mov rcx, rbx
    xor rdx, rdx
    mov r8d, MEM_RELEASE
    call VirtualFree
    
    ; Clear buffer pointer
    mov qword ptr [g_fileBuf], 0
    mov qword ptr [g_fileSize], 0

@exit:
    ; Exit with appropriate code
    mov ecx, dword ptr [g_exitCode]
    add rsp, 30h
    pop rdi
    pop rsi
    pop rbx
    call ExitProcess
    
@exit_no_stdout:
    ; Fatal error: no stdout available
    mov ecx, 3
    add rsp, 30h
    pop rdi
    pop rsi
    pop rbx
    call ExitProcess
    
    ; Should never reach here
    int 3
main ENDP

END
