; RawrXD_Enh4_KVCacheCompression.asm
OPTION CASEMAP:NONE

.DATA
g_kv_bytes_per_token QWORD 0
g_max_seq_len QWORD 0

.CODE
KVCacheCompression_Initialize PROC
    mov rax, rcx
    imul rax, rdx
    imul rax, r8
    shl rax, 3
    mov g_kv_bytes_per_token, rax
    mov g_max_seq_len, r9
    xor eax, eax
    ret
KVCacheCompression_Initialize ENDP

KVCacheCompression_UpdateScores PROC
    xor eax, eax
    ret
KVCacheCompression_UpdateScores ENDP

KVCacheCompression_CompressTier PROC
    xor eax, eax
    ret
KVCacheCompression_CompressTier ENDP
END
