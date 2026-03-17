; ============================================================================
; PIRAM_REVERSE_BYPASS.ASM - Reverse Bypass Mode for MASM LOCAL Issues
; Automatic memory addressing without manual LOCAL assignments
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; ============================================================================
; EXPORTS
; ============================================================================
PUBLIC ReverseBP_Init
PUBLIC ReverseBP_AllocStack
PUBLIC ReverseBP_FreeStack
PUBLIC ReverseBP_SetValue
PUBLIC ReverseBP_GetValue
PUBLIC ReverseBP_CopyBuffer

; ============================================================================
; CONSTANTS
; ============================================================================
MAX_STACK_FRAMES    equ 64
FRAME_SIZE          equ 512     ; Max per frame
FRAME_BUFFER_SIZE   equ 32768   ; 64 frames × 512 bytes

; ============================================================================
; DATA
; ============================================================================
.data
    ; Global reverse bypass stack
    g_RB_StackPtr       dd 0
    g_RB_FrameCount     dd 0
    g_RB_EnableBypass   dd 1     ; Reverse bypass mode enabled
    
    ; Pre-allocated buffer for stack frames
    g_RB_FrameBuffer    dd 0

.data?
    ; Frame stack (up to 64 frames of 512 bytes each = 32KB)
    g_RB_Frames         dd MAX_STACK_FRAMES dup(?)
    
; ============================================================================
; CODE
; ============================================================================
.code

; ============================================================================
; ReverseBP_Init - Initialize reverse bypass mode
; Returns: EAX = 1 success
; ============================================================================
ReverseBP_Init PROC
    mov g_RB_StackPtr, 0
    mov g_RB_FrameCount, 0
    mov g_RB_EnableBypass, 1
    mov eax, 1
    ret
ReverseBP_Init ENDP

; ============================================================================
; ReverseBP_AllocStack - Allocate a stack frame (returns pointer in EAX)
; dwSize: Size needed (max 512)
; Returns: EAX = frame pointer, 0 on error
; ============================================================================
ReverseBP_AllocStack PROC dwSize:DWORD
    push esi
    
    ; Check size
    mov eax, dwSize
    cmp eax, FRAME_SIZE
    jg @alloc_fail
    
    ; Check frame count
    mov ecx, g_RB_FrameCount
    cmp ecx, MAX_STACK_FRAMES
    jge @alloc_fail
    
    ; Get current frame offset
    mov eax, ecx
    imul eax, FRAME_SIZE
    
    ; Return frame address (base + offset)
    lea esi, g_RB_Frames
    add eax, esi
    
    ; Increment frame counter
    inc g_RB_FrameCount
    mov edx, g_RB_FrameCount
    cmp g_RB_FrameCount, edx
    
    jmp @alloc_done
    
@alloc_fail:
    xor eax, eax
    
@alloc_done:
    pop esi
    ret
ReverseBP_AllocStack ENDP

; ============================================================================
; ReverseBP_FreeStack - Free (pop) last frame
; Returns: EAX = 1 success
; ============================================================================
ReverseBP_FreeStack PROC
    mov eax, g_RB_FrameCount
    test eax, eax
    jz @free_fail
    
    dec eax
    mov g_RB_FrameCount, eax
    mov eax, 1
    ret
    
@free_fail:
    xor eax, eax
    ret
ReverseBP_FreeStack ENDP

; ============================================================================
; ReverseBP_SetValue - Set DWORD value in current frame at offset
; dwOffset: Offset within frame
; dwValue: Value to set
; Returns: EAX = 1 success
; ============================================================================
ReverseBP_SetValue PROC dwOffset:DWORD, dwValue:DWORD
    push esi
    
    ; Get current frame
    mov eax, g_RB_FrameCount
    test eax, eax
    jz @set_fail
    
    dec eax
    imul eax, FRAME_SIZE
    lea esi, g_RB_Frames
    add esi, eax
    
    ; Check offset
    mov eax, dwOffset
    cmp eax, FRAME_SIZE
    jge @set_fail
    
    ; Set value
    mov eax, dwValue
    mov [esi + dwOffset], eax
    
    mov eax, 1
    jmp @set_done
    
@set_fail:
    xor eax, eax
    
@set_done:
    pop esi
    ret
ReverseBP_SetValue ENDP

; ============================================================================
; ReverseBP_GetValue - Get DWORD value from current frame at offset
; dwOffset: Offset within frame
; Returns: EAX = value
; ============================================================================
ReverseBP_GetValue PROC dwOffset:DWORD
    push esi
    
    ; Get current frame
    mov eax, g_RB_FrameCount
    test eax, eax
    jz @get_fail
    
    dec eax
    imul eax, FRAME_SIZE
    lea esi, g_RB_Frames
    add esi, eax
    
    ; Check offset
    mov eax, dwOffset
    cmp eax, FRAME_SIZE
    jge @get_fail
    
    ; Get value
    mov eax, [esi + dwOffset]
    jmp @get_done
    
@get_fail:
    xor eax, eax
    
@get_done:
    pop esi
    ret
ReverseBP_GetValue ENDP

; ============================================================================
; ReverseBP_CopyBuffer - Copy buffer to/from current frame
; pBuffer: Source/dest buffer
; dwSize: Size to copy
; dwFrameOffset: Frame offset
; bToFrame: TRUE=copy to frame, FALSE=copy from frame
; Returns: EAX = bytes copied
; ============================================================================
ReverseBP_CopyBuffer PROC pBuffer:DWORD, dwSize:DWORD, dwFrameOffset:DWORD, bToFrame:DWORD
    push esi
    push edi
    
    ; Get current frame
    mov eax, g_RB_FrameCount
    test eax, eax
    jz @copy_fail
    
    dec eax
    imul eax, FRAME_SIZE
    lea esi, g_RB_Frames
    add esi, eax
    add esi, dwFrameOffset
    
    mov edi, pBuffer
    mov ecx, dwSize
    
    ; Check direction
    mov eax, bToFrame
    test eax, eax
    jz @copy_from_frame
    
    ; Copy TO frame (buffer → frame)
    xchg esi, edi
    rep movsb
    jmp @copy_done
    
@copy_from_frame:
    ; Copy FROM frame (frame → buffer)
    rep movsb
    
@copy_done:
    mov eax, dwSize
    jmp @copy_exit
    
@copy_fail:
    xor eax, eax
    
@copy_exit:
    pop edi
    pop esi
    ret
ReverseBP_CopyBuffer ENDP

END
