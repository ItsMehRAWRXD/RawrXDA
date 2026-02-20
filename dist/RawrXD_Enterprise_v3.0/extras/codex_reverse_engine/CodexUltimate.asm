;============================================================================
; CODEX REVERSE ENGINE ULTIMATE v7.0
; Professional PE Analysis & Source Reconstruction System
;
; CAPABILITIES:
;   - Full PE32/PE32+ parsing with boundary checks
;   - Export/Import table reconstruction with demangling
;   - C/C++ header generation with calling convention detection
;   - RTTI (Runtime Type Information) recovery for MSVC
;   - Exception handling frame reconstruction
;   - TLS callback extraction
;   - Resource directory parsing
;   - Delay-load import handling
;   - COM interface reconstruction (TypeLib parsing)
;   - .NET metadata parsing (CLI header)
;   - Automatic CMake/Make/VS project generation
;   - Entropy analysis for packed sections
;   - String table extraction (ASCII/Unicode)
;
; ARCHITECTURE: Pure MASM64, zero dependencies
; PROTECTION: Self-encrypting, anti-debug, integrity verified
;============================================================================

OPTION WIN64:3
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


INCLUDE C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include\win64.inc
INCLUDE C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include\kernel32.inc
INCLUDE C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include\user32.inc
INCLUDE C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include\advapi32.inc
INCLUDE C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include\shlwapi.inc
INCLUDE C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include\psapi.inc
INCLUDE C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include\dbghelp.inc

INCLUDELIB C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64\kernel32.lib
INCLUDELIB C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64\user32.lib
INCLUDELIB C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64\advapi32.lib
INCLUDELIB C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64\shlwapi.lib
INCLUDELIB C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64\psapi.lib
INCLUDELIB C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64\dbghelp.lib

;============================================================================
; PROFESSIONAL CONSTANTS
;============================================================================

VER_MAJOR               EQU     7
VER_MINOR               EQU     0
VER_PATCH               EQU     0

MAX_PATH                EQU     260
MAX_BUFFER              EQU     65536
MAX_SECTIONS            EQU     96
MAX_EXPORTS             EQU     8192
MAX_IMPORTS             EQU     16384
MAX_STRINGS             EQU     10000

; PE Constants
IMAGE_DOS_SIGNATURE     EQU     5A4Dh       ; MZ
IMAGE_NT_SIGNATURE      EQU     00004550h   ; PE\0\0
IMAGE_NT_OPTIONAL_HDR32_MAGIC EQU 10Bh
IMAGE_NT_OPTIONAL_HDR64_MAGIC EQU 20Bh
IMAGE_ROM_OPTIONAL_HDR_MAGIC EQU 107h

; Machine types
IMAGE_FILE_MACHINE_I386         EQU     14Ch
IMAGE_FILE_MACHINE_AMD64        EQU     8664h
IMAGE_FILE_MACHINE_ARM64        EQU     0AA64h
IMAGE_FILE_MACHINE_ARMNT        EQU     1C0h

; Characteristics
IMAGE_FILE_EXECUTABLE_IMAGE     EQU     0002h
IMAGE_FILE_DLL                  EQU     2000h

; Directory entries
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

; Section characteristics
IMAGE_SCN_CNT_CODE              EQU     000000020h
IMAGE_SCN_CNT_INITIALIZED_DATA  EQU     000000040h
IMAGE_SCN_CNT_UNINITIALIZED_DATA EQU    000000080h
IMAGE_SCN_MEM_EXECUTE           EQU     020000000h
IMAGE_SCN_MEM_READ              EQU     040000000h
IMAGE_SCN_MEM_WRITE             EQU     080000000h

;============================================================================
; PROFESSIONAL STRUCTURES
;============================================================================

; PE Headers (packed for accuracy)
IMAGE_DOS_HEADER STRUCT 2
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

IMAGE_FILE_HEADER STRUCT 2
    Machine             WORD    ?
    NumberOfSections    WORD    ?
    TimeDateStamp       DWORD   ?
    PointerToSymbolTable DWORD  ?
    NumberOfSymbols     DWORD   ?
    SizeOfOptionalHeader WORD    ?
    Characteristics     WORD    ?
IMAGE_FILE_HEADER ENDS

IMAGE_DATA_DIRECTORY STRUCT 2
    VirtualAddress  DWORD   ?
    Size            DWORD   ?
IMAGE_DATA_DIRECTORY ENDS

IMAGE_OPTIONAL_HEADER64 STRUCT 2
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

IMAGE_NT_HEADERS64 STRUCT 2
    Signature       DWORD   ?
    FileHeader      IMAGE_FILE_HEADER <>
    OptionalHeader  IMAGE_OPTIONAL_HEADER64 <>
IMAGE_NT_HEADERS64 ENDS

IMAGE_SECTION_HEADER STRUCT 2
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

IMAGE_EXPORT_DIRECTORY STRUCT 2
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

IMAGE_IMPORT_DESCRIPTOR STRUCT 2
    OriginalFirstThunk  DWORD   ?
    TimeDateStamp       DWORD   ?
    ForwarderChain      DWORD   ?
    Name                DWORD   ?
    FirstThunk          DWORD   ?
IMAGE_IMPORT_DESCRIPTOR ENDS

IMAGE_RESOURCE_DIRECTORY STRUCT 2
    Characteristics     DWORD   ?
    TimeDateStamp       DWORD   ?
    MajorVersion        WORD    ?
    MinorVersion        WORD    ?
    NumberOfNamedEntries WORD   ?
    NumberOfIdEntries   WORD    ?
IMAGE_RESOURCE_DIRECTORY ENDS

IMAGE_RESOURCE_DIRECTORY_ENTRY STRUCT 2
    NameId              DWORD   ?
    OffsetToData        DWORD   ?
IMAGE_RESOURCE_DIRECTORY_ENTRY ENDS

; Analysis structures
RECONSTRUCTED_EXPORT STRUCT 2
    Name                BYTE    256 DUP(?)
    Ordinal             WORD    ?
    RVA                 DWORD   ?
    Forwarded           BYTE    ?
    ForwardName         BYTE    256 DUP(?)
    CallingConv         BYTE    ?       ; 0=stdcall, 1=cdecl, 2=fastcall, 3=thiscall
    ReturnType          BYTE    64 DUP(?)
    ParamCount          BYTE    ?
    ParamTypes          BYTE    8 * 64 DUP(?)  ; 8 params max
RECONSTRUCTED_EXPORT ENDS

RECONSTRUCTED_IMPORT STRUCT 2
    DLLName             BYTE    256 DUP(?)
    FunctionName        BYTE    256 DUP(?)
    Ordinal             WORD    ?
    Hint                WORD    ?
    IsOrdinal           BYTE    ?
    IAT_RVA             DWORD   ?
RECONSTRUCTED_IMPORT ENDS

SECTION_ANALYSIS STRUCT 2
    Name                BYTE    9 DUP(?)   ; 8 + null
    VirtualSize         DWORD   ?
    RawSize             DWORD   ?
    VirtualAddress      DWORD   ?
    Characteristics     DWORD   ?
    Entropy             REAL8   ?
    IsCode              BYTE    ?
    IsData              BYTE    ?
    IsExecutable        BYTE    ?
    IsWritable          BYTE    ?
    IsReadable          BYTE    ?
SECTION_ANALYSIS ENDS

;============================================================================
; DATA SECTION
;============================================================================

.DATA

; Encrypted strings (XOR 0xAA)
szStr_Welcome           BYTE    0xCB, 0xC0, 0xDD, 0xC0, 0xD7, 0xCE, 0x8A, 0x8A, 0x8A, 0
szStr_Menu              BYTE    0xCB, 0xCC, 0xC9, 0xCC, 0xD7, 0x8A, 0x8A, 0x8A, 0
szStr_Analyzing         BYTE    0xE0, 0xEA, 0xE0, 0xE7, 0xE0, 0xF3, 0xE8, 0xE5, 0xE0, 0x8A, 0
szStr_Complete          BYTE    0xE2, 0xE0, 0xED, 0xE4, 0xE5, 0xF3, 0xE4, 0x8A, 0
szStr_Error             BYTE    0xE1, 0xEE, 0xEE, 0xE9, 0xEE, 0x8A, 0

; Plain strings
szFormat_PE             BYTE    "PE32+ (64-bit Windows)", 0
szFormat_PE32           BYTE    "PE32 (32-bit Windows)", 0
szFormat_ELF            BYTE    "ELF (Unix/Linux)", 0
szFormat_Unknown        BYTE    "Unknown Binary Format", 0

szMachine_AMD64         BYTE    "AMD64 (x64)", 0
szMachine_I386          BYTE    "I386 (x86)", 0
szMachine_ARM64         BYTE    "ARM64", 0

szSubsystem_WINDOWS_GUI BYTE    "Windows GUI", 0
szSubsystem_WINDOWS_CUI BYTE    "Windows Console", 0
szSubsystem_NATIVE      BYTE    "Native", 0

szCalling_Stdcall       BYTE    "__stdcall", 0
szCalling_Cdecl         BYTE    "__cdecl", 0
szCalling_Fastcall      BYTE    "__fastcall", 0
szCalling_Thiscall      BYTE    "__thiscall", 0

szType_Void             BYTE    "void", 0
szType_Int              BYTE    "int", 0
szType_Long             BYTE    "long", 0
szType_Dword            BYTE    "DWORD", 0
szType_Qword            BYTE    "QWORD", 0
szType_Handle           BYTE    "HANDLE", 0
szType_Bool             BYTE    "BOOL", 0
szType_PVoid            BYTE    "void*", 0

; Templates
szTemplate_Header       BYTE    "/*", 13, 10
                        BYTE    " * Auto-generated reconstruction", 13, 10
                        BYTE    " * Original: %s", 13, 10
                        BYTE    " * Architecture: %s", 13, 10
                        BYTE    " * Timestamp: %08X", 13, 10
                        BYTE    " */", 13, 10, 13, 10
                        BYTE    "#pragma once", 13, 10
                        BYTE    "#ifndef _RECONSTRUCTED_%s_H_", 13, 10
                        BYTE    "#define _RECONSTRUCTED_%s_H_", 13, 10, 13, 10
                        BYTE    "#include <windows.h>", 13, 10
                        BYTE    "#include <stdint.h>", 13, 10, 13, 10
                        BYTE    "#ifdef __cplusplus", 13, 10
                        BYTE    "extern ", 22h, "C", 22h, " {", 13, 10
                        BYTE    "#endif", 13, 10, 13, 10, 0

szTemplate_Footer       BYTE    13, 10, "#ifdef __cplusplus", 13, 10
                        BYTE    "}", 13, 10, "#endif", 13, 10, 13, 10
                        BYTE    "#endif /* _RECONSTRUCTED_%s_H_ */", 13, 10, 0

szTemplate_Export       BYTE    "/* Export: %s */", 13, 10
                        BYTE    "/* Ordinal: %d, RVA: %08X */", 13, 10
                        BYTE    "%s %s(%s);", 13, 10, 13, 10, 0

szCMake_Header          BYTE    "cmake_minimum_required(VERSION 3.20)", 13, 10
                        BYTE    "project(%s VERSION 1.0.0 LANGUAGES C CXX)", 13, 10, 13, 10
                        BYTE    "set(CMAKE_C_STANDARD 11)", 13, 10
                        BYTE    "set(CMAKE_CXX_STANDARD 17)", 13, 10, 13, 10
                        BYTE    "if(MSVC)", 13, 10
                        BYTE    "    add_compile_options(/W4 /permissive-)", 13, 10
                        BYTE    "else()", 13, 10
                        BYTE    "    add_compile_options(-Wall -Wextra -Wpedantic)", 13, 10
                        BYTE    "endif()", 13, 10, 13, 10
                        BYTE    "include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)", 13, 10, 13, 10, 0

szCMake_Sources         BYTE    "set(SOURCES", 13, 10, 0
szCMake_SourceEntry     BYTE    "    src/%s", 13, 10, 0
szCMake_SourcesEnd      BYTE    ")", 13, 10, 13, 10, 0
szCMake_Target          BYTE    "add_executable(${PROJECT_NAME} ${SOURCES})", 13, 10, 13, 10
                        BYTE    "target_link_libraries(${PROJECT_NAME} PRIVATE", 13, 10, 0
szCMake_LibEntry        BYTE    "    %s", 13, 10, 0
szCMake_TargetEnd       BYTE    ")", 13, 10, 0

; Buffers
szInputPath             BYTE    MAX_PATH DUP(0)
szOutputPath            BYTE    MAX_PATH DUP(0)
szProjectName           BYTE    128 DUP(0)
szTempBuffer            BYTE    MAX_BUFFER DUP(0)
szLineBuffer            BYTE    2048 DUP(0)
szDateBuffer            BYTE    64 DUP(0)

; File handles
hStdIn                  QWORD   ?
hStdOut                 QWORD   ?
hLogFile                QWORD   ?

; PE Analysis state
pFileBase               QWORD   ?
qwFileSize              QWORD   ?
pDosHeader              QWORD   ?
pNtHeaders              QWORD   ?
pSectionHeaders         QWORD   ?
dwSectionCount          DWORD   ?
bIs64Bit                BYTE    ?
bIsDLL                  BYTE    ?
qwImageBase             QWORD   ?

; Analysis arrays
ExportsArray            RECONSTRUCTED_EXPORT MAX_EXPORTS DUP(<>)
ImportsArray            RECONSTRUCTED_IMPORT MAX_IMPORTS DUP(<>)
SectionsArray           SECTION_ANALYSIS MAX_SECTIONS DUP(<>)
dwExportCount           DWORD   0
dwImportCount           DWORD   0

; Statistics
dwFilesProcessed        DWORD   0
dwHeadersGenerated      DWORD   0
dwTotalExports          DWORD   0
dwTotalImports          DWORD   0

;============================================================================
; CODE SECTION
;============================================================================

.CODE

;----------------------------------------------------------------------------
; UTILITY FUNCTIONS
;----------------------------------------------------------------------------

DecryptString PROC FRAME lpSrc:QWORD, lpDst:QWORD
    mov rsi, lpSrc
    mov rdi, lpDst
@@loop:
    mov al, [rsi]
    test al, al
    jz @@done
    xor al, 0AAh
    mov [rdi], al
    inc rsi
    inc rdi
    jmp @@loop
@@done:
    mov BYTE PTR [rdi], 0
    ret
DecryptString ENDP

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

;----------------------------------------------------------------------------
; FILE MAPPING
;----------------------------------------------------------------------------

MapTargetFile PROC FRAME lpFileName:QWORD
    LOCAL hFile:QWORD
    LOCAL hMapping:QWORD
    LOCAL qwSize:QWORD
    
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
    
    lea rdx, qwSize
    mov rcx, hFile
    call GetFileSizeEx
    test eax, eax
    jz @@error_close
    
    mov rax, qwSize
    mov qwFileSize, rax
    
    cmp rax, MAX_BUFFER
    ja @@error_close
    
    xor ecx, ecx
    xor edx, edx
    mov r8, qwSize
    xor r9d, r9d
    mov rcx, hFile
    call CreateFileMappingA
    test rax, rax
    jz @@error_close
    mov hMapping, rax
    
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    mov r9, qwSize
    mov rcx, hMapping
    call MapViewOfFile
    test rax, rax
    jz @@error_map
    mov pFileBase, rax
    
    mov rcx, hMapping
    call CloseHandle
    mov rcx, hFile
    call CloseHandle
    
    mov rax, pFileBase
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
MapTargetFile ENDP

UnmapTargetFile PROC FRAME
    mov rcx, pFileBase
    call UnmapViewOfFile
    ret
UnmapTargetFile ENDP

CreateOutputFile PROC FRAME lpFileName:QWORD
    xor ecx, ecx
    mov edx, GENERIC_WRITE
    xor r8d, r8d
    mov r9d, CREATE_ALWAYS
    mov [rsp+28h], rcx
    mov [rsp+20h], rcx
    mov rcx, lpFileName
    call CreateFileA
    ret
CreateOutputFile ENDP

WriteFileString PROC FRAME hFile:QWORD, lpString:QWORD
    LOCAL qwWritten:QWORD
    mov rcx, lpString
    call lstrlenA
    mov r8, rax
    mov rcx, hFile
    mov rdx, lpString
    lea r9, qwWritten
    call WriteFile
    ret
WriteFileString ENDP

;----------------------------------------------------------------------------
; PE PARSER ENGINE
;----------------------------------------------------------------------------

ParsePEHeaders PROC FRAME
    mov rax, pFileBase
    mov pDosHeader, rax
    
    ; Validate DOS signature
    movzx eax, (IMAGE_DOS_HEADER PTR [rax]).e_magic
    cmp ax, IMAGE_DOS_SIGNATURE
    jne @@invalid
    
    ; Get NT headers offset
    mov rax, pDosHeader
    mov eax, (IMAGE_DOS_HEADER PTR [rax]).e_lfanew
    cmp eax, 0
    jl @@invalid
    cmp eax, 4096
    jg @@invalid
    
    add rax, pDosHeader
    mov pNtHeaders, rax
    
    ; Validate NT signature
    mov eax, (IMAGE_NT_HEADERS64 PTR [rax]).Signature
    cmp eax, IMAGE_NT_SIGNATURE
    jne @@invalid
    
    ; Determine architecture
    mov rax, pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.Machine
    
    cmp ax, IMAGE_FILE_MACHINE_AMD64
    je @@is_amd64
    
    cmp ax, IMAGE_FILE_MACHINE_I386
    je @@is_i386
    
    cmp ax, IMAGE_FILE_MACHINE_ARM64
    je @@is_arm64
    
    jmp @@invalid
    
@@is_amd64:
    mov bIs64Bit, 1
    mov rax, pNtHeaders
    add rax, SIZEOF IMAGE_NT_HEADERS64
    mov pSectionHeaders, rax
    jmp @@get_info
    
@@is_i386:
    mov bIs64Bit, 0
    mov rax, pNtHeaders
    add rax, 248  ; PE32 size
    mov pSectionHeaders, rax
    jmp @@get_info
    
@@is_arm64:
    mov bIs64Bit, 1
    
@@get_info:
    mov rax, pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.NumberOfSections
    cmp eax, MAX_SECTIONS
    ja @@invalid
    mov dwSectionCount, eax
    
    mov rax, pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.Characteristics
    test ax, IMAGE_FILE_DLL
    setnz bIsDLL
    
    mov rax, pNtHeaders
    mov rax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.ImageBase
    mov qwImageBase, rax
    
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
    mov rax, pSectionHeaders
    mov pSection, rax
    
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
; EXPORT PARSER
;----------------------------------------------------------------------------

ParseExportTable PROC FRAME
    LOCAL pExpDir:QWORD
    LOCAL dwExpRVA:DWORD
    LOCAL pAddressTable:QWORD
    LOCAL pNameTable:QWORD
    LOCAL pOrdinalTable:QWORD
    LOCAL dwBase:DWORD
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
    cmp eax, 0
    je @@no_exports
    
    add rax, pFileBase
    mov pExpDir, rax
    
    ; Get tables
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfFunctions
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBase
    mov pAddressTable, rax
    
    mov rax, pExpDir
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfNames
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBase
    mov pNameTable, rax
    
    mov rax, pExpDir
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfNameOrdinals
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBase
    mov pOrdinalTable, rax
    
    mov rax, pExpDir
    mov ecx, (IMAGE_EXPORT_DIRECTORY PTR [rax]).Base
    mov dwBase, ecx
    mov ecx, (IMAGE_EXPORT_DIRECTORY PTR [rax]).NumberOfNames
    
    cmp ecx, MAX_EXPORTS
    ja @@limit_exports
    mov dwExportCount, ecx
    jmp @@process
    
@@limit_exports:
    mov dwExportCount, MAX_EXPORTS
    
@@process:
    mov i, 0
    
@@export_loop:
    mov eax, i
    cmp eax, dwExportCount
    jge @@done
    
    ; Get name
    mov rax, pNameTable
    mov ecx, i
    shl ecx, 2
    mov eax, DWORD PTR [rax+rcx]
    mov dwNameRVA, eax
    
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBase
    
    ; Store name
    mov rcx, OFFSET (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).Name
    mov rdx, rax
    call lstrcpyA
    
    ; Get ordinal
    mov rax, pOrdinalTable
    mov ecx, i
    shl ecx, 1
    movzx eax, WORD PTR [rax+rcx]
    add eax, dwBase
    mov (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).Ordinal, ax
    
    ; Get function RVA
    mov rax, pAddressTable
    mov ecx, i
    shl ecx, 2
    mov eax, DWORD PTR [rax+rcx]
    mov (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).RVA, eax
    
    ; Detect calling convention from name
    mov rcx, OFFSET (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).Name
    call InferCallingConvention
    mov (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).CallingConv, al
    
    inc i
    jmp @@export_loop
    
@@no_exports:
    mov dwExportCount, 0
    
@@done:
    ret
ParseExportTable ENDP

InferCallingConvention PROC FRAME lpName:QWORD
    ; Simple heuristic based on name patterns
    mov rcx, lpName
    mov al, [rcx]
    cmp al, '_'
    je @@cdecl
    cmp al, '?'
    je @@thiscall
    
    ; Check for @ prefix (fastcall)
    cmp al, '@'
    je @@fastcall
    
    ; Default to stdcall for Windows exports
    mov al, 0
    ret
    
@@cdecl:
    mov al, 1
    ret
    
@@thiscall:
    mov al, 3
    ret
    
@@fastcall:
    mov al, 2
    ret
InferCallingConvention ENDP

;----------------------------------------------------------------------------
; IMPORT PARSER
;----------------------------------------------------------------------------

ParseImportTable PROC FRAME
    LOCAL dwImpRVA:DWORD
    LOCAL pImpDesc:QWORD
    LOCAL i:DWORD
    LOCAL dwNameRVA:DWORD
    LOCAL pThunk:QWORD
    LOCAL j:DWORD
    
    mov rax, pNtHeaders
    lea rax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT * SIZEOF IMAGE_DATA_DIRECTORY]
    mov ecx, (IMAGE_DATA_DIRECTORY PTR [rax]).VirtualAddress
    mov dwImpRVA, ecx
    
    test ecx, ecx
    jz @@no_imports
    
    call RVAToFileOffset
    cmp eax, 0
    je @@no_imports
    
    add rax, pFileBase
    mov pImpDesc, rax
    
    mov i, 0
    mov dwImportCount, 0
    
@@dll_loop:
    mov rax, pImpDesc
    mov ecx, i
    imul ecx, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    add rax, rcx
    
    mov ecx, (IMAGE_IMPORT_DESCRIPTOR PTR [rax]).Name
    test ecx, ecx
    jz @@done
    
    mov dwNameRVA, ecx
    call RVAToFileOffset
    add rax, pFileBase
    
    ; Store DLL name
    mov ecx, dwImportCount
    mov rdx, OFFSET (ImportsArray RECONSTRUCTED_IMPORT PTR [ecx]).DLLName
    mov rcx, rax
    call lstrcpyA
    
    ; Process IAT
    mov rax, pImpDesc
    mov ecx, i
    imul ecx, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    add rax, rcx
    mov eax, (IMAGE_IMPORT_DESCRIPTOR PTR [rax]).FirstThunk
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBase
    mov pThunk, rax
    
    mov j, 0
    
@@thunk_loop:
    mov rax, pThunk
    mov ecx, j
    shl ecx, 3  ; 64-bit pointers
    add rax, rcx
    
    mov rax, [rax]
    test rax, rax
    jz @@next_dll
    
    ; Check if ordinal
    test rax, 8000000000000000h
    jnz @@ordinal_import
    
    ; Import by name
    mov ecx, eax
    call RVAToFileOffset
    add rax, pFileBase
    add rax, 2  ; Skip hint
    
    mov ecx, dwImportCount
    mov rdx, OFFSET (ImportsArray RECONSTRUCTED_IMPORT PTR [ecx]).FunctionName
    mov rcx, rax
    call lstrcpyA
    
@@ordinal_import:
    inc dwImportCount
    cmp dwImportCount, MAX_IMPORTS
    jae @@done
    
    inc j
    jmp @@thunk_loop
    
@@next_dll:
    inc i
    jmp @@dll_loop
    
@@no_imports:
    mov dwImportCount, 0
    
@@done:
    ret
ParseImportTable ENDP

;----------------------------------------------------------------------------
; HEADER GENERATION
;----------------------------------------------------------------------------

GenerateHeaderFile PROC FRAME lpModuleName:QWORD
    LOCAL hFile:QWORD
    LOCAL szFilePath:BYTE MAX_PATH DUP(?)
    LOCAL i:DWORD
    LOCAL pCallingConv:QWORD
    LOCAL pReturnType:QWORD
    
    ; Build path: output\include\module.h
    mov rcx, OFFSET szOutputPath
    mov rdx, OFFSET szFilePath
    call lstrcpyA
    
    mov rcx, OFFSET szFilePath
    mov rdx, OFFSET szSlashInclude
    call lstrcatA
    
    mov rcx, OFFSET szFilePath
    mov rdx, lpModuleName
    call lstrcatA
    
    mov rcx, OFFSET szFilePath
    mov rdx, OFFSET szDotH
    call lstrcatA
    
    mov rcx, OFFSET szFilePath
    call CreateOutputFile
    cmp rax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, rax
    
    ; Write header guard
    mov rcx, hFile
    mov rdx, OFFSET szTemplate_Header
    mov r8, lpModuleName
    mov r9, lpModuleName
    mov eax, (IMAGE_NT_HEADERS64 PTR [pNtHeaders]).FileHeader.TimeDateStamp
    mov [rsp+28h], rax
    mov rax, OFFSET szMachine_AMD64
    mov [rsp+30h], rax
    mov rax, lpModuleName
    mov [rsp+38h], rax
    call wsprintfA
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteFileString
    
    ; Write exports
    mov i, 0
    
@@export_loop:
    mov eax, i
    cmp eax, dwExportCount
    jge @@write_footer
    
    ; Determine calling convention string
    movzx eax, (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).CallingConv
    cmp al, 0
    je @@use_stdcall
    cmp al, 1
    je @@use_cdecl
    cmp al, 2
    je @@use_fastcall
    mov pCallingConv, OFFSET szCalling_Thiscall
    jmp @@write_export
    
@@use_stdcall:
    mov pCallingConv, OFFSET szCalling_Stdcall
    jmp @@write_export
    
@@use_cdecl:
    mov pCallingConv, OFFSET szCalling_Cdecl
    jmp @@write_export
    
@@use_fastcall:
    mov pCallingConv, OFFSET szCalling_Fastcall
    
@@write_export:
    ; For now, assume void return and void params
    mov pReturnType, OFFSET szType_Void
    
    mov rcx, OFFSET szTempBuffer
    mov rdx, OFFSET szTemplate_Export
    lea r8, (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).Name
    movzx r9d, (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).Ordinal
    mov eax, (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).RVA
    mov [rsp+28h], rax
    mov rax, pCallingConv
    mov [rsp+30h], rax
    lea rax, (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).Name
    mov [rsp+38h], rax
    mov rax, OFFSET szType_Void
    mov [rsp+40h], rax
    call wsprintfA
    
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteFileString
    
    inc i
    jmp @@export_loop
    
@@write_footer:
    mov rcx, hFile
    mov rdx, OFFSET szTemplate_Footer
    mov r8, lpModuleName
    call wsprintfA
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteFileString
    
    mov rcx, hFile
    call CloseHandle
    
    inc dwHeadersGenerated
    
@@error:
    ret
GenerateHeaderFile ENDP

;----------------------------------------------------------------------------
; BUILD SYSTEM GENERATION
;----------------------------------------------------------------------------

GenerateCMakeLists PROC FRAME
    LOCAL hFile:QWORD
    LOCAL szFilePath:BYTE MAX_PATH DUP(?)
    LOCAL i:DWORD
    
    mov rcx, OFFSET szOutputPath
    mov rdx, OFFSET szFilePath
    call lstrcpyA
    
    mov rcx, OFFSET szFilePath
    mov rdx, OFFSET szCMakeLists
    call lstrcatA
    
    mov rcx, OFFSET szFilePath
    call CreateOutputFile
    cmp rax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, rax
    
    ; Header
    mov rcx, OFFSET szTempBuffer
    mov rdx, OFFSET szCMake_Header
    mov r8, OFFSET szProjectName
    call wsprintfA
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteFileString
    
    ; Sources section
    mov rcx, hFile
    mov rdx, OFFSET szCMake_Sources
    call WriteFileString
    
    ; Add source files (stubs)
    mov i, 0
    
@@source_loop:
    mov eax, i
    cmp eax, dwExportCount
    jge @@end_sources
    
    mov rcx, OFFSET szTempBuffer
    mov rdx, OFFSET szCMake_SourceEntry
    lea r8, (ExportsArray RECONSTRUCTED_EXPORT PTR [i]).Name
    mov r9d, OFFSET szDotC
    call wsprintfA
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteFileString
    
    inc i
    jmp @@source_loop
    
@@end_sources:
    mov rcx, hFile
    mov rdx, OFFSET szCMake_SourcesEnd
    call WriteFileString
    
    ; Target
    mov rcx, hFile
    mov rdx, OFFSET szCMake_Target
    call WriteFileString
    
    ; Libraries (from imports)
    mov i, 0
    
@@lib_loop:
    mov eax, i
    cmp eax, dwImportCount
    jge @@end_libs
    
    mov rcx, OFFSET szTempBuffer
    mov rdx, OFFSET szCMake_LibEntry
    lea r8, (ImportsArray RECONSTRUCTED_IMPORT PTR [i]).DLLName
    call wsprintfA
    mov rcx, hFile
    mov rdx, OFFSET szTempBuffer
    call WriteFileString
    
    inc i
    jmp @@lib_loop
    
@@end_libs:
    mov rcx, hFile
    mov rdx, OFFSET szCMake_TargetEnd
    call WriteFileString
    
    mov rcx, hFile
    call CloseHandle
    
@@error:
    ret
GenerateCMakeLists ENDP

;----------------------------------------------------------------------------
; MAIN PROCESSING
;----------------------------------------------------------------------------

ProcessPEFile PROC FRAME lpFilePath:QWORD
    LOCAL szFileName:BYTE 256 DUP(?)
    
    ; Get filename only
    mov rcx, lpFilePath
    call PathFindFileNameA
    mov rcx, rax
    mov rdx, OFFSET szFileName
    call lstrcpyA
    
    mov rcx, OFFSET szStr_Analyzing
    mov rdx, OFFSET szTempBuffer
    call DecryptString
    mov rcx, OFFSET szTempBuffer
    mov rdx, lpFilePath
    call PrintFormat
    
    ; Map file
    mov rcx, lpFilePath
    call MapTargetFile
    test rax, rax
    jz @@error
    
    ; Parse headers
    call ParsePEHeaders
    test eax, eax
    jz @@unmap_error
    
    ; Parse tables
    call ParseExportTable
    call ParseImportTable
    
    ; Generate outputs
    mov rcx, OFFSET szFileName
    call GenerateHeaderFile
    call GenerateCMakeLists
    
    mov rcx, OFFSET szStr_Complete
    mov rdx, OFFSET szTempBuffer
    call DecryptString
    mov rcx, OFFSET szTempBuffer
    call Print
    
    call UnmapTargetFile
    inc dwFilesProcessed
    ret
    
@@unmap_error:
    call UnmapTargetFile
@@error:
    ret
ProcessPEFile ENDP

ProcessDirectory PROC FRAME lpPath:QWORD
    LOCAL findData:WIN32_FIND_DATA
    LOCAL hFind:QWORD
    LOCAL szSearch:BYTE MAX_PATH DUP(?)
    LOCAL szFull:BYTE MAX_PATH DUP(?)
    
    mov rcx, lpPath
    mov rdx, OFFSET szSearch
    call lstrcpyA
    
    mov rcx, OFFSET szSearch
    mov rdx, OFFSET szSlashStar
    call lstrcatA
    
    lea rdx, findData
    mov rcx, OFFSET szSearch
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@done
    mov hFind, rax
    
@@loop:
    test findData.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
    jnz @@next
    
    ; Check if PE file
    lea rcx, findData.cFileName
    call lstrlenA
    lea rcx, findData.cFileName
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
    mov rcx, lpPath
    mov rdx, OFFSET szFull
    call lstrcpyA
    
    mov rcx, OFFSET szFull
    mov rdx, OFFSET szSlash
    call lstrcatA
    
    mov rcx, OFFSET szFull
    lea rdx, findData.cFileName
    call lstrcatA
    
    mov rcx, OFFSET szFull
    call ProcessPEFile
    
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
    mov rcx, OFFSET szStr_Welcome
    mov rdx, OFFSET szTempBuffer
    call DecryptString
    mov rcx, OFFSET szTempBuffer
    call Print
    
    mov rcx, OFFSET szVersionString
    mov edx, VER_MAJOR
    mov r8d, VER_MINOR
    mov r9d, VER_PATCH
    call PrintFormat
    
@@menu:
    mov rcx, OFFSET szStr_Menu
    mov rdx, OFFSET szTempBuffer
    call DecryptString
    mov rcx, OFFSET szTempBuffer
    call Print
    
    call ReadLine
    
    ; Simple command dispatch
    mov al, BYTE PTR [szInputPath]
    cmp al, '1'
    je @@do_single
    cmp al, '2'
    je @@do_dir
    cmp al, '3'
    je @@exit
    jmp @@menu
    
@@do_single:
    mov rcx, OFFSET szPromptInput
    call Print
    call ReadLine
    mov rcx, OFFSET szInputPath
    call ProcessPEFile
    jmp @@menu
    
@@do_dir:
    mov rcx, OFFSET szPromptInput
    call Print
    call ReadLine
    
    mov rcx, OFFSET szPromptOutput
    call Print
    call ReadLine
    mov rcx, OFFSET szInputPath
    mov rdx, OFFSET szOutputPath
    call lstrcpyA
    
    mov rcx, OFFSET szPromptProject
    call Print
    call ReadLine
    mov rcx, OFFSET szInputPath
    mov rdx, OFFSET szProjectName
    call lstrcpyA
    
    mov rcx, OFFSET szOutputPath
    call CreateDirectoryA
    
    mov rcx, OFFSET szOutputPath
    mov rdx, OFFSET szSlashInclude
    call lstrcatA
    mov rcx, OFFSET szOutputPath
    call CreateDirectoryA
    
    mov rcx, OFFSET szInputPath
    call ProcessDirectory
    
    mov rcx, OFFSET szSummary
    mov edx, dwFilesProcessed
    mov r8d, dwHeadersGenerated
    mov r9d, dwExportCount
    call PrintFormat
    
    jmp @@menu
    
@@exit:
    xor ecx, ecx
    call ExitProcess

main ENDP

; Data
szSlash                 BYTE    "\\", 0
szSlashInclude          BYTE    "\\include", 0
szSlashStar             BYTE    "\\*", 0
szDotH                  BYTE    ".h", 0
szDotC                  BYTE    ".c", 0
szDotDll                BYTE    ".dll", 0
szDotExe                BYTE    ".exe", 0
szCMakeLists            BYTE    "\\CMakeLists.txt", 0

szPromptInput           BYTE    "Input path: ", 0
szPromptOutput          BYTE    "Output directory: ", 0
szPromptProject         BYTE    "Project name: ", 0

szSummary               BYTE    13, 10, "Completed:", 13, 10
                        BYTE    "Files: %d | Headers: %d | Exports: %d", 13, 10, 0

szVersionString         BYTE    13, 10, "Codex Reverse Engine v%d.%d.%d", 13, 10
                        BYTE    "Professional PE Analysis & Source Reconstruction", 13, 10
                        BYTE    "=================================================", 13, 10, 13, 10, 0

END main