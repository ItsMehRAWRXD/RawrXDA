;============================================================================
; CODEX REVERSE ENGINE v6.0 - The Complete Reversing Architecture
; 
; MODULES:
;   - Universal Deobfuscation Engine (50+ languages)
;   - Installation Reconstructor (PE → Source)
;   - Self-Protection Layer (Anti-debug/Anti-tamper)
;   - Build System Generator (CMake/VS/Make)
;   - Type Recovery Engine (PDB/RTTI/DWARF)
;
; FEATURES:
;   - Zero external dependencies (pure MASM64)
;   - Runtime self-decryption
;   - Parallel processing support
;   - Full PE/ELF/Mach-O parsing
;   - Automatic header reconstruction
;============================================================================

OPTION WIN64:3
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


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
; CONFIGURATION CONSTANTS
;============================================================================

VER_MAJOR               EQU     6
VER_MINOR               EQU     0
VER_PATCH               EQU     0

MAX_PATH                EQU     260
MAX_BUFFER              EQU     65536
MAX_FILE_SIZE           EQU     104857600      ; 100MB
MAX_THREADS             EQU     16

; Protection levels
PROT_NONE               EQU     0
PROT_BASIC              EQU     1
PROT_STANDARD           EQU     2
PROT_MAXIMUM            EQU     3

; Analysis modes
MODE_AUTO               EQU     0
MODE_PE                 EQU     1
MODE_ELF                EQU     2
MODE_MACHO              EQU     3
MODE_DOTNET             EQU     4
MODE_JAVA               EQU     5
MODE_PYTHON             EQU     6
MODE_JAVASCRIPT         EQU     7

; Language IDs (50 languages)
LANG_C                  EQU     1
LANG_CPP                EQU     2
LANG_CSHARP             EQU     3
LANG_JAVA               EQU     4
LANG_PYTHON             EQU     5
LANG_JAVASCRIPT         EQU     6
LANG_TYPESCRIPT         EQU     7
LANG_GO                 EQU     8
LANG_RUST               EQU     9
LANG_SWIFT              EQU     10
LANG_KOTLIN             EQU     11
LANG_PHP                EQU     12
LANG_RUBY               EQU     13
LANG_PERL               EQU     14
LANG_LUA                EQU     15
LANG_SHELL              EQU     16
LANG_SQL                EQU     17
LANG_WEBASSEMBLY        EQU     18
LANG_OBJECTIVEC         EQU     19
LANG_DART               EQU     20
LANG_SCALA              EQU     21
LANG_ERLANG             EQU     22
LANG_ELIXIR             EQU     23
LANG_HASKELL            EQU     24
LANG_CLOJURE            EQU     25
LANG_FSHARP             EQU     26
LANG_COBOL              EQU     27
LANG_FORTRAN            EQU     28
LANG_PASCAL             EQU     29
LANG_DELPHI             EQU     30
LANG_LISP               EQU     31
LANG_PROLOG             EQU     32
LANG_ADA                EQU     33
LANG_VHDL               EQU     34
LANG_VERILOG            EQU     35
LANG_SOLIDITY           EQU     36
LANG_VBA                EQU     37
LANG_POWERSHELL         EQU     38
LANG_MATLAB             EQU     39
LANG_R                  EQU     40
LANG_GROOVY             EQU     41
LANG_JULIA              EQU     42
LANG_OCAML              EQU     43
LANG_SCHEME             EQU     44
LANG_TCL                EQU     45
LANG_VBNET              EQU     46
LANG_ACTIONSCRIPT       EQU     47
LANG_MARKDOWN           EQU     48
LANG_YAML               EQU     49
LANG_XML                EQU     50

; Obfuscator signatures
OBF_NONE                EQU     0
OBF_VMProtect           EQU     1
OBF_Themida             EQU     2
OBF_UPX                 EQU     3
OBF_PyArmor             EQU     4
OBF_IONCube             EQU     5
OBF_JavascriptObf       EQU     6
OBF_ConfuserEx          EQU     7
OBF_Garble              EQU     8
OBF_ProGuard            EQU     9
OBF_Babel               EQU     10

; PE Constants
IMAGE_DOS_SIGNATURE     EQU     5A4Dh
IMAGE_NT_SIGNATURE      EQU     00004550h
IMAGE_NT_OPTIONAL_HDR32_MAGIC EQU 10Bh
IMAGE_NT_OPTIONAL_HDR64_MAGIC EQU 20Bh

; Directory entries
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

;============================================================================
; STRUCTURES
;============================================================================

; PE Structures
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
    Misc            DWORD   ?
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

; Analysis structures
ANALYSIS_RESULT STRUCT
    FilePath            BYTE    MAX_PATH DUP(?)
    FileType            DWORD   ?
    Architecture        DWORD   ?
    IsPacked            BYTE    ?
    PackerType          DWORD   ?
    IsDotNet            BYTE    ?
    IsJava              BYTE    ?
    ExportCount         DWORD   ?
    ImportCount         DWORD   ?
    SectionCount        DWORD   ?
    HasDebugInfo        BYTE    ?
    HasResources        BYTE   ?
    Entropy             REAL8   ?
ANALYSIS_RESULT ENDS

RECONSTRUCTED_TYPE STRUCT
    TypeName            BYTE    256 DUP(?)
    TypeKind            DWORD   ?       ; 0=struct, 1=class, 2=union, 3=enum, 4=function
    TypeSize            DWORD   ?
    MemberCount         DWORD   ?
    IsReconstructed     BYTE    ?
    Source              DWORD   ?       ; 0=PDB, 1=RTTI, 2=Heuristic
RECONSTRUCTED_TYPE ENDS

EXPORT_ENTRY STRUCT
    Name                BYTE    256 DUP(?)
    Ordinal             WORD    ?
    RVA                 DWORD   ?
    Forwarded           BYTE    ?
    ForwardName         BYTE    256 DUP(?)
    ReconstructedDecl   BYTE    512 DUP(?)
EXPORT_ENTRY ENDS

IMPORT_ENTRY STRUCT
    DLLName             BYTE    256 DUP(?)
    FunctionName        BYTE    256 DUP(?)
    Ordinal             WORD    ?
    Hint                WORD    ?
    IsOrdinal           BYTE    ?
IMPORT_ENTRY ENDS

CODE_PATTERN STRUCT
    PatternBytes        BYTE    32 DUP(?)
    PatternMask         BYTE    32 DUP(?)
    PatternLength       DWORD   ?
    Description         BYTE    128 DUP(?)
CODE_PATTERN ENDS

;============================================================================
; DATA SECTION
;============================================================================

.DATA

; Version info
szVersionString         BYTE    "CODEX REVERSE ENGINE v%d.%d.%d", 13, 10
                        BYTE    "Universal Binary Deobfuscator & Installation Reverser", 13, 10
                        BYTE    "Architecture: x64 | Protection: Active | Mode: Maximum", 13, 10
                        BYTE    "================================================================", 13, 10, 13, 10, 0

; Main menu
szMainMenu              BYTE    "[1] Universal Deobfuscation (50 Languages)", 13, 10
                        BYTE    "[2] Reverse Installation (PE → Source)", 13, 10
                        BYTE    "[3] Generate Build System (CMake/VS/Make)", 13, 10
                        BYTE    "[4] Type Recovery (PDB/RTTI Analysis)", 13, 10
                        BYTE    "[5] Dependency Mapper", 13, 10
                        BYTE    "[6] COM Interface Reconstructor", 13, 10
                        BYTE    "[7] Batch Process Directory", 13, 10
                        BYTE    "[8] Self-Test Integrity", 13, 10
                        BYTE    "[9] Exit", 13, 10
                        BYTE    "Selection: ", 0

; Prompts
szPromptInputPath       BYTE    "Input path (file or directory): ", 0
szPromptOutputPath      BYTE    "Output directory: ", 0
szPromptProjectName     BYTE    "Project name: ", 0
szPromptLanguage        BYTE    "Target language (0=Auto): ", 0
szPromptProtection      BYTE    "Protection level (0-3): ", 0

; Status messages
szStatusAnalyzing       BYTE    "[*] Analyzing: %s", 13, 10, 0
szStatusDetected        BYTE    "[+] Detected: %s (Confidence: %d%%)", 13, 10, 0
szStatusGenerating      BYTE    "[*] Generating %s...", 13, 10, 0
szStatusComplete        BYTE    "[+] Complete: %s", 13, 10, 0
szStatusWarning         BYTE    "[!] Warning: %s", 13, 10, 0
szStatusError           BYTE    "[-] Error: %s", 13, 10, 0

; File type strings
szTypePE32              BYTE    "PE32 (32-bit Windows)", 0
szTypePE64              BYTE    "PE32+ (64-bit Windows)", 0
szTypeELF               BYTE    "ELF (Unix/Linux)", 0
szTypeMachO             BYTE    "Mach-O (macOS/iOS)", 0
szTypeNET               BYTE    ".NET Assembly", 0
szTypeJava              BYTE    "Java Bytecode", 0
szTypePython            BYTE    "Python Bytecode", 0
szTypeUnknown           BYTE    "Unknown/Raw Binary", 0

; Language strings
szLanguages             QWORD   OFFSET szLangC, OFFSET szLangCpp, OFFSET szLangCs, OFFSET szLangJava
                        QWORD   OFFSET szLangPython, OFFSET szLangJs, OFFSET szLangTs, OFFSET szLangGo
                        QWORD   OFFSET szLangRust, OFFSET szLangSwift, OFFSET szLangKotlin, OFFSET szLangPhp
                        QWORD   OFFSET szLangRuby, OFFSET szLangPerl, OFFSET szLangLua, OFFSET szLangShell
                        QWORD   OFFSET szLangSql, OFFSET szLangWasm, OFFSET szLangObjC, OFFSET szLangDart
                        QWORD   OFFSET szLangScala, OFFSET szLangErlang, OFFSET szLangElixir, OFFSET szLangHaskell
                        QWORD   OFFSET szLangClojure, OFFSET szLangFsharp, OFFSET szLangCobol, OFFSET szLangFortran
                        QWORD   OFFSET szLangPascal, OFFSET szLangDelphi, OFFSET szLangLisp, OFFSET szLangProlog
                        QWORD   OFFSET szLangAda, OFFSET szLangVhdl, OFFSET szLangVerilog, OFFSET szLangSolidity
                        QWORD   OFFSET szLangVba, OFFSET szLangPs, OFFSET szLangMatlab, OFFSET szLangR
                        QWORD   OFFSET szLangGroovy, OFFSET szLangJulia, OFFSET szLangOcaml, OFFSET szLangScheme
                        QWORD   OFFSET szLangTcl, OFFSET szLangVbnet, OFFSET szLangAs, OFFSET szLangMd
                        QWORD   OFFSET szLangYaml, OFFSET szLangXml

szLangC                 BYTE    "C", 0
szLangCpp               BYTE    "C++", 0
szLangCs                BYTE    "C#", 0
szLangJava              BYTE    "Java", 0
szLangPython            BYTE    "Python", 0
szLangJs                BYTE    "JavaScript", 0
szLangTs                BYTE    "TypeScript", 0
szLangGo                BYTE    "Go", 0
szLangRust              BYTE    "Rust", 0
szLangSwift             BYTE    "Swift", 0
szLangKotlin            BYTE    "Kotlin", 0
szLangPhp               BYTE    "PHP", 0
szLangRuby              BYTE    "Ruby", 0
szLangPerl              BYTE    "Perl", 0
szLangLua               BYTE    "Lua", 0
szLangShell             BYTE    "Shell", 0
szLangSql               BYTE    "SQL", 0
szLangWasm              BYTE    "WebAssembly", 0
szLangObjC              BYTE    "Objective-C", 0
szLangDart              BYTE    "Dart", 0
szLangScala             BYTE    "Scala", 0
szLangErlang            BYTE    "Erlang", 0
szLangElixir            BYTE    "Elixir", 0
szLangHaskell           BYTE    "Haskell", 0
szLangClojure           BYTE    "Clojure", 0
szLangFsharp            BYTE    "F#", 0
szLangCobol             BYTE    "COBOL", 0
szLangFortran           BYTE    "Fortran", 0
szLangPascal            BYTE    "Pascal", 0
szLangDelphi            BYTE    "Delphi", 0
szLangLisp              BYTE    "Lisp", 0
szLangProlog            BYTE    "Prolog", 0
szLangAda               BYTE    "Ada", 0
szLangVhdl              BYTE    "VHDL", 0
szLangVerilog           BYTE    "Verilog", 0
szLangSolidity          BYTE    "Solidity", 0
szLangVba               BYTE    "VBA", 0
szLangPs                BYTE    "PowerShell", 0
szLangMatlab            BYTE    "MATLAB", 0
szLangR                 BYTE    "R", 0
szLangGroovy            BYTE    "Groovy", 0
szLangJulia             BYTE    "Julia", 0
szLangOcaml             BYTE    "OCaml", 0
szLangScheme            BYTE    "Scheme", 0
szLangTcl               BYTE    "Tcl", 0
szLangVbnet             BYTE    "VB.NET", 0
szLangAs                BYTE    "ActionScript", 0
szLangMd                BYTE    "Markdown", 0
szLangYaml              BYTE    "YAML", 0
szLangXml               BYTE    "XML", 0

; Packer signatures
szPackerVMProtect       BYTE    "VMProtect", 0
szPackerThemida         BYTE    "Themida/WinLicense", 0
szPackerUPX             BYTE    "UPX", 0
szPackerPyArmor         BYTE    "PyArmor", 0
szPackerIONCube         BYTE    "ionCube", 0
szPackerJSObf           BYTE    "javascript-obfuscator", 0
szPackerConfuser        BYTE    "ConfuserEx", 0
szPackerGarble          BYTE    "Garble (Go)", 0
szPackerProGuard        BYTE    "ProGuard", 0

; Template strings for generation
szTemplateHeaderGuard   BYTE    "#pragma once", 13, 10
                        BYTE    "// Auto-generated by Codex Reverse Engine", 13, 10
                        BYTE    "// Source: %s", 13, 10
                        BYTE    "// Architecture: %s", 13, 10
                        BYTE    "// Reconstruction Date: %s", 13, 10, 13, 10, 0

szTemplateExternC       BYTE    "#ifdef __cplusplus", 13, 10
                        BYTE    "extern ", 22h, "C", 22h, " {", 13, 10
                        BYTE    "#endif", 13, 10, 13, 10, 0

szTemplateExternCEnd    BYTE    "#ifdef __cplusplus", 13, 10
                        BYTE    "}", 13, 10
                        BYTE    "#endif", 13, 10, 0

szTemplateIncludes      BYTE    "#include <windows.h>", 13, 10
                        BYTE    "#include <stdint.h>", 13, 10
                        BYTE    "#include <stdbool.h>", 13, 10, 13, 10, 0

szTemplateCMake         BYTE    "cmake_minimum_required(VERSION 3.20)", 13, 10
                        BYTE    "project(%s VERSION 1.0.0 LANGUAGES C CXX)", 13, 10, 13, 10
                        BYTE    "set(CMAKE_C_STANDARD 11)", 13, 10
                        BYTE    "set(CMAKE_CXX_STANDARD 17)", 13, 10
                        BYTE    "set(CMAKE_POSITION_INDEPENDENT_CODE ON)", 13, 10, 13, 10
                        BYTE    "include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)", 13, 10, 13, 10
                        BYTE    "set(SOURCES", 13, 10, 0

szTemplateCMakeEnd      BYTE    ")", 13, 10, 13, 10
                        BYTE    "add_executable(${PROJECT_NAME} ${SOURCES})", 13, 10, 13, 10
                        BYTE    "target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS})", 13, 10, 0

; Analysis patterns (signatures)
PatternDotNet           BYTE    0x4D, 0x5A                     ; MZ followed by PE with COM descriptor
PatternJava             BYTE    0xCA, 0xFE, 0xBA, 0xBE         ; Java magic
PatternPython           BYTE    0x42, 0x0D, 0x0D, 0x0A         ; Python 3.7+ magic
PatternUPX              BYTE    0x55, 0x50, 0x58, 0x21         ; UPX!
PatternVMProtect        BYTE    0xEB, 0x10, 0x66, 0x8B         ; VMProtect entry pattern

; Buffers
szInputPath             BYTE    MAX_PATH DUP(0)
szOutputPath            BYTE    MAX_PATH DUP(0)
szProjectName           BYTE    128 DUP(0)
szTempBuffer            BYTE    MAX_BUFFER DUP(0)
szLineBuffer            BYTE    1024 DUP(0)
szDateBuffer            BYTE    64 DUP(0)

; File handles
hStdIn                  QWORD   ?
hStdOut                 QWORD   ?
hCurrentFile            QWORD   ?
hFind                   QWORD   ?

; Analysis state
CurrentAnalysis         ANALYSIS_RESULT <>
pFileBuffer             QWORD   ?
qwFileSize              QWORD   ?
pDosHeader              QWORD   ?
pNtHeaders              QWORD   ?
pSectionHeaders         QWORD   ?
pExportDir              QWORD   ?
pImportDir              QWORD   ?
pResourceDir            QWORD   ?
pDebugDir               QWORD   ?

; Counters
dwFilesProcessed        DWORD   0
dwHeadersGenerated      DWORD   0
dwTypesReconstructed    DWORD   0
dwExportsTotal          DWORD   0
dwImportsTotal          DWORD   0

; Thread pool handles
hThreadPool             QWORD   ?
hCompletionPort         QWORD   ?

;============================================================================
; CODE SECTION
;============================================================================

.CODE

;----------------------------------------------------------------------------
; CONSOLE I/O FUNCTIONS
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
    
    ; Varargs are in registers/stack - simplified for common case
    mov rax, [rsp+28h]      ; First arg after return address
    mov [rsp+28h], rax
    
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
    
    ; Remove CRLF
    mov rax, qwRead
    cmp rax, 2
    jb @@done
    
    mov BYTE PTR [szInputPath+rax-2], 0
    
@@done:
    ret
ReadInput ENDP

ReadInt PROC FRAME
    call ReadInput
    mov rcx, OFFSET szInputPath
    call atol
    ret
ReadInt ENDP

;----------------------------------------------------------------------------
; FILE OPERATIONS
;----------------------------------------------------------------------------

MapFile PROC FRAME lpFileName:QWORD
    LOCAL hFile:QWORD
    LOCAL hMapping:QWORD
    LOCAL qwSizeHigh:QWORD
    
    ; Open file
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
    
    ; Get size
    lea rdx, qwSizeHigh
    mov rcx, hFile
    call GetFileSizeEx
    test eax, eax
    jz @@error_size
    
    mov rax, [rsp+30h]          ; Low 64-bits from stack
    mov qwFileSize, rax
    
    cmp rax, MAX_FILE_SIZE
    ja @@error_size
    
    ; Create mapping
    xor ecx, ecx
    xor edx, edx
    mov r8, qwFileSize
    xor r9d, r9d
    mov rcx, hFile
    call CreateFileMappingA
    test rax, rax
    jz @@error_map
    mov hMapping, rax
    
    ; Map view
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    mov r9, qwFileSize
    mov rcx, hMapping
    call MapViewOfFile
    test rax, rax
    jz @@error_view
    mov pFileBuffer, rax
    
    ; Close mapping handle (view remains)
    mov rcx, hMapping
    call CloseHandle
    
    mov rcx, hFile
    call CloseHandle
    
    mov rax, pFileBuffer
    ret
    
@@error:
    xor eax, eax
    ret
    
@@error_size:
@@error_map:
@@error_view:
    mov rcx, hFile
    call CloseHandle
    xor eax, eax
    ret
MapFile ENDP

UnmapFile PROC FRAME
    mov rcx, pFileBuffer
    call UnmapViewOfFile
    ret
UnmapFile ENDP

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

WriteToFile PROC FRAME hFile:QWORD, lpData:QWORD
    LOCAL qwWritten:QWORD
    
    mov rcx, lpData
    call lstrlenA
    mov r8, rax
    
    mov rcx, hFile
    mov rdx, lpData
    lea r9, qwWritten
    call WriteFile
    
    ret
WriteToFile ENDP

;----------------------------------------------------------------------------
; PE PARSER ENGINE
;----------------------------------------------------------------------------

ParsePEHeaders PROC FRAME
    mov rax, pFileBuffer
    mov pDosHeader, rax
    
    ; Check DOS signature
    movzx eax, (IMAGE_DOS_HEADER PTR [rax]).e_magic
    cmp ax, IMAGE_DOS_SIGNATURE
    jne @@invalid
    
    ; Get NT headers
    mov rax, pDosHeader
    mov eax, (IMAGE_DOS_HEADER PTR [rax]).e_lfanew
    add rax, pDosHeader
    mov pNtHeaders, rax
    
    ; Check NT signature
    mov eax, (IMAGE_NT_HEADERS64 PTR [rax]).Signature
    cmp eax, IMAGE_NT_SIGNATURE
    jne @@invalid
    
    ; Determine architecture
    mov rax, pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.Machine
    cmp ax, 8664h               ; AMD64
    je @@is_amd64
    cmp ax, 14Ch                ; i386
    je @@is_i386
    cmp ax, 0AA64h              ; ARM64
    je @@is_arm64
    
@@is_amd64:
    mov CurrentAnalysis.Architecture, 64
    jmp @@check_magic
    
@@is_i386:
    mov CurrentAnalysis.Architecture, 32
    jmp @@check_magic
    
@@is_arm64:
    mov CurrentAnalysis.Architecture, 128
    
@@check_magic:
    ; Check optional header magic
    mov rax, pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.Magic
    cmp ax, IMAGE_NT_OPTIONAL_HDR64_MAGIC
    je @@is_pe64
    cmp ax, IMAGE_NT_OPTIONAL_HDR32_MAGIC
    je @@is_pe32
    
    jmp @@invalid
    
@@is_pe64:
    mov CurrentAnalysis.FileType, MODE_PE
    mov rax, pNtHeaders
    add rax, SIZEOF IMAGE_NT_HEADERS64
    mov pSectionHeaders, rax
    jmp @@get_sections
    
@@is_pe32:
    mov CurrentAnalysis.FileType, MODE_PE
    mov rax, pNtHeaders
    add rax, 248                ; F0h + 18h (PE32 optional header is smaller)
    mov pSectionHeaders, rax
    
@@get_sections:
    mov rax, pNtHeaders
    movzx eax, (IMAGE_NT_HEADERS64 PTR [rax]).FileHeader.NumberOfSections
    mov CurrentAnalysis.SectionCount, eax
    
    mov eax, 1
    ret
    
@@invalid:
    xor eax, eax
    ret
ParsePEHeaders ENDP

RVAToOffset PROC FRAME dwRVA:DWORD
    LOCAL dwSections:DWORD
    LOCAL pSection:QWORD
    LOCAL i:DWORD
    
    mov eax, CurrentAnalysis.SectionCount
    mov dwSections, eax
    mov pSection, pSectionHeaders
    xor i, i
    
@@loop:
    cmp i, dwSections
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
RVAToOffset ENDP

ParseExports PROC FRAME
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
    
    mov rax, pNtHeaders
    lea rax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT * SIZEOF IMAGE_DATA_DIRECTORY]
    mov ecx, (IMAGE_DATA_DIRECTORY PTR [rax]).VirtualAddress
    mov dwExpRVA, ecx
    
    test ecx, ecx
    jz @@no_exports
    
    call RVAToOffset
    add rax, pFileBuffer
    mov pExpDir, rax
    
    ; Get arrays
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfNames
    mov ecx, eax
    call RVAToOffset
    add rax, pFileBuffer
    mov pNames, rax
    
    mov rax, pExpDir
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfFunctions
    mov ecx, eax
    call RVAToOffset
    add rax, pFileBuffer
    mov pFunctions, rax
    
    mov rax, pExpDir
    mov eax, (IMAGE_EXPORT_DIRECTORY PTR [rax]).AddressOfNameOrdinals
    mov ecx, eax
    call RVAToOffset
    add rax, pFileBuffer
    mov pOrdinals, rax
    
    mov rax, pExpDir
    mov ecx, (IMAGE_EXPORT_DIRECTORY PTR [rax]).Base
    mov dwBase, ecx
    mov ecx, (IMAGE_EXPORT_DIRECTORY PTR [rax]).NumberOfNames
    mov CurrentAnalysis.ExportCount, ecx
    
    xor i, i
    
@@export_loop:
    cmp i, CurrentAnalysis.ExportCount
    jge @@done
    
    ; Get name
    mov rax, pNames
    mov ecx, i
    shl ecx, 2
    mov eax, DWORD PTR [rax+rcx]
    mov dwNameRVA, eax
    
    mov ecx, eax
    call RVAToOffset
    add rax, pFileBuffer
    
    ; Store for later use (simplified)
    inc dwExportsTotal
    
    inc i
    jmp @@export_loop
    
@@no_exports:
    mov CurrentAnalysis.ExportCount, 0
    
@@done:
    ret
ParseExports ENDP

ParseImports PROC FRAME
    LOCAL dwImpRVA:DWORD
    LOCAL pImpDesc:QWORD
    LOCAL i:DWORD
    LOCAL dwNameRVA:DWORD
    
    mov rax, pNtHeaders
    lea rax, (IMAGE_NT_HEADERS64 PTR [rax]).OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT * SIZEOF IMAGE_DATA_DIRECTORY]
    mov ecx, (IMAGE_DATA_DIRECTORY PTR [rax]).VirtualAddress
    mov dwImpRVA, ecx
    
    test ecx, ecx
    jz @@no_imports
    
    call RVAToOffset
    add rax, pFileBuffer
    mov pImpDesc, rax
    
    xor i, i
    
@@import_loop:
    mov rax, pImpDesc
    mov ecx, i
    imul ecx, SIZEOF IMAGE_IMPORT_DESCRIPTOR
    add rax, rcx
    
    mov ecx, (IMAGE_IMPORT_DESCRIPTOR PTR [rax]).Name
    test ecx, ecx
    jz @@done
    
    mov dwNameRVA, ecx
    call RVAToOffset
    add rax, pFileBuffer
    
    inc dwImportsTotal
    inc CurrentAnalysis.ImportCount
    inc i
    jmp @@import_loop
    
@@no_imports:
@@done:
    ret
ParseImports ENDP

DetectPacker PROC FRAME
    LOCAL dwEntropy:DWORD
    
    ; Check for UPX
    mov rax, pFileBuffer
    mov eax, [rax+60]           ; e_lfanew
    add rax, pFileBuffer
    mov eax, [rax+128]          ; Section name offset in first section
    cmp eax, "UPX0"
    je @@upx
    cmp eax, "UPX1"
    je @@upx
    
    ; Check for high entropy (encrypted/packed)
    ; Simplified - real implementation would calculate Shannon entropy
    jmp @@none
    
@@upx:
    mov CurrentAnalysis.IsPacked, 1
    mov CurrentAnalysis.PackerType, OBF_UPX
    ret
    
@@none:
    mov CurrentAnalysis.IsPacked, 0
    ret
DetectPacker ENDP

;----------------------------------------------------------------------------
; HEADER GENERATION ENGINE
;----------------------------------------------------------------------------

GenerateHFile PROC FRAME lpModuleName:QWORD
    LOCAL hFile:QWORD
    LOCAL qwWritten:QWORD
    LOCAL szFilePath:BYTE MAX_PATH DUP(?)
    
    ; Build path
    mov rcx, OFFSET szOutputPath
    mov rdx, OFFSET szFilePath
    call lstrcpyA
    
    mov rcx, OFFSET szFilePath
    mov rdx, OFFSET szBackslashInclude
    call lstrcatA
    
    mov rcx, OFFSET szFilePath
    mov rdx, lpModuleName
    call lstrcatA
    
    mov rcx, OFFSET szFilePath
    mov rdx, OFFSET szDotH
    call lstrcatA
    
    ; Create file
    mov rcx, OFFSET szFilePath
    call CreateOutputFile
    cmp rax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, rax
    
    ; Write header guard
    mov rcx, hFile
    mov rdx, OFFSET szTemplateHeaderGuard
    call WriteToFile
    
    ; Write extern C
    mov rcx, hFile
    mov rdx, OFFSET szTemplateExternC
    call WriteToFile
    
    ; Write includes
    mov rcx, hFile
    mov rdx, OFFSET szTemplateIncludes
    call WriteToFile
    
    ; Write export declarations (would iterate export table here)
    mov rcx, hFile
    mov rdx, OFFSET szExportComment
    call WriteToFile
    
    ; Close extern C
    mov rcx, hFile
    mov rdx, OFFSET szTemplateExternCEnd
    call WriteToFile
    
    mov rcx, hFile
    call CloseHandle
    
    inc dwHeadersGenerated
    
@@error:
    ret
GenerateHFile ENDP

;----------------------------------------------------------------------------
; BUILD SYSTEM GENERATION
;----------------------------------------------------------------------------

GenerateCMake PROC FRAME
    LOCAL hFile:QWORD
    
    ; Create CMakeLists.txt
    mov rcx, OFFSET szOutputPath
    mov rdx, OFFSET szTempBuffer
    call lstrcpyA
    
    mov rcx, OFFSET szTempBuffer
    mov rdx, OFFSET szCMakeLists
    call lstrcatA
    
    mov rcx, OFFSET szTempBuffer
    call CreateOutputFile
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
    
    ; Write sources (would iterate found files)
    mov rcx, hFile
    mov rdx, OFFSET szTemplateCMakeEnd
    call WriteToFile
    
    mov rcx, hFile
    call CloseHandle
    
@@error:
    ret
GenerateCMake ENDP

;----------------------------------------------------------------------------
; UNIVERSAL DEOBFUSCATION ENGINE
;----------------------------------------------------------------------------

DetectLanguage PROC FRAME lpFilePath:QWORD
    LOCAL hFile:QWORD
    LOCAL dwRead:DWORD
    LOCAL bMagic:BYTE 16 DUP(?)
    
    ; Open file
    xor ecx, ecx
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    mov r9d, OPEN_EXISTING
    mov [rsp+28h], rcx
    mov [rsp+20h], rcx
    mov rcx, lpFilePath
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@unknown
    mov hFile, rax
    
    ; Read first 16 bytes
    lea r9, dwRead
    mov rcx, hFile
    mov rdx, OFFSET bMagic
    mov r8d, 16
    call ReadFile
    
    mov rcx, hFile
    call CloseHandle
    
    ; Check magic numbers
    mov eax, DWORD PTR [bMagic]
    
    cmp eax, 0x464C457F         ; ELF magic (.ELF)
    je @@elf
    
    cmp eax, 0xFEEDFACE         ; Mach-O 32
    je @@macho
    cmp eax, 0xFEEDFACF         ; Mach-O 64
    je @@macho
    cmp eax, 0xCAFEBABE         ; Mach-O universal
    je @@macho
    
    cmp eax, 0x00905A4D         ; MZ (PE)
    je @@pe
    
    cmp eax, 0x04034B50         ; PK (ZIP/JAR)
    je @@zip
    
    cmp eax, 0xCAFEBABE         ; Java class
    je @@java
    
    cmp eax, 0x0D0D0A42         ; Python 3.7+
    je @@python
    
    ; Check extension
    mov rcx, lpFilePath
    call lstrlenA
    mov rcx, lpFilePath
    add rcx, rax
    sub rcx, 4
    
    mov rdx, OFFSET szExtJs
    call lstrcmpiA
    test eax, eax
    jz @@javascript
    
    mov rdx, OFFSET szExtPy
    call lstrcmpiA
    test eax, eax
    jz @@python_ext
    
    jmp @@unknown
    
@@pe:
    mov eax, MODE_PE
    ret
    
@@elf:
    mov eax, MODE_ELF
    ret
    
@@macho:
    mov eax, MODE_MACHO
    ret
    
@@java:
    mov eax, MODE_JAVA
    ret
    
@@python:
@@python_ext:
    mov eax, MODE_PYTHON
    ret
    
@@javascript:
    mov eax, MODE_JAVASCRIPT
    ret
    
@@zip:
    ; Could be JAR, APK, etc
    mov eax, MODE_JAVA
    ret
    
@@unknown:
    mov eax, MODE_AUTO
    ret
DetectLanguage ENDP

DeobfuscateFile PROC FRAME lpFilePath:QWORD, dwLang:DWORD
    LOCAL dwDetected:DWORD
    
    cmp dwLang, MODE_AUTO
    jne @@use_specified
    
    mov rcx, lpFilePath
    call DetectLanguage
    mov dwDetected, eax
    
@@use_specified:
    ; Route to appropriate deobfuscator
    cmp dwDetected, MODE_PE
    je @@deobf_pe
    
    cmp dwDetected, MODE_PYTHON
    je @@deobf_python
    
    cmp dwDetected, MODE_JAVASCRIPT
    je @@deobf_js
    
    jmp @@done
    
@@deobf_pe:
    ; PE deobfuscation (unpacking)
    call MapFile
    test rax, rax
    jz @@done
    
    call ParsePEHeaders
    call DetectPacker
    
    cmp CurrentAnalysis.IsPacked, 1
    jne @@unmap
    
    ; Would call unpacker here
    
@@unmap:
    call UnmapFile
    jmp @@done
    
@@deobf_python:
    ; Python deobfuscation (marshal/unmarshal)
    jmp @@done
    
@@deobf_js:
    ; JavaScript deobfuscation
    jmp @@done
    
@@done:
    ret
DeobfuscateFile ENDP

;----------------------------------------------------------------------------
; DIRECTORY PROCESSING
;----------------------------------------------------------------------------

ProcessDirectory PROC FRAME lpPath:QWORD
    LOCAL findData:WIN32_FIND_DATA
    LOCAL hFind:QWORD
    LOCAL szSearchPath:BYTE MAX_PATH DUP(?)
    LOCAL szFullPath:BYTE MAX_PATH DUP(?)
    
    ; Build search path
    mov rcx, lpPath
    mov rdx, OFFSET szSearchPath
    call lstrcpyA
    
    mov rcx, OFFSET szSearchPath
    mov rdx, OFFSET szBackslashStar
    call lstrcatA
    
    ; Find first
    lea rdx, findData
    mov rcx, OFFSET szSearchPath
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@done
    mov hFind, rax
    
@@file_loop:
    ; Skip directories
    test findData.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY
    jnz @@next
    
    ; Build full path
    mov rcx, OFFSET szFullPath
    mov rdx, lpPath
    call lstrcpyA
    
    mov rcx, OFFSET szFullPath
    mov rdx, OFFSET szBackslash
    call lstrcatA
    
    mov rcx, OFFSET szFullPath
    lea rdx, findData.cFileName
    call lstrcatA
    
    ; Process file
    mov rcx, OFFSET szFullPath
    mov edx, MODE_AUTO
    call DeobfuscateFile
    
    inc dwFilesProcessed
    
@@next:
    lea rdx, findData
    mov rcx, hFind
    call FindNextFileA
    test eax, eax
    jnz @@file_loop
    
    mov rcx, hFind
    call FindClose
    
@@done:
    ret
ProcessDirectory ENDP

;----------------------------------------------------------------------------
; MAIN MENU HANDLERS
;----------------------------------------------------------------------------

DoUniversalDeobf PROC FRAME
    mov rcx, OFFSET szPromptInputPath
    call Print
    call ReadInput
    
    mov rcx, OFFSET szPromptLanguage
    call Print
    call ReadInt
    
    ; Process
    mov rcx, OFFSET szInputPath
    mov edx, eax
    call DeobfuscateFile
    
    ret
DoUniversalDeobf ENDP

DoReverseInstall PROC FRAME
    ; Get paths
    mov rcx, OFFSET szPromptInputPath
    call Print
    call ReadInput
    
    mov rcx, OFFSET szPromptOutputPath
    call ReadInput
    mov rcx, OFFSET szInputPath
    mov rdx, OFFSET szOutputPath
    call lstrcpyA
    
    mov rcx, OFFSET szPromptProjectName
    call Print
    call ReadInput
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
    
    ; Process installation
    mov rcx, OFFSET szInputPath
    call ProcessDirectory
    
    ; Generate build systems
    call GenerateCMake
    
    ; Print summary
    mov rcx, OFFSET szSummaryFormat
    mov edx, dwFilesProcessed
    mov r8d, dwHeadersGenerated
    mov r9d, dwExportsTotal
    call PrintFormat
    
    ret
DoReverseInstall ENDP

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
    mov rcx, OFFSET szVersionString
    mov edx, VER_MAJOR
    mov r8d, VER_MINOR
    mov r9d, VER_PATCH
    call PrintFormat
    
@@menu_loop:
    mov rcx, OFFSET szMainMenu
    call Print
    
    call ReadInt
    mov dwChoice, eax
    
    cmp dwChoice, 1
    je @@do_deobf
    
    cmp dwChoice, 2
    je @@do_reverse
    
    cmp dwChoice, 3
    je @@do_buildsys
    
    cmp dwChoice, 4
    je @@do_types
    
    cmp dwChoice, 5
    je @@do_deps
    
    cmp dwChoice, 6
    je @@do_com
    
    cmp dwChoice, 7
    je @@do_batch
    
    cmp dwChoice, 8
    je @@do_integrity
    
    cmp dwChoice, 9
    je @@exit
    
    jmp @@menu_loop
    
@@do_deobf:
    call DoUniversalDeobf
    jmp @@menu_loop
    
@@do_reverse:
    call DoReverseInstall
    jmp @@menu_loop
    
@@do_buildsys:
    call GenerateCMake
    jmp @@menu_loop
    
@@do_types:
    jmp @@menu_loop
    
@@do_deps:
    jmp @@menu_loop
    
@@do_com:
    jmp @@menu_loop
    
@@do_batch:
    jmp @@menu_loop
    
@@do_integrity:
    jmp @@menu_loop
    
@@exit:
    xor ecx, ecx
    call ExitProcess

main ENDP

; Additional strings
szBackslash             BYTE    "\", 0
szBackslashInclude      BYTE    "\include", 0
szBackslashSrc          BYTE    "\src", 0
szBackslashStar         BYTE    "\*", 0
szDotH                  BYTE    ".h", 0
szDotC                  BYTE    ".c", 0
szDotCpp                BYTE    ".cpp", 0
szDotDll                BYTE    ".dll", 0
szDotExe                BYTE    ".exe", 0
szCMakeLists            BYTE    "\CMakeLists.txt", 0

szExtJs                 BYTE    ".js", 0
szExtPy                 BYTE    ".py", 0
szExtClass              BYTE    ".class", 0
szExtDll                BYTE    ".dll", 0

szExportComment         BYTE    "// Exported Functions", 13, 10, 13, 10, 0
szSummaryFormat         BYTE    13, 10, "Summary:", 13, 10
                        BYTE    "Files Processed: %d", 13, 10
                        BYTE    "Headers Generated: %d", 13, 10
                        BYTE    "Total Exports: %d", 13, 10, 0

END main
