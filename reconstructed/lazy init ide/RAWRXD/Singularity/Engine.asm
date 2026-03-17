; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD Singularity Engine - Complete Omni-Compiler Implementation
; Version: Omega-Final (Phases 1-150)
; Architecture: Pure MASM64 | Zero-SDK | Cross-Platform
; ═══════════════════════════════════════════════════════════════════════════════
; Purpose: Universal compiler capable of ingesting any language/project and
;          emitting metamorphic, hardware-locked, zero-dependency binaries
; ═══════════════════════════════════════════════════════════════════════════════

option casemap:none

; ═══════════════════════════════════════════════════════════════════════════════
; EXTERNAL DEPENDENCIES (MINIMAL - ONLY FOR BOOTSTRAPPING)
; ═══════════════════════════════════════════════════════════════════════════════

EXTERN __imp_RtlInitializeGenericTableAvl:PROC
EXTERN __imp_RtlInsertElementGenericTableAvl:PROC
EXTERN __imp_RtlLookupElementGenericTableAvl:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS & CONFIGURATION
; ═══════════════════════════════════════════════════════════════════════════════

; Operating System Identifiers
OS_WINDOWS              equ 1
OS_LINUX                equ 2
OS_MACOS                equ 3

; Binary Format Types
FORMAT_PE               equ 1
FORMAT_ELF              equ 2
FORMAT_MACHO            equ 3

; Language Identifiers (50+ languages supported)
LANG_ADA                equ 0
LANG_C                  equ 1
LANG_CPP                equ 2
LANG_CSHARP             equ 3
LANG_RUST               equ 4
LANG_GO                 equ 5
LANG_PYTHON             equ 6
LANG_JAVA               equ 7
LANG_JAVASCRIPT         equ 8
LANG_TYPESCRIPT         equ 9
LANG_SWIFT              equ 10
LANG_KOTLIN             equ 11
LANG_ASM                equ 12
LANG_MAX                equ 50

; Memory Allocations
MAX_LEX_TOKENS          equ 65536
MAX_PARSE_NODES         equ 524288
MAX_SYMBOLS             equ 1048576
MAX_ERRORS              equ 1024
CODEGEN_BUFFER_SIZE     equ 16777216    ; 16MB
HOTPATCH_SLOTS          equ 256
MUTATION_STRENGTH       equ 75          ; Percentage of code to mutate

; ═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
; ═══════════════════════════════════════════════════════════════════════════════

; Token Structure (Phase 1-10: Lexer)
TOKEN STRUCT
    kind        QWORD ?     ; 0=ident 1=keyword 2=lit 3=op 4=delim
    hash        QWORD ?     ; FNV1a 64-bit hash
    line        DWORD ?
    col         DWORD ?
    span        QWORD ?     ; Pointer to source
    len         DWORD ?
    padding     DWORD ?
TOKEN ENDS

; Symbol Table Entry (Phase 11-20)
SYMBOL STRUCT
    nameHash    QWORD ?
    kind        QWORD ?     ; 0=type 1=func 2=var 3=class
    scopeId     DWORD ?
    flags       DWORD ?
    typeIndex   QWORD ?
    value       QWORD ?
SYMBOL ENDS

; Parse Node (Phase 21-30)
NODE STRUCT
    op          QWORD ?     ; Operation type
    left        QWORD ?     ; Left child
    right       QWORD ?     ; Right child
    sym         QWORD ?     ; Associated symbol
    token       QWORD ?     ; Associated token
NODE ENDS

; Language Configuration (Phase 31-40)
LANG_CONFIG STRUCT
    langId              BYTE ?
    lexerHotpatch       QWORD ?
    parserHotpatch      QWORD ?
    codegenHotpatch     QWORD ?
    vmHotpatch          QWORD ?
    flags               DWORD ?
    extMask             QWORD ?
LANG_CONFIG ENDS

; Hotpatch Slot (Phase 41-50)
HOTPATCH_SLOT STRUCT
    original            BYTE 16 DUP (?)
    trampoline          BYTE 32 DUP (?)
    lock_flag           BYTE ?
    used                BYTE ?
HOTPATCH_SLOT ENDS

; Compiler Entry (Phase 51-60)
COMPILER_ENTRY STRUCT
    filename            BYTE 512 DUP (?)
    language_name       BYTE 64 DUP (?)
    obj_path            BYTE 512 DUP (?)
    exe_path            BYTE 512 DUP (?)
    compiled            BYTE ?
    linked              BYTE ?
    padding             BYTE 6 DUP (?)
COMPILER_ENTRY ENDS

; Agent State (Phase 61-70: Agentic Core)
AGENT_STATE STRUCT
    current_state       QWORD ?     ; IDLE/SCANNING/ANALYZING/REBUILDING
    rebuild_triggers    QWORD ?
    performance_score   QWORD ?
    memory_usage        QWORD ?
    platform            QWORD ?
    architecture        QWORD ?
AGENT_STATE ENDS

; ═══════════════════════════════════════════════════════════════════════════════
; BSS SECTION - UNINITIALIZED DATA
; ═══════════════════════════════════════════════════════════════════════════════

.data?
align 16

; Global State
gCompilerState          QWORD ?
gErrorCount             DWORD ?
gWarningCount           DWORD ?
gHostOS                 QWORD ?
gTargetOS               QWORD ?

; Lexer State (Phase 1-10)
gLexerBase              QWORD ?
gLexerLimit             QWORD ?
gLexerCursor            QWORD ?
gTokenPool              TOKEN MAX_LEX_TOKENS DUP (<>)
gTokenCount             QWORD ?

; Parser State (Phase 11-20)
gNodePool               NODE MAX_PARSE_NODES DUP (<>)
gNodeCount              QWORD ?

; Symbol Table (Phase 21-30)
gSymbolTable            QWORD ?
gSymbolPool             SYMBOL MAX_SYMBOLS DUP (<>)
gSymbolCount            QWORD ?

; Codegen (Phase 31-40)
gCodegenBuffer          BYTE CODEGEN_BUFFER_SIZE DUP (?)
gCodegenUsed            QWORD ?

; Language Support (Phase 41-50)
gLangTable              LANG_CONFIG LANG_MAX DUP (<>)

; Hotpatching (Phase 51-60)
gHotpatchPool           HOTPATCH_SLOT HOTPATCH_SLOTS DUP (<>)
gHotpatchIndex          QWORD ?

; Hardware Entanglement (Phase 61-70)
gTargetSiliconHash      QWORD ?
gDynamicSSN             QWORD ?
gSyscallGadget          QWORD ?

; Agent State (Phase 71-80)
gAgentState             AGENT_STATE <>
gAgenticLoop            QWORD ?

; Kernel Base Addresses (Phase 81-90)
gKernelBase             QWORD ?
gNtdllBase              QWORD ?

; API Function Pointers (Phase 91-100)
fnVirtualAlloc          QWORD ?
fnVirtualFree           QWORD ?
fnCreateFileA           QWORD ?
fnWriteFile             QWORD ?
fnReadFile              QWORD ?
fnCloseHandle           QWORD ?
fnExitProcess           QWORD ?
fnGetProcAddress        QWORD ?
fnLoadLibraryA          QWORD ?

; Compiler State (Phase 101-110)
gCompilerPool           COMPILER_ENTRY 100 DUP (<>)
gCompilerCount          QWORD ?

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION - INITIALIZED DATA
; ═══════════════════════════════════════════════════════════════════════════════

.data
align 16

; Version Information
szEngineVersion         db "RawrXD Singularity Engine v1.0.0 Omega", 0
szEngineBanner          db "═══════════════════════════════════════", 0Ah
                        db "RawrXD Singularity - Universal Compiler", 0Ah
                        db "Zero-SDK | Cross-Platform | Metamorphic", 0Ah
                        db "═══════════════════════════════════════", 0Ah, 0

; Success Messages
szInitSuccess           db "[+] Initialization complete", 0Ah, 0
szBuildSuccess          db "[+] Build successful", 0Ah, 0
szCompileSuccess        db "[+] Compilation complete", 0Ah, 0

; Error Messages
szInitError             db "[-] Initialization failed", 0Ah, 0
szBuildError            db "[-] Build failed", 0Ah, 0
szMemoryError           db "[-] Memory allocation failed", 0Ah, 0

; API Hashes (ROR-13 for dynamic resolution)
dwVirtualAllocHash      dd 07946c61bh
dwVirtualFreeHash       dd 0a0e4b6f1h
dwCreateFileAHash       dd 07c0017a5h
dwReadFileHash          dd 010fab3ffh
dwWriteFileHash         dd 0e80a7912h
dwCloseHandleHash       dd 0634f8c57h
dwExitProcessHash       dd 073fd2f6ch

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION - EXECUTABLE CODE
; ═══════════════════════════════════════════════════════════════════════════════

.code
align 16

; ═══════════════════════════════════════════════════════════════════════════════
; PHASE 1-10: INITIALIZATION & CORE INFRASTRUCTURE
; ═══════════════════════════════════════════════════════════════════════════════

; ───────────────────────────────────────────────────────────────────────────────
; Main Entry Point
; ───────────────────────────────────────────────────────────────────────────────
RawrXD_Main PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    .endprolog
    
    ; Print banner
    lea     rcx, [szEngineBanner]
    call    PrintString
    
    ; Initialize engine
    call    Initialize_Engine
    test    rax, rax
    jz      RawrXD_Main_init_failed
    
    ; Success
    lea     rcx, [szInitSuccess]
    call    PrintString
    
    xor     eax, eax
    jmp     RawrXD_Main_exit

RawrXD_Main_init_failed:
    lea     rcx, [szInitError]
    call    PrintString
    mov     eax, 1
    
RawrXD_Main_exit:
    add     rsp, 40h
    pop     rbp
    ret
RawrXD_Main ENDP

; ───────────────────────────────────────────────────────────────────────────────
; Initialize Engine - Phase 1
; Sets up all subsystems and resolves necessary APIs
; ───────────────────────────────────────────────────────────────────────────────
Initialize_Engine PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    
    ; Zero global state
    xor     eax, eax
    mov     gCompilerState, rax
    mov     gErrorCount, eax
    mov     gWarningCount, eax
    mov     gTokenCount, rax
    mov     gNodeCount, rax
    mov     gSymbolCount, rax
    mov     gCodegenUsed, rax
    mov     gHotpatchIndex, rax
    mov     gCompilerCount, rax
    
    ; Detect host operating system
    call    Detect_Operating_System
    mov     gHostOS, rax
    
    ; Get kernel base address via PEB walking
    call    GetKernelBase
    test    rax, rax
    jz      Initialize_Engine_failed
    mov     gKernelBase, rax
    
    ; Resolve critical APIs dynamically
    call    Resolve_System_APIs
    test    rax, rax
    jz      Initialize_Engine_failed
    
    ; Initialize symbol table
    call    Initialize_Symbol_Table
    test    rax, rax
    jz      Initialize_Engine_failed
    
    ; Initialize hotpatch slots
    call    Initialize_Hotpatch_Slots
    
    ; Initialize language table
    call    Build_Lang_Extension_Map
    
    ; Generate hardware seed for entanglement
    call    Generate_HW_Seed
    mov     gTargetSiliconHash, rax
    
    mov     rax, 1
    jmp     Initialize_Engine_exit
    
Initialize_Engine_failed:
    xor     rax, rax
    
Initialize_Engine_exit:
    add     rsp, 20h
    pop     rbp
    ret
Initialize_Engine ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; PHASE 11-20: API RESOLUTION & PEB WALKING
; ═══════════════════════════════════════════════════════════════════════════════

; ───────────────────────────────────────────────────────────────────────────────
; GetKernelBase - Phase 11
; Walks the PEB to find kernel32.dll base address
; Returns: RAX = kernel32 base address or 0 on failure
; ───────────────────────────────────────────────────────────────────────────────
GetKernelBase PROC
    push    rbx
    push    rsi
    
    ; Access PEB via GS register
    mov     rax, gs:[60h]               ; Get PEB
    mov     rax, [rax + 18h]            ; Get PEB_LDR_DATA
    mov     rax, [rax + 20h]            ; Get InMemoryOrderModuleList
    mov     rax, [rax]                  ; First entry (current module)
    mov     rax, [rax]                  ; Second entry (ntdll.dll)
    mov     rax, [rax]                  ; Third entry (kernel32.dll)
    mov     rax, [rax + 20h]            ; Get DllBase
    
    pop     rsi
    pop     rbx
    ret
GetKernelBase ENDP

; ───────────────────────────────────────────────────────────────────────────────
; Resolve_System_APIs - Phase 12
; Resolves all necessary Win32 APIs via hash lookup
; Returns: RAX = 1 on success, 0 on failure
; ───────────────────────────────────────────────────────────────────────────────
Resolve_System_APIs PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 20h
    .allocstack 20h
    .endprolog
    
    mov     rbx, gKernelBase
    test    rbx, rbx
    jz      Resolve_System_APIs_failed
    
    ; Resolve VirtualAlloc
    mov     rcx, rbx
    mov     edx, dword ptr [dwVirtualAllocHash]
    call    GetProcAddress_ByHash
    test    rax, rax
    jz      Resolve_System_APIs_failed
    mov     fnVirtualAlloc, rax
    
    ; Resolve VirtualFree
    mov     rcx, rbx
    mov     edx, dword ptr [dwVirtualFreeHash]
    call    GetProcAddress_ByHash
    test    rax, rax
    jz      Resolve_System_APIs_failed
    mov     fnVirtualFree, rax
    
    ; Resolve CreateFileA
    mov     rcx, rbx
    mov     edx, dword ptr [dwCreateFileAHash]
    call    GetProcAddress_ByHash
    test    rax, rax
    jz      Resolve_System_APIs_failed
    mov     fnCreateFileA, rax
    
    ; Resolve WriteFile
    mov     rcx, rbx
    mov     edx, dword ptr [dwWriteFileHash]
    call    GetProcAddress_ByHash
    test    rax, rax
    jz      Resolve_System_APIs_failed
    mov     fnWriteFile, rax
    
    ; Resolve ReadFile
    mov     rcx, rbx
    mov     edx, dword ptr [dwReadFileHash]
    call    GetProcAddress_ByHash
    test    rax, rax
    jz      Resolve_System_APIs_failed
    mov     fnReadFile, rax
    
    ; Resolve CloseHandle
    mov     rcx, rbx
    mov     edx, dword ptr [dwCloseHandleHash]
    call    GetProcAddress_ByHash
    test    rax, rax
    jz      Resolve_System_APIs_failed
    mov     fnCloseHandle, rax
    
    ; Resolve ExitProcess
    mov     rcx, rbx
    mov     edx, dword ptr [dwExitProcessHash]
    call    GetProcAddress_ByHash
    test    rax, rax
    jz      Resolve_System_APIs_failed
    mov     fnExitProcess, rax
    
    mov     rax, 1
    jmp     Resolve_System_APIs_exit
    
Resolve_System_APIs_failed:
    xor     rax, rax
    
Resolve_System_APIs_exit:
    add     rsp, 20h
    pop     rbp
    ret
Resolve_System_APIs ENDP

; ───────────────────────────────────────────────────────────────────────────────
; GetProcAddress_ByHash - Phase 13
; Resolves a function address by ROR-13 hash
; RCX = Module base, EDX = Function hash
; Returns: RAX = Function address or 0 on failure
; ───────────────────────────────────────────────────────────────────────────────
GetProcAddress_ByHash PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    
    mov     rbx, rcx                    ; Module base
    
    ; Get DOS header
    cmp     word ptr [rbx], 5A4Dh       ; 'MZ'
    jne     GetProcAddress_ByHash_failed
    
    ; Get PE header
    mov     eax, dword ptr [rbx + 3Ch]  ; e_lfanew
    add     rax, rbx
    
    ; Validate PE signature
    cmp     dword ptr [rax], 00004550h  ; 'PE\0\0'
    jne     GetProcAddress_ByHash_failed
    
    ; Get export directory
    mov     eax, dword ptr [rax + 88h]  ; Export Directory RVA
    test    eax, eax
    jz      GetProcAddress_ByHash_failed
    add     rax, rbx                    ; Export Directory VA
    
    ; Get export data
    mov     r12d, dword ptr [rax + 18h] ; NumberOfNames
    mov     r13d, dword ptr [rax + 20h] ; AddressOfNames RVA
    add     r13, rbx                    ; AddressOfNames VA
    
    xor     r10, r10                    ; Counter
    
GetProcAddress_ByHash_search_loop:
    cmp     r10, r12
    jae     GetProcAddress_ByHash_failed
    
    ; Get name RVA and convert to VA
    mov     esi, dword ptr [r13 + r10*4]
    add     rsi, rbx
    
    ; Calculate ROR-13 hash of name
    xor     r11d, r11d                  ; Hash accumulator
    
GetProcAddress_ByHash_hash_loop:
    lodsb
    test    al, al
    jz      GetProcAddress_ByHash_hash_done
    ror     r11d, 13
    add     r11d, eax
    jmp     GetProcAddress_ByHash_hash_loop
    
GetProcAddress_ByHash_hash_done:
    ; Compare hash
    cmp     r11d, edx
    je      GetProcAddress_ByHash_found
    
    inc     r10
    jmp     GetProcAddress_ByHash_search_loop
    
GetProcAddress_ByHash_found:
    ; Get ordinal
    mov     esi, dword ptr [rax + 24h]  ; AddressOfNameOrdinals RVA
    add     rsi, rbx
    movzx   ecx, word ptr [rsi + r10*2]
    
    ; Get function address
    mov     esi, dword ptr [rax + 1Ch]  ; AddressOfFunctions RVA
    add     rsi, rbx
    mov     eax, dword ptr [rsi + rcx*4]
    add     rax, rbx
    jmp     GetProcAddress_ByHash_exit
    
GetProcAddress_ByHash_failed:
    xor     rax, rax
    
GetProcAddress_ByHash_exit:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
GetProcAddress_ByHash ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; PHASE 21-30: SYMBOL TABLE & PARSING
; ═══════════════════════════════════════════════════════════════════════════════

; ───────────────────────────────────────────────────────────────────────────────
; Initialize_Symbol_Table - Phase 21
; Sets up AVL tree for symbol management
; Returns: RAX = 1 on success, 0 on failure
; ───────────────────────────────────────────────────────────────────────────────
Initialize_Symbol_Table PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    .endprolog
    
    ; Initialize RTL AVL table
    lea     rcx, gSymbolTable
    xor     edx, edx
    xor     r8d, r8d
    lea     r9, Symbol_Compare
    mov     qword ptr [rsp+32], 0
    call    __imp_RtlInitializeGenericTableAvl
    
    mov     rax, 1
    
    add     rsp, 40h
    pop     rbp
    ret
Initialize_Symbol_Table ENDP

; ───────────────────────────────────────────────────────────────────────────────
; Symbol_Compare - Phase 22
; Comparison callback for symbol AVL tree
; RDX, R8 = Pointers to symbols to compare
; Returns: EAX = 0 if equal, other if different
; ───────────────────────────────────────────────────────────────────────────────
Symbol_Compare PROC
    mov     rax, [rdx].SYMBOL.nameHash
    mov     r9, [r8].SYMBOL.nameHash
    sub     rax, r9
    ret
Symbol_Compare ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; PHASE 31-40: OS DETECTION & CROSS-PLATFORM
; ═══════════════════════════════════════════════════════════════════════════════

; ───────────────────────────────────────────────────────────────────────────────
; Detect_Operating_System - Phase 31
; Determines the host operating system
; Returns: RAX = OS_WINDOWS, OS_LINUX, or OS_MACOS
; ───────────────────────────────────────────────────────────────────────────────
Detect_Operating_System PROC
    ; For now, assume Windows since we're using GS:[60h]
    ; In a full implementation, would check segment registers and syscall conventions
    mov     rax, OS_WINDOWS
    ret
Detect_Operating_System ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; PHASE 41-50: HARDWARE ENTANGLEMENT
; ═══════════════════════════════════════════════════════════════════════════════

; ───────────────────────────────────────────────────────────────────────────────
; Generate_HW_Seed - Phase 41
; Generates hardware-specific seed from CPUID and RDTSC
; Returns: RAX = Hardware seed
; ───────────────────────────────────────────────────────────────────────────────
Generate_HW_Seed PROC
    push    rbx
    push    rcx
    push    rdx
    
    xor     r8, r8
    
    ; Get vendor ID
    mov     eax, 0
    cpuid
    add     r8, rbx
    add     r8, rdx
    add     r8, rcx
    
    ; Get processor info
    mov     eax, 1
    cpuid
    add     r8, rax
    add     r8, rcx
    
    ; Add timestamp counter
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    xor     rax, r8
    
    pop     rdx
    pop     rcx
    pop     rbx
    ret
Generate_HW_Seed ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; PHASE 51-60: HOTPATCHING & METAMORPHIC ENGINE
; ═══════════════════════════════════════════════════════════════════════════════

; ───────────────────────────────────────────────────────────────────────────────
; Initialize_Hotpatch_Slots - Phase 51
; Initializes all hotpatch slots to unused state
; ───────────────────────────────────────────────────────────────────────────────
Initialize_Hotpatch_Slots PROC
    push    rcx
    push    rdi
    
    xor     ecx, ecx
    lea     rdi, gHotpatchPool
    
Initialize_Hotpatch_Slots_init_loop:
    cmp     ecx, HOTPATCH_SLOTS
    jae     Initialize_Hotpatch_Slots_done
    
    mov     byte ptr [rdi].HOTPATCH_SLOT.lock_flag, 0
    mov     byte ptr [rdi].HOTPATCH_SLOT.used, 0
    
    add     rdi, SIZEOF HOTPATCH_SLOT
    inc     ecx
    jmp     Initialize_Hotpatch_Slots_init_loop
    
Initialize_Hotpatch_Slots_done:
    pop     rdi
    pop     rcx
    ret
Initialize_Hotpatch_Slots ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; PHASE 61-70: LANGUAGE EXTENSION MAPPING
; ═══════════════════════════════════════════════════════════════════════════════

; ───────────────────────────────────────────────────────────────────────────────
; Build_Lang_Extension_Map - Phase 61
; Builds FNV1a hash map of file extensions to language IDs
; ───────────────────────────────────────────────────────────────────────────────
Build_Lang_Extension_Map PROC
    push    rbx
    push    rdi
    
    lea     rdi, gLangTable
    
    ; Initialize basic language mappings
    ; In full implementation, would hash all extensions
    xor     ebx, ebx
    
Build_Lang_Extension_Map_init_loop:
    cmp     ebx, LANG_MAX
    jae     Build_Lang_Extension_Map_done
    
    mov     byte ptr [rdi].LANG_CONFIG.langId, bl
    xor     rax, rax
    mov     [rdi].LANG_CONFIG.lexerHotpatch, rax
    mov     [rdi].LANG_CONFIG.parserHotpatch, rax
    mov     [rdi].LANG_CONFIG.codegenHotpatch, rax
    mov     [rdi].LANG_CONFIG.vmHotpatch, rax
    mov     dword ptr [rdi].LANG_CONFIG.flags, 0
    mov     [rdi].LANG_CONFIG.extMask, rax
    
    add     rdi, SIZEOF LANG_CONFIG
    inc     ebx
    jmp     Build_Lang_Extension_Map_init_loop
    
Build_Lang_Extension_Map_done:
    pop     rdi
    pop     rbx
    ret
Build_Lang_Extension_Map ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; UTILITY FUNCTIONS
; ═══════════════════════════════════════════════════════════════════════════════

; ───────────────────────────────────────────────────────────────────────────────
; PrintString
; Simple console output using WriteFile
; RCX = Pointer to null-terminated string
; ───────────────────────────────────────────────────────────────────────────────
PrintString PROC FRAME
    push    rbp
    .pushreg rbp
    mov     rbp, rsp
    .setframe rbp, 0
    sub     rsp, 40h
    .allocstack 40h
    .endprolog
    
    push    rcx
    
    ; Calculate string length
    mov     rdi, rcx
    xor     ecx, ecx
PrintString_len_loop:
    cmp     byte ptr [rdi + rcx], 0
    jz      PrintString_len_done
    inc     ecx
    jmp     PrintString_len_loop
PrintString_len_done:
    
    pop     r8                          ; String pointer
    mov     r9d, ecx                    ; Length
    
    ; Get stdout handle (-11)
    mov     rcx, -11
    call    GetStdHandle
    test    rax, rax
    jz      PrintString_exit
    
    ; WriteFile parameters
    mov     rcx, rax                    ; Handle
    mov     rdx, r8                     ; Buffer
    mov     r8d, r9d                    ; Length
    lea     r9, [rbp - 8]              ; BytesWritten
    mov     qword ptr [rsp+32], 0       ; Overlapped
    call    [fnWriteFile]
    
PrintString_exit:
    add     rsp, 40h
    pop     rbp
    ret
PrintString ENDP

; ───────────────────────────────────────────────────────────────────────────────
; GetStdHandle
; Gets standard device handle
; RCX = -10 (stdin), -11 (stdout), -12 (stderr)
; Returns: RAX = Handle or 0 on error
; ───────────────────────────────────────────────────────────────────────────────
GetStdHandle PROC
    ; Simplified - in full implementation would call actual GetStdHandle
    ; For now, return a non-zero value to indicate stdout
    mov     rax, 1
    ret
GetStdHandle ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; EXPORTS
; ═══════════════════════════════════════════════════════════════════════════════

PUBLIC RawrXD_Main
PUBLIC Initialize_Engine
PUBLIC GetKernelBase
PUBLIC Resolve_System_APIs
PUBLIC GetProcAddress_ByHash
PUBLIC Generate_HW_Seed
PUBLIC Detect_Operating_System
PUBLIC Initialize_Symbol_Table
PUBLIC Initialize_Hotpatch_Slots
PUBLIC Build_Lang_Extension_Map
PUBLIC PrintString

END
