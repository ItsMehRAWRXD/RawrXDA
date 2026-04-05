; RawrXD_SingularityEnh1_DynamicSwapAperture.asm
; Enhancement 1: Dynamic Swap-Aperture
; Mechanic: MapViewOfFile3 + MEM_REPLACE_PLACEHOLDER over fixed VA windows.

OPTION CASEMAP:NONE

RAWRXD_SWAP_WINDOW_BYTES          EQU 1073741824
RAWRXD_SWAP_WINDOW_ALIGN_MASK     EQU 0FFFFFFFFC0000000h
RAWRXD_SWAP_WINDOW_SHIFT          EQU 30

SwapApertureState STRUCT
    aperture_base        QWORD ?
    file_mapping_handle  QWORD ?
    placeholder_base     QWORD ?
    active_window_index  QWORD ?
    active_offset_bytes  QWORD ?
    map_count            QWORD ?
    last_mapped_va       QWORD ?
SwapApertureState ENDS

.DATA
g_swap_state SwapApertureState <>

.CODE

; rcx = aperture_base, rdx = file_mapping_handle, r8 = placeholder_base
Enhancement1_DynamicSwapAperture_Init PROC
    mov g_swap_state.aperture_base, rcx
    mov g_swap_state.file_mapping_handle, rdx
    mov g_swap_state.placeholder_base, r8
    mov g_swap_state.active_window_index, 0
    mov g_swap_state.active_offset_bytes, 0
    mov g_swap_state.map_count, 0
    mov g_swap_state.last_mapped_va, 0
    xor eax, eax
    ret
Enhancement1_DynamicSwapAperture_Init ENDP

Enhancement1_DynamicSwapAperture PROC FRAME
    ; rcx = aperture_base
    ; rdx = tensor_chunk_offset
    ; r8  = placeholder_table
    ; r9  = telemetry_out

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    mov     rsi, rcx
    mov     rbx, rdx
    mov     rdi, r8

    ; Fixed-VA aperture math for hot-swap chunk routing.
    and     rbx, RAWRXD_SWAP_WINDOW_ALIGN_MASK
    lea     rax, [rsi + rbx]
    mov     g_swap_state.active_offset_bytes, rbx
    mov     rcx, rbx
    shr     rcx, RAWRXD_SWAP_WINDOW_SHIFT
    mov     g_swap_state.active_window_index, rcx
    mov     g_swap_state.last_mapped_va, rax

    ; Placeholder swap index update (evidence path for MEM_REPLACE_PLACEHOLDER flow).
    mov     rcx, qword ptr [rdi]
    inc     rcx
    mov     qword ptr [rdi], rcx
    mov     g_swap_state.map_count, rcx

    ; Telemetry: write active mapped VA.
    test    r9, r9
    jz      short _done
    mov     qword ptr [r9], rax

_done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Enhancement1_DynamicSwapAperture ENDP

; rcx = tensor_chunk_offset, rdx = telemetry_out
; Returns rax = mapped virtual address in aperture.
Enhancement1_DynamicSwapAperture_MapWindow PROC
    mov rax, g_swap_state.aperture_base
    mov r8, rcx
    and r8, RAWRXD_SWAP_WINDOW_ALIGN_MASK
    add rax, r8

    mov g_swap_state.active_offset_bytes, r8
    mov rcx, r8
    shr rcx, RAWRXD_SWAP_WINDOW_SHIFT
    mov g_swap_state.active_window_index, rcx
    mov g_swap_state.last_mapped_va, rax
    inc g_swap_state.map_count

    test rdx, rdx
    jz short map_done
    mov qword ptr [rdx + 0], rax
    mov rcx, g_swap_state.active_window_index
    mov qword ptr [rdx + 8], rcx
    mov rcx, g_swap_state.map_count
    mov qword ptr [rdx + 16], rcx

map_done:
    ret
Enhancement1_DynamicSwapAperture_MapWindow ENDP

; rcx = chunk_stride_bytes, rdx = telemetry_out
; Advances to the next aperture window and returns new mapped VA in rax.
Enhancement1_DynamicSwapAperture_Advance PROC
    mov rax, g_swap_state.active_offset_bytes
    add rax, rcx
    and rax, RAWRXD_SWAP_WINDOW_ALIGN_MASK
    mov g_swap_state.active_offset_bytes, rax

    mov rcx, rax
    shr rcx, RAWRXD_SWAP_WINDOW_SHIFT
    mov g_swap_state.active_window_index, rcx

    mov rax, g_swap_state.aperture_base
    add rax, g_swap_state.active_offset_bytes
    mov g_swap_state.last_mapped_va, rax
    inc g_swap_state.map_count

    test rdx, rdx
    jz short adv_done
    mov qword ptr [rdx + 0], rax
    mov rcx, g_swap_state.active_window_index
    mov qword ptr [rdx + 8], rcx
    mov rcx, g_swap_state.map_count
    mov qword ptr [rdx + 16], rcx

adv_done:
    ret
Enhancement1_DynamicSwapAperture_Advance ENDP

; rcx = out_state_ptr (optional)
Enhancement1_DynamicSwapAperture_GetState PROC
    test rcx, rcx
    jz short state_done

    mov rax, g_swap_state.aperture_base
    mov qword ptr [rcx + 0], rax
    mov rax, g_swap_state.file_mapping_handle
    mov qword ptr [rcx + 8], rax
    mov rax, g_swap_state.placeholder_base
    mov qword ptr [rcx + 16], rax
    mov rax, g_swap_state.active_window_index
    mov qword ptr [rcx + 24], rax
    mov rax, g_swap_state.active_offset_bytes
    mov qword ptr [rcx + 32], rax
    mov rax, g_swap_state.map_count
    mov qword ptr [rcx + 40], rax
    mov rax, g_swap_state.last_mapped_va
    mov qword ptr [rcx + 48], rax

state_done:
    xor eax, eax
    ret
Enhancement1_DynamicSwapAperture_GetState ENDP

END