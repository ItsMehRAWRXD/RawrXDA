; ============================================================================
; PE_Backend_Emitter.asm
; Monolithic PE32+ Backend — Generates Runnable x64 Executables
; Zero dependencies. Zero CRT. Bare-metal structural emission.
; ============================================================================

OPTION CASEMAP:NONE
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; ----------------------------------------------------------------------------
; EXPORTS — COMPLETE PUBLIC SYMBOL TABLE
; ----------------------------------------------------------------------------

; --- Monolithic Builders ---
PUBLIC Build_Full_PE64
PUBLIC Write_PE64_To_Disk
PUBLIC Build_And_Write_PE64

; --- Instruction Emitters ---
PUBLIC Emit_MOV_R64_IMM64
PUBLIC Emit_CALL_REL32
PUBLIC Emit_RET
PUBLIC Emit_FUNCTION_EPILOGUE
PUBLIC Align_Up

; --- Hash-Based API Resolution ---
PUBLIC Compute_CRC32
PUBLIC Resolve_Proc_Hash

; --- Data Tables ---
PUBLIC pe_sect_text
PUBLIC pe_sect_rdata
PUBLIC pe_dll_name
PUBLIC pe_fn_name
PUBLIC pe_default_out
PUBLIC pe_image_buf
PUBLIC crc32_table

; --- Exported PE Constants (linkable values) ---
PUBLIC const_PE_IMAGE_BASE
PUBLIC const_PE_SECTION_ALIGN
PUBLIC const_PE_FILE_ALIGN
PUBLIC const_PE_TEXT_RVA
PUBLIC const_PE_RDATA_RVA
PUBLIC const_PE_TOTAL_SIZE
PUBLIC const_IMAGE_SCN_CNT_CODE
PUBLIC const_IMAGE_SCN_MEM_EXECUTE
PUBLIC const_IMAGE_SCN_MEM_READ
PUBLIC const_IMAGE_SCN_MEM_WRITE
PUBLIC const_GENERIC_WRITE
PUBLIC const_CREATE_ALWAYS

; ----------------------------------------------------------------------------
; 1. PE STRUCTURES (Exact byte layouts — no packing pragmas needed)
; ----------------------------------------------------------------------------
IMAGE_DOS_HEADER STRUCT
    e_magic         WORD    ?
    e_cblp          WORD    ?
    e_cp            WORD    ?
    e_crlc          WORD    ?
    e_cparhdr       WORD    ?
    e_minalloc      WORD    ?
    e_maxalloc      WORD    ?
    e_ss            WORD    ?
    e_sp            WORD    ?
    e_csum          WORD    ?
    e_ip            WORD    ?
    e_cs            WORD    ?
    e_lfarlc        WORD    ?
    e_ovno          WORD    ?
    e_res           WORD    4 DUP(?)
    e_oemid         WORD    ?
    e_oeminfo       WORD    ?
    e_res2          WORD    10 DUP(?)
    e_lfanew        DWORD   ?
IMAGE_DOS_HEADER ENDS

IMAGE_FILE_HEADER STRUCT
    Machine                 WORD    ?
    NumberOfSections        WORD    ?
    TimeDateStamp           DWORD   ?
    PointerToSymbolTable    DWORD   ?
    NumberOfSymbols         DWORD   ?
    SizeOfOptionalHeader    WORD    ?
    Characteristics         WORD    ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  DWORD   ?
    Size1           DWORD   ?       ; "Size" is a MASM keyword
IMAGE_DATA_DIRECTORY ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT
    Magic                           WORD    ?
    MajorLinkerVersion              BYTE    ?
    MinorLinkerVersion              BYTE    ?
    SizeOfCode                      DWORD   ?
    SizeOfInitializedData           DWORD   ?
    SizeOfUninitializedData         DWORD   ?
    AddressOfEntryPoint             DWORD   ?
    BaseOfCode                      DWORD   ?
    ImageBase                       QWORD   ?
    SectionAlignment                DWORD   ?
    FileAlignment                   DWORD   ?
    MajorOperatingSystemVersion     WORD    ?
    MinorOperatingSystemVersion     WORD    ?
    MajorImageVersion               WORD    ?
    MinorImageVersion               WORD    ?
    MajorSubsystemVersion           WORD    ?
    MinorSubsystemVersion           WORD    ?
    Win32VersionValue               DWORD   ?
    SizeOfImage                     DWORD   ?
    SizeOfHeaders                   DWORD   ?
    CheckSum                        DWORD   ?
    Subsystem                       WORD    ?
    DllCharacteristics              WORD    ?
    SizeOfStackReserve              QWORD   ?
    SizeOfStackCommit               QWORD   ?
    SizeOfHeapReserve               QWORD   ?
    SizeOfHeapCommit                QWORD   ?
    LoaderFlags                     DWORD   ?
    NumberOfRvaAndSizes             DWORD   ?
    DataDirectory                   IMAGE_DATA_DIRECTORY 16 DUP(<>)
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_NT_HEADERS64 STRUCT
    Signature       DWORD   ?
    FileHeader      IMAGE_FILE_HEADER       <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_SECTION_HEADER STRUCT
    Name1                   BYTE    8 DUP(?)
    VirtualSize             DWORD   ?
    VirtualAddress          DWORD   ?
    SizeOfRawData           DWORD   ?
    PointerToRawData        DWORD   ?
    PointerToRelocations    DWORD   ?
    PointerToLinenumbers    DWORD   ?
    NumberOfRelocations     WORD    ?
    NumberOfLinenumbers     WORD    ?
    Characteristics         DWORD   ?
IMAGE_SECTION_HEADER ENDS

IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk      DWORD   ?
    TimeDateStamp           DWORD   ?
    ForwarderChain          DWORD   ?
    Name1                   DWORD   ?
    FirstThunk              DWORD   ?
IMAGE_IMPORT_DESCRIPTOR ENDS

; ----------------------------------------------------------------------------
; 2. CONSTANTS
; ----------------------------------------------------------------------------
PE_IMAGE_BASE           EQU 0000000140000000h
PE_SECTION_ALIGN        EQU 1000h
PE_FILE_ALIGN           EQU 200h

PE_TEXT_RVA             EQU 1000h
PE_RDATA_RVA            EQU 2000h

PE_HEADERS_SIZE         EQU 200h
PE_TEXT_FILE_OFF        EQU 200h
PE_RDATA_FILE_OFF       EQU 400h
PE_TOTAL_SIZE           EQU 600h    ; 0x200 hdrs + 0x200 text + 0x200 idata

RDATA_ILT_OFF           EQU 28h
RDATA_IAT_OFF           EQU 38h
RDATA_HINTNAME_OFF      EQU 48h
RDATA_DLLNAME_OFF       EQU 58h

; Section characteristics
IMAGE_SCN_CNT_CODE              EQU 000000020h
IMAGE_SCN_CNT_INITIALIZED_DATA  EQU 000000040h
IMAGE_SCN_MEM_EXECUTE           EQU 020000000h
IMAGE_SCN_MEM_READ              EQU 040000000h
IMAGE_SCN_MEM_WRITE             EQU 080000000h    ; REQUIRED for IAT patching

; Subsystems
IMAGE_SUBSYSTEM_WINDOWS_CUI     EQU 3

; Machine types
IMAGE_FILE_MACHINE_AMD64        EQU 8664h
IMAGE_NT_OPTIONAL_HDR64_MAGIC   EQU 20Bh

; DllCharacteristics
IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA    EQU 0020h
IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE       EQU 0040h
IMAGE_DLLCHARACTERISTICS_NX_COMPAT          EQU 0100h
IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE EQU 8000h

; File I/O constants
GENERIC_WRITE               EQU 40000000h
CREATE_ALWAYS               EQU 2
FILE_ATTRIBUTE_NORMAL       EQU 000000080h
INVALID_HANDLE_VALUE        EQU -1

; ----------------------------------------------------------------------------
; 3. DATA SECTION (Read-only data for the builder)
; ----------------------------------------------------------------------------
.data
ALIGN 16

pe_sect_text    BYTE    ".text",0,0,0
pe_sect_rdata   BYTE    ".rdata",0,0
pe_dll_name     BYTE    "kernel32.dll",0
pe_fn_name      BYTE    "ExitProcess",0
pe_default_out  BYTE    "output.exe",0

; 1.5 KB scratch buffer for the PE image (assembled in memory before write)
ALIGN 16
pe_image_buf    BYTE    PE_TOTAL_SIZE DUP(0)

; --- Exported Constant Values (linkable DWORDs/QWORDs) ---
ALIGN 8
const_PE_IMAGE_BASE             QWORD PE_IMAGE_BASE
const_PE_SECTION_ALIGN          DWORD PE_SECTION_ALIGN
const_PE_FILE_ALIGN             DWORD PE_FILE_ALIGN
const_PE_TEXT_RVA               DWORD PE_TEXT_RVA
const_PE_RDATA_RVA              DWORD PE_RDATA_RVA
const_PE_TOTAL_SIZE             DWORD PE_TOTAL_SIZE
const_IMAGE_SCN_CNT_CODE        DWORD IMAGE_SCN_CNT_CODE
const_IMAGE_SCN_MEM_EXECUTE     DWORD IMAGE_SCN_MEM_EXECUTE
const_IMAGE_SCN_MEM_READ        DWORD IMAGE_SCN_MEM_READ
const_IMAGE_SCN_MEM_WRITE       DWORD IMAGE_SCN_MEM_WRITE
const_GENERIC_WRITE             DWORD GENERIC_WRITE
const_CREATE_ALWAYS             DWORD CREATE_ALWAYS

; CRC32 lookup table (poly 0xEDB88320)
crc32_table   dd 0x00000000,0x09073096,0x120E612C,0x1B0951BA,0xFF6DC419,0xF66AF48F,0xED63A535,0xE46495A3
    dd 0xFEDB8832,0xF7DCB8A4,0xECD5E91E,0xE5D2D988,0x01B64C2B,0x08B17CBD,0x13B82D07,0x1ABF1D91
    dd 0xFDB71064,0xF4B020F2,0xEFB97148,0xE6BE41DE,0x02DAD47D,0x0BDDE4EB,0x10D4B551,0x19D385C7
    dd 0x036C9856,0x0A6BA8C0,0x1162F97A,0x1865C9EC,0xFC015C4F,0xF5066CD9,0xEE0F3D63,0xE7080DF5
    dd 0xFB6E20C8,0xF269105E,0xE96041E4,0xE0677172,0x0403E4D1,0x0D04D447,0x160D85FD,0x1F0AB56B
    dd 0x05B5A8FA,0x0CB2986C,0x17BBC9D6,0x1EBCF940,0xFAD86CE3,0xF3DF5C75,0xE8D60DCF,0xE1D13D59
    dd 0x06D930AC,0x0FDE003A,0x14D75180,0x1DD06116,0xF9B4F4B5,0xF0B3C423,0xEBBA9599,0xE2BDA50F
    dd 0xF802B89E,0xF1058808,0xEA0CD9B2,0xE30BE924,0x076F7C87,0x0E684C11,0x15611DAB,0x1C662D3D
    dd 0xF6DC4190,0xFFDB7106,0xE4D220BC,0xEDD5102A,0x09B18589,0x00B6B51F,0x1BBFE4A5,0x12B8D433
    dd 0x0807C9A2,0x0100F934,0x1A09A88E,0x130E9818,0xF76A0DBB,0xFE6D3D2D,0xE5646C97,0xEC635C01
    dd 0x0B6B51F4,0x026C6162,0x196530D8,0x1062004E,0xF40695ED,0xFD01A57B,0xE608F4C1,0xEF0FC457
    dd 0xF5B0D9C6,0xFCB7E950,0xE7BEB8EA,0xEEB9887C,0x0ADD1DDF,0x03DA2D49,0x18D37CF3,0x11D44C65
    dd 0x0DB26158,0x04B551CE,0x1FBC0074,0x16BB30E2,0xF2DFA541,0xFBD895D7,0xE0D1C46D,0xE9D6F4FB
    dd 0xF369E96A,0xFA6ED9FC,0xE1678846,0xE860B8D0,0x0C042D73,0x05031DE5,0x1E0A4C5F,0x170D7CC9
    dd 0xF005713C,0xF90241AA,0xE20B1010,0xEB0C2086,0x0F68B525,0x066F85B3,0x1D66D409,0x1461E49F
    dd 0x0EDEF90E,0x07D9C998,0x1CD09822,0x15D7A8B4,0xF1B33D17,0xF8B40D81,0xE3BD5C3B,0xEABA6CAD
    dd 0xEDB88320,0xE4BFB3B6,0xFFB6E20C,0xF6B1D29A,0x12D54739,0x1BD277AF,0x00DB2615,0x09DC1683
    dd 0x13630B12,0x1A643B84,0x016D6A3E,0x086A5AA8,0xEC0ECF0B,0xE509FF9D,0xFE00AE27,0xF7079EB1
    dd 0x100F9344,0x1908A3D2,0x0201F268,0x0B06C2FE,0xEF62575D,0xE66567CB,0xFD6C3671,0xF46B06E7
    dd 0xEED41B76,0xE7D32BE0,0xFCDA7A5A,0xF5DD4ACC,0x11B9DF6F,0x18BEEFF9,0x03B7BE43,0x0AB08ED5
    dd 0x16D6A3E8,0x1FD1937E,0x04D8C2C4,0x0DDFF252,0xE9BB67F1,0xE0BC5767,0xFBB506DD,0xF2B2364B
    dd 0xE80D2BDA,0xE10A1B4C,0xFA034AF6,0xF3047A60,0x1760EFC3,0x1E67DF55,0x056E8EEF,0x0C69BE79
    dd 0xEB61B38C,0xE266831A,0xF96FD2A0,0xF068E236,0x140C7795,0x1D0B4703,0x060216B9,0x0F05262F
    dd 0x15BA3BBE,0x1CBD0B28,0x07B45A92,0x0EB36A04,0xEAD7FFA7,0xE3D0CF31,0xF8D99E8B,0xF1DEAE1D
    dd 0x1B64C2B0,0x1263F226,0x096AA39C,0x006D930A,0xE40906A9,0xED0E363F,0xF6076785,0xFF005713
    dd 0xE5BF4A82,0xECB87A14,0xF7B12BAE,0xFEB61B38,0x1AD28E9B,0x13D5BE0D,0x08DCEFB7,0x01DBDF21
    dd 0xE6D3D2D4,0xEFD4E242,0xF4DDB3F8,0xFDDA836E,0x19BE16CD,0x10B9265B,0x0BB077E1,0x02B74777
    dd 0x18085AE6,0x110F6A70,0x0A063BCA,0x03010B5C,0xE7659EFF,0xEE62AE69,0xF56BFFD3,0xFC6CCF45
    dd 0xE00AE278,0xE90DD2EE,0xF2048354,0xFB03B3C2,0x1F672661,0x166016F7,0x0D69474D,0x046E77DB
    dd 0x1ED16A4A,0x17D65ADC,0x0CDF0B66,0x05D83BF0,0xE1BCAE53,0xE8BB9EC5,0xF3B2CF7F,0xFAB5FFE9
    dd 0x1DBDF21C,0x14BAC28A,0x0FB39330,0x06B4A3A6,0xE2D03605,0xEBD70693,0xF0DE5729,0xF9D967BF
    dd 0xE3667A2E,0xEA614AB8,0xF1681B02,0xF86F2B94,0x1C0BBE37,0x150C8EA1,0x0E05DF1B,0x0702EF8D

; ----------------------------------------------------------------------------
; 4. CODE SECTION (The Backend Engine)
; ----------------------------------------------------------------------------
.code

; ---------------------------------------------------------------------------
; Utility: Align value up to boundary
;   RCX = value, RDX = align (power of 2)
;   Returns RAX = aligned value
; ---------------------------------------------------------------------------
Align_Up PROC
    mov     rax, rcx
    dec     rdx
    add     rax, rdx
    not     rdx
    and     rax, rdx
    ret
Align_Up ENDP

; ---------------------------------------------------------------------------
; Instruction Emitter: MOV r64, imm64
;   RCX = reg (0-15), RDX = imm64, RDI = buffer ptr (updated)
; ---------------------------------------------------------------------------
Emit_MOV_R64_IMM64 PROC
    push    rax
    mov     al, 48h             ; REX.W
    cmp     cl, 7
    jle     @F
    or      al, 1               ; REX.B if reg > 7
@@:
    mov     [rdi], al
    inc     rdi
    mov     al, 0B8h            ; MOV r64, imm64 opcode base
    and     cl, 7
    add     al, cl              ; + rd
    mov     [rdi], al
    inc     rdi
    mov     [rdi], rdx          ; 8-byte immediate
    add     rdi, 8
    pop     rax
    ret
Emit_MOV_R64_IMM64 ENDP

; ---------------------------------------------------------------------------
; Instruction Emitter: CALL rel32 (RIP-relative indirect)
;   RCX = target RVA, RDX = current RVA (instruction start), RDI = buffer
; ---------------------------------------------------------------------------
Emit_CALL_REL32 PROC
    push    rax
    mov     byte ptr [rdi], 0FFh    ; ModRM group
    mov     byte ptr [rdi+1], 15h   ; /2 = CALL r/m64, with RIP-relative
    add     rdi, 2
    
    ; Displacement = Target - (Current + 6)   [6 = size of CALL insn]
    mov     rax, rcx
    sub     rax, rdx
    sub     rax, 6
    mov     dword ptr [rdi], eax
    add     rdi, 4
    pop     rax
    ret
Emit_CALL_REL32 ENDP

; ---------------------------------------------------------------------------
; Instruction Emitter: RET
;   RDI = buffer
; ---------------------------------------------------------------------------
Emit_RET PROC
    mov     byte ptr [rdi], 0C3h
    inc     rdi
    ret
Emit_RET ENDP

; ---------------------------------------------------------------------------
; Emit_FUNCTION_EPILOGUE — mov rsp, rbp; pop rbp; ret (5 bytes)
;   RDI = buffer
; ---------------------------------------------------------------------------
Emit_FUNCTION_EPILOGUE PROC
    mov     byte ptr [rdi], 48h
    mov     byte ptr [rdi+1], 89h
    mov     byte ptr [rdi+2], 0ECh
    mov     byte ptr [rdi+3], 5Dh
    mov     byte ptr [rdi+4], 0C3h
    add     rdi, 5
    ret
Emit_FUNCTION_EPILOGUE ENDP

; ============================================================================
; CORE BUILDER: Construct a complete, runnable PE32+ image in pe_image_buf
;   Returns: RAX = PE_TOTAL_SIZE (1536 bytes)
;   Image layout:
;       0x000-0x1FF   Headers (DOS + NT + Section Table)
;       0x200-0x3FF   .text (entry point: xor rcx,rcx; call ExitProcess)
;       0x400-0x5FF   .rdata (Import Directory, ILT, IAT, Names)
; ============================================================================
Build_Full_PE64 PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    push    rdi
    .pushreg rdi
    push    rsi
    .pushreg rsi
    sub     rsp, 40
    .allocstack 40
    .endprolog

    lea     rbx, [pe_image_buf]

    ; --- Zero entire buffer ---
    mov     rdi, rbx
    mov     ecx, PE_TOTAL_SIZE / 8
    xor     eax, eax
    rep     stosq

    ; =========================================================
    ; (A) DOS HEADER (64 bytes)
    ; =========================================================
    mov     word ptr  [rbx+0],  5A4Dh       ; 'MZ'
    mov     word ptr  [rbx+8],  4           ; e_cparhdr (legacy)
    mov     word ptr  [rbx+10], 0FFFFh      ; e_maxalloc
    mov     word ptr  [rbx+16], 0B8h        ; e_sp
    mov     word ptr  [rbx+24], 40h         ; e_lfarlc
    mov     dword ptr [rbx+60], 80h         ; e_lfanew → NT @ 0x80

    ; --- DOS STUB (minimal 16-bit exit) ---
    ; mov ax, 4C01h ; int 21h (exit with code 1)
    mov     byte ptr [rbx+40h], 0B8h        ; mov ax, ...
    mov     byte ptr [rbx+41h], 01h
    mov     byte ptr [rbx+42h], 00h
    mov     byte ptr [rbx+43h], 0B4h        ; mov ah, 4Ch
    mov     byte ptr [rbx+44h], 4Ch
    mov     byte ptr [rbx+45h], 0CDh        ; int 21h
    mov     byte ptr [rbx+46h], 21h
    ; Pad rest with NOPs until 0x80
    mov     rdi, rbx
    add     rdi, 47h
    mov     ecx, (80h - 47h)
    mov     al, 90h
    rep     stosb

    ; =========================================================
    ; (B) NT HEADERS (start at 0x80)
    ; =========================================================
    lea     r12, [rbx+80h]
    mov     dword ptr [r12+0], 00004550h    ; 'PE\0\0'

    ; --- COFF File Header ---
    mov     word ptr  [r12+4],  IMAGE_FILE_MACHINE_AMD64
    mov     word ptr  [r12+6],  2           ; 2 sections
    mov     dword ptr [r12+8],  0           ; Timestamp
    mov     dword ptr [r12+12], 0           ; SymbolTable
    mov     dword ptr [r12+16], 0           ; NumSymbols
    mov     word ptr  [r12+20], 240         ; SizeOfOptionalHeader
    mov     word ptr  [r12+22], 0022h       ; EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE

    ; --- Optional Header PE32+ ---
    lea     r13, [r12+24]
    mov     word ptr  [r13+0],  IMAGE_NT_OPTIONAL_HDR64_MAGIC
    mov     byte ptr  [r13+2],  14          ; MajorLinker
    mov     byte ptr  [r13+3],  0           ; MinorLinker
    mov     dword ptr [r13+4],  200h        ; SizeOfCode
    mov     dword ptr [r13+8],  200h        ; SizeOfInitializedData
    mov     dword ptr [r13+12], 0           ; SizeOfUninitializedData
    mov     dword ptr [r13+16], PE_TEXT_RVA ; EntryPoint
    mov     dword ptr [r13+20], PE_TEXT_RVA ; BaseOfCode
    
    mov     rax, PE_IMAGE_BASE
    mov     qword ptr [r13+24], rax         ; ImageBase
    
    mov     dword ptr [r13+32], PE_SECTION_ALIGN
    mov     dword ptr [r13+36], PE_FILE_ALIGN
    mov     word ptr  [r13+40], 6           ; MajorOS
    mov     word ptr  [r13+42], 0           ; MinorOS
    mov     word ptr  [r13+48], 6           ; MajorSubsystem
    mov     word ptr  [r13+50], 0           ; MinorSubsystem
    mov     dword ptr [r13+56], 3000h       ; SizeOfImage (3 pages)
    mov     dword ptr [r13+60], PE_HEADERS_SIZE
    mov     dword ptr [r13+64], 0           ; Checksum
    mov     word ptr  [r13+68], IMAGE_SUBSYSTEM_WINDOWS_CUI
    
    mov     ax, IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA OR \
                IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE OR \
                IMAGE_DLLCHARACTERISTICS_NX_COMPAT OR \
                IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE
    mov     word ptr [r13+70], ax
    
    mov     rax, 100000h
    mov     qword ptr [r13+72], rax         ; StackReserve
    mov     rax, 1000h
    mov     qword ptr [r13+80], rax         ; StackCommit
    mov     qword ptr [r13+88], rax         ; HeapReserve
    mov     qword ptr [r13+96], rax         ; HeapCommit
    
    mov     dword ptr [r13+104], 0          ; LoaderFlags
    mov     dword ptr [r13+108], 16         ; NumberOfRvaAndSizes

    ; Data Directory[1] = Import Table
    mov     dword ptr [r13+120], PE_RDATA_RVA
    mov     dword ptr [r13+124], 28h        ; 2 descriptors
    
    ; Data Directory[12] = IAT
    mov     dword ptr [r13+208], PE_RDATA_RVA + RDATA_IAT_OFF
    mov     dword ptr [r13+212], 10h        ; 2 qwords

    ; =========================================================
    ; (C) SECTION HEADERS (start at 0x80 + 4 + 20 + 240 = 0x188)
    ; =========================================================
    
    ; --- .text ---
    lea     rdi, [rbx+188h]
    lea     rsi, [pe_sect_text]
    movsq                                   ; Copy name (8 bytes)
    mov     dword ptr [rdi+0],  200h        ; VirtualSize
    mov     dword ptr [rdi+4],  PE_TEXT_RVA
    mov     dword ptr [rdi+8],  200h        ; SizeOfRawData
    mov     dword ptr [rdi+12], PE_TEXT_FILE_OFF
    xor     eax, eax
    mov     dword ptr [rdi+16], eax         ; Relocations
    mov     dword ptr [rdi+20], eax         ; Linenumbers
    mov     dword ptr [rdi+24], eax         ; NumRelocs/NumLines
    mov     eax, IMAGE_SCN_CNT_CODE OR IMAGE_SCN_MEM_EXECUTE OR IMAGE_SCN_MEM_READ
    mov     dword ptr [rdi+28], eax

    ; --- .rdata --- (movsq already advanced rdi by 8, so +32 not +40)
    add     rdi, 32
    lea     rsi, [pe_sect_rdata]
    movsq
    mov     dword ptr [rdi+0],  200h        ; VirtualSize
    mov     dword ptr [rdi+4],  PE_RDATA_RVA
    mov     dword ptr [rdi+8],  200h        ; SizeOfRawData
    mov     dword ptr [rdi+12], PE_RDATA_FILE_OFF
    xor     eax, eax
    mov     dword ptr [rdi+16], eax
    mov     dword ptr [rdi+20], eax
    mov     dword ptr [rdi+24], eax
    ; CRITICAL: Include WRITE permission for IAT patching
    mov     eax, IMAGE_SCN_CNT_INITIALIZED_DATA OR IMAGE_SCN_MEM_READ OR IMAGE_SCN_MEM_WRITE
    mov     dword ptr [rdi+28], eax

    ; =========================================================
    ; (D) .TEXT SECTION (File offset 0x200)
    ;     Entry Point: sub rsp,28h; xor rcx,rcx; call [rip+disp]
    ; =========================================================
    lea     rdi, [rbx + PE_TEXT_FILE_OFF]
    
    ; sub rsp, 28h (48 83 EC 28)
    mov     byte ptr [rdi+0], 48h
    mov     byte ptr [rdi+1], 83h
    mov     byte ptr [rdi+2], 0ECh
    mov     byte ptr [rdi+3], 28h
    
    ; xor rcx, rcx (48 31 C9)
    mov     byte ptr [rdi+4], 48h
    mov     byte ptr [rdi+5], 31h
    mov     byte ptr [rdi+6], 0C9h
    
    ; call [rip+disp32] (FF 15 ...)
    mov     byte ptr [rdi+7], 0FFh
    mov     byte ptr [rdi+8], 15h
    
    ; Calculate displacement:
    ; RIP after = ImageBase + TEXT_RVA + 13 = 0x14000100D
    ; Target    = ImageBase + RDATA_RVA + IAT_OFF = 0x140002038
    ; Disp = 0x2038 - 0x100D = 0x102B
    mov     eax, (PE_RDATA_RVA + RDATA_IAT_OFF) - (PE_TEXT_RVA + 13)
    mov     dword ptr [rdi+9], eax
    
    ; Padding with INT3 (0xCC) for alignment/debugging
    mov     rdi, rbx
    add     rdi, PE_TEXT_FILE_OFF + 13
    mov     ecx, 200h - 13
    mov     al, 0CCh
    rep     stosb

    ; =========================================================
    ; (E) .RDATA SECTION (File offset 0x400)
    ; =========================================================
    lea     rdi, [rbx + PE_RDATA_FILE_OFF]

    ; Import Descriptor #0 (kernel32)
    mov     eax, PE_RDATA_RVA + RDATA_ILT_OFF
    mov     dword ptr [rdi+0],  eax         ; OriginalFirstThunk → ILT
    mov     dword ptr [rdi+4],  0           ; TimeDateStamp
    mov     dword ptr [rdi+8],  0           ; ForwarderChain
    mov     eax, PE_RDATA_RVA + RDATA_DLLNAME_OFF
    mov     dword ptr [rdi+12], eax         ; Name RVA
    mov     eax, PE_RDATA_RVA + RDATA_IAT_OFF
    mov     dword ptr [rdi+16], eax         ; FirstThunk → IAT
    
    ; Null terminator descriptor (already zero from memset)
    
    ; ILT[0] = RVA of Hint/Name
    mov     rax, PE_RDATA_RVA + RDATA_HINTNAME_OFF
    mov     qword ptr [rdi + RDATA_ILT_OFF], rax
    
    ; IAT[0] = same (loader will overwrite with actual address)
    mov     qword ptr [rdi + RDATA_IAT_OFF], rax
    
    ; Hint/Name: Hint=0, Name="ExitProcess"
    mov     word ptr [rdi + RDATA_HINTNAME_OFF], 0
    lea     rsi, [pe_fn_name]
    lea     r8,  [rdi + RDATA_HINTNAME_OFF + 2]
@@copy_fn:
    lodsb
    mov     [r8], al
    inc     r8
    test    al, al
    jnz     @@copy_fn
    
    ; DLL Name: "kernel32.dll"
    lea     rsi, [pe_dll_name]
    lea     r8,  [rdi + RDATA_DLLNAME_OFF]
@@copy_dll:
    lodsb
    mov     [r8], al
    inc     r8
    test    al, al
    jnz     @@copy_dll
    
    ; Zero-fill remainder of section
    mov     rdi, rbx
    add     rdi, PE_RDATA_FILE_OFF + 100h   ; End of .rdata area
    mov     ecx, 100h / 8                   ; Fill remaining 0x100 bytes
    xor     eax, eax
    rep     stosq

    ; Return total size
    mov     eax, PE_TOTAL_SIZE

    add     rsp, 40
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
Build_Full_PE64 ENDP

; ============================================================================
; DISK I/O (Win32 API — only used by the host compiler, not the generated PE)
; ============================================================================
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC

Write_PE64_To_Disk PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    push    rbx
    .pushreg rbx
    sub     rsp, 80
    .allocstack 80
    .endprolog

    ; RCX = filename (or NULL for default)
    test    rcx, rcx
    jnz     @@has_name
    lea     rcx, [pe_default_out]
@@has_name:
    mov     rsi, rcx                    ; Save filename

    ; CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, NORMAL, NULL)
    mov     rdx, GENERIC_WRITE
    xor     r8d, r8d                    ; Share mode 0
    xor     r9d, r9d                    ; Security NULL
    mov     dword ptr [rsp+32], CREATE_ALWAYS
    mov     dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov     qword ptr [rsp+48], 0       ; Template NULL
    call    CreateFileA
    
    cmp     rax, INVALID_HANDLE_VALUE
    je      @@fail
    mov     rbx, rax                    ; hFile

    ; WriteFile(hFile, buffer, size, &written, NULL)
    mov     rcx, rbx
    lea     rdx, [pe_image_buf]
    mov     r8d, PE_TOTAL_SIZE
    lea     r9, [rsp+64]                ; &bytesWritten (stack scratch)
    mov     qword ptr [rsp+32], 0       ; Overlapped NULL
    call    WriteFile
    
    test    eax, eax
    jz      @@close_fail

    ; CloseHandle(hFile)
    mov     rcx, rbx
    call    CloseHandle
    
    mov     eax, 1                      ; Success
    jmp     @@done

@@close_fail:
    mov     rcx, rbx
    call    CloseHandle
@@fail:
    xor     eax, eax                    ; Failure
@@done:
    add     rsp, 80
    pop     rbx
    pop     rbp
    ret
Write_PE64_To_Disk ENDP

; ---------------------------------------------------------------------------
; Convenience wrapper: Build then Write
; ---------------------------------------------------------------------------
Build_And_Write_PE64 PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 32
    .allocstack 32
    .endprolog

    ; Build image to internal buffer
    call    Build_Full_PE64
    
    ; Write to disk (filename in RCX)
    call    Write_PE64_To_Disk

    add     rsp, 32
    pop     rbp
    ret
Build_And_Write_PE64 ENDP

; ---------------------------------------------------------------------------
; Compute_CRC32 — calculate CRC32 of ASCIIZ string at RDI
;   RDI = pointer
;   Returns: EAX = CRC32
; ---------------------------------------------------------------------------
Compute_CRC32 PROC
    xor     eax, eax            ; crc = 0xFFFFFFFF
    not     eax
    mov     rsi, rdi
@@crc_loop:
    lodsb
    test    al, al
    jz      @@crc_done
    movzx   ecx, al
    xor     eax, ecx
    and     eax, 0FFh
    mov     edx, crc32_table
    mov     edx, [rdx + rax*4]
    shr     eax, 8
    xor     eax, edx
    jmp     @@crc_loop
@@crc_done:
    not     eax
    ret
Compute_CRC32 ENDP

; ---------------------------------------------------------------------------
; Resolve_Proc_Hash
;   RDX = module base, ECX = CRC32 hash
;   Returns: RAX = function VA or 0
; ---------------------------------------------------------------------------
Resolve_Proc_Hash PROC
    push    rbx
    push    rsi
    push    rdi

    mov     rbx, rdx
    mov     eax, [rbx + 3Ch]
    add     rbx, rax
    mov     edi, [rbx + 0x88]
    test    edi, edi
    jz      .not_found
    add     rbx, rdi
    mov     esi, [rbx + 24]
    mov     rdi, [rbx + 32]
    add     rdi, rdx
    mov     r8, [rbx + 36]
    add     r8, rdx
    mov     r9, [rbx + 28]
    add     r9, rdx

@@search_loop:
    test    esi, esi
    jz      @@not_found
    mov     eax, [rdi]
    add     rax, rdx
    push    rdx
    mov     rdx, rax
    call    Compute_CRC32
    pop     rdx
    cmp     eax, ecx
    jne     @@next_name
    movzx   eax, word ptr [r8]
    mov     rax, [r9 + rax*4]
    add     rax, rdx
    jmp     @@done
@@next_name:
    add     rdi, 4
    add     r8, 2
    dec     esi
    jmp     @@search_loop
@@not_found:
    xor     rax, rax
@@done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Resolve_Proc_Hash ENDP

END
