; ============================================================================
; PE_Backend_Emitter.asm
; Monolithic PE Writer / Machine Code Emitter Backend in x64 MASM
; Zero dependencies, pure structural definitions and emitter macros/functions.
; ============================================================================

; ============================================
; EXPORTS
; ============================================
PUBLIC Emit_DOS_Header
PUBLIC Emit_DOS_Stub
PUBLIC Emit_NT_Headers
PUBLIC Emit_Section_Header
PUBLIC Emit_Import_Descriptor

; ----------------------------------------------------------------------------
; 1. PE STRUCTS & TEMPLATES (Backend Designer View)
; ----------------------------------------------------------------------------

IMAGE_DOS_HEADER STRUCT
    e_magic     WORD    ? ; 0x5A4D ('MZ')
    e_cblp      WORD    ?
    e_cp        WORD    ?
    e_crlc      WORD    ?
    e_cparhdr   WORD    ?
    e_minalloc  WORD    ?
    e_maxalloc  WORD    ?
    e_ss        WORD    ?
    e_sp        WORD    ?
    e_csum      WORD    ?
    e_ip        WORD    ?
    e_cs        WORD    ?
    e_lfarlc    WORD    ?
    e_ovno      WORD    ?
    e_res       WORD 4 DUP(?)
    e_oemid     WORD    ?
    e_oeminfo   WORD    ?
    e_res2      WORD 10 DUP(?)
    e_lfanew    DWORD   ? ; Offset to NT headers
IMAGE_DOS_HEADER ENDS

IMAGE_FILE_HEADER STRUCT
    Machine               WORD    ? ; 0x8664 for x64
    NumberOfSections      WORD    ?
    TimeDateStamp         DWORD   ?
    PointerToSymbolTable  DWORD   ?
    NumberOfSymbols       DWORD   ?
    SizeOfOptionalHeader  WORD    ?
    Characteristics       WORD    ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  DWORD   ?
    Size1           DWORD   ? ; "Size" is a MASM keyword (SIZE operator)
IMAGE_DATA_DIRECTORY ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT
    Magic                       WORD    ? ; 0x020B for PE32+
    MajorLinkerVersion          BYTE    ?
    MinorLinkerVersion          BYTE    ?
    SizeOfCode                  DWORD   ?
    SizeOfInitializedData       DWORD   ?
    SizeOfUninitializedData     DWORD   ?
    AddressOfEntryPoint         DWORD   ?
    BaseOfCode                  DWORD   ?
    ImageBase                   QWORD   ?
    SectionAlignment            DWORD   ?
    FileAlignment               DWORD   ?
    MajorOperatingSystemVersion WORD    ?
    MinorOperatingSystemVersion WORD    ?
    MajorImageVersion           WORD    ?
    MinorImageVersion           WORD    ?
    MajorSubsystemVersion       WORD    ?
    MinorSubsystemVersion       WORD    ?
    Win32VersionValue           DWORD   ?
    SizeOfImage                 DWORD   ?
    SizeOfHeaders               DWORD   ?
    CheckSum                    DWORD   ?
    Subsystem                   WORD    ?
    DllCharacteristics          WORD    ?
    SizeOfStackReserve          QWORD   ?
    SizeOfStackCommit           QWORD   ?
    SizeOfHeapReserve           QWORD   ?
    SizeOfHeapCommit            QWORD   ?
    LoaderFlags                 DWORD   ?
    NumberOfRvaAndSizes         DWORD   ?
    DataDirectory               IMAGE_DATA_DIRECTORY 16 DUP(<>)
IMAGE_OPTIONAL_HEADER64 ENDS

IMAGE_NT_HEADERS64 STRUCT
    Signature       DWORD                   ? ; 0x00004550 ('PE\0\0')
    FileHeader      IMAGE_FILE_HEADER       <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_SECTION_HEADER STRUCT
    Name1                 BYTE 8 DUP(?)
    VirtualSize           DWORD   ?
    VirtualAddress        DWORD   ?
    SizeOfRawData         DWORD   ?
    PointerToRawData      DWORD   ?
    PointerToRelocations  DWORD   ?
    PointerToLinenumbers  DWORD   ?
    NumberOfRelocations   WORD    ?
    NumberOfLinenumbers   WORD    ?
    Characteristics       DWORD   ?
IMAGE_SECTION_HEADER ENDS

IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk  DWORD   ?
    TimeDateStamp       DWORD   ?
    ForwarderChain      DWORD   ?
    Name1               DWORD   ?
    FirstThunk          DWORD   ?
IMAGE_IMPORT_DESCRIPTOR ENDS

; ----------------------------------------------------------------------------
; 2. EMITTER MACROS (Machine Code Generation)
; ----------------------------------------------------------------------------

; Emit_FunctionPrologue - Standard x64 stack frame setup
Emit_FunctionPrologue MACRO stackSize:REQ
    push rbp
    mov rbp, rsp
    sub rsp, stackSize
ENDM

; Emit_FunctionEpilogue - Standard x64 stack frame teardown
Emit_FunctionEpilogue MACRO stackSize:REQ
    add rsp, stackSize
    pop rbp
    ret
ENDM

; ============================================
; CODE SECTION
; ============================================
.code

; ----------------------------------------------------------------------------
; 3. PE WRITER BACKEND (Monolithic Implementation)
; ----------------------------------------------------------------------------

; Initialize a DOS Header in memory
; RCX = Pointer to IMAGE_DOS_HEADER buffer
; RDX = Offset to NT Headers (e_lfanew)
Emit_DOS_Header PROC
    Emit_FunctionPrologue 0
    
    ; Zero out the structure first (64 bytes)
    xor rax, rax
    mov qword ptr [rcx], rax
    mov qword ptr [rcx+8], rax
    mov qword ptr [rcx+16], rax
    mov qword ptr [rcx+24], rax
    mov qword ptr [rcx+32], rax
    mov qword ptr [rcx+40], rax
    mov qword ptr [rcx+48], rax
    mov qword ptr [rcx+56], rax

    ; Set MZ magic
    mov word ptr [rcx], 5A4Dh
    
    ; Set e_lfanew
    mov dword ptr [rcx+60], edx
    
    Emit_FunctionEpilogue 0
Emit_DOS_Header ENDP

; Emit standard DOS Stub
; RCX = Pointer to buffer (must be at least 64 bytes)
Emit_DOS_Stub PROC
    Emit_FunctionPrologue 0
    
    ; Standard 16-bit DOS stub: "This program cannot be run in DOS mode."
    ; 0E 1F BA 0E 00 B4 09 CD 21 B8 01 4C CD 21
    mov dword ptr [rcx], 0EBA1F0Eh
    mov dword ptr [rcx+4], 0CD09B400h
    mov dword ptr [rcx+8], 04C01B821h
    mov dword ptr [rcx+12], 000021CDh
    
    ; String: "This program cannot be run in DOS mode.$"
    ; We'll just copy it as qwords for efficiency
    mov rax, 6F72702073696854h    ; "This pro"
    mov qword ptr [rcx+0Eh], rax  ; stub sets DX=0x0E
    mov rax, 6E6163206D617267h    ; "gram can"
    mov qword ptr [rcx+16h], rax
    mov rax, 7220656220746F6Eh    ; "not be r"
    mov qword ptr [rcx+1Eh], rax
    mov rax, 4F44206E69206E75h    ; "un in DO"
    mov qword ptr [rcx+26h], rax
    mov rax, 242E65646F6D2053h    ; "S mode.$"
    mov qword ptr [rcx+2Eh], rax
    
    Emit_FunctionEpilogue 0
Emit_DOS_Stub ENDP

; Initialize NT Headers (x64)
; RCX = Pointer to IMAGE_NT_HEADERS64 buffer
; RDX = Entry Point RVA
; R8  = Image Base
Emit_NT_Headers PROC
    Emit_FunctionPrologue 0
    
    ; Set PE Signature
    mov dword ptr [rcx], 00004550h
    
    ; --- File Header ---
    mov word ptr [rcx+4], 8664h     ; Machine = AMD64
    mov word ptr [rcx+6], 1         ; NumberOfSections (default 1, update later)
    mov dword ptr [rcx+8], 0        ; TimeDateStamp
    mov dword ptr [rcx+12], 0       ; PointerToSymbolTable
    mov dword ptr [rcx+16], 0       ; NumberOfSymbols
    mov word ptr [rcx+20], 240      ; SizeOfOptionalHeader (F0h for x64)
    mov word ptr [rcx+22], 0022h    ; Characteristics (Executable | LargeAddressAware)
    
    ; --- Optional Header ---
    mov word ptr [rcx+24], 020Bh    ; Magic = PE32+
    mov byte ptr [rcx+26], 14       ; MajorLinkerVersion
    mov byte ptr [rcx+27], 0        ; MinorLinkerVersion
    mov dword ptr [rcx+28], 0       ; SizeOfCode
    mov dword ptr [rcx+32], 0       ; SizeOfInitializedData
    mov dword ptr [rcx+36], 0       ; SizeOfUninitializedData
    mov dword ptr [rcx+40], edx     ; AddressOfEntryPoint
    mov dword ptr [rcx+44], 1000h   ; BaseOfCode
    mov qword ptr [rcx+48], r8      ; ImageBase
    mov dword ptr [rcx+56], 1000h   ; SectionAlignment
    mov dword ptr [rcx+60], 200h    ; FileAlignment
    mov word ptr [rcx+64], 6        ; MajorOperatingSystemVersion
    mov word ptr [rcx+66], 0        ; MinorOperatingSystemVersion
    mov word ptr [rcx+68], 0        ; MajorImageVersion
    mov word ptr [rcx+70], 0        ; MinorImageVersion
    mov word ptr [rcx+72], 6        ; MajorSubsystemVersion
    mov word ptr [rcx+74], 0        ; MinorSubsystemVersion
    mov dword ptr [rcx+76], 0       ; Win32VersionValue
    mov dword ptr [rcx+80], 0       ; SizeOfImage (update later)
    mov dword ptr [rcx+84], 400h    ; SizeOfHeaders
    mov dword ptr [rcx+88], 0       ; CheckSum
    mov word ptr [rcx+92], 3        ; Subsystem = Windows CUI
    mov word ptr [rcx+94], 8140h    ; DllCharacteristics (DynamicBase | NXCompat | TerminalServerAware)
    
    ; Stack & Heap
    mov rax, 100000h
    mov qword ptr [rcx+96], rax     ; SizeOfStackReserve
    mov rax, 1000h
    mov qword ptr [rcx+104], rax    ; SizeOfStackCommit
    mov rax, 100000h
    mov qword ptr [rcx+112], rax    ; SizeOfHeapReserve
    mov rax, 1000h
    mov qword ptr [rcx+120], rax    ; SizeOfHeapCommit
    
    mov dword ptr [rcx+128], 0      ; LoaderFlags
    mov dword ptr [rcx+132], 16     ; NumberOfRvaAndSizes
    
    ; Clear Data Directories (16 * 8 = 128 bytes)
    lea rax, [rcx+136]
    xor r9, r9
clear_dirs:
    mov qword ptr [rax+r9*8], 0
    inc r9
    cmp r9, 16
    jl clear_dirs
    
    Emit_FunctionEpilogue 0
Emit_NT_Headers ENDP

; Initialize a Section Header
; RCX = Pointer to IMAGE_SECTION_HEADER buffer
; RDX = Pointer to 8-byte Name
; R8  = VirtualAddress
; R9  = VirtualSize
; [RSP+40] = PointerToRawData
; [RSP+48] = SizeOfRawData
; [RSP+56] = Characteristics
Emit_Section_Header PROC
    Emit_FunctionPrologue 0
    
    ; Copy Name (8 bytes)
    mov rax, qword ptr [rdx]
    mov qword ptr [rcx], rax
    
    ; Set Sizes and Addresses
    mov dword ptr [rcx+8], r9d      ; VirtualSize
    mov dword ptr [rcx+12], r8d     ; VirtualAddress
    
    ; Win64 stack args are at [rbp+48].. after the prologue.
    mov eax, dword ptr [rbp+56]
    mov dword ptr [rcx+16], eax     ; SizeOfRawData
    
    mov eax, dword ptr [rbp+48]
    mov dword ptr [rcx+20], eax     ; PointerToRawData
    
    ; Clear Relocations/Linenumbers
    mov dword ptr [rcx+24], 0       ; PointerToRelocations
    mov dword ptr [rcx+28], 0       ; PointerToLinenumbers
    mov dword ptr [rcx+32], 0       ; NumberOfRelocations/Linenumbers
    
    ; Set Characteristics
    mov eax, dword ptr [rbp+64]
    mov dword ptr [rcx+36], eax     ; Characteristics
    
    Emit_FunctionEpilogue 0
Emit_Section_Header ENDP

; Initialize an Import Descriptor
; RCX = Pointer to IMAGE_IMPORT_DESCRIPTOR buffer
; RDX = OriginalFirstThunk (RVA of ILT)
; R8  = Name RVA (RVA of DLL Name string)
; R9  = FirstThunk (RVA of IAT)
Emit_Import_Descriptor PROC
    Emit_FunctionPrologue 0
    
    mov dword ptr [rcx], edx        ; OriginalFirstThunk
    mov dword ptr [rcx+4], 0        ; TimeDateStamp
    mov dword ptr [rcx+8], 0        ; ForwarderChain
    mov dword ptr [rcx+12], r8d     ; Name
    mov dword ptr [rcx+16], r9d     ; FirstThunk
    
    Emit_FunctionEpilogue 0
Emit_Import_Descriptor ENDP

END
