; =====================================================================
; REVERSE ENGINEERING MODULE - GHIDRA-LIKE FEATURES
; Pure MASM64 implementation for binary analysis
; =====================================================================

PUBLIC DisassembleX64, AnalyzePEHeader, ParseELFHeader, AnalyzeMachO
PUBLIC HexDumpMemory, FindStrings, AnalyzeCodePatterns, DetectPacker
PUBLIC CalculateEntropy, ExtractResources, AnalyzeImportsExports
PUBLIC DetectAntiDebug, AnalyzeObfuscation, ExtractSections

EXTERN GlobalAlloc:PROC
EXTERN GlobalFree:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN WriteConsoleA:PROC
EXTERN GetStdHandle:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrlenA:PROC
EXTERN wsprintfA:PROC

; =====================================================================
; CONSTANTS
; =====================================================================

GENERIC_READ        EQU 80000000h
GENERIC_WRITE       EQU 40000000h
CREATE_ALWAYS       EQU 2
OPEN_EXISTING       EQU 3
FILE_SHARE_READ     EQU 1
FILE_ATTRIBUTE_NORMAL EQU 80h
INVALID_HANDLE_VALUE EQU -1
PAGE_READONLY       EQU 2
FILE_MAP_READ       EQU 4
STD_OUTPUT_HANDLE   EQU -11

; PE Header Constants
PE_SIGNATURE        EQU 00004550h
ELF_SIGNATURE       EQU 464C457Fh
MACHO_SIGNATURE     EQU FEEDFACEh
MACHO64_SIGNATURE   EQU FEEDFACFh

; Analysis Constants
MAX_SECTION_NAME    EQU 8
MAX_IMPORT_NAME     EQU 256
MAX_STRING_LENGTH  EQU 256
ENTROPY_THRESHOLD   EQU 7.0

; =====================================================================
; STRUCTURES
; =====================================================================

PE_HEADER STRUCT
    signature       DWORD ?
    machine         WORD ?
    numberOfSections WORD ?
    timeDateStamp   DWORD ?
    pointerToSymbolTable DWORD ?
    numberOfSymbols DWORD ?
    sizeOfOptionalHeader WORD ?
    characteristics WORD ?
PE_HEADER ENDS

SECTION_HEADER STRUCT
    name            BYTE MAX_SECTION_NAME DUP(?)
    virtualSize     DWORD ?
    virtualAddress  DWORD ?
    sizeOfRawData   DWORD ?
    pointerToRawData DWORD ?
    pointerToRelocations DWORD ?
    pointerToLinenumbers DWORD ?
    numberOfRelocations WORD ?
    numberOfLinenumbers WORD ?
    characteristics DWORD ?
SECTION_HEADER ENDS

IMPORT_DESCRIPTOR STRUCT
    originalFirstThunk DWORD ?
    timeDateStamp   DWORD ?
    forwarderChain   DWORD ?
    name            DWORD ?
    firstThunk      DWORD ?
IMPORT_DESCRIPTOR ENDS

ANALYSIS_RESULT STRUCT
    fileType        DWORD ?
    isPacked        DWORD ?
    entropy         REAL8 ?
    hasAntiDebug    DWORD ?
    isObfuscated    DWORD ?
    stringCount     DWORD ?
    sectionCount    DWORD ?
    importCount     DWORD ?
    exportCount     DWORD ?
ANALYSIS_RESULT ENDS

; =====================================================================
; DATA
; =====================================================================

.data
szPEHeader         db "PE Header Analysis",0
szELFHeader         db "ELF Header Analysis",0
szMachOHeader       db "Mach-O Header Analysis",0
szDisassembly      db "Disassembly",0
szHexDump          db "Hex Dump",0
szStringAnalysis   db "String Analysis",0
szCodePatterns     db "Code Pattern Analysis",0
szPackerDetection  db "Packer Detection",0
szEntropyAnalysis  db "Entropy Analysis",0
szResourceExtract  db "Resource Extraction",0
szImportAnalysis   db "Import Analysis",0
szExportAnalysis   db "Export Analysis",0
szAntiDebugDetect  db "Anti-Debug Detection",0
szObfuscationDetect db "Obfuscation Detection",0

; File type strings
szPE               db "PE (Portable Executable)",0
szELF              db "ELF (Executable and Linkable Format)",0
szMachO            db "Mach-O (macOS)",0
szUnknown          db "Unknown Format",0

; Packer signatures
szUPX              db "UPX",0
szASPack           db "ASPack",0
szPECompact        db "PECompact",0
szThemida          db "Themida",0

; Anti-debug patterns
szIsDebuggerPresent db "IsDebuggerPresent",0
szCheckRemoteDebugger db "CheckRemoteDebuggerPresent",0
szNtQueryInfo      db "NtQueryInformationProcess",0

; =====================================================================
; CODE
; =====================================================================

.code

; =====================================================================
; DisassembleX64 - Simple x64 disassembler
; RCX = buffer address, RDX = buffer size, R8 = output buffer
; =====================================================================
DisassembleX64 PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40h
    
    mov     rbx, rcx        ; input buffer
    mov     rsi, rdx        ; size
    mov     rdi, r8         ; output buffer
    
    xor     r9, r9          ; instruction count
    
disasm_loop:
    cmp     r9, rsi
    jae     disasm_done
    
    mov     al, byte ptr [rbx + r9]
    
    ; Simple disassembly - extend with proper x64 decoding
    cmp     al, 0xC3        ; RET
    je      disasm_ret
    cmp     al, 0x90        ; NOP
    je      disasm_nop
    cmp     al, 0x48        ; REX.W prefix
    je      disasm_rex
    
    ; Add more instruction decoding here
    jmp     disasm_next
    
disasm_ret:
    lea     rcx, [rdi]
    lea     rdx, [str$("ret")]
    call    lstrcpyA
    add     rdi, 4
    jmp     disasm_next
    
disasm_nop:
    lea     rcx, [rdi]
    lea     rdx, [str$("nop")]
    call    lstrcpyA
    add     rdi, 4
    jmp     disasm_next
    
disasm_rex:
    lea     rcx, [rdi]
    lea     rdx, [str$("rex.w")]
    call    lstrcpyA
    add     rdi, 6
    
disasm_next:
    inc     r9
    jmp     disasm_loop
    
disasm_done:
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
DisassembleX64 ENDP

; =====================================================================
; AnalyzePEHeader - PE file analysis
; RCX = file path
; Returns: RAX = analysis result structure
; =====================================================================
AnalyzePEHeader PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 60h
    
    mov     rbx, rcx        ; file path
    
    ; Open file
    mov     rcx, rbx
    mov     rdx, GENERIC_READ
    xor     r8, r8
    xor     r9, r9
    mov     qword ptr [rsp+20h], OPEN_EXISTING
    mov     qword ptr [rsp+28h], 0
    mov     qword ptr [rsp+30h], 0
    call    CreateFileA
    cmp     rax, INVALID_HANDLE_VALUE
    je      analyze_error
    mov     rsi, rax        ; file handle
    
    ; Get file size
    mov     rcx, rsi
    xor     rdx, rdx
    call    GetFileSize
    mov     rdi, rax
    
    ; Create mapping
    mov     rcx, rsi
    xor     rdx, rdx
    mov     r8, PAGE_READONLY
    xor     r9, r9
    mov     qword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    CreateFileMappingA
    test    rax, rax
    jz      analyze_close
    mov     r8, rax         ; mapping handle
    
    ; Map view
    mov     rcx, r8
    mov     rdx, FILE_MAP_READ
    xor     r9, r9
    mov     qword ptr [rsp+20h], 0
    mov     qword ptr [rsp+28h], 0
    call    MapViewOfFile
    test    rax, rax
    jz      analyze_unmap
    mov     r9, rax         ; mapped view
    
    ; Check DOS header
    mov     eax, dword ptr [r9]
    cmp     ax, 5A4Dh       ; MZ signature
    jne     analyze_unview
    
    ; Get PE header offset
    mov     eax, dword ptr [r9 + 3Ch]  ; e_lfanew
    add     rax, r9
    
    ; Check PE signature
    mov     edx, dword ptr [rax]
    cmp     edx, PE_SIGNATURE
    jne     analyze_unview
    
    ; Allocate result structure
    mov     rcx, GPTR
    mov     rdx, SIZEOF ANALYSIS_RESULT
    call    GlobalAlloc
    mov     rbx, rax
    
    ; Set file type
    mov     dword ptr [rbx + ANALYSIS_RESULT.fileType], 1  ; PE
    
    ; Analyze sections
    mov     cx, word ptr [rax + PE_HEADER.numberOfSections]
    mov     word ptr [rbx + ANALYSIS_RESULT.sectionCount], cx
    
    ; TODO: Add more PE analysis (imports, exports, resources, etc.)
    
analyze_unview:
    mov     rcx, r9
    call    UnmapViewOfFile
    
analyze_unmap:
    mov     rcx, r8
    call    CloseHandle
    
analyze_close:
    mov     rcx, rsi
    call    CloseHandle
    mov     rax, rbx
    jmp     analyze_done
    
analyze_error:
    xor     rax, rax
    
analyze_done:
    add     rsp, 60h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
AnalyzePEHeader ENDP

; =====================================================================
; ParseELFHeader - ELF file analysis
; RCX = file path
; =====================================================================
ParseELFHeader PROC
    ; Similar structure to AnalyzePEHeader but for ELF
    ret
ParseELFHeader ENDP

; =====================================================================
; AnalyzeMachO - Mach-O file analysis
; RCX = file path
; =====================================================================
AnalyzeMachO PROC
    ; Similar structure to AnalyzePEHeader but for Mach-O
    ret
AnalyzeMachO ENDP

; =====================================================================
; HexDumpMemory - Hexadecimal memory dump
; RCX = memory address, RDX = size, R8 = output buffer
; =====================================================================
HexDumpMemory PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40h
    
    mov     rbx, rcx        ; memory address
    mov     rsi, rdx        ; size
    mov     rdi, r8         ; output buffer
    
    xor     r9, r9          ; offset
    
hexdump_loop:
    cmp     r9, rsi
    jae     hexdump_done
    
    ; Format: "00000000: 00 00 00 00 00 00 00 00  ................"
    lea     rcx, [rdi]
    lea     rdx, [str$("%08X: ")]
    mov     r8, r9
    call    wsprintfA
    add     rdi, 10
    
    ; Hex bytes
    mov     r10, 0
hex_bytes:
    cmp     r10, 8
    jae     hex_ascii
    mov     al, byte ptr [rbx + r9 + r10]
    lea     rcx, [rdi]
    lea     rdx, [str$("%02X ")]
    movzx   r8, al
    call    wsprintfA
    add     rdi, 3
    inc     r10
    jmp     hex_bytes
    
hex_ascii:
    lea     rcx, [rdi]
    lea     rdx, [str$(" ")]
    call    lstrcpyA
    add     rdi, 1
    
    ; ASCII representation
    mov     r10, 0
ascii_chars:
    cmp     r10, 8
    jae     hex_next
    mov     al, byte ptr [rbx + r9 + r10]
    cmp     al, 20h
    jb      ascii_dot
    cmp     al, 7Eh
    ja      ascii_dot
    mov     byte ptr [rdi], al
    jmp     ascii_next
ascii_dot:
    mov     byte ptr [rdi], '.'
ascii_next:
    inc     rdi
    inc     r10
    jmp     ascii_chars
    
hex_next:
    lea     rcx, [rdi]
    lea     rdx, [str$(13,10)]
    call    lstrcpyA
    add     rdi, 2
    add     r9, 8
    jmp     hexdump_loop
    
hexdump_done:
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
HexDumpMemory ENDP

; =====================================================================
; FindStrings - Extract ASCII strings from binary
; RCX = buffer, RDX = size, R8 = output buffer
; =====================================================================
FindStrings PROC
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 40h
    
    mov     rbx, rcx        ; buffer
    mov     rsi, rdx        ; size
    mov     rdi, r8         ; output
    
    xor     r9, r9          ; position
    xor     r10, r10        ; string start
    xor     r11, r11        ; string length
    
strings_loop:
    cmp     r9, rsi
    jae     strings_done
    
    mov     al, byte ptr [rbx + r9]
    cmp     al, 20h         ; space
    jb      check_string
    cmp     al, 7Eh         ; tilde
    ja      check_string
    
    ; Valid ASCII character
    cmp     r11, 0
    jne     string_continue
    mov     r10, r9         ; start of string
    
string_continue:
    inc     r11
    jmp     string_next
    
check_string:
    cmp     r11, 4          ; minimum string length
    jb      string_reset
    
    ; Copy string to output
    mov     r12, r10
string_copy:
    mov     al, byte ptr [rbx + r12]
    mov     byte ptr [rdi], al
    inc     rdi
    inc     r12
    dec     r11
    jnz     string_copy
    
    lea     rcx, [rdi]
    lea     rdx, [str$(13,10)]
    call    lstrcpyA
    add     rdi, 2
    
string_reset:
    xor     r11, r11
    
string_next:
    inc     r9
    jmp     strings_loop
    
strings_done:
    add     rsp, 40h
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
FindStrings ENDP

; =====================================================================
; AnalyzeCodePatterns - Detect code patterns
; RCX = buffer, RDX = size
; =====================================================================
AnalyzeCodePatterns PROC
    ; Detect common patterns like function prologues, API calls, etc.
    ret
AnalyzeCodePatterns ENDP

; =====================================================================
; DetectPacker - Detect executable packers
; RCX = buffer, RDX = size
; =====================================================================
DetectPacker PROC
    ; Check for UPX, ASPack, PECompact signatures
    ret
DetectPacker ENDP

; =====================================================================
; CalculateEntropy - Calculate Shannon entropy
; RCX = buffer, RDX = size
; Returns: XMM0 = entropy value
; =====================================================================
CalculateEntropy PROC
    ; Calculate byte frequency and entropy
    ret
CalculateEntropy ENDP

; =====================================================================
; ExtractResources - Extract PE resources
; RCX = file path
; =====================================================================
ExtractResources PROC
    ; Extract icons, bitmaps, strings from PE files
    ret
ExtractResources ENDP

; =====================================================================
; AnalyzeImportsExports - Analyze imports and exports
; RCX = file path
; =====================================================================
AnalyzeImportsExports PROC
    ; Parse import/export tables
    ret
AnalyzeImportsExports ENDP

; =====================================================================
; DetectAntiDebug - Detect anti-debugging techniques
; RCX = buffer, RDX = size
; =====================================================================
DetectAntiDebug PROC
    ; Check for IsDebuggerPresent, NtQueryInformationProcess, etc.
    ret
DetectAntiDebug ENDP

; =====================================================================
; AnalyzeObfuscation - Detect code obfuscation
; RCX = buffer, RDX = size
; =====================================================================
AnalyzeObfuscation PROC
    ; Detect VMProtect, Themida, etc.
    ret
AnalyzeObfuscation ENDP

; =====================================================================
; ExtractSections - Extract and analyze PE sections
; RCX = file path
; =====================================================================
ExtractSections PROC
    ; Extract .text, .data, .rsrc sections
    ret
ExtractSections ENDP

; String helper macro
str$ MACRO t:VARARG
    LOCAL s
    .const
    s db t,0
    .code
    EXITM <OFFSET s>
ENDM

END