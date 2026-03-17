; PE Header Definitions for Pure MASM Compiler
; Defines all necessary structures for Portable Executable format

; PE Header Constants
IMAGE_DOS_SIGNATURE equ 5A4Dh
IMAGE_NT_SIGNATURE equ 00004550h

; DOS Header Structure
DOS_HEADER struct
    e_magic     dw ?
    e_cblp      dw ?
    e_cp        dw ?
    e_crlc      dw ?
    e_cparhdr   dw ?
    e_minalloc  dw ?
    e_maxalloc  dw ?
    e_ss        dw ?
    e_sp        dw ?
    e_csum      dw ?
    e_ip        dw ?
    e_cs        dw ?
    e_lfarlc    dw ?
    e_ovno      dw ?
    e_res       dw 4 dup(?)
    e_oemid     dw ?
    e_oeminfo   dw ?
    e_res2      dw 10 dup(?)
    e_lfanew    dd ?
DOS_HEADER ends

; PE Header Structure
IMAGE_NT_HEADERS struct
    Signature           dd ?
    FileHeader          IMAGE_FILE_HEADER {}
    OptionalHeader      IMAGE_OPTIONAL_HEADER {}
IMAGE_NT_HEADERS ends

; File Header Structure
IMAGE_FILE_HEADER struct
    Machine                 dw ?
    NumberOfSections       dw ?
    TimeDateStamp          dd ?
    PointerToSymbolTable   dd ?
    NumberOfSymbols        dd ?
    SizeOfOptionalHeader   dw ?
    Characteristics        dw ?
IMAGE_FILE_HEADER ends

; Optional Header Structure
IMAGE_OPTIONAL_HEADER struct
    Magic                       dw ?
    MajorLinkerVersion          db ?
    MinorLinkerVersion          db ?
    SizeOfCode                  dd ?
    SizeOfInitializedData       dd ?
    SizeOfUninitializedData     dd ?
    AddressOfEntryPoint         dd ?
    BaseOfCode                  dd ?
    BaseOfData                  dd ?
    ImageBase                   dd ?
    SectionAlignment            dd ?
    FileAlignment               dd ?
    MajorOperatingSystemVersion dw ?
    MinorOperatingSystemVersion dw ?
    MajorImageVersion           dw ?
    MinorImageVersion           dw ?
    MajorSubsystemVersion       dw ?
    MinorSubsystemVersion       dw ?
    Win32VersionValue           dd ?
    SizeOfImage                 dd ?
    SizeOfHeaders               dd ?
    CheckSum                    dd ?
    Subsystem                   dw ?
    DllCharacteristics          dw ?
    SizeOfStackReserve          dd ?
    SizeOfStackCommit           dd ?
    SizeOfHeapReserve           dd ?
    SizeOfHeapCommit            dd ?
    LoaderFlags                 dd ?
    NumberOfRvaAndSizes         dd ?
    DataDirectory               dd 16 dup(?)
IMAGE_OPTIONAL_HEADER ends

; Section Header Structure
IMAGE_SECTION_HEADER struct
    Name                    db 8 dup(?)
    VirtualSize             dd ?
    VirtualAddress          dd ?
    SizeOfRawData           dd ?
    PointerToRawData        dd ?
    PointerToRelocations    dd ?
    PointerToLinenumbers    dd ?
    NumberOfRelocations     dw ?
    NumberOfLinenumbers     dw ?
    Characteristics         dd ?
IMAGE_SECTION_HEADER ends

; Machine Types
IMAGE_FILE_MACHINE_I386     equ 014Ch
IMAGE_FILE_MACHINE_AMD64    equ 8664h

; Subsystem Types
IMAGE_SUBSYSTEM_WINDOWS_GUI     equ 2
IMAGE_SUBSYSTEM_WINDOWS_CUI     equ 3

; Section Characteristics
IMAGE_SCN_CNT_CODE              equ 00000020h
IMAGE_SCN_CNT_INITIALIZED_DATA  equ 00000040h
IMAGE_SCN_MEM_EXECUTE           equ 20000000h
IMAGE_SCN_MEM_READ              equ 40000000h
IMAGE_SCN_MEM_WRITE             equ 80000000h

; Data Directory Indices
IMAGE_DIRECTORY_ENTRY_EXPORT    equ 0
IMAGE_DIRECTORY_ENTRY_IMPORT    equ 1
IMAGE_DIRECTORY_ENTRY_RESOURCE  equ 2
IMAGE_DIRECTORY_ENTRY_EXCEPTION equ 3
IMAGE_DIRECTORY_ENTRY_SECURITY  equ 4
IMAGE_DIRECTORY_ENTRY_BASERELOC equ 5
IMAGE_DIRECTORY_ENTRY_DEBUG     equ 6
IMAGE_DIRECTORY_ENTRY_ARCHITECTURE equ 7
IMAGE_DIRECTORY_ENTRY_GLOBALPTR equ 8
IMAGE_DIRECTORY_ENTRY_TLS       equ 9
IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG equ 10
IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT equ 11
IMAGE_DIRECTORY_ENTRY_IAT       equ 12
IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT equ 13
IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR equ 14

; Function to create basic PE header
CreatePEHeader macro
    local dos_header, pe_header, file_header, opt_header, text_section
    
    ; DOS Header
    dos_header DOS_HEADER {
        IMAGE_DOS_SIGNATURE,
        0, 0, 0, 0, 0, 0FFFFh,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        40h  ; e_lfanew - offset to PE header
    }
    
    ; PE Header
    pe_header IMAGE_NT_HEADERS {
        IMAGE_NT_SIGNATURE
    }
    
    ; File Header
    file_header IMAGE_FILE_HEADER {
        IMAGE_FILE_MACHINE_I386,
        1,      ; NumberOfSections
        0,      ; TimeDateStamp
        0,      ; PointerToSymbolTable
        0,      ; NumberOfSymbols
        0E0h,   ; SizeOfOptionalHeader
        010Fh   ; Characteristics
    }
    
    ; Optional Header
    opt_header IMAGE_OPTIONAL_HEADER {
        010Bh,  ; Magic (PE32)
        0, 0,   ; Linker version
        0,      ; SizeOfCode
        0,      ; SizeOfInitializedData
        0,      ; SizeOfUninitializedData
        0,      ; AddressOfEntryPoint
        0,      ; BaseOfCode
        0,      ; BaseOfData
        400000h,; ImageBase
        1000h,  ; SectionAlignment
        200h,   ; FileAlignment
        4, 0,   ; OS Version
        0, 0,   ; Image Version
        4, 0,   ; Subsystem Version
        0,      ; Win32VersionValue
        0,      ; SizeOfImage
        0,      ; SizeOfHeaders
        0,      ; CheckSum
        IMAGE_SUBSYSTEM_WINDOWS_CUI,
        0,      ; DllCharacteristics
        100000h,; SizeOfStackReserve
        1000h,  ; SizeOfStackCommit
        100000h,; SizeOfHeapReserve
        1000h,  ; SizeOfHeapCommit
        0,      ; LoaderFlags
        16,     ; NumberOfRvaAndSizes
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    }
    
    ; Text Section
    text_section IMAGE_SECTION_HEADER {
        '.text',0,0,0,
        0,      ; VirtualSize
        1000h,  ; VirtualAddress
        0,      ; SizeOfRawData
        200h,   ; PointerToRawData
        0,0,0,0,
        IMAGE_SCN_CNT_CODE or IMAGE_SCN_MEM_EXECUTE or IMAGE_SCN_MEM_READ
    }
endm