; rawrxd_probe - Standalone GGUF metadata analyzer
; Pure x64 MASM with no external includes

OPTION CASEMAP:NONE

CONST_GGUF_MAGIC    EQU 0x46554747h  ; "GGUF"
CONST_MAX_STR_LEN   EQU 10485760     ; 10MB safety limit
STD_OUTPUT_HANDLE   EQU -11

.CODE
MAIN PROC
    mov     rax, 0  ; Normal exit
    ret
MAIN ENDP
END MAIN
