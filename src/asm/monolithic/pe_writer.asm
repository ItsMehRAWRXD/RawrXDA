; ────────────────────────────────────────────────────────────────
; PE_WRITER.ASM — Non-Stubbed x64 PE32+ Emitter
; (C) 2024 GitHub Copilot. Backend Designer Edition.
; ────────────────────────────────────────────────────────────────

; Internal State
PUBLIC SavePEToDisk
PUBLIC g_cursor
PUBLIC g_dataCursor

EXTERN g_peBuffer:BYTE             ; 8192-byte buffer defined in ui.asm
EXTERN g_peSize:QWORD              ; actual PE size set by WritePEFile in ui.asm

.data
align 8
g_cursor     dq 0
g_dataCursor dq 0               ; Points to current head of .data section
szPayloadMessage db "Sovereign Link Active",0
szPath DW 'o','u','t','p','u','t','.','e','x','e',0

.CODE

; Structural constants
IMAGE_DOS_SIGNATURE         EQU 5A4Dh
IMAGE_NT_SIGNATURE          EQU 00004550h
IMAGE_FILE_MACHINE_AMD64    EQU 8664h
IMAGE_NT_OPTIONAL_HDR64_MAGIC EQU 20Bh
SECTION_ALIGNMENT           EQU 1000h
FILE_ALIGNMENT              EQU 200h

; ────────────────────────────────────────────────────────────────
; Emit_Byte — Simple byte emitter
; ────────────────────────────────────────────────────────────────
Emit_Byte PROC
    mov     rax, g_cursor
    mov     byte ptr [rax], cl
    inc     g_cursor
    ret
Emit_Byte ENDP

; ────────────────────────────────────────────────────────────────
; Emit_DOSHeader — Generate MZ & DOS stub
; ────────────────────────────────────────────────────────────────
Emit_DOSHeader PROC
    mov     rax, g_cursor
    mov     word ptr [rax], IMAGE_DOS_SIGNATURE
    mov     word ptr [rax + 3Ch], 80h       ; e_lfanew
    add     g_cursor, 80h
    ret
Emit_DOSHeader ENDP

; ────────────────────────────────────────────────────────────────
; Emit_NTHeaders — Generate PE Signature, FileHeader, OptionalHeader
; ────────────────────────────────────────────────────────────────
Emit_NTHeaders PROC
    mov     rax, g_cursor
    
    ; PE Signature
    mov     dword ptr [rax], IMAGE_NT_SIGNATURE
    add     rax, 4
    
    ; File Header
    mov     word ptr [rax], IMAGE_FILE_MACHINE_AMD64
    mov     word ptr [rax + 2], 4           ; 4 Sections (.text, .data, .idata, .reloc)
    mov     word ptr [rax + 10h], 0F0h      ; Size of Optional Header
    mov     word ptr [rax + 12h], 22h       ; Characteristics: EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
    add     rax, 14h
    
    ; Optional Header
    mov     word ptr [rax], IMAGE_NT_OPTIONAL_HDR64_MAGIC
    mov     dword ptr [rax + 10h], 1000h    ; EntryPoint (RVA)
    
    ; ImageBase (140000000h)
    mov     rcx, 140000000h
    mov     qword ptr [rax + 18h], rcx
    
    mov     dword ptr [rax + 20h], SECTION_ALIGNMENT
    mov     dword ptr [rax + 24h], FILE_ALIGNMENT
    mov     word ptr [rax + 2Ch], 6          ; MajorSubsystemVersion
    mov     dword ptr [rax + 38h], 5000h    ; SizeOfImage (5 pages: HDR+.text+.data+.idata+.reloc)
    mov     dword ptr [rax + 3Ch], 200h     ; SizeOfHeaders
    mov     word ptr [rax + 44h], 3          ; Subsystem: Console
    
    mov     dword ptr [rax + 6Ch], 16        ; NumberOfRvaAndSizes
    
    ; Data Directories (Import Table at index 1)
    lea     rcx, [rax + 70h]
    mov     dword ptr [rcx + 8], 3000h      ; Import Table RVA
    mov     dword ptr [rcx + 0Ch], 100h     ; Import Table Size
    
    ; Section 5: Relocation Directory (Index 5)
    mov     dword ptr [rcx + 28h], 4000h    ; Relocation Table RVA
    mov     dword ptr [rcx + 2Ch], 100h     ; Size
    
    add     g_cursor, 108h
    ret
Emit_NTHeaders ENDP

; ────────────────────────────────────────────────────────────────
; Emit_SectionHeaders — .text, .data, .idata
; ────────────────────────────────────────────────────────────────
Emit_SectionHeaders PROC
    mov     rax, g_cursor
    
    ; .text (RVA 1000h, FileOffset 400h)
    mov     rcx, 000000747865742Eh           ; ".text"
    mov     qword ptr [rax], rcx
    mov     dword ptr [rax + 8], 1000h
    mov     dword ptr [rax + 0Ch], 1000h
    mov     dword ptr [rax + 10h], 200h
    mov     dword ptr [rax + 14h], 400h
    mov     dword ptr [rax + 24h], 60000020h ; RX
    
    ; .data (RVA 2000h, FileOffset 600h)
    add     rax, 28h
    mov     rcx, 00000000617461642Eh         ; ".data"
    mov     qword ptr [rax], rcx
    mov     dword ptr [rax + 8], 1000h
    mov     dword ptr [rax + 0Ch], 2000h
    mov     dword ptr [rax + 10h], 200h
    mov     dword ptr [rax + 14h], 600h
    mov     dword ptr [rax + 24h], 0C0000040h ; RW
    
    ; .idata (RVA 3000h, FileOffset 800h)
    add     rax, 28h
    mov     rcx, 000061746164692Eh           ; ".idata"
    mov     qword ptr [rax], rcx
    mov     dword ptr [rax + 8], 1000h
    mov     dword ptr [rax + 0Ch], 3000h
    mov     dword ptr [rax + 10h], 200h
    mov     dword ptr [rax + 14h], 800h
    mov     dword ptr [rax + 24h], 0C0000040h ; RW
    
    ; .reloc (RVA 4000h, FileOffset 0A00h)
    add     rax, 28h
    mov     rcx, 000000636F6C65722Eh         ; ".reloc"
    mov     qword ptr [rax], rcx
    mov     dword ptr [rax + 8], 1000h
    mov     dword ptr [rax + 0Ch], 4000h
    mov     dword ptr [rax + 10h], 200h
    mov     dword ptr [rax + 14h], 0A00h
    mov     dword ptr [rax + 24h], 42000040h ; R | DISCARDABLE | DATA
    
    add     g_cursor, 0A0h                   ; 4 sections
    ret
Emit_SectionHeaders ENDP

; ────────────────────────────────────────────────────────────────
; Emit_FunctionPrologue — Standard x64 frame: push rbp; mov rbp, rsp; sub rsp, 20h
; ────────────────────────────────────────────────────────────────
Emit_FunctionPrologue PROC
    mov     rax, g_cursor
    mov     byte ptr [rax], 55h          ; push rbp
    mov     byte ptr [rax + 1], 48h      ; REX.W
    mov     byte ptr [rax + 2], 89h      ; mov rbp, rsp
    mov     byte ptr [rax + 3], 0E5h
    mov     byte ptr [rax + 4], 48h      ; REX.W
    mov     byte ptr [rax + 5], 83h      ; sub rsp, 20h
    mov     byte ptr [rax + 6], 0ECh
    mov     byte ptr [rax + 7], 20h
    add     g_cursor, 8
    ret
Emit_FunctionPrologue ENDP

; ────────────────────────────────────────────────────────────────
; Emit_FunctionEpilogue — Standard x64 teardown: mov rsp, rbp; pop rbp; ret
; (Reverse engineer spec: backend designer)
; ────────────────────────────────────────────────────────────────
Emit_FunctionEpilogue PROC
    mov     rax, g_cursor
    mov     byte ptr [rax], 48h          ; REX.W
    mov     byte ptr [rax + 1], 89h      ; mov rsp, rbp
    mov     byte ptr [rax + 2], 0ECh
    mov     byte ptr [rax + 3], 5Dh      ; pop rbp
    mov     byte ptr [rax + 4], 0C3h     ; ret
    add     g_cursor, 5
    ret
Emit_FunctionEpilogue ENDP

; ────────────────────────────────────────────────────────────────
; Emit_ImportTable — Non-stubbed IAT/ILT
; ────────────────────────────────────────────────────────────────
Emit_ImportTable PROC
    ; Assuming .idata FileOffset = 800h
    lea     rax, [g_peBuffer]
    add     rax, 800h
    
    ; IDT Entry (KERNEL32.DLL)
    mov     dword ptr [rax], 3028h          ; ILT RVA
    mov     dword ptr [rax + 0Ch], 3050h    ; Name RVA ("KERNEL32.DLL")
    mov     dword ptr [rax + 10h], 3040h    ; IAT RVA
    
    ; IDT Entry (USER32.DLL)
    lea     rax, [g_peBuffer]
    add     rax, 814h                       ; 2nd IDT entry
    mov     dword ptr [rax], 30A0h          ; ILT RVA (USER32)
    mov     dword ptr [rax + 0Ch], 30C0h    ; Name RVA ("USER32.DLL")
    mov     dword ptr [rax + 10h], 30B0h    ; IAT RVA (USER32)

    ; ILT/IAT Entry (ExitProcess)
    lea     rax, [g_peBuffer]
    add     rax, 828h                       ; ILT
    mov     qword ptr [rax], 3060h
    
    lea     rax, [g_peBuffer]
    add     rax, 840h                       ; IAT
    mov     qword ptr [rax], 3060h

    ; ILT/IAT Entry (MessageBoxA)
    lea     rax, [g_peBuffer]
    add     rax, 8A0h                       ; ILT USER32
    mov     qword ptr [rax], 30D0h
    
    lea     rax, [g_peBuffer]
    add     rax, 8B0h                       ; IAT USER32
    mov     qword ptr [rax], 30D0h
    
    ; Names
    lea     rax, [g_peBuffer]
    add     rax, 850h                       ; "KERNEL32.DLL"
    mov     dword ptr [rax], "NREK"         ; KERN
    mov     dword ptr [rax + 4], "23LE"     ; EL32
    mov     dword ptr [rax + 8], "LLD."     ; .DLL
    
    lea     rax, [g_peBuffer]
    add     rax, 860h                       ; "ExitProcess"
    mov     word ptr [rax], 0
    mov     dword ptr [rax + 2], "tixE"
    mov     dword ptr [rax + 6], "corP"
    mov     dword ptr [rax + 10], "sse"

    lea     rax, [g_peBuffer]
    add     rax, 8C0h                       ; "USER32.DLL"
    mov     dword ptr [rax], "RESU"         ; USER
    mov     dword ptr [rax + 4], "23"       ; 32
    mov     dword ptr [rax + 6], ".DLL"

    lea     rax, [g_peBuffer]
    add     rax, 8D0h                       ; "MessageBoxA"
    mov     word ptr [rax], 0
    mov     dword ptr [rax + 2], "sseM"
    mov     dword ptr [rax + 6], "eBga"
    mov     dword ptr [rax + 10], "xo"      ; Box
    mov     word ptr [rax + 12], "A"        ; A

    ; --- Resolver Logic for Dynamic API Resolution ---
    lea     rax, [g_peBuffer]
    add     rax, 880h                       ; "GetProcAddress"
    mov     word ptr [rax], 0
    mov     dword ptr [rax + 2], "PteG"
    mov     dword ptr [rax + 6], "Acor"
    mov     dword ptr [rax + 10], "erdd"
    mov     word ptr [rax + 14], "ss"

    ret
Emit_ImportTable ENDP

; ────────────────────────────────────────────────────────────────
; Emit_RelocTable — Base Relocations for ASLR
; ────────────────────────────────────────────────────────────────
Emit_RelocTable PROC
    ; .reloc FileOffset = 0A00h (assuming 4th section)
    lea     rax, [g_peBuffer]
    add     rax, 0A00h
    
    ; Block Header (1000h Page)
    mov     dword ptr [rax], 1000h          ; Page RVA
    mov     dword ptr [rax + 4], 0Ch        ; Block Size (8 bytes header + 2 entries * 2 bytes)
    
    ; Entries
    ; Type 10 (IMAGE_REL_BASED_DIR64) | Offset 0
    mov     word ptr [rax + 8], 0A000h      ; 10 << 12 | 000
    ; Null terminator within block
    mov     word ptr [rax + 0Ah], 0
    
    ret
Emit_RelocTable ENDP

; ────────────────────────────────────────────────────────────────
; Emit_CallIndirect — call qword ptr [REG]
; ────────────────────────────────────────────────────────────────
Emit_CallIndirect PROC
    ; call r8 = FF D0 + (r8 index = 0)
    ; In x64: FF D0 (call rax), FF D1 (call rcx), FF D2 (call rdx)
    ; if we pass reg index in r9d (0=rax, 1=rcx, 2=rdx, 3=rbx, 4=rsp, 5=rbp, 6=rsi, 7=rdi, 8=r8...)
    mov     rax, g_cursor
    mov     byte ptr [rax], 0FFh
    mov     dl, r9b
    or      dl, 0D0h
    mov     byte ptr [rax + 1], dl
    add     g_cursor, 2
    ret
Emit_CallIndirect ENDP

; ────────────────────────────────────────────────────────────────
; Machine Code Emitters — Thinking like a Backend Designer
; ────────────────────────────────────────────────────────────────

; Emit_CallImport: call qword ptr [rip+disp32]  — FF 15 [rel32]
; r8d = target IAT RVA (e.g. 3040h for ExitProcess, 30B0h for MessageBoxA)
; Fully dynamic: computes RIP-relative displacement from current g_cursor.
Emit_CallImport PROC
    mov     r10, g_cursor                   ; r10 = cursor ptr (preserved across lea)
    mov     byte ptr [r10], 0FFh
    mov     byte ptr [r10 + 1], 15h

    ; nextInstrRVA = 1000h + (cursor - g_peBuffer - 400h) + 6
    ; disp32 = targetIAT_RVA - nextInstrRVA
    lea     rax, [g_peBuffer]               ; rax = buffer base
    mov     rcx, r10
    sub     rcx, rax                        ; rcx = cursor offset from buf start
    sub     rcx, 400h                       ; rcx = offset within .text section
    add     rcx, 1000h                      ; rcx = RVA of this instruction
    add     rcx, 6                          ; rcx = RVA of next instruction
    mov     edx, r8d                        ; edx = target IAT RVA (caller-supplied)
    sub     edx, ecx                        ; edx = RIP-relative displacement
    mov     dword ptr [r10 + 2], edx        ; patch displacement bytes
    add     g_cursor, 6
    ret
Emit_CallImport ENDP

; Emit_MovRCX_Imm32: mov ecx, imm32
; Opcode: B9 [imm32]
Emit_MovRCX_Imm32 PROC
    ; ecx passed in r8d
    mov     rax, g_cursor
    mov     byte ptr [rax], 0B9h
    mov     dword ptr [rax + 1], r8d
    add     g_cursor, 5
    ret
Emit_MovRCX_Imm32 ENDP

; Emit_LeaRAX_RIP_Rel: lea rax, [rip + displacement]
; Opcode: 48 8D 05 [disp32]
Emit_LeaRAX_RIP_Rel PROC
    ; disp32 passed in r8d
    mov     rax, g_cursor
    mov     byte ptr [rax], 48h
    mov     byte ptr [rax + 1], 8Dh
    mov     byte ptr [rax + 2], 05h
    mov     dword ptr [rax + 3], r8d
    add     g_cursor, 7
    ret
Emit_LeaRAX_RIP_Rel ENDP

; Emit_XorRCX_RCX: xor rcx, rcx
; Opcode: 48 31 C9
Emit_XorRCX_RCX PROC
    mov     rax, g_cursor
    mov     byte ptr [rax], 48h
    mov     byte ptr [rax + 1], 31h
    mov     byte ptr [rax + 2], 0C9h
    add     g_cursor, 3
    ret
Emit_XorRCX_RCX ENDP

; --- Branching & Logic Emitters ---

; Emit_CMP_RAX_RCX: cmp rax, rcx
; Opcode: 48 39 C8
Emit_CMP_RAX_RCX PROC
    mov     rax, g_cursor
    mov     byte ptr [rax], 48h
    mov     byte ptr [rax + 1], 39h
    mov     byte ptr [rax + 2], 0C8h
    add     g_cursor, 3
    ret
Emit_CMP_RAX_RCX ENDP

; --- Data Section Emitters & RVA Fixups (Option A) ---

; Emit_Data_Bytes: Write raw bytes to .data section
; rcx = pData, edx = length
Emit_Data_Bytes PROC
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     rdi, g_dataCursor
    mov     ecx, edx
    rep     movsb
    mov     g_dataCursor, rdi           ; Advance data cursor
    pop     rdi
    pop     rsi
    ret
Emit_Data_Bytes ENDP

; Emit_LeaRCX_RVA: lea rcx, [rip + displacement]  — 48 8D 0D [disp32]
; r8d = offset within .data section (RVA = 2000h + r8d)
; Computes RIP-relative displacement dynamically (backend designer style).
Emit_LeaRCX_RVA PROC
    mov     r10, g_cursor               ; r10 = cursor ptr (saved before rax is clobbered)
    mov     byte ptr [r10], 48h         ; REX.W
    mov     byte ptr [r10 + 1], 8Dh    ; LEA opcode
    mov     byte ptr [r10 + 2], 0Dh    ; ModRM: rcx, [rip+disp32]

    ; nextInstrRVA = 1000h + (cursor - g_peBuffer - 400h) + 7
    ; disp32 = targetDataRVA - nextInstrRVA
    lea     rax, [g_peBuffer]           ; rax = buffer base (clobbers saved-rax intent)
    mov     rcx, r10
    sub     rcx, rax
    sub     rcx, 400h                   ; offset within .text section
    add     rcx, 1000h                  ; RVA of this instruction
    add     rcx, 7                      ; RVA of next instruction

    mov     r9d, 2000h
    add     r9d, r8d                    ; target .data RVA
    sub     r9d, ecx                    ; RIP-relative displacement
    mov     dword ptr [r10 + 3], r9d   ; patch displacement at cursor+3 (NOT buf+3)
    add     g_cursor, 7
    ret
Emit_LeaRCX_RVA ENDP

; Emit_JZ_Rel8: jz short offset
; Opcode: 74 [rel8]
Emit_JZ_Rel8 PROC
    ; offset passed in r8b
    mov     rax, g_cursor
    mov     byte ptr [rax], 74h
    mov     byte ptr [rax + 1], r8b
    add     g_cursor, 2
    ret
Emit_JZ_Rel8 ENDP

; Emit_JNZ_Rel8: jnz short offset
; Opcode: 75 [rel8]
Emit_JNZ_Rel8 PROC
    ; offset passed in r8b
    mov     rax, g_cursor
    mov     byte ptr [rax], 75h
    mov     byte ptr [rax + 1], r8b
    add     g_cursor, 2
    ret
Emit_JNZ_Rel8 ENDP

; Emit_JMP_Rel8: jmp short offset
; Opcode: EB [rel8]
Emit_JMP_Rel8 PROC
    ; offset passed in r8b
    mov     rax, g_cursor
    mov     byte ptr [rax], 0EBh
    mov     byte ptr [rax + 1], r8b
    add     g_cursor, 2
    ret
Emit_JMP_Rel8 ENDP

; Emit_XorEAX_EAX: xor eax, eax
; Opcode: 31 C0
Emit_XorEAX_EAX PROC
    mov     rax, g_cursor
    mov     word ptr [rax], 0C031h
    add     g_cursor, 2
    ret
Emit_XorEAX_EAX ENDP

; ────────────────────────────────────────────────────────────────
; Emit_Payload — Emits a complete PE32+ .text program:
;   MessageBoxA(NULL, "Sovereign Link Active", same_caption, MB_OK)
;   ExitProcess(0)
; Uses Emit_* helpers + inline encoding for full correctness.
; All RIP-relative displacements are computed dynamically.
; ────────────────────────────────────────────────────────────────
Emit_Payload PROC
    ; ── 1. Write "Sovereign Link Active\0" into .data at FileOffset 0x600 (RVA 0x2000) ──
    lea     rax, [g_peBuffer]
    add     rax, 600h               ; .data file offset
    mov     g_dataCursor, rax
    lea     rcx, [szPayloadMessage]
    mov     edx, 23                 ; len("Sovereign Link Active") + null = 23 bytes
    call    Emit_Data_Bytes

    ; ── 2. Set .text cursor to FileOffset 0x400 (RVA 0x1000) ──
    lea     rax, [g_peBuffer]
    add     rax, 400h
    mov     g_cursor, rax

    ; sub rsp, 28h
    call    Emit_FunctionPrologue

    ; xor ecx, ecx   (hWnd = NULL)  — 48 31 C9
    call    Emit_XorRCX_RCX

    ; lea rdx, [rip+disp]  (lpText → "Sovereign Link Active")  — 48 8D 15 [disp32]
    mov     r10, g_cursor
    mov     byte ptr [r10],     48h     ; REX.W
    mov     byte ptr [r10 + 1], 8Dh    ; LEA
    mov     byte ptr [r10 + 2], 15h    ; ModRM: rdx,[rip+disp32]
    lea     rax, [g_peBuffer]
    mov     rcx, r10
    sub     rcx, rax
    sub     rcx, 400h
    add     rcx, 1000h
    add     rcx, 7
    mov     edx, 2000h                  ; .data RVA
    sub     edx, ecx                    ; displacement
    mov     dword ptr [r10 + 3], edx
    add     g_cursor, 7

    ; mov r8, rdx  (lpCaption = same string)  — 4D 8B C2
    mov     r10, g_cursor
    mov     byte ptr [r10],     4Dh     ; REX.WR
    mov     byte ptr [r10 + 1], 8Bh    ; MOV r64,r/m64
    mov     byte ptr [r10 + 2], 0C2h   ; ModRM: r8 ← rdx
    add     g_cursor, 3

    ; xor r9d, r9d  (uType = MB_OK = 0)  — 45 31 C9
    mov     r10, g_cursor
    mov     byte ptr [r10],     45h     ; REX.R+B
    mov     byte ptr [r10 + 1], 31h    ; XOR r/m32, r32
    mov     byte ptr [r10 + 2], 0C9h   ; ModRM: r9d, r9d
    add     g_cursor, 3

    ; call [MessageBoxA IAT at RVA 30B0h]  — FF 15 [disp32]
    mov     r8d, 30B0h
    call    Emit_CallImport

    ; xor ecx, ecx  (ExitProcess exit code = 0)
    call    Emit_XorRCX_RCX

    ; call [ExitProcess IAT at RVA 3040h]  — FF 15 [disp32]
    mov     r8d, 3040h
    call    Emit_CallImport

    ; add rsp, 28h; ret  (epilogue — defensive, ExitProcess doesn't return)
    call    Emit_FunctionEpilogue
    ret
Emit_Payload ENDP

; ────────────────────────────────────────────────────────────────
; WritePEFile — Fully dynamic PE generation
; ────────────────────────────────────────────────────────────────
PUBLIC WritePEFile
WritePEFile PROC FRAME
    push    rbp
    .pushreg rbp
    push    rdi
    .pushreg rdi
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; Zero out the PE buffer to ensure clean slate
    lea     rdi, [g_peBuffer]
    mov     rcx, 8192
    xor     eax, eax
    rep stosb

    ; Initialize general cursor
    lea     rax, [g_peBuffer]
    mov     [g_cursor], rax

    ; Build PE Components
    call    Emit_DOSHeader
    call    Emit_NTHeaders
    call    Emit_SectionHeaders
    call    Emit_Payload
    call    Emit_ImportTable
    call    Emit_RelocTable

    ; Ensure actual PE size covers .reloc down to 0xC00
    mov     rax, 0C00h
    mov     qword ptr [g_peSize], rax

    mov     eax, 1
    lea     rsp, [rbp]
    pop     rdi
    pop     rbp
    ret
WritePEFile ENDP

; ────────────────────────────────────────────────────────────────
; SavePEToDisk — External helper
; ────────────────────────────────────────────────────────────────
extern CreateFileW : PROC
extern WriteFile   : PROC
extern CloseHandle : PROC

SavePEToDisk PROC FRAME
    sub     rsp, 38h
    .allocstack 38h
    .endprolog
    
    ; Ensure PE data is populated
    cmp     qword ptr [g_peSize], 0
    je      @spe_fail

    ; CreateFileW(szPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, NORMAL, NULL)
    lea     rcx, [szPath]
    mov     edx, 40000000h                  ; GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword ptr [rsp + 20h], 2         ; CREATE_ALWAYS
    mov     qword ptr [rsp + 28h], 80h       ; FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp + 30h], 0         ; hTemplateFile = NULL
    call    CreateFileW
    
    cmp     rax, -1                          ; INVALID_HANDLE_VALUE
    je      @spe_fail

    mov     rdi, rax                         ; save hFile

    ; WriteFile(hFile, &g_peBuffer, g_peSize, &bytesWritten, NULL)
    mov     rcx, rax
    lea     rdx, [g_peBuffer]               ; address of buffer (not value!)
    mov     r8d, dword ptr [g_peSize]        ; actual PE size from WritePEFile
    lea     r9, [rsp + 30h]                  ; lpBytesWritten
    mov     qword ptr [rsp + 20h], 0         ; lpOverlapped = NULL
    call    WriteFile
    
    mov     rcx, rdi
    call    CloseHandle

    mov     eax, 1
    jmp     @spe_done
@spe_fail:
    xor     eax, eax
@spe_done:
    add     rsp, 38h
    ret
SavePEToDisk ENDP

END
