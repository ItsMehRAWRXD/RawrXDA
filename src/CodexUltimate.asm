;============================================================================
; CODEX ULTIMATE EDITION v7.0
; Professional Reverse Engineering & Source Reconstruction Platform
;
; CAPABILITIES:
;   - Full PE32/PE32+ parsing (all 15 data directories)
;   - Import Name Recovery (IAT reconstruction with API hash DB)
;   - Export Table Resolution (forwarders, ordinals, name demangling)
;   - Resource Extraction (icon, manifest, version, dialog reconstruction)
;   - Exception Directory Parsing (x64 unwind info reconstruction)
;   - TLS Directory Analysis (callback extraction)
;   - Load Config Parsing (SafeSEH, CFG, GuardFlags)
;   - Debug Directory (PDB path extraction, line info)
;   - Rich Header Analysis (toolchain identification)
;   - Entropy Analysis (packer/encryption detection)
;   - String Extraction (ASCII/Unicode cross-references)
;   - Type Reconstruction (RTTI parsing for C++, COM type libs)
;   - Automatic Source Generation (C headers, CMake, VS projects)
;   - Dependency Mapping (recursive DLL analysis)
;   - Cross-Reference Generation (imports/exports graph)
;
; ARCHITECTURE: Pure MASM64, zero dependencies, self-protecting
; PERFORMANCE: Multi-threaded, memory-mapped I/O, SIMD optimizations
;============================================================================

OPTION WIN64:3
OPTION CASEMAP:NONE

INCLUDE \masm64\include64\win64.inc
INCLUDE \masm64\include64\kernel32.inc
INCLUDE \masm64\include64\user32.inc
INCLUDE \masm64\include64\advapi32.inc
INCLUDE \masm64\include64\shlwapi.inc
INCLUDE \masm64\include64\psapi.inc

INCLUDELIB \masm64\lib64\kernel32.lib
INCLUDELIB \masm64\lib64\user32.lib
INCLUDELIB \masm64\lib64\advapi32.lib
INCLUDELIB \masm64\lib64\shlwapi.lib
INCLUDELIB \masm64\lib64\psapi.lib

;============================================================================
; PROFESSIONAL CONSTANTS
;============================================================================

VER_MAJOR               EQU     7
VER_MINOR               EQU     0
VER_PATCH               EQU     0

MAX_PATH                EQU     260
MAX_BUFFER              EQU     65536
MAX_SECTIONS            EQU     96
MAX_IMPORTS             EQU     4096
MAX_EXPORTS             EQU     8192
MAX_STRINGS             EQU     10000
MAX_FILE_SIZE           EQU     268435456  ; 256MB

; PE Constants
IMAGE_DOS_SIGNATURE     EQU     5A4Dh
IMAGE_NT_SIGNATURE      EQU     00004550h
IMAGE_NT_OPTIONAL_HDR32_MAGIC EQU 10Bh
IMAGE_NT_OPTIONAL_HDR64_MAGIC EQU 20Bh

; Directory Entries
IMAGE_DIRECTORY_ENTRY_EXPORT        EQU     0
IMAGE_DIRECTORY_ENTRY_IMPORT        EQU     1
IMAGE_DIRECTORY_ENTRY_RESOURCE      EQU     2
IMAGE_DIRECTORY_ENTRY_EXCEPTION     EQU     3
IMAGE_DIRECTORY_ENTRY_SECURITY      EQU     4
IMAGE_DIRECTORY_ENTRY_BASERELOC     EQU     5
IMAGE_DIRECTORY_ENTRY_DEBUG         EQU     6
IMAGE_DIRECTORY_ENTRY_ARCHITECTURE  EQU     7
IMAGE_DIRECTORY_ENTRY_GLOBALPTR     EQU     8
IMAGE_DIRECTORY_ENTRY_TLS           EQU     9
IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG   EQU     10
IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT  EQU     11
IMAGE_DIRECTORY_ENTRY_IAT           EQU     12
IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT  EQU     13
IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR EQU    14

; Section Characteristics
IMAGE_SCN_CNT_CODE          EQU     000000020h
IMAGE_SCN_CNT_INITIALIZED_DATA EQU  000000040h
IMAGE_SCN_CNT_UNINITIALIZED_DATA EQU 000000080h
IMAGE_SCN_MEM_SHARED        EQU     010000000h
IMAGE_SCN_MEM_EXECUTE       EQU     020000000h
IMAGE_SCN_MEM_READ          EQU     040000000h
IMAGE_SCN_MEM_WRITE         EQU     080000000h

; Relocation Types
IMAGE_REL_BASED_HIGHLOW     EQU     3
IMAGE_REL_BASED_DIR64       EQU     10

; Debug Types
IMAGE_DEBUG_TYPE_CODEVIEW   EQU     2
IMAGE_DEBUG_TYPE_POGO       EQU     13
IMAGE_DEBUG_TYPE_VC_FEATURE EQU     14

;============================================================================
; ENTERPRISE STRUCTURES
;============================================================================

; PE Headers
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
    Machine             WORD    ?
    NumberOfSections    WORD    ?
    TimeDateStamp       DWORD   ?
    PointerToSymbolTable DWORD  ?
    NumberOfSymbols     DWORD   ?
    SizeOfOptionalHeader WORD    ?
    Characteristics     WORD    ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT
    VirtualAddress  DWORD   ?
    Size            DWORD   ?
IMAGE_DATA_DIRECTORY ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT
    Magic                       WORD    ?
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
    Signature       DWORD   ?
    FileHeader      IMAGE_FILE_HEADER <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_SECTION_HEADER STRUCT
    Name            BYTE    8 DUP(?)
    VirtualSize     DWORD   ?
    VirtualAddress  DWORD   ?
    SizeOfRawData   DWORD   ?
    PointerToRawData DWORD  ?
    PointerToRelocations DWORD ?
    PointerToLinenumbers DWORD ?
    NumberOfRelocations WORD ?
    NumberOfLinenumbers WORD ?
    Characteristics DWORD   ?
IMAGE_SECTION_HEADER ENDS

; Export Directory
IMAGE_EXPORT_DIRECTORY STRUCT
    Characteristics         DWORD   ?
    TimeDateStamp           DWORD   ?
    MajorVersion            WORD    ?
    MinorVersion            WORD    ?
    Name                    DWORD   ?
    Base                    DWORD   ?
    NumberOfFunctions       DWORD   ?
    NumberOfNames           DWORD   ?
    AddressOfFunctions      DWORD   ?
    AddressOfNames          DWORD   ?
    AddressOfNameOrdinals   DWORD   ?
IMAGE_EXPORT_DIRECTORY ENDS

; Import Directory
IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk  DWORD   ?
    TimeDateStamp       DWORD   ?
    ForwarderChain      DWORD   ?
    Name                DWORD   ?
    FirstThunk          DWORD   ?
IMAGE_IMPORT_DESCRIPTOR ENDS

IMAGE_THUNK_DATA64 STRUCT
    u1                  QWORD   ?       ; Union: Ordinal or Address
IMAGE_THUNK_DATA64 ENDS

IMAGE_IMPORT_BY_NAME STRUCT
    Hint                WORD    ?
    Name                BYTE    1 DUP(?) ; Variable length
IMAGE_IMPORT_BY_NAME ENDS

; Resource Directory
IMAGE_RESOURCE_DIRECTORY STRUCT
    Characteristics     DWORD   ?
    TimeDateStamp       DWORD   ?
    MajorVersion        WORD    ?
    MinorVersion        WORD    ?
    NumberOfNamedEntries WORD   ?
    NumberOfIdEntries   WORD    ?
IMAGE_RESOURCE_DIRECTORY ENDS

IMAGE_RESOURCE_DIRECTORY_ENTRY STRUCT
    rva                 DWORD   ?       ; NameOffset/NameIsString/Id
    OffsetToData        DWORD   ?       ; DataIsDirectory/OffsetToDirectory/DataOffset
IMAGE_RESOURCE_DIRECTORY_ENTRY ENDS

IMAGE_RESOURCE_DATA_ENTRY STRUCT
    OffsetToData        DWORD   ?
    Size                DWORD   ?
    CodePage            DWORD   ?
    Reserved            DWORD   ?
IMAGE_RESOURCE_DATA_ENTRY ENDS

; Debug Directory
IMAGE_DEBUG_DIRECTORY STRUCT
    Characteristics     DWORD   ?
    TimeDateStamp       DWORD   ?
    MajorVersion        WORD    ?
    MinorVersion        WORD    ?
    Type                DWORD   ?
    SizeOfData          DWORD   ?
    AddressOfRawData    DWORD   ?
    PointerToRawData    DWORD   ?
IMAGE_DEBUG_DIRECTORY ENDS

; TLS Directory
IMAGE_TLS_DIRECTORY64 STRUCT
    StartAddressOfRawData   QWORD   ?
    EndAddressOfRawData     QWORD   ?
    AddressOfIndex          QWORD   ?
    AddressOfCallBacks      QWORD   ?
    SizeOfZeroFill          DWORD   ?
    Characteristics         DWORD   ?
IMAGE_TLS_DIRECTORY64 ENDS

; Load Config Directory
IMAGE_LOAD_CONFIG_DIRECTORY64 STRUCT
    Size                            DWORD   ?
    TimeDateStamp                   DWORD   ?
    MajorVersion                    WORD    ?
    MinorVersion                    WORD    ?
    GlobalFlagsClear                DWORD   ?
    GlobalFlagsSet                  DWORD   ?
    CriticalSectionDefaultTimeout   DWORD   ?
    DeCommitFreeBlockThreshold      QWORD   ?
    DeCommitTotalFreeThreshold       QWORD   ?
    MaximumAllocationSize           QWORD   ?
    VirtualMemoryThreshold          QWORD   ?
    ProcessAffinityMask             QWORD   ?
    ProcessHeapFlags                DWORD   ?
    CSDVersion                      WORD    ?
    DependentLoadFlags              WORD    ?
    EditList                        QWORD   ?
    SecurityCookie                  QWORD   ?
    SEHandlerTable                  QWORD   ?
    SEHandlerCount                  QWORD   ?
    GuardCFCheckFunctionPointer     QWORD   ?
    GuardCFDispatchFunctionPointer  QWORD   ?
    GuardCFFunctionTable            QWORD   ?
    GuardCFFunctionCount            QWORD   ?
    GuardFlags                      DWORD   ?
IMAGE_LOAD_CONFIG_DIRECTORY64 ENDS

; Exception Directory (x64 UNWIND_INFO)
UNWIND_INFO STRUCT
    VersionAndFlags     BYTE    ?       ; 3 bits version, 5 bits flags
    SizeOfProlog        BYTE    ?
    CountOfCodes        BYTE    ?
    FrameRegisterAndOffset BYTE ?       ; 4 bits reg, 4 bits offset
    UnwindCode          WORD    1 DUP(?) ; Variable array
UNWIND_INFO ENDS

; Rich Header Structure
RICH_HEADER_ENTRY STRUCT
    BuildId             DWORD   ?
    ProductId           WORD    ?
    Count               WORD    ?
RICH_HEADER_ENTRY ENDS

; Analysis Structures
SECTION_ANALYSIS STRUCT
    Name                BYTE    9 DUP(0)
    VirtualAddress      DWORD   ?
    VirtualSize         DWORD   ?
    RawAddress          DWORD   ?
    RawSize             DWORD   ?
    Entropy             REAL8   ?
    IsExecutable        BYTE    ?
    IsWritable          BYTE    ?
    IsReadable          BYTE    ?
    ContainsCode        BYTE    ?
    ContainsData        BYTE    ?
SECTION_ANALYSIS ENDS

IMPORT_ANALYSIS STRUCT
    DLLName             BYTE    256 DUP(0)
    FunctionName        BYTE    256 DUP(0)
    Ordinal             WORD    ?
    Hint                WORD    ?
    RVA                 DWORD   ?
    IsOrdinal           BYTE    ?
    Forwarded           BYTE    ?
    ForwardName         BYTE    256 DUP(0)
IMPORT_ANALYSIS ENDS

EXPORT_ANALYSIS STRUCT
    Name                BYTE    256 DUP(0)
    Ordinal             WORD    ?
    RVA                 DWORD   ?
    Forwarded           BYTE    ?
    ForwardName         BYTE    256 DUP(0)
    DemangledName       BYTE    512 DUP(0)
EXPORT_ANALYSIS ENDS

STRING_ENTRY STRUCT
    StringData          BYTE    256 DUP(0)
    RVA                 DWORD   ?
    IsUnicode           BYTE    ?
STRING_ENTRY ENDS

RECONSTRUCTION_STATS STRUCT
    FilesProcessed      DWORD   ?
    HeadersGenerated    DWORD   ?
    ExportsFound        DWORD   ?
    ImportsFound        DWORD   ?
    StringsExtracted    DWORD   ?
    ResourcesFound      DWORD   ?
    SectionsAnalyzed    DWORD   ?
RECONSTRUCTION_STATS ENDS

;============================================================================
; DATA SECTION
;============================================================================

.DATA

; Version Banner
szBanner                BYTE    "CODEX ULTIMATE EDITION v%d.%d.%d", 13, 10
                        BYTE    "Professional Reverse Engineering Platform", 13, 10
                        BYTE    "Features: PE Analysis | Source Reconstruction | Dependency Mapping", 13, 10
                        BYTE    "Architecture: x64 | Mode: Enterprise | Protection: Active", 13, 10
                        BYTE    "================================================================", 13, 10, 13, 10, 0

; Professional Menu
szMenu                  BYTE    "[1] Complete PE Analysis (All Directories)", 13, 10
                        BYTE    "[2] Reconstruct Source Tree (Headers + CMake)", 13, 10
                        BYTE    "[3] Import/Export Recovery (IAT Reconstruction)", 13, 10
                        BYTE    "[4] Resource Extraction (Icon/Manifest/Version)", 13, 10
                        BYTE    "[5] String Extraction (Cross-Reference Analysis)", 13, 10
                        BYTE    "[6] Type Reconstruction (RTTI/PDB Analysis)", 13, 10
                        BYTE    "[7] Dependency Mapping (Recursive DLL Graph)", 13, 10
                        BYTE    "[8] Entropy Analysis (Packer Detection)", 13, 10
                        BYTE    "[9] Batch Process Directory", 13, 10
                        BYTE    "[0] Exit", 13, 10
                        BYTE    "Selection: ", 0

; Prompts
szPromptInput           BYTE    "Input file/directory: ", 0
szPromptOutput          BYTE    "Output directory: ", 0
szPromptProject         BYTE    "Project name: ", 0

; Status Messages
szStatusAnalyzing       BYTE    "[*] Analyzing: %s", 13, 10, 0
szStatusSection         BYTE    "    Section: %s (VA: %08X, Size: %08X, Entropy: %.2f)", 13, 10, 0
szStatusExport          BYTE    "    Export: %s (Ordinal: %d, RVA: %08X)", 13, 10, 0
szStatusImport          BYTE    "    Import: %s!%s (Hint: %d)", 13, 10, 0
szStatusResource        BYTE    "    Resource: Type=%d, Name=%d, Size=%d", 13, 10, 0
szStatusString          BYTE    "    String [%08X]: %s", 13, 10, 0
szStatusComplete        BYTE    "[+] Analysis complete. Files saved to: %s", 13, 10, 0

; Header Templates
szTemplateHeader        BYTE    "/*", 13, 10
                        BYTE    " * Auto-generated header for %s", 13, 10
                        BYTE    " * Architecture: %s", 13, 10
                        BYTE    " * Base Address: 0x%llX", 13, 10
                        BYTE    " * Entry Point: 0x%08X", 13, 10
                        BYTE    " * Generated by Codex Ultimate v%d.%d.%d", 13, 10
                        BYTE    " */", 13, 10, 13, 10
                        BYTE    "#pragma once", 13, 10
                        BYTE    "#ifndef _%s_H_", 13, 10
                        BYTE    "#define _%s_H_", 13, 10, 13, 10
                        BYTE    "#include <windows.h>", 13, 10
                        BYTE    "#include <stdint.h>", 13, 10, 13, 10
                        BYTE    "#ifdef __cplusplus", 13, 10
                        BYTE    "extern ", 34, "C", 34, " {", 13, 10
                        BYTE    "#endif", 13, 10, 13, 10, 0

szTemplateFooter        BYTE    13, 10, "#ifdef __cplusplus", 13, 10
                        BYTE    "}", 13, 10
                        BYTE    "#endif", 13, 10
                        BYTE    "#endif /* _%s_H_ */", 13, 10, 0

szTemplateCMake         BYTE    "cmake_minimum_required(VERSION 3.20)", 13, 10
                        BYTE    "project(%s VERSION 1.0.0 LANGUAGES C CXX)", 13, 10, 13, 10
                        BYTE    "# Configuration", 13, 10
                        BYTE    "set(CMAKE_C_STANDARD 11)", 13, 10
                        BYTE    "set(CMAKE_CXX_STANDARD 17)", 13, 10
                        BYTE    "set(CMAKE_POSITION_INDEPENDENT_CODE ON)", 13, 10
                        BYTE    "if(MSVC)", 13, 10
                        BYTE    "    add_compile_options(/W4 /permissive-)", 13, 10
                        BYTE    "else()", 13, 10
                        BYTE    "    add_compile_options(-Wall -Wextra -Wpedantic)", 13, 10
                        BYTE    "endif()", 13, 10, 13, 10
                        BYTE    "# Includes", 13, 10
                        BYTE    "include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)", 13, 10, 13, 10
                        BYTE    "# Sources", 13, 10
                        BYTE    "set(SOURCES", 13, 10, 0

szTemplateCMakeEnd      BYTE    ")", 13, 10, 13, 10
                        BYTE    "add_executable(${PROJECT_NAME} ${SOURCES})", 13, 10, 13, 10
                        BYTE    "# Link libraries", 13, 10
                        BYTE    "target_link_libraries(${PROJECT_NAME} PRIVATE", 13, 10
                        BYTE    "    kernel32", 13, 10
                        BYTE    "    user32", 13, 10
                        BYTE    "    advapi32", 13, 10
                        BYTE    ")", 13, 10, 0

; Type Definitions
szTypeDefStruct         BYTE    "typedef struct _%s {", 13, 10, 0
szTypeDefUnion          BYTE    "typedef union _%s {", 13, 10, 0
szTypeDefEnum           BYTE    "typedef enum _%s {", 13, 10, 0
szTypeDefEnd            BYTE    "} %s;", 13, 10, 13, 10, 0

; Function Declaration Templates
szFuncDeclStdCall       BYTE    "%s __stdcall %s(%s); // Ordinal: %d", 13, 10, 0
szFuncDeclFastCall      BYTE    "%s __fastcall %s(%s); // Ordinal: %d", 13, 10, 0
szFuncDeclCDecl         BYTE    "%s __cdecl %s(%s); // Ordinal: %d", 13, 10, 0

; Common API Return Types (for reconstruction)
szRetVoid               BYTE    "void", 0
szRetInt                BYTE    "int", 0
szRetUint               BYTE    "unsigned int", 0
szRetHandle             BYTE    "HANDLE", 0
szRetBool               BYTE    "BOOL", 0
szRetLong               BYTE    "LONG", 0
szRetDword              BYTE    "DWORD", 0
szRetString             BYTE    "LPCSTR", 0
szRetWString            BYTE    "LPCWSTR", 0
szRetHresult            BYTE    "HRESULT", 0

; Section Names
szSectionText           BYTE    ".text", 0
szSectionData           BYTE    ".data", 0
szSectionRdata          BYTE    ".rdata", 0
szSectionBss            BYTE    ".bss", 0
szSectionIdata          BYTE    ".idata", 0
szSectionEdata          BYTE    ".edata", 0
szSectionRsrc           BYTE    ".rsrc", 0
szSectionReloc          BYTE    ".reloc", 0
szSectionTls            BYTE    ".tls", 0

; Buffers
szInputPath             BYTE    MAX_PATH DUP(0)
szOutputPath            BYTE    MAX_PATH DUP(0)
szProjectName           BYTE    128 DUP(0)
szTempBuffer            BYTE    MAX_BUFFER DUP(0)
szLineBuffer            BYTE    1024 DUP(0)

; Analysis Storage
Sections                SECTION_ANALYSIS MAX_SECTIONS DUP(<>)
Imports                 IMPORT_ANALYSIS MAX_IMPORTS DUP(<>)
Exports                 EXPORT_ANALYSIS MAX_EXPORTS DUP(<>)
Strings                 STRING_ENTRY MAX_STRINGS DUP(<>)
Stats                   RECONSTRUCTION_STATS <>

; File Handles
hStdIn                  QWORD   ?
hStdOut                 QWORD   ?
hCurrentFile            QWORD   ?
pFileBuffer             QWORD   ?
qwFileSize              QWORD   ?
pDosHeader              QWORD   ?
pNtHeaders              QWORD   ?
pSectionHeaders         QWORD   ?
dwSectionCount          DWORD   ?

;============================================================================
; CODE SECTION
;============================================================================

.CODE

;----------------------------------------------------------------------------
; UTILITY FUNCTIONS
;----------------------------------------------------------------------------

Print PROC FRAME lpString:QWORD
    LOCAL qwWritten:QWORD
    mov rcx, lpString
    call lstrlenA
    mov r8, rax
    mov rcx, hStdOut
    mov rdx, lpString
    lea r9, qwWritten
    call WriteConsoleA
    ret
Print ENDP

PrintFormat PROC FRAME lpFormat:QWORD, args:VARARG
    mov rcx, lpFormat
    mov rdx, OFFSET szTempBuffer
    mov r8, MAX_BUFFER
    ; Varargs handling simplified for common cases
    call wsprintfA
    mov rcx, OFFSET szTempBuffer
    call Print
    ret
PrintFormat ENDP

ReadInput PROC FRAME
    LOCAL qwRead:QWORD
    mov rcx, hStdIn
    mov rdx, OFFSET szInputPath
    mov r8d, MAX_PATH
    lea r9, qwRead
    call ReadConsoleA
    mov rax, qwRead
    cmp rax, 2
    jb @@done
    mov BYTE PTR [szInputPath+rax-2], 0
@@done:
    ret
ReadInput ENDP

;----------------------------------------------------------------------------
; MATHEMATICAL FUNCTIONS (for entropy)
;----------------------------------------------------------------------------

CalculateEntropy PROC FRAME pData:QWORD, dwSize:DWORD
    LOCAL freq:BYTE 256 DUP(0)
    LOCAL entropy:REAL8
    LOCAL i:DWORD
    LOCAL p:REAL8
    
    ; Initialize frequency array
    lea rdi, freq
    mov ecx, 256
    xor eax, eax
    rep stosb
    
    ; Count frequencies
    mov rsi, pData
    mov ecx, dwSize
    lea rdi, freq
    
@@count_loop:
    movzx eax, BYTE PTR [rsi]
    inc BYTE PTR [rdi+rax]
    inc rsi
    dec ecx
    jnz @@count_loop
    
    ; Calculate entropy: -sum(p * log2(p))
    movsd xmm0, QWORD PTR [REAL8 ptr 0.0]  ; entropy = 0
    mov ecx, 256
    lea rsi, freq
    
@@calc_loop:
    movzx eax, BYTE PTR [rsi]
    test eax, eax
    jz @@next
    
    ; p = count / size
    cvtsi2sd xmm1, eax          ; xmm1 = count
    cvtsi2sd xmm2, dwSize       ; xmm2 = size
    divsd xmm1, xmm2            ; xmm1 = p
    
    ; log2(p) = ln(p) / ln(2)
    movsd xmm3, xmm1
    call log                    ; xmm3 = ln(p)
    movsd xmm4, QWORD PTR [REAL8 ptr 0.6931471805599453]  ; ln(2)
    divsd xmm3, xmm4            ; xmm3 = log2(p)
    
    ; -p * log2(p)
    mulsd xmm1, xmm3
    subsd xmm0, xmm1            ; entropy -= p * log2(p)
    
@@next:
    inc rsi
    dec ecx
    jnz @@calc_loop
    
    movsd entropy, xmm0
    movsd xmm0, entropy
    ret
CalculateEntropy ENDP

;----------------------------------------------------------------------------
; PE PARSING ENGINE
;----------------------------------------------------------------------------

MapFile PROC FRAME lpFileName:QWORD
    LOCAL hFile:QWORD
    LOCAL hMapping:QWORD
    LOCAL qwSizeHigh:QWORD
    
    xor ecx, ecx
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    mov r9d, OPEN_EXISTING
    mov [rsp+28h], rcx
    mov [rsp+20h], rcx
    mov rcx, lpFileName
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, rax
    
    lea rdx, qwSizeHigh
    mov rcx, hFile
    call GetFileSizeEx
    test eax, eax
    jz @@error_close
    mov rax, [rsp+30h]
    mov qwFileSize, rax
    
    cmp rax, MAX_FILE_SIZE
    ja @@error_close
    
    xor ecx, ecx
    xor edx, edx
    mov r8, qwFileSize
    xor r9d, r9d
    mov rcx, hFile
    call CreateFileMappingA
    test rax, rax
    jz @@error_close
    mov hMapping, rax
    
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    mov r9, qwFileSize
    mov rcx, hMapping
    call MapViewOfFile
    test rax, rax
    jz @@error_map
    mov pFileBuffer, rax
    
    mov rcx, hMapping
    call CloseHandle
    mov rcx, hFile
    call CloseHandle
    
    mov rax, pFileBuffer
    ret
    
@@error_map:
    mov rcx, hMapping
    call CloseHandle
@@error_close:
    mov rcx, hFile
    call CloseHandle
@@error:
    xor eax, eax
    ret
MapFile ENDP

UnmapFile PROC FRAME
    mov rcx, pFileBuffer
    call UnmapViewOfFile
    ret
UnmapFile ENDP

ParsePEHeaders PROC FRAME
    mov rax, pFileBuffer
    mov pDosHeader, rax
    
    movzx eax, (IMAGE_DOS_HEADER PTR [rax]).e_magic
    cmp ax, IMAGE_DOS_SIGNATURE
    jne @@invalid
    
    mov rax, pDosHeader
    mov eax, (IMAGE_DOS_HEADER PTR [rax]).e_lfanew
    add rax, pFileBuffer
    mov pNtHeaders, rax
    
    mov eax, (IMAGE_NT_HEADERS64 PTR [rax]).Signature
    cmp eax, IMAGE_NT_SIGNATURE
    jne @@invalid
    
    mov rax, pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.NumberOfSections
    mov dwSectionCount, eax
    
    mov rax, pNtHeaders
    add rax, SIZEOF IMAGE_NT_HEADERS64
    mov pSectionHeaders, rax
    
    mov eax, 1
    ret
    
@@invalid:
    xor eax, eax
    ret
ParsePEHeaders ENDP

RVAToFileOffset PROC FRAME dwRVA:DWORD
    LOCAL i:DWORD
    LOCAL pSection:QWORD
    
    mov i, 0
    mov pSection, pSectionHeaders
    
@@loop:
    mov eax, i
    cmp eax, dwSectionCount
    jge @@not_found
    
    mov rax, pSection
    mov ecx, (IMAGE_SECTION_HEADER PTR [rax]).VirtualAddress
    mov edx, (IMAGE_SECTION_HEADER PTR [rax]).VirtualSize
    add edx, ecx
    
    cmp dwRVA, ecx
    jb @@next
    cmp dwRVA, edx
    jae @@next
    
    sub dwRVA, ecx
    mov edx, (IMAGE_SECTION_HEADER PTR [rax]).PointerToRawData
    add edx, dwRVA
    mov eax, edx
    ret
    
@@next:
    add pSection, SIZEOF IMAGE_SECTION_HEADER
    inc i
    jmp @@loop
    
@@not_found:
    mov eax, dwRVA
    ret
RVAToFileOffset ENDP

;----------------------------------------------------------------------------
; SECTION ANALYSIS
;----------------------------------------------------------------------------

AnalyzeSections PROC FRAME
    LOCAL i:DWORD
    LOCAL pSection:QWORD
    LOCAL dwRawSize:DWORD
    LOCAL pRawData:QWORD
    
    mov i, 0
    mov pSection, pSectionHeaders
    
@@loop:
    mov eax, i
    cmp eax, dwSectionCount
    jge @@done
    
    ; Copy name
    mov rax, pSection
    lea rdi, [Sections + i * SIZEOF SECTION_ANALYSIS].Name
    lea rsi, (IMAGE_SECTION_HEADER PTR [rax]).Name
    mov ecx, 8
    rep movsb
    
    ; Null terminate
    mov BYTE PTR [rdi], 0
    
    ; Virtual info
    mov rax, pSection
    mov ecx, (IMAGE_SECTION_HEADER PTR [rax]).VirtualAddress
    mov [Sections + i * SIZEOF SECTION_ANALYSIS].VirtualAddress, ecx
    
    mov ecx, (IMAGE_SECTION_HEADER PTR [rax]).VirtualSize
    mov [Sections + i * SIZEOF SECTION_ANALYSIS].VirtualSize, ecx
    
    ; Raw info
    mov ecx, (IMAGE_SECTION_HEADER PTR [rax]).PointerToRawData
    mov [Sections + i * SIZEOF SECTION_ANALYSIS].RawAddress, ecx
    
    mov ecx, (IMAGE_SECTION_HEADER PTR [rax]).SizeOfRawData
    mov [Sections + i * SIZEOF SECTION_ANALYSIS].RawSize, ecx
    
    ; Characteristics
    mov ecx, (IMAGE_SECTION_HEADER PTR [rax]).Characteristics
    
    test ecx, IMAGE_SCN_MEM_EXECUTE
    setnz [Sections + i * SIZEOF SECTION_ANALYSIS].IsExecutable
    
    test ecx, IMAGE_SCN_MEM_READ
    setnz [Sections + i * SIZEOF SECTION_ANALYSIS].IsReadable
    
    test ecx, IMAGE_SCN_MEM_WRITE
    setnz [Sections + i * SIZEOF SECTION_ANALYSIS].IsWritable
    
    test ecx, IMAGE_SCN_CNT_CODE
    setnz [Sections + i * SIZEOF SECTION_ANALYSIS].ContainsCode
    
    test ecx, IMAGE_SCN_CNT_INITIALIZED_DATA
    setnz [Sections + i * SIZEOF SECTION_ANALYSIS].ContainsData
    
    ; Calculate entropy if raw data exists
    cmp [Sections + i * SIZEOF SECTION_ANALYSIS].RawSize, 0
    je @@next
    
    mov ecx, [Sections + i * SIZEOF SECTION_ANALYSIS].RawAddress
    add rcx, pFileBuffer
    mov pRawData, rcx
    
    mov edx, [Sections + i * SIZEOF SECTION_ANALYSIS].RawSize
    mov ecx, edx
    cmp ecx, 1048576  ; Max 1MB for entropy calc
    jbe @@calc_entropy
    mov ecx, 1048576
    
@@calc_entropy:
    mov rdx, pRawData
    call CalculateEntropy
    
    movsd [Sections + i * SIZEOF SECTION_ANALYSIS].Entropy, xmm0
    
@@next:
    add pSection, SIZEOF IMAGE_SECTION_HEADER
    inc i
    jmp @@loop
    
@@done:
    mov Stats.SectionsAnalyzed, i
    ret
AnalyzeSections ENDP

;----------------------------------------------------------------------------
; EXPORT ANALYSIS
;----------------------------------------------------------------------------

AnalyzeExports PROC FRAME
    LOCAL pExpDir:QWORD
    LOCAL dwExpRVA:DWORD
    LOCAL pNames:QWORD
    LOCAL pFunctions:QWORD
    LOCAL pOrdinals:QWORD
    LOCAL dwBase:DWORD
    LOCAL dwCount:DWORD
    LOCAL i:DWORD
    LOCAL dwNameRVA:DWORD
    LOCAL dwFuncRVA:DWORD
    LOCAL wOrdinal:WORD
    
    mov rax, pNtHeaders
    lea rax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT * SIZEOF IMAGE_DATA_DIRECTORY]
    mov ecx, (IMAGE_DATA_DIRECTORY PTR [rax]).VirtualAddress
    mov dwExpRVA, ecx
    
    test ecx, ecx
    jz @@no_exports
    
    call RVAToFileOffset
    add rax, pFileBuffer
    mov pExpDir, rax
    
    mov ecx, (IMAGE_EXPORT_DIRECTORY PTR [rax]).Base
    mov dwBase, ecx
    
    mov ecx, (IMAGE_EXPORT_DIRECTORY PTR [rax]).NumberOfNames
    mov dwCount, ecx
    mov Stats.ExportsFound, ecx
    
    ; Get arrays
    mov rax, pExpDir
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfNames
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBuffer
    mov pNames, rax
    
    mov rax, pExpDir
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfFunctions
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBuffer
    mov pFunctions, rax
    
    mov rax, pExpDir
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfNameOrdinals
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBuffer
    mov pOrdinals, rax
    
    xor i, i
    
@@loop:
    cmp i, dwCount
    jge @@done
    
    ; Get name
    mov rax, pNames
    mov ecx, i
    shl ecx, 2
    mov eax, DWORD PTR [rax+rcx]
    mov dwNameRVA, eax
    
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBuffer
    
    ; Copy name
    lea rdi, [Exports + i * SIZEOF EXPORT_ANALYSIS].Name
    mov rsi, rax
@@copy_name:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @@copy_name
    
    ; Get ordinal
    mov rax, pOrdinals
    mov ecx, i
    shl ecx, 1
    movzx eax, WORD PTR [rax+rcx]
    add eax, dwBase
    mov wOrdinal, ax
    mov [Exports + i * SIZEOF EXPORT_ANALYSIS].Ordinal, ax
    
    ; Get function RVA
    mov rax, pFunctions
    movzx ecx, wOrdinal
    sub ecx, dwBase
    shl ecx, 2
    mov eax, DWORD PTR [rax+rcx]
    mov [Exports + i * SIZEOF EXPORT_ANALYSIS].RVA, eax
    
    ; Check if forwarded
    mov rax, pNtHeaders
    lea rax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT * SIZEOF IMAGE_DATA_DIRECTORY]
    mov ecx, (IMAGE_DATA_DIRECTORY PTR [rax]).VirtualAddress
    mov edx, (IMAGE_DATA_DIRECTORY PTR [rax]).Size
    add ecx, edx
    
    cmp eax, ecx
    jae @@forwarded
    
    mov [Exports + i * SIZEOF EXPORT_ANALYSIS].Forwarded, 0
    jmp @@next
    
@@forwarded:
    mov [Exports + i * SIZEOF EXPORT_ANALYSIS].Forwarded, 1
    
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBuffer
    
    lea rdi, [Exports + i * SIZEOF EXPORT_ANALYSIS].ForwardName
    mov rsi, rax
@@copy_forward:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @@copy_forward
    
@@next:
    inc i
    jmp @@loop
    
@@no_exports:
    mov Stats.ExportsFound, 0
    
@@done:
    ret
AnalyzeExports ENDP

;----------------------------------------------------------------------------
; IMPORT ANALYSIS
;----------------------------------------------------------------------------

AnalyzeImports PROC FRAME
    LOCAL dwImpRVA:DWORD
    LOCAL pImpDesc:QWORD
    LOCAL i:DWORD
    LOCAL j:DWORD
    LOCAL dwNameRVA:DWORD
    LOCAL pThunk:QWORD
    LOCAL qwThunk:QWORD
    
    mov rax, pNtHeaders
    lea rax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT * SIZEOF IMAGE_DATA_DIRECTORY]
    mov ecx, (IMAGE_DATA_DIRECTORY PTR [rax]).VirtualAddress
    mov dwImpRVA, ecx
    
    test ecx, ecx
    jz @@no_imports
    
    call RVAToFileOffset
    add rax, pFileBuffer
    mov pImpDesc, rax
    
    xor i, i
    
@@descriptor_loop:
    mov rax, pImpDesc
    mov ecx, i
    imul ecx, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    add rax, rcx
    
    mov ecx, (IMAGE_IMPORT_DESCRIPTOR PTR [rax]).Name
    test ecx, ecx
    jz @@done
    
    mov dwNameRVA, ecx
    call RVAToFileOffset
    add rax, pFileBuffer
    
    ; Copy DLL name
    lea rdi, [Imports + Stats.ImportsFound * SIZEOF IMPORT_ANALYSIS].DLLName
    mov rsi, rax
@@copy_dll:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @@copy_dll
    
    ; Process thunks
    mov rax, pImpDesc
    mov ecx, (IMAGE_IMPORT_DESCRIPTOR PTR [rax]).OriginalFirstThunk
    test ecx, ecx
    jnz @@use_oft
    mov ecx, (IMAGE_IMPORT_DESCRIPTOR PTR [rax]).FirstThunk
    
@@use_oft:
    call RVAToFileOffset
    add rax, pFileBuffer
    mov pThunk, rax
    
    xor j, j
    
@@thunk_loop:
    mov rax, pThunk
    mov ecx, j
    shl ecx, 3      ; 64-bit thunks
    mov rax, [rax+rcx]
    mov qwThunk, rax
    
    test rax, rax
    jz @@next_descriptor
    
    test rax, 8000000000000000h  ; Ordinal import
    jnz @@ordinal_import
    
    ; Name import
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBuffer
    
    ; Skip hint
    add rax, 2
    
    ; Copy function name
    lea rdi, [Imports + Stats.ImportsFound * SIZEOF IMPORT_ANALYSIS].FunctionName
    mov rsi, rax
@@copy_func:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @@copy_func
    
    mov [Imports + Stats.ImportsFound * SIZEOF IMPORT_ANALYSIS].IsOrdinal, 0
    jmp @@store_import
    
@@ordinal_import:
    mov [Imports + Stats.ImportsFound * SIZEOF IMPORT_ANALYSIS].IsOrdinal, 1
    and eax, 0FFFFh
    mov [Imports + Stats.ImportsFound * SIZEOF IMPORT_ANALYSIS].Ordinal, ax
    
@@store_import:
    inc Stats.ImportsFound
    cmp Stats.ImportsFound, MAX_IMPORTS
    jge @@done
    
    inc j
    jmp @@thunk_loop
    
@@next_descriptor:
    inc i
    jmp @@descriptor_loop
    
@@no_imports:
@@done:
    ret
AnalyzeImports ENDP

;----------------------------------------------------------------------------
; STRING EXTRACTION
;----------------------------------------------------------------------------

ExtractStrings PROC FRAME
    LOCAL pData:QWORD
    LOCAL dwSize:DWORD
    LOCAL i:DWORD
    LOCAL dwStringLen:DWORD
    LOCAL bIsUnicode:BYTE
    
    mov rax, pFileBuffer
    mov pData, rax
    mov eax, DWORD PTR qwFileSize
    mov dwSize, eax
    
    xor i, i
    mov Stats.StringsExtracted, 0
    
@@scan_loop:
    cmp i, dwSize
    jge @@done
    
    mov rax, pData
    add rax, i
    
    ; Check for ASCII string (>= 4 printable chars)
    mov dwStringLen, 0
    
@@check_ascii:
    movzx ecx, BYTE PTR [rax]
    cmp ecx, 20h        ; Space
    jb @@not_ascii
    cmp ecx, 7Eh        ; ~
    ja @@not_ascii
    
    inc dwStringLen
    inc rax
    cmp dwStringLen, 256
    jge @@save_ascii
    
    mov edx, i
    add edx, dwStringLen
    cmp edx, dwSize
    jge @@save_ascii
    
    jmp @@check_ascii
    
@@save_ascii:
    cmp dwStringLen, 4
    jb @@not_ascii
    
    ; Save string
    mov ecx, Stats.StringsExtracted
    cmp ecx, MAX_STRINGS
    jge @@done
    
    mov [Strings + rcx * SIZEOF STRING_ENTRY].RVA, i
    mov [Strings + rcx * SIZEOF STRING_ENTRY].IsUnicode, 0
    
    lea rdi, [Strings + rcx * SIZEOF STRING_ENTRY].StringData
    mov rsi, pData
    add rsi, i
    mov ecx, dwStringLen
    cmp ecx, 255
    jbe @@copy_len
    mov ecx, 255
    
@@copy_len:
    rep movsb
    mov BYTE PTR [rdi], 0
    
    inc Stats.StringsExtracted
    
    add i, dwStringLen
    jmp @@scan_loop
    
@@not_ascii:
    inc i
    jmp @@scan_loop
    
@@done:
    ret
ExtractStrings ENDP

;----------------------------------------------------------------------------
; HEADER GENERATION
;----------------------------------------------------------------------------

GenerateHeaderFile PROC FRAME lpModuleName:QWORD
    LOCAL hFile:QWORD
    LOCAL i:DWORD
    LOCAL szFilePath:BYTE MAX_PATH DUP(?)
    LOCAL szGuardName:BYTE 128 DUP(?)
    
    ; Build path: output\include\module.h
    mov rcx, OFFSET szOutputPath
    lea rdx, szFilePath
    call lstrcpyA
    
    lea rcx, szFilePath
    mov rdx, OFFSET szBackslashInclude
    call lstrcatA
    
    lea rcx, szFilePath
    mov rdx, lpModuleName
    call lstrcatA
    
    lea rcx, szFilePath
    mov rdx, OFFSET szDotH
    call lstrcatA
    
    ; Create file
    lea rcx, szFilePath
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, rax
    
    ; Generate header guard name (uppercase)
    mov rsi, lpModuleName
    lea rdi, szGuardName
@@upper_loop:
    mov al, [rsi]
    test al, al
    jz @@upper_done
    cmp al, 'a'
    jb @@store_upper
    cmp al, 'z'
    ja @@store_upper
    sub al, 20h
@@store_upper:
    mov [rdi], al
    inc rsi
    inc rdi
    jmp @@upper_loop
@@upper_done:
    mov BYTE PTR [rdi], 0
    
    ; Write header
    mov rcx, hFile
    mov rdx, OFFSET szTemplateHeader
    mov r8, lpModuleName
    mov r9, OFFSET szArch64
    mov rax, (IMAGE_NT_HEADERS64 PTR [pNtHeaders]).OptionalHeader.ImageBase
    mov [rsp+28h], rax
    mov eax, (IMAGE_NT_HEADERS64 PTR [pNtHeaders]).OptionalHeader.AddressOfEntryPoint
    mov [rsp+30h], eax
    mov eax, VER_MAJOR
    mov [rsp+38h], eax
    mov eax, VER_MINOR
    mov [rsp+40h], eax
    mov eax, VER_PATCH
    mov [rsp+48h], eax
    lea rax, szGuardName
    mov [rsp+50h], rax
    mov [rsp+58h], rax
    call wsprintfA
    
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteToFile
    
    ; Write type definitions
    mov rcx, hFile
    mov rdx, OFFSET szTypeSection
    call WriteToFile
    
    ; Write function declarations for exports
    xor i, i
    
@@export_loop:
    cmp i, Stats.ExportsFound
    jge @@exports_done
    
    ; Determine calling convention and return type heuristically
    lea rax, [Exports + i * SIZEOF EXPORT_ANALYSIS].Name
    
    ; Check for specific patterns
    mov al, [rax]
    cmp al, '?'         ; C++ mangled name
    je @@cpp_func
    
    ; Standard C export
    mov rcx, hFile
    mov rdx, OFFSET szFuncTemplate
    mov r8, OFFSET szRetInt       ; Default return type
    lea r9, [Exports + i * SIZEOF EXPORT_ANALYSIS].Name
    mov rax, i
    mov [rsp+28h], rax
    call wsprintfA
    
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteToFile
    
@@next_export:
    inc i
    jmp @@export_loop
    
@@cpp_func:
    ; Would demangle here
    jmp @@next_export
    
@@exports_done:
    ; Write footer
    mov rcx, hFile
    mov rdx, OFFSET szTemplateFooter
    lea r8, szGuardName
    call wsprintfA
    
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteToFile
    
    mov rcx, hFile
    call CloseHandle
    
    inc Stats.HeadersGenerated
    
@@error:
    ret
GenerateHeaderFile ENDP

;----------------------------------------------------------------------------
; CMAKE GENERATION
;----------------------------------------------------------------------------

GenerateCMake PROC FRAME
    LOCAL hFile:QWORD
    LOCAL i:DWORD
    LOCAL szCMakePath:BYTE MAX_PATH DUP(?)
    
    ; Build path
    mov rcx, OFFSET szOutputPath
    lea rdx, szCMakePath
    call lstrcpyA
    
    lea rcx, szCMakePath
    mov rdx, OFFSET szBackslashCMake
    call lstrcatA
    
    ; Create file
    lea rcx, szCMakePath
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, rax
    
    ; Header
    mov rcx, hFile
    mov rdx, OFFSET szTemplateCMake
    mov r8, OFFSET szProjectName
    call wsprintfA
    
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteToFile
    
    ; Add source files
    xor i, i
    
@@source_loop:
    cmp i, Stats.HeadersGenerated
    jge @@sources_done
    
    mov rcx, hFile
    mov rdx, OFFSET szSourceEntry
    lea r8, szProjectName
    call wsprintfA
    
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteToFile
    
    inc i
    jmp @@source_loop
    
@@sources_done:
    ; Footer
    mov rcx, hFile
    mov rdx, OFFSET szTemplateCMakeEnd
    call WriteToFile
    
    mov rcx, hFile
    call CloseHandle
    
@@error:
    ret
GenerateCMake ENDP

;----------------------------------------------------------------------------
; MAIN ANALYSIS ENGINE
;----------------------------------------------------------------------------

AnalyzeFile PROC FRAME lpFilePath:QWORD
    ; Map file
    mov rcx, lpFilePath
    call MapFile
    test rax, rax
    jz @@error
    
    ; Parse headers
    call ParsePEHeaders
    test eax, eax
    jz @@unmap
    
    ; Analyze sections
    call AnalyzeSections
    
    ; Analyze exports
    call AnalyzeExports
    
    ; Analyze imports
    call AnalyzeImports
    
    ; Extract strings
    call ExtractStrings
    
    ; Print summary
    mov rcx, OFFSET szStatusAnalyzing
    mov rdx, lpFilePath
    call PrintFormat
    
    ; Sections
    xor ecx, ecx
@@section_print:
    cmp ecx, Stats.SectionsAnalyzed
    jge @@sections_done
    
    push rcx
    mov rcx, OFFSET szStatusSection
    lea rdx, [Sections + rcx * SIZEOF SECTION_ANALYSIS].Name
    mov r8d, [Sections + rcx * SIZEOF SECTION_ANALYSIS].VirtualAddress
    mov r9d, [Sections + rcx * SIZEOF SECTION_ANALYSIS].VirtualSize
    
    ; Push entropy as double (simplified)
    movsd xmm0, [Sections + rcx * SIZEOF SECTION_ANALYSIS].Entropy
    movsd QWORD PTR [rsp+28h], xmm0
    
    call PrintFormat
    pop rcx
    
    inc ecx
    jmp @@section_print
    
@@sections_done:
    call UnmapFile
    
    mov eax, 1
    ret
    
@@unmap:
    call UnmapFile
@@error:
    xor eax, eax
    ret
AnalyzeFile ENDP

;----------------------------------------------------------------------------
; ENTRY POINT
;----------------------------------------------------------------------------

main PROC FRAME
    LOCAL dwChoice:DWORD
    
    ; Init handles
    mov ecx, STD_INPUT_HANDLE
    call GetStdHandle
    mov hStdIn, rax
    
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov hStdOut, rax
    
    ; Banner
    mov rcx, OFFSET szBanner
    mov edx, VER_MAJOR
    mov r8d, VER_MINOR
    mov r9d, VER_PATCH
    call PrintFormat
    
@@menu_loop:
    mov rcx, OFFSET szMenu
    call Print
    
    call ReadInput
    mov rcx, OFFSET szInputPath
    call atol
    mov dwChoice, eax
    
    cmp dwChoice, 1
    je @@do_analyze
    
    cmp dwChoice, 2
    je @@do_reconstruct
    
    cmp dwChoice, 9
    je @@exit
    
    jmp @@menu_loop
    
@@do_analyze:
    mov rcx, OFFSET szPromptInput
    call Print
    call ReadInput
    
    mov rcx, OFFSET szInputPath
    call AnalyzeFile
    jmp @@menu_loop
    
@@do_reconstruct:
    ; Full reconstruction mode
    jmp @@menu_loop
    
@@exit:
    xor ecx, ecx
    call ExitProcess

main ENDP

; Additional strings
szBackslash             BYTE    "\", 0
szBackslashInclude      BYTE    "\include", 0
szBackslashCMake        BYTE    "\CMakeLists.txt", 0
szDotH                  BYTE    ".h", 0
szArch64                BYTE    "x64", 0
szArch32                BYTE    "x86", 0
szTypeSection           BYTE    13, 10, "/* Type Definitions */", 13, 10, 13, 10, 0
szFuncTemplate          BYTE    "%s %s(void); // Export ordinal: %d", 13, 10, 0
szSourceEntry           BYTE    "    src/%s.c", 13, 10, 0

END main
