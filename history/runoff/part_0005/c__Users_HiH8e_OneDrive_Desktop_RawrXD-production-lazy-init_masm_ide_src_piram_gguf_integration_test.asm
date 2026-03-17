; ============================================================================
; PIRAM_GGUF_INTEGRATION_TEST.ASM - Full GGUF + π-RAM Integration Test
; Loads a GGUF model and compresses it with π-RAM, reports metrics
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc
include piram_compress.inc

; GGUF Loader functions
GGUF_LoadModel PROTO :DWORD
GGUF_CloseModel PROTO :DWORD

; Win32 console I/O
GetStdHandle PROTO :DWORD
WriteConsoleA PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
ExitProcess PROTO :DWORD

.data
    STD_OUTPUT_HANDLE equ -11

    szBanner        db "π-RAM GGUF Integration Test",13,10
                    db "==========================",13,10,0

    szTestStart     db "[*] Loading GGUF model...",13,10,0
    szLoadSuccess   db "[✓] GGUF model loaded successfully",13,10,0
    szCompressStart db "[*] Applying π-RAM compression...",13,10,0
    szCompressDone  db "[✓] π-RAM compression applied",13,10,0
    szRatio         db "[✓] Compression ratio: ",0
    szPercent       db "%",13,10,0
    szCleanup       db "[*] Cleaning up...",13,10,0
    szDone          db "[✓] Test completed successfully",13,10,0
    szLoadFail      db "[✗] Failed to load GGUF model",13,10,0
    szCompressFail  db "[✗] π-RAM compression failed",13,10,0

    ; Test model path (placeholder - would be passed as argument)
    szTestModel     db "test.gguf",0

.data?
    hStdOut         dd ?
    dwWritten       dd ?
    hModel          dd ?

.code

; Print string to console
PrintString proc pStr:DWORD
    push esi
    push edi
    push ebx
    
    mov esi, pStr
    test esi, esi
    jz @@exit
    
    xor ecx, ecx
@@len_loop:
    cmp byte ptr [esi + ecx], 0
    je @@len_done
    inc ecx
    cmp ecx, 1024 ; safety limit
    jb @@len_loop
@@len_done:
    
    invoke WriteConsoleA, hStdOut, pStr, ecx, addr dwWritten, NULL
    
@@exit:
    pop ebx
    pop edi
    pop esi
    ret
PrintString endp

; Print decimal number
PrintDec proc dwVal:DWORD
    LOCAL szNum[16]:BYTE
    push esi
    push edi
    push ebx

    mov eax, dwVal
    lea edi, [szNum + 15]
    mov byte ptr [edi], 0
    dec edi
    
    mov ecx, 10
    
@@div_loop:
    xor edx, edx
    div ecx
    add dl, '0'
    mov [edi], dl
    dec edi
    test eax, eax
    jnz @@div_loop

    inc edi
    invoke PrintString, edi

    pop ebx
    pop edi
    pop esi
    ret
PrintDec endp

start:
    ; Get stdout handle
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax

    ; Print banner
    invoke PrintString, addr szBanner

    ; Print test start
    invoke PrintString, addr szTestStart

    ; Load GGUF model
    invoke GGUF_LoadModel, addr szTestModel
    test eax, eax
    jz @@load_fail

    mov hModel, eax

    ; Print load success
    invoke PrintString, addr szLoadSuccess

    ; Print compression start
    invoke PrintString, addr szCompressStart

    ; Enable RAM halving
    invoke PiRam_EnableHalving, TRUE

    ; Compress the model
    invoke PiRam_CompressGGUF, hModel
    test eax, eax
    jz @@compress_fail

    ; Print compression done
    invoke PrintString, addr szCompressDone

    ; Get and print compression ratio
    invoke PrintString, addr szRatio
    invoke PiRam_GetCompressionRatio
    invoke PrintDec, eax
    invoke PrintString, addr szPercent

    ; Cleanup
    invoke PrintString, addr szCleanup
    invoke GGUF_CloseModel, hModel

    ; Print done
    invoke PrintString, addr szDone

    ; Exit with ratio as code
    invoke PiRam_GetCompressionRatio
    invoke ExitProcess, eax

@@load_fail:
    invoke PrintString, addr szLoadFail
    invoke ExitProcess, 1

@@compress_fail:
    invoke PrintString, addr szCompressFail
    ; Still cleanup if model was loaded
    cmp hModel, 0
    je @@exit_fail
    invoke GGUF_CloseModel, hModel
@@exit_fail:
    invoke ExitProcess, 2

end start
