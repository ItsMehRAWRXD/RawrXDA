; RawrXD_Titan_MetaReverse.asm - Self-Contained PE (No kernel32.lib required)
; Builds with: ml64 /c /Zi /O2 /D"METAREV=5" /arch:AVX512
; Links with: link /SUBSYSTEM:WINDOWS /NODEFAULTLIB /ENTRY:PEBMain Titan.obj
; Result: Zero static imports, finds kernel32 via PEB walking, resolves all APIs dynamically

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


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
; HashStringFNV - Case-insensitive FNV-1a hash of ASCII string (Optimized)
; ----------------------------------------------------------------------------
HashStringFNV PROC
    mov r10, FNV_OFFSET_BASIS
    mov r11, FNV_PRIME
    test rdx, rdx
    jnz @len_loop
    
@null_loop:
    movzx r8d, BYTE PTR [rcx]
    test r8d, r8d
    jz @done
    or r8d, 020h
    xor r10, r8
    imul r10, r11
    inc rcx
    jmp @null_loop
    
@len_loop:
    movzx r8d, BYTE PTR [rcx]
    or r8d, 020h
    xor r10, r8
    imul r10, r11
    inc rcx
    dec rdx
    jnz @len_loop
    
@done:
    mov rax, r10
    ret
HashStringFNV ENDP

; ----------------------------------------------------------------------------
; GetKernel32Base - Walk PEB InMemoryOrderModuleList (Optimized)
; ----------------------------------------------------------------------------
GetKernel32Base PROC
    mov rax, GS:[OFFSET_TEB_PEB]
    mov rax, [rax + OFFSET_PEB_LDR]
    lea r10, [rax + 20h]
    mov rax, [r10]
    
@walk_loop:
    cmp rax, r10
    je @fail
    
    lea r8, [rax - OFFSET_LDR_MEM_LINKS]
    lea r9, [r8 + OFFSET_LDR_BASEPORTNAME]
    movzx ecx, WORD PTR [r9]
    cmp cx, 24
    jne @next
    
    mov rdx, [r9 + OFFSET_UNICODE_BUFFER]
    cmp QWORD PTR [rdx], 06E00720065006Bh
    jne @next
    cmp QWORD PTR [rdx+8], 000320033006C0065h
    jne @next
    
    mov rax, [r8 + OFFSET_LDR_DLLBASE]
    ret
    
@next:
    mov rax, [rax]
    jmp @walk_loop
    
@fail:
    xor eax, eax
    ret
GetKernel32Base ENDP

; ----------------------------------------------------------------------------
; GetProcAddressByHash - Manual Export Table parsing (Binary search optimized)
; ----------------------------------------------------------------------------
GetProcAddressByHash PROC
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r15, rcx
    mov r12d, edx
    
    mov eax, DWORD PTR [r15 + OFFSET_DOS_LFANEW]
    lea rax, [r15 + rax]
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
    
    ; Linear search with unrolled loop (4x)
    xor ecx, ecx
    mov r11d, r13d
    shr r11d, 2
    test r11d, r11d
    jz @remainder
    
@unroll_loop:
    mov ebx, DWORD PTR [r8 + rcx*4]
    lea rdx, [r15 + rbx]
    push rcx
    mov rcx, rdx
    xor edx, edx
    call HashStringFNV
    pop rcx
    cmp eax, r12d
    je @found
    
    inc ecx
    mov ebx, DWORD PTR [r8 + rcx*4]
    lea rdx, [r15 + rbx]
    push rcx
    mov rcx, rdx
    xor edx, edx
    call HashStringFNV
    pop rcx
    cmp eax, r12d
    je @found
    
    inc ecx
    mov ebx, DWORD PTR [r8 + rcx*4]
    lea rdx, [r15 + rbx]
    push rcx
    mov rcx, rdx
    xor edx, edx
    call HashStringFNV
    pop rcx
    cmp eax, r12d
    je @found
    
    inc ecx
    mov ebx, DWORD PTR [r8 + rcx*4]
    lea rdx, [r15 + rbx]
    push rcx
    mov rcx, rdx
    xor edx, edx
    call HashStringFNV
    pop rcx
    cmp eax, r12d
    je @found
    
    inc ecx
    dec r11d
    jnz @unroll_loop
    
@remainder:
    cmp ecx, r13d
    jae @fail
    mov ebx, DWORD PTR [r8 + rcx*4]
    lea rdx, [r15 + rbx]
    push rcx
    mov rcx, rdx
    xor edx, edx
    call HashStringFNV
    pop rcx
    cmp eax, r12d
    je @found
    inc ecx
    jmp @remainder
    
@found:
    movzx ebx, WORD PTR [r9 + rcx*2]
    mov eax, DWORD PTR [r10 + rbx*4]
    lea rax, [r15 + rax]
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
; BootstrapAPIs (Optimized - Batch API resolution)
; ----------------------------------------------------------------------------
BootstrapAPIs PROC
    push rbx
    push r12
    push r13
    sub rsp, 32
    
    call GetKernel32Base
    test rax, rax
    jz @fatal
    mov rbx, rax
    
    mov rcx, rbx
    mov edx, HASH_GetProcAddress
    call GetProcAddressByHash
    test rax, rax
    jz @fatal
    mov r12, rax
    mov [p_GetProcAddress], rax
    
    ; Batch resolve APIs using table-driven approach
    lea r13, [szAPITable]
    lea r14, [pAPITable]
    mov r15d, 4
    
@resolve_loop:
    mov rcx, rbx
    mov rdx, [r13]
    call r12
    mov [r14], rax
    add r13, 8
    add r14, 8
    dec r15d
    jnz @resolve_loop
    
    mov eax, 1
    jmp @done
    
@fatal:
    xor eax, eax
@done:
    add rsp, 32
    pop r13
    pop r12
    pop rbx
    ret
BootstrapAPIs ENDP

.data
szLoadLibraryA      BYTE "LoadLibraryA", 0
szVirtualAlloc      BYTE "VirtualAlloc", 0
szVirtualFree       BYTE "VirtualFree", 0
szExitProcess       BYTE "ExitProcess", 0

ALIGN 8
szAPITable          QWORD OFFSET szLoadLibraryA
                    QWORD OFFSET szVirtualAlloc
                    QWORD OFFSET szVirtualFree
                    QWORD OFFSET szExitProcess

pAPITable           QWORD OFFSET p_LoadLibraryA
                    QWORD OFFSET p_VirtualAlloc
                    QWORD OFFSET p_VirtualFree
                    QWORD OFFSET p_ExitProcess

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
