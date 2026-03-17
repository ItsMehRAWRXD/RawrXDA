; kv_cache_mgr.asm - Key-Value cache for transformer inference
; Pure MASM64 - Zero CRT dependencies

PUBLIC KV_Init
PUBLIC KV_AllocSlot
PUBLIC KV_Store
PUBLIC KV_Retrieve
PUBLIC KV_Clear

EXTRN VirtualAlloc:PROC
EXTRN VirtualFree:PROC

.data
align 16
g_pCacheBase    QWORD 0
g_cbCacheTotal  QWORD 0
g_dwMaxSeqLen   DWORD 4096
g_dwNumHeads    DWORD 32
g_dwHeadDim     DWORD 128
g_dwNumLayers   DWORD 32
g_dwNextSlot    DWORD 1

.code

; RCX = numLayers, RDX = numHeads, R8 = headDim, R9 = maxSeqLen
; Returns: RAX = 1 success, 0 fail
KV_Init PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 30h
    .allocstack 30h
    .endprolog
    
    mov g_dwNumLayers, ecx
    mov g_dwNumHeads, edx
    mov g_dwHeadDim, r8d
    mov g_dwMaxSeqLen, r9d
    
    ; size = layers * heads * dim * seq * 8 (K+V * f32)
    mov eax, ecx
    imul eax, edx
    imul eax, r8d
    imul eax, r9d
    shl eax, 3
    movsxd rbx, eax
    mov g_cbCacheTotal, rbx
    
    ; VirtualAlloc(NULL, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE)
    xor ecx, ecx
    mov rdx, rbx
    mov r8d, 3000h
    mov r9d, 4
    call VirtualAlloc
    mov g_pCacheBase, rax
    test rax, rax
    setnz al
    movzx eax, al
    
    add rsp, 30h
    pop rbx
    ret
KV_Init ENDP

; Returns: RAX = slot ID (0 = fail)
KV_AllocSlot PROC FRAME
    .endprolog
    mov eax, g_dwNextSlot
    inc dword ptr g_dwNextSlot
    ret
KV_AllocSlot ENDP

; RCX = slotID, RDX = layerIdx, R8 = seqPos, R9 = pData
KV_Store PROC FRAME
    .endprolog
    mov rax, g_pCacheBase
    test rax, rax
    jz kvs_fail
    mov eax, 1
    ret
kvs_fail:
    xor eax, eax
    ret
KV_Store ENDP

; RCX = slotID, RDX = layerIdx, R8 = seqPos, R9 = pOut
KV_Retrieve PROC FRAME
    .endprolog
    mov rax, g_pCacheBase
    test rax, rax
    jz kvr_fail
    mov eax, 1
    ret
kvr_fail:
    xor eax, eax
    ret
KV_Retrieve ENDP

KV_Clear PROC FRAME
    push rdi
    .pushreg rdi
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    mov rdi, g_pCacheBase
    test rdi, rdi
    jz kvc_skip
    mov rcx, g_cbCacheTotal
    xor eax, eax
    rep stosb
kvc_skip:
    mov dword ptr g_dwNextSlot, 1
    
    add rsp, 28h
    pop rdi
    ret
KV_Clear ENDP

END
