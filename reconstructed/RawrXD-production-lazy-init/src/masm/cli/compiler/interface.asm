; ===============================================================================
; CLI Compiler Interface - Universal Language Support
; Command Line Interface for 48+ programming languages
; ===============================================================================

option casemap:none

include universal_language_constants.inc

; ===============================================================================
; EXTERNAL DEPENDENCIES
; ===============================================================================

extern InitializeUniversalCompiler:proc
extern DetectLanguageByExtension:proc
extern CompileFileWithLanguage:proc
extern GetLanguageNameById:proc
extern UniversalDispatch:proc
extern InitializeDispatcher:proc

; Windows API
extern GetCommandLineA:proc
extern CommandLineToArgvW:proc
extern LocalFree:proc
extern GetStdHandle:proc
extern WriteConsoleA:proc
extern SetConsoleTextAttribute:proc
extern GetConsoleScreenBufferInfo:proc
extern ExitProcess:proc

; ===============================================================================
; CONSTANTS
; ===============================================================================

STD_OUTPUT_HANDLE       equ -11
STD_ERROR_HANDLE        equ -12

; Console colors
COLOR_WHITE             equ 15
COLOR_GREEN             equ 10
COLOR_YELLOW            equ 14
COLOR_RED               equ 12
COLOR_CYAN              equ 11

; CLI Command types
CLI_CMD_COMPILE         equ 1
CLI_CMD_DETECT          equ 2
CLI_CMD_LIST            equ 3
CLI_CMD_HELP            equ 4
CLI_CMD_VERSION         equ 5
CLI_CMD_BATCH           equ 6

; ===============================================================================
; STRUCTURES
; ===============================================================================

CLICommand STRUCT
    cmdType             dword ?
    sourceFile          qword ?
    outputFile          qword ?
    languageId          dword ?
    flags               dword ?
CLICommand ENDS

; ===============================================================================
; DATA SEGMENT
; ===============================================================================

.data

; CLI Help text
szWelcome       db "RawrXD Universal Compiler CLI v1.0", 13, 10
                db "Supports 48+ programming languages", 13, 10, 13, 10, 0

szUsage         db "Usage: rawrxd-cli [command] [options]", 13, 10
                db "Commands:", 13, 10
                db "  compile <source> [output]  - Compile source file", 13, 10
                db "  detect <file>             - Detect language from file", 13, 10
                db "  list                      - List supported languages", 13, 10
                db "  batch <dir>              - Batch compile directory", 13, 10
                db "  help                      - Show this help", 13, 10
                db "  version                   - Show version info", 13, 10, 13, 10
                db "Examples:", 13, 10
                db "  rawrxd-cli compile main.c", 13, 10
                db "  rawrxd-cli compile main.cpp main.exe", 13, 10
                db "  rawrxd-cli detect script.py", 13, 10, 0

szVersion       db "RawrXD Universal Compiler CLI", 13, 10
                db "Version: 1.0.0", 13, 10
                db "Architecture: x86-64 MASM", 13, 10
                db "Supported Languages: 48", 13, 10
                db "Zero Dependencies: Yes", 13, 10, 0

szLanguageList  db "Supported Programming Languages:", 13, 10
                db "  Assembly (.asm)        Ada (.adb)           C (.c)", 13, 10
                db "  C++ (.cpp/.cxx)        C# (.cs)             Rust (.rs)", 13, 10
                db "  JavaScript (.js)       TypeScript (.ts)     Python (.py)", 13, 10
                db "  Go (.go)               Java (.java)         Kotlin (.kt)", 13, 10
                db "  Swift (.swift)         Dart (.dart)         Ruby (.rb)", 13, 10
                db "  PHP (.php)             Lua (.lua)           Perl (.pl)", 13, 10
                db "  R (.r)                 Scala (.scala)       Clojure (.clj)", 13, 10
                db "  Elixir (.ex)           Erlang (.erl)        Haskell (.hs)", 13, 10
                db "  OCaml (.ml)            F# (.fs)             Fortran (.f90)", 13, 10
                db "  Pascal (.pas)          Delphi (.pas)        COBOL (.cob)", 13, 10
                db "  Crystal (.cr)          Nim (.nim)           Zig (.zig)", 13, 10
                db "  V (.v)                 Odin (.odin)         Jai (.jai)", 13, 10
                db "  Julia (.jl)            MATLAB (.m)          Carbon (.carbon)", 13, 10
                db "  Cadence (.cdc)         Move (.move)         Motoko (.mo)", 13, 10
                db "  Solidity (.sol)        Vyper (.vy)          VB.NET (.vb)", 13, 10
                db "  LLVM IR (.ll)          WebAssembly (.wat)", 13, 10, 0

; Status messages
szCompileSuccess    db "[SUCCESS] Compilation completed successfully", 13, 10, 0
szCompileError      db "[ERROR] Compilation failed", 13, 10, 0
szDetected          db "[INFO] Detected language: ", 0
szInitializing      db "[INFO] Initializing compiler system...", 13, 10, 0
szInitialized       db "[SUCCESS] Compiler system ready", 13, 10, 0
szInitError         db "[ERROR] Failed to initialize compiler system", 13, 10, 0
szFileNotFound      db "[ERROR] Source file not found", 13, 10, 0
szLangNotSupported  db "[ERROR] Language not supported", 13, 10, 0
szBatchStarting     db "[INFO] Starting batch compilation...", 13, 10, 0
szBatchComplete     db "[SUCCESS] Batch compilation completed", 13, 10, 0

; Command line arguments
g_argc              dword 0
g_argv              qword 0
g_hStdOut           qword 0
g_hStdErr           qword 0

; Current console attributes
g_originalAttribs   word 0

.code

; ===============================================================================
; MAIN CLI ENTRY POINT
; ===============================================================================

; CLI main entry point
CLIMain PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 64
    
    ; Get console handles
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    mov     [g_hStdOut], rax
    
    mov     ecx, STD_ERROR_HANDLE
    call    GetStdHandle
    mov     [g_hStdErr], rax
    
    ; Show welcome message
    lea     rcx, szWelcome
    call    CLIPrintInfo
    
    ; Initialize compiler system
    lea     rcx, szInitializing
    call    CLIPrintInfo
    
    call    InitializeDispatcher
    test    rax, rax
    jz      .init_error
    
    call    InitializeUniversalCompiler
    test    rax, rax
    jz      .init_error
    
    lea     rcx, szInitialized
    call    CLIPrintSuccess
    
    ; Parse command line arguments
    call    ParseCommandLine
    test    rax, rax
    jz      .show_help
    
    ; Execute command
    call    ExecuteCLICommand
    jmp     .main_done
    
.init_error:
    lea     rcx, szInitError
    call    CLIPrintError
    mov     eax, 1
    jmp     .main_done
    
.show_help:
    call    ShowHelp
    xor     eax, eax
    
.main_done:
    ; Exit with return code
    mov     ecx, eax
    call    ExitProcess
CLIMain ENDP

; ===============================================================================
; COMMAND LINE PARSING
; ===============================================================================

; Parse command line arguments
; Returns: RAX = 1 success, 0 failure
ParseCommandLine PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 64
    
    ; Get command line
    call    GetCommandLineA
    mov     [rbp-8], rax
    
    ; For now, check first argument as command
    ; This is a simplified implementation
    mov     rax, 1
    
    add     rsp, 64
    pop     rbp
    ret
ParseCommandLine ENDP

; ===============================================================================
; COMMAND EXECUTION
; ===============================================================================

; Execute CLI command based on arguments
ExecuteCLICommand PROC
    push    rbp
    mov     rbp, rsp
    
    ; Simplified command execution
    ; In real implementation, parse actual arguments
    
    ; For demonstration, show language list
    call    ShowLanguageList
    
    mov     rax, 1
    pop     rbp
    ret
ExecuteCLICommand ENDP

; Compile file command
; RCX = source file, RDX = output file
CLICompileFile PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 64
    
    ; Save parameters
    mov     [rbp-8], rcx
    mov     [rbp-16], rdx
    
    ; Detect language
    mov     rcx, [rbp-8]
    call    DetectLanguageByExtension
    test    eax, eax
    jz      .lang_not_supported
    mov     [rbp-20], eax
    
    ; Show detected language
    mov     ecx, eax
    call    GetLanguageNameById
    mov     rcx, rax
    call    ShowDetectedLanguage
    
    ; Compile file
    mov     rcx, [rbp-8]        ; source
    mov     rdx, [rbp-16]       ; output
    mov     r8d, [rbp-20]       ; language ID
    call    CompileFileWithLanguage
    test    eax, eax
    jz      .compile_failed
    
    ; Show success
    lea     rcx, szCompileSuccess
    call    CLIPrintSuccess
    mov     rax, 1
    jmp     .compile_done
    
.lang_not_supported:
    lea     rcx, szLangNotSupported
    call    CLIPrintError
    xor     rax, rax
    jmp     .compile_done
    
.compile_failed:
    lea     rcx, szCompileError
    call    CLIPrintError
    xor     rax, rax
    
.compile_done:
    add     rsp, 64
    pop     rbp
    ret
CLICompileFile ENDP

; Detect language command
; RCX = file path
CLIDetectLanguage PROC
    push    rbp
    mov     rbp, rsp
    
    ; Detect language
    call    DetectLanguageByExtension
    test    eax, eax
    jz      .lang_unknown
    
    ; Get language name
    mov     ecx, eax
    call    GetLanguageNameById
    
    ; Show result
    mov     rcx, rax
    call    ShowDetectedLanguage
    
    mov     rax, 1
    jmp     .detect_done
    
.lang_unknown:
    lea     rcx, szLangNotSupported
    call    CLIPrintError
    xor     rax, rax
    
.detect_done:
    pop     rbp
    ret
CLIDetectLanguage ENDP

; ===============================================================================
; DISPLAY FUNCTIONS
; ===============================================================================

; Show help information
ShowHelp PROC
    push    rbp
    mov     rbp, rsp
    
    lea     rcx, szUsage
    call    CLIPrintInfo
    
    pop     rbp
    ret
ShowHelp ENDP

; Show version information
ShowVersion PROC
    push    rbp
    mov     rbp, rsp
    
    lea     rcx, szVersion
    call    CLIPrintInfo
    
    pop     rbp
    ret
ShowVersion ENDP

; Show supported languages
ShowLanguageList PROC
    push    rbp
    mov     rbp, rsp
    
    lea     rcx, szLanguageList
    call    CLIPrintInfo
    
    pop     rbp
    ret
ShowLanguageList ENDP

; Show detected language
; RCX = language name
ShowDetectedLanguage PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Save language name
    mov     [rbp-8], rcx
    
    ; Print prefix
    lea     rcx, szDetected
    call    CLIPrintInfo
    
    ; Print language name
    mov     rcx, [rbp-8]
    call    CLIPrintSuccess
    
    ; Print newline
    lea     rcx, szNewline
    call    CLIPrintInfo
    
    add     rsp, 32
    pop     rbp
    ret
ShowDetectedLanguage ENDP

; ===============================================================================
; CONSOLE OUTPUT FUNCTIONS
; ===============================================================================

; Print info message (cyan color)
; RCX = string pointer
CLIPrintInfo PROC
    push    rbp
    mov     rbp, rsp
    
    push    rcx
    mov     ecx, COLOR_CYAN
    call    SetConsoleColor
    pop     rcx
    
    call    CLIPrint
    call    RestoreConsoleColor
    
    pop     rbp
    ret
CLIPrintInfo ENDP

; Print success message (green color)
; RCX = string pointer
CLIPrintSuccess PROC
    push    rbp
    mov     rbp, rsp
    
    push    rcx
    mov     ecx, COLOR_GREEN
    call    SetConsoleColor
    pop     rcx
    
    call    CLIPrint
    call    RestoreConsoleColor
    
    pop     rbp
    ret
CLIPrintSuccess ENDP

; Print error message (red color)
; RCX = string pointer
CLIPrintError PROC
    push    rbp
    mov     rbp, rsp
    
    push    rcx
    mov     ecx, COLOR_RED
    call    SetConsoleColor
    pop     rcx
    
    call    CLIPrint
    call    RestoreConsoleColor
    
    pop     rbp
    ret
CLIPrintError ENDP

; Print warning message (yellow color)
; RCX = string pointer
CLIPrintWarning PROC
    push    rbp
    mov     rbp, rsp
    
    push    rcx
    mov     ecx, COLOR_YELLOW
    call    SetConsoleColor
    pop     rcx
    
    call    CLIPrint
    call    RestoreConsoleColor
    
    pop     rbp
    ret
CLIPrintWarning ENDP

; Print string to console
; RCX = string pointer
CLIPrint PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    
    ; Calculate string length
    mov     rax, rcx
    call    StrLen
    mov     r9, rax         ; length
    
    ; Write to console
    mov     rcx, [g_hStdOut]    ; console handle
    mov     rdx, rax            ; string pointer
    mov     r8, r9              ; length
    mov     qword ptr [rsp+32], 0   ; bytes written
    call    WriteConsoleA
    
    add     rsp, 32
    pop     rbp
    ret
CLIPrint ENDP

; ===============================================================================
; UTILITY FUNCTIONS
; ===============================================================================

; Set console text color
; ECX = color attribute
SetConsoleColor PROC
    push    rbp
    mov     rbp, rsp
    
    ; Save current attributes if not already saved
    cmp     word ptr [g_originalAttribs], 0
    jne     .set_color
    
    ; Get current attributes
    ; (Implementation simplified for demo)
    mov     word ptr [g_originalAttribs], COLOR_WHITE
    
.set_color:
    mov     rdx, [g_hStdOut]
    ; SetConsoleTextAttribute call would go here
    
    pop     rbp
    ret
SetConsoleColor ENDP

; Restore original console color
RestoreConsoleColor PROC
    push    rbp
    mov     rbp, rsp
    
    mov     cx, [g_originalAttribs]
    ; SetConsoleTextAttribute call would go here
    
    pop     rbp
    ret
RestoreConsoleColor ENDP

; Calculate string length
; RCX = string pointer
; Returns: RAX = length
StrLen PROC
    push    rbp
    mov     rbp, rsp
    
    mov     rax, rcx
    xor     rdx, rdx
    
.count_loop:
    cmp     byte ptr [rax + rdx], 0
    je      .count_done
    inc     rdx
    jmp     .count_loop
    
.count_done:
    mov     rax, rdx
    pop     rbp
    ret
StrLen ENDP

.data
szNewline   db 13, 10, 0

; ===============================================================================
; EXPORTS
; ===============================================================================

PUBLIC CLIMain
PUBLIC CLICompileFile
PUBLIC CLIDetectLanguage
PUBLIC ShowHelp
PUBLIC ShowVersion
PUBLIC ShowLanguageList

END