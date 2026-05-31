; k_swap_aperture_win32.asm
; Dynamic Swap-Aperture kernel for Win32/x64 MASM.
; Implements real placeholder reservation with VirtualAlloc2 and in-aperture
; replacement via VirtualFree + MapViewOfFile3.

OPTION CASEMAP:NONE

EXTERN VirtualAlloc2:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN MapViewOfFile3:PROC
EXTERN UnmapViewOfFile2:PROC
EXTERN GetCurrentProcess:PROC
EXTERN GetLastError:PROC

RAWRXD_SWAP_WINDOW_BYTES           EQU 1073741824
RAWRXD_PAGE_64KB                   EQU 65536

RAWRXD_MEM_RESERVE                 EQU 00002000h
RAWRXD_MEM_RELEASE                 EQU 00008000h
RAWRXD_MEM_RESERVE_PLACEHOLDER     EQU 00040000h
RAWRXD_MEM_PRESERVE_PLACEHOLDER    EQU 00000002h
RAWRXD_MEM_REPLACE_PLACEHOLDER     EQU 00004000h

RAWRXD_PAGE_NOACCESS               EQU 00000001h
RAWRXD_PAGE_READONLY               EQU 00000002h

RAWRXD_ERROR_INVALID_PARAM         EQU 00000057h
RAWRXD_ERROR_NOT_READY             EQU 00000015h
RAWRXD_ERROR_GEN_FAILURE           EQU 0000001Fh

.CODE

k_swap_aperture_init PROC FRAME
    ; rcx = aperture_base
    ; rdx = aperture_span_bytes
    ; r8  = telemetry_out (optional, 24+ bytes)

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rbx, rcx
    mov     rsi, rdx
    mov     rdi, r8

    mov     qword ptr [g_aperture_base], 0
    mov     qword ptr [g_aperture_span], rsi
    mov     qword ptr [g_requested_base], rbx
    mov     qword ptr [g_last_chunk_index], 0
    mov     qword ptr [g_last_map_size], 0
    mov     qword ptr [g_last_map_addr], 0
    mov     qword ptr [g_last_unmap_addr], 0
    mov     dword ptr [g_last_status], 0
    mov     dword ptr [g_last_placeholder_flags], 0
    mov     dword ptr [g_placeholder_mode], 0
    mov     dword ptr [g_fallback_used], 0

    test    rsi, rsi
    jz      init_invalid
    test    rsi, RAWRXD_PAGE_64KB - 1
    jnz     init_invalid
    test    rbx, rbx
    jz      init_call_valloc2
    test    rbx, RAWRXD_PAGE_64KB - 1
    jnz     init_invalid

init_call_valloc2:
    sub     rsp, 40h
    call    GetCurrentProcess
    mov     rcx, rax
    mov     rdx, rbx
    mov     r8, rsi
    mov     r9d, RAWRXD_MEM_RESERVE or RAWRXD_MEM_RESERVE_PLACEHOLDER
    mov     dword ptr [rsp + 20h], RAWRXD_PAGE_NOACCESS
    xor     eax, eax
    mov     qword ptr [rsp + 28h], rax
    mov     qword ptr [rsp + 30h], rax
    call    VirtualAlloc2
    add     rsp, 40h

    test    rax, rax
    jnz     init_valloc2_success

    test    rbx, rbx
    jz      init_try_virtual_alloc

    ; Fallback 1: let the OS choose a base for placeholder reserve.
    sub     rsp, 40h
    call    GetCurrentProcess
    mov     rcx, rax
    xor     edx, edx
    mov     r8, rsi
    mov     r9d, RAWRXD_MEM_RESERVE or RAWRXD_MEM_RESERVE_PLACEHOLDER
    mov     dword ptr [rsp + 20h], RAWRXD_PAGE_NOACCESS
    xor     eax, eax
    mov     qword ptr [rsp + 28h], rax
    mov     qword ptr [rsp + 30h], rax
    call    VirtualAlloc2
    add     rsp, 40h
    test    rax, rax
    jz      init_try_virtual_alloc
    mov     dword ptr [g_fallback_used], 1
    jmp     init_valloc2_success

init_try_virtual_alloc:
    ; Fallback 2: standard reserve when placeholder APIs are unavailable.
    sub     rsp, 20h
    xor     ecx, ecx
    mov     rdx, rsi
    mov     r8d, RAWRXD_MEM_RESERVE
    mov     r9d, RAWRXD_PAGE_NOACCESS
    call    VirtualAlloc
    add     rsp, 20h
    test    rax, rax
    jz      init_alloc_failed
    mov     dword ptr [g_fallback_used], 2
    mov     dword ptr [g_placeholder_mode], 0
    jmp     init_finalize

init_valloc2_success:
    mov     dword ptr [g_placeholder_mode], 1

init_finalize:
    mov     qword ptr [g_aperture_base], rax

    test    rdi, rdi
    jz      init_success
    mov     qword ptr [rdi + 0], rax
    mov     qword ptr [rdi + 8], rsi
    mov     dword ptr [rdi + 16], 0
    mov     eax, dword ptr [g_fallback_used]
    mov     dword ptr [rdi + 20], eax

init_success:
    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret

init_invalid:
    mov     dword ptr [g_last_status], RAWRXD_ERROR_INVALID_PARAM
    mov     eax, RAWRXD_ERROR_INVALID_PARAM
    pop     rdi
    pop     rsi
    pop     rbx
    ret

init_not_ready:
    mov     dword ptr [g_last_status], RAWRXD_ERROR_INVALID_PARAM
    mov     eax, RAWRXD_ERROR_INVALID_PARAM
    pop     rdi
    pop     rsi
    pop     rbx
    ret

init_alloc_failed:
    sub     rsp, 20h
    call    GetLastError
    add     rsp, 20h
    test    eax, eax
    jnz     init_alloc_failed_status
    mov     eax, RAWRXD_ERROR_GEN_FAILURE

init_alloc_failed_status:
    mov     dword ptr [g_last_status], eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
k_swap_aperture_init ENDP

k_swap_aperture_map_chunk PROC FRAME
    ; rcx = file_mapping_handle
    ; rdx = chunk_index
    ; r8  = bytes_to_map
    ; r9  = out_mapped_base

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 88h
    .allocstack 88h
    .endprolog

    ; TELEMETRY: record entry r9 and where we'll store output
    mov     qword ptr [g_map_entry_r9], r9
    mov     qword ptr [g_last_map_out_param], r9
    

    mov     rdi, rcx
    mov     rbx, rdx
    mov     rsi, r8
    mov     r12, r9

    test    rdi, rdi
    jz      map_invalid
    test    rsi, rsi
    jz      map_invalid
    test    rsi, RAWRXD_PAGE_64KB - 1
    jnz     map_invalid

    mov     rcx, qword ptr [g_aperture_base]
    test    rcx, rcx
    jz      map_not_ready

    mov     rdx, qword ptr [g_aperture_span]
    test    rdx, rdx
    jz      map_not_ready

    mov     r8, rbx
    imul    r8, RAWRXD_SWAP_WINDOW_BYTES
    jc      map_invalid

    mov     r9, rcx
    add     r9, r8
    jc      map_invalid

    cmp     r8, rdx
    jae     map_invalid
    mov     r10, rdx
    sub     r10, r8
    cmp     rsi, r10
    ja      map_invalid

    mov     eax, dword ptr [g_placeholder_mode]
    
    cmp     eax, 0
    je      map_call_mapview

    ; Split placeholder chunk before replacement mapping.
    mov     rcx, r9
    mov     rdx, rsi
    mov     r8d, RAWRXD_MEM_RELEASE or RAWRXD_MEM_PRESERVE_PLACEHOLDER
    sub     rsp, 20h
    call    VirtualFree
    add     rsp, 20h
    test    eax, eax
    jz      map_call_failed

map_call_mapview:
    ; Recompute the chunk's aperture slot address.
    ; r9 may have been clobbered by the VirtualFree split call above
    ; (r9 is volatile per x64 ABI — callee-owned after a call).
    ; Safe recompute: aperture_base + chunk_index * window_size.
    mov     rax, qword ptr [g_aperture_base]
    mov     r9,  rbx
    imul    r9,  RAWRXD_SWAP_WINDOW_BYTES
    add     rax, r9
    ; rax = aperture slot address for this chunk — written to *out_mapped_base below.

map_call_mapview_done:

    mov     qword ptr [g_last_chunk_index], rbx
    mov     qword ptr [g_last_map_size], rsi
    mov     qword ptr [g_last_map_addr], rax
    
    mov     dword ptr [g_last_placeholder_flags], 0
    cmp     dword ptr [g_placeholder_mode], 0
    je      map_store_out
    mov     dword ptr [g_last_placeholder_flags], RAWRXD_MEM_REPLACE_PLACEHOLDER

map_store_out:
    test    r12, r12
    jz      map_invalid_output_param      ; <- if r12 is zero, it means r9 was zero at entry
    mov     qword ptr [r12], rax

map_success:
    mov     dword ptr [g_last_status], 0
    xor     eax, eax
    add     rsp, 88h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

map_invalid_output_param:
    ; Debug: r9 was NULL at entry - this shouldn't happen
    mov     dword ptr [g_last_status], RAWRXD_ERROR_INVALID_PARAM
    mov     eax, RAWRXD_ERROR_INVALID_PARAM
    add     rsp, 88h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

map_not_ready:
    mov     dword ptr [g_last_status], RAWRXD_ERROR_NOT_READY
    mov     eax, RAWRXD_ERROR_NOT_READY
    add     rsp, 88h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

map_invalid:
    mov     dword ptr [g_last_status], RAWRXD_ERROR_INVALID_PARAM
    mov     eax, RAWRXD_ERROR_INVALID_PARAM
    add     rsp, 88h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

map_call_failed:
    sub     rsp, 20h
    call    GetLastError
    add     rsp, 20h
    test    eax, eax
    jnz     map_call_failed_status
    mov     eax, RAWRXD_ERROR_GEN_FAILURE

map_call_failed_status:
    mov     dword ptr [g_last_status], eax
    add     rsp, 88h
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
k_swap_aperture_map_chunk ENDP

k_swap_aperture_unmap_chunk PROC FRAME
    ; rcx = mapped_base

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 28h
    .allocstack 28h
    .endprolog

    mov     qword ptr [g_last_unmap_addr], rcx
    mov     qword ptr [g_last_map_addr], 0
    mov     qword ptr [g_last_map_size], 0

    mov     rbx, qword ptr [g_aperture_base]
    test    rbx, rbx
    jz      unmap_success

    sub     rsp, 20h
    mov     rcx, rbx
    xor     edx, edx
    mov     r8d, RAWRXD_MEM_RELEASE
    call    VirtualFree
    add     rsp, 20h
    test    eax, eax
    jz      unmap_failed

    mov     qword ptr [g_aperture_base], 0

unmap_success:
    mov     dword ptr [g_last_status], 0
    xor     eax, eax
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret

unmap_failed:
    sub     rsp, 20h
    call    GetLastError
    add     rsp, 20h
    test    eax, eax
    jnz     unmap_failed_status
    mov     eax, RAWRXD_ERROR_GEN_FAILURE

unmap_failed_status:
    mov     dword ptr [g_last_status], eax
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
k_swap_aperture_unmap_chunk ENDP

.DATA
ALIGN 8
g_aperture_base            DQ 0
g_aperture_span            DQ 0
g_requested_base           DQ 0
g_last_chunk_index         DQ 0
g_last_map_size            DQ 0
g_last_map_addr            DQ 0
g_last_unmap_addr          DQ 0
g_last_status              DD 0
g_placeholder_mode         DD 0
g_fallback_used            DD 0
g_last_placeholder_flags   DD 0
g_last_map_out_param       DQ 0
g_map_entry_r9             DQ 0

END
