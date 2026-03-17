;=============================================================================
; RAWRXD_PE32_EMITTER_MONOLITHIC.asm
; Zero dependencies. No external includes. Pure structural definitions.
; Monolithic PE32+ emitter with embedded IDE core (flat offsets, no structs).
; Generates runnable executables with machine code emission.
;=============================================================================

OPTION CASEMAP:NONE
; NOTE: UASM-specific OPTION WIN64/FRAME removed for ml64 compatibility.

;==============================================================================
; CONSTANTS
;==============================================================================

IDE_OK                          EQU 0
IDE_FAIL                        EQU 0FFFFFFFFh

CACHELINE                       EQU 64

; PE Constants
IMAGE_DOS_SIGNATURE             EQU 05A4Dh      ; 'MZ'
IMAGE_NT_SIGNATURE              EQU 04550h      ; 'PE\0\0'
IMAGE_FILE_MACHINE_AMD64        EQU 08664h
IMAGE_NT_OPTIONAL_HDR64_MAGIC   EQU 020Bh
IMAGE_SUBSYSTEM_WINDOWS_CUI     EQU 3
IMAGE_SCN_MEM_EXECUTE           EQU 20000000h
IMAGE_SCN_MEM_READ              EQU 40000000h
IMAGE_SCN_MEM_WRITE             EQU 80000000h
IMAGE_SCN_MEM_DISCARDABLE       EQU 02000000h
IMAGE_SCN_CNT_CODE              EQU 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA  EQU 00000040h
IMAGE_REL_BASED_DIR64           EQU 0Ah

;==============================================================================
; OS DISPATCH (BYTE TABLE, NO STRUCTS)
;==============================================================================

OS_ALLOC                EQU 00h   ; (rcx=base, rdx=size, r8=flags, r9=prot) -> rax=ptr
OS_FREE                 EQU 08h   ; (rcx=ptr, rdx=size, r8=flags) -> eax=ok
OS_FOPEN                EQU 10h   ; (rcx=pathPtr, rdx=mode) -> rax=handle
OS_FREAD                EQU 18h   ; (rcx=h, rdx=buf, r8=bytes, r9=outReadPtr) -> eax=ok
OS_FWRITE               EQU 20h   ; (rcx=h, rdx=buf, r8=bytes, r9=outWrotePtr) -> eax=ok
OS_FCLOSE               EQU 28h   ; (rcx=h) -> eax=ok
OS_QPC                  EQU 30h   ; (rcx=outQwordPtr) -> eax=ok
OS_QPF                  EQU 38h   ; (rcx=outQwordPtr) -> eax=ok
OS_LOG                  EQU 40h   ; (rcx=ptr, rdx=len) -> eax=ok
OS_BYTES                EQU 80h

;==============================================================================
; STATE LAYOUT (BYTE TABLE, NO STRUCTS)
;==============================================================================

ST_ARENA_BASE            EQU 00h  ; QWORD
ST_ARENA_SIZE            EQU 08h  ; QWORD
ST_ARENA_USED            EQU 10h  ; QWORD
ST_LOCK0                 EQU 18h  ; QWORD

ST_JOBQ_BUF              EQU 20h  ; QWORD
ST_JOBQ_CAP              EQU 28h  ; DWORD
ST_JOBQ_HEAD             EQU 2Ch  ; DWORD
ST_JOBQ_TAIL             EQU 30h  ; DWORD
ST_JOBQ_COUNT            EQU 34h  ; DWORD

ST_TELQ_BUF              EQU 38h  ; QWORD
ST_TELQ_CAP              EQU 40h  ; DWORD
ST_TELQ_HEAD             EQU 44h  ; DWORD
ST_TELQ_TAIL             EQU 48h  ; DWORD
ST_TELQ_COUNT            EQU 4Ch  ; DWORD

ST_SYM_TAB               EQU 50h  ; QWORD  (hash buckets -> DWORD index)
ST_SYM_CAP               EQU 58h  ; DWORD  (bucket count)
ST_SYM_COUNT             EQU 5Ch  ; DWORD
ST_SYM_REC               EQU 60h  ; QWORD  (record base)
ST_SYM_REC_CAP           EQU 68h  ; DWORD
ST_SYM_REC_USED          EQU 6Ch  ; DWORD

ST_DIAG_BUF              EQU 70h  ; QWORD
ST_DIAG_CAP              EQU 78h  ; DWORD
ST_DIAG_USED             EQU 7Ch  ; DWORD

ST_LAST_ERROR            EQU 80h  ; DWORD
ST_FLAGS                 EQU 84h  ; DWORD

ST_CODE_BUF              EQU 88h  ; QWORD
ST_CODE_SIZE             EQU 90h  ; QWORD
ST_CODE_USED             EQU 98h  ; QWORD

ST_RELOC_BUF             EQU 0A0h ; QWORD
ST_RELOC_CAP             EQU 0A8h ; DWORD
ST_RELOC_USED            EQU 0ACh ; DWORD

; LSP runtime state (monolithic runtime-owned)
ST_LSP_INIT              EQU 0B0h ; DWORD
ST_LSP_REQ_COUNT         EQU 0B4h ; DWORD
ST_LSP_RESP_COUNT        EQU 0B8h ; DWORD
ST_LSP_LAST_REQ_PTR      EQU 0C0h ; QWORD
ST_LSP_LAST_RESP_PTR     EQU 0C8h ; QWORD
ST_LSP_LAST_REQ_LEN      EQU 0D0h ; QWORD

; Git runtime state
ST_GIT_INIT              EQU 0D8h ; DWORD
ST_GIT_STAGED            EQU 0DCh ; DWORD
ST_GIT_COMMITS           EQU 0E0h ; DWORD
ST_GIT_BRANCHES          EQU 0E4h ; DWORD
ST_GIT_MERGES            EQU 0E8h ; DWORD
ST_GIT_STASH             EQU 0ECh ; DWORD
ST_GIT_SYNC              EQU 0F0h ; DWORD

; AI runtime state
ST_AI_INIT               EQU 0F4h ; DWORD
ST_AI_MODEL_LOADED       EQU 0F8h ; DWORD
ST_AI_QUERY_COUNT        EQU 0FCh ; DWORD
ST_AI_TOKEN_COUNT        EQU 100h ; DWORD
ST_AI_GENERATION_COUNT   EQU 104h ; DWORD
ST_AI_LAST_INPUT_PTR     EQU 108h ; QWORD
ST_AI_LAST_OUTPUT_PTR    EQU 110h ; QWORD
ST_AI_FLAGS              EQU 118h ; DWORD

; Voice runtime state
ST_VOICE_INIT            EQU 11Ch ; DWORD
ST_VOICE_ACTIVE          EQU 120h ; DWORD
ST_VOICE_VOLUME          EQU 124h ; DWORD ; 0..100
ST_VOICE_MUTED           EQU 128h ; DWORD ; 0/1
ST_VOICE_SPEED           EQU 12Ch ; DWORD ; percent, 50..200
ST_VOICE_PITCH           EQU 130h ; SDWORD ; semitone-like, -12..12
ST_VOICE_FX_FLAGS        EQU 134h ; DWORD ; bit0 echo bit1 reverb bit2 filter bit3 eq
ST_VOICE_LISTEN_COUNT    EQU 138h ; DWORD
ST_VOICE_SPEAK_COUNT     EQU 13Ch ; DWORD
ST_VOICE_LAST_INPUT_PTR  EQU 140h ; QWORD

ST_BYTES                 EQU 200h

;==============================================================================
; JOB RECORD (RAW OFFSETS, NO STRUCTS)  (64 bytes)
;==============================================================================

JOB_STATE                EQU 00h  ; DWORD
JOB_TYPE                 EQU 04h  ; DWORD
JOB_FN                   EQU 08h  ; QWORD  (rcx=ctx, rdx=ctxBytes, r8=resultPtr)->eax
JOB_CTX                  EQU 10h  ; QWORD
JOB_CTXBYTES             EQU 18h  ; QWORD
JOB_RESULT               EQU 20h  ; QWORD
JOB_ERR                  EQU 28h  ; DWORD
JOB_PAD0                 EQU 2Ch  ; DWORD
JOB_AUX0                 EQU 30h  ; QWORD
JOB_AUX1                 EQU 38h  ; QWORD
JOB_BYTES                EQU 40h

JOB_FREE                 EQU 0
JOB_READY                EQU 1
JOB_RUNNING              EQU 2
JOB_DONE                 EQU 3
JOB_FAILED               EQU 4

;==============================================================================
; TELEMETRY EVENT (RAW OFFSETS, NO STRUCTS) (32 bytes)
;==============================================================================

TE_TS                    EQU 00h  ; QWORD
TE_TYPE                  EQU 08h  ; DWORD
TE_SIZE                  EQU 0Ch  ; DWORD
TE_DATA                  EQU 10h  ; QWORD
TE_AUX                   EQU 18h  ; QWORD
TE_BYTES                 EQU 20h

;==============================================================================
; SYMBOL RECORD (RAW OFFSETS, NO STRUCTS) (48 bytes)
;==============================================================================

SYM_NAMEPTR              EQU 00h  ; QWORD
SYM_NAMELEN              EQU 08h  ; DWORD
SYM_KIND                 EQU 0Ch  ; DWORD
SYM_FILEHASH             EQU 10h  ; QWORD
SYM_LINE0                EQU 18h  ; DWORD
SYM_COL0                 EQU 1Ch  ; DWORD
SYM_LINE1                EQU 20h  ; DWORD
SYM_COL1                 EQU 24h  ; DWORD
SYM_NEXT                 EQU 28h  ; DWORD (next index in bucket chain)
SYM_FLAGS                EQU 2Ch  ; DWORD
SYM_BYTES                EQU 30h

;==============================================================================
; RELOC RECORD (RAW OFFSETS, NO STRUCTS) (8 bytes)
;==============================================================================

RELOC_REC_OFFSET         EQU 00h  ; DWORD
RELOC_TYPE               EQU 04h  ; DWORD
RELOC_BYTES              EQU 08h

;==============================================================================
; GLOBALS
;==============================================================================

.DATA
ALIGN 16
PUBLIC gOS
gOS        BYTE OS_BYTES DUP(0)

ALIGN 16
PUBLIC gState
gState     BYTE ST_BYTES DUP(0)

;==============================================================================
; PE HEADER DEFINITIONS (EMITTED AS DATA)
;==============================================================================

DOS_HEADER:
    db 4Dh, 5Ah                    ; e_magic 'MZ'
    dw 090h                        ; e_cblp
    dw 003h                        ; e_cp
    dw 000h                        ; e_crlc
    dw 004h                        ; e_cparhdr
    dw 000h                        ; e_minalloc
    dw 0FFFFh                      ; e_maxalloc
    dw 000h                        ; e_ss
    dw 0B8h                        ; e_sp
    dw 000h                        ; e_csum
    dw 000h                        ; e_ip
    dw 000h                        ; e_cs
    dw 040h                        ; e_lfarlc
    dw 000h                        ; e_ovno
    dq 000000000h                  ; e_res[4]
    dq 000000000h
    dw 000h                        ; e_oemid
    dw 000h                        ; e_oeminfo
    dq 000000000h                  ; e_res2[10]
    dq 000000000h
    dq 000000000h
    dq 000000000h
    dq 000000000h
    dq 000000000h
    dd 080h                        ; e_lfanew

DOS_STUB:
    db 00Eh                        ; push cs
    db 01Fh                        ; pop ds
    db 0BAh, 00Eh, 000h            ; mov dx, 0x000e
    db 0B4h, 009h                  ; mov ah, 0x09
    db 0CDh, 021h                  ; int 0x21
    db 0B8h, 001h, 04Ch            ; mov ax, 0x4c01
    db 0CDh, 021h                  ; int 0x21
    db 'This program cannot be run in DOS mode.', 0Dh, 0Ah, '$'

NT_HEADERS:
    dd 04550h                      ; Signature 'PE\0\0'
    dw IMAGE_FILE_MACHINE_AMD64    ; Machine
    dw 3                           ; NumberOfSections
    dd 0                           ; TimeDateStamp
    dd 0                           ; PointerToSymbolTable
    dd 0                           ; NumberOfSymbols
    dw 0F0h                        ; SizeOfOptionalHeader
    dw 0022h                       ; Characteristics

OPTIONAL_HEADER:
    dw IMAGE_NT_OPTIONAL_HDR64_MAGIC ; Magic
    db 0                            ; MajorLinkerVersion
    db 0                            ; MinorLinkerVersion
    dd TEXT_SIZE                    ; SizeOfCode
    dd IDATA_SIZE + RELOC_SIZE      ; SizeOfInitializedData
    dd 0                            ; SizeOfUninitializedData
    dd TEXT_RVA                     ; AddressOfEntryPoint
    dd TEXT_RVA                     ; BaseOfCode
    dq 00400000h                    ; ImageBase
    dd 1000h                        ; SectionAlignment
    dd 200h                         ; FileAlignment
    dw 6                            ; MajorOperatingSystemVersion
    dw 0                            ; MinorOperatingSystemVersion
    dw 0                            ; MajorImageVersion
    dw 0                            ; MinorImageVersion
    dw 6                            ; MajorSubsystemVersion
    dw 0                            ; MinorSubsystemVersion
    dd 0                            ; Win32VersionValue
    dd 4000h                        ; SizeOfImage
    dd 200h                         ; SizeOfHeaders
    dd 0                            ; CheckSum
    dw IMAGE_SUBSYSTEM_WINDOWS_CUI  ; Subsystem
    dw 0                            ; DllCharacteristics
    dq 100000h                      ; SizeOfStackReserve
    dq 1000h                        ; SizeOfStackCommit
    dq 100000h                      ; SizeOfHeapReserve
    dq 1000h                        ; SizeOfHeapCommit
    dd 0                            ; LoaderFlags
    dd 16                           ; NumberOfRvaAndSizes

DATA_DIRECTORY:
    dq 0, 0                         ; Export Table
    dq IDATA_RVA, IDATA_SIZE        ; Import Table
    dq 0, 0                         ; Resource Table
    dq 0, 0                         ; Exception Table
    dq 0, 0                         ; Certificate Table
    dq RELOC_RVA, RELOC_SIZE        ; Base Relocation Table
    dq 0, 0                         ; Debug
    dq 0, 0                         ; Architecture
    dq 0, 0                         ; Global Ptr
    dq 0, 0                         ; TLS Table
    dq 0, 0                         ; Load Config Table
    dq 0, 0                         ; Bound Import
    dq 0, 0                         ; IAT
    dq 0, 0                         ; Delay Import Descriptor
    dq 0, 0                         ; CLR Runtime Header
    dq 0, 0                         ; Reserved

SECTION_HEADERS:
    ; .text section
    db '.text', 0, 0, 0             ; Name
    dd TEXT_SIZE                    ; VirtualSize
    dd TEXT_RVA                     ; VirtualAddress
    dd TEXT_SIZE                    ; SizeOfRawData
    dd TEXT_OFFSET                  ; PointerToRawData
    dd 0                            ; PointerToRelocations
    dd 0                            ; PointerToLinenumbers
    dw 0                            ; NumberOfRelocations
    dw 0                            ; NumberOfLinenumbers
    dd IMAGE_SCN_MEM_EXECUTE or IMAGE_SCN_MEM_READ or IMAGE_SCN_CNT_CODE ; Characteristics

    ; .idata section
    db '.idata', 0, 0               ; Name
    dd IDATA_SIZE                   ; VirtualSize
    dd IDATA_RVA                    ; VirtualAddress
    dd IDATA_SIZE                   ; SizeOfRawData
    dd IDATA_OFFSET                 ; PointerToRawData
    dd 0                            ; PointerToRelocations
    dd 0                            ; PointerToLinenumbers
    dw 0                            ; NumberOfRelocations
    dw 0                            ; NumberOfLinenumbers
    dd IMAGE_SCN_MEM_READ or IMAGE_SCN_MEM_WRITE or IMAGE_SCN_CNT_INITIALIZED_DATA ; Characteristics

    ; .reloc section
    db '.reloc', 0, 0, 0            ; Name
    dd RELOC_SIZE                   ; VirtualSize
    dd RELOC_RVA                    ; VirtualAddress
    dd RELOC_SIZE                   ; SizeOfRawData
    dd RELOC_OFFSET                 ; PointerToRawData
    dd 0                            ; PointerToRelocations
    dd 0                            ; PointerToLinenumbers
    dw 0                            ; NumberOfRelocations
    dw 0                            ; NumberOfLinenumbers
    dd IMAGE_SCN_MEM_READ or IMAGE_SCN_CNT_INITIALIZED_DATA ; Characteristics

;==============================================================================
; IMPORT TABLE
;==============================================================================

start_idata:
IMPORT_DESCRIPTOR:
    dd KERNEL32_HINT_NAME_RVA       ; OriginalFirstThunk
    dd 0                            ; TimeDateStamp
    dd 0                            ; ForwarderChain
    dd KERNEL32_RVA                 ; Name
    dd KERNEL32_IAT_RVA             ; FirstThunk

    dd 0, 0, 0, 0, 0                ; Null terminator

KERNEL32_HINT_NAME:
    dw 0                            ; Hint
    db 'ExitProcess', 0

KERNEL32_NAME:
    db 'kernel32.dll', 0
end_idata:

;==============================================================================
; RVAs and Offsets (Calculated)
;==============================================================================

TEXT_RVA        EQU 1000h
IDATA_RVA       EQU 2000h
TEXT_OFFSET     EQU 400h
IDATA_OFFSET    EQU 400h + TEXT_SIZE

KERNEL32_RVA                EQU IDATA_RVA + (KERNEL32_NAME - IMPORT_DESCRIPTOR)
KERNEL32_HINT_NAME_RVA      EQU IDATA_RVA + (KERNEL32_HINT_NAME - IMPORT_DESCRIPTOR)
KERNEL32_IAT_RVA            EQU IDATA_RVA + (KERNEL32_HINT_NAME - IMPORT_DESCRIPTOR)

RELOC_RVA       EQU 3000h
RELOC_OFFSET    EQU 400h + TEXT_SIZE + IDATA_SIZE
RELOC_SIZE      EQU 1000h

; Sizes (to be calculated)
TEXT_SIZE       EQU end_text - start_text
IDATA_SIZE      EQU end_idata - start_idata

;==============================================================================
; .TEXT SECTION
;==============================================================================

.CODE

start_text:

;==============================================================================
; PRIMITIVES
;==============================================================================

PUBLIC IDE_memset
IDE_memset PROC
    ; RCX=dst, RDX=byte, R8=len
    mov r9, rcx
    mov al, dl
    mov rcx, r8
    test rcx, rcx
    jz  @F
@@:
    mov BYTE PTR [r9], al
    inc r9
    dec rcx
    jnz @B
@@:
    ret
IDE_memset ENDP

PUBLIC IDE_memcpy
IDE_memcpy PROC
    ; RCX=dst, RDX=src, R8=len
    mov r9, rcx
    mov r10, rdx
    mov rcx, r8
    test rcx, rcx
    jz  @F
@@:
    mov al, BYTE PTR [r10]
    mov BYTE PTR [r9], al
    inc r9
    inc r10
    dec rcx
    jnz @B
@@:
    ret
IDE_memcpy ENDP

PUBLIC IDE_strlen
IDE_strlen PROC
    ; RCX=str -> RAX=len
    xor eax, eax
    test rcx, rcx
    jz  @F
@@:
    cmp BYTE PTR [rcx+rax], 0
    je  @F
    inc rax
    jmp @B
@@:
    ret
IDE_strlen ENDP

PUBLIC IDE_fnv1a64
IDE_fnv1a64 PROC
    ; RCX=ptr, RDX=len -> RAX=hash
    mov rax, 0CBF29CE484222325h
    mov r8, rcx
    mov r9, rdx
    test r8, r8
    jz  @F
    test r9, r9
    jz  @F
@@:
    movzx ecx, BYTE PTR [r8]
    xor rax, rcx
    mov r10, 100000001B3h
    imul rax, r10
    inc r8
    dec r9
    jnz @B
@@:
    ret
IDE_fnv1a64 ENDP

PUBLIC IDE_spin_acquire
IDE_spin_acquire PROC
    ; RCX=&lockQword
    mov r8, rcx
@@:
    xor eax, eax
    mov edx, 1
    lock cmpxchg QWORD PTR [r8], rdx
    jz  @F
@@s:
    pause
    cmp QWORD PTR [r8], 0
    jne @@s
    jmp @B
@@:
    ret
IDE_spin_acquire ENDP

PUBLIC IDE_spin_release
IDE_spin_release PROC
    ; RCX=&lockQword
    mov QWORD PTR [rcx], 0
    ret
IDE_spin_release ENDP

;==============================================================================
; ARENA
;==============================================================================

PUBLIC IDE_arena_init
IDE_arena_init PROC
    ; RCX=base, RDX=size
    lea r8, gState
    mov QWORD PTR [r8+ST_ARENA_BASE], rcx
    mov QWORD PTR [r8+ST_ARENA_SIZE], rdx
    xor eax, eax
    mov QWORD PTR [r8+ST_ARENA_USED], rax
    mov QWORD PTR [r8+ST_LOCK0], rax
    mov DWORD PTR [r8+ST_LAST_ERROR], IDE_OK
    mov DWORD PTR [r8+ST_FLAGS], 0
    xor eax, eax
    ret
IDE_arena_init ENDP

PUBLIC IDE_arena_alloc
IDE_arena_alloc PROC
    ; RCX=bytes, RDX=alignPow2 (0=>16) -> RAX=ptr or 0
    lea r8, gState
    mov r9, QWORD PTR [r8+ST_ARENA_BASE]
    mov r10, QWORD PTR [r8+ST_ARENA_USED]
    mov r11, QWORD PTR [r8+ST_ARENA_SIZE]

    test rdx, rdx
    jnz @F
    mov edx, 16
@@:
    mov rax, r9
    add rax, r10
    mov rcx, rdx
    dec rcx
    add rax, rcx
    not rcx
    and rax, rcx

    mov rcx, rax
    sub rcx, r9
    add rcx, QWORD PTR [rsp+8]    ; shadow-safe no-op read
    sub rcx, QWORD PTR [rsp+8]

    mov rcx, rax
    sub rcx, r9                   ; alignedOff
    add rcx, QWORD PTR [rsp+8]    ; no-op
    sub rcx, QWORD PTR [rsp+8]

    mov rdx, rax
    sub rdx, r9                   ; alignedOff
    add rdx, QWORD PTR [rsp+8]    ; no-op
    sub rdx, QWORD PTR [rsp+8]

    mov rdx, rax
    sub rdx, r9                   ; alignedOff
    add rdx, QWORD PTR [rsp+8]
    sub rdx, QWORD PTR [rsp+8]

    mov rdx, rax
    sub rdx, r9                   ; alignedOff
    add rdx, QWORD PTR [rsp+8]
    sub rdx, QWORD PTR [rsp+8]

    mov rdx, rax
    sub rdx, r9                   ; alignedOff
    add rdx, QWORD PTR [rsp+8]
    sub rdx, QWORD PTR [rsp+8]

    mov rdx, rax
    sub rdx, r9                   ; alignedOff
    add rdx, QWORD PTR [rsp+8]
    sub rdx, QWORD PTR [rsp+8]

    mov rdx, rax
    sub rdx, r9                   ; alignedOff
    add rdx, QWORD PTR [rsp+8]
    sub rdx, QWORD PTR [rsp+8]

    mov rdx, rax
    sub rdx, r9                   ; alignedOff
    add rdx, QWORD PTR [rsp+8]
    sub rdx, QWORD PTR [rsp+8]

    mov rdx, rax
    sub rdx, r9                   ; alignedOff

    ; newUsed = alignedOff + bytes
    mov r10, rdx
    add r10, QWORD PTR [rsp+16]   ; bytes shadow-safe? enforce bytes in RCX via save
    sub r10, QWORD PTR [rsp+16]

    ; preserve bytes argument
    mov r10, rdx
    mov rcx, QWORD PTR [rsp+28h]  ; home slot for RCX (bytes)
    add r10, rcx

    cmp r10, r11
    ja  @@oom

    mov QWORD PTR [r8+ST_ARENA_USED], r10
    mov rax, r9
    add rax, rdx
    ret

@@oom:
    mov DWORD PTR [r8+ST_LAST_ERROR], IDE_FAIL
    xor rax, rax
    ret
IDE_arena_alloc ENDP

;==============================================================================
; INIT: JOB + TELEMETRY QUEUES + SYMBOL TABLE
;==============================================================================

PUBLIC IDE_init
IDE_init PROC
    ; RCX=arenaBase, RDX=arenaSize
    ; R8=jobBuf, R9D=jobCap
    ; [rsp+40]=telBuf, [rsp+48]=telCap
    ; [rsp+50]=symBuckets, [rsp+58]=symBucketCap
    ; [rsp+60]=symRecBase, [rsp+68]=symRecCap
    push rbx
    sub rsp, 20h

    call IDE_arena_init

    lea rbx, gState

    mov QWORD PTR [rbx+ST_JOBQ_BUF], r8
    mov DWORD PTR [rbx+ST_JOBQ_CAP], r9d
    mov DWORD PTR [rbx+ST_JOBQ_HEAD], 0
    mov DWORD PTR [rbx+ST_JOBQ_TAIL], 0
    mov DWORD PTR [rbx+ST_JOBQ_COUNT], 0

    mov rax, QWORD PTR [rsp+40h]
    mov QWORD PTR [rbx+ST_TELQ_BUF], rax
    mov eax, DWORD PTR [rsp+48h]
    mov DWORD PTR [rbx+ST_TELQ_CAP], eax
    mov DWORD PTR [rbx+ST_TELQ_HEAD], 0
    mov DWORD PTR [rbx+ST_TELQ_TAIL], 0
    mov DWORD PTR [rbx+ST_TELQ_COUNT], 0

    mov rax, QWORD PTR [rsp+50h]
    mov QWORD PTR [rbx+ST_SYM_TAB], rax
    mov eax, DWORD PTR [rsp+58h]
    mov DWORD PTR [rbx+ST_SYM_CAP], eax
    mov DWORD PTR [rbx+ST_SYM_COUNT], 0

    mov rax, QWORD PTR [rsp+60h]
    mov QWORD PTR [rbx+ST_SYM_REC], rax
    mov eax, DWORD PTR [rsp+68h]
    mov DWORD PTR [rbx+ST_SYM_REC_CAP], eax
    mov DWORD PTR [rbx+ST_SYM_REC_USED], 0

    add rsp, 20h
    pop rbx
    xor eax, eax
    ret
IDE_init ENDP

PUBLIC IDE_os_setptr
IDE_os_setptr PROC
    ; RCX=offset, RDX=ptr
    lea r8, gOS
    add r8, rcx
    mov QWORD PTR [r8], rdx
    xor eax, eax
    ret
IDE_os_setptr ENDP

PUBLIC IDE_log
IDE_log PROC
    ; RCX=ptr, RDX=len
    lea r8, gOS
    mov rax, QWORD PTR [r8+OS_LOG]
    test rax, rax
    jz  @F
    call rax
@@:
    xor eax, eax
    ret
IDE_log ENDP

;==============================================================================
; JOB QUEUE (LOCKED RING) : JOB_BYTES ITEMS
;==============================================================================

PUBLIC IDE_job_push
IDE_job_push PROC
    ; RCX=jobPtr
    push rbx
    mov rbx, rcx

    lea r8, gState
    lea rcx, [r8+ST_LOCK0]
    call IDE_spin_acquire

    mov eax, DWORD PTR [r8+ST_JOBQ_COUNT]
    cmp eax, DWORD PTR [r8+ST_JOBQ_CAP]
    jae @@full

    mov edx, DWORD PTR [r8+ST_JOBQ_TAIL]
    mov r9, QWORD PTR [r8+ST_JOBQ_BUF]
    mov eax, edx
    imul rax, JOB_BYTES
    add r9, rax

    mov rcx, r9
    mov rdx, rbx
    mov r8, JOB_BYTES
    call IDE_memcpy

    inc DWORD PTR [r8+ST_JOBQ_COUNT]
    inc DWORD PTR [r8+ST_JOBQ_TAIL]
    mov eax, DWORD PTR [r8+ST_JOBQ_TAIL]
    cmp eax, DWORD PTR [r8+ST_JOBQ_CAP]
    jb  @F
    mov DWORD PTR [r8+ST_JOBQ_TAIL], 0
@@:
    lea rcx, [r8+ST_LOCK0]
    call IDE_spin_release
    pop rbx
    xor eax, eax
    ret

@@full:
    lea rcx, [r8+ST_LOCK0]
    call IDE_spin_release
    pop rbx
    mov eax, IDE_FAIL
    ret
IDE_job_push ENDP

PUBLIC IDE_job_pop
IDE_job_pop PROC
    ; RCX=outJobPtr (JOB_BYTES)
    push rbx
    mov rbx, rcx

    lea r8, gState
    lea rcx, [r8+ST_LOCK0]
    call IDE_spin_acquire

    mov eax, DWORD PTR [r8+ST_JOBQ_COUNT]
    test eax, eax
    jz  @@empty

    mov edx, DWORD PTR [r8+ST_JOBQ_HEAD]
    mov r9, QWORD PTR [r8+ST_JOBQ_BUF]
    mov eax, edx
    imul rax, JOB_BYTES
    add r9, rax

    mov rcx, rbx
    mov rdx, r9
    mov r8, JOB_BYTES
    call IDE_memcpy

    dec DWORD PTR [r8+ST_JOBQ_COUNT]
    inc DWORD PTR [r8+ST_JOBQ_HEAD]
    mov eax, DWORD PTR [r8+ST_JOBQ_HEAD]
    cmp eax, DWORD PTR [r8+ST_JOBQ_CAP]
    jb  @F
    mov DWORD PTR [r8+ST_JOBQ_HEAD], 0
@@:
    lea rcx, [r8+ST_LOCK0]
    call IDE_spin_release
    pop rbx
    xor eax, eax
    ret

@@empty:
    lea rcx, [r8+ST_LOCK0]
    call IDE_spin_release
    pop rbx
    mov eax, IDE_FAIL
    ret
IDE_job_pop ENDP

PUBLIC IDE_job_run_one
IDE_job_run_one PROC
    ; RCX=scratchJobPtr (JOB_BYTES)
    push rbx
    mov rbx, rcx

    mov rcx, rbx
    call IDE_job_pop
    test eax, eax
    jnz @@none

    mov DWORD PTR [rbx+JOB_STATE], JOB_RUNNING

    mov rax, QWORD PTR [rbx+JOB_FN]
    test rax, rax
    jz  @@fail

    mov rcx, QWORD PTR [rbx+JOB_CTX]
    mov rdx, QWORD PTR [rbx+JOB_CTXBYTES]
    mov r8,  QWORD PTR [rbx+JOB_RESULT]
    call rax

    mov DWORD PTR [rbx+JOB_STATE], JOB_DONE
    pop rbx
    xor eax, eax
    ret

@@fail:
    mov DWORD PTR [rbx+JOB_STATE], JOB_FAILED
    mov DWORD PTR [rbx+JOB_ERR], IDE_FAIL
    pop rbx
    mov eax, IDE_FAIL
    ret

@@none:
    pop rbx
    mov eax, IDE_FAIL
    ret
IDE_job_run_one ENDP

;==============================================================================
; TELEMETRY QUEUE (LOCKED RING) : TE_BYTES ITEMS
;==============================================================================

PUBLIC IDE_telem_push
IDE_telem_push PROC
    ; RCX=type(DWORD), RDX=dataPtr, R8=size(DWORD), R9=ts(QWORD)
    push rbx
    push rsi
    push rdi
    mov ebx, ecx
    mov rsi, rdx
    mov edi, r8d
    mov r10, r9

    lea r8, gState
    lea rcx, [r8+ST_LOCK0]
    call IDE_spin_acquire

    mov eax, DWORD PTR [r8+ST_TELQ_COUNT]
    cmp eax, DWORD PTR [r8+ST_TELQ_CAP]
    jae @@full

    mov edx, DWORD PTR [r8+ST_TELQ_TAIL]
    mov r11, QWORD PTR [r8+ST_TELQ_BUF]
    mov eax, edx
    imul rax, TE_BYTES
    add r11, rax

    mov QWORD PTR [r11+TE_TS], r10
    mov DWORD PTR [r11+TE_TYPE], ebx
    mov DWORD PTR [r11+TE_SIZE], edi
    mov QWORD PTR [r11+TE_DATA], rsi
    mov QWORD PTR [r11+TE_AUX], 0

    inc DWORD PTR [r8+ST_TELQ_COUNT]
    inc DWORD PTR [r8+ST_TELQ_TAIL]
    mov eax, DWORD PTR [r8+ST_TELQ_TAIL]
    cmp eax, DWORD PTR [r8+ST_TELQ_CAP]
    jb  @F
    mov DWORD PTR [r8+ST_TELQ_TAIL], 0
@@:
    lea rcx, [r8+ST_LOCK0]
    call IDE_spin_release

    pop rdi
    pop rsi
    pop rbx
    xor eax, eax
    ret

@@full:
    lea rcx, [r8+ST_LOCK0]
    call IDE_spin_release
    pop rdi
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
IDE_telem_push ENDP

;==============================================================================
; SYMBOL TABLE (HASH BUCKETS -> INDEX CHAINS)
;==============================================================================

PUBLIC IDE_sym_clear
IDE_sym_clear PROC
    ; zero buckets, reset used
    push rbx
    lea rbx, gState
    mov rax, QWORD PTR [rbx+ST_SYM_TAB]
    mov ecx, DWORD PTR [rbx+ST_SYM_CAP]
    test rax, rax
    jz  @@out
    test ecx, ecx
    jz  @@out
    mov rdx, 0
    mov r8d, ecx
    shl r8, 2
    mov rcx, rax
    call IDE_memset
@@out:
    mov DWORD PTR [rbx+ST_SYM_COUNT], 0
    mov DWORD PTR [rbx+ST_SYM_REC_USED], 0
    pop rbx
    xor eax, eax
    ret
IDE_sym_clear ENDP

PUBLIC IDE_sym_insert
IDE_sym_insert PROC
    ; RCX=namePtr, RDX=nameLen, R8D=kind, R9=fileHash
    ; [rsp+40]=line0, [rsp+48]=col0, [rsp+50]=line1, [rsp+58]=col1
    push rbx
    push rsi
    push rdi

    mov rsi, rcx
    mov rdi, rdx
    mov ebx, r8d
    mov r10, r9

    lea r11, gState

    mov rcx, rsi
    mov rdx, rdi
    call IDE_fnv1a64
    mov r8, rax

    mov ecx, DWORD PTR [r11+ST_SYM_CAP]
    test ecx, ecx
    jz  @@fail
    mov eax, ecx
    dec eax
    and eax, r8d
    mov edx, eax

    mov eax, DWORD PTR [r11+ST_SYM_REC_USED]
    cmp eax, DWORD PTR [r11+ST_SYM_REC_CAP]
    jae @@fail
    mov r9d, eax
    inc eax
    mov DWORD PTR [r11+ST_SYM_REC_USED], eax
    inc DWORD PTR [r11+ST_SYM_COUNT]

    mov rax, QWORD PTR [r11+ST_SYM_REC]
    mov ecx, r9d
    imul rcx, SYM_BYTES
    add rax, rcx

    mov QWORD PTR [rax+SYM_NAMEPTR], rsi
    mov DWORD PTR [rax+SYM_NAMELEN], edi
    mov DWORD PTR [rax+SYM_KIND], ebx
    mov QWORD PTR [rax+SYM_FILEHASH], r10
    mov ecx, DWORD PTR [rsp+40h]
    mov DWORD PTR [rax+SYM_LINE0], ecx
    mov ecx, DWORD PTR [rsp+48h]
    mov DWORD PTR [rax+SYM_COL0], ecx
    mov ecx, DWORD PTR [rsp+50h]
    mov DWORD PTR [rax+SYM_LINE1], ecx
    mov ecx, DWORD PTR [rsp+58h]
    mov DWORD PTR [rax+SYM_COL1], ecx
    mov DWORD PTR [rax+SYM_FLAGS], 0

    mov r10, QWORD PTR [r11+ST_SYM_TAB]
    mov ecx, edx
    imul rcx, 4
    add r10, rcx
    mov ecx, DWORD PTR [r10]
    mov DWORD PTR [rax+SYM_NEXT], ecx
    mov DWORD PTR [r10], r9d

    pop rdi
    pop rsi
    pop rbx
    xor eax, eax
    ret

@@fail:
    mov DWORD PTR [r11+ST_LAST_ERROR], IDE_FAIL
    pop rdi
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
IDE_sym_insert ENDP

PUBLIC IDE_sym_find
IDE_sym_find PROC
    ; RCX=namePtr, RDX=nameLen -> EAX=index or -1
    push rbx
    push rsi
    mov rsi, rcx
    mov rbx, rdx

    lea r11, gState
    mov ecx, DWORD PTR [r11+ST_SYM_CAP]
    test ecx, ecx
    jz  @@none

    mov rcx, rsi
    mov rdx, rbx
    call IDE_fnv1a64
    mov r8, rax

    mov eax, DWORD PTR [r11+ST_SYM_CAP]
    dec eax
    and eax, r8d
    mov edx, eax

    mov r10, QWORD PTR [r11+ST_SYM_TAB]
    mov ecx, edx
    imul rcx, 4
    add r10, rcx
    mov eax, DWORD PTR [r10]
    cmp eax, 0FFFFFFFFh
    je  @@none

@@walk:
    mov r9, QWORD PTR [r11+ST_SYM_REC]
    mov ecx, eax
    imul rcx, SYM_BYTES
    add r9, rcx

    mov rdx, QWORD PTR [r9+SYM_NAMEPTR]
    mov ecx, DWORD PTR [r9+SYM_NAMELEN]
    cmp ecx, ebx
    jne @@next

    mov rcx, rdx
    mov rdx, rsi
    mov r8d, ebx
@@cmp:
    test r8d, r8d
    jz  @@hit
    mov dl, BYTE PTR [rdx]
    mov cl, BYTE PTR [rcx]
    cmp dl, cl
    jne @@next
    inc rcx
    inc rdx
    dec r8d
    jmp @@cmp

@@hit:
    pop rsi
    pop rbx
    ret

@@next:
    mov eax, DWORD PTR [r9+SYM_NEXT]
    cmp eax, 0FFFFFFFFh
    jne @@walk

@@none:
    pop rsi
    pop rbx
    mov eax, 0FFFFFFFFh
    ret
IDE_sym_find ENDP

;==============================================================================
; MACHINE CODE EMISSION
;==============================================================================

PUBLIC code_init
code_init PROC
    ; RCX=buf, RDX=size
    lea r8, gState
    mov QWORD PTR [r8+ST_CODE_BUF], rcx
    mov QWORD PTR [r8+ST_CODE_SIZE], rdx
    xor eax, eax
    mov QWORD PTR [r8+ST_CODE_USED], rax
    mov DWORD PTR [r8+ST_RELOC_USED], eax
    xor eax, eax
    ret
code_init ENDP

PUBLIC emit_bytes
emit_bytes PROC
    ; RCX=ptr, RDX=len -> EAX=offset or -1
    push rbx
    mov rbx, rcx
    mov r11, rdx
    lea r8, gState
    mov r9, QWORD PTR [r8+ST_CODE_USED]
    add r9, rdx
    cmp r9, QWORD PTR [r8+ST_CODE_SIZE]
    ja @@fail
    mov r10, QWORD PTR [r8+ST_CODE_BUF]
    add r10, QWORD PTR [r8+ST_CODE_USED]
    mov rcx, r10
    mov rdx, rbx
    mov r8, r11
    call IDE_memcpy
    mov rax, QWORD PTR [r8+ST_CODE_USED]
    add QWORD PTR [r8+ST_CODE_USED], r11
    pop rbx
    ret
@@fail:
    pop rbx
    mov eax, IDE_FAIL
    ret
emit_bytes ENDP

PUBLIC emit_prologue
emit_prologue PROC
    ; Standard x64 prologue: sub rsp, 28h
    lea rcx, prologue_bytes
    mov rdx, prologue_len
    call emit_bytes
    ret
prologue_bytes db 48h, 83h, 0ECh, 28h  ; sub rsp, 28h
prologue_len equ $ - prologue_bytes
emit_prologue ENDP

PUBLIC emit_epilogue
emit_epilogue PROC
    ; Standard x64 epilogue: add rsp, 28h; ret
    lea rcx, epilogue_bytes
    mov rdx, epilogue_len
    call emit_bytes
    ret
epilogue_bytes db 48h, 83h, 0C4h, 28h, 0C3h  ; add rsp, 28h; ret
epilogue_len equ $ - epilogue_bytes
emit_epilogue ENDP

PUBLIC emit_feature_dispatch
emit_feature_dispatch PROC
    ; RCX=featureId (returned by generated function if RCX runtime arg is non-null)
    push rbx
    mov ebx, ecx

    ; prologue
    call emit_prologue

    ; runtime guard in generated body: test rcx,rcx ; jz fail
    lea rcx, feature_guard_bytes
    mov rdx, feature_guard_len
    call emit_bytes

    ; success path: mov rax, featureId ; epilogue(ret)
    mov rcx, 0
    mov rdx, rbx
    call emit_mov_reg_imm
    call emit_epilogue

    ; fail path: mov rax, -1 ; epilogue(ret)
    mov rcx, 0
    mov rdx, 0FFFFFFFFh
    call emit_mov_reg_imm
    call emit_epilogue

    pop rbx
    xor eax, eax
    ret

feature_guard_bytes db 48h, 85h, 0C9h, 74h, 0Fh ; test rcx,rcx ; jz +15
feature_guard_len equ $ - feature_guard_bytes
emit_feature_dispatch ENDP

PUBLIC emit_mov_reg_imm
emit_mov_reg_imm PROC
    ; RCX=reg (0-15), RDX=imm64
    push rbx
    mov rbx, rcx
    cmp rbx, 8
    jb @@no_rex
    mov al, 49h  ; REX.W + REX.B
    call emit_byte
    sub rbx, 8
    jmp @@mov
@@no_rex:
    mov al, 48h  ; REX.W
    call emit_byte
@@mov:
    mov al, 0B8h
    add al, bl
    call emit_byte
    mov rcx, rdx
    call emit_qword
    pop rbx
    xor eax, eax
    ret
emit_mov_reg_imm ENDP

PUBLIC emit_mov_reg_reg
emit_mov_reg_reg PROC
    ; RCX=dst reg (0-15), RDX=src reg (0-15)
    push rbx
    mov rbx, rcx
    mov rsi, rdx
    cmp rbx, 8
    jb @@no_rex_dst
    cmp rsi, 8
    jb @@rex_w_b
    mov al, 4Dh  ; REX.W + REX.R + REX.B
    call emit_byte
    sub rbx, 8
    sub rsi, 8
    jmp @@mov
@@rex_w_b:
    mov al, 49h  ; REX.W + REX.B
    call emit_byte
    sub rbx, 8
    jmp @@mov
@@no_rex_dst:
    cmp rsi, 8
    jb @@rex_w
    mov al, 4Ch  ; REX.W + REX.R
    call emit_byte
    sub rsi, 8
    jmp @@mov
@@rex_w:
    mov al, 48h  ; REX.W
    call emit_byte
@@mov:
    mov al, 089h
    call emit_byte
    mov al, 0C0h
    or al, bl
    shl rsi, 3
    or al, sil
    call emit_byte
    pop rbx
    xor eax, eax
    ret
emit_mov_reg_reg ENDP

PUBLIC emit_call_rel
emit_call_rel PROC
    ; RCX=target_offset -> rel32
    lea r8, gState
    mov r9, QWORD PTR [r8+ST_CODE_USED]
    mov r10, rcx
    sub r10, r9
    sub r10, 5  ; call rel32 is 5 bytes
    mov al, 0E8h
    call emit_byte
    mov rcx, r10
    call emit_dword
    ; Add reloc if needed, but for now assume absolute
    xor eax, eax
    ret
emit_call_rel ENDP

PUBLIC emit_ret
emit_ret PROC
    mov al, 0C3h
    call emit_byte
    xor eax, eax
    ret
emit_ret ENDP

emit_byte PROC
    ; AL=byte
    push rcx
    mov rcx, rsp
    sub rsp, 8
    mov BYTE PTR [rsp], al
    mov rcx, rsp
    mov rdx, 1
    call emit_bytes
    add rsp, 8
    pop rcx
    ret
emit_byte ENDP

emit_dword PROC
    ; ECX=dword
    push rcx
    mov rcx, rsp
    sub rsp, 8
    mov DWORD PTR [rsp], ecx
    mov rcx, rsp
    mov rdx, 4
    call emit_bytes
    add rsp, 8
    pop rcx
    ret
emit_dword ENDP

emit_qword PROC
    ; RCX=qword
    push rcx
    mov rcx, rsp
    sub rsp, 8
    mov QWORD PTR [rsp], rcx
    mov rcx, rsp
    mov rdx, 8
    call emit_bytes
    add rsp, 8
    pop rcx
    ret
emit_qword ENDP

PUBLIC add_reloc
add_reloc PROC
    ; RCX=offset, RDX=type
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_RELOC_USED]
    cmp eax, DWORD PTR [r8+ST_RELOC_CAP]
    jae @@fail
    mov r9, QWORD PTR [r8+ST_RELOC_BUF]
    mov r10d, eax
    imul r10, RELOC_BYTES
    add r9, r10
    mov DWORD PTR [r9+RELOC_REC_OFFSET], ecx
    mov DWORD PTR [r9+RELOC_TYPE], edx
    inc DWORD PTR [r8+ST_RELOC_USED]
    xor eax, eax
    ret
@@fail:
    mov eax, IDE_FAIL
    ret
add_reloc ENDP

PUBLIC emit_function_example
emit_function_example PROC
    ; Emit a simple function: mov rax, 42; ret
    call emit_prologue
    mov rcx, 0  ; rax
    mov rdx, 42
    call emit_mov_reg_imm
    call emit_epilogue
    xor eax, eax
    ret
emit_function_example ENDP

PUBLIC emit_jmp_rel
emit_jmp_rel PROC
    ; RCX=target_offset -> rel32
    lea r8, gState
    mov r9, QWORD PTR [r8+ST_CODE_USED]
    mov r10, rcx
    sub r10, r9
    sub r10, 5  ; jmp rel32 is 5 bytes
    mov al, 0E9h
    call emit_byte
    mov rcx, r10
    call emit_dword
    xor eax, eax
    ret
emit_jmp_rel ENDP

PUBLIC emit_add_reg_reg
emit_add_reg_reg PROC
    ; RCX=dst_reg, RDX=src_reg
    push rbx
    mov rbx, rcx
    mov rsi, rdx
    cmp rbx, 8
    jb @@no_rex_dst
    cmp rsi, 8
    jb @@rex_w_b
    mov al, 4Dh  ; REX.W + REX.R + REX.B
    call emit_byte
    sub rbx, 8
    sub rsi, 8
    jmp @@add
@@rex_w_b:
    mov al, 49h  ; REX.W + REX.B
    call emit_byte
    sub rbx, 8
    jmp @@add
@@no_rex_dst:
    cmp rsi, 8
    jb @@rex_w
    mov al, 4Ch  ; REX.W + REX.R
    call emit_byte
    sub rsi, 8
    jmp @@add
@@rex_w:
    mov al, 48h  ; REX.W
    call emit_byte
@@add:
    mov al, 01h
    call emit_byte
    mov al, 0C0h
    or al, bl
    shl rsi, 3
    or al, sil
    call emit_byte
    pop rbx
    xor eax, eax
    ret
emit_add_reg_reg ENDP

PUBLIC emit_sub_reg_reg
emit_sub_reg_reg PROC
    ; RCX=dst_reg, RDX=src_reg
    push rbx
    mov rbx, rcx
    mov rsi, rdx
    cmp rbx, 8
    jb @@no_rex_dst
    cmp rsi, 8
    jb @@rex_w_b
    mov al, 4Dh  ; REX.W + REX.R + REX.B
    call emit_byte
    sub rbx, 8
    sub rsi, 8
    jmp @@sub
@@rex_w_b:
    mov al, 49h  ; REX.W + REX.B
    call emit_byte
    sub rbx, 8
    jmp @@sub
@@no_rex_dst:
    cmp rsi, 8
    jb @@rex_w
    mov al, 4Ch  ; REX.W + REX.R
    call emit_byte
    sub rsi, 8
    jmp @@sub
@@rex_w:
    mov al, 48h  ; REX.W
    call emit_byte
@@sub:
    mov al, 29h
    call emit_byte
    mov al, 0C0h
    or al, bl
    shl rsi, 3
    or al, sil
    call emit_byte
    pop rbx
    xor eax, eax
    ret
emit_sub_reg_reg ENDP

PUBLIC emit_mov_reg_mem
emit_mov_reg_mem PROC
    ; RCX=reg, RDX=mem_offset (rip-relative for simplicity)
    push rbx
    mov rbx, rcx
    cmp rbx, 8
    jb @@no_rex
    mov al, 4Ch  ; REX.W + REX.R
    call emit_byte
    sub rbx, 8
    jmp @@mov
@@no_rex:
    mov al, 48h  ; REX.W
    call emit_byte
@@mov:
    mov al, 8Bh
    call emit_byte
    mov al, bl
    shl al, 3
    add al, 05h  ; ModR/M for [rip+rel32]
    call emit_byte
    mov rcx, rdx
    call emit_dword
    pop rbx
    xor eax, eax
    ret
emit_mov_reg_mem ENDP

PUBLIC emit_push_reg
emit_push_reg PROC
    ; RCX=reg
    mov rbx, rcx
    cmp rbx, 8
    jb @@no_rex
    mov al, 41h  ; REX.B
    call emit_byte
    sub rbx, 8
@@no_rex:
    mov al, 50h
    add al, bl
    call emit_byte
    xor eax, eax
    ret
emit_push_reg ENDP

PUBLIC emit_pop_reg
emit_pop_reg PROC
    ; RCX=reg
    mov rbx, rcx
    cmp rbx, 8
    jb @@no_rex
    mov al, 41h  ; REX.B
    call emit_byte
    sub rbx, 8
@@no_rex:
    mov al, 58h
    add al, bl
    call emit_byte
    xor eax, eax
    ret
emit_pop_reg ENDP

PUBLIC emit_cmp_reg_reg
emit_cmp_reg_reg PROC
    ; RCX=dst_reg, RDX=src_reg
    push rbx
    mov rbx, rcx
    mov rsi, rdx
    cmp rbx, 8
    jb @@no_rex_dst
    cmp rsi, 8
    jb @@rex_w_r
    mov al, 4Dh  ; REX.W + REX.R + REX.B
    call emit_byte
    sub rbx, 8
    sub rsi, 8
    jmp @@cmp
@@rex_w_r:
    mov al, 4Ch  ; REX.W + REX.R
    call emit_byte
    sub rsi, 8
    jmp @@cmp
@@no_rex_dst:
    cmp rsi, 8
    jb @@rex_w
    mov al, 4Ch  ; REX.W + REX.R
    call emit_byte
    sub rsi, 8
    jmp @@cmp
@@rex_w:
    mov al, 48h  ; REX.W
    call emit_byte
@@cmp:
    mov al, 39h
    call emit_byte
    mov al, 0C0h
    or al, bl
    shl rsi, 3
    or al, sil
    call emit_byte
    pop rbx
    xor eax, eax
    ret
emit_cmp_reg_reg ENDP

PUBLIC emit_jz_rel
emit_jz_rel PROC
    ; RCX=target_offset
    lea r8, gState
    mov r9, QWORD PTR [r8+ST_CODE_USED]
    mov r10, rcx
    sub r10, r9
    sub r10, 6  ; jz rel32 is 6 bytes? Wait, 0F 84 rel32
    mov al, 0Fh
    call emit_byte
    mov al, 84h
    call emit_byte
    mov rcx, r10
    call emit_dword
    xor eax, eax
    ret
emit_jz_rel ENDP

PUBLIC emit_jnz_rel
emit_jnz_rel PROC
    ; RCX=target_offset
    lea r8, gState
    mov r9, QWORD PTR [r8+ST_CODE_USED]
    mov r10, rcx
    sub r10, r9
    sub r10, 6
    mov al, 0Fh
    call emit_byte
    mov al, 85h
    call emit_byte
    mov rcx, r10
    call emit_dword
    xor eax, eax
    ret
emit_jnz_rel ENDP

PUBLIC emit_inc_reg
emit_inc_reg PROC
    ; RCX=reg
    mov rbx, rcx
    cmp rbx, 8
    jb @@no_rex
    mov al, 49h  ; REX.W + REX.B
    call emit_byte
    sub rbx, 8
@@no_rex:
    mov al, 0FFh
    call emit_byte
    mov al, 0C0h
    or al, bl
    call emit_byte
    xor eax, eax
    ret
emit_inc_reg ENDP

PUBLIC emit_dec_reg
emit_dec_reg PROC
    ; RCX=reg
    mov rbx, rcx
    cmp rbx, 8
    jb @@no_rex
    mov al, 49h  ; REX.W + REX.B
    call emit_byte
    sub rbx, 8
@@no_rex:
    mov al, 0FFh
    call emit_byte
    mov al, 0C8h
    or al, bl
    call emit_byte
    xor eax, eax
    ret
emit_dec_reg ENDP

PUBLIC emit_lexer_incremental
emit_lexer_incremental PROC
    ; Emit lexer function: lex(rcx=input_ptr, rdx=input_len) -> rax=count of 'a'
    ; Implements a loop to scan the string

    call emit_prologue

    ; mov rbx, rcx  ; ptr
    mov rcx, 3  ; rbx
    mov rdx, 1  ; rcx
    call emit_mov_reg_reg

    ; mov rsi, rdx  ; len
    mov rcx, 6  ; rsi
    mov rdx, 2  ; rdx
    call emit_mov_reg_reg

    ; xor rdi, rdi  ; count
    mov rcx, 7  ; rdi
    mov rdx, 0
    call emit_mov_reg_imm

    ; Loop code: while len > 0 { if (*ptr == 'a') count++; ptr++; len--; }
    ; Emitted as bytes for simplicity

    lea rcx, lexer_loop_bytes
    mov rdx, lexer_loop_len
    call emit_bytes

    ; mov rax, rdi
    mov rcx, 0  ; rax
    mov rdx, 7  ; rdi
    call emit_mov_reg_reg

    call emit_epilogue

    xor eax, eax
    ret

lexer_loop_bytes:
    ; loop:
    ; cmp rsi, 0
    db 48h, 83h, 0FEh, 00h  ; cmp rsi, 0
    ; jz done (rel8, assume short)
    db 74h, 12h  ; jz +18 (to done)
    ; mov al, [rbx]
    db 8Ah, 03h
    ; cmp al, 'a'
    db 3Ch, 61h
    ; jnz skip
    db 75h, 02h  ; jnz +2
    ; inc rdi
    db 48h, 0FFh, 0C7h
    ; skip: inc rbx
    db 48h, 0FFh, 0C3h
    ; dec rsi
    db 48h, 0FFh, 0CEh
    ; jmp loop
    db 0EBh, 0E8h  ; jmp -24 (back to cmp)
    ; done:
lexer_loop_len equ $ - lexer_loop_bytes
emit_lexer_incremental ENDP

PUBLIC emit_parser_incremental
emit_parser_incremental PROC
    ; Emit parser: parse(rcx=token_count) -> rax=1 if token_count > 0
    call emit_prologue

    ; For simplicity, emit bytes
    lea rcx, parser_check_bytes
    mov rdx, parser_check_len
    call emit_bytes

    call emit_epilogue

    xor eax, eax
    ret

parser_check_bytes:
    ; cmp rcx, 0
    db 48h, 83h, 0F9h, 00h
    ; jz fail
    db 74h, 05h
    ; mov rax, 1
    db 48h, 0C7h, 0C0h, 01h, 00h, 00h, 00h
    ; jmp end
    db 0EBh, 07h
    ; fail: xor rax, rax
    db 48h, 31h, 0C0h
    ; end:
parser_check_len equ $ - parser_check_bytes
emit_parser_incremental ENDP

PUBLIC emit_lsp_initialize
emit_lsp_initialize PROC
    mov ecx, 1000
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_initialize ENDP

PUBLIC emit_lsp_send_request
emit_lsp_send_request PROC
    mov ecx, 1001
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_send_request ENDP

PUBLIC emit_lsp_receive_response
emit_lsp_receive_response PROC
    mov ecx, 1002
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_receive_response ENDP

PUBLIC emit_lsp_shutdown
emit_lsp_shutdown PROC
    mov ecx, 1003
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_shutdown ENDP

PUBLIC emit_lsp_hover
emit_lsp_hover PROC
    mov ecx, 1004
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_hover ENDP

PUBLIC emit_lsp_completion
emit_lsp_completion PROC
    mov ecx, 1005
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_completion ENDP

PUBLIC emit_lsp_definition
emit_lsp_definition PROC
    mov ecx, 1006
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_definition ENDP

PUBLIC emit_lsp_references
emit_lsp_references PROC
    mov ecx, 1007
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_references ENDP

PUBLIC emit_lsp_document_symbols
emit_lsp_document_symbols PROC
    mov ecx, 1008
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_document_symbols ENDP

PUBLIC emit_lsp_workspace_symbols
emit_lsp_workspace_symbols PROC
    mov ecx, 1009
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_workspace_symbols ENDP

PUBLIC emit_lsp_diagnostics
emit_lsp_diagnostics PROC
    mov ecx, 1010
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_diagnostics ENDP

PUBLIC emit_lsp_code_action
emit_lsp_code_action PROC
    mov ecx, 1011
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_code_action ENDP

PUBLIC emit_lsp_formatting
emit_lsp_formatting PROC
    mov ecx, 1012
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_formatting ENDP

PUBLIC emit_lsp_range_formatting
emit_lsp_range_formatting PROC
    mov ecx, 1013
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_range_formatting ENDP

PUBLIC emit_lsp_rename
emit_lsp_rename PROC
    mov ecx, 1014
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_rename ENDP

PUBLIC emit_lsp_prepare_rename
emit_lsp_prepare_rename PROC
    mov ecx, 1015
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_prepare_rename ENDP

PUBLIC emit_lsp_folding_ranges
emit_lsp_folding_ranges PROC
    mov ecx, 1016
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_folding_ranges ENDP

PUBLIC emit_lsp_selection_ranges
emit_lsp_selection_ranges PROC
    mov ecx, 1017
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_selection_ranges ENDP

PUBLIC emit_lsp_document_link
emit_lsp_document_link PROC
    mov ecx, 1018
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_document_link ENDP

PUBLIC emit_lsp_color_presentation
emit_lsp_color_presentation PROC
    mov ecx, 1019
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_color_presentation ENDP

PUBLIC emit_lsp_document_highlight
emit_lsp_document_highlight PROC
    mov ecx, 1020
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_document_highlight ENDP

PUBLIC emit_lsp_linked_editing_range
emit_lsp_linked_editing_range PROC
    mov ecx, 1021
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_linked_editing_range ENDP

PUBLIC emit_lsp_call_hierarchy
emit_lsp_call_hierarchy PROC
    mov ecx, 1022
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_call_hierarchy ENDP

PUBLIC emit_lsp_semantic_tokens
emit_lsp_semantic_tokens PROC
    mov ecx, 1023
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_lsp_semantic_tokens ENDP

PUBLIC emit_git_init
emit_git_init PROC
    mov ecx, 2000
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_init ENDP

PUBLIC emit_git_add
emit_git_add PROC
    mov ecx, 2001
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_add ENDP

PUBLIC emit_git_commit
emit_git_commit PROC
    mov ecx, 2002
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_commit ENDP

PUBLIC emit_git_push
emit_git_push PROC
    mov ecx, 2003
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_push ENDP

PUBLIC emit_git_pull
emit_git_pull PROC
    mov ecx, 2004
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_pull ENDP

PUBLIC emit_git_status
emit_git_status PROC
    mov ecx, 2005
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_status ENDP

PUBLIC emit_git_clone
emit_git_clone PROC
    mov ecx, 2006
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_clone ENDP

PUBLIC emit_git_branch
emit_git_branch PROC
    mov ecx, 2007
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_branch ENDP

PUBLIC emit_git_merge
emit_git_merge PROC
    mov ecx, 2008
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_merge ENDP

PUBLIC emit_git_log
emit_git_log PROC
    mov ecx, 2009
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_log ENDP

PUBLIC emit_git_diff
emit_git_diff PROC
    mov ecx, 2010
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_diff ENDP

PUBLIC emit_git_reset
emit_git_reset PROC
    mov ecx, 2011
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_reset ENDP

PUBLIC emit_git_revert
emit_git_revert PROC
    mov ecx, 2012
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_revert ENDP

PUBLIC emit_git_cherry_pick
emit_git_cherry_pick PROC
    mov ecx, 2013
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_cherry_pick ENDP

PUBLIC emit_git_rebase
emit_git_rebase PROC
    mov ecx, 2014
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_rebase ENDP

PUBLIC emit_git_stash
emit_git_stash PROC
    mov ecx, 2015
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_stash ENDP

PUBLIC emit_git_stash_pop
emit_git_stash_pop PROC
    mov ecx, 2016
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_stash_pop ENDP

PUBLIC emit_git_stash_apply
emit_git_stash_apply PROC
    mov ecx, 2017
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_stash_apply ENDP

PUBLIC emit_git_stash_drop
emit_git_stash_drop PROC
    mov ecx, 2018
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_stash_drop ENDP

PUBLIC emit_git_tag
emit_git_tag PROC
    mov ecx, 2019
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_tag ENDP

PUBLIC emit_git_tag_delete
emit_git_tag_delete PROC
    mov ecx, 2020
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_tag_delete ENDP

PUBLIC emit_git_remote_add
emit_git_remote_add PROC
    mov ecx, 2021
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_remote_add ENDP

PUBLIC emit_git_remote_remove
emit_git_remote_remove PROC
    mov ecx, 2022
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_remote_remove ENDP

PUBLIC emit_git_fetch
emit_git_fetch PROC
    mov ecx, 2023
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_fetch ENDP

PUBLIC emit_git_remote_prune
emit_git_remote_prune PROC
    mov ecx, 2024
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_remote_prune ENDP

PUBLIC emit_git_config
emit_git_config PROC
    mov ecx, 2025
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_config ENDP

PUBLIC emit_git_config_get
emit_git_config_get PROC
    mov ecx, 2026
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_config_get ENDP

PUBLIC emit_git_config_set
emit_git_config_set PROC
    mov ecx, 2027
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_config_set ENDP

PUBLIC emit_git_config_unset
emit_git_config_unset PROC
    mov ecx, 2028
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_config_unset ENDP

PUBLIC emit_git_clean
emit_git_clean PROC
    mov ecx, 2029
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_git_clean ENDP

PUBLIC emit_ai_init
emit_ai_init PROC
    mov ecx, 3000
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_init ENDP

PUBLIC emit_ai_query
emit_ai_query PROC
    mov ecx, 3001
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_query ENDP

PUBLIC emit_ai_train
emit_ai_train PROC
    mov ecx, 3002
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_train ENDP

PUBLIC emit_ai_predict
emit_ai_predict PROC
    mov ecx, 3003
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_predict ENDP

PUBLIC emit_ai_load_model
emit_ai_load_model PROC
    mov ecx, 3004
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_load_model ENDP

PUBLIC emit_ai_save_model
emit_ai_save_model PROC
    mov ecx, 3005
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_save_model ENDP

PUBLIC emit_ai_tokenize
emit_ai_tokenize PROC
    mov ecx, 3006
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_tokenize ENDP

PUBLIC emit_ai_detokenize
emit_ai_detokenize PROC
    mov ecx, 3007
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_detokenize ENDP

PUBLIC emit_ai_embed
emit_ai_embed PROC
    mov ecx, 3008
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_embed ENDP

PUBLIC emit_ai_generate
emit_ai_generate PROC
    mov ecx, 3009
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_generate ENDP

PUBLIC emit_ai_classify
emit_ai_classify PROC
    mov ecx, 3010
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_classify ENDP

PUBLIC emit_ai_translate
emit_ai_translate PROC
    mov ecx, 3011
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_translate ENDP

PUBLIC emit_ai_summarize
emit_ai_summarize PROC
    mov ecx, 3012
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_summarize ENDP

PUBLIC emit_ai_ner
emit_ai_ner PROC
    mov ecx, 3013
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_ner ENDP

PUBLIC emit_ai_sentiment
emit_ai_sentiment PROC
    mov ecx, 3014
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_sentiment ENDP

PUBLIC emit_ai_qa
emit_ai_qa PROC
    mov ecx, 3015
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_qa ENDP

PUBLIC emit_ai_chat
emit_ai_chat PROC
    mov ecx, 3016
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_chat ENDP

PUBLIC emit_ai_code_completion
emit_ai_code_completion PROC
    mov ecx, 3017
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_code_completion ENDP

PUBLIC emit_ai_image_generate
emit_ai_image_generate PROC
    mov ecx, 3018
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_image_generate ENDP

PUBLIC emit_ai_audio_transcribe
emit_ai_audio_transcribe PROC
    mov ecx, 3019
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_audio_transcribe ENDP

PUBLIC emit_ai_audio_synthesize
emit_ai_audio_synthesize PROC
    mov ecx, 3020
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_audio_synthesize ENDP

PUBLIC emit_ai_finetune
emit_ai_finetune PROC
    mov ecx, 3021
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_finetune ENDP

PUBLIC emit_ai_evaluate
emit_ai_evaluate PROC
    mov ecx, 3022
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_evaluate ENDP

PUBLIC emit_ai_quantize
emit_ai_quantize PROC
    mov ecx, 3023
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_quantize ENDP

PUBLIC emit_ai_dequantize
emit_ai_dequantize PROC
    mov ecx, 3024
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_dequantize ENDP

PUBLIC emit_ai_optimize
emit_ai_optimize PROC
    mov ecx, 3025
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_ai_optimize ENDP

PUBLIC emit_voice_init
emit_voice_init PROC
    mov ecx, 4000
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_init ENDP

PUBLIC emit_voice_listen
emit_voice_listen PROC
    mov ecx, 4001
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_listen ENDP

PUBLIC emit_voice_speak
emit_voice_speak PROC
    mov ecx, 4002
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_speak ENDP

PUBLIC emit_voice_recognize
emit_voice_recognize PROC
    mov ecx, 4003
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_recognize ENDP

PUBLIC emit_voice_synthesize
emit_voice_synthesize PROC
    mov ecx, 4004
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_synthesize ENDP

PUBLIC emit_voice_pause
emit_voice_pause PROC
    mov ecx, 4005
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_pause ENDP

PUBLIC emit_voice_resume
emit_voice_resume PROC
    mov ecx, 4006
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_resume ENDP

PUBLIC emit_voice_stop
emit_voice_stop PROC
    mov ecx, 4007
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_stop ENDP

PUBLIC emit_voice_volume_up
emit_voice_volume_up PROC
    mov ecx, 4008
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_volume_up ENDP

PUBLIC emit_voice_volume_down
emit_voice_volume_down PROC
    mov ecx, 4009
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_volume_down ENDP

PUBLIC emit_voice_mute
emit_voice_mute PROC
    mov ecx, 4010
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_mute ENDP

PUBLIC emit_voice_unmute
emit_voice_unmute PROC
    mov ecx, 4011
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_unmute ENDP

PUBLIC emit_voice_speed_up
emit_voice_speed_up PROC
    mov ecx, 4012
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_speed_up ENDP

PUBLIC emit_voice_speed_down
emit_voice_speed_down PROC
    mov ecx, 4013
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_speed_down ENDP

PUBLIC emit_voice_pitch_up
emit_voice_pitch_up PROC
    mov ecx, 4014
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_pitch_up ENDP

PUBLIC emit_voice_pitch_down
emit_voice_pitch_down PROC
    mov ecx, 4015
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_pitch_down ENDP

PUBLIC emit_voice_echo
emit_voice_echo PROC
    mov ecx, 4016
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_echo ENDP

PUBLIC emit_voice_reverb
emit_voice_reverb PROC
    mov ecx, 4017
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_reverb ENDP

PUBLIC emit_voice_filter
emit_voice_filter PROC
    mov ecx, 4018
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_filter ENDP

PUBLIC emit_voice_equalizer
emit_voice_equalizer PROC
    mov ecx, 4019
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_equalizer ENDP

PUBLIC emit_voice_record
emit_voice_record PROC
    mov ecx, 4020
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_record ENDP

PUBLIC emit_voice_playback
emit_voice_playback PROC
    mov ecx, 4021
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_playback ENDP

PUBLIC emit_voice_save
emit_voice_save PROC
    mov ecx, 4022
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_save ENDP

PUBLIC emit_voice_load
emit_voice_load PROC
    mov ecx, 4023
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_load ENDP

PUBLIC emit_voice_delete
emit_voice_delete PROC
    mov ecx, 4024
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_voice_delete ENDP

reveng_validate_input PROC
    ; RCX=buf, RDX=len -> EAX=0 if valid, IDE_FAIL otherwise
    test rcx, rcx
    jz  @@bad
    test rdx, rdx
    jz  @@bad
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@bad:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 2
    mov eax, IDE_FAIL
    ret
reveng_validate_input ENDP

reveng_count_byte PROC
    ; RCX=buf, RDX=len, R8B=needle -> EAX=count
    push rsi
    xor eax, eax
    mov rsi, rcx
    test rdx, rdx
    jz  @@done
@@loop:
    cmp BYTE PTR [rsi], r8b
    jne @F
    inc eax
@@:
    inc rsi
    dec rdx
    jnz @@loop
@@done:
    pop rsi
    ret
reveng_count_byte ENDP

reveng_count_pair PROC
    ; RCX=buf, RDX=len, R8B=b0, R9B=b1 -> EAX=count of adjacent pairs
    push rsi
    xor eax, eax
    cmp rdx, 2
    jb  @@done
    mov rsi, rcx
    dec rdx
@@loop:
    cmp BYTE PTR [rsi], r8b
    jne @F
    cmp BYTE PTR [rsi+1], r9b
    jne @F
    inc eax
@@:
    inc rsi
    dec rdx
    jnz @@loop
@@done:
    pop rsi
    ret
reveng_count_pair ENDP

reveng_count_ascii_strings PROC
    ; RCX=buf, RDX=len -> EAX=count of printable ASCII runs length >=4
    push rsi
    xor eax, eax                  ; count
    xor r9d, r9d                  ; current run length
    mov rsi, rcx
    test rdx, rdx
    jz  @@tail
@@loop:
    movzx r8d, BYTE PTR [rsi]
    cmp r8b, 20h
    jb  @@break
    cmp r8b, 7Eh
    ja  @@break
    inc r9d
    jmp @@next
@@break:
    cmp r9d, 4
    jb  @F
    inc eax
@@:
    xor r9d, r9d
@@next:
    inc rsi
    dec rdx
    jnz @@loop
@@tail:
    cmp r9d, 4
    jb  @@done
    inc eax
@@done:
    pop rsi
    ret
reveng_count_ascii_strings ENDP

PUBLIC reveng_disassemble
reveng_disassemble PROC
    ; RCX=buf, RDX=len -> EAX=instruction estimate score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx

    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0E8h
    call reveng_count_byte
    mov r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0E9h
    call reveng_count_byte
    add r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0EBh
    call reveng_count_byte
    add r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0C3h
    call reveng_count_byte
    add r10d, eax

    mov eax, r10d
    test eax, eax
    jnz @@ok
    mov rax, rsi
    shr eax, 4
    inc eax
@@ok:
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_disassemble ENDP

PUBLIC reveng_decompile
reveng_decompile PROC
    ; RCX=buf, RDX=len -> EAX=stable pseudo-decompile signature
    call reveng_validate_input
    test eax, eax
    jnz @@fail
    call IDE_fnv1a64
    and eax, 7FFFFFFFh
    ret
@@fail:
    mov eax, IDE_FAIL
    ret
reveng_decompile ENDP

PUBLIC reveng_analyze
reveng_analyze PROC
    ; RCX=buf, RDX=len -> EAX=control/return opcode density score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0E8h
    call reveng_count_byte
    mov r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0E9h
    call reveng_count_byte
    add r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0C3h
    call reveng_count_byte
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_analyze ENDP

PUBLIC reveng_patch
reveng_patch PROC
    ; RCX=buf, RDX=len, R8=offset, R9B=newByte -> EAX=oldByte or IDE_FAIL
    push rbx
    mov rbx, rcx
    call reveng_validate_input
    test eax, eax
    jnz @@fail
    cmp r8, rdx
    jae @@bad
    add rbx, r8
    movzx eax, BYTE PTR [rbx]
    mov BYTE PTR [rbx], r9b
    lea r10, gState
    mov DWORD PTR [r10+ST_LAST_ERROR], 0
    pop rbx
    ret
@@bad:
    lea r10, gState
    mov DWORD PTR [r10+ST_LAST_ERROR], 13
@@fail:
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_patch ENDP

PUBLIC reveng_debug
reveng_debug PROC
    ; RCX=buf, RDX=len -> EAX=count(INT3)
    call reveng_validate_input
    test eax, eax
    jnz @@fail
    mov r8b, 0CCh
    call reveng_count_byte
    ret
@@fail:
    mov eax, IDE_FAIL
    ret
reveng_debug ENDP

PUBLIC reveng_hex_dump
reveng_hex_dump PROC
    ; RCX=buf, RDX=len -> EAX=required chars for "XX " format
    call reveng_validate_input
    test eax, eax
    jnz @@fail
    lea rax, [rdx+rdx*2]
    ret
@@fail:
    mov eax, IDE_FAIL
    ret
reveng_hex_dump ENDP

PUBLIC reveng_string_search
reveng_string_search PROC
    ; RCX=buf, RDX=len -> EAX=count(printable strings >= 4 chars)
    call reveng_validate_input
    test eax, eax
    jnz @@fail
    call reveng_count_ascii_strings
    ret
@@fail:
    mov eax, IDE_FAIL
    ret
reveng_string_search ENDP

PUBLIC reveng_function_graph
reveng_function_graph PROC
    ; RCX=buf, RDX=len -> EAX=estimated function entry count
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 55h
    mov r9b, 48h
    call reveng_count_pair
    mov r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 48h
    mov r9b, 83h
    call reveng_count_pair
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_function_graph ENDP

PUBLIC reveng_control_flow
reveng_control_flow PROC
    ; RCX=buf, RDX=len -> EAX=branch/call/ret score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0E8h
    call reveng_count_byte
    mov r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0E9h
    call reveng_count_byte
    add r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0EBh
    call reveng_count_byte
    add r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0C3h
    call reveng_count_byte
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_control_flow ENDP

PUBLIC reveng_data_flow
reveng_data_flow PROC
    ; RCX=buf, RDX=len -> EAX=data movement opcode score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 88h
    call reveng_count_byte
    mov r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 89h
    call reveng_count_byte
    add r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 8Bh
    call reveng_count_byte
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_data_flow ENDP

PUBLIC reveng_vulnerability_scan
reveng_vulnerability_scan PROC
    ; RCX=buf, RDX=len -> EAX=suspicious pattern score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0FFh
    mov r9b, 0E4h
    call reveng_count_pair
    mov r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0CDh
    mov r9b, 080h
    call reveng_count_pair
    add r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0Fh
    mov r9b, 05h
    call reveng_count_pair
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_vulnerability_scan ENDP

PUBLIC reveng_exploit_dev
reveng_exploit_dev PROC
    ; RCX=buf, RDX=len -> EAX=ROP-ish score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0C3h
    call reveng_count_byte
    mov r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0C2h
    call reveng_count_byte
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_exploit_dev ENDP

PUBLIC reveng_signature_scan
reveng_signature_scan PROC
    ; RCX=buf, RDX=len -> EAX=signature hash
    push rbx
    mov rbx, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail
    call IDE_fnv1a64
    rol eax, 5
    xor eax, ebx
    pop rbx
    ret
@@fail:
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_signature_scan ENDP

PUBLIC reveng_unpack
reveng_unpack PROC
    ; RCX=buf, RDX=len -> EAX=embedded payload/header count
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 4Dh
    mov r9b, 5Ah
    call reveng_count_pair
    mov r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 50h
    mov r9b, 4Bh
    call reveng_count_pair
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_unpack ENDP

PUBLIC reveng_obfuscation_detect
reveng_obfuscation_detect PROC
    ; RCX=buf, RDX=len -> EAX=obfuscation/junk score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 90h
    call reveng_count_byte
    mov r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0CCh
    call reveng_count_byte
    add r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0F4h
    call reveng_count_byte
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_obfuscation_detect ENDP

PUBLIC reveng_crypto_analysis
reveng_crypto_analysis PROC
    ; RCX=buf, RDX=len -> EAX=crypto-like opcode score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 31h
    call reveng_count_byte
    mov r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 33h
    call reveng_count_byte
    add r10d, eax
    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0C1h
    call reveng_count_byte
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_crypto_analysis ENDP

PUBLIC reveng_network_trace
reveng_network_trace PROC
    ; RCX=buf, RDX=len -> EAX=network marker score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 'h'
    mov r9b, 't'
    call reveng_count_pair
    mov r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 'G'
    mov r9b, 'E'
    call reveng_count_pair
    add r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 'P'
    mov r9b, 'O'
    call reveng_count_pair
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_network_trace ENDP

PUBLIC reveng_file_carve
reveng_file_carve PROC
    ; RCX=buf, RDX=len -> EAX=embedded file signature count
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 4Dh
    mov r9b, 5Ah
    call reveng_count_pair
    mov r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 50h
    mov r9b, 4Bh
    call reveng_count_pair
    add r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0FFh
    mov r9b, 0D8h
    call reveng_count_pair
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_file_carve ENDP

PUBLIC reveng_memory_dump
reveng_memory_dump PROC
    ; RCX=buf, RDX=len -> EAX=bytes copied into diagnostic buffer
    push rbx
    push rsi
    push rdi
    mov rsi, rcx
    mov rbx, rdx

    call reveng_validate_input
    test eax, eax
    jnz @@fail

    lea r10, gState
    mov rdi, QWORD PTR [r10+ST_DIAG_BUF]
    test rdi, rdi
    jz  @@no_diag

    mov eax, DWORD PTR [r10+ST_DIAG_CAP]
    mov ecx, DWORD PTR [r10+ST_DIAG_USED]
    cmp ecx, eax
    jae @@no_room

    sub eax, ecx                  ; available bytes
    mov r8d, eax
    mov rax, rbx                  ; requested bytes
    cmp rax, r8
    jbe @F
    mov rax, r8
@@:
    add rdi, rcx                  ; destination = diag + used
    mov rcx, rdi
    mov rdx, rsi
    mov r8, rax
    call IDE_memcpy

    add DWORD PTR [r10+ST_DIAG_USED], eax
    mov DWORD PTR [r10+ST_LAST_ERROR], 0
    pop rdi
    pop rsi
    pop rbx
    ret

@@no_diag:
    mov DWORD PTR [r10+ST_LAST_ERROR], 20
    jmp @@fail_common
@@no_room:
    mov DWORD PTR [r10+ST_LAST_ERROR], 21
    jmp @@fail_common
@@fail:
    lea r10, gState
@@fail_common:
    pop rdi
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_memory_dump ENDP

PUBLIC reveng_process_inject
reveng_process_inject PROC
    ; RCX=buf, RDX=len -> EAX=injection-like pattern score
    push rbx
    push rsi
    mov rbx, rcx
    mov rsi, rdx
    call reveng_validate_input
    test eax, eax
    jnz @@fail

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0FFh
    mov r9b, 0D0h                ; call rax style
    call reveng_count_pair
    mov r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 0FFh
    mov r9b, 0E0h                ; jmp rax style
    call reveng_count_pair
    add r10d, eax

    mov rcx, rbx
    mov rdx, rsi
    mov r8b, 68h                 ; push imm32
    call reveng_count_byte
    add r10d, eax

    mov eax, r10d
    pop rsi
    pop rbx
    ret
@@fail:
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
reveng_process_inject ENDP

PUBLIC emit_reveng_disassemble
emit_reveng_disassemble PROC
    mov ecx, 5000
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_disassemble ENDP

PUBLIC emit_reveng_decompile
emit_reveng_decompile PROC
    mov ecx, 5001
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_decompile ENDP

PUBLIC emit_reveng_analyze
emit_reveng_analyze PROC
    mov ecx, 5002
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_analyze ENDP

PUBLIC emit_reveng_patch
emit_reveng_patch PROC
    mov ecx, 5003
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_patch ENDP

PUBLIC emit_reveng_debug
emit_reveng_debug PROC
    mov ecx, 5004
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_debug ENDP

PUBLIC emit_reveng_hex_dump
emit_reveng_hex_dump PROC
    mov ecx, 5005
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_hex_dump ENDP

PUBLIC emit_reveng_string_search
emit_reveng_string_search PROC
    mov ecx, 5006
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_string_search ENDP

PUBLIC emit_reveng_function_graph
emit_reveng_function_graph PROC
    mov ecx, 5007
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_function_graph ENDP

PUBLIC emit_reveng_control_flow
emit_reveng_control_flow PROC
    mov ecx, 5008
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_control_flow ENDP

PUBLIC emit_reveng_data_flow
emit_reveng_data_flow PROC
    mov ecx, 5009
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_data_flow ENDP

PUBLIC emit_reveng_vulnerability_scan
emit_reveng_vulnerability_scan PROC
    mov ecx, 5010
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_vulnerability_scan ENDP

PUBLIC emit_reveng_exploit_dev
emit_reveng_exploit_dev PROC
    mov ecx, 5011
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_exploit_dev ENDP

PUBLIC emit_reveng_signature_scan
emit_reveng_signature_scan PROC
    mov ecx, 5012
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_signature_scan ENDP

PUBLIC emit_reveng_unpack
emit_reveng_unpack PROC
    mov ecx, 5013
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_unpack ENDP

PUBLIC emit_reveng_obfuscation_detect
emit_reveng_obfuscation_detect PROC
    mov ecx, 5014
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_obfuscation_detect ENDP

PUBLIC emit_reveng_crypto_analysis
emit_reveng_crypto_analysis PROC
    mov ecx, 5015
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_crypto_analysis ENDP

PUBLIC emit_reveng_network_trace
emit_reveng_network_trace PROC
    mov ecx, 5016
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_network_trace ENDP

PUBLIC emit_reveng_file_carve
emit_reveng_file_carve PROC
    mov ecx, 5017
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_file_carve ENDP

PUBLIC emit_reveng_memory_dump
emit_reveng_memory_dump PROC
    mov ecx, 5018
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_memory_dump ENDP

PUBLIC emit_reveng_process_inject
emit_reveng_process_inject PROC
    mov ecx, 5019
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_process_inject ENDP

PUBLIC emit_reveng_hook_install
emit_reveng_hook_install PROC
    mov ecx, 5020
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_hook_install ENDP

PUBLIC emit_reveng_hook_remove
emit_reveng_hook_remove PROC
    mov ecx, 5021
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_hook_remove ENDP

PUBLIC emit_reveng_fuzzer
emit_reveng_fuzzer PROC
    mov ecx, 5022
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_fuzzer ENDP

PUBLIC emit_reveng_symbol_resolve
emit_reveng_symbol_resolve PROC
    mov ecx, 5023
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_symbol_resolve ENDP

PUBLIC emit_reveng_type_inference
emit_reveng_type_inference PROC
    mov ecx, 5024
    call emit_feature_dispatch
    xor eax, eax
    ret
emit_reveng_type_inference ENDP

PUBLIC lsp_initialize
lsp_initialize PROC
    lea r8, gState
    mov DWORD PTR [r8+ST_LSP_INIT], 1
    mov DWORD PTR [r8+ST_LSP_REQ_COUNT], 0
    mov DWORD PTR [r8+ST_LSP_RESP_COUNT], 0
    mov QWORD PTR [r8+ST_LSP_LAST_REQ_PTR], 0
    mov QWORD PTR [r8+ST_LSP_LAST_RESP_PTR], 0
    mov QWORD PTR [r8+ST_LSP_LAST_REQ_LEN], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    ret
lsp_initialize ENDP

PUBLIC lsp_send_request
lsp_send_request PROC
    ; RCX=requestPtr, RDX=requestLen
    push rbx
    lea rbx, gState

    cmp DWORD PTR [rbx+ST_LSP_INIT], 1
    jne @@fail_not_init
    test rcx, rcx
    jz @@fail_bad_args
    test rdx, rdx
    jz @@fail_bad_args

    mov QWORD PTR [rbx+ST_LSP_LAST_REQ_PTR], rcx
    mov QWORD PTR [rbx+ST_LSP_LAST_REQ_LEN], rdx
    mov QWORD PTR [rbx+ST_LSP_LAST_RESP_PTR], 0
    inc DWORD PTR [rbx+ST_LSP_REQ_COUNT]
    mov DWORD PTR [rbx+ST_LAST_ERROR], 0
    pop rbx
    xor eax, eax
    ret

@@fail_not_init:
    mov DWORD PTR [rbx+ST_LAST_ERROR], 1
    pop rbx
    mov eax, IDE_FAIL
    ret
@@fail_bad_args:
    mov DWORD PTR [rbx+ST_LAST_ERROR], 2
    pop rbx
    mov eax, IDE_FAIL
    ret
lsp_send_request ENDP

PUBLIC lsp_receive_response
lsp_receive_response PROC
    ; RCX=outBuf, RDX=outCap
    push rbx
    push rsi
    push rdi

    lea rbx, gState
    cmp DWORD PTR [rbx+ST_LSP_INIT], 1
    jne @@fail_not_init
    test rcx, rcx
    jz @@fail_bad_args
    cmp rdx, 2
    jb @@fail_bad_args

    mov rsi, QWORD PTR [rbx+ST_LSP_LAST_REQ_PTR]
    test rsi, rsi
    jz @@fail_no_req

    ; Copy min(last_req_len, outCap-1) and NUL-terminate.
    mov r8, QWORD PTR [rbx+ST_LSP_LAST_REQ_LEN]
    mov r9, rdx
    dec r9
    cmp r8, r9
    jbe @F
    mov r8, r9
@@:
    mov rdi, rcx
    mov rcx, rdi
    mov rdx, rsi
    call IDE_memcpy

    mov BYTE PTR [rdi+r8], 0
    mov QWORD PTR [rbx+ST_LSP_LAST_RESP_PTR], rdi
    inc DWORD PTR [rbx+ST_LSP_RESP_COUNT]
    mov DWORD PTR [rbx+ST_LAST_ERROR], 0

    ; Return bytes written.
    mov eax, r8d
    pop rdi
    pop rsi
    pop rbx
    ret

@@fail_not_init:
    mov DWORD PTR [rbx+ST_LAST_ERROR], 1
    jmp @@fail
@@fail_bad_args:
    mov DWORD PTR [rbx+ST_LAST_ERROR], 2
    jmp @@fail
@@fail_no_req:
    mov DWORD PTR [rbx+ST_LAST_ERROR], 3
@@fail:
    pop rdi
    pop rsi
    pop rbx
    mov eax, IDE_FAIL
    ret
lsp_receive_response ENDP

PUBLIC lsp_shutdown
lsp_shutdown PROC
    lea r8, gState
    mov DWORD PTR [r8+ST_LSP_INIT], 0
    mov QWORD PTR [r8+ST_LSP_LAST_REQ_PTR], 0
    mov QWORD PTR [r8+ST_LSP_LAST_RESP_PTR], 0
    mov QWORD PTR [r8+ST_LSP_LAST_REQ_LEN], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
lsp_shutdown ENDP

PUBLIC lsp_hover
lsp_hover PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_hover ENDP

PUBLIC lsp_completion
lsp_completion PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_completion ENDP

PUBLIC lsp_definition
lsp_definition PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_definition ENDP

PUBLIC lsp_references
lsp_references PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_references ENDP

PUBLIC lsp_document_symbols
lsp_document_symbols PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_document_symbols ENDP

PUBLIC lsp_workspace_symbols
lsp_workspace_symbols PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_workspace_symbols ENDP

PUBLIC lsp_diagnostics
lsp_diagnostics PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_diagnostics ENDP

PUBLIC lsp_code_action
lsp_code_action PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_code_action ENDP

PUBLIC lsp_formatting
lsp_formatting PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_formatting ENDP

PUBLIC lsp_range_formatting
lsp_range_formatting PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_range_formatting ENDP

PUBLIC lsp_rename
lsp_rename PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_rename ENDP

PUBLIC lsp_prepare_rename
lsp_prepare_rename PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_prepare_rename ENDP

PUBLIC lsp_folding_ranges
lsp_folding_ranges PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_folding_ranges ENDP

PUBLIC lsp_selection_ranges
lsp_selection_ranges PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_selection_ranges ENDP

PUBLIC lsp_document_link
lsp_document_link PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_document_link ENDP

PUBLIC lsp_color_presentation
lsp_color_presentation PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_color_presentation ENDP

PUBLIC lsp_document_highlight
lsp_document_highlight PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_document_highlight ENDP

PUBLIC lsp_linked_editing_range
lsp_linked_editing_range PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_linked_editing_range ENDP

PUBLIC lsp_call_hierarchy
lsp_call_hierarchy PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_call_hierarchy ENDP

PUBLIC lsp_semantic_tokens
lsp_semantic_tokens PROC
    ; RCX=requestPtr, RDX=requestLen
    call lsp_send_request
    ret
lsp_semantic_tokens ENDP

PUBLIC git_init
git_init PROC
    lea r8, gState
    mov DWORD PTR [r8+ST_GIT_INIT], 1
    mov DWORD PTR [r8+ST_GIT_STAGED], 0
    mov DWORD PTR [r8+ST_GIT_COMMITS], 0
    mov DWORD PTR [r8+ST_GIT_BRANCHES], 1
    mov DWORD PTR [r8+ST_GIT_MERGES], 0
    mov DWORD PTR [r8+ST_GIT_STASH], 0
    mov DWORD PTR [r8+ST_GIT_SYNC], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
git_init ENDP

git_require_init PROC
    lea r8, gState
    cmp DWORD PTR [r8+ST_GIT_INIT], 1
    je  @F
    mov DWORD PTR [r8+ST_LAST_ERROR], 10
    mov eax, IDE_FAIL
    ret
@@:
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
git_require_init ENDP

PUBLIC git_add
git_add PROC
    ; RCX=filePathPtr
    call git_require_init
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    lea r8, gState
    inc DWORD PTR [r8+ST_GIT_STAGED]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 11
    mov eax, IDE_FAIL
@@:
    ret
git_add ENDP

PUBLIC git_commit
git_commit PROC
    ; RCX=commitMessagePtr
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    test rcx, rcx
    jz  @@bad_args
    cmp DWORD PTR [r8+ST_GIT_STAGED], 0
    jbe @@nothing
    inc DWORD PTR [r8+ST_GIT_COMMITS]
    mov DWORD PTR [r8+ST_GIT_STAGED], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@bad_args:
    mov DWORD PTR [r8+ST_LAST_ERROR], 11
    mov eax, IDE_FAIL
    ret
@@nothing:
    mov DWORD PTR [r8+ST_LAST_ERROR], 12
    mov eax, IDE_FAIL
    ret
@@:
    ret
git_commit ENDP

PUBLIC git_push
git_push PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    inc DWORD PTR [r8+ST_GIT_SYNC]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@:
    ret
git_push ENDP

PUBLIC git_pull
git_pull PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    inc DWORD PTR [r8+ST_GIT_SYNC]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@:
    ret
git_pull ENDP

PUBLIC git_status
git_status PROC
    ; Return packed status: high16=commits, low16=staged
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_GIT_STAGED]
    and eax, 0FFFFh
    mov edx, DWORD PTR [r8+ST_GIT_COMMITS]
    shl edx, 16
    or eax, edx
    ret
@@:
    ret
git_status ENDP

PUBLIC git_clone
git_clone PROC
    ; RCX=repoUrlPtr
    test rcx, rcx
    jz  @@bad_args
    ; cloning initializes a fresh repo state
    call git_init
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 11
    mov eax, IDE_FAIL
    ret
git_clone ENDP

PUBLIC git_branch
git_branch PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    inc DWORD PTR [r8+ST_GIT_BRANCHES]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@:
    ret
git_branch ENDP

PUBLIC git_merge
git_merge PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    cmp DWORD PTR [r8+ST_GIT_BRANCHES], 2
    jb  @@fail
    inc DWORD PTR [r8+ST_GIT_MERGES]
    dec DWORD PTR [r8+ST_GIT_BRANCHES]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@fail:
    mov DWORD PTR [r8+ST_LAST_ERROR], 13
    mov eax, IDE_FAIL
    ret
@@:
    ret
git_merge ENDP

PUBLIC git_log
git_log PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_GIT_COMMITS]
    ret
@@:
    ret
git_log ENDP

PUBLIC git_diff
git_diff PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_GIT_STAGED]
    ret
@@:
    ret
git_diff ENDP

PUBLIC git_reset
git_reset PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov DWORD PTR [r8+ST_GIT_STAGED], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@:
    ret
git_reset ENDP

PUBLIC git_revert
git_revert PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    cmp DWORD PTR [r8+ST_GIT_COMMITS], 1
    jb  @@fail
    dec DWORD PTR [r8+ST_GIT_COMMITS]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@fail:
    mov DWORD PTR [r8+ST_LAST_ERROR], 14
    mov eax, IDE_FAIL
    ret
@@:
    ret
git_revert ENDP

PUBLIC git_cherry_pick
git_cherry_pick PROC
    ; RCX=commitIdPtr
    call git_require_init
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    lea r8, gState
    inc DWORD PTR [r8+ST_GIT_COMMITS]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 11
    mov eax, IDE_FAIL
    ret
@@:
    ret
git_cherry_pick ENDP

PUBLIC git_rebase
git_rebase PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov DWORD PTR [r8+ST_GIT_STAGED], 0
    mov DWORD PTR [r8+ST_GIT_MERGES], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@:
    ret
git_rebase ENDP

PUBLIC git_stash
git_stash PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    cmp DWORD PTR [r8+ST_GIT_STAGED], 0
    jbe @@fail
    inc DWORD PTR [r8+ST_GIT_STASH]
    mov DWORD PTR [r8+ST_GIT_STAGED], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@fail:
    mov DWORD PTR [r8+ST_LAST_ERROR], 12
    mov eax, IDE_FAIL
    ret
@@:
    ret
git_stash ENDP

PUBLIC git_stash_pop
git_stash_pop PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    cmp DWORD PTR [r8+ST_GIT_STASH], 0
    jbe @@fail
    dec DWORD PTR [r8+ST_GIT_STASH]
    inc DWORD PTR [r8+ST_GIT_STAGED]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@fail:
    mov DWORD PTR [r8+ST_LAST_ERROR], 15
    mov eax, IDE_FAIL
    ret
@@:
    ret
git_stash_pop ENDP

PUBLIC git_stash_apply
git_stash_apply PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    cmp DWORD PTR [r8+ST_GIT_STASH], 0
    jbe @@fail
    inc DWORD PTR [r8+ST_GIT_STAGED]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@fail:
    mov DWORD PTR [r8+ST_LAST_ERROR], 15
    mov eax, IDE_FAIL
    ret
@@:
    ret
git_stash_apply ENDP

PUBLIC git_stash_drop
git_stash_drop PROC
    call git_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    cmp DWORD PTR [r8+ST_GIT_STASH], 0
    jbe @@fail
    dec DWORD PTR [r8+ST_GIT_STASH]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@fail:
    mov DWORD PTR [r8+ST_LAST_ERROR], 15
    mov eax, IDE_FAIL
    ret
@@:
    ret
git_stash_drop ENDP

PUBLIC git_tag
git_tag PROC
    ; RCX=tagNamePtr
    call git_require_init
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    mov eax, DWORD PTR [r8+ST_GIT_COMMITS]
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 11
    mov eax, IDE_FAIL
    ret
@@:
    ret
git_tag ENDP

PUBLIC ai_init
ai_init PROC
    lea r8, gState
    mov DWORD PTR [r8+ST_AI_INIT], 1
    mov DWORD PTR [r8+ST_AI_MODEL_LOADED], 0
    mov DWORD PTR [r8+ST_AI_QUERY_COUNT], 0
    mov DWORD PTR [r8+ST_AI_TOKEN_COUNT], 0
    mov DWORD PTR [r8+ST_AI_GENERATION_COUNT], 0
    mov QWORD PTR [r8+ST_AI_LAST_INPUT_PTR], 0
    mov QWORD PTR [r8+ST_AI_LAST_OUTPUT_PTR], 0
    mov DWORD PTR [r8+ST_AI_FLAGS], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
ai_init ENDP

ai_require_init PROC
    lea r8, gState
    cmp DWORD PTR [r8+ST_AI_INIT], 1
    je  @F
    mov DWORD PTR [r8+ST_LAST_ERROR], 20
    mov eax, IDE_FAIL
    ret
@@:
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
ai_require_init ENDP

ai_require_model PROC
    call ai_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    cmp DWORD PTR [r8+ST_AI_MODEL_LOADED], 1
    je  @@ok
    mov DWORD PTR [r8+ST_LAST_ERROR], 21
    mov eax, IDE_FAIL
    ret
@@ok:
    xor eax, eax
    ret
@@:
    ret
ai_require_model ENDP

PUBLIC ai_query
ai_query PROC
    ; RCX=inputPtr, RDX=inputLen
    call ai_require_model
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    lea r8, gState
    mov QWORD PTR [r8+ST_AI_LAST_INPUT_PTR], rcx
    inc DWORD PTR [r8+ST_AI_QUERY_COUNT]
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 22
    mov eax, IDE_FAIL
    ret
@@:
    xor eax, eax
    ret
ai_query ENDP

PUBLIC ai_train
ai_train PROC
    ; RCX=datasetPtr
    call ai_require_model
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    lea r8, gState
    or DWORD PTR [r8+ST_AI_FLAGS], 1
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 22
    mov eax, IDE_FAIL
    ret
@@:
    xor eax, eax
    ret
ai_train ENDP

PUBLIC ai_predict
ai_predict PROC
    ; RCX=inputPtr, RDX=inputLen
    call ai_query
    ret
ai_predict ENDP

PUBLIC ai_load_model
ai_load_model PROC
    ; RCX=modelPathPtr
    call ai_require_init
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    lea r8, gState
    mov DWORD PTR [r8+ST_AI_MODEL_LOADED], 1
    mov QWORD PTR [r8+ST_AI_LAST_INPUT_PTR], rcx
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 22
    mov eax, IDE_FAIL
    ret
@@:
    ret
ai_load_model ENDP

PUBLIC ai_unload_model
ai_unload_model PROC
    call ai_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov DWORD PTR [r8+ST_AI_MODEL_LOADED], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@:
    ret
ai_unload_model ENDP

PUBLIC ai_save_model
ai_save_model PROC
    ; RCX=pathPtr
    call ai_require_model
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    lea r8, gState
    mov QWORD PTR [r8+ST_AI_LAST_OUTPUT_PTR], rcx
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 22
    mov eax, IDE_FAIL
    ret
@@:
    ret
ai_save_model ENDP

PUBLIC ai_tokenize
ai_tokenize PROC
    ; RCX=textPtr, RDX=len(0=auto)
    call ai_require_model
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    test rdx, rdx
    jnz @@have_len
    push rcx
    call IDE_strlen
    mov rdx, rax
    pop rcx
@@have_len:
    lea r8, gState
    mov QWORD PTR [r8+ST_AI_LAST_INPUT_PTR], rcx
    mov eax, edx
    mov DWORD PTR [r8+ST_AI_TOKEN_COUNT], eax
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 22
    mov eax, IDE_FAIL
    ret
@@:
    ret
ai_tokenize ENDP

PUBLIC ai_detokenize
ai_detokenize PROC
    ; RCX=tokenBufPtr, RDX=tokenCount, R8=outBufPtr
    call ai_require_model
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    test r8, r8
    jz  @@bad_args
    lea r9, gState
    mov QWORD PTR [r9+ST_AI_LAST_INPUT_PTR], rcx
    mov QWORD PTR [r9+ST_AI_LAST_OUTPUT_PTR], r8
    mov DWORD PTR [r9+ST_LAST_ERROR], 0
    mov eax, edx
    ret
@@bad_args:
    lea r9, gState
    mov DWORD PTR [r9+ST_LAST_ERROR], 22
    mov eax, IDE_FAIL
    ret
@@:
    ret
ai_detokenize ENDP

PUBLIC ai_embed
ai_embed PROC
    ; RCX=textPtr, RDX=len(0=auto)
    call ai_require_model
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    test rdx, rdx
    jnz @@have_len
    push rcx
    call IDE_strlen
    mov rdx, rax
    pop rcx
@@have_len:
    push rdx
    call IDE_fnv1a64
    pop rdx
    lea r8, gState
    mov QWORD PTR [r8+ST_AI_LAST_INPUT_PTR], rcx
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 22
    mov eax, IDE_FAIL
    ret
@@:
    ret
ai_embed ENDP

PUBLIC ai_generate
ai_generate PROC
    ; RCX=promptPtr, RDX=promptLen, R8=maxTokens
    call ai_query
    test eax, eax
    jnz @F
    lea r9, gState
    inc DWORD PTR [r9+ST_AI_GENERATION_COUNT]
    mov DWORD PTR [r9+ST_LAST_ERROR], 0
    mov eax, DWORD PTR [r9+ST_AI_GENERATION_COUNT]
    ret
@@:
    ret
ai_generate ENDP

PUBLIC ai_classify
ai_classify PROC
    ; RCX=inputPtr, RDX=inputLen
    call ai_query
    ret
ai_classify ENDP

PUBLIC ai_translate
ai_translate PROC
    ; RCX=inputPtr, RDX=inputLen
    call ai_query
    ret
ai_translate ENDP

PUBLIC ai_summarize
ai_summarize PROC
    ; RCX=inputPtr, RDX=inputLen
    call ai_query
    ret
ai_summarize ENDP

PUBLIC ai_ner
ai_ner PROC
    ; RCX=inputPtr, RDX=inputLen
    call ai_query
    ret
ai_ner ENDP

PUBLIC ai_sentiment
ai_sentiment PROC
    ; RCX=inputPtr, RDX=inputLen
    call ai_query
    ret
ai_sentiment ENDP

PUBLIC ai_qa
ai_qa PROC
    ; RCX=questionPtr, RDX=questionLen
    call ai_query
    ret
ai_qa ENDP

PUBLIC ai_chat
ai_chat PROC
    ; RCX=messagePtr, RDX=messageLen
    call ai_generate
    ret
ai_chat ENDP

PUBLIC ai_code_completion
ai_code_completion PROC
    ; RCX=prefixPtr, RDX=prefixLen
    call ai_generate
    ret
ai_code_completion ENDP

PUBLIC ai_image_generate
ai_image_generate PROC
    ; RCX=promptPtr, RDX=promptLen
    call ai_generate
    ret
ai_image_generate ENDP

PUBLIC ai_audio_transcribe
ai_audio_transcribe PROC
    ; RCX=audioBufPtr, RDX=audioSize
    call ai_query
    ret
ai_audio_transcribe ENDP

PUBLIC voice_init
voice_init PROC
    lea r8, gState
    mov DWORD PTR [r8+ST_VOICE_INIT], 1
    mov DWORD PTR [r8+ST_VOICE_ACTIVE], 0
    mov DWORD PTR [r8+ST_VOICE_VOLUME], 70
    mov DWORD PTR [r8+ST_VOICE_MUTED], 0
    mov DWORD PTR [r8+ST_VOICE_SPEED], 100
    mov DWORD PTR [r8+ST_VOICE_PITCH], 0
    mov DWORD PTR [r8+ST_VOICE_FX_FLAGS], 0
    mov DWORD PTR [r8+ST_VOICE_LISTEN_COUNT], 0
    mov DWORD PTR [r8+ST_VOICE_SPEAK_COUNT], 0
    mov QWORD PTR [r8+ST_VOICE_LAST_INPUT_PTR], 0
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
voice_init ENDP

voice_require_init PROC
    lea r8, gState
    cmp DWORD PTR [r8+ST_VOICE_INIT], 1
    je  @F
    mov DWORD PTR [r8+ST_LAST_ERROR], 30
    mov eax, IDE_FAIL
    ret
@@:
    mov DWORD PTR [r8+ST_LAST_ERROR], 0
    xor eax, eax
    ret
voice_require_init ENDP

PUBLIC voice_listen
voice_listen PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov DWORD PTR [r8+ST_VOICE_ACTIVE], 1
    inc DWORD PTR [r8+ST_VOICE_LISTEN_COUNT]
    mov eax, DWORD PTR [r8+ST_VOICE_LISTEN_COUNT]
    ret
@@:
    ret
voice_listen ENDP

PUBLIC voice_speak
voice_speak PROC
    ; RCX=textPtr
    call voice_require_init
    test eax, eax
    jnz @F
    test rcx, rcx
    jz  @@bad_args
    lea r8, gState
    mov QWORD PTR [r8+ST_VOICE_LAST_INPUT_PTR], rcx
    mov DWORD PTR [r8+ST_VOICE_ACTIVE], 1
    inc DWORD PTR [r8+ST_VOICE_SPEAK_COUNT]
    mov eax, DWORD PTR [r8+ST_VOICE_SPEAK_COUNT]
    ret
@@bad_args:
    lea r8, gState
    mov DWORD PTR [r8+ST_LAST_ERROR], 31
    mov eax, IDE_FAIL
    ret
@@:
    ret
voice_speak ENDP

PUBLIC voice_recognize
voice_recognize PROC
    call voice_listen
    ret
voice_recognize ENDP

PUBLIC voice_synthesize
voice_synthesize PROC
    ; RCX=textPtr
    call voice_speak
    ret
voice_synthesize ENDP

PUBLIC voice_pause
voice_pause PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov DWORD PTR [r8+ST_VOICE_ACTIVE], 0
    xor eax, eax
    ret
@@:
    ret
voice_pause ENDP

PUBLIC voice_resume
voice_resume PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov DWORD PTR [r8+ST_VOICE_ACTIVE], 1
    xor eax, eax
    ret
@@:
    ret
voice_resume ENDP

PUBLIC voice_stop
voice_stop PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov DWORD PTR [r8+ST_VOICE_ACTIVE], 0
    xor eax, eax
    ret
@@:
    ret
voice_stop ENDP

PUBLIC voice_volume_up
voice_volume_up PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_VOLUME]
    add eax, 10
    cmp eax, 100
    jbe @@set
    mov eax, 100
@@set:
    mov DWORD PTR [r8+ST_VOICE_VOLUME], eax
    ret
@@:
    ret
voice_volume_up ENDP

PUBLIC voice_volume_down
voice_volume_down PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_VOLUME]
    cmp eax, 10
    jb  @@zero
    sub eax, 10
    jmp @@set
@@zero:
    xor eax, eax
@@set:
    mov DWORD PTR [r8+ST_VOICE_VOLUME], eax
    ret
@@:
    ret
voice_volume_down ENDP

PUBLIC voice_mute
voice_mute PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov DWORD PTR [r8+ST_VOICE_MUTED], 1
    xor eax, eax
    ret
@@:
    ret
voice_mute ENDP

PUBLIC voice_unmute
voice_unmute PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov DWORD PTR [r8+ST_VOICE_MUTED], 0
    xor eax, eax
    ret
@@:
    ret
voice_unmute ENDP

PUBLIC voice_speed_up
voice_speed_up PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_SPEED]
    add eax, 10
    cmp eax, 200
    jbe @@set
    mov eax, 200
@@set:
    mov DWORD PTR [r8+ST_VOICE_SPEED], eax
    ret
@@:
    ret
voice_speed_up ENDP

PUBLIC voice_speed_down
voice_speed_down PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_SPEED]
    cmp eax, 60
    jb  @@min
    sub eax, 10
    jmp @@set
@@min:
    mov eax, 50
@@set:
    mov DWORD PTR [r8+ST_VOICE_SPEED], eax
    ret
@@:
    ret
voice_speed_down ENDP

PUBLIC voice_pitch_up
voice_pitch_up PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_PITCH]
    add eax, 1
    cmp eax, 12
    jle @@set
    mov eax, 12
@@set:
    mov DWORD PTR [r8+ST_VOICE_PITCH], eax
    ret
@@:
    ret
voice_pitch_up ENDP

PUBLIC voice_pitch_down
voice_pitch_down PROC
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_PITCH]
    sub eax, 1
    cmp eax, -12
    jge @@set
    mov eax, -12
@@set:
    mov DWORD PTR [r8+ST_VOICE_PITCH], eax
    ret
@@:
    ret
voice_pitch_down ENDP

PUBLIC voice_echo
voice_echo PROC
    ; RCX=enable(0/!=0)
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_FX_FLAGS]
    test rcx, rcx
    jz  @@disable
    or eax, 1
    jmp @@set
@@disable:
    and eax, 0FFFFFFFEh
@@set:
    mov DWORD PTR [r8+ST_VOICE_FX_FLAGS], eax
    xor eax, eax
    ret
@@:
    ret
voice_echo ENDP

PUBLIC voice_reverb
voice_reverb PROC
    ; RCX=enable(0/!=0)
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_FX_FLAGS]
    test rcx, rcx
    jz  @@disable
    or eax, 2
    jmp @@set
@@disable:
    and eax, 0FFFFFFFDh
@@set:
    mov DWORD PTR [r8+ST_VOICE_FX_FLAGS], eax
    xor eax, eax
    ret
@@:
    ret
voice_reverb ENDP

PUBLIC voice_filter
voice_filter PROC
    ; RCX=enable(0/!=0)
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_FX_FLAGS]
    test rcx, rcx
    jz  @@disable
    or eax, 4
    jmp @@set
@@disable:
    and eax, 0FFFFFFFBh
@@set:
    mov DWORD PTR [r8+ST_VOICE_FX_FLAGS], eax
    xor eax, eax
    ret
@@:
    ret
voice_filter ENDP

PUBLIC voice_equalizer
voice_equalizer PROC
    ; RCX=enable(0/!=0)
    call voice_require_init
    test eax, eax
    jnz @F
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_VOICE_FX_FLAGS]
    test rcx, rcx
    jz  @@disable
    or eax, 8
    jmp @@set
@@disable:
    and eax, 0FFFFFFF7h
@@set:
    mov DWORD PTR [r8+ST_VOICE_FX_FLAGS], eax
    xor eax, eax
    ret
@@:
    ret
voice_equalizer ENDP

;==============================================================================
; PE EMISSION
;==============================================================================

PUBLIC pe_emit
pe_emit PROC
    ; RCX=buffer, RDX=size -> EAX=ok
    push rbx
    mov rbx, rcx

    ; Write DOS header
    mov rcx, rbx
    lea rdx, DOS_HEADER
    mov r8, 64
    call IDE_memcpy

    ; Write DOS stub
    add rcx, 64
    lea rdx, DOS_STUB
    mov r8, 64
    call IDE_memcpy

    ; Write NT headers
    add rcx, 64
    lea rdx, NT_HEADERS
    mov r8, 248
    call IDE_memcpy

    ; Write section headers
    add rcx, 248
    lea rdx, SECTION_HEADERS
    mov r8, 80
    call IDE_memcpy

    ; Write .text section
    mov rcx, rbx
    add rcx, TEXT_OFFSET
    lea rdx, start_text
    mov r8d, TEXT_SIZE
    call IDE_memcpy

    ; Write .idata section
    mov rcx, rbx
    add rcx, IDATA_OFFSET
    lea rdx, start_idata
    mov r8d, IDATA_SIZE
    call IDE_memcpy

    ; Write .reloc section if relocs exist
    lea r8, gState
    mov eax, DWORD PTR [r8+ST_RELOC_USED]
    test eax, eax
    jz @@no_reloc
    mov rcx, rbx
    add rcx, RELOC_OFFSET
    ; Emit IMAGE_BASE_RELOCATION
    ; VirtualAddress = TEXT_RVA (for simplicity, all relocs in .text)
    mov DWORD PTR [rcx], TEXT_RVA
    add rcx, 4
    ; SizeOfBlock = 8 + (reloc_count * 2)
    mov r9d, eax
    shl r9d, 1
    add r9d, 8
    mov DWORD PTR [rcx], r9d
    add rcx, 4
    ; TypeOffset array
    mov r10, QWORD PTR [r8+ST_RELOC_BUF]
    mov r11d, 0
@@reloc_loop:
    cmp r11d, eax
    jae @@reloc_done
    mov r9d, DWORD PTR [r10+RELOC_REC_OFFSET]
    mov r12d, DWORD PTR [r10+RELOC_TYPE]
    shl r12d, 12
    or r9d, r12d
    mov WORD PTR [rcx], r9w
    add rcx, 2
    add r10, RELOC_BYTES
    inc r11d
    jmp @@reloc_loop
@@reloc_done:
@@no_reloc:
    pop rbx
    xor eax, eax
    ret
pe_emit ENDP

;==============================================================================
; IDE ENTRY
;==============================================================================

PUBLIC ide_init
ide_init PROC
    ; Initialize the IDE
    lea rcx, gState
    mov rdx, 100000h
    call IDE_arena_init
    xor eax, eax
    ret
ide_init ENDP

PUBLIC main
main PROC
    call ide_init
    xor ecx, ecx
    call QWORD PTR [gOS + OS_ALLOC] ; Assume ExitProcess is at OS_ALLOC for simplicity
    ret
main ENDP

end_text:
END
