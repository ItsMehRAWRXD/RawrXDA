; RawrXD_Titan_MetaReverse.asm - Self-Contained PE (No kernel32.lib required)
; Builds with: ml64 /c /Zi /O2 /D"METAREV=5" /arch:AVX512
; Links with: link /SUBSYSTEM:WINDOWS /NODEFAULTLIB /ENTRY:PEBMain Titan.obj
; Result: Zero static imports, finds kernel32 via PEB walking, resolves all APIs dynamically

OPTION CASEMAP:NONE

; ============================================================================
; PEB / LDR / PE STRUCTURE OFFSETS (Windows 10/11 x64 - Stable since Vista)
; ============================================================================ 
.const
; TEB/PEB Access
OFFSET_TEB_PEB              EQU 60h         ; GS:[0x60] = PEB
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

; === HASH CONSTANTS (FNV-1a 64-bit for API names) ===
FNV_OFFSET_BASIS            EQU 14695981039346656037
FNV_PRIME                   EQU 1099511628211

; === API HASHES (Pre-calculated for speed) ===
HASH_LoadLibraryA           EQU 08A8B4036h        ; FNV1a("LoadLibraryA")
HASH_GetProcAddress         EQU 07C0DFCAAh

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
p_QueryPerformanceCounter   QWORD ?
p_QueryPerformanceFrequency QWORD ?

; User32 Functions
p_MessageBoxA               QWORD ?

; ============================================================================
; CODE: PEB WALKING & API RESOLUTION
; ============================================================================ 
.code

; ----------------------------------------------------------------------------
; HashStringFNV - Case-insensitive FNV-1a hash of ASCII string
; ----------------------------------------------------------------------------
HashStringFNV PROC
    push rsi
    mov rsi, rcx
    mov rax, FNV_OFFSET_BASIS
    
    test rdx, rdx
    jnz @len_known
    
@null_loop:
    movzx r8, BYTE PTR [rsi]
    test r8, r8
    jz @done
    
    or r8, 020h                 ; lowercase
    xor rax, r8
    mov r9, FNV_PRIME
    mul r9
    inc rsi
    jmp @null_loop
    
@len_known:
    jz @done
@len_loop:
    movzx r8, BYTE PTR [rsi]
    or r8, 020h
    xor rax, r8
    mov r9, FNV_PRIME
    mul r9
    inc rsi
    dec rdx
    jmp @len_loop
    
@done:
    pop rsi
    ret
HashStringFNV ENDP

; ----------------------------------------------------------------------------
; GetKernel32Base - Walk PEB InMemoryOrderModuleList
; ----------------------------------------------------------------------------
GetKernel32Base PROC
    push rbx
    push rsi
    push rdi
    
    mov rax, GS:[OFFSET_TEB_PEB]
    test rax, rax
    jz @fail
    
    mov rax, [rax + OFFSET_PEB_LDR]
    test rax, rax
    jz @fail
    
    lea rbx, [rax + 20h]       ; InMemoryOrderModuleList
    mov rax, [rbx]              ; Flink
    
@walk_loop:
    cmp rax, rbx
    je @fail
    
    mov rsi, rax
    sub rsi, OFFSET_LDR_MEM_LINKS   ; Base of LDR entry
    
    lea rdi, [rsi + OFFSET_LDR_BASEPORTNAME]
    movzx ecx, WORD PTR [rdi]
    shr ecx, 1
    mov rdx, [rdi + OFFSET_UNICODE_BUFFER]
    
    ; Check length (12 for kernel32.dll)
    cmp ecx, 12
    jne @next
    
    ; Simplified check: match "kern" and "el32"
    mov r8, QWORD PTR [rdx]
    mov r9, 06E00720065006Bh
    cmp r8, r9
    jne @next
    
    mov r8, QWORD PTR [rdx+8]
    mov r9, 000320033006C0065h
    cmp r8, r9
    jne @next
    
    mov rax, [rsi + OFFSET_LDR_DLLBASE]
    jmp @done
    
@next:
    mov rax, [rax]
    jmp @walk_loop
    
@fail:
    xor eax, eax
@done:
    pop rdi
    pop rsi
    pop rbx
    ret
GetKernel32Base ENDP

; ----------------------------------------------------------------------------
; GetProcAddressByHash - Manual Export Table parsing
; ----------------------------------------------------------------------------
GetProcAddressByHash PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r15, rcx            ; DllBase
    mov r12, rdx            ; Hash
    
    mov eax, DWORD PTR [r15 + OFFSET_DOS_LFANEW]
    add rax, r15
    
    cmp DWORD PTR [rax], 000004550h
    jne @fail
    
    mov ebx, DWORD PTR [rax + OFFSET_PE_OPTHDR + OFFSET_OPT_DATADIR]
    test ebx, ebx
    jz @fail
    
    lea r14, [r15 + rbx]
    
    mov r13d, DWORD PTR [r14 + OFFSET_EXPORT_NUMBEROFNAMES]
    test r13d, r13d
    jz @fail
    
    mov ebx, DWORD PTR [r14 + OFFSET_EXPORT_NAMES]
    lea r8, [r15 + rbx]
    
    mov ebx, DWORD PTR [r14 + OFFSET_EXPORT_ORDINALS]
    lea r9, [r15 + rbx]
    
    mov ebx, DWORD PTR [r14 + OFFSET_EXPORT_FUNCTIONS]
    lea r10, [r15 + rbx]
    
    xor rcx, rcx
    
@search_loop:
    cmp ecx, r13d
    jae @fail
    
    mov ebx, DWORD PTR [r8 + rcx*4]
    test ebx, ebx
    jz @next_name
    
    lea r11, [r15 + rbx]
    
    push rcx
    mov rcx, r11
    xor edx, edx
    call HashStringFNV
    pop rcx
    
    cmp eax, r12d
    je @found
    
@next_name:
    inc ecx
    jmp @search_loop
    
@found:
    movzx ebx, WORD PTR [r9 + rcx*2]
    mov eax, DWORD PTR [r10 + rbx*4]
    test eax, eax
    jz @fail
    add rax, r15
    jmp @done
    
@fail:
    xor eax, eax
@done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
GetProcAddressByHash ENDP

; ----------------------------------------------------------------------------
; BootstrapAPIs
; ----------------------------------------------------------------------------
BootstrapAPIs PROC
    push rbx
    push rsi
    push rdi
    
    call GetKernel32Base
    test rax, rax
    jz @fatal
    mov rbx, rax
    
    mov rcx, rbx
    mov edx, HASH_GetProcAddress
    call GetProcAddressByHash
    test rax, rax
    jz @fatal
    mov [p_GetProcAddress], rax
    
    lea rdx, [szLoadLibraryA]
    mov rcx, rbx
    call rax
    mov [p_LoadLibraryA], rax
    
    sub rsp, 32
    
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
    
    add rsp, 32
    mov eax, 1
    jmp @done
    
@fatal:
    xor eax, eax
@done:
    pop rdi
    pop rsi
    pop rbx
    ret
BootstrapAPIs ENDP

.data
szLoadLibraryA      BYTE "LoadLibraryA", 0
szVirtualAlloc      BYTE "VirtualAlloc", 0
szVirtualFree       BYTE "VirtualFree", 0
szExitProcess       BYTE "ExitProcess", 0

.code
PUBLIC PEBMain
PEBMain PROC
    sub rsp, 40h
    call BootstrapAPIs
    test eax, eax
    jz @die
    
    xor ecx, ecx
    call [p_ExitProcess]
    
@die:
    int 3
    jmp @die
PEBMain ENDP

END
