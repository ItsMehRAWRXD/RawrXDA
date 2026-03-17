; ================================================================================
; RawrXD PE Generator & Encoder - Pure MASM x64 Production Implementation
; ================================================================================
; Generates valid Windows PE32+ executables from scratch with polymorphic encoding
; Zero dependencies, zero imports, pure assembly implementation
; ================================================================================

OPTION CASEMAP:NONE
OPTION DOTNAME

; External Windows API references
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

; Directory entries
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

; PE Generator Context
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
    
    ; Section tracking
    SectionVirtualAddr  QWORD   ?
    SectionRawAddr      QWORD   ?
    
    ; Encoding state
    EncoderKey          QWORD   ?
    EncoderMethod       DWORD   ?
    IsEncoded           BYTE    ?
    
    ; Import/Export tracking
    pImportTable        QWORD   ?
    NumImports          DWORD   ?
    pExportTable        QWORD   ?
    NumExports          DWORD   ?
    
    ; Entry point
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

; Align value to boundary
ALIGN_VALUE MACRO val, align_val
    (((val) + ((align_val) - 1)) AND NOT ((align_val) - 1))
ENDM

; Calculate RVA from raw offset
CALC_RVA MACRO ctx, raw_offset
    (((raw_offset) - (ctx).pDosHeader) + (ctx).SectionVirtualAddr)
ENDM

; ================================================================================
; CODE SECTION
; ================================================================================

; ================================================================================
; PE GENERATOR CORE FUNCTIONS
; ================================================================================

; --------------------------------------------------------------------------------
; PeGen_Initialize - Initialize PE generator context
; RCX = pContext (PEGENCTX pointer)
; RDX = InitialBufferSize
; Returns EAX = 1 (success) or 0 (failure)
; --------------------------------------------------------------------------------
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
    
    mov     r12, rcx                    ; r12 = pContext
    mov     r13, rdx                    ; r13 = BufferSize
    
    ; Zero context structure
    mov     rdi, r12
    mov     rcx, SIZEOF PEGENCTX
    xor     eax, eax
    rep     stosb
    
    ; Allocate buffer with RWX permissions for encoding
    mov     rcx, 0                      ; lpAddress = NULL (let system choose)
    mov     rdx, r13                    ; dwSize
    mov     r8, 0x3000                 ; flAllocationType = MEM_COMMIT | MEM_RESERVE
    mov     r9, 0x40                   ; flProtect = PAGE_EXECUTE_READWRITE
    
    call    qword ptr [__imp_VirtualAlloc]
    
    test    rax, rax
    jz      @@init_failed
    
    mov     (PEGENCTX PTR [r12]).pBuffer, rax
    mov     (PEGENCTX PTR [r12]).BufferSize, r13
    mov     (PEGENCTX PTR [r12]).CurrentOffset, rax
    mov     (PEGENCTX PTR [r12]).pDosHeader, rax
    
    ; Initialize encoding key with entropy
    rdtsc
    shl     rax, 32
    or      rax, rdx
    xor     rax, ENCODER_XOR_KEY
    mov     (PEGENCTX PTR [r12]).EncoderKey, rax
    
    mov     eax, 1
    jmp     @@init_done
    
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
; RCX = pContext
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
    
    mov     rbx, rcx                    ; rbx = pContext
    mov     rdi, (PEGENCTX PTR [rbx]).pDosHeader
    
    ; Magic number "MZ"
    mov     word ptr [rdi], PE_MAGIC
    
    ; DOS stub (minimal)
    mov     word ptr [rdi+2], 0x0090    ; Bytes on last page
    mov     word ptr [rdi+4], 0x0003    ; Pages in file
    mov     word ptr [rdi+6], 0x0000    ; Relocations
    mov     word ptr [rdi+8], 0x0004    ; Size of header in paragraphs
    mov     word ptr [rdi+10], 0x0000   ; Minimum extra paragraphs
    mov     word ptr [rdi+12], 0xFFFF   ; Maximum extra paragraphs
    mov     word ptr [rdi+14], 0x0000   ; Initial SS value
    mov     word ptr [rdi+16], 0x00B8   ; Initial SP value
    mov     word ptr [rdi+18], 0x0000   ; Checksum
    mov     word ptr [rdi+20], 0x0000   ; Initial IP value
    mov     word ptr [rdi+22], 0x0000   ; Initial CS value
    mov     word ptr [rdi+24], 0x0040   ; File address of relocation table
    mov     word ptr [rdi+26], 0x0000   ; Overlay number
    
    ; Reserved words
    xor     eax, eax
    mov     [rdi+28], eax
    mov     [rdi+32], eax
    mov     [rdi+36], eax
    mov     [rdi+40], eax
    mov     [rdi+44], eax
    mov     [rdi+46], eax
    
    ; PE header offset (at 0x40)
    mov     dword ptr [rdi+60], 0x40    ; e_lfanew
    
    ; Simple DOS stub
    mov     byte ptr [rdi+64], 0xEB
    mov     byte ptr [rdi+65], 0x02
    mov     byte ptr [rdi+66], 0x90
    mov     byte ptr [rdi+67], 0x90
    
    ; Update context
    mov     rax, (PEGENCTX PTR [rbx]).pDosHeader
    add     rax, 64 + 14                ; After DOS header + stub
    mov     (PEGENCTX PTR [rbx]).CurrentOffset, rax
    
    mov     eax, 1
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_CreateDosHeader ENDP

; --------------------------------------------------------------------------------
; PeGen_CreateNtHeaders - Create PE32+ headers
; RCX = pContext
; RDX = ImageBase
; R8  = Subsystem (2=GUI, 3=Console)
; Returns EAX = 1 (success)
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
    
    mov     rbx, rcx                    ; rbx = pContext
    mov     r12, rdx                    ; r12 = ImageBase
    mov     r13d, r8d                   ; r13d = Subsystem
    
    ; Align to 8 bytes
    mov     rax, (PEGENCTX PTR [rbx]).CurrentOffset
    add     rax, 7
    and     rax, NOT 7
    mov     (PEGENCTX PTR [rbx]).CurrentOffset, rax
    mov     rdi, rax
    
    ; PE Signature
    mov     dword ptr [rdi], PE_SIGNATURE
    add     rdi, 4
    
    ; COFF File Header
    mov     (PEGENCTX PTR [rbx]).pFileHeader, rdi
    
    mov     word ptr [rdi], MACHINE_AMD64   ; Machine
    mov     word ptr [rdi+2], 0             ; NumberOfSections (will be updated)
    mov     dword ptr [rdi+4], 0            ; TimeDateStamp
    mov     dword ptr [rdi+8], 0            ; PointerToSymbolTable
    mov     dword ptr [rdi+12], 0           ; NumberOfSymbols
    mov     word ptr [rdi+16], 0            ; SizeOfOptionalHeader (will be set)
    mov     word ptr [rdi+18], IMAGE_FILE_EXECUTABLE_IMAGE OR IMAGE_FILE_LARGE_ADDRESS_AWARE
    
    add     rdi, 20                         ; Size of COFF header
    
    ; Optional Header (PE32+)
    mov     (PEGENCTX PTR [rbx]).pOptionalHeader, rdi
    mov     (PEGENCTX PTR [rbx]).pNtHeaders, (PEGENCTX PTR [rbx]).pFileHeader
    
    mov     word ptr [rdi], OPTIONAL_HDR64_MAGIC  ; Magic
    mov     byte ptr [rdi+2], 0             ; MajorLinkerVersion
    mov     byte ptr [rdi+3], 0             ; MinorLinkerVersion
    mov     dword ptr [rdi+4], 0            ; SizeOfCode
    mov     dword ptr [rdi+8], 0            ; SizeOfInitializedData
    mov     dword ptr [rdi+12], 0           ; SizeOfUninitializedData
    mov     dword ptr [rdi+16], 0           ; AddressOfEntryPoint (will be set)
    mov     dword ptr [rdi+20], 0x1000      ; BaseOfCode
    mov     qword ptr [rdi+24], r12         ; ImageBase
    mov     dword ptr [rdi+32], 0x1000      ; SectionAlignment
    mov     dword ptr [rdi+36], 0x200       ; FileAlignment
    mov     word ptr [rdi+40], 6            ; MajorOperatingSystemVersion
    mov     word ptr [rdi+42], 0            ; MinorOperatingSystemVersion
    mov     word ptr [rdi+44], 0            ; MajorImageVersion
    mov     word ptr [rdi+46], 0            ; MinorImageVersion
    mov     word ptr [rdi+48], 6            ; MajorSubsystemVersion
    mov     word ptr [rdi+50], 0            ; MinorSubsystemVersion
    mov     dword ptr [rdi+52], 0           ; Win32VersionValue
    mov     dword ptr [rdi+56], 0x10000     ; SizeOfImage (will be updated)
    mov     dword ptr [rdi+60], 0x400       ; SizeOfHeaders
    mov     dword ptr [rdi+64], 0           ; Checksum
    mov     word ptr [rdi+68], r13w         ; Subsystem
    mov     word ptr [rdi+70], IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE OR IMAGE_DLLCHARACTERISTICS_NX_COMPAT
    mov     qword ptr [rdi+72], 0x100000    ; SizeOfStackReserve
    mov     qword ptr [rdi+80], 0x10000     ; SizeOfStackCommit
    mov     qword ptr [rdi+88], 0x100000    ; SizeOfHeapReserve
    mov     qword ptr [rdi+96], 0x10000     ; SizeOfHeapCommit
    mov     dword ptr [rdi+104], 0          ; LoaderFlags
    mov     dword ptr [rdi+108], 16         ; NumberOfRvaAndSizes
    
    ; Data Directory
    add     rdi, 112
    mov     (PEGENCTX PTR [rbx]).pDataDirectory, rdi
    
    ; Zero all 16 data directories
    mov     rcx, 16 * 8                     ; 16 entries * 8 bytes each
    xor     eax, eax
    rep     stosb
    
    ; Update sizes
    mov     rax, (PEGENCTX PTR [rbx]).pOptionalHeader
    mov     rdx, rdi
    sub     rdx, rax
    mov     word ptr (PEGENCTX PTR [rbx]).pFileHeader[16], dx
    
    ; Update current offset
    mov     (PEGENCTX PTR [rbx]).CurrentOffset, rdi
    
    ; Section headers start here (aligned to 8)
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
; RCX = pContext
; RDX = pName (8-byte name)
; R8  = VirtualSize
; R9  = RawSize
; [RSP+0x28] = Characteristics
; [RSP+0x30] = pData (optional, can be NULL)
; Returns EAX = 1 (success) or 0 (failure)
; --------------------------------------------------------------------------------
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
    
    mov     rbx, rcx                    ; rbx = pContext
    mov     r12, rdx                    ; r12 = pName
    mov     r13d, r8d                   ; r13d = VirtualSize
    mov     r14d, r9d                   ; r14d = RawSize
    mov     r15d, [rsp+0x58]            ; r15d = Characteristics
    mov     rsi, [rsp+0x60]             ; rsi = pData
    
    ; Check section limit
    mov     eax, (PEGENCTX PTR [rbx]).NumSections
    cmp     eax, MAX_SECTIONS
    jae     @@add_failed
    
    ; Calculate section header location
    mov     rdi, (PEGENCTX PTR [rbx]).pSectionHeaders
    mov     ecx, SIZEOF SECTION_TEMPLATE
    mul     ecx
    add     rdi, rax
    
    ; Calculate virtual address
    mov     eax, (PEGENCTX PTR [rbx]).NumSections
    test    eax, eax
    jz      @@first_section
    
    ; Not first section - calculate from previous
    mov     rdx, (PEGENCTX PTR [rbx]).pSectionHeaders
    dec     eax
    mov     ecx, SIZEOF SECTION_TEMPLATE
    mul     ecx
    add     rdx, rax
    
    mov     eax, [rdx+12]               ; Previous VirtualAddress
    add     eax, [rdx+8]                ; + Previous VirtualSize
    add     eax, 0xFFF
    and     eax, 0xFFFFF000             ; Align to 0x1000
    jmp     @@set_va
    
@@first_section:
    mov     eax, 0x1000                 ; First section at 0x1000
    
@@set_va:
    mov     edx, eax                    ; edx = VirtualAddress
    
    ; Calculate raw offset
    mov     rax, (PEGENCTX PTR [rbx]).CurrentOffset
    sub     rax, (PEGENCTX PTR [rbx]).pDosHeader
    add     eax, 0x1FF
    and     eax, 0xFFFFFE00             ; Align to 0x200
    mov     r8d, eax                    ; r8d = PointerToRawData
    
    ; Fill section header
    mov     rax, r12
    mov     [rdi], rax                  ; Name (first 8 bytes)
    mov     dword ptr [rdi+8], r13d     ; VirtualSize
    mov     dword ptr [rdi+12], edx     ; VirtualAddress
    
    ; Align raw size
    mov     eax, r14d
    add     eax, 0x1FF
    and     eax, 0xFFFFFE00
    mov     dword ptr [rdi+16], eax     ; SizeOfRawData
    mov     dword ptr [rdi+20], r8d     ; PointerToRawData
    mov     dword ptr [rdi+24], 0       ; PointerToRelocations
    mov     dword ptr [rdi+28], 0       ; PointerToLinenumbers
    mov     word ptr [rdi+32], 0        ; NumberOfRelocations
    mov     word ptr [rdi+34], 0        ; NumberOfLinenumbers
    mov     dword ptr [rdi+36], r15d    ; Characteristics
    
    ; Copy section data if provided
    test    rsi, rsi
    jz      @@no_data
    
    mov     rdi, (PEGENCTX PTR [rbx]).pDosHeader
    add     rdi, r8                     ; rdi = destination
    mov     rcx, r14                    ; rcx = size
    mov     rax, rcx
    
    ; Copy data
    rep     movsb
    
    ; Pad with zeros
    mov     rcx, [rdi-8+16]             ; SizeOfRawData
    sub     rcx, rax
    xor     eax, eax
    rep     stosb
    
@@no_data:
    ; Update context
    inc     (PEGENCTX PTR [rbx]).NumSections
    
    ; Update file header
    mov     rdi, (PEGENCTX PTR [rbx]).pFileHeader
    mov     ax, (PEGENCTX PTR [rbx]).NumSections
    mov     word ptr [rdi+2], ax
    
    ; Update current offset
    mov     eax, r8d
    add     eax, [rdi-40+16]            ; + SizeOfRawData
    mov     rax, (PEGENCTX PTR [rbx]).pDosHeader
    add     rax, r8
    add     rax, [rdi-40+16]
    mov     (PEGENCTX PTR [rbx]).CurrentOffset, rax
    
    mov     eax, 1
    jmp     @@add_done
    
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

; Encoder_XOR - XOR encoding
; RCX = pData, RDX = Size, R8 = Key
Encoder_XOR PROC FRAME
    push    rsi
    push    rbx
    .PUSHREG rsi
    .PUSHREG rbx
    .ENDPROLOG
    
    mov     rsi, rcx
    mov     rbx, rdx
    mov     r9, r8
    
    shr     rbx, 3                      ; Process 8 bytes at a time
    jz      @@done
    
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
; RCX = pData, RDX = Size, R8 = Key
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
    jz      @@done
    
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

; Encoder_Polymorphic - Multi-layer encoding
; RCX = pData, RDX = Size, R8 = Key
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
    
    ; Layer 1: XOR with shifted key
    mov     rcx, rbx
    mov     rdx, r12
    mov     r8, r13
    rol     r8, 17
    call    Encoder_XOR
    
    ; Layer 2: Rotate with inverted key
    mov     rcx, rbx
    mov     rdx, r12
    mov     r8, r13
    not     r8
    add     r8, ENCODER_ADD_DELTA
    call    Encoder_Rotate
    
    ; Layer 3: XOR with original key
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
    mov     r13d, r8d
    
    ; Get section header
    mov     rax, r12
    mov     ecx, SIZEOF SECTION_TEMPLATE
    mul     ecx
    mov     rsi, (PEGENCTX PTR [rbx]).pSectionHeaders
    add     rsi, rax
    
    ; Get section data location
    mov     edi, [rsi+20]               ; PointerToRawData
    mov     rcx, (PEGENCTX PTR [rbx]).pDosHeader
    add     rcx, rdi                    ; rcx = pData
    
    mov     edx, [rsi+16]               ; rdx = SizeOfRawData
    mov     r8, (PEGENCTX PTR [rbx]).EncoderKey
    
    ; Call appropriate encoder
    cmp     r13d, 0
    je      @@use_xor
    cmp     r13d, 1
    je      @@use_rotate
    
@@use_polymorphic:
    call    Encoder_Polymorphic
    jmp     @@encoding_done
    
@@use_xor:
    call    Encoder_XOR
    jmp     @@encoding_done
    
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
; ================================================================================

; PeGen_Finalize - Finalize PE and calculate sizes
; RCX = pContext, RDX = EntryPointRVA
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
    mov     esi, edx
    
    ; Set entry point
    mov     rdi, (PEGENCTX PTR [rbx]).pOptionalHeader
    mov     dword ptr [rdi+16], esi     ; AddressOfEntryPoint
    
    ; Calculate total image size
    mov     eax, (PEGENCTX PTR [rbx]).NumSections
    test    eax, eax
    jz      @@no_sections
    
    dec     eax
    mov     ecx, SIZEOF SECTION_TEMPLATE
    mul     ecx
    mov     rsi, (PEGENCTX PTR [rbx]).pSectionHeaders
    add     rsi, rax
    
    mov     eax, [rsi+12]               ; VirtualAddress
    add     eax, [rsi+8]                ; + VirtualSize
    add     eax, 0xFFF
    and     eax, 0xFFFFF000             ; Align to section alignment
    
    mov     dword ptr [rdi+56], eax     ; SizeOfImage
    
@@no_sections:
    ; Set SizeOfHeaders
    mov     rax, (PEGENCTX PTR [rbx]).pSectionHeaders
    mov     rcx, (PEGENCTX PTR [rbx]).NumSections
    mov     edx, SIZEOF SECTION_TEMPLATE
    mul     edx
    add     rax, 24                     ; + section headers
    add     rax, (PEGENCTX PTR [rbx]).pDosHeader
    sub     rax, (PEGENCTX PTR [rbx]).pDosHeader
    add     eax, 0x1FF
    and     eax, 0xFFFFFE00
    mov     dword ptr [rdi+60], eax     ; SizeOfHeaders
    
    mov     eax, 1
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PeGen_Finalize ENDP

; PeGen_Cleanup - Free resources
; RCX = pContext
PeGen_Cleanup PROC FRAME
    push    rbx
    .PUSHREG rbx
    .ENDPROLOG
    
    mov     rbx, rcx
    
    ; Free buffer
    mov     rcx, (PEGENCTX PTR [rbx]).pBuffer
    test    rcx, rcx
    jz      @@no_buffer
    
    xor     edx, edx                    ; dwFreeType = MEM_RELEASE
    mov     r8d, 0                      ; dwSize = 0 (release all)
    call    qword ptr [__imp_VirtualFree]
    
@@no_buffer:
    ; Zero context
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
    xor     eax, eax                    ; checksum
    xor     ecx, ecx                    ; high bits
    
    shr     rdi, 1                      ; Process words
    jz      @@checksum_done
    
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
    
    ; Fold to 16 bits
    mov     edx, eax
    shr     edx, 16
    and     eax, 0xFFFF
    add     eax, edx
    
    ; Final fold
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

PUBLIC PeGen_Initialize
PUBLIC PeGen_CreateDosHeader
PUBLIC PeGen_CreateNtHeaders
PUBLIC PeGen_AddSection
PUBLIC PeGen_EncodeSection
PUBLIC PeGen_Finalize
PUBLIC PeGen_Cleanup
PUBLIC PeGen_CalculateChecksum
PUBLIC Encoder_XOR
PUBLIC Encoder_Rotate
PUBLIC Encoder_Polymorphic

; ================================================================================
; END OF FILE
; ================================================================================
