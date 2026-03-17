; ============================================================================
; GGUF_CHAIN_TEST.ASM - Chain API Cycling Test & Ollama Integration
; Tests all loader methods and integrates with Ollama for inference
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; Win32 API
GetStdHandle PROTO :DWORD
WriteFile PROTO :DWORD,:DWORD,:DWORD,:DWORD,:DWORD
GetEnvironmentVariableA PROTO :DWORD,:DWORD,:DWORD
lstrlenA PROTO :DWORD
lstrcpyA PROTO :DWORD,:DWORD
lstrcatA PROTO :DWORD,:DWORD
ExitProcess PROTO :DWORD

includelib kernel32.lib

; Chain API
GGUFChain_Init PROTO
GGUFChain_LoadModel PROTO :DWORD,:DWORD
GGUFChain_StreamChunk PROTO :DWORD,:DWORD,:DWORD
GGUFChain_CloseModel PROTO :DWORD
GGUFChain_CycleMethod PROTO :DWORD
GGUFChain_GetPerformance PROTO :DWORD,:DWORD
GGUFChain_StackMethods PROTO :DWORD,:DWORD

; ============================================================================
; CONSTANTS
; ============================================================================
NULL                equ 0
STD_OUTPUT_HANDLE   equ -11

METHOD_DISC         equ 0
METHOD_MEMORY       equ 1
METHOD_MMAP         equ 2
METHOD_HYBRID       equ 3
METHOD_AUTO         equ 4

; ============================================================================
; DATA
; ============================================================================
.data
    ; Model path
    szOllamaPath    db "C:\Users\HiH8e\.ollama\models\manifests\registry.ollama.ai\library\",0
    szModelName     db "bigdaddyg",0
    szModelFile     db "\latest",0
    szFullPath      db 512 dup(0)
    
    ; Alternative: Direct GGUF path (BigDaddyG blob)
    szGGUFPath      db "D:\OllamaModels\blobs\sha256-dde5aa3fc5ffc17176b5e8bdc82f587b24b2678c6c66101bf7da77af9f7ccdff",0
    
    ; Test prompt
    szPrompt        db "whats up:",0
    
    ; Output messages
    szBanner        db 13,10,"===== GGUF Chain API Cycling Test =====",13,10,0
    szInitMsg       db "Initializing Chain API...",13,10,0
    szTestMethod    db "Testing METHOD_",0
    szMethodDisc    db "DISC",0
    szMethodMem     db "MEMORY",0
    szMethodMmap    db "MMAP",0
    szMethodHybrid  db "HYBRID",0
    szMethodAuto    db "AUTO",0
    szMethodUnknown db "UNKNOWN",0
    szLoading       db "Loading model: ",0
    szLoadSuccess   db "Success! Handle: ",0
    szLoadFail      db "Failed to load model",13,10,0
    szCycling       db "Cycling to next method...",13,10,0
    szPerf          db "Performance - Method: ",0
    szPerfTime      db ", Time: ",0
    szPerfMs        db "ms",13,10,0
    szStreaming     db "Streaming chunk...",13,10,0
    szStreamBytes   db "Streamed bytes: ",0
    szClosing       db "Closing model...",13,10,0
    szPromptMsg     db "Sending prompt to Ollama: ",0
    szResponse      db "Response: ",0
    szComplete      db 13,10,"===== Test Complete =====",13,10,0
    szNewLine       db 13,10,0
    
.data?
    hStdOut         dd ?
    dwWritten       dd ?
    hChain          dd ?
    perfData        dd 4 dup(?)
    streamBuf       db 4096 dup(?)

; ============================================================================
; CODE
; ============================================================================
.code

; ============================================================================
; PrintString - Helper to print string
; ============================================================================
PrintString PROC pStr:DWORD
    push esi
    
    invoke lstrlenA, pStr
    invoke WriteFile, hStdOut, pStr, eax, addr dwWritten, NULL
    
    pop esi
    ret
PrintString ENDP

; ============================================================================
; PrintMethod - Print method name
; ============================================================================
PrintMethod PROC dwMethod:DWORD
    mov eax, dwMethod
    
    cmp eax, METHOD_DISC
    jne @check_mem
    invoke PrintString, addr szMethodDisc
    ret
    
@check_mem:
    cmp eax, METHOD_MEMORY
    jne @check_mmap
    invoke PrintString, addr szMethodMem
    ret
    
@check_mmap:
    cmp eax, METHOD_MMAP
    jne @check_hybrid
    invoke PrintString, addr szMethodMmap
    ret
    
@check_hybrid:
    cmp eax, METHOD_HYBRID
    jne @check_auto
    invoke PrintString, addr szMethodHybrid
    ret
    
@check_auto:
    cmp eax, METHOD_AUTO
    jne @unknown
    invoke PrintString, addr szMethodAuto
    ret
    
@unknown:
    invoke PrintString, addr szMethodUnknown
    ret
PrintMethod ENDP

; ============================================================================
; PrintDword - Print DWORD as decimal (simple)
; ============================================================================
PrintDword PROC dwValue:DWORD
    LOCAL buffer[12]:BYTE
    LOCAL idx:DWORD
    
    push esi
    push edi
    push ebx
    
    ; Convert to string (reverse)
    lea edi, buffer
    add edi, 10
    mov byte ptr [edi], 0
    
    mov eax, dwValue
    test eax, eax
    jnz @convert_loop
    
    ; Zero
    dec edi
    mov byte ptr [edi], '0'
    jmp @print_it

@convert_loop:
    test eax, eax
    jz @print_it
    
    mov ecx, 10
    xor edx, edx
    div ecx
    add dl, '0'
    dec edi
    mov byte ptr [edi], dl
    jmp @convert_loop

@print_it:
    invoke PrintString, edi
    
    pop ebx
    pop edi
    pop esi
    
    ret
PrintDword ENDP

; ============================================================================
; TestMethod - Test a specific loading method
; ============================================================================
TestMethod PROC dwMethod:DWORD
    invoke PrintString, addr szTestMethod
    invoke PrintMethod, dwMethod
    invoke PrintString, addr szNewLine
    
    ; Load model
    invoke PrintString, addr szLoading
    invoke PrintString, addr szGGUFPath
    invoke PrintString, addr szNewLine
    
    invoke GGUFChain_LoadModel, addr szGGUFPath, dwMethod
    mov hChain, eax
    test eax, eax
    jz @test_fail
    
    invoke PrintString, addr szLoadSuccess
    invoke PrintDword, eax
    invoke PrintString, addr szNewLine
    
    ; Get performance
    lea eax, perfData
    invoke GGUFChain_GetPerformance, hChain, eax
    test eax, eax
    jz @skip_perf
    
    invoke PrintString, addr szPerf
    invoke PrintMethod, perfData
    invoke PrintString, addr szPerfTime
    invoke PrintDword, perfData+4
    invoke PrintString, addr szPerfMs
    
@skip_perf:
    ; Stream a chunk
    invoke PrintString, addr szStreaming
    lea eax, streamBuf
    invoke GGUFChain_StreamChunk, hChain, eax, 4096
    
    invoke PrintString, addr szStreamBytes
    invoke PrintDword, eax
    invoke PrintString, addr szNewLine
    
    ; Close
    invoke PrintString, addr szClosing
    invoke GGUFChain_CloseModel, hChain
    
    mov eax, 1
    ret

@test_fail:
    invoke PrintString, addr szLoadFail
    xor eax, eax
    ret
TestMethod ENDP

; ============================================================================
; start - Main entry point
; ============================================================================
start:
    ; Get stdout
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    
    ; Print banner
    invoke PrintString, addr szBanner
    
    ; Initialize Chain API
    invoke PrintString, addr szInitMsg
    invoke GGUFChain_Init
    
    ; Test AUTO method (lets it choose)
    invoke TestMethod, METHOD_AUTO
    
    ; Cycle through methods
    invoke PrintString, addr szNewLine
    invoke PrintString, addr szCycling
    
    ; Test DISC method
    invoke TestMethod, METHOD_DISC
    
    invoke PrintString, addr szNewLine
    invoke PrintString, addr szCycling
    
    ; Test MEMORY method (will fail for large files, but tests the path)
    invoke TestMethod, METHOD_MEMORY
    
    invoke PrintString, addr szNewLine
    invoke PrintString, addr szCycling
    
    ; Test MMAP method
    invoke TestMethod, METHOD_MMAP
    
    invoke PrintString, addr szNewLine
    invoke PrintString, addr szCycling
    
    ; Test HYBRID method
    invoke TestMethod, METHOD_HYBRID
    
    ; Stack methods test
    invoke PrintString, addr szNewLine
    invoke PrintString, addr szNewLine
    db "===== Testing Method Stacking =====",13,10,0
    
    ; Load with AUTO, then stack all methods
    invoke GGUFChain_LoadModel, addr szGGUFPath, METHOD_AUTO
    mov hChain, eax
    test eax, eax
    jz @skip_stack
    
    ; Stack all methods (bitmask: 0x1F = all 5 methods)
    invoke GGUFChain_StackMethods, hChain, 1Fh
    
    invoke PrintString, addr szNewLine
    db "Stacked all methods for parallel execution",13,10,0
    
    invoke GGUFChain_CloseModel, hChain
    
@skip_stack:
    ; Complete
    invoke PrintString, addr szComplete
    
    ; Exit
    invoke ExitProcess, 0

end start
