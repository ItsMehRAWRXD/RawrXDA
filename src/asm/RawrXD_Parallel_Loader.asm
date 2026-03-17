;-------------------------------------------------------------------------------
; RawrXD_Parallel_Loader.asm
; MASM64 - Asynchronous Model Weight Streaming Kernel
; Phase 15: Scaling and Full Model Loading (800B Infrastructure)
;-------------------------------------------------------------------------------

option casemap:none

; External Win32 and Swarm prototypes
extern CreateFileA      : proc
extern ReadFile         : proc
extern CloseHandle      : proc
extern GetFileSizeEx    : proc
extern VirtualAlloc     : proc
extern VirtualFree      : proc
extern RawrXD_Tensor_SliceAndDistribute : proc

.data
    GENERIC_READ        equ 80000000h
    FILE_SHARE_READ     equ 1
    OPEN_EXISTING       equ 3
    MEM_COMMIT          equ 1000h
    MEM_RESERVE         equ 2000h
    PAGE_READWRITE      equ 04h
    
    CHUNK_SIZE          equ 1048576 * 64 ; 64MB Streaming Chunk
    
.code

;-------------------------------------------------------------------------------
; RawrXD_LoadAndStreamModel
; RCX = Ptr to Model Path String
; RDX = Number of Swarm Nodes
; R8  = Socket Array Ptr
; Returns: RAX = 1 (Success), 0 (IO Error), -1 (Memory Error)
;-------------------------------------------------------------------------------
RawrXD_LoadAndStreamModel proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 64             ; Shadow space + locals

    mov r12, rcx            ; r12 = Path
    mov r13, rdx            ; r13 = Node Count
    mov r14, r8             ; r14 = Sockets

    ; 1. Open Model File (800B Scale)
    xor r9, r9              ; Template
    mov qword ptr [rsp + 40], 0 ; Attr
    mov dword ptr [rsp + 32], OPEN_EXISTING
    mov r8, FILE_SHARE_READ
    mov rdx, GENERIC_READ
    mov rcx, r12
    call CreateFileA
    
    cmp rax, -1
    je @io_error
    mov rbx, rax            ; rbx = File Handle

    ; 2. Allocate Streaming Buffer (Chunked)
    xor r9, r9              ; Address
    mov r8, PAGE_READWRITE
    mov rdx, MEM_COMMIT or MEM_RESERVE
    mov rcx, CHUNK_SIZE
    call VirtualAlloc
    
    test rax, rax
    jz @mem_error
    mov r15, rax            ; r15 = Chunk Buffer

@stream_loop:
    ; 3. Read Chunk from Disk
    mov qword ptr [rsp + 32], 0 ; Overlapped
    lea r9, [rsp + 48]      ; BytesRead (Local)
    mov r8, CHUNK_SIZE
    mov rdx, r15
    mov rcx, rbx
    call ReadFile
    
    test eax, eax
    jz @stream_finished
    
    mov rax, [rsp + 48]     ; Get actual bytes read
    test rax, rax
    jz @stream_finished

    ; 4. Pipe Chunk to Swarm Distributor
    mov rcx, r15            ; Buffer
    mov rdx, rax            ; Actual Chunk Size
    mov r8, r13             ; Node Count
    mov r9, r14             ; Socket Array
    call RawrXD_Tensor_SliceAndDistribute

    jmp @stream_loop

@stream_finished:
    mov rcx, rbx
    call CloseHandle
    
    mov rax, 1
    jmp @exit

@io_error:
    xor rax, rax
    jmp @exit

@mem_error:
    mov rax, -1

@exit:
    add rsp, 64
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
RawrXD_LoadAndStreamModel endp

end
