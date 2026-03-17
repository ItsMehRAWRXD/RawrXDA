; ================================================================================
; RawrXD PE Generator & Encoder - Pure MASM x64 Production Impleme
; ================================================================================
; Generates valid Windows PE32+ executables from scratch with polymorphic encodi
; Zero dependencies, zero imports, pure assembly impleme
; ================================================================================

OPTION CASEMAP:NONE
OPTION DOTNAME

; External Windows API refere
EXTERNDEF __imp_VirtualAlloc:QWORD
EXTERNDEF __imp_VirtualFree:QWORD
EXTERNDEF __imp_CreateFileA:QWORD
EXTERNDEF __imp_WriteFile:QWORD
EXTERNDEF __imp_CloseHandle:QWORD
PE_MAGIC                EQU     0x5A4D          ; "MZ"
PE_SIGNATURE            EQU     0x00004550      ; "PE\0\0"
MACHINE_AMD64           EQU     0x8664
OPTIONAL_HDR64_MAGIC    EQU     0x20B
IMAGE_SUBSYSTEM_WINDOWS_CUI EQU 3
IMAGE_SUBSYSTEM_WINDOWS_GUI EQU 2
IMAGE_FILE_EXECUTABLE_IMAGE EQU 0x0002
IMAGE_FILE_LARGE_ADDRESS_AWARE EQU 0x0020
IMAGE_DLLCHARACTERISTICS_NX_COMPAT EQU 0x0100
IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE EQU 0x0040

; Directory e
IMAGE_DIRECTORY_ENTRY_EXPORT        EQU 0
IMAGE_DIRECTORY_ENTRY_IMPORT        EQU 1
IMAGE_DIRECTORY_ENTRY_RESOURCE      EQU 2
IMAGE_DIRECTORY_ENTRY_EXCEPTION     EQU 3
IMAGE_DIRECTORY_ENTRY_SECURITY      EQU 4
IMAGE_DIRECTORY_ENTRY_BASERELOC     EQU 5
IMAGE_DIRECTORY_ENTRY_DEBUG         EQU 6
IMAGE_DIRECTORY_ENTRY_ARCHITECTURE  EQU 7
IMAGE_DIRECTORY_ENTRY_GLOBALPTR     EQU 8
IMAGE_DIRECTORY_ENTRY_TLS           EQU 9
IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG   EQU 10
IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT  EQU 11
IMAGE_DIRECTORY_ENTRY_IAT           EQU 12
IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT  EQU 13
IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR EQU 14

; Section characteristics
IMAGE_SCN_CNT_CODE          EQU 0x00000020
IMAGE_SCN_CNT_INITIALIZED_DATA EQU 0x00000040
IMAGE_SCN_CNT_UNINITIALIZED_DATA EQU 0x00000080
IMAGE_SCN_MEM_EXECUTE       EQU 0x20000000
IMAGE_SCN_MEM_READ          EQU 0x40000000
IMAGE_SCN_MEM_WRITE         EQU 0x80000000
IMAGE_SCN_ALIGN_16BYTES     EQU 0x00500000

; ================================================================================
; ENCODER CONSTANTS
; ================================================================================
ENCODER_XOR_KEY         EQU     0xDEADBEEFCAFEBABEh
ENCODER_ROL_BITS        EQU     7
ENCODER_ROR_BITS        EQU     9
ENCODER_ADD_DELTA       EQU     0x1337C0DEh
MAX_SECTIONS            EQU     16
MAX_IMPORTS             EQU     256
MAX_EXPORTS             EQU     64

; ================================================================================
; DATA STRUCTURES
; ================================================================================

; PE Generator Co
PEGENCTX STRUCT
    ; Output buffer
    pBuffer             QWORD   ?
    BufferSize          QWORD   ?
    CurrentOffset       QWORD   ?
    
    ; PE Headers
    pDosHeader          QWORD   ?
    pNtHeaders          QWORD   ?
    pFileHeader         QWORD   ?
    pOptionalHeader     QWORD   ?
    pDataDirectory      QWORD   ?
    pSectionHeaders     QWORD   ?
    NumSections         DWORD   ?
    
    ; Section tracki
    SectionVirtualAddr  QWORD   ?
    SectionRawAddr      QWORD   ?
    
    ; Encoding state
    EncoderKey          QWORD   ?
    EncoderMethod       DWORD   ?
    IsEncoded           BYTE    ?
    
    ; Import/Export tracki
    pImportTable        QWORD   ?
    NumImports          DWORD   ?
    pExportTable        QWORD   ?
    NumExports          DWORD   ?
    
    ; Entry poi
    EntryPointRVA       DWORD   ?
    EntryPointRaw       QWORD   ?
PEGENCTX ENDS

; Import descriptor template
IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk  DWORD   ?
    TimeDateStamp       DWORD   ?
    ForwarderChain      DWORD   ?
    NameRVA             DWORD   ?
    FirstThunk          DWORD   ?
IMPORT_DESCRIPTOR ENDS

; Section template
SECTION_TEMPLATE STRUCT
    Name                BYTE    8 DUP(?)
    VirtualSize         DWORD   ?
    VirtualAddress      DWORD   ?
    SizeOfRawData       DWORD   ?
    PointerToRawData    DWORD   ?
    PointerToRelocations DWORD  ?
    PointerToLinenumbers DWORD  ?
    NumberOfRelocations WORD    ?
    NumberOfLinenumbers WORD    ?
    Characteristics     DWORD   ?
SECTION_TEMPLATE ENDS

; ================================================================================
; EXTERNAL FUNCTIONS (Windows API via manual imports)
; ================================================================================

EXTERNDEF __imp_VirtualAlloc:QWORD
EXTERNDEF __imp_VirtualFree:QWORD
EXTERNDEF __imp_GetProcessHeap:QWORD
EXTERNDEF __imp_HeapAlloc:QWORD
EXTERNDEF __imp_HeapFree:QWORD
EXTERNDEF __imp_CreateFileA:QWORD
EXTERNDEF __imp_WriteFile:QWORD
EXTERNDEF __imp_CloseHandle:QWORD
EXTERNDEF __imp_GetLastError:QWORD
EXTERNDEF __imp_ExitProcess:QWORD

; ================================================================================
; MACROS
; ================================================================================

; Align value to bou
ALIGN_VALUE MACRO val, align_val
    (((val) + ((align_val) - 1)) AND NOT ((align_val) - 1))
ENDM

; Calculate RVA from raw offset
CALC_RVA MACRO ctx, raw_offset
    (((raw_offset) - (ctx).pDosHeader) + (ctx).SectionVirtualAddr)
ENDM

; ================================================================================
; CODE SECTION - Executable Code
; ================================================================================

.code

ALIGN 16

; ================================================================================
; PE GENERATOR CORE FUNCTIONS
; ================================================================================

; --------------------------------------------------------------------------------
; PeGen_Initialize - Initialize PE generator co
; RCX = pContext (PEGENCTX pointer)
; RDX = InitialBufferSize
; Returns EAX = 1 (success) or 0 (failure)
PeGen_Initialize PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .PUSHREG r12
    .PUSHREG r13
    .PUSHREG r14
    .PUSHREG r15
    .ENDPROLOG
    
    mov     r12, rcx
    mov     r13, rdx
    
    ; Zero context structure
    mov     rdi, r12
    mov     rcx, SIZEOF PEGENCTX
    xor     eax, eax
    rep     stosb
    
    ; Allocate buffer with RWX permissions for encodi
    mov     rcx, 0NULL (let system choose)
    mov     rdx, r13
    mov     r8, 0x3000nType = MEM_COMMIT | MEM_RESERVE
    mov     r9, 0x40
    
    call    qword ptr [__imp_VirtualAlloc]
    
    test    rax, rax
    jz      @@init_failed
    
    mov     (PEGENCTX PTR [r12]).pBuffer, rax
    mov     (PEGENCTX PTR [r12]).BufferSize, r13
    mov     (PEGENCTX PTR [r12]).CurrentOffset, rax
    mov     (PEGENCTX PTR [r12]).pDosHeader, rax
    rdtsc
    shl     rax, 32
    or      rax, rdx
    xor     rax, ENCODER_XOR_KEY
    mov     (PEGENCTX PTR [r12]).EncoderKey, rax
    
    mov     eax, 1
    jmp     @@init_do
    
@@init_failed:
    xor     eax, eax
    
@@init_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_Initialize ENDP

; --------------------------------------------------------------------------------
; PeGen_CreateDosHeader - Create MZ header
; RCX = pCo
; Returns EAX = 1 (success)
; --------------------------------------------------------------------------------
PeGen_CreateDosHeader PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .ENDPROLOG
    
    mov     rbx, rcx
    mov     rdi, (PEGENCTX PTR [rbx]).pDosHeader "MZ"
    mov     word ptr [rdi], PE_MAGIC)
    mov     word ptr [rdi+2], 0x0090n last page
    mov     word ptr [rdi+4], 0x0003n file
    mov     word ptr [rdi+6], 0x0000
    mov     word ptr [rdi+8], 0x0004n paragraphs
    mov     word ptr [rdi+10], 0x0000nimum extra paragraphs
    mov     word ptr [rdi+12], 0xFFFF
    mov     word ptr [rdi+14], 0x0000nitial SS value
    mov     word ptr [rdi+16], 0x00B8nitial SP value
    mov     word ptr [rdi+18], 0x0000
    mov     word ptr [rdi+20], 0x0000nitial IP value
    mov     word ptr [rdi+22], 0x0000nitial CS value
    mov     word ptr [rdi+24], 0x0040n table
    mov     word ptr [rdi+26], 0x0000
    
    ; Reserved words
    xor     eax, eax
    mov     [rdi+28], eax
    mov     [rdi+32], eax
    mov     [rdi+36], eax
    mov     [rdi+40], eax
    mov     [rdi+44], eax
    mov     [rdi+46], eax
    mov     dword ptr [rdi+60], 0x40
    
    ; Simple DOS stub
    mov     byte ptr [rdi+64], 0xEB
    mov     byte ptr [rdi+65], 0x02
    mov     byte ptr [rdi+66], 0x90
    mov     byte ptr [rdi+67], 0x90
    mov     rax, (PEGENCTX PTR [rbx]).pDosHeader
    add     rax, 64 + 14
    mov     (PEGENCTX PTR [rbx]).CurrentOffset, rax
    
    mov     eax, 1
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_CreateDosHeader ENDP

; --------------------------------------------------------------------------------
; PeGen_CreateNtHeaders - Create PE32+ headers
; RCX = pCo
; RDX = ImageBase
; R8  = Subsystem (2=GUI, 3=Console)ns EAX = 1 (success)
; --------------------------------------------------------------------------------
PeGen_CreateNtHeaders PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .PUSHREG r12
    .PUSHREG r13
    .PUSHREG r14
    .ENDPROLOG
    
    mov     rbx, rcx
    mov     r12, rdx
    mov     r13d, r8d
    
    ; Align to 8 bytes
    mov     rax, (PEGENCTX PTR [rbx]).CurrentOffset
    add     rax, 7
    and     rax, NOT 7
    mov     (PEGENCTX PTR [rbx]).CurrentOffset, rax
    mov     rdi, rax
    mov     dword ptr [rdi], PE_SIGNATURE
    add     rdi, 4
    mov     (PEGENCTX PTR [rbx]).pFileHeader, rdi
    
    mov     word ptr [rdi], MACHINE_AMD64
    mov     word ptr [rdi+2], 0NumberOfSections (will be updated)
    mov     dword ptr [rdi+4], 0
    mov     dword ptr [rdi+8], 0nterToSymbolTable
    mov     dword ptr [rdi+12], 0NumberOfSymbols
    mov     word ptr [rdi+16], 0nalHeader (will be set)
    mov     word ptr [rdi+18], IMAGE_FILE_EXECUTABLE_IMAGE OR IMAGE_FILE_LARGE_ADDRESS_AWARE
    
    add     rdi, 20
    
    ; Optional Header (PE32+)
    mov     (PEGENCTX PTR [rbx]).pOptionalHeader, rdi
    mov     (PEGENCTX PTR [rbx]).pNtHeaders, (PEGENCTX PTR [rbx]).pFileHeader
    
    mov     word ptr [rdi], OPTIONAL_HDR64_MAGIC
    mov     byte ptr [rdi+2], 0nkerVersion
    mov     byte ptr [rdi+3], 0norLinkerVersion
    mov     dword ptr [rdi+4], 0
    mov     dword ptr [rdi+8], 0nitializedData
    mov     dword ptr [rdi+12], 0ninitializedData
    mov     dword ptr [rdi+16], 0ntryPoint (will be set)
    mov     dword ptr [rdi+20], 0x1000
    mov     qword ptr [rdi+24], r12
    mov     dword ptr [rdi+32], 0x1000nAlignme
    mov     dword ptr [rdi+36], 0x200nme
    mov     word ptr [rdi+40], 6ngSystemVersion
    mov     word ptr [rdi+42], 0norOperatingSystemVersion
    mov     word ptr [rdi+44], 0n
    mov     word ptr [rdi+46], 0norImageVersion
    mov     word ptr [rdi+48], 6n
    mov     word ptr [rdi+50], 0norSubsystemVersion
    mov     dword ptr [rdi+52], 0n32VersionValue
    mov     dword ptr [rdi+56], 0x10000
    mov     dword ptr [rdi+60], 0x400
    mov     dword ptr [rdi+64], 0
    mov     word ptr [rdi+68], r13w
    mov     word ptr [rdi+70], IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE OR IMAGE_DLLCHARACTERISTICS_NX_COMPAT
    mov     qword ptr [rdi+72], 0x100000
    mov     qword ptr [rdi+80], 0x10000
    mov     qword ptr [rdi+88], 0x100000
    mov     qword ptr [rdi+96], 0x10000
    mov     dword ptr [rdi+104], 0
    mov     dword ptr [rdi+108], 16NumberOfRvaAndSizes
    
    ; Data Directory
    add     rdi, 112
    mov     (PEGENCTX PTR [rbx]).pDataDirectory, rdi
    mov     rcx, 16 * 8ntries * 8 bytes each
    xor     eax, eax
    rep     stosb
    
    ; Update sizes
    mov     rax, (PEGENCTX PTR [rbx]).pOptionalHeader
    mov     rdx, rdi
    sub     rdx, rax
    mov     word ptr (PEGENCTX PTR [rbx]).pFileHeader[16], dxnt offset
    mov     (PEGENCTX PTR [rbx]).CurrentOffset, rdin headers start here (aligned to 8)
    add     rdi, 7
    and     rdi, NOT 7
    mov     (PEGENCTX PTR [rbx]).pSectionHeaders, rdi
    
    mov     eax, 1
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_CreateNtHeaders ENDP

; --------------------------------------------------------------------------------
; PeGen_AddSection - Add a section to the PE
; RDX = pName (8-byte name)
; R8  = VirtualSize
; R9  = RawSize
; [RSP+0x28] = Characteristics
; [RSP+0x30] = pData (optional, can be NULL)
; Returns EAX = 1 (success) or 0 (failure)
PeGen_AddSection PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .PUSHREG r12
    .PUSHREG r13
    .PUSHREG r14
    .PUSHREG r15
    .ENDPROLOG
    
    mov     rbx, rcx
    mov     r12, rdxName
    mov     r13d, r8d
    mov     r14d, r9d
    mov     r15d, [rsp+0x58]
    mov     rsi, [rsp+0x60]
    
    ; Check section limit
    mov     eax, (PEGENCTX PTR [rbx]).NumSectio
    cmp     eax, MAX_SECTIONS
    jae     @@add_failed
    
    ; Calculate section header location
    mov     rdi, (PEGENCTX PTR [rbx]).pSectionHeaders
    mov     ecx, SIZEOF SECTION_TEMPLATE
    mul     ecx
    add     rdi, rax
    mov     eax, (PEGENCTX PTR [rbx]).NumSectio
    test    eax, eax
    jz      @@first_section
    
    ; Not first section - calculate from previous
    mov     rdx, (PEGENCTX PTR [rbx]).pSectionHeaders
    dec     eax
    mov     ecx, SIZEOF SECTION_TEMPLATE
    mul     ecx
    add     rdx, rax
    
    mov     eax, [rdx+12]
    add     eax, [rdx+8]
    add     eax, 0xFFF
    and     eax, 0xFFFFF000n to 0x1000
    jmp     @@set_va
    
@@first_section:
    mov     eax, 0x1000n at 0x1000
    
@@set_va:
    mov     edx, eax
    
    ; Calculate raw offset
    mov     rax, (PEGENCTX PTR [rbx]).CurrentOffset
    sub     rax, (PEGENCTX PTR [rbx]).pDosHeader
    add     eax, 0x1FF
    and     eax, 0xFFFFFE00n to 0x200
    mov     r8d, eaxnterToRawData
    
    ; Fill section header
    mov     rax, r12
    mov     [rdi], raxName (first 8 bytes)
    mov     dword ptr [rdi+8], r13d
    mov     dword ptr [rdi+12], edx
    
    ; Align raw size
    mov     eax, r14d
    add     eax, 0x1FF
    and     eax, 0xFFFFFE00
    mov     dword ptr [rdi+16], eax
    mov     dword ptr [rdi+20], r8dnterToRawData
    mov     dword ptr [rdi+24], 0nterToRelocatio
    mov     dword ptr [rdi+28], 0nterToLine
    mov     word ptr [rdi+32], 0NumberOfRelocatio
    mov     word ptr [rdi+34], 0NumberOfLine
    mov     dword ptr [rdi+36], r15d
    
    ; Copy section data if provided
    test    rsi, rsi
    jz      @@no_data
    
    mov     rdi, (PEGENCTX PTR [rbx]).pDosHeader
    add     rdi, r8
    mov     rcx, r14
    mov     rax, rcx
    rep     movsb
    mov     rcx, [rdi-8+16]
    sub     rcx, rax
    xor     eax, eax
    rep     stosb
    
@@no_data:
    ; Update co
    inc     (PEGENCTX PTR [rbx]).NumSectio
    mov     rdi, (PEGENCTX PTR [rbx]).pFileHeader
    mov     ax, (PEGENCTX PTR [rbx]).NumSectio
    mov     word ptr [rdi+2], axnt offset
    mov     eax, r8d
    add     eax, [rdi-40+16]
    mov     rax, (PEGENCTX PTR [rbx]).pDosHeader
    add     rax, r8
    add     rax, [rdi-40+16]
    mov     (PEGENCTX PTR [rbx]).CurrentOffset, rax
    
    mov     eax, 1
    jmp     @@add_do
    
@@add_failed:
    xor     eax, eax
    
@@add_done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_AddSection ENDP

; ================================================================================
; ENCODER FUNCTIONS
; ================================================================================

; Encoder_XOR - XOR encodi
Encoder_XOR PROC FRAME
    push    rsi
    push    rbx
    .PUSHREG rsi
    .PUSHREG rbx
    .ENDPROLOG
    
    mov     rsi, rcx
    mov     rbx, rdx
    mov     r9, r8
    
    shr     rbx, 3
    jz      @@do
    
@@loop_qwords:
    mov     rax, [rsi]
    xor     rax, r9
    mov     [rsi], rax
    add     rsi, 8
    dec     rbx
    jnz     @@loop_qwords
    
@@done:
    pop     rbx
    pop     rsi
    ret
Encoder_XOR ENDP

; Encoder_Rotate - Rotate encoding with ADD
Encoder_Rotate PROC FRAME
    push    rsi
    push    rbx
    push    r12
    .PUSHREG rsi
    .PUSHREG rbx
    .PUSHREG r12
    .ENDPROLOG
    
    mov     rsi, rcx
    mov     rbx, rdx
    mov     r12, r8
    
    shr     rbx, 3
    jz      @@do
    
@@loop_qwords:
    mov     rax, [rsi]
    add     rax, r12
    ror     rax, ENCODER_ROL_BITS
    mov     [rsi], rax
    add     rsi, 8
    dec     rbx
    jnz     @@loop_qwords
    
@@done:
    pop     r12
    pop     rbx
    pop     rsi
    ret
Encoder_Rotate ENDP

; Encoder_Polymorphic - Multi-layer encodi
Encoder_Polymorphic PROC FRAME
    push    rbx
    push    r12
    push    r13
    .PUSHREG rbx
    .PUSHREG r12
    .PUSHREG r13
    .ENDPROLOG
    
    mov     rbx, rcx
    mov     r12, rdx
    mov     r13, r8
    mov     rcx, rbx
    mov     rdx, r12
    mov     r8, r13
    rol     r8, 17
    call    Encoder_XORnverted key
    mov     rcx, rbx
    mov     rdx, r12
    mov     r8, r13
    not     r8
    add     r8, ENCODER_ADD_DELTA
    call    Encoder_Rotatenal key
    mov     rcx, rbx
    mov     rdx, r12
    mov     r8, r13
    call    Encoder_XOR
    
    pop     r13
    pop     r12
    pop     rbx
    ret
Encoder_Polymorphic ENDP

; PeGen_EncodeSection - Encode a section
; RCX = pContext, RDX = SectionIndex, R8 = Method
; Returns EAX = 1 (success)
PeGen_EncodeSection PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .PUSHREG r12
    .PUSHREG r13
    .ENDPROLOG
    
    mov     rbx, rcx
    mov     r12d, edx
    mov     r13d, r8dn header
    mov     rax, r12
    mov     ecx, SIZEOF SECTION_TEMPLATE
    mul     ecx
    mov     rsi, (PEGENCTX PTR [rbx]).pSectionHeaders
    add     rsi, raxn data location
    mov     edi, [rsi+20]nterToRawData
    mov     rcx, (PEGENCTX PTR [rbx]).pDosHeader
    add     rcx, rdi
    
    mov     edx, [rsi+16]
    mov     r8, (PEGENCTX PTR [rbx]).EncoderKey
    cmp     r13d, 0
    je      @@use_xor
    cmp     r13d, 1
    je      @@use_rotate
    
@@use_polymorphic:
    call    Encoder_Polymorphic
    jmp     @@encoding_do
    
@@use_xor:
    call    Encoder_XOR
    jmp     @@encoding_do
    
@@use_rotate:
    call    Encoder_Rotate
    
@@encoding_done:
    mov     (PEGENCTX PTR [rbx]).IsEncoded, 1
    mov     (PEGENCTX PTR [rbx]).EncoderMethod, r13d
    
    mov     eax, 1
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_EncodeSection ENDP

; ================================================================================
; FINALIZATION AND OUTPUT

; PeGen_Finalize - Finalize PE and calculate sizesntext, RDX = EntryPointRVA
; Returns EAX = 1 (success)
PeGen_Finalize PROC FRAME
    push    rbx
    push    rsi
    push    rdi
    .PUSHREG rbx
    .PUSHREG rsi
    .PUSHREG rdi
    .ENDPROLOG
    
    mov     rbx, rcx
    mov     esi, edxntry poi
    mov     rdi, (PEGENCTX PTR [rbx]).pOptionalHeader
    mov     dword ptr [rdi+16], esintryPoi
    
    ; Calculate total image size
    mov     eax, (PEGENCTX PTR [rbx]).NumSectio
    test    eax, eax
    jz      @@no_sectio
    
    dec     eax
    mov     ecx, SIZEOF SECTION_TEMPLATE
    mul     ecx
    mov     rsi, (PEGENCTX PTR [rbx]).pSectionHeaders
    add     rsi, rax
    
    mov     eax, [rsi+12]
    add     eax, [rsi+8]
    add     eax, 0xFFF
    and     eax, 0xFFFFF000n to section alignme
    
    mov     dword ptr [rdi+56], eax
    
@@no_sections:
    ; Set SizeOfHeaders
    mov     rax, (PEGENCTX PTR [rbx]).pSectionHeaders
    mov     rcx, (PEGENCTX PTR [rbx]).NumSectio
    mov     edx, SIZEOF SECTION_TEMPLATE
    mul     edx
    add     rax, 24n headers
    add     rax, (PEGENCTX PTR [rbx]).pDosHeader
    sub     rax, (PEGENCTX PTR [rbx]).pDosHeader
    add     eax, 0x1FF
    and     eax, 0xFFFFFE00
    mov     dword ptr [rdi+60], eax
    
    mov     eax, 1
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_Finalize ENDP

; PeGen_Cleanup - Free resources
; RCX = pCo
PeGen_Cleanup PROC FRAME
    push    rbx
    .PUSHREG rbx
    .ENDPROLOG
    
    mov     rbx, rcx
    mov     rcx, (PEGENCTX PTR [rbx]).pBuffer
    test    rcx, rcx
    jz      @@no_buffer
    
    xor     edx, edx
    mov     r8d, 0
    call    qword ptr [__imp_VirtualFree]
    
@@no_buffer:
    ; Zero co
    mov     rdi, rbx
    mov     rcx, SIZEOF PEGENCTX
    xor     eax, eax
    rep     stosb
    
    pop     rbx
    ret
PeGen_Cleanup ENDP

; ================================================================================
; UTILITY FUNCTIONS
; ================================================================================

; PeGen_CalculateChecksum - Calculate PE checksum
; RCX = pPEBuffer, RDX = Size
PeGen_CalculateChecksum PROC FRAME
    push    rsi
    .PUSHREG rsi
    .ENDPROLOG
    
    mov     rsi, rcx
    mov     rdi, rdx
    xor     eax, eax
    xor     ecx, ecx
    
    shr     rdi, 1
    jz      @@checksum_do
    
@@checksum_loop:
    movzx   edx, word ptr [rsi]
    add     eax, edx
    adc     eax, ecx
    mov     ecx, 0
    adc     ecx, 0
    add     rsi, 2
    dec     rdi
    jnz     @@checksum_loop
    
    ; Add high bits to low
    add     eax, ecx
    mov     edx, eax
    shr     edx, 16
    and     eax, 0xFFFF
    add     eax, edxnal fold
    mov     edx, eax
    shr     edx, 16
    add     eax, edx
    and     eax, 0xFFFF
    
@@checksum_done:
    pop     rsi
    ret
PeGen_CalculateChecksum ENDP

; ================================================================================
; EXPORTS
; ================================================================================

PUBLIC PeGen_I
PUBLIC PeGen_CreateDosHeader
PUBLIC PeGen_CreateNtHeaders
PUBLIC PeGen_AddSection
PUBLIC PeGen_EncodeSection
PUBLIC PeGen_Fi
PUBLIC PeGen_Clea
PUBLIC PeGen_CalculateChecksum
PUBLIC Encoder_XOR
PUBLIC Encoder_Rotate
PUBLIC Encoder_Polymorphic

; ================================================================================
; END OF FILE
; ================================================================================


