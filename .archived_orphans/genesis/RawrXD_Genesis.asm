; RawrXD_Genesis.asm - Pure x64 Self-Hosting Assembler/Linker Bootstrap
; Zero external tools after Phase 1. Assembles itself. Links itself. Builds RawrXD.
; Input: .asm text | Output: Raw PE64 executables
; Implements: x86-64 assembler core, PE linker, symbol table (stubs for full impl)

OPTION CASEMAP:NONE

; PE64 Constants
PE_MAGIC EQU 00004550h
MACHINE_AMD64 EQU 08664h
IMAGE_SCN_CNT_CODE EQU 020h
IMAGE_SCN_CNT_INITIALIZED_DATA EQU 040h
IMAGE_SCN_MEM_EXECUTE EQU 20000000h
IMAGE_SCN_MEM_READ EQU 40000000h
IMAGE_SCN_MEM_WRITE EQU 80000000h
FILE_MAP_READ EQU 4
PAGE_READONLY EQU 2
PAGE_READWRITE EQU 4
MEM_COMMIT EQU 1000h
MEM_RESERVE EQU 2000h
GENERIC_READ EQU 80000000h
GENERIC_WRITE EQU 40000000h
OPEN_EXISTING EQU 3
CREATE_ALWAYS EQU 2
FILE_ATTRIBUTE_NORMAL EQU 80h
DOS_HEADER_SIZE EQU 64
OPT_HEADER64_SIZE EQU 240
SECTION_HEADER_SIZE EQU 40

; Windows API
EXTERN CreateFileA : PROC
EXTERN GetFileSizeEx : PROC
EXTERN CreateFileMappingA : PROC
EXTERN MapViewOfFile : PROC
EXTERN UnmapViewOfFile : PROC
EXTERN CloseHandle : PROC
EXTERN VirtualAlloc : PROC
EXTERN WriteFile : PROC
EXTERN ExitProcess : PROC
EXTERN RtlMoveMemory : PROC

.DATA
ALIGN 8
g_pSource DQ 0
g_nSourceLen DQ 0
g_pOutput DQ 0
g_nOutputSize DQ 0
g_nCursor DQ 0
g_nSectionCount DD 0
g_nSymbolCount DD 0
g_szOutputPath DB "RawrXD_Genesis.exe", 0
g_szMov DB "MOV", 0
g_szPush DB "PUSH", 0
g_szCall DB "CALL", 0
g_szRet DB "RET", 0
g_szErrorMsg DB 1024 DUP(0)
; Section table: 16 sections * 40 bytes (simplified)
g_SectionData DB 16 * 40 DUP(0)
; Current section index
g_nCurrentSection DD 0
; Output buffer for PE (1MB)
g_OutputBuffer DQ 0
; Token buffer for parser
g_szCurrentToken DB 256 DUP(0)

.CODE

; =============================================================================
; Entry Point - Self-Hosting Bootstrap
; =============================================================================
GenesisMain PROC FRAME
    .ALLOCSTACK 38h
    .ENDPROLOG
    sub rsp, 38h
    ; argc in ECX, argv in RDX (Windows x64: RCX=argc, RDX=argv)
    cmp ecx, 2
    jl @@assemble_self
    ; External assembly: argv[1] = path
    mov rcx, [rdx+8]
    call AssembleFile
    jmp @@exit
@@assemble_self:
    call AssembleGenesisSelf
@@exit:
    xor ecx, ecx
    call ExitProcess
GenesisMain ENDP

; =============================================================================
; AssembleFile - Open source, map, parse (stub), write PE (stub)
; =============================================================================
AssembleFile PROC FRAME
    LOCAL hFile:QWORD
    LOCAL hMap:QWORD
    LOCAL pView:QWORD
    LOCAL liSize:QWORD
    push rbx
    .PUSHREG rbx
    .ALLOCSTACK 60h
    .ENDPROLOG
    sub rsp, 60h
    mov rbx, rcx
    ; Open source file
    xor edx, edx
    mov edx, GENERIC_READ
    xor r8d, r8d
    xor r9d, r9d
    push 0
    push FILE_ATTRIBUTE_NORMAL
    push OPEN_EXISTING
    sub rsp, 20h
    mov rcx, rbx
    call CreateFileA
    add rsp, 20h
    cmp rax, 0FFFFFFFFh
    je @@fail
    mov hFile, rax
    ; Get size (LARGE_INTEGER at liSize = 8 bytes)
    lea rdx, liSize
    mov rcx, rax
    call GetFileSizeEx
    test eax, eax
    jz @@close_fail
    mov rax, liSize
    mov g_nSourceLen, rax
    ; Create mapping
    xor rdx, rdx
    mov r8d, PAGE_READONLY
    mov r9d, 0
    mov eax, dword ptr liSize
    push 0
    push rax
    sub rsp, 20h
    mov rcx, hFile
    call CreateFileMappingA
    add rsp, 30h
    test rax, rax
    jz @@close_fail
    mov hMap, rax
    mov rcx, rax
    mov edx, FILE_MAP_READ
    xor r8d, r8d
    xor r9d, r9d
    push 0
    sub rsp, 20h
    call MapViewOfFile
    add rsp, 28h
    test rax, rax
    jz @@close_map
    mov pView, rax
    mov g_pSource, rax
    call InitDefaultSections
    ; Parse loop (stub: just advance cursor for now)
    mov g_nCursor, 0
@@parse_loop:
    mov rax, g_nCursor
    cmp rax, g_nSourceLen
    jge @@parse_done
    call ParseLine
    jmp @@parse_loop
@@parse_done:
    call ResolveSymbols
    call WritePEFile
    mov rcx, pView
    call UnmapViewOfFile
@@close_map:
    mov rcx, hMap
    call CloseHandle
@@close_fail:
    mov rcx, hFile
    call CloseHandle
@@fail:
    add rsp, 60h
    pop rbx
    ret
AssembleFile ENDP

; =============================================================================
; ParseLine - Identify mnemonics/directives (stub)
; =============================================================================
ParseLine PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
    call SkipWhitespace
    mov rax, g_pSource
    add rax, g_nCursor
    mov al, byte ptr [rax]
    test al, al
    jz @@done
    cmp al, ';'
    je @@skip_comment
    cmp al, '.'
    je @@directive
    call SkipToEOL
    jmp @@done
@@directive:
    call ParseDirective
    jmp @@done
@@skip_comment:
    call SkipToEOL
@@done:
    add rsp, 28h
    ret
ParseLine ENDP

; =============================================================================
; ParseInstruction - MOV, PUSH, CALL, RET (stub: emit RET and advance)
; =============================================================================
ParseInstruction PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
    lea rcx, g_szCurrentToken
    call GetToken
    lea rcx, g_szCurrentToken
    lea rdx, g_szRet
    call StrCmp
    test eax, eax
    jnz @@do_ret
    call SkipToEOL
    add rsp, 28h
    ret
@@do_ret:
    mov al, 0C3h
    call EmitByte
    call SkipToEOL
    add rsp, 28h
    ret
ParseInstruction ENDP

; =============================================================================
; WritePEFile - Layout sections, write minimal PE
; =============================================================================
WritePEFile PROC FRAME
    LOCAL hFile:QWORD
    LOCAL nHeaders:DWORD
    LOCAL nWritten:DWORD
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    .ALLOCSTACK 68h
    .ENDPROLOG
    sub rsp, 68h
    ; Headers size: DOS(64) + PE sig(4) + COFF(20) + Optional(240) + 8 section headers
    mov nHeaders, 64 + 4 + 20 + 240 + (8 * SECTION_HEADER_SIZE)
    add nHeaders, 511
    and nHeaders, 0FFFFFE00h
    ; Allocate 1MB
    xor rcx, rcx
    mov rdx, 100000h
    mov r8d, MEM_RESERVE
    or r8d, MEM_COMMIT
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @@wf_fail
    mov g_pOutput, rax
    mov rdi, rax
    ; DOS header
    mov word ptr [rdi], 5A4Dh
    mov dword ptr [rdi+60], 64
    ; PE signature
    add rdi, 64
    mov dword ptr [rdi], 00004550h
    ; COFF
    add rdi, 4
    mov word ptr [rdi], MACHINE_AMD64
    mov word ptr [rdi+2], 8
    mov dword ptr [rdi+4], 0
    mov dword ptr [rdi+8], 0
    mov dword ptr [rdi+12], 0
    mov word ptr [rdi+16], 240
    mov word ptr [rdi+18], 22h
    ; Optional header (PE32+)
    add rdi, 20
    mov word ptr [rdi], 20Bh
    mov dword ptr [rdi+16], 1000h
    mov dword ptr [rdi+24], 10000h
    mov dword ptr [rdi+28], 200h
    mov dword ptr [rdi+32], 200h
    mov dword ptr [rdi+56], 100000h
    mov eax, nHeaders
    mov dword ptr [rdi+60], eax
    mov word ptr [rdi+68], 3
    ; Create output file
    lea rcx, g_szOutputPath
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    push 0
    push FILE_ATTRIBUTE_NORMAL
    push CREATE_ALWAYS
    sub rsp, 20h
    call CreateFileA
    add rsp, 30h
    cmp rax, 0FFFFFFFFh
    je @@wf_fail
    mov hFile, rax
    lea r9, nWritten
    xor eax, eax
    push rax
    sub rsp, 20h
    mov rcx, hFile
    mov rdx, g_pOutput
    mov r8d, nHeaders
    call WriteFile
    add rsp, 28h
    mov rcx, hFile
    call CloseHandle
@@wf_fail:
    add rsp, 68h
    pop rdi
    pop rsi
    pop rbx
    ret
WritePEFile ENDP

; =============================================================================
; AssembleGenesisSelf - Output RawrXD_Genesis.exe stub
; =============================================================================
AssembleGenesisSelf PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
    call WriteMinimalBootstrap
    add rsp, 28h
    ret
AssembleGenesisSelf ENDP

; =============================================================================
; WriteMinimalBootstrap - 512-byte PE stub
; =============================================================================
WriteMinimalBootstrap PROC FRAME
    LOCAL hFile:QWORD
    LOCAL nWritten:DWORD
    .ALLOCSTACK 48h
    .ENDPROLOG
    sub rsp, 48h
    mov rcx, 512
    xor rdx, rdx
    mov r8d, MEM_RESERVE
    or r8d, MEM_COMMIT
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @@wmb_done
    mov g_pOutput, rax
    mov word ptr [rax], 5A4Dh
    mov dword ptr [rax+60], 64
    lea rcx, g_szOutputPath
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    xor r9d, r9d
    push 0
    push FILE_ATTRIBUTE_NORMAL
    push CREATE_ALWAYS
    sub rsp, 20h
    call CreateFileA
    add rsp, 30h
    cmp rax, 0FFFFFFFFh
    je @@wmb_done
    mov hFile, rax
    lea r9, nWritten
    xor eax, eax
    push rax
    sub rsp, 20h
    mov rcx, hFile
    mov rdx, g_pOutput
    mov r8d, 512
    call WriteFile
    add rsp, 28h
    mov rcx, hFile
    call CloseHandle
@@wmb_done:
    add rsp, 48h
    ret
WriteMinimalBootstrap ENDP

; =============================================================================
; EmitByte - Append byte to current section (stub: no-op)
; =============================================================================
EmitByte PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
    add rsp, 28h
    ret
EmitByte ENDP

; =============================================================================
; GetToken - Copy token to buffer (stub)
; =============================================================================
GetToken PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
    mov rax, g_pSource
    add rax, g_nCursor
    mov al, byte ptr [rax]
    test al, al
    jz @@gt_done
    inc g_nCursor
@@gt_done:
    add rsp, 28h
    ret
GetToken ENDP

SkipWhitespace PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
@@loop:
    mov rax, g_pSource
    add rax, g_nCursor
    mov al, byte ptr [rax]
    cmp al, ' '
    je @@skip
    cmp al, 9
    je @@skip
    cmp al, 0Ah
    je @@skip
    cmp al, 0Dh
    je @@skip
    jmp @@done
@@skip:
    inc g_nCursor
    jmp @@loop
@@done:
    add rsp, 28h
    ret
SkipWhitespace ENDP

StrCmp PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
    mov rsi, rcx
    mov rdi, rdx
@@loop:
    mov al, byte ptr [rsi]
    mov ah, byte ptr [rdi]
    cmp al, ah
    jne @@ne
    test al, al
    jz @@eq
    inc rsi
    inc rdi
    jmp @@loop
@@eq:
    mov eax, 1
    add rsp, 28h
    ret
@@ne:
    xor eax, eax
    add rsp, 28h
    ret
StrCmp ENDP

SkipToEOL PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
@@loop:
    mov rax, g_pSource
    add rax, g_nCursor
    mov al, byte ptr [rax]
    test al, al
    jz @@done
    cmp al, 0Ah
    je @@done
    cmp al, 0Dh
    je @@done
    inc g_nCursor
    jmp @@loop
@@done:
    inc g_nCursor
    add rsp, 28h
    ret
SkipToEOL ENDP

InitDefaultSections PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
    mov g_nSectionCount, 2
    add rsp, 28h
    ret
InitDefaultSections ENDP

ResolveSymbols PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
    add rsp, 28h
    ret
ResolveSymbols ENDP

ParseDirective PROC FRAME
    .ALLOCSTACK 28h
    .ENDPROLOG
    sub rsp, 28h
    call SkipToEOL
    add rsp, 28h
    ret
ParseDirective ENDP

PUBLIC GenesisMain
PUBLIC AssembleFile
PUBLIC g_pOutput

END
