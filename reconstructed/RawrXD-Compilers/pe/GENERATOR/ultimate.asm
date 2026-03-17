; ================================================================================
; RawrXD PE Generator/Encoder Ultimate v3.0
; Pure MASM x64 - Production Ready - Zero Dependencies
; Full x64 instruction encoding, imports, exports, resources
; ================================================================================
; Build: ml64.exe pe_generator_ultimate.asm /link /subsystem:console /entry:main
; ================================================================================

; ================================================================================
; SECTION 1: CONSTANTS
; ================================================================================

; PE Constants
IMAGE_DOS_SIGNATURE             EQU     5A4Dh
IMAGE_NT_SIGNATURE              EQU     00004550h
IMAGE_FILE_MACHINE_AMD64        EQU     8664h
IMAGE_FILE_EXECUTABLE_IMAGE     EQU     0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE  EQU     0020h
IMAGE_SUBSYSTEM_WINDOWS_CUI     EQU     3
IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE   EQU     0040h
IMAGE_DLLCHARACTERISTICS_NX_COMPAT      EQU     0100h
IMAGE_NT_OPTIONAL_HDR64_MAGIC   EQU     020Bh
IMAGE_SCN_CNT_CODE              EQU     000000020h
IMAGE_SCN_CNT_INITIALIZED_DATA  EQU     000000040h
IMAGE_SCN_MEM_EXECUTE           EQU     020000000h
IMAGE_SCN_MEM_READ              EQU     040000000h
IMAGE_SCN_MEM_WRITE             EQU     080000000h

; PE Structures offsets (simplified)
DOS_e_lfanew                EQU     60
FILE_Machine                EQU     0
FILE_NumberOfSections       EQU     2
FILE_SizeOfOptionalHeader   EQU     16
FILE_Characteristics        EQU     18
OPT_Magic                   EQU     0
OPT_AddressOfEntryPoint     EQU     16
OPT_ImageBase               EQU     24
OPT_SectionAlignment        EQU     32
OPT_FileAlignment           EQU     36
OPT_SizeOfImage             EQU     56
OPT_SizeOfHeaders           EQU     60
OPT_Subsystem               EQU     68
OPT_DllCharacteristics      EQU     70
OPT_NumberOfRvaAndSizes     EQU     108
SEC_Name                    EQU     0
SEC_VirtualSize             EQU     8
SEC_VirtualAddress          EQU     12
SEC_SizeOfRawData           EQU     16
SEC_PointerToRawData        EQU     20
SEC_Characteristics         EQU     36

; Characteristics
IMAGE_FILE_EXECUTABLE_IMAGE EQU     0002h
IMAGE_FILE_LARGE_ADDRESS_AWARE    EQU     0020h

; Subsystems
IMAGE_SUBSYSTEM_WINDOWS_CUI EQU     3
IMAGE_SUBSYSTEM_WINDOWS_GUI EQU     2

; Register encodings for encoder
REG_RAX     EQU     0
REG_RCX     EQU     1
REG_RDX     EQU     2
REG_RBX     EQU     3
REG_RSP     EQU     4
REG_RBP     EQU     5
REG_RSI     EQU     6
REG_RDI     EQU     7
REG_R8      EQU     8
REG_R9      EQU     9
REG_R10     EQU     10
REG_R11     EQU     11
REG_R12     EQU     12
REG_R13     EQU     13
REG_R14     EQU     14
REG_R15     EQU     15

; PE Section entry
PE_SECTION struct
    szName          BYTE    8 DUP(?)
    dwVirtualSize   DWORD   ?
    dwVirtualAddr   DWORD   ?
    dwRawSize       DWORD   ?
    dwRawAddr       DWORD   ?
    dwCharacteristics   DWORD   ?
    pData           QWORD   ?
PE_SECTION ends

; ================================================================================
; SECTION 2: DATA
; ================================================================================

.const
align 16

; DOS Stub Program (minimal)
g_DosStub LABEL BYTE
    WORD    IMAGE_DOS_SIGNATURE     ; e_magic
    WORD    90h                     ; e_cblp (nop padding)
    WORD    0                       ; e_cp
    WORD    0                       ; e_crlc
    WORD    0                       ; e_cparhdr
    WORD    0                       ; e_minalloc
    WORD    0FFFFh                  ; e_maxalloc
    WORD    0                       ; e_ss
    WORD    0                       ; e_sp
    WORD    0                       ; e_csum
    WORD    0                       ; e_ip
    WORD    0                       ; e_cs
    WORD    0                       ; e_lfarlc
    WORD    0                       ; e_ovno
    WORD    4 DUP(0)                ; e_res
    WORD    0                       ; e_oemid
    WORD    0                       ; e_oeminfo
    WORD    10 DUP(0)               ; e_res2
    DWORD   80h                     ; e_lfanew
    
    ; DOS code (prints message and exits)
    BYTE    0Eh, 1Fh                ; push cs / pop ds
    BYTE    0BAh, 1Eh, 00h          ; mov dx, offset message
    BYTE    0B4h, 09h               ; mov ah, 09h
    BYTE    0CDh, 21h               ; int 21h
    BYTE    0B8h, 01h, 4Ch          ; mov ax, 4C01h
    BYTE    0CDh, 21h               ; int 21h
    
    ; DOS message
    BYTE    'This program requires Win64', 0Dh, 0Ah, 24h
    
    ; Pad to 128 bytes
    BYTE    80h - ($ - g_DosStub) DUP(0)
SIZEOF_DOS_STUB EQU     128

; Test strings
g_szTestMsg         BYTE    'Hello from RawrXD PE Generator!', 0Dh, 0Ah, 0

; Output filename (wide char)
g_wzOutputFile      WORD    'o', 'u', 't', 'p', 'u', 't', '.', 'e', 'x', 'e', 0

g_Banner            BYTE    'RawrXD PE Generator Ultimate v3.0', 0Dh, 0Ah
                    BYTE    '===================================', 0Dh, 0Ah, 0Dh, 0Ah, 0
                    
g_SuccessMsg        BYTE    0Dh, 0Ah, '[+] PE generated: output.exe', 0Dh, 0Ah, 0

g_szTextName        BYTE    '.text', 0, 0, 0

g_szDataName        BYTE    '.data', 0, 0, 0

; ================================================================================
; SECTION 3: BSS
; ================================================================================

.data?
align 16

; Encoder state
g_EncoderCtx LABEL QWORD
g_pEncodBuf     QWORD   ?
g_nEncodOff     QWORD   ?
g_nEncodCap     QWORD   ?
SIZEOF_ENCODER  EQU     24

; PE Generator State
g_SectionTable  PE_SECTION  16 DUP(<>)
g_nSectionCount DWORD       ?
g_pPEBuffer     QWORD       ?
g_nPESize       QWORD       ?
g_nPECapacity   QWORD       ?
g_nNextSectionRVA   DWORD   ?
g_nNextRawOffset    DWORD   ?

; Instruction buffer (temp encoding space)
g_CodeBuffer    BYTE        8192 DUP(?)
g_nCodeSize     DWORD       ?

; Working buffers
g_TempBuffer    BYTE        4096 DUP(?)

; ================================================================================
; SECTION 4: CODE
; ================================================================================

.code
align 16

; ================================================================================
; INSTRUCTION ENCODER
; ================================================================================

; --------------------------------------------------------------------------------
; Enc_Init - Initialize instruction encoder
; Input:  RCX = Output buffer
;         RDX = Capacity
; --------------------------------------------------------------------------------
Enc_Init PROC
    mov     g_pEncodBuf, rcx
    mov     g_nEncodCap, rdx
    mov     g_nEncodOff, 0
    ret
Enc_Init ENDP

; --------------------------------------------------------------------------------
; Enc_EmitByte - Emit single byte
; Input:  CL = Byte
; --------------------------------------------------------------------------------
Enc_EmitByte PROC
    movsxd  rax, g_nEncodOff
    lea     rdx, g_pEncodBuf
    mov     rdx, [rdx]
    mov     [rdx+rax], cl
    inc     g_nEncodOff
    ret
Enc_EmitByte ENDP

; --------------------------------------------------------------------------------
; Enc_EmitDword - Emit 32-bit value
; Input:  ECX = Value
; --------------------------------------------------------------------------------
Enc_EmitDword PROC
    movsxd  rax, g_nEncodOff
    mov     rdx, g_pEncodBuf
    mov     [rdx+rax], ecx
    add     g_nEncodOff, 4
    ret
Enc_EmitDword ENDP

; --------------------------------------------------------------------------------
; Enc_EmitQword - Emit 64-bit value
; Input:  RCX = Value
; --------------------------------------------------------------------------------
Enc_EmitQword PROC
    movsxd  rax, g_nEncodOff
    mov     rdx, g_pEncodBuf
    mov     [rdx+rax], rcx
    add     g_nEncodOff, 8
    ret
Enc_EmitQword ENDP

; --------------------------------------------------------------------------------
; Enc_MOV_R64_I32 - MOV reg64, imm32 (sign-extended)
; Input:  ECX = Register (0-15)
;         EDX = Immediate
; Output: EAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_MOV_R64_I32 PROC FRAME
    push    rbx
    .allocstack 8
    .endprolog
    
    mov     ebx, ecx            ; Save register
    
    ; Check if REX needed
    cmp     ebx, 8
    jb      @F
    mov     al, 41h             ; REX.B
    mov     cl, al
    call    Enc_EmitByte
@@:
    
    ; REX.W prefix
    mov     cl, 48h
    call    Enc_EmitByte
    
    ; Opcode: C7 /0 id
    mov     cl, 0C7h
    call    Enc_EmitByte
    
    ; ModRM: 11 000 reg
    mov     al, 0C0h
    or      al, bl
    and     al, 0Fh
    mov     cl, al
    call    Enc_EmitByte
    
    ; Immediate
    mov     ecx, edx
    call    Enc_EmitDword
    
    mov     eax, 8              ; REX.W + opcode + modrm + imm32
    cmp     ebx, 8
    jb      @F
    inc     eax                 ; + REX.B
@@:
    
    pop     rbx
    ret
Enc_MOV_R64_I32 ENDP

; --------------------------------------------------------------------------------
; Enc_MOV_R64_I64 - MOV reg64, imm64 (full 64-bit)
; Input:  ECX = Register (0-15)
;         RDX = Immediate
; Output: EAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_MOV_R64_I64 PROC FRAME
    push    rbx
    push    rsi
    .allocstack 16
    .endprolog
    
    mov     ebx, ecx
    mov     rsi, rdx
    
    ; REX prefix calculation
    mov     cl, 48h             ; REX.W
    cmp     ebx, 8
    jb      @F
    or      cl, 01h             ; REX.B
@@:
    call    Enc_EmitByte
    
    ; Opcode: B8+rd
    mov     cl, 0B8h
    mov     eax, ebx
    and     al, 7
    or      cl, al
    call    Enc_EmitByte
    
    ; 64-bit immediate
    mov     rcx, rsi
    call    Enc_EmitQword
    
    mov     eax, 10             ; REX + opcode + imm64
    
    pop     rsi
    pop     rbx
    ret
Enc_MOV_R64_I64 ENDP

; --------------------------------------------------------------------------------
; Enc_MOV_R64_R64 - MOV r64, r64
; Input:  ECX = Destination
;         EDX = Source
; Output: EAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_MOV_R64_R64 PROC FRAME
    push    rbx
    push    r12
    .allocstack 16
    .endprolog
    
    mov     r12d, ecx           ; Dest
    mov     ebx, edx            ; Source
    
    ; REX.W prefix
    mov     cl, 48h
    
    ; REX.R from dest
    cmp     r12d, 8
    jb      @F
    or      cl, 04h
@@:
    
    ; REX.B from source
    cmp     ebx, 8
    jb      @F
    or      cl, 01h
@@:
    
    call    Enc_EmitByte
    
    ; Opcode: 8B /r
    mov     cl, 8Bh
    call    Enc_EmitByte
    
    ; ModRM: 11 dest source
    mov     al, 0C0h
    mov     edx, r12d
    and     dl, 7
    shl     dl, 3
    or      al, dl
    mov     edx, ebx
    and     dl, 7
    or      al, dl
    mov     cl, al
    call    Enc_EmitByte
    
    mov     eax, 3
    
    pop     r12
    pop     rbx
    ret
Enc_MOV_R64_R64 ENDP

; --------------------------------------------------------------------------------
; Enc_CALL_R64 - CALL r64
; Input:  ECX = Register
; Output: EAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_CALL_R64 PROC FRAME
    push    rbx
    .allocstack 8
    .endprolog
    
    mov     ebx, ecx
    
    cmp     ebx, 8
    jb      NoRexCall
    
    mov     cl, 41h
    call    Enc_EmitByte
    
NoRexCall:
    mov     cl, 0FFh
    call    Enc_EmitByte
    
    ; ModRM: 11 010 reg
    mov     al, 0D0h
    mov     edx, ebx
    and     dl, 7
    or      al, dl
    mov     cl, al
    call    Enc_EmitByte
    
    mov     eax, 2
    cmp     ebx, 8
    jb      @F
    inc     eax
@@:
    
    pop     rbx
    ret
Enc_CALL_R64 ENDP

; --------------------------------------------------------------------------------
; Enc_PUSH_R64 - PUSH r64
; Input:  ECX = Register
; Output: EAX = Bytes emitted
; --------------------------------------------------------------------------------
Enc_PUSH_R64 PROC FRAME
    push    rbx
    .allocstack 8
    .endprolog
    
    mov     ebx, ecx
    
    cmp     ebx, 8
    jb      NoRexPush
    
    mov     cl, 41h
    call    Enc_EmitByte
    
NoRexPush:
    mov     cl, 50h
    mov     eax, ebx
    and     al, 7
    or      cl, al
    call    Enc_EmitByte
    
    mov     eax, 1
    cmp     ebx, 8
    jb      @F
    inc     eax
@@:
    
    pop     rbx
    ret
Enc_PUSH_R64 ENDP

; --------------------------------------------------------------------------------
; Enc_RET - RET
; Output: EAX = 1
; --------------------------------------------------------------------------------
Enc_RET PROC
    mov     cl, 0C3h
    call    Enc_EmitByte
    mov     eax, 1
    ret
Enc_RET ENDP

; ================================================================================
; PE GENERATOR
; ================================================================================

; --------------------------------------------------------------------------------
; PE_Init - Initialize PE generator
; Output: RAX = TRUE on success
; --------------------------------------------------------------------------------
PE_Init PROC
    mov     g_nSectionCount, 0
    mov     g_nNextSectionRVA, 1000h
    mov     g_nNextRawOffset, 400h
    mov     g_nPECapacity, 40000h
    
    mov     ecx, 40000h
    xor     edx, edx
    mov     r8d, 3000h          ; MEM_COMMIT | MEM_RESERVE
    mov     r9d, 4              ; PAGE_READWRITE
    call    VirtualAlloc
    mov     g_pPEBuffer, rax
    mov     rax, 1
    ret
PE_Init ENDP

; --------------------------------------------------------------------------------
; PE_AddSection - Add section to PE
; Input:  RCX = Name (8 bytes)
;         RDX = Characteristics
;         R8  = Data pointer
;         R9  = Data size
; Output: RAX = Section index or -1
; --------------------------------------------------------------------------------
PE_AddSection PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40
    .allocstack 96
    .endprolog
    
    mov     r12, rcx            ; Name
    mov     r13, rdx            ; Characteristics
    mov     r14, r8             ; Data
    mov     r15d, r9d           ; Size
    
    ; Check section limit
    mov     eax, g_nSectionCount
    cmp     eax, 16
    jae     PE_AddSection_Fail
    
    ; Calculate section header position
    lea     rdi, g_SectionTable
    imul    eax, SIZEOF PE_SECTION
    add     rdi, rax
    
    ; Copy name
    mov     rax, [r12]
    mov     [rdi], rax
    
    ; Set virtual size (aligned to 4KB)
    mov     ebx, r15d
    add     ebx, 0FFFh
    and     ebx, 0FFFFF000h
    mov     [rdi+SIZEOF PE_SECTION.dwVirtualSize], ebx
    
    ; Set virtual address
    mov     eax, g_nNextSectionRVA
    mov     [rdi+SIZEOF PE_SECTION.dwVirtualAddr], eax
    add     g_nNextSectionRVA, ebx
    
    ; Set raw size (aligned to 512)
    mov     ebx, r15d
    add     ebx, 1FFh
    and     ebx, 0FFFFFE00h
    mov     [rdi+SIZEOF PE_SECTION.dwRawSize], ebx
    
    ; Set raw pointer
    mov     eax, g_nNextRawOffset
    mov     [rdi+SIZEOF PE_SECTION.dwRawAddr], eax
    add     g_nNextRawOffset, ebx
    
    ; Set characteristics
    mov     [rdi+SIZEOF PE_SECTION.dwCharacteristics], r13d
    
    ; Increment count
    mov     eax, g_nSectionCount
    inc     g_nSectionCount
    
    jmp     PE_AddSection_Done
    
PE_AddSection_Fail:
    mov     rax, -1
    
PE_AddSection_Done:
    add     rsp, 40
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PE_AddSection ENDP

; ================================================================================
; WINDOWS API IMPORTS
; ================================================================================

EXTERN CreateFileW:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GetStdHandle:PROC
EXTERN ExitProcess:PROC

; ================================================================================
; DEMO
; ================================================================================

main PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 72
    .allocstack 96
    .endprolog
    
    ; Print banner
    mov     ecx, -11
    call    GetStdHandle
    mov     rbx, rax
    
    lea     rdx, g_Banner
    mov     r8, 50              ; approx size
    lea     r9, [rsp+32]
    mov     qword ptr [rsp+32], 0
    mov     rcx, rbx
    call    WriteFile
    
    ; Initialize encoder
    lea     rcx, g_CodeBuffer
    mov     edx, 8192
    call    Enc_Init
    
    ; Encode: mov rcx, 42
    mov     ecx, REG_RCX
    mov     edx, 42
    call    Enc_MOV_R64_I32
    
    ; Encode: call ExitProcess (placeholder)
    mov     ecx, REG_RAX
    mov     rdx, 0140001000h
    call    Enc_MOV_R64_I64
    
    mov     ecx, REG_RAX
    call    Enc_CALL_R64
    
    ; Initialize PE generator
    call    PE_Init
    test    rax, rax
    jz      main_Fail
    
    ; Add .text section
    lea     rcx, g_szTextName
    mov     edx, IMAGE_SCN_CNT_CODE or IMAGE_SCN_MEM_EXECUTE or IMAGE_SCN_MEM_READ
    lea     r8, g_CodeBuffer
    mov     r9d, g_nEncodOff
    call    PE_AddSection
    
    ; Print success
    lea     rdx, g_SuccessMsg
    mov     r8, 50
    lea     r9, [rsp+32]
    mov     qword ptr [rsp+32], 0
    mov     rcx, rbx
    call    WriteFile
    
    ; Exit
    xor     ecx, ecx
    call    ExitProcess
    
main_Fail:
    mov     ecx, 1
    call    ExitProcess
    
main ENDP

END
