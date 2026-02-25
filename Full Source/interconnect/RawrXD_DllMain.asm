; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_DllMain.asm  ─  DLL Entry Point & Initialization
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\ws2_32.inc

includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\ws2_32.lib

; Import Shared Data Pointers
EXTERNDEF RawrXD_DMA_RingBuffer_Base:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_Lock:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_Semaphore:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_ProducerOffset:QWORD
EXTERNDEF RawrXD_DMA_RingBuffer_ConsumerOffset:QWORD

; Helper Imports (Custom)
EXTERNDEF RawrXD_MemAlloc:PROC
EXTERNDEF RawrXD_MemFree:PROC

CONSTANTS:
RING_BUFFER_SIZE        EQU 67108864      ; 64MB

.DATA
    wsaData WSADATA <>

.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; DllMain
; Entry point for the DLL.
; ═══════════════════════════════════════════════════════════════════════════════
DllMain PROC FRAME hInstDLL:QWORD, fdwReason:DWORD, lpvReserved:QWORD
    
    .if fdwReason == DLL_PROCESS_ATTACH
        ; 1. Initialize Winsock
        sub rsp, 20h
        lea rcx, wsaData
        mov edx, 0202h ; Version 2.2
        call WSAStartup
        add rsp, 20h
        
        cmp eax, 0
        jne @fail
        
        ; 2. Allocate Ring Buffer (64MB)
        mov rcx, RING_BUFFER_SIZE
        mov edx, MEM_COMMIT or MEM_RESERVE
        mov r8, PAGE_READWRITE
        call VirtualAlloc
        mov RawrXD_DMA_RingBuffer_Base, rax
        
        test rax, rax
        jz @fail
        
        ; 3. Allocate Critical Section Struct
        mov rcx, SIZEOF CRITICAL_SECTION
        call RawrXD_MemAlloc
        mov RawrXD_DMA_RingBuffer_Lock, rax
        
        test rax, rax
        jz @fail
        
        ; Initialize Critical Section
        mov rcx, RawrXD_DMA_RingBuffer_Lock
        call InitializeCriticalSection
        
        ; 4. Create Semaphore
        xor ecx, ecx        ; lpSemaphoreAttributes = NULL
        xor edx, edx        ; lInitialCount = 0
        mov r8d, 7FFFFFFFh  ; lMaximumCount = Max Int
        xor r9, r9          ; lpName = NULL
        call CreateSemaphoreA
        mov RawrXD_DMA_RingBuffer_Semaphore, rax
        
        test rax, rax
        jz @fail
        
        ; Reset Offsets
        mov RawrXD_DMA_RingBuffer_ProducerOffset, 0
        mov RawrXD_DMA_RingBuffer_ConsumerOffset, 0
        
        mov rax, 1
        ret
        
    .elseif fdwReason == DLL_PROCESS_DETACH
        
        ; Cleanup Semaphore
        mov rcx, RawrXD_DMA_RingBuffer_Semaphore
        call CloseHandle
        
        ; Delete Critical Section
        mov rcx, RawrXD_DMA_RingBuffer_Lock
        cmp rcx, 0
        je @skip_cs
        call DeleteCriticalSection
        
        mov rcx, RawrXD_DMA_RingBuffer_Lock
        call RawrXD_MemFree
@skip_cs:
        
        ; Free Ring Buffer
        mov rcx, RawrXD_DMA_RingBuffer_Base
        cmp rcx, 0
        je @skip_buf
        xor edx, edx ; dwSize = 0 when MEM_RELEASE
        mov r8d, MEM_RELEASE
        call VirtualFree
@skip_buf:
        
        ; Cleanup Winsock
        call WSACleanup
        
        mov rax, 1
        ret
    .endif
    
    mov rax, 1
    ret

@fail:
    xor rax, rax
    ret
DllMain ENDP

END
