; ============================================================================
; RawrXD Agentic IDE - GGUF Streaming Test Harness (Pure MASM)
; Streams a dummy large file and prints progress to console
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; GGUF streaming prototypes
GGUFStream_Init proto
GGUFStream_Open proto :DWORD
GGUFStream_Read proto :DWORD, :DWORD
GGUFStream_GetProgress proto
GGUFStream_Close proto

.data
    public g_hMainWindow
    public g_hInstance
    g_hMainWindow dd 0
    g_hInstance  dd 0
    szStart    db "GGUF streaming test starting...",13,10,0
    szOpenOK   db "Opened dummy file for streaming.",13,10,0
    szOpenErr  db "Failed to open dummy file for streaming.",13,10,0
    szDone     db 13,10,"Streaming complete.",13,10,0
    szFmt      db "Progress: %d%%",13,10,0
    szFilePath db "C:\\Users\\HiH8e\\OneDrive\\Desktop\\RawrXD-production-lazy-init\\masm_ide\\build\\dummy-gguf.bin",0

.data?
    szBuf      db 128 dup(?)
    streamBuf  db 65536 dup(?)

.code
WinMain proc hInst:DWORD, hPrev:DWORD, lpCmd:DWORD, nCmd:DWORD
    ; Print start
    push 0
    push 0
    push sizeof szStart - 1
    push offset szStart
    push STD_OUTPUT_HANDLE
    call GetStdHandle
    push eax
    call WriteConsoleA

    ; Init streaming
    call GGUFStream_Init

    ; Open file
    push offset szFilePath
    call GGUFStream_Open
    test eax, eax
    jz @OpenFailed

    ; Print open OK
    push 0
    push 0
    push sizeof szOpenOK - 1
    push offset szOpenOK
    push STD_OUTPUT_HANDLE
    call GetStdHandle
    push eax
    call WriteConsoleA

@StreamLoop:
    ; Read chunk
    push 65536
    lea eax, streamBuf
    push eax
    call GGUFStream_Read

    ; Get progress
    call GGUFStream_GetProgress
    mov ecx, eax

    ; Print progress
    lea eax, szBuf
    push ecx
    push offset szFmt
    push eax
    call wsprintfA
    add esp, 12
    push 0
    push 0
    push -1
    push eax
    push STD_OUTPUT_HANDLE
    call GetStdHandle
    push eax
    call WriteConsoleA

    ; If progress < 100, continue
    cmp ecx, 100
    jl @Continue
    jmp @Done

@Continue:
    push 10
    call Sleep
    jmp @StreamLoop

@Done:
    call GGUFStream_Close
    ; Print done
    push 0
    push 0
    push sizeof szDone - 1
    push offset szDone
    push STD_OUTPUT_HANDLE
    call GetStdHandle
    push eax
    call WriteConsoleA
    mov eax, 0
    ret

@OpenFailed:
    push 0
    push 0
    push sizeof szOpenErr - 1
    push offset szOpenErr
    push STD_ERROR_HANDLE
    call GetStdHandle
    push eax
    call WriteConsoleA
    mov eax, 1
    ret
WinMain endp

end WinMain
