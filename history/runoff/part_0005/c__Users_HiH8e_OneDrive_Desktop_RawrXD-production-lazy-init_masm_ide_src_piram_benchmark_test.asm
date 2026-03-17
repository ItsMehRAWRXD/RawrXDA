; ============================================================================
; PIRAM_BENCHMARK_TEST.ASM - Comprehensive π-RAM Benchmark Runner
; Loads and compresses test data, displays results
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; Benchmark functions
BenchPiRam_CompressLarge PROTO
BenchPiRam_MeasureRatio PROTO
BenchPiRam_Throughput PROTO :DWORD

; Win32 console I/O (for output)
GetStdHandle PROTO :DWORD
WriteConsoleA PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
ExitProcess PROTO :DWORD

.data
    STD_OUTPUT_HANDLE equ -11
    
    szBanner db "π-RAM Ultra-Minimal Compression Benchmark",13,10
             db "=========================================",13,10,0
    
    szTestStart db "[*] Starting 1MB compression test...",13,10,0
    szTestDone  db "[✓] Compression complete!",13,10,0
    szRatio     db "[✓] Compression ratio: ",0
    szPercent   db "%",13,10,0
    szMBSec     db " MB/sec",13,10,0
    szFail      db "[✗] Test FAILED",13,10,0
    
.data?
    hStdOut     dd ?
    dwWritten   dd ?
    szBuffer    db 256 dup(?)

.code

; Print string to console (null-terminated)
PrintString proc pStr:DWORD
    push esi
    push edi
    push ebx
    
    ; Find length
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
    
    ; Write to console
    invoke WriteConsoleA, hStdOut, pStr, ecx, addr dwWritten, NULL
    
@@exit:
    pop ebx
    pop edi
    pop esi
    ret
PrintString endp

; Print decimal number (32-bit, max 10 digits)
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
    
    ; Run compression test
    call BenchPiRam_CompressLarge
    test eax, eax
    jz @@test_fail
    
    ; Print success
    invoke PrintString, addr szTestDone
    
    ; Print ratio
    invoke PrintString, addr szRatio
    call BenchPiRam_MeasureRatio
    invoke PrintDec, eax
    invoke PrintString, addr szPercent
    
    ; Exit with ratio as code (for verification)
    call BenchPiRam_MeasureRatio
    invoke ExitProcess, eax
    
@@test_fail:
    invoke PrintString, addr szFail
    invoke ExitProcess, 1

end start
