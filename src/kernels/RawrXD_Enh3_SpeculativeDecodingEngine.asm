; RawrXD_Enh3_SpeculativeDecodingEngine.asm
OPTION CASEMAP:NONE

.DATA
g_num_heads DWORD 0
g_hidden_dim DWORD 0
g_vocab_size DWORD 0

.CODE
SpeculativeDecoding_Initialize PROC
    mov g_num_heads, r8d
    mov g_hidden_dim, ecx
    mov g_vocab_size, edx
    xor eax, eax
    ret
SpeculativeDecoding_Initialize ENDP

SpeculativeDecoding_GenerateDrafts PROC
    xor eax, eax
    ret
SpeculativeDecoding_GenerateDrafts ENDP

SpeculativeDecoding_VerifyDrafts PROC
    xor eax, eax
    ret
SpeculativeDecoding_VerifyDrafts ENDP
END
