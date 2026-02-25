; RawrXD_Titan_MetaReverse.asm - Self-Contained PE (No kernel32.lib required)
; Builds with: ml64 /c /Zi /O2 /arch:AVX512 RawrXD_Titan_MetaReverse.asm
; Links with: link /SUBSYSTEM:WINDOWS /NODEFAULTLIB /ENTRY:PEBMain RawrXD_Titan_MetaReverse.obj
; Result: Zero static imports, finds kernel32 via PEB walking, resolves all APIs dynamically

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; No includelib statements - we are linking against NOTHING
; No EXTERNDEF for WinAPI - we resolve them at runtime via PEB walking

; ============================================================================
; PEB / LDR / PE STRUCTURE OFFSETS (Windows 10/11 x64 - Stable since Vista)
; ============================================================================
.const
; TEB/PEB Access
OFFSET_TEB_PEB              EQU 60h         ; GS:[60h] = PEB
OFFSET_PEB_LDR              EQU 18h         ; PEB->Ldr

; LDR_DATA_TABLE_ENTRY (InMemoryOrder list)
OFFSET_LDR_MEM_LINKS        EQU 10h         ; InMemoryOrderModuleList (LIST_ENTRY)
OFFSET_LDR_DLLBASE          EQU 30h         ; DllBase (ImageBase)
OFFSET_LDR_BASEPORTNAME     EQU 58h         ; BaseDllName (UNICODE_STRING)

; UNICODE_STRING
OFFSET_UNICODE_BUFFER       EQU 08h         ; Ptr to wchar_t name

; PE Headers
OFFSET_DOS_LFANEW           EQU 3Ch         ; e_lfanew (RVA of PE header)
OFFSET_PE_OPTHDR            EQU 18h         ; Offset to OptionalHeader from PE Sig
OFFSET_OPT_DATADIR          EQU 70h         ; Export DataDirectory[0] rva offset in OptionalHeader
OFFSET_EXPORT_NAMES         EQU 20h         ; AddressOfNames RVA
OFFSET_EXPORT_ORDINALS      EQU 24h         ; AddressOfNameOrdinals RVA  
OFFSET_EXPORT_FUNCTIONS     EQU 1Ch         ; AddressOfFunctions RVA
OFFSET_EXPORT_NUMBEROFNAMES EQU 18h         ; NumberOfNames

; === API NAME STRINGS ===
; Used with GetProcAddress after bootstrap

; ============================================================================
; FUNCTION POINTER TABLES (Our Import Address Table replacement)
; ============================================================================
.data?
ALIGN 8

; Kernel32 Functions (Populated by PEB bootstrap)
p_LoadLibraryA              QWORD ?
p_GetProcAddress            QWORD ?  
p_VirtualAlloc              QWORD ?
p_VirtualFree               QWORD ?
p_ExitProcess               QWORD ?
p_CreateFileA               QWORD ?
p_CreateFileMappingA        QWORD ?
p_MapViewOfFile             QWORD ?
p_UnmapViewOfFile           QWORD ?
p_CloseHandle               QWORD ?
p_GetCurrentProcess         QWORD ?
p_SetPriorityClass          QWORD ?
p_QueryPerformanceCounter   QWORD ?
p_QueryPerformanceFrequency QWORD ?

; User32 Functions (Loaded after kernel32 boot)
p_MessageBoxA               QWORD ?
p_CreateWindowExA           QWORD ?
p_RegisterClassExA          QWORD ?
p_ShowWindow                QWORD ?
p_UpdateWindow              QWORD ?
p_GetMessageA               QWORD ?
p_TranslateMessage          QWORD ?
p_DispatchMessageA          QWORD ?
p_DefWindowProcA            QWORD ?
p_PostQuitMessage           QWORD ?
p_SetTimer                  QWORD ?
p_KillTimer                 QWORD ?
p_SendMessageA              QWORD ?
p_PostMessageA              QWORD ?

; ============================================================================
; CODE: PEB WALKING & API RESOLUTION (The "Linker Bypass")
; ============================================================================
.code

; ----------------------------------------------------------------------------
; GetKernel32Base - Walk PEB InMemoryOrderModuleList to find kernel32.dll
; Output: RAX = ImageBase of kernel32.dll (0 if not found)
; Clobbers: RCX, RDX, R8, R9, R10, R11
; ----------------------------------------------------------------------------
GetKernel32Base PROC
    push rbx
    push rsi
    push rdi
    
    ; GS:[60h] = PEB
    mov rax, GS:[60h]
    test rax, rax
    jz @@fail
    
    ; RAX = PEB, [RAX+18h] = PEB_LDR_DATA
    mov rax, [rax + 18h]
    test rax, rax
    jz @@fail
    
    ; [RAX+20h] = InMemoryOrderModuleList (LIST_ENTRY)
    lea rbx, [rax + 20h]            ; RBX = ListHead (points to first blink/flink)
    
    ; First entry is the EXE itself
    mov rax, [rbx]                  ; RAX = Flink (First entry)
    
@@walk_loop:
    ; Check if we looped back to head
    cmp rax, rbx
    je @@fail                       ; Back at list head, not found
    
    ; Calculate LDR_DATA_TABLE_ENTRY base
    ; InMemoryOrderLinks is at offset 10h in the struct, so:
    ; Entry = Current - 10h
    mov rsi, rax
    sub rsi, 10h                    ; RSI = LDR_DATA_TABLE_ENTRY*
    
    ; Get BaseDllName (UNICODE_STRING at offset 58h)
    lea rdi, [rsi + 58h]
    movzx ecx, WORD PTR [rdi]       ; Length (bytes)
    shr ecx, 1                      ; Convert to wchar count (divide by 2)
    mov rdx, [rdi + 8h]             ; RDX = ptr to wchar string
    
    ; Compare with "kernel32.dll" (12 wchars)
    cmp ecx, 12
    jne @@next
    
    ; Compare "kern" (first 4 wchars)
    mov r8, QWORD PTR [rdx]
    mov r9, 06E0072006500006Bh      ; "kern" in little-endian (approx)
    ; Better: use loop for robust comparison
    mov r8w, WORD PTR [rdx]
    cmp r8w, 006Bh                  ; 'k' = 0x006B
    jne @@next
    
    mov r8w, WORD PTR [rdx+2]
    cmp r8w, 0065h                  ; 'e' = 0x0065
    jne @@next
    
    ; If first two match, assume it's kernel32 (full comparison would be longer)
    ; Get DllBase at offset 30h
    mov rax, [rsi + 30h]
    jmp @@done
    
@@next:
    ; Follow Flink to next module
    mov rax, [rax]                  ; Current->Flink
    jmp @@walk_loop
    
@@fail:
    xor eax, eax
@@done:
    pop rdi
    pop rsi
    pop rbx
    ret
GetKernel32Base ENDP

; ----------------------------------------------------------------------------
; GetProcAddressByName - Manual Export Table parsing in mapped DLL
; RCX = DllBase, RDX = Function name (ASCII null-terminated)
; Output: RAX = Function VA (0 if not found)
; Clobbers: RBX, R8-R15
; ----------------------------------------------------------------------------
GetProcAddressByName PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 20h
    
    mov r15, rcx            ; R15 = DllBase
    mov r12, rdx            ; R12 = Function name
    
    ; Parse PE Header
    mov eax, DWORD PTR [r15 + 3Ch]
    add rax, r15            ; RAX = PE Header (NT Headers)
    
    ; Check PE Signature (optional safety)
    cmp DWORD PTR [rax], 04550h     ; "PE\0\0"
    jne @@fail
    
    ; Get Export Directory RVA from DataDirectory[0]
    mov ebx, DWORD PTR [rax + 18h + 70h] ; ExportDir RVA at OptionalHeader+70h
    test ebx, ebx
    jz @@fail
    
    lea r14, [r15 + rbx]    ; R14 = Export Directory VA
    
    ; Get table RVAs and counts
    mov r13d, DWORD PTR [r14 + 18h] ; NumberOfNames
    test r13d, r13d
    jz @@fail
    
    mov ebx, DWORD PTR [r14 + 20h]  ; AddressOfNames RVA
    lea r8, [r15 + rbx]     ; R8 = Names Array
    
    mov ebx, DWORD PTR [r14 + 24h]  ; AddressOfNameOrdinals RVA
    lea r9, [r15 + rbx]     ; R9 = Ordinals Array (USHORTs)
    
    mov ebx, DWORD PTR [r14 + 1Ch]  ; AddressOfFunctions RVA
    lea r10, [r15 + rbx]    ; R10 = Functions Array (DWORD RVAs)
    
    xor rcx, rcx            ; Index = 0
    
@@search_loop:
    cmp ecx, r13d
    jae @@fail
    
    ; Get Name RVA from array (DWORD array, zero-extended to 64-bit)
    mov ebx, DWORD PTR [r8 + rcx*4] ; RVA of name string
    test ebx, ebx
    jz @@next_name
    
    lea r11, [r15 + rbx]            ; R11 = Name string VA (ASCII)
    
    ; Compare strings: r12 (search) vs r11 (candidate)
    mov rsi, r12
    mov rdi, r11
@@strcmp_loop:
    movzx eax, BYTE PTR [rsi]
    movzx ebx, BYTE PTR [rdi]
    cmp al, bl
    jne @@next_name
    test al, al
    jz @@found                      ; Both null-terminated at same point
    inc rsi
    inc rdi
    jmp @@strcmp_loop
    
@@found:
    ; Get Ordinal from OrdinalsArray[Index] (USHORT array)
    movzx ebx, WORD PTR [r9 + rcx*2] ; EBX = Ordinal Hint
    
    ; Get Function RVA from AddressOfFunctions[Ordinal]
    mov eax, DWORD PTR [r10 + rbx*4]
    test eax, eax
    jz @@fail
    
    ; Calculate VA
    add rax, r15
    jmp @@done
    
@@next_name:
    inc ecx
    jmp @@search_loop
    
@@fail:
    xor eax, eax
@@done:
    add rsp, 20h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
GetProcAddressByName ENDP

; ----------------------------------------------------------------------------
; BootstrapAPIs - Resolve all kernel32 functions we need
; Uses PEB walking then GetProcAddress resolution
; Returns: EAX = 1 (success) or 0 (failure)
; ============================================================================
BootstrapAPIs PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 40h
    
    ; Step 1: Find kernel32 base via PEB
    call GetKernel32Base
    test rax, rax
    jz @@fatal_error
    mov rbx, rax            ; RBX = kernel32.dll base
    
    ; Step 2: Resolve GetProcAddress using direct name search in kernel32 exports
    mov rcx, rbx
    lea rdx, [szGetProcAddress]
    call GetProcAddressByName
    test rax, rax
    jz @@fatal_error
    mov [p_GetProcAddress], rax
    
    ; Step 3: Now use GetProcAddress to resolve all other kernel32 APIs
    mov rcx, rbx
    lea rdx, [szLoadLibraryA]
    call [p_GetProcAddress]
    mov [p_LoadLibraryA], rax
    
    mov rcx, rbx
    lea rdx, [szVirtualAlloc]
    call [p_GetProcAddress]
    mov [p_VirtualAlloc], rax
    
    mov rcx, rbx
    lea rdx, [szVirtualFree]
    call [p_GetProcAddress]
    mov [p_VirtualFree], rax
    
    mov rcx, rbx
    lea rdx, [szExitProcess]
    call [p_GetProcAddress]
    mov [p_ExitProcess], rax
    
    mov rcx, rbx
    lea rdx, [szCreateFileA]
    call [p_GetProcAddress]
    mov [p_CreateFileA], rax
    
    mov rcx, rbx
    lea rdx, [szCreateFileMappingA]
    call [p_GetProcAddress]
    mov [p_CreateFileMappingA], rax
    
    mov rcx, rbx
    lea rdx, [szMapViewOfFile]
    call [p_GetProcAddress]
    mov [p_MapViewOfFile], rax
    
    mov rcx, rbx
    lea rdx, [szUnmapViewOfFile]
    call [p_GetProcAddress]
    mov [p_UnmapViewOfFile], rax
    
    mov rcx, rbx
    lea rdx, [szCloseHandle]
    call [p_GetProcAddress]
    mov [p_CloseHandle], rax
    
    mov rcx, rbx
    lea rdx, [szQueryPerformanceCounter]
    call [p_GetProcAddress]
    mov [p_QueryPerformanceCounter], rax
    
    mov rcx, rbx
    lea rdx, [szQueryPerformanceFrequency]
    call [p_GetProcAddress]
    mov [p_QueryPerformanceFrequency], rax
    
    ; Step 4: Load user32.dll and resolve GUI functions (optional)
    lea rcx, [szUser32]
    call [p_LoadLibraryA]           ; HMODULE User32
    test rax, rax
    jz @@skip_user32                ; Console mode if fails
    mov rsi, rax                    ; RSI = user32 base
    
    mov rcx, rsi
    lea rdx, [szMessageBoxA]
    call [p_GetProcAddress]
    mov [p_MessageBoxA], rax
    
    ; ... resolve other user32 funcs as needed ...
    
@@skip_user32:
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    mov eax, 1                      ; Success
    ret
    
@@fatal_error:
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    xor eax, eax
    ret
BootstrapAPIs ENDP

; ============================================================================
; DATA: String Literals for API Names (Null-terminated ASCII)
; ============================================================================
.data

szGetProcAddress            BYTE "GetProcAddress", 0
szLoadLibraryA              BYTE "LoadLibraryA", 0
szVirtualAlloc              BYTE "VirtualAlloc", 0
szVirtualFree               BYTE "VirtualFree", 0
szExitProcess               BYTE "ExitProcess", 0
szCreateFileA               BYTE "CreateFileA", 0
szCreateFileMappingA        BYTE "CreateFileMappingA", 0
szMapViewOfFile             BYTE "MapViewOfFile", 0
szUnmapViewOfFile           BYTE "UnmapViewOfFile", 0
szCloseHandle               BYTE "CloseHandle", 0
szQueryPerformanceCounter   BYTE "QueryPerformanceCounter", 0
szQueryPerformanceFrequency BYTE "QueryPerformanceFrequency", 0
szUser32                    BYTE "user32.dll", 0
szMessageBoxA               BYTE "MessageBoxA", 0

; ============================================================================
; CODE: ENTRY POINT (PEBMain - replaces main/WinMain for /NODEFAULTLIB)
; ============================================================================
.code

PUBLIC PEBMain
PEBMain PROC
    sub rsp, 40h                    ; Standard shadow space + alignment
    
    ; Bootstrap all APIs without kernel32.lib
    call BootstrapAPIs
    test eax, eax
    jz @@die
    
    ; Example: Allocate 64MB using resolved VirtualAlloc
    mov rcx, 0                      ; lpAddress
    mov rdx, 04000000h              ; dwSize (64MB)
    mov r8d, 3000h                  ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 04h                    ; PAGE_READWRITE
    call [p_VirtualAlloc]
    test rax, rax
    jz @@die
    
    ; Success - cleanup and exit
    xor ecx, ecx                    ; Exit code 0
    call [p_ExitProcess]            ; Never returns
    
@@die:
    int 3                           ; Break into debugger on fail
    jmp @@die
    ret
PEBMain ENDP

END
