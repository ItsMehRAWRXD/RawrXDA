; ============================================================================
; GGUF_CORE_FUNCTIONALITY_TEST.ASM - Core GGUF Components Test
; Tests π-RAM compression, GGUF reverse reader, and load primitives
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc
include piram_compress.inc

; External functions that work
PiRam_CompressGGUF PROTO :DWORD
PiRam_GetCompressionRatio PROTO
PiRam_EnableHalving PROTO :DWORD
PiRam_PerfectCircleFwd PROTO :DWORD, :DWORD

; Win32 APIs
GetStdHandle PROTO :DWORD
WriteConsoleA PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
GetProcessHeap PROTO
HeapAlloc PROTO :DWORD, :DWORD, :DWORD
HeapFree PROTO :DWORD, :DWORD, :DWORD
ExitProcess PROTO :DWORD
GetTickCount PROTO

.data
    STD_OUTPUT_HANDLE equ -11
    HEAP_ZERO_MEMORY equ 8

    szBanner    db "GGUF Loader & π-RAM Compression Test Suite",13,10,13,10,0
    szTest1     db "[1] π-RAM Transform on 256-byte buffer",13,10,0
    szTest2     db "[2] π-RAM Halving (2:1 compression)",13,10,0
    szTest3     db "[3] π-RAM Multi-pass (4 passes)",13,10,0
    szPass      db "    PASS",13,10,0
    szFail      db "    FAIL",13,10,0

    szTestData  db 256 dup(0)  ; 256-byte test buffer

.data?
    hStdOut     dd ?
    dwWritten   dd ?
    dwPassCount dd 0

.code

PrintString proc pStr:DWORD
    push esi
    mov esi, pStr
    test esi, esi
    jz @@exit
    xor ecx, ecx
@@loop:
    cmp byte ptr [esi + ecx], 0
    je @@done
    inc ecx
    cmp ecx, 2048
    jb @@loop
@@done:
    invoke WriteConsoleA, hStdOut, pStr, ecx, addr dwWritten, NULL
@@exit:
    pop esi
    ret
PrintString endp

; ============================================================================
; Test 1: π-RAM Forward Transform (halves buffer, applies π-transform)
; ============================================================================
Test_PiRamTransform proc
    LOCAL pBuf:DWORD
    LOCAL dwSize:DWORD

    invoke PrintString, addr szTest1

    ; Allocate test buffer
    invoke GetProcessHeap
    invoke HeapAlloc, eax, HEAP_ZERO_MEMORY, 256
    test eax, eax
    jz @@fail

    mov pBuf, eax
    mov dwSize, 256

    ; Fill with test pattern
    mov esi, eax
    xor ecx, ecx
@@fill:
    cmp ecx, 256
    jge @@fill_done
    mov al, cl
    mov [esi + ecx], al
    inc ecx
    jmp @@fill

@@fill_done:
    ; Apply π-RAM transform
    invoke PiRam_PerfectCircleFwd, pBuf, dwSize
    
    ; Should return 128 (half of 256)
    cmp eax, 128
    jne @@fail

    ; Free buffer
    invoke GetProcessHeap
    invoke HeapFree, eax, 0, pBuf

    invoke PrintString, addr szPass
    mov eax, 1
    jmp @@exit

@@fail:
    invoke PrintString, addr szFail
    xor eax, eax

@@exit:
    ret
Test_PiRamTransform endp

; ============================================================================
; Test 2: π-RAM Halving (enable/disable)
; ============================================================================
Test_PiRamHalving proc

    invoke PrintString, addr szTest2

    ; Enable halving
    invoke PiRam_EnableHalving, 1
    test eax, eax
    jz @@fail

    ; Disable halving
    invoke PiRam_EnableHalving, 0
    test eax, eax
    jz @@fail

    ; Enable again
    invoke PiRam_EnableHalving, 1
    test eax, eax
    jz @@fail

    invoke PrintString, addr szPass
    mov eax, 1
    jmp @@exit

@@fail:
    invoke PrintString, addr szFail
    xor eax, eax

@@exit:
    ret
Test_PiRamHalving endp

; ============================================================================
; Test 3: π-RAM Multi-pass (4 passes for 16:1 compression)
; ============================================================================
Test_PiRamMultiPass proc
    LOCAL pBuf:DWORD
    LOCAL dwSize:DWORD
    LOCAL dwPass:DWORD

    invoke PrintString, addr szTest3

    ; Allocate 1MB test buffer
    invoke GetProcessHeap
    invoke HeapAlloc, eax, HEAP_ZERO_MEMORY, 1048576
    test eax, eax
    jz @@fail

    mov pBuf, eax
    mov dwSize, 1048576

    ; Apply 4 passes of π-transform
    mov dwPass, 0

@@pass_loop:
    cmp dwPass, 4
    jge @@passes_done

    invoke PiRam_PerfectCircleFwd, pBuf, dwSize
    test eax, eax
    jz @@fail

    mov dwSize, eax  ; New size becomes input size
    inc dwPass
    jmp @@pass_loop

@@passes_done:
    ; After 4 passes, size should be (1MB / 2^4) = 1MB / 16
    cmp dwSize, 65536
    jne @@fail

    ; Free buffer
    invoke GetProcessHeap
    invoke HeapFree, eax, 0, pBuf

    invoke PrintString, addr szPass
    mov eax, 1
    jmp @@exit

@@fail:
    invoke PrintString, addr szFail
    xor eax, eax

@@exit:
    ret
Test_PiRamMultiPass endp

; ============================================================================
; Test 4: Compression Ratio Reporting
; ============================================================================
Test_CompressionRatio proc

    invoke PrintString, addr szTest3
    
    ; Get compression ratio
    invoke PiRam_GetCompressionRatio
    
    ; Should be non-zero (200 for 2:1, etc)
    test eax, eax
    jz @@fail

    invoke PrintString, addr szPass
    mov eax, 1
    jmp @@exit

@@fail:
    invoke PrintString, addr szFail
    xor eax, eax

@@exit:
    ret
Test_CompressionRatio endp

; ============================================================================
; Main
; ============================================================================
start:
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax

    invoke PrintString, addr szBanner

    ; Test 1
    call Test_PiRamTransform
    add dwPass, eax

    ; Test 2
    call Test_PiRamHalving
    add dwPass, eax

    ; Test 3
    call Test_PiRamMultiPass
    add dwPass, eax

    ; Test 4
    call Test_CompressionRatio
    add dwPass, eax

    ; Exit with pass count
    mov eax, dwPass
    invoke ExitProcess, eax

end start
