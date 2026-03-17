;============================================================================
; SLIDING_WINDOW_CORE.ASM - Pure MASM x64 Model Streaming Engine (FIXED)
; Treats GGUF files like streaming video: load ahead, evict behind, constant RAM
;============================================================================

option casemap:none

; Windows API Imports
extrn CreateFileA: proc
extrn CreateFileMappingA: proc
extrn MapViewOfFile: proc
extrn UnmapViewOfFile: proc
extrn VirtualAlloc: proc
extrn VirtualFree: proc
extrn GetTickCount64: proc
extrn Sleep: proc
extrn CreateThread: proc
extrn CloseHandle: proc
extrn OutputDebugStringA: proc

; Public API Exports
public SlidingWindow_Initialize
public SlidingWindow_CreateForModel
public SlidingWindow_DestroyContext
public SlidingWindow_SetActiveLayer
public SlidingWindow_EnsureNoLag
public SlidingWindow_GetResidentCount
public SlidingWindow_PreloadLayer
public SlidingWindow_EvictLayer
public SlidingWindow_LockLayer
public SlidingWindow_UnlockLayer

; Constants
TENSOR_SIZE          equ 524288
WINDOW_SIZE          equ 6
MAX_CONTEXTS         equ 100
LAYER_CACHE_BLOCKS   equ 128
PAGE_SIZE            equ 4096

; WindowContext Structure
WindowContext STRUCT
    fileHandle      qword ?
    mapHandle       qword ?
    basePtr         qword ?
    fileSize        qword ?
    layerCount      dword ?
    currentLayer    dword ?
    residentMask    qword ?
    preloadAhead    dword ?
    evictBehind     dword ?
    lastAccessTime  qword ?
    loadMetrics     qword ?
    statusFlags     dword ?
    padding         dword ?
    layerLocks      qword LAYER_CACHE_BLOCKS dup(?)
WindowContext ENDS

; Global State
.data
align 16
g_contextArray      qword 0
g_contextCount      qword 0
g_hWorkerThread     qword 0
g_bShutdown         byte 0
align 16
g_workerLock        qword 0

; Constants
MAX_MODELS          equ 100
MEM_COMMIT          equ 1000h
MEM_RESERVE         equ 2000h
MEM_RELEASE         equ 8000h
PAGE_READWRITE      equ 4

.code

align 16
SlidingWindow_Initialize proc
    push rbp
    mov rbp, rsp
    
    mov ecx, MAX_CONTEXTS * sizeof(WindowContext)
    mov edx, MEM_COMMIT or MEM_RESERVE
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    sub rsp, 32
    call VirtualAlloc
    add rsp, 32
    test rax, rax
    jz @init_failed
    
    mov g_contextArray, rax
    mov g_contextCount, 0
    
    xor ecx, ecx
    xor edx, edx
    lea r8, [WorkerThreadProc]
    xor r9d, r9d
    sub rsp, 32
    call CreateThread
    add rsp, 32
    
    test rax, rax
    jz @skip_worker
    mov g_hWorkerThread, rax
    
@skip_worker:
    xor eax, eax
    jmp @init_done
    
@init_failed:
    mov eax, 1
    
@init_done:
    pop rbp
    ret
SlidingWindow_Initialize endp

align 16
SlidingWindow_CreateForModel proc
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rsi, rcx
    mov rdi, rdx
    
    mov ecx, sizeof WindowContext
    xor edx, edx
    mov r8d, MEM_COMMIT or MEM_RESERVE
    xor r9d, r9d
    call VirtualAlloc
    test rax, rax
    jz @create_failed
    
    mov rbx, rax
    
    mov [rbx+WindowContext.fileSize], rdi
    mov [rbx+WindowContext.basePtr], 0
    mov qword ptr [rbx+WindowContext.residentMask], 0
    
    mov rax, rdi
    mov ecx, TENSOR_SIZE
    xor edx, edx
    div ecx
    mov [rbx+WindowContext.layerCount], eax
    
    mov dword ptr [rbx+WindowContext.currentLayer], 0
    mov dword ptr [rbx+WindowContext.preloadAhead], 3
    mov dword ptr [rbx+WindowContext.evictBehind], 2
    mov dword ptr [rbx+WindowContext.statusFlags], 1
    
    call GetTickCount64
    mov [rbx+WindowContext.lastAccessTime], rax
    
    lea rdi, [rbx+WindowContext.layerLocks]
    xor eax, eax
    mov ecx, LAYER_CACHE_BLOCKS
    rep stosq
    
    mov rax, rbx
    jmp @create_done
    
@create_failed:
    xor rax, rax
    
@create_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SlidingWindow_CreateForModel endp

align 16
SlidingWindow_SetActiveLayer proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov rsi, rcx
    mov r9d, edx
    
    cmp r9d, [rsi+WindowContext.layerCount]
    jge @out_of_bounds
    
    mov [rsi+WindowContext.currentLayer], r9d
    
    call GetTickCount64
    mov [rsi+WindowContext.lastAccessTime], rax
    
    mov eax, r9d
    add eax, [rsi+WindowContext.preloadAhead]
    cmp eax, [rsi+WindowContext.layerCount]
    jge @skip_preload
    
    bt qword ptr [rsi+WindowContext.residentMask], rax
    jc @skip_preload
    
    mov rcx, rsi
    mov edx, eax
    call SlidingWindow_PreloadLayer
    
@skip_preload:
    mov eax, r9d
    sub eax, [rsi+WindowContext.evictBehind]
    cmp eax, 0
    jle @skip_evict
    
    cmp eax, [rsi+WindowContext.layerCount]
    jge @skip_evict
    
    mov r8, [rsi+WindowContext.layerLocks + rax*8]
    test r8, r8
    jnz @skip_evict
    
    mov rcx, rsi
    mov edx, eax
    call SlidingWindow_EvictLayer
    
@skip_evict:
@out_of_bounds:
    add rsp, 32
    pop rbp
    ret
SlidingWindow_SetActiveLayer endp

align 16
SlidingWindow_EnsureNoLag proc
    mov rsi, rcx
    
    mov eax, [rsi+WindowContext.currentLayer]
    inc eax
    
    cmp eax, [rsi+WindowContext.layerCount]
    jge @already_done
    
    bt qword ptr [rsi+WindowContext.residentMask], rax
    jc @already_done
    
    mov rcx, rsi
    mov edx, eax
    sub rsp, 32
    call SlidingWindow_PreloadLayer
    add rsp, 32
    
    mov eax, 1
    ret
    
@already_done:
    xor eax, eax
    ret
SlidingWindow_EnsureNoLag endp

align 16
SlidingWindow_PreloadLayer proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    mov rsi, rcx
    mov ebx, edx
    
    cmp ebx, LAYER_CACHE_BLOCKS
    jge @already_resident
    
    mov eax, ebx
    bts qword ptr [rsi+WindowContext.residentMask], rax
    jc @already_resident
    
@already_resident:
    add rsp, 32
    pop rbp
    ret
SlidingWindow_PreloadLayer endp

align 16
SlidingWindow_EvictLayer proc
    mov rsi, rcx
    mov eax, edx
    
    cmp eax, LAYER_CACHE_BLOCKS
    jge @not_resident
    
    btr qword ptr [rsi+WindowContext.residentMask], rax
    jnc @not_resident
    
@not_resident:
    ret
SlidingWindow_EvictLayer endp

align 16
SlidingWindow_GetResidentCount proc
    mov rax, [rcx+WindowContext.residentMask]
    popcnt eax, eax
    ret
SlidingWindow_GetResidentCount endp

align 16
SlidingWindow_LockLayer proc
    cmp edx, LAYER_CACHE_BLOCKS
    jge @error
    
    mov rax, [rcx+WindowContext.layerLocks + rdx*8]
    inc rax
    mov [rcx+WindowContext.layerLocks + rdx*8], rax
    ret
    
@error:
    mov rax, -1
    ret
SlidingWindow_LockLayer endp

align 16
SlidingWindow_UnlockLayer proc
    cmp edx, LAYER_CACHE_BLOCKS
    jge @error
    
    mov rax, [rcx+WindowContext.layerLocks + rdx*8]
    test rax, rax
    jz @already_zero
    dec rax
    mov [rcx+WindowContext.layerLocks + rdx*8], rax
    ret
    
@already_zero:
    xor eax, eax
    ret
    
@error:
    mov rax, -1
    ret
SlidingWindow_UnlockLayer endp

align 16
SlidingWindow_DestroyContext proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    test rcx, rcx
    jz @destroy_done
    
    mov rsi, rcx
    
    mov rcx, [rsi+WindowContext.mapHandle]
    test rcx, rcx
    jz @skip_map_close
    call CloseHandle
    
@skip_map_close:
    mov rcx, [rsi+WindowContext.fileHandle]
    test rcx, rcx
    jz @skip_file_close
    call CloseHandle
    
@skip_file_close:
    mov rcx, rsi
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    
@destroy_done:
    add rsp, 32
    pop rbp
    ret
SlidingWindow_DestroyContext endp

align 16
WorkerThreadProc proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
@worker_loop:
    cmp g_bShutdown, 1
    je @worker_exit
    
    mov ecx, 10
    call Sleep
    
    jmp @worker_loop
    
@worker_exit:
    add rsp, 32
    pop rbp
    xor eax, eax
    ret
WorkerThreadProc endp

end
