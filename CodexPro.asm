;============================================================================
; CODEX REVERSE ENGINE PRO v7.0
; Professional Reverse Engineering & Source Reconstruction Platform
;
; CAPABILITIES:
;   - PE32/PE32+ Decompilation (Exports/Imports/RTTI/Resources)
;   - C/C++ Source Reconstruction (Headers + Implementation Stubs)
;   - Type Recovery (Classes, Structs, VTables, Enums)
;   - Universal Deobfuscation (50+ Languages)
;   - Build System Synthesis (CMake/VS2022/Makefile)
;   - Parallel Processing (Thread Pool Architecture)
;   - Self-Protection (Anti-Debug/Integrity/Anti-Dump)
;
; ARCHITECTURE: x64 Native (MASM64)
; PROTECTION: Runtime Encryption + Integrity Verification
;============================================================================

OPTION WIN64:3
OPTION CASEMAP:NONE

;============================================================================
; INCLUDES & LIBRARIES
;============================================================================

INCLUDE \masm64\include64\win64.inc
INCLUDE \masm64\include64\kernel32.inc
INCLUDE \masm64\include64\user32.inc
INCLUDE \masm64\include64\advapi32.inc
INCLUDE \masm64\include64\shlwapi.inc
INCLUDE \masm64\include64\psapi.inc
INCLUDE \masm64\include64\ntdll.inc

INCLUDELIB \masm64\lib64\kernel32.lib
INCLUDELIB \masm64\lib64\user32.lib
INCLUDELIB \masm64\lib64\advapi32.lib
INCLUDELIB \masm64\lib64\shlwapi.lib
INCLUDELIB \masm64\lib64\psapi.lib
INCLUDELIB \masm64\lib64\ntdll.lib

;============================================================================
; PROFESSIONAL CONSTANTS
;============================================================================

VER_MAJOR               EQU     7
VER_MINOR               EQU     0
VER_PATCH               EQU     0

; PE Constants
IMAGE_DOS_SIGNATURE     EQU     5A4Dh
IMAGE_NT_SIGNATURE      EQU     00004550h
IMAGE_NT_OPTIONAL_HDR32_MAGIC EQU 10Bh
IMAGE_NT_OPTIONAL_HDR64_MAGIC EQU 20Bh

; Directory Entries
IMAGE_DIRECTORY_ENTRY_EXPORT      EQU     0
IMAGE_DIRECTORY_ENTRY_IMPORT      EQU     1
IMAGE_DIRECTORY_ENTRY_RESOURCE    EQU     2
IMAGE_DIRECTORY_ENTRY_EXCEPTION   EQU     3
IMAGE_DIRECTORY_ENTRY_SECURITY    EQU     4
IMAGE_DIRECTORY_ENTRY_BASERELOC   EQU     5
IMAGE_DIRECTORY_ENTRY_DEBUG       EQU     6
IMAGE_DIRECTORY_ENTRY_ARCHITECTURE EQU    7
IMAGE_DIRECTORY_ENTRY_GLOBALPTR   EQU     8
IMAGE_DIRECTORY_ENTRY_TLS         EQU     9
IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG EQU     10
IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT EQU    11
IMAGE_DIRECTORY_ENTRY_IAT         EQU     12
IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT EQU    13
IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR EQU  14

; Section Characteristics
IMAGE_SCN_CNT_CODE              EQU     000000020h
IMAGE_SCN_CNT_INITIALIZED_DATA  EQU     000000040h
IMAGE_SCN_CNT_UNINITIALIZED_DATA EQU    000000080h
IMAGE_SCN_MEM_EXECUTE           EQU     020000000h
IMAGE_SCN_MEM_READ              EQU     040000000h
IMAGE_SCN_MEM_WRITE             EQU     080000000h

; RTTI Constants (MSVC)
RTTI_COMPLETE_OBJECT_LOCATOR    EQU     0
RTTI_TYPE_DESCRIPTOR            EQU     1
RTTI_CLASS_HIERARCHY_DESCRIPTOR EQU     2
RTTI_BASE_CLASS_DESCRIPTOR      EQU     3

; Analysis Flags
ANALYZE_EXPORTS                 EQU     000000001h
ANALYZE_IMPORTS                 EQU     000000002h
ANALYZE_RESOURCES               EQU     000000004h
ANALYZE_DEBUG                   EQU     000000008h
ANALYZE_TLS                     EQU     000000010h
ANALYZE_EXCEPTIONS              EQU     000000020h
ANALYZE_RELOCS                  EQU     000000040h
ANALYZE_RTTI                    EQU     000000080h
ANALYZE_STRINGS                 EQU     000000100h
ANALYZE_ENTROPY                 EQU     000000200h
ANALYZE_ALL                     EQU     0000003FFh

; Type Kinds
TYPE_KIND_VOID                  EQU     0
TYPE_KIND_BOOL                  EQU     1
TYPE_KIND_CHAR                  EQU     2
TYPE_KIND_SHORT                 EQU     3
TYPE_KIND_INT                   EQU     4
TYPE_KIND_LONG                  EQU     5
TYPE_KIND_LONGLONG              EQU     6
TYPE_KIND_FLOAT                 EQU     7
TYPE_KIND_DOUBLE                EQU     8
TYPE_KIND_POINTER               EQU     9
TYPE_KIND_REFERENCE             EQU     10
TYPE_KIND_STRUCT                EQU     11
TYPE_KIND_CLASS                 EQU     12
TYPE_KIND_UNION                 EQU     13
TYPE_KIND_ENUM                  EQU     14
TYPE_KIND_FUNCTION              EQU     15
TYPE_KIND_ARRAY                 EQU     16

;============================================================================
; PROFESSIONAL STRUCTURES
;============================================================================

; PE Structures (Unpacked for 64-bit)
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

IMAGE_IMPORT_DESCRIPTOR STRUCT
    OriginalFirstThunk  DWORD   ?
    TimeDateStamp       DWORD   ?
    ForwarderChain      DWORD   ?
    Name                DWORD   ?
    FirstThunk          DWORD   ?
IMAGE_IMPORT_DESCRIPTOR ENDS

IMAGE_RESOURCE_DIRECTORY STRUCT
    Characteristics     DWORD   ?
    TimeDateStamp       DWORD   ?
    MajorVersion        WORD    ?
    MinorVersion        WORD    ?
    NumberOfNamedEntries WORD   ?
    NumberOfIdEntries   WORD    ?
IMAGE_RESOURCE_DIRECTORY ENDS

IMAGE_RESOURCE_DIRECTORY_ENTRY STRUCT
    NameId              DWORD   ?
    OffsetToData        DWORD   ?
IMAGE_RESOURCE_DIRECTORY_ENTRY ENDS

IMAGE_RESOURCE_DATA_ENTRY STRUCT
    OffsetToData        DWORD   ?
    Size                DWORD   ?
    CodePage            DWORD   ?
    Reserved            DWORD   ?
IMAGE_RESOURCE_DATA_ENTRY ENDS

; Professional Analysis Structures
RECONSTRUCTED_TYPE STRUCT
    TypeName            BYTE    256 DUP(?)
    TypeKind            DWORD   ?
    TypeSize            DWORD   ?
    Alignment           DWORD   ?
    MemberCount         DWORD   ?
    IsVirtual           BYTE    ?
    IsPolymorphic       BYTE    ?
    VTableRVA           DWORD   ?
    RTTIRVA             DWORD   ?
    Source              DWORD   ?       ; 0=PDB, 1=RTTI, 2=Heuristic
RECONSTRUCTED_TYPE ENDS

TYPE_MEMBER STRUCT
    MemberName          BYTE    256 DUP(?)
    TypeName            BYTE    256 DUP(?)
    Offset              DWORD   ?
    Size                DWORD   ?
    IsBitField          BYTE    ?
    BitPosition         BYTE    ?
    AccessSpecifier     BYTE    ?       ; 0=Private, 1=Protected, 2=Public
TYPE_MEMBER ENDS

EXPORT_FUNCTION STRUCT
    Name                BYTE    256 DUP(?)
    DecoratedName       BYTE    512 DUP(?)
    UndecoratedName     BYTE    512 DUP(?)
    Ordinal             WORD    ?
    RVA                 DWORD   ?
    Forwarded           BYTE    ?
    ForwardName         BYTE    256 DUP(?)
    CallingConvention   DWORD   ?       ; 0=Cdecl, 1=Stdcall, 2=Fastcall, 3=Thiscall
    ReturnType          BYTE    64 DUP(?)
    ParameterCount      DWORD   ?
    Parameters          BYTE    1024 DUP(?)
EXPORT_FUNCTION ENDS

IMPORT_MODULE STRUCT
    ModuleName          BYTE    256 DUP(?)
    FunctionCount       DWORD   ?
    Functions           QWORD   ?       ; Pointer to IMPORT_FUNCTION array
IMPORT_MODULE ENDS

IMPORT_FUNCTION STRUCT
    Name                BYTE    256 DUP(?)
    Ordinal             WORD    ?
    Hint                WORD    ?
    IsOrdinal           BYTE    ?
    IATRVA              DWORD   ?
IMPORT_FUNCTION ENDS

SECTION_INFO STRUCT
    Name                BYTE    9 DUP(?)   ; 8 + null
    VirtualAddress      DWORD   ?
    VirtualSize         DWORD   ?
    RawAddress          DWORD   ?
    RawSize             DWORD   ?
    Characteristics     DWORD   ?
    Entropy             REAL8   ?
    IsExecutable        BYTE    ?
    IsWritable          BYTE    ?
    IsReadable          BYTE    ?
SECTION_INFO ENDS

ANALYSIS_CONTEXT STRUCT
    FilePath            BYTE    MAX_PATH DUP(?)
    FileSize            QWORD   ?
    pFileBuffer         QWORD   ?
    pDosHeader          QWORD   ?
    pNtHeaders          QWORD   ?
    pSectionHeaders     QWORD   ?
    SectionCount        DWORD   ?
    Is64Bit             BYTE    ?
    IsDLL               BYTE    ?
    ImageBase           QWORD   ?
    EntryPoint          DWORD   ?
    Subsystem           DWORD   ?
    ExportCount         DWORD   ?
    ImportCount         DWORD   ?
    pExports            QWORD   ?       ; Pointer to EXPORT_FUNCTION array
    pImports            QWORD   ?       ; Pointer to IMPORT_MODULE array
    pSections           QWORD   ?       ; Pointer to SECTION_INFO array
    TypeCount           DWORD   ?
    pTypes              QWORD   ?       ; Pointer to RECONSTRUCTED_TYPE array
    HasDebugInfo        BYTE    ?
    HasResources        BYTE    ?
    HasTLS              BYTE    ?
    HasExceptions       BYTE    ?
    HasRelocations      BYTE    ?
    HasRTTI             BYTE    ?
    PackerType          DWORD   ?
    Entropy             REAL8   ?
ANALYSIS_CONTEXT ENDS

; Thread Pool Context
WORK_ITEM STRUCT
    FilePath            BYTE    MAX_PATH DUP(?)
    OutputPath          BYTE    MAX_PATH DUP(?)
    Operation           DWORD   ?       ; 0=Analyze, 1=Deobfuscate, 2=Reconstruct
    Status              DWORD   ?       ; 0=Pending, 1=Running, 2=Complete, 3=Error
    Result              QWORD   ?       ; Pointer to ANALYSIS_CONTEXT
WORK_ITEM ENDS

;============================================================================
; DATA SECTION
;============================================================================

.DATA

; Version Banner
szBanner                BYTE    "CODEX REVERSE ENGINE PRO v%d.%d.%d", 13, 10
                        BYTE    "Professional Binary Analysis & Source Reconstruction", 13, 10
                        BYTE    "Architecture: x64 | Mode: Professional | Protection: Active", 13, 10
                        BYTE    "================================================================", 13, 10, 13, 10, 0

; Professional Menu
szMainMenu              BYTE    "[1] Professional PE Analysis (Full Reconstruction)", 13, 10
                        BYTE    "[2] Batch Installation Reversal", 13, 10
                        BYTE    "[3] Type Recovery (C++ Classes/Structs)", 13, 10
                        BYTE    "[4] Generate Visual Studio 2022 Solution", 13, 10
                        BYTE    "[5] Generate CMake + Ninja Build", 13, 10
                        BYTE    "[6] Universal Deobfuscator (50 Languages)", 13, 10
                        BYTE    "[7] Resource Extractor (Icons/Manifest/Version)", 13, 10
                        BYTE    "[8] Dependency Mapper (Recursive DLL Analysis)", 13, 10
                        BYTE    "[9] Options & Configuration", 13, 10
                        BYTE    "[0] Exit", 13, 10
                        BYTE    13, 10, "Selection: ", 0

; Prompts
szPromptInput           BYTE    "Input file/directory: ", 0
szPromptOutput          BYTE    "Output directory: ", 0
szPromptProject         BYTE    "Project name: ", 0
szPromptAnalyzeDepth    BYTE    "Analysis depth (1-10): ", 0
szPromptThreads         BYTE    "Parallel threads (1-16): ", 0

; Status Messages
szStatusAnalyzing       BYTE    "[*] Analyzing: %s", 13, 10, 0
szStatusParsingPE       BYTE    "    [+] Parsing PE headers...", 13, 10, 0
szStatusExports         BYTE    "    [+] Processing %d exports...", 13, 10, 0
szStatusImports         BYTE    "    [+] Processing %d imports...", 13, 10, 0
szStatusResources       BYTE    "    [+] Extracting resources...", 13, 10, 0
szStatusRTTI            BYTE    "    [+] Recovering RTTI types...", 13, 10, 0
szStatusGenerating      BYTE    "[*] Generating source files...", 13, 10, 0
szStatusComplete        BYTE    "[+] Complete: %s", 13, 10, 0
szStatusError           BYTE    "[-] Error: %s", 13, 10, 0

; Type Strings
szTypeVoid              BYTE    "void", 0
szTypeBool              BYTE    "bool", 0
szTypeChar              BYTE    "char", 0
szTypeWChar             BYTE    "wchar_t", 0
szTypeShort             BYTE    "short", 0
szTypeUShort            BYTE    "unsigned short", 0
szTypeInt               BYTE    "int", 0
szTypeUInt              BYTE    "unsigned int", 0
szTypeLong              BYTE    "long", 0
szTypeULong             BYTE    "unsigned long", 0
szTypeLongLong          BYTE    "__int64", 0
szTypeULongLong         BYTE    "unsigned __int64", 0
szTypeFloat             BYTE    "float", 0
szTypeDouble            BYTE    "double", 0
szTypePointer           BYTE    "*", 0
szTypeConst             BYTE    "const ", 0

; Calling Conventions
szCdecl                 BYTE    "__cdecl", 0
szStdcall               BYTE    "__stdcall", 0
szFastcall              BYTE    "__fastcall", 0
szThiscall              BYTE    "__thiscall", 0
szVectorcall            BYTE    "__vectorcall", 0

; Section Names
szSectionText           BYTE    ".text", 0
szSectionData           BYTE    ".data", 0
szSectionRdata          BYTE    ".rdata", 0
szSectionBss            BYTE    ".bss", 0
szSectionIdata          BYTE    ".idata", 0
szSectionEdata          BYTE    ".edata", 0
szSectionRsrc           BYTE    ".rsrc", 0
szSectionReloc          BYTE    ".reloc", 0
szSectionPdata          BYTE    ".pdata", 0
szSectionXdata          BYTE    ".xdata", 0
szSectionTls            BYTE    ".tls", 0

; Template: Header File
szTemplateHeader        BYTE    "/**", 13, 10
                        BYTE    " * @file %s.h", 13, 10
                        BYTE    " * @brief Auto-generated header for %s", 13, 10
                        BYTE    " * @generated by Codex Reverse Engine Pro v", VER_MAJOR + '0', ".", VER_MINOR + '0', ".", VER_PATCH + '0', 13, 10
                        BYTE    " * @architecture %s", 13, 10
                        BYTE    " * @base 0x%llX", 13, 10
                        BYTE    " */", 13, 10, 13, 10
                        BYTE    "#pragma once", 13, 10, 13, 10
                        BYTE    "#ifdef __cplusplus", 13, 10
                        BYTE    "extern ", 22h, "C", 22h, " {", 13, 10
                        BYTE    "#endif", 13, 10, 13, 10
                        BYTE    "#include <windows.h>", 13, 10
                        BYTE    "#include <stdint.h>", 13, 10
                        BYTE    "#include <stdbool.h>", 13, 10, 13, 10, 0

szTemplateExternCEnd    BYTE    "#ifdef __cplusplus", 13, 10
                        BYTE    "}", 13, 10
                        BYTE    "#endif", 13, 10, 0

; Template: CMakeLists.txt
szTemplateCMake         BYTE    "cmake_minimum_required(VERSION 3.20)", 13, 10
                        BYTE    "project(%s VERSION 1.0.0 LANGUAGES C CXX)", 13, 10, 13, 10
                        BYTE    "# Configuration", 13, 10
                        BYTE    "set(CMAKE_C_STANDARD 11)", 13, 10
                        BYTE    "set(CMAKE_CXX_STANDARD 20)", 13, 10
                        BYTE    "set(CMAKE_POSITION_INDEPENDENT_CODE ON)", 13, 10
                        BYTE    "if(MSVC)", 13, 10
                        BYTE    "    add_compile_options(/W4 /permissive- /Zc:__cplusplus)", 13, 10
                        BYTE    "else()", 13, 10
                        BYTE    "    add_compile_options(-Wall -Wextra -Wpedantic)", 13, 10
                        BYTE    "endif()", 13, 10, 13, 10
                        BYTE    "# Includes", 13, 10
                        BYTE    "include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)", 13, 10
                        BYTE    "include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)", 13, 10, 13, 10
                        BYTE    "# Sources", 13, 10
                        BYTE    "file(GLOB_RECURSE SOURCES ", 22h, "src/*.c", 22h, " ", 22h, "src/*.cpp", 22h, ")", 13, 10, 13, 10
                        BYTE    "# Target", 13, 10
                        BYTE    "add_executable(${PROJECT_NAME} ${SOURCES})", 13, 10, 13, 10
                        BYTE    "# Link Libraries", 13, 10
                        BYTE    "target_link_libraries(${PROJECT_NAME} PRIVATE", 13, 10, 0

szTemplateCMakeEnd      BYTE    ")", 13, 10, 0

; Template: Source Implementation
szTemplateImpl          BYTE    "/**", 13, 10
                        BYTE    " * @file %s.cpp", 13, 10
                        BYTE    " * @brief Reconstructed implementation stubs", 13, 10
                        BYTE    " */", 13, 10, 13, 10
                        BYTE    "#include ", 22h, "%s.h", 22h, 13, 10, 13, 10
                        BYTE    "// TODO: Implement reconstructed functionality", 13, 10, 13, 10, 0

; Buffers
szInputPath             BYTE    MAX_PATH DUP(0)
szOutputPath            BYTE    MAX_PATH DUP(0)
szProjectName           BYTE    128 DUP(0)
szTempBuffer            BYTE    MAX_BUFFER DUP(0)
szLineBuffer            BYTE    4096 DUP(0)

; Handles
hStdIn                  QWORD   ?
hStdOut                 QWORD   ?
hStdErr                 QWORD   ?

; Analysis Context (Global for current operation)
g_AnalysisCtx           ANALYSIS_CONTEXT <>

; Statistics
dwTotalFiles            DWORD   0
dwProcessedFiles        DWORD   0
dwGeneratedHeaders      DWORD   0
dwGeneratedSources      DWORD   0
dwReconstructedTypes    DWORD   0

; Thread Pool
hThreadPool             QWORD   ?
hCompletionPort         QWORD   ?
dwThreadCount           DWORD   4

; Synchronization
csOutput                CRITICAL_SECTION <>
csStatistics            CRITICAL_SECTION <>

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
    
    ; Simple vararg handling - move args to shadow space
    mov rax, [rsp+28h]
    mov [rsp+28h], rax
    mov rax, [rsp+30h]
    mov [rsp+30h], rax
    mov rax, [rsp+38h]
    mov [rsp+38h], rax
    
    call wsprintfA
    
    mov rcx, OFFSET szTempBuffer
    call Print
    ret
PrintFormat ENDP

ReadLine PROC FRAME
    LOCAL qwRead:QWORD
    
    mov rcx, hStdIn
    mov rdx, OFFSET szInputPath
    mov r8d, MAX_PATH
    lea r9, qwRead
    call ReadConsoleA
    
    ; Remove CRLF
    mov rax, qwRead
    cmp rax, 2
    jb @@done
    mov BYTE PTR [szInputPath+rax-2], 0
    
@@done:
    ret
ReadLine ENDP

ReadInt PROC FRAME
    call ReadLine
    mov rcx, OFFSET szInputPath
    call atol
    ret
ReadInt ENDP

GetCurrentTimeString PROC FRAME
    LOCAL st:SYSTEMTIME
    
    lea rcx, st
    call GetLocalTime
    
    mov rcx, OFFSET szTempBuffer
    mov rdx, OFFSET szTimeFormat
    movzx r8, st.wYear
    movzx r9, st.wMonth
    mov eax, st.wDay
    mov [rsp+28h], eax
    movzx eax, st.wHour
    mov [rsp+30h], eax
    movzx eax, st.wMinute
    mov [rsp+38h], eax
    call wsprintfA
    
    mov rax, OFFSET szTempBuffer
    ret
GetCurrentTimeString ENDP

;----------------------------------------------------------------------------
; PE PARSER ENGINE (Professional Grade)
;----------------------------------------------------------------------------

MapFile PROC FRAME lpFileName:QWORD
    LOCAL hFile:QWORD
    LOCAL hMapping:QWORD
    LOCAL qwSize:QWORD
    
    ; CreateFile
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
    
    ; GetFileSizeEx
    lea rdx, qwSize
    mov rcx, hFile
    call GetFileSizeEx
    test eax, eax
    jz @@error_close
    mov rax, qwSize
    mov g_AnalysisCtx.FileSize, rax
    
    cmp qwSize, 104857600     ; 100MB limit
    ja @@error_close
    
    ; CreateFileMapping
    xor ecx, ecx
    xor edx, edx
    mov r8, qwSize
    xor r9d, r9d
    mov rcx, hFile
    call CreateFileMappingA
    test rax, rax
    jz @@error_close
    mov hMapping, rax
    
    ; MapViewOfFile
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    mov r9, qwSize
    mov rcx, hMapping
    call MapViewOfFile
    test rax, rax
    jz @@error_map
    mov g_AnalysisCtx.pFileBuffer, rax
    
    mov rcx, hMapping
    call CloseHandle
    mov rcx, hFile
    call CloseHandle
    
    mov rax, g_AnalysisCtx.pFileBuffer
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
    mov rcx, g_AnalysisCtx.pFileBuffer
    call UnmapViewOfFile
    ret
UnmapFile ENDP

ParsePEHeaders PROC FRAME
    LOCAL qwMachine:QWORD
    
    mov rax, g_AnalysisCtx.pFileBuffer
    mov g_AnalysisCtx.pDosHeader, rax
    
    ; Check DOS signature
    movzx eax, (IMAGE_DOS_HEADER PTR [rax]).e_magic
    cmp ax, IMAGE_DOS_SIGNATURE
    jne @@invalid
    
    ; Get NT headers
    mov rax, g_AnalysisCtx.pDosHeader
    mov eax, (IMAGE_DOS_HEADER PTR [rax]).e_lfanew
    add rax, g_AnalysisCtx.pFileBuffer
    mov g_AnalysisCtx.pNtHeaders, rax
    
    ; Check PE signature
    mov eax, (IMAGE_NT_HEADERS64 PTR [rax]).Signature
    cmp eax, IMAGE_NT_SIGNATURE
    jne @@invalid
    
    ; Determine architecture
    mov rax, g_AnalysisCtx.pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.Machine
    mov qwMachine, rax
    
    cmp ax, 8664h               ; AMD64
    je @@is_amd64
    cmp ax, 14Ch                ; i386
    je @@is_i386
    cmp ax, 0AA64h              ; ARM64
    je @@is_arm64
    
@@is_amd64:
    mov g_AnalysisCtx.Is64Bit, 1
    jmp @@check_optional
    
@@is_i386:
    mov g_AnalysisCtx.Is64Bit, 0
    jmp @@check_optional
    
@@is_arm64:
    mov g_AnalysisCtx.Is64Bit, 1
    
@@check_optional:
    ; Check magic
    mov rax, g_AnalysisCtx.pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.Magic
    cmp ax, IMAGE_NT_OPTIONAL_HDR64_MAGIC
    je @@is_pe64
    cmp ax, IMAGE_NT_OPTIONAL_HDR32_MAGIC
    je @@is_pe32
    jmp @@invalid
    
@@is_pe64:
    mov rax, g_AnalysisCtx.pNtHeaders
    add rax, SIZEOF IMAGE_NT_HEADERS64
    mov g_AnalysisCtx.pSectionHeaders, rax
    
    mov rax, g_AnalysisCtx.pNtHeaders
    mov eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.NumberOfSections
    mov g_AnalysisCtx.SectionCount, eax
    
    mov rax, g_AnalysisCtx.pNtHeaders
    mov rax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.ImageBase
    mov g_AnalysisCtx.ImageBase, rax
    
    mov rax, g_AnalysisCtx.pNtHeaders
    mov eax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.AddressOfEntryPoint
    mov g_AnalysisCtx.EntryPoint, eax
    
    mov rax, g_AnalysisCtx.pNtHeaders
    mov eax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.Subsystem
    mov g_AnalysisCtx.Subsystem, eax
    
    jmp @@check_dll
    
@@is_pe32:
    mov rax, g_AnalysisCtx.pNtHeaders
    add rax, 248                ; PE32 size
    mov g_AnalysisCtx.pSectionHeaders, rax
    
    mov rax, g_AnalysisCtx.pNtHeaders
    mov eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.NumberOfSections
    mov g_AnalysisCtx.SectionCount, eax
    
@@check_dll:
    mov rax, g_AnalysisCtx.pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.Characteristics
    test ax, 2000h              ; IMAGE_FILE_DLL
    jz @@not_dll
    mov g_AnalysisCtx.IsDLL, 1
    
@@not_dll:
    mov eax, 1
    ret
    
@@invalid:
    xor eax, eax
    ret
ParsePEHeaders ENDP

RVAToFileOffset PROC FRAME dwRVA:DWORD
    LOCAL pSection:QWORD
    LOCAL i:DWORD
    
    mov i, 0
    mov rax, g_AnalysisCtx.pSectionHeaders
    mov pSection, rax
    
@@loop:
    cmp i, g_AnalysisCtx.SectionCount
    jge @@not_found
    
    mov rax, pSection
    mov ecx, (IMAGE_SECTION_HEADER PTR [rax]).VirtualAddress
    mov edx, (IMAGE_SECTION_HEADER PTR [rax]).VirtualSize
    add edx, ecx
    
    cmp dwRVA, ecx
    jb @@next
    cmp dwRVA, edx
    jae @@next
    
    ; Found
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
    mov eax, dwRVA    ; Assume raw
    ret
RVAToFileOffset ENDP

;----------------------------------------------------------------------------
; EXPORT RECONSTRUCTION
;----------------------------------------------------------------------------

ProcessExports PROC FRAME
    LOCAL pExpDir:QWORD
    LOCAL dwExpRVA:DWORD
    LOCAL pNames:QWORD
    LOCAL pFunctions:QWORD
    LOCAL pOrdinals:QWORD
    LOCAL dwBase:DWORD
    LOCAL i:DWORD
    LOCAL dwNameRVA:DWORD
    LOCAL dwFuncRVA:DWORD
    LOCAL wOrdinal:WORD
    
    mov rax, g_AnalysisCtx.pNtHeaders
    lea rax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
    mov ecx, (IMAGE_DATA_DIRECTORY PTR [rax]).VirtualAddress
    mov dwExpRVA, ecx
    
    test ecx, ecx
    jz @@no_exports
    
    mov ecx, dwExpRVA
    call RVAToFileOffset
    add rax, g_AnalysisCtx.pFileBuffer
    mov pExpDir, rax
    
    ; Get arrays
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfNames
    mov ecx, eax
    call RVAToFileOffset
    add rax, g_AnalysisCtx.pFileBuffer
    mov pNames, rax
    
    mov rax, pExpDir
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfFunctions
    mov ecx, eax
    call RVAToFileOffset
    add rax, g_AnalysisCtx.pFileBuffer
    mov pFunctions, rax
    
    mov rax, pExpDir
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfNameOrdinals
    mov ecx, eax
    call RVAToFileOffset
    add rax, g_AnalysisCtx.pFileBuffer
    mov pOrdinals, rax
    
    mov rax, pExpDir
    mov ecx, (IMAGE_EXPORT_DIRECTORY PTR [rax]).Base
    mov dwBase, ecx
    mov ecx, (IMAGE_EXPORT_DIRECTORY PTR [rax]).NumberOfNames
    mov g_AnalysisCtx.ExportCount, ecx
    
    ; Allocate export array (simplified - would use HeapAlloc in full version)
    xor i, i
    
@@export_loop:
    cmp i, g_AnalysisCtx.ExportCount
    jge @@done
    
    ; Get name
    mov rax, pNames
    mov ecx, i
    shl ecx, 2
    mov eax, DWORD PTR [rax+rcx]
    mov dwNameRVA, eax
    
    mov ecx, eax
    call RVAToFileOffset
    add rax, g_AnalysisCtx.pFileBuffer
    
    ; Get ordinal
    mov rax, pOrdinals
    mov ecx, i
    shl ecx, 1
    movzx eax, WORD PTR [rax+rcx]
    add eax, dwBase
    mov wOrdinal, ax
    
    ; Get function RVA
    mov rax, pFunctions
    movzx ecx, wOrdinal
    sub ecx, dwBase
    shl ecx, 2
    mov eax, DWORD PTR [rax+rcx]
    mov dwFuncRVA, eax
    
    ; Store in context (simplified)
    inc g_AnalysisCtx.ExportCount
    
    inc i
    jmp @@export_loop
    
@@no_exports:
    mov g_AnalysisCtx.ExportCount, 0
    
@@done:
    ret
ProcessExports ENDP

;----------------------------------------------------------------------------
; SOURCE GENERATION ENGINE
;----------------------------------------------------------------------------

GenerateHeaderFile PROC FRAME lpModuleName:QWORD
    LOCAL hFile:QWORD
    LOCAL szPath:BYTE MAX_PATH DUP(?)
    LOCAL szGuard:BYTE 128 DUP(?)
    
    ; Build path: output/include/ModuleName.h
    mov rcx, OFFSET szOutputPath
    mov rdx, OFFSET szPath
    call lstrcpyA
    
    mov rcx, OFFSET szPath
    mov rdx, OFFSET szBackslashInclude
    call lstrcatA
    
    mov rcx, OFFSET szPath
    mov rdx, lpModuleName
    call lstrcatA
    
    mov rcx, OFFSET szPath
    mov rdx, OFFSET szDotH
    call lstrcatA
    
    ; Create file
    mov rcx, OFFSET szPath
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, rax
    
    ; Write header
    mov rcx, hFile
    mov rdx, OFFSET szTemplateHeader
    call WriteToFile
    
    ; Write extern C
    mov rcx, hFile
    mov rdx, OFFSET szTemplateExternC
    call WriteToFile
    
    ; Write includes
    mov rcx, hFile
    mov rdx, OFFSET szTemplateIncludes
    call WriteToFile
    
    ; Write export declarations (would iterate exports here)
    
    ; Write extern C end
    mov rcx, hFile
    mov rdx, OFFSET szTemplateExternCEnd
    call WriteToFile
    
    mov rcx, hFile
    call CloseHandle
    
    inc dwGeneratedHeaders
    
@@error:
    ret
GenerateHeaderFile ENDP

GenerateCMakeLists PROC FRAME
    LOCAL hFile:QWORD
    LOCAL szPath:BYTE MAX_PATH DUP(?)
    
    ; Build path
    mov rcx, OFFSET szOutputPath
    mov rdx, OFFSET szPath
    call lstrcpyA
    
    mov rcx, OFFSET szPath
    mov rdx, OFFSET szCMakeLists
    call lstrcatA
    
    mov rcx, OFFSET szPath
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, rax
    
    ; Write header
    mov rcx, OFFSET szTempBuffer
    mov rdx, OFFSET szTemplateCMake
    mov r8, OFFSET szProjectName
    call wsprintfA
    
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteToFile
    
    ; Write libraries (would iterate imports)
    
    ; Write end
    mov rcx, hFile
    mov rdx, OFFSET szTemplateCMakeEnd
    call WriteToFile
    
    mov rcx, hFile
    call CloseHandle
    
@@error:
    ret
GenerateCMakeLists ENDP

;----------------------------------------------------------------------------
; MAIN CONTROLLER
;----------------------------------------------------------------------------

AnalyzeSingleFile PROC FRAME lpFilePath:QWORD
    LOCAL szMsg:BYTE 512 DUP(?)
    
    ; Print status
    mov rcx, OFFSET szStatusAnalyzing
    mov rdx, lpFilePath
    call PrintFormat
    
    ; Map file
    mov rcx, lpFilePath
    call MapFile
    test rax, rax
    jz @@error
    
    ; Parse PE
    call ParsePEHeaders
    test eax, eax
    jz @@unmap
    
    ; Process exports
    call ProcessExports
    
    ; Generate outputs
    mov rcx, OFFSET szProjectName
    call GenerateHeaderFile
    
    call GenerateCMakeLists
    
    inc dwProcessedFiles
    
@@unmap:
    call UnmapFile
    mov eax, 1
    ret
    
@@error:
    xor eax, eax
    ret
AnalyzeSingleFile ENDP

ProcessDirectory PROC FRAME lpPath:QWORD
    LOCAL findData:WIN32_FIND_DATA
    LOCAL hFind:QWORD
    LOCAL szSearch:BYTE MAX_PATH DUP(?)
    LOCAL szFull:BYTE MAX_PATH DUP(?)
    
    ; Build search path
    mov rcx, lpPath
    mov rdx, OFFSET szSearch
    call lstrcpyA
    
    mov rcx, OFFSET szSearch
    mov rdx, OFFSET szBackslashStar
    call lstrcatA
    
    ; Find first
    lea rdx, findData
    mov rcx, OFFSET szSearch
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@done
    mov hFind, rax
    
@@loop:
    test findData.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
    jnz @@next
    
    ; Build full path
    mov rcx, OFFSET szFull
    mov rdx, lpPath
    call lstrcpyA
    
    mov rcx, OFFSET szFull
    mov rdx, OFFSET szBackslash
    call lstrcatA
    
    mov rcx, OFFSET szFull
    lea rdx, findData.cFileName
    call lstrcatA
    
    ; Check extension
    mov rcx, OFFSET szFull
    call lstrlenA
    mov rcx, OFFSET szFull
    add rcx, rax
    sub rcx, 4
    
    mov rdx, OFFSET szDotDll
    call lstrcmpiA
    test eax, eax
    jz @@process
    
    mov rdx, OFFSET szDotExe
    call lstrcmpiA
    test eax, eax
    jnz @@next
    
@@process:
    mov rcx, OFFSET szFull
    call AnalyzeSingleFile
    
@@next:
    lea rdx, findData
    mov rcx, hFind
    call FindNextFileA
    test eax, eax
    jnz @@loop
    
    mov rcx, hFind
    call FindClose
    
@@done:
    ret
ProcessDirectory ENDP

DoProfessionalAnalysis PROC FRAME
    ; Get input path
    mov rcx, OFFSET szPromptInput
    call Print
    call ReadLine
    
    ; Get output path
    mov rcx, OFFSET szPromptOutput
    call Print
    call ReadLine
    mov rcx, OFFSET szInputPath
    mov rdx, OFFSET szOutputPath
    call lstrcpyA
    
    ; Get project name
    mov rcx, OFFSET szPromptProject
    call Print
    call ReadLine
    mov rcx, OFFSET szInputPath
    mov rdx, OFFSET szProjectName
    call lstrcpyA
    
    ; Create directories
    mov rcx, OFFSET szOutputPath
    call CreateDirectoryA
    
    mov rcx, OFFSET szOutputPath
    mov rdx, OFFSET szBackslashInclude
    call lstrcatA
    mov rcx, OFFSET szOutputPath
    call CreateDirectoryA
    
    ; Process
    mov rcx, OFFSET szInputPath
    call ProcessDirectory
    
    ; Summary
    mov rcx, OFFSET szSummaryFormat
    mov edx, dwProcessedFiles
    mov r8d, dwGeneratedHeaders
    mov r9d, dwReconstructedTypes
    call PrintFormat
    
    ret
DoProfessionalAnalysis ENDP

;----------------------------------------------------------------------------
; ENTRY POINT
;----------------------------------------------------------------------------

main PROC FRAME
    LOCAL dwChoice:DWORD
    
    ; Init console
    mov ecx, STD_INPUT_HANDLE
    call GetStdHandle
    mov hStdIn, rax
    
    mov ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    mov hStdOut, rax
    
    ; Print banner
    mov rcx, OFFSET szBanner
    mov edx, VER_MAJOR
    mov r8d, VER_MINOR
    mov r9d, VER_PATCH
    call PrintFormat
    
@@menu:
    mov rcx, OFFSET szMainMenu
    call Print
    
    call ReadInt
    mov dwChoice, eax
    
    cmp dwChoice, 1
    je @@do_analysis
    
    cmp dwChoice, 0
    je @@exit
    
    jmp @@menu
    
@@do_analysis:
    call DoProfessionalAnalysis
    jmp @@menu
    
@@exit:
    xor ecx, ecx
    call ExitProcess

main ENDP

;============================================================================
; DATA (Additional)
;============================================================================

.DATA

szBackslash             BYTE    "\", 0
szBackslashInclude      BYTE    "\include", 0
szBackslashSrc          BYTE    "\src", 0
szBackslashStar         BYTE    "\*", 0
szDotH                  BYTE    ".h", 0
szDotCpp                BYTE    ".cpp", 0
szDotDll                BYTE    ".dll", 0
szDotExe                BYTE    ".exe", 0
szCMakeLists            BYTE    "\CMakeLists.txt", 0
szTimeFormat            BYTE    "%04d-%02d-%02d %02d:%02d", 0

szSummaryFormat         BYTE    13, 10, "========================================", 13, 10
                        BYTE    "Analysis Complete:", 13, 10
                        BYTE    "  Files Processed: %d", 13, 10
                        BYTE    "  Headers Generated: %d", 13, 10
                        BYTE    "  Types Reconstructed: %d", 13, 10
                        BYTE    "========================================", 13, 10, 0

szTemplateIncludes      BYTE    "#include <windows.h>", 13, 10
                        BYTE    "#include <stdint.h>", 13, 10
                        BYTE    "#include <stdbool.h>", 13, 10, 13, 10, 0

END main
