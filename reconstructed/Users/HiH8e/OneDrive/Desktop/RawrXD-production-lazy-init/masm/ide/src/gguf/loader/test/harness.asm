; ============================================================================
; GGUF_LOADER_TEST_HARNESS.ASM - Comprehensive GGUF Loader Test Suite
; Tests gguf_loader_working with π-RAM compression, reverse reading, and streaming
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc
include piram_compress.inc

; GGUF Loaders & Components
GGUF_LoadModel PROTO :DWORD
GGUF_CloseModel PROTO :DWORD
GGUF_ReverseReadHeader PROTO :DWORD, :DWORD
GGUF_ReverseGetMetadata PROTO :DWORD, :DWORD

; π-RAM Compression
PiRam_CompressGGUF PROTO :DWORD
PiRam_GetCompressionRatio PROTO
PiRam_EnableHalving PROTO :DWORD

; Win32 APIs
GetStdHandle PROTO :DWORD
WriteConsoleA PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
GetTickCount PROTO
ExitProcess PROTO :DWORD

.data
    STD_OUTPUT_HANDLE equ -11

    ; Test banner
    szBanner    db "╔════════════════════════════════════════╗",13,10
                db "║  GGUF LOADER COMPREHENSIVE TEST SUITE  ║",13,10
                db "╚════════════════════════════════════════╝",13,10,13,10,0

    ; Test descriptions
    szTest1     db "[TEST 1] GGUF Loader (working)",13,10,0
    szTest2     db "[TEST 2] GGUF Reverse Reader",13,10,0
    szTest3     db "[TEST 3] π-RAM Compression Integration",13,10,0
    szTest4     db "[TEST 4] RAM Halving & Streaming",13,10,0

    ; Test results
    szPass      db "  PASS",13,10,0
    szFail      db "  FAIL",13,10,0
    szSkip      db "  SKIP",13,10,0

    szModelLoaded   db "    - Model loaded",13,10,0
    szBytesCompress db "    - Compression ratio: ",0
    szPercent       db "%",13,10,0
    szSeparator     db "---",13,10,0

    ; Test model (would be a real GGUF file in production)
    szTestModel db "test.gguf",0

    szSummary   db "===========================================",13,10,0
    szTests     db "Tests Passed: ",0
    szOf        db " of ",0
    szElapsed   db " ms",13,10,0

.data?
    hStdOut     dd ?
    dwWritten   dd ?
    dwPass      dd 0
    dwTotal     dd 0
    dwStartTime dd ?
    dwEndTime   dd ?
    hModel      dd ?

.code

; ============================================================================
; Utility Functions
; ============================================================================

PrintString proc pStr:DWORD
    push esi
    mov esi, pStr
    test esi, esi
    jz @@exit
    xor ecx, ecx
@@len_loop:
    cmp byte ptr [esi + ecx], 0
    je @@len_done
    inc ecx
    cmp ecx, 2048
    jb @@len_loop
@@len_done:
    invoke WriteConsoleA, hStdOut, pStr, ecx, addr dwWritten, NULL
@@exit:
    pop esi
    ret
PrintString endp

PrintDec proc dwVal:DWORD
    LOCAL szNum[16]:BYTE
    push esi
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
    pop esi
    ret
PrintDec endp

; ============================================================================
; Test 1: GGUF Loader (gguf_loader_working)
; ============================================================================
Test_GGUFLoader proc
    LOCAL mdlHandle:DWORD

    invoke PrintString, addr szTest1

    ; Try to load model
    invoke GGUF_LoadModel, addr szTestModel
    test eax, eax
    jz @@fail

    mov mdlHandle, eax

    invoke PrintString, addr szModelLoaded

    ; Clean up
    invoke GGUF_CloseModel, mdlHandle

    invoke PrintString, addr szPass
    mov eax, TRUE
    jmp @@exit

@@fail:
    invoke PrintString, addr szFail
    xor eax, eax

@@exit:
    ret
Test_GGUFLoader endp

; ============================================================================
; Test 2: GGUF Reverse Reader
; ============================================================================
Test_ReverseReader proc
    LOCAL hdrBuffer[1024]:BYTE
    LOCAL mdtBuffer[32]:BYTE

    invoke PrintString, addr szTest2

    ; Try to read GGUF header in reverse
    invoke GGUF_ReverseReadHeader, addr szTestModel, addr hdrBuffer
    test eax, eax
    jz @@fail

    ; Get metadata
    invoke GGUF_ReverseGetMetadata, addr hdrBuffer, addr mdtBuffer
    test eax, eax
    jz @@fail

    invoke PrintString, addr szModelLoaded

    invoke PrintString, addr szPass
    mov eax, TRUE
    jmp @@exit

@@fail:
    invoke PrintString, addr szFail
    xor eax, eax

@@exit:
    ret
Test_ReverseReader endp

; ============================================================================
; Test 3: π-RAM Compression Integration
; ============================================================================
Test_PiRamCompression proc
    LOCAL mdlHandle2:DWORD

    invoke PrintString, addr szTest3

    ; Load model
    invoke GGUF_LoadModel, addr szTestModel
    test eax, eax
    jz @@fail

    mov mdlHandle2, eax

    ; Enable halving
    invoke PiRam_EnableHalving, TRUE

    ; Compress model
    invoke PiRam_CompressGGUF, mdlHandle2
    test eax, eax
    jz @@fail

    ; Get and print ratio
    invoke PrintString, addr szBytesCompress
    invoke PiRam_GetCompressionRatio
    invoke PrintDec, eax
    invoke PrintString, addr szPercent

    ; Cleanup
    invoke GGUF_CloseModel, mdlHandle2

    invoke PrintString, addr szPass
    mov eax, TRUE
    jmp @@exit

@@fail:
    invoke PrintString, addr szFail
    xor eax, eax

@@exit:
    ret
Test_PiRamCompression endp

; ============================================================================
; Test 4: RAM Halving & Streaming
; ============================================================================
Test_RamHalving proc

    invoke PrintString, addr szTest4

    ; Verify halving can be enabled/disabled
    invoke PiRam_EnableHalving, TRUE
    test eax, eax
    jz @@fail

    invoke PiRam_EnableHalving, FALSE
    test eax, eax
    jz @@fail

    invoke PrintString, addr szPass
    mov eax, TRUE
    jmp @@exit

@@fail:
    invoke PrintString, addr szFail
    xor eax, eax

@@exit:
    ret
Test_RamHalving endp

; ============================================================================
; Main Test Runner
; ============================================================================
start:
    ; Get stdout
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax

    ; Print banner
    invoke PrintString, addr szBanner

    ; Start timer
    invoke GetTickCount
    mov dwStartTime, eax

    ; Run Test 1: GGUF Loader
    mov dwTotal, 1
    call Test_GGUFLoader
    add dwPass, eax

    ; Run Test 2: Reverse Reader
    mov eax, dwTotal
    inc eax
    mov dwTotal, eax
    call Test_ReverseReader
    add dwPass, eax

    ; Run Test 3: π-RAM Compression
    mov eax, dwTotal
    inc eax
    mov dwTotal, eax
    call Test_PiRamCompression
    add dwPass, eax

    ; Run Test 4: RAM Halving
    mov eax, dwTotal
    inc eax
    mov dwTotal, eax
    call Test_RamHalving
    add dwPass, eax

    ; End timer
    invoke GetTickCount
    mov dwEndTime, eax

    ; Print summary
    invoke PrintString, addr szSeparator
    invoke PrintString, addr szTests
    invoke PrintDec, dwPass
    invoke PrintString, addr szOf
    invoke PrintDec, dwTotal

    ; Calculate elapsed time
    mov eax, dwEndTime
    sub eax, dwStartTime
    invoke PrintDec, eax
    invoke PrintString, addr szElapsed

    ; Exit with pass count
    mov eax, dwPass
    invoke ExitProcess, eax

end start
