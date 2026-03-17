; ===============================================================================
; RawrXD PE32+ Writer — Complete Structures & Constants
; ===============================================================================
; Purpose: x64 ml64 PE32+ binary writer for Amphibious CLI/GUI generation
; Eliminates link.exe dependency, enables byte-reproducible builds, creates
; patentable toolchain layer for $32M defense artifact.
; 
; Handles: DOS header, NT headers, section headers, import table, relocations
; Target: x64 PE32+, Win32 subsystem (CONSOLE/WINDOWS)
; ===============================================================================

; DOS Header Structure (64 bytes static, rest stub)
IMAGE_DOS_HEADER struct
    e_magic         dw 0x5A4D  ; "MZ" signature (0x4D5A in little-endian)
    e_cblp          dw 0       ; Bytes on last page
    e_cp            dw 0       ; Pages in file
    e_cr            dw 0       ; Relocations
    e_crlc          dw 0       ; Size of header
    e_minalloc      dw 0       ; Min extra paragraphs
    e_maxalloc      dw 0       ; Max extra paragraphs
    e_ss            dw 0       ; Initial SS
    e_sp            dw 0       ; Initial SP
    e_csum          dw 0       ; Checksum
    e_ip            dw 0       ; Initial IP
    e_cs            dw 0       ; Initial CS
    e_lfarlc        dw 0       ; File address of relocation table
    e_ovno          dw 0       ; Overlay number
    e_res           dq 0, 0, 0, 0  ; Reserved words
    e_oemid         dw 0       ; OEM identifier
    e_oeminfo       dw 0       ; OEM information
    e_res2          dq 0, 0, 0  ; Reserved words
    e_lfanew        dd 0       ; File address of PE header (offset to "PE\0\0")
IMAGE_DOS_HEADER ends

; NT File Header (20 bytes fixed)
IMAGE_FILE_HEADER struct
    Machine         dw 0       ; Intel x64 = 0x8664
    NumberOfSections dw 0      ; Number of sections (.text, .data, .reloc, .idata)
    TimeDateStamp   dd 0       ; Seconds since 1970 (UTC, for reproducibility)
    PointerToSymbolTable dd 0  ; COFF symbol table (must be 0 for modern PE)
    NumberOfSymbols dd 0       ; COFF symbol count (must be 0 for modern PE)
    SizeOfOptionalHeader dw 0  ; Size of optional header = 240 for PE32+
    Characteristics dw 0       ; File characteristics (flags)
IMAGE_FILE_HEADER ends

; Optional Header — PE32+ (240 bytes for x64)
IMAGE_OPTIONAL_HEADER32PLUS struct
    Magic           dw 0       ; 0x20B for PE32+
    MajorLinkerVersion db 0    ; Linker major version
    MinorLinkerVersion db 0    ; Linker minor version
    SizeOfCode      dd 0       ; Size of .text section
    SizeOfInitializedData dd 0 ; Size of initialized data (.data)
    SizeOfUninitializedData dd 0 ; Size of uninitialized data (.bss)
    AddressOfEntryPoint dd 0   ; RVA of entry point (Main or GuiMain)
    BaseOfCode      dd 0       ; RVA of .text section
    BASE_OF_DATA_UNUSED dd 0   ; Reserved field (not used in PE32+)
    
    ; Optional fields specific to PE32+
    ImageBase       dq 0       ; Base address = 0x140000000 (typical)
    SectionAlignment dd 0      ; Section alignment = 0x1000 (4KB)
    FileAlignment   dd 0       ; File alignment = 0x200 (512 bytes)
    MajorOperatingSystemVersion dw 0  ; OS version = 6 (Windows Vista+)
    MinorOperatingSystemVersion dw 0
    MajorImageVersion dw 0     ; Image version = 0
    MinorImageVersion dw 0
    MajorSubsystemVersion dw 6 ; Subsystem version = 6
    MinorSubsystemVersion dw 0
    Win32VersionValue dd 0     ; Reserved
    SizeOfImage     dd 0       ; Total image size (rounded to section alignment)
    SizeOfHeaders   dd 0       ; Size of headers (DOS + NT + sections)
    CheckSum        dd 0       ; Checksum (optional, can be 0)
    Subsystem       dw 0       ; 3 = WINDOWS GUI, 2 = WINDOWS CONSOLE
    DllCharacteristics dw 0    ; DLL characteristics
    StackReserveSize dq 0      ; Stack reserve = 0x100000 (1MB)
    StackCommitSize dq 0       ; Stack commit = 0x1000 (4KB)
    HeapReserveSize dq 0       ; Heap reserve = 0x100000 (1MB)
    HeapCommitSize  dq 0       ; Heap commit = 0x1000 (4KB)
    LoaderFlags     dd 0       ; Reserved
    NumberOfRvaAndSizes dd 16  ; Data directory count (always 16)
    
    ; Data Directories (16 entries × 8 bytes)
    ExportTableRVA  dd 0       ; Export table RVA (0 if not exported)
    ExportTableSize dd 0
    ImportTableRVA  dd 0       ; Import table RVA (points to .idata)
    ImportTableSize dd 0
    ResourceTableRVA dd 0      ; Resource table RVA
    ResourceTableSize dd 0
    ExceptionTableRVA dd 0     ; Exception table RVA
    ExceptionTableSize dd 0
    CertificateTableRVA dd 0   ; Certificate RVA
    CertificateTableSize dd 0
    BaseRelocationTableRVA dd 0 ; Base relocations (.reloc) RVA
    BaseRelocationTableSize dd 0
    DebugRVA        dd 0       ; Debug table RVA
    DebugSize       dd 0
    ArchitectureRVA dd 0       ; Architecture-specific RVA
    ArchitectureSize dd 0
    GlobalPtrRVA    dd 0       ; Global pointer RVA
    GlobalPtrSize   dd 0
    TlsTableRVA     dd 0       ; TLS table RVA
    TlsTableSize    dd 0
    LoadConfigTableRVA dd 0    ; Load config table RVA
    LoadConfigTableSize dd 0
    BoundImportRVA  dd 0       ; Bound import RVA
    BoundImportSize dd 0
    IatRVA          dd 0       ; Import address table RVA
    IatSize         dd 0
    DelayImportDescRVA dd 0    ; Delay import RVA
    DelayImportDescSize dd 0
    ComPlusRuntimeHeaderRVA dd 0 ; COM+ runtime header RVA
    ComPlusRuntimeHeaderSize dd 0
    ReservedRVA     dd 0       ; Reserved
    ReservedSize    dd 0
IMAGE_OPTIONAL_HEADER32PLUS ends

; Section Header (40 bytes per section)
IMAGE_SECTION_HEADER struct
    Name            db 8 dup(0)  ; Section name (padded with zeros)
    VirtualSize     dd 0         ; Size in memory (rounded up)
    VirtualAddress  dd 0         ; RVA of section
    SizeOfRawData   dd 0         ; Size in file (rounded to file alignment)
    PointerToRawData dd 0        ; File offset of section data
    PointerToRelocations dd 0    ; Relocation table offset (0 for PE executable)
    PointerToLinenumbers dd 0    ; Line numbers offset (0)
    NumberOfRelocations dw 0     ; Relocation count (0)
    NumberOfLinenumbers dw 0     ; Line number count (0)
    Characteristics dd 0         ; Section flags
IMAGE_SECTION_HEADER ends

; Import Directory Entry (20 bytes per imported DLL)
IMAGE_IMPORT_DESCRIPTOR struct
    ImportLookupTableRVA dd 0   ; RVA of Import Lookup Table (ILT)
    TimeDateStamp   dd 0        ; Time/date stamp (0 for bound imports)
    ForwarderChain  dd 0        ; Forwarding chain index (-1 / 0xFFFFFFFF)
    NameRVA         dd 0        ; RVA of DLL name string (null-terminated)
    ImportAddressTableRVA dd 0  ; RVA of Import Address Table (IAT)
IMAGE_IMPORT_DESCRIPTOR ends

; Import Lookup Table Entry (8 bytes for PE32+, highest bit = 1 for name lookup)
; Format: Bit 63 = 1 (name lookup), Bits 0-62 = RVA of hint/name pair
; Alternative: Bit 63 = 0 (ordinal lookup), Bits 0-30 = ordinal number

; Hint/Name Pair (variable size, null-terminated function name)
IMPORT_BY_NAME struct
    Hint            dw 0        ; Ordinal hint (function index)
    Name            db 1 dup(0) ; Function name (null-terminated string follows)
IMPORT_BY_NAME ends

; Base Relocation Block (8 bytes header + variable relocation entries)
BASE_RELOCATION_BLOCK struct
    PageRVA         dd 0        ; RVA of page with relocations
    BlockSize       dd 0        ; Size of this block (including header)
    ; Followed by relocation entries (2 bytes each):
    ; Bits 15-12 = relocation type (3 = DIR64)
    ; Bits 11-0 = offset within page
BASE_RELOCATION_BLOCK ends

; ===============================================================================
; PE32+ Writing Constants
; ===============================================================================

; Machine types
MACHINE_I386        equ 0x014C  ; 32-bit Intel
MACHINE_AMD64       equ 0x8664  ; 64-bit AMD/Intel

; File characteristics
FILE_CHARACTERISTICS_RELOC_STRIPPED equ 0x0001  ; No base relocations
FILE_CHARACTERISTICS_EXECUTABLE_IMAGE equ 0x0002  ; Executable
FILE_CHARACTERISTICS_LINE_NUMS_STRIPPED equ 0x0004
FILE_CHARACTERISTICS_LOCAL_SYMS_STRIPPED equ 0x0008
FILE_CHARACTERISTICS_AGGRESSIVE_WS_TRIM equ 0x0010
FILE_CHARACTERISTICS_LARGE_ADDRESS equ 0x0020
FILE_CHARACTERISTICS_RESERVED equ 0x0040
FILE_CHARACTERISTICS_REVERSE_LO equ 0x0080
FILE_CHARACTERISTICS_32BIT_MACHINE equ 0x0100
FILE_CHARACTERISTICS_DEBUG_STRIPPED equ 0x0200
FILE_CHARACTERISTICS_REMOVABLE equ 0x0400
FILE_CHARACTERISTICS_NET equ 0x0800
FILE_CHARACTERISTICS_SYSTEM equ 0x1000
FILE_CHARACTERISTICS_DLL equ 0x2000
FILE_CHARACTERISTICS_UP equ 0x4000
FILE_CHARACTERISTICS_REVERSE_HI equ 0x8000

; Subsystem types
SUBSYSTEM_UNKNOWN   equ 0
SUBSYSTEM_NATIVE    equ 1
SUBSYSTEM_WINDOWS_GUI equ 3
SUBSYSTEM_WINDOWS_CUI equ 2

; Section characteristics
SECTION_SCN_CODE    equ 0x00000020  ; Executable code
SECTION_SCN_INITIALIZED_DATA equ 0x00000040  ; Initialized data (read-only by default)
SECTION_SCN_UNINITIALIZED_DATA equ 0x00000080  ; Uninitialized data (BSS)
SECTION_SCN_DISCARDABLE equ 0x02000000  ; Can be discarded
SECTION_SCN_SHARED  equ 0x10000000  ; Shareable
SECTION_SCN_EXECUTE equ 0x20000000  ; Executable
SECTION_SCN_READ    equ 0x40000000  ; Readable
SECTION_SCN_WRITE   equ 0x80000000  ; Writable

; DLL characteristics
DLL_CHAR_ASLR       equ 0x0040  ; ASLR
DLL_CHAR_DEP        equ 0x0100  ; Data Execution Prevention
DLL_CHAR_NX_COMPAT  equ 0x0100  ; NX compatible

; Relocation types (for BASE_RELOCATION entries)
RELOC_ABSOLUTE      equ 0  ; Skip (placeholder)
RELOC_HIGH          equ 1  ; 16-bit high address
RELOC_LOW           equ 2  ; 16-bit low address
RELOC_HIGHLOW       equ 3  ; 32-bit address
RELOC_HIGHADJ       equ 4  ; Adjusted high 16-bit (x86 only)
RELOC_MIPS_JMPADDR  equ 5  ; MIPS jump address
RELOC_MIPS_RELHI    equ 6  ; MIPS relative high 16
RELOC_MIPS_RELLO    equ 7  ; MIPS relative low 16
RELOC_MIPS_GPREL    equ 8  ; MIPS global pointer relative
RELOC_ARM_MOV32     equ 5  ; ARM mov32 + reloc
RELOC_THUMB_MOV32   equ 7  ; Thumb mov32 + reloc
RELOC_ARM_BRANCH24  equ 3  ; ARM branch24
RELOC_THUMB_BRANCH20 equ 2  ; Thumb branch20
RELOC_DIR64         equ 10 ; x64 direct 64-bit address (most common)

; Import table magic
IMAGE_ORDINAL_FLAG  equ 0x8000000000000000  ; High bit set for name import
IMPORT_NULL_TERMINATOR equ 0x00  ; End of ILT/IAT arrays

; PE32+ magic constants
PE_SIGNATURE        equ 0x4550  ; "PE"
PE32PLUS_MAGIC      equ 0x020B  ; PE32+ magic number
DOS_STUB_SIZE       equ 0x40    ; Minimal DOS header (64 bytes)
PE_SIGNATURE_SIZE   equ 4       ; "PE\0\0"
PE_HEADER_ALIGNMENT equ 0x1000  ; 4KB section alignment
PE_FILE_ALIGNMENT   equ 0x0200  ; 512-byte file alignment
PE_IMAGE_BASE_X64   equ 0x140000000  ; Default x64 image base

; Default subsystem
DEFAULT_SUBSYSTEM_CLI equ SUBSYSTEM_WINDOWS_CUI
DEFAULT_SUBSYSTEM_GUI equ SUBSYSTEM_WINDOWS_GUI

; Standard DLL names (kernel32.dll, user32.dll, msvcrt.dll)
DLL_KERNEL32        db "kernel32.dll", 0
DLL_USER32          db "user32.dll", 0
DLL_MSVCRT          db "msvcrt.dll", 0

; Standard kernel32 imports
FUNC_KERNEL32_ExitProcess        db "ExitProcess", 0
FUNC_KERNEL32_CreateWindowExA    db "CreateWindowExA", 0
FUNC_KERNEL32_GetMessageA        db "GetMessageA", 0
FUNC_KERNEL32_DispatchMessageA   db "DispatchMessageA", 0
FUNC_KERNEL32_SendMessageA       db "SendMessageA", 0
FUNC_KERNEL32_SetTimer           db "SetTimer", 0
FUNC_KERNEL32_GetModuleHandleA   db "GetModuleHandleA", 0
FUNC_KERNEL32_LoadLibraryA       db "LoadLibraryA", 0
FUNC_KERNEL32_GetProcAddress     db "GetProcAddress", 0

; Standard user32 imports
FUNC_USER32_CreateWindowExA      db "CreateWindowExA", 0
FUNC_USER32_DefWindowProcA       db "DefWindowProcA", 0
FUNC_USER32_RegisterClassA       db "RegisterClassA", 0
FUNC_USER32_MessageBoxA          db "MessageBoxA", 0

; Standard msvcrt imports
FUNC_MSVCRT_printf               db "printf", 0
FUNC_MSVCRT_assert               db "assert", 0

; ===============================================================================
; Utility Macros
; ===============================================================================

; Calculate aligned size (round up to nearest alignment boundary)
; Usage: AlignUp value, alignment  -> result in rax
MACRO_ALIGN_UP MACRO value, alignment
    local aligned_value
    aligned_value = value
    if (aligned_value mod alignment) <> 0
        aligned_value = aligned_value + (alignment - (aligned_value mod alignment))
    endif
endm

; Section name padding helper (fill 8 bytes: name + null padding)
; Usage: write section name to buffer, pad to 8 bytes
; Example: .text = 0x7478742E (little-endian)

; Export structures for use by PE Writer Core
; (These will be populated by PE_Writer_Core)

extern RawrXD_PE_WriteDOSHeader_ml64:near
extern RawrXD_PE_WriteNTHeaders_ml64:near
extern RawrXD_PE_WriteSectionHeaders_ml64:near
extern RawrXD_PE_GenerateImportTable_ml64:near
extern RawrXD_PE_GenerateRelocations_ml64:near
extern RawrXD_PE_AssembleBinary_ml64:near
extern RawrXD_PE_ValidateReproduce_ml64:near

; ===============================================================================
end
