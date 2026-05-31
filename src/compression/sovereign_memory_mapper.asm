; ============================================================================
; RawrXD Sovereign Memory Mapper
; Zero-CRT Win32 Memory Mapping for Extreme Model Compression
; ============================================================================
; Target: x64 MASM (ML64)
; Purpose: Map massive model files without committing physical RAM
;          until pages are touched. Supports staged paging and
;          large-page optimization for TLB efficiency.
; ============================================================================

; --- External Win32 Symbols ---
extern CreateFileW           : proc
extern CreateFileMappingW    : proc
extern MapViewOfFileEx       : proc
extern UnmapViewOfFile       : proc
extern CloseHandle           : proc
extern GetLargePageMinimum   : proc
extern VirtualAlloc          : proc
extern VirtualFree           : proc
extern GetLastError          : proc
extern OutputDebugStringA    : proc
extern TlsAlloc              : proc
extern TlsFree               : proc
extern TlsGetValue           : proc
extern TlsSetValue           : proc

; --- Constants ---
GENERIC_READ              equ 80000000h
OPEN_EXISTING             equ 3
FILE_SHARE_READ           equ 00000001h
PAGE_READONLY             equ 02h
FILE_MAP_READ             equ 00000004h
MEM_COMMIT                equ 00001000h
MEM_RESERVE               equ 00002000h
MEM_LARGE_PAGES           equ 20000000h
MEM_RELEASE               equ 00008000h
TLS_OUT_OF_INDEXES        equ 0FFFFFFFFh

; --- Data Section ---
.data
    ; Error strings for debug output
    err_open_file           db "[SovMem] Failed to open model file", 0
    err_create_mapping      db "[SovMem] Failed to create file mapping", 0
    err_map_view            db "[SovMem] Failed to map view", 0
    err_large_pages         db "[SovMem] Large pages not available", 0
    info_mapping_ok         db "[SovMem] Model mapped successfully", 0
    info_large_page_size    db "[SovMem] Large page size: ", 0
    
    ; State tracking
    g_hFile                 dq 0
    g_hMap                  dq 0
    g_lpBaseAddr            dq 0
    g_mapSize               dq 0
    g_largePageSize         dq 0
    g_useLargePages         db 0
    g_spinLockWord          dd 0
    g_tlsActivationSlot     dd TLS_OUT_OF_INDEXES

; --- Code Section ---
.code

; ============================================================================
; SovMem_Init
; Initialize the sovereign memory mapper subsystem.
; Detects large page support and caches the minimum size.
; ============================================================================
SovMem_Init proc
    sub rsp, 40                 ; Shadow space + alignment
    
    ; Query large page minimum size
    call GetLargePageMinimum
    mov g_largePageSize, rax
    
    test rax, rax
    jz _no_large_pages
    
    mov g_useLargePages, 1
    
    ; Debug output
    lea rcx, info_large_page_size
    call OutputDebugStringA
    
    jmp _init_done
    
_no_large_pages:
    mov g_useLargePages, 0
    lea rcx, err_large_pages
    call OutputDebugStringA
    
_init_done:
    xor eax, eax                ; Return SUCCESS
    add rsp, 40
    ret
SovMem_Init endp

; ============================================================================
; SovMem_MapModel
; Maps a model file into process address space without committing RAM.
; 
; Parameters:
;   RCX = Pointer to wide-char file path
;   RDX = Desired base address (0 = let system choose)
;   R8  = Mapping size (0 = entire file)
;
; Returns:
;   RAX = Base address of mapping, or 0 on failure
; ============================================================================
SovMem_MapModel proc
    sub rsp, 56                 ; Shadow space + local vars
    mov [rsp+48], rsi           ; Save non-volatile registers
    mov [rsp+40], rdi
    
    mov rsi, rcx                ; RSI = file path
    mov rdi, rdx                ; RDI = desired base
    mov r12, r8                 ; R12 = mapping size
    
    ; --- Step 1: Open the model file ---
    xor r9, r9                  ; lpSecurityAttributes = NULL
    mov r8, OPEN_EXISTING       ; dwCreationDisposition
    xor edx, edx
    or edx, FILE_SHARE_READ     ; dwShareMode
    mov ecx, GENERIC_READ       ; dwDesiredAccess
    mov rcx, rsi                ; lpFileName
    call CreateFileW
    
    cmp rax, -1
    je _map_fail_open
    mov g_hFile, rax
    
    ; --- Step 2: Create file mapping ---
    xor r9, r9                  ; dwMaximumSizeHigh = 0
    xor r8, r8                  ; dwMaximumSizeLow = 0 (use file size)
    xor edx, edx                ; lpName = NULL
    mov ecx, PAGE_READONLY      ; flProtect
    mov rcx, g_hFile            ; hFile
    call CreateFileMappingW
    
    test rax, rax
    jz _map_fail_mapping
    mov g_hMap, rax
    
    ; --- Step 3: Map view of file ---
    xor r9, r9                  ; dwNumberOfBytesToMap = 0 (entire file)
    xor r8, r8                  ; dwFileOffsetHigh = 0
    xor edx, edx                ; dwFileOffsetLow = 0
    mov ecx, FILE_MAP_READ      ; dwDesiredAccess
    mov r8, rdi                 ; lpBaseAddress (desired base, 0 = auto)
    mov rdx, g_hMap             ; hFileMappingObject
    call MapViewOfFileEx
    
    test rax, rax
    jz _map_fail_view
    mov g_lpBaseAddr, rax
    mov g_mapSize, r12
    
    ; Success
    lea rcx, info_mapping_ok
    call OutputDebugStringA
    
    mov rax, g_lpBaseAddr       ; Return base address
    jmp _map_cleanup
    
_map_fail_open:
    lea rcx, err_open_file
    call OutputDebugStringA
    xor eax, eax
    jmp _map_cleanup
    
_map_fail_mapping:
    lea rcx, err_create_mapping
    call OutputDebugStringA
    mov rcx, g_hFile
    call CloseHandle
    xor eax, eax
    jmp _map_cleanup
    
_map_fail_view:
    lea rcx, err_map_view
    call OutputDebugStringA
    mov rcx, g_hMap
    call CloseHandle
    mov rcx, g_hFile
    call CloseHandle
    xor eax, eax
    
_map_cleanup:
    mov rsi, [rsp+48]           ; Restore registers
    mov rdi, [rsp+40]
    add rsp, 56
    ret
SovMem_MapModel endp

; ============================================================================
; SovMem_UnmapModel
; Unmaps the model and releases all handles.
; ============================================================================
SovMem_UnmapModel proc
    sub rsp, 40
    
    ; Unmap view
    mov rcx, g_lpBaseAddr
    test rcx, rcx
    jz _skip_unmap
    call UnmapViewOfFile
    mov g_lpBaseAddr, 0
    
_skip_unmap:
    ; Close mapping handle
    mov rcx, g_hMap
    test rcx, rcx
    jz _skip_close_map
    call CloseHandle
    mov g_hMap, 0
    
_skip_close_map:
    ; Close file handle
    mov rcx, g_hFile
    test rcx, rcx
    jz _skip_close_file
    call CloseHandle
    mov g_hFile, 0
    
_skip_close_file:
    xor eax, eax
    add rsp, 40
    ret
SovMem_UnmapModel endp

; ============================================================================
; SovMem_AllocateLargePage
; Allocates a large-page aligned buffer for weight staging.
; 
; Parameters:
;   RCX = Size in bytes
;
; Returns:
;   RAX = Allocated address, or 0 on failure
; ============================================================================
SovMem_AllocateLargePage proc
    sub rsp, 40
    
    cmp g_useLargePages, 0
    je _use_regular_alloc
    
    ; Round up to large page size
    mov rax, g_largePageSize
    mov rdx, rcx
    add rdx, rax
    dec rdx
    not rax
    and rdx, rax
    inc rax
    and rdx, rax
    mov rcx, rdx
    
    ; VirtualAlloc with large pages
    xor r8, r8                  ; flProtect = PAGE_NOACCESS (reserve only)
    mov edx, MEM_COMMIT or MEM_RESERVE or MEM_LARGE_PAGES
    ; RCX = size already set
    xor ecx, ecx                ; lpAddress = NULL (let system choose)
    call VirtualAlloc
    
    test rax, rax
    jnz _alloc_done
    
_use_regular_alloc:
    ; Fallback to regular allocation
    xor r8, r8                  ; flProtect = PAGE_READWRITE
    mov edx, MEM_COMMIT or MEM_RESERVE
    xor ecx, ecx                ; lpAddress = NULL
    call VirtualAlloc
    
_alloc_done:
    add rsp, 40
    ret
SovMem_AllocateLargePage endp

; ============================================================================
; SovMem_FreeLargePage
; Frees a large-page allocation.
; 
; Parameters:
;   RCX = Base address to free
; ============================================================================
SovMem_FreeLargePage proc
    sub rsp, 40
    
    mov rdx, 0                  ; dwSize = 0 (release entire region)
    mov r8d, MEM_RELEASE
    ; RCX = address
    call VirtualFree
    
    add rsp, 40
    ret
SovMem_FreeLargePage endp

; ============================================================================
; SovMem_GetMappedBase
; Returns the current mapped base address.
; ============================================================================
SovMem_GetMappedBase proc
    mov rax, g_lpBaseAddr
    ret
SovMem_GetMappedBase endp

; ============================================================================
; SovMem_GetMapSize
; Returns the current mapped size.
; ============================================================================
SovMem_GetMapSize proc
    mov rax, g_mapSize
    ret
SovMem_GetMapSize endp

; ============================================================================
; SovMem_SpinAcquire
; Acquire a tiny lock using lock bts on bit 0.
; Governs shared state access (KV cache + inference state).
; ============================================================================
SovMem_SpinAcquire proc
_spin_try:
    lock bts dword ptr [g_spinLockWord], 0
    jc _spin_wait
    ret

_spin_wait:
    pause
    cmp dword ptr [g_spinLockWord], 0
    jne _spin_wait
    jmp _spin_try
SovMem_SpinAcquire endp

; ============================================================================
; SovMem_SpinRelease
; Release lock bit and publish with xchg.
; ============================================================================
SovMem_SpinRelease proc
    xor eax, eax
    xchg dword ptr [g_spinLockWord], eax
    ret
SovMem_SpinRelease endp

; ============================================================================
; SovMem_TLS_Init
; Allocates one TLS slot for per-thread activation buffer pointers.
;
; Returns:
;   EAX = 0 on success, non-zero on failure
; ============================================================================
SovMem_TLS_Init proc
    sub rsp, 40

    call TlsAlloc
    cmp eax, TLS_OUT_OF_INDEXES
    je _tls_init_fail

    mov g_tlsActivationSlot, eax
    xor eax, eax
    add rsp, 40
    ret

_tls_init_fail:
    mov eax, 1
    add rsp, 40
    ret
SovMem_TLS_Init endp

; ============================================================================
; SovMem_TLS_Shutdown
; Frees the TLS slot for activation pointers.
;
; Returns:
;   EAX = 0 on success, non-zero on failure
; ============================================================================
SovMem_TLS_Shutdown proc
    sub rsp, 40

    mov eax, g_tlsActivationSlot
    cmp eax, TLS_OUT_OF_INDEXES
    je _tls_shutdown_ok

    mov ecx, eax
    call TlsFree
    test eax, eax
    jz _tls_shutdown_fail
    mov dword ptr [g_tlsActivationSlot], TLS_OUT_OF_INDEXES

_tls_shutdown_ok:
    xor eax, eax
    add rsp, 40
    ret

_tls_shutdown_fail:
    mov eax, 1
    add rsp, 40
    ret
SovMem_TLS_Shutdown endp

; ============================================================================
; SovMem_TLS_SetActivationBuffer
; Store per-thread activation buffer pointer in TLS.
;
; Parameters:
;   RCX = activation buffer pointer for current thread
;
; Returns:
;   EAX = 0 on success, non-zero on failure
; ============================================================================
SovMem_TLS_SetActivationBuffer proc
    sub rsp, 40

    mov eax, g_tlsActivationSlot
    cmp eax, TLS_OUT_OF_INDEXES
    je _tls_set_fail

    mov rdx, rcx
    mov ecx, eax
    call TlsSetValue
    test eax, eax
    jz _tls_set_fail

    xor eax, eax
    add rsp, 40
    ret

_tls_set_fail:
    mov eax, 1
    add rsp, 40
    ret
SovMem_TLS_SetActivationBuffer endp

; ============================================================================
; SovMem_TLS_GetActivationBuffer
; Retrieves per-thread activation buffer pointer.
;
; Returns:
;   RAX = activation pointer for current thread, or 0 if unavailable
; ============================================================================
SovMem_TLS_GetActivationBuffer proc
    sub rsp, 40

    mov eax, g_tlsActivationSlot
    cmp eax, TLS_OUT_OF_INDEXES
    je _tls_get_none

    mov ecx, eax
    call TlsGetValue
    add rsp, 40
    ret

_tls_get_none:
    xor eax, eax
    add rsp, 40
    ret
SovMem_TLS_GetActivationBuffer endp

end
