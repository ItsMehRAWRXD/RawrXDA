; RawrXD 800B+ Optimized Memory Operations
; Target: x64 MASM
; Features: AVX-512 Non-Temporal Stores, Corruption Scanning

.code

; void RawrxdCopyNonTemporal(void* dest, const void* src, size_t size)
RawrxdCopyNonTemporal PROC
    ; rcx = dest, rdx = src, r8 = size
    
    ; Check if size is large enough for AVX-512 (64 bytes)
    cmp r8, 64
    jb @small_copy
    
    ; Align to 64 bytes
@loop_avx512:
    vmovups zmm0, zmmword ptr [rdx]
    vmovntdq zmmword ptr [rcx], zmm0
    add rcx, 64
    add rdx, 64
    sub r8, 64
    cmp r8, 64
    jae @loop_avx512
    
@small_copy:
    test r8, r8
    jz @done
    rep movsb
    
@done:
    sfence    ; Ensure non-temporal stores are visible
    ret
RawrxdCopyNonTemporal ENDP

; uint32_t RawrxdQuickChecksum(const void* data, size_t size)
RawrxdQuickChecksum PROC
    ; rcx = data, rdx = size
    xor eax, eax
    test rdx, rdx
    jz @done
    
@loop:
    add eax, [rcx]
    rol eax, 1
    add rcx, 4
    sub rdx, 4
    ja @loop
    
@done:
    ret
RawrxdQuickChecksum ENDP

END
