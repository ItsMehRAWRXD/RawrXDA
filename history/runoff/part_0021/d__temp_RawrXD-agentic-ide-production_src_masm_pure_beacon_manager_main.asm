;============================================================================
; BEACON_MANAGER_MAIN.ASM - Pure MASM x64 model lifecycle controller (FIXED)
; Single entry point, no C++ dependencies
; FIXES: struct size 40 bytes, event reset, lockCount decrement, handle cleanup
;============================================================================

option casemap:none

; Windows APIs (direct imports, no CRT)
extrn CreateEventA: proc
extrn SetEvent: proc
extrn ResetEvent: proc
extrn WaitForSingleObject: proc
extrn CreateThread: proc
extrn CloseHandle: proc
extrn Sleep: proc
extrn GetTickCount64: proc
extrn VirtualAlloc: proc
extrn VirtualFree: proc

; Public API exports
public Beacon_InitializeSystem
public Beacon_ShutdownSystem
public Beacon_CreateForModel
public Beacon_LoadModelAsync
public Beacon_UnloadModelAsync
public Beacon_Touch
public Beacon_UnlockModel
public Beacon_GetStatus
public Beacon_WaitLoadComplete
public Beacon_PollNonBlocking

; Beacon structure (40 bytes, cache-aligned)
Beacon STRUCT
    hEvent              qword ?     ; 0:  Event handle (8 bytes)
    modelId             dword ?     ; 8:  Model ID (4 bytes)
    state               dword ?     ; 12: State: 0=unload, 1=load, 2=loaded, 3=evict (4 bytes)
    lastAccessTime      qword ?     ; 16: Timestamp (8 bytes)
    modelPtr            qword ?     ; 24: Loaded model pointer (8 bytes)
    lockCount           dword ?     ; 32: Reference/lock count (4 bytes)
    padding             dword ?     ; 36: Alignment padding (4 bytes)
Beacon ENDS                         ; Total: 40 bytes

; Data section
.data
align 16
g_beaconArray           qword 0     ; Array of Beacon*
g_beaconCount           qword 0
g_hMasterLoadEvent      qword 0
g_hLoaderThread         qword 0
g_hIdleThread           qword 0
g_bShutdown             byte 0
align 16
g_contextLock           qword 0     ; Spinlock for concurrent access

; Constants
MAX_MODELS              equ 1000
BEACON_SIZE             equ 40
LOADING                 equ 1
LOADED                  equ 2
UNLOADED                equ 0
EVICTING                equ 3
WAIT_OBJECT_0           equ 0
WAIT_TIMEOUT            equ 258
MEM_COMMIT              equ 1000h
MEM_RESERVE             equ 2000h
MEM_RELEASE             equ 8000h
PAGE_READWRITE          equ 4

; Code section
.code

align 16
Beacon_InitializeSystem proc
    push rbp
    mov rbp, rsp
    
    mov ecx, MAX_MODELS * BEACON_SIZE
    mov edx, MEM_COMMIT or MEM_RESERVE
    mov r8d, PAGE_READWRITE
    xor r9d, r9d
    call VirtualAlloc
    test rax, rax
    jz @init_failed
    
    mov g_beaconArray, rax
    mov g_beaconCount, 0
    
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call CreateEventA
    mov g_hMasterLoadEvent, rax
    test rax, rax
    jz @init_failed
    
    xor ecx, ecx
    xor edx, edx
    lea r8, [LoaderThreadMain]
    xor r9d, r9d
    sub rsp, 32
    call CreateThread
    add rsp, 32
    test rax, rax
    jz @init_failed
    mov g_hLoaderThread, rax
    
    xor ecx, ecx
    xor edx, edx
    lea r8, [IdleDetectorThreadMain]
    xor r9d, r9d
    sub rsp, 32
    call CreateThread
    add rsp, 32
    test rax, rax
    jz @init_failed
    mov g_hIdleThread, rax
    
    xor eax, eax
    pop rbp
    ret
    
@init_failed:
    mov eax, -1
    pop rbp
    ret
Beacon_InitializeSystem endp

align 8
Beacon_CreateForModel proc
    push rbp
    mov rbp, rsp
    push rbx
    
    mov rbx, rcx
    
    mov rax, g_beaconCount
    cmp rax, MAX_MODELS
    jge @create_failed
    
    mov rax, g_beaconCount
    imul rax, BEACON_SIZE
    mov r8, g_beaconArray
    add r8, rax
    
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    sub rsp, 32
    call CreateEventA
    add rsp, 32
    test rax, rax
    jz @create_failed
    
    mov rdx, g_beaconCount
    mov r8, g_beaconArray
    imul rdx, BEACON_SIZE
    add r8, rdx
    
    mov [r8+Beacon.hEvent], rax
    mov [r8+Beacon.modelId], ebx
    mov dword ptr [r8+Beacon.state], UNLOADED
    mov qword ptr [r8+Beacon.lastAccessTime], 0
    mov qword ptr [r8+Beacon.modelPtr], 0
    mov dword ptr [r8+Beacon.lockCount], 0
    
    inc g_beaconCount
    
    mov rax, r8
    
    pop rbx
    pop rbp
    ret
    
@create_failed:
    xor rax, rax
    pop rbx
    pop rbp
    ret
Beacon_CreateForModel endp

align 8
Beacon_LoadModelAsync proc
    cmp dword ptr [rcx+Beacon.state], UNLOADED
    jne @already_loading
    
    mov dword ptr [rcx+Beacon.state], LOADING
    
    push rcx
    mov rcx, [rcx+Beacon.hEvent]
    sub rsp, 32
    call ResetEvent
    add rsp, 32
    pop rcx
    
    push rcx
    mov rcx, g_hMasterLoadEvent
    sub rsp, 32
    call SetEvent
    add rsp, 32
    pop rcx
    
@already_loading:
    ret
Beacon_LoadModelAsync endp

align 8
Beacon_Touch proc
    push rcx
    sub rsp, 32
    call GetTickCount64
    add rsp, 32
    pop rcx
    mov [rcx+Beacon.lastAccessTime], rax
    
    inc dword ptr [rcx+Beacon.lockCount]
    
    ret
Beacon_Touch endp

align 8
Beacon_UnlockModel proc
    cmp dword ptr [rcx+Beacon.lockCount], 0
    jle @already_zero
    
    dec dword ptr [rcx+Beacon.lockCount]
    
@already_zero:
    ret
Beacon_UnlockModel endp

align 8
Beacon_GetStatus proc
    mov eax, [rcx+Beacon.state]
    ret
Beacon_GetStatus endp

align 8
Beacon_WaitLoadComplete proc
    push rbp
    mov rbp, rsp
    
    mov r8, rcx
    mov rcx, [rcx+Beacon.hEvent]
    sub rsp, 32
    call WaitForSingleObject
    add rsp, 32
    
    cmp eax, WAIT_OBJECT_0
    jne @timeout
    
    xor eax, eax
    pop rbp
    ret
    
@timeout:
    mov eax, -1
    pop rbp
    ret
Beacon_WaitLoadComplete endp

align 8
Beacon_PollNonBlocking proc
    push rcx
    mov rcx, [rcx+Beacon.hEvent]
    xor edx, edx
    sub rsp, 32
    call WaitForSingleObject
    add rsp, 32
    pop rcx
    
    cmp eax, WAIT_TIMEOUT
    je @still_loading
    
    mov eax, [rcx+Beacon.state]
    ret
    
@still_loading:
    mov eax, LOADING
    ret
Beacon_PollNonBlocking endp

align 8
Beacon_UnloadModelAsync proc
    mov dword ptr [rcx+Beacon.state], EVICTING
    
    push rcx
    mov rcx, [rcx+Beacon.hEvent]
    sub rsp, 32
    call ResetEvent
    add rsp, 32
    pop rcx
    
    push rcx
    mov rcx, g_hMasterLoadEvent
    sub rsp, 32
    call SetEvent
    add rsp, 32
    pop rcx
    
    ret
Beacon_UnloadModelAsync endp

align 8
Beacon_ShutdownSystem proc
    push rbp
    mov rbp, rsp
    push rsi
    sub rsp, 32
    
    mov g_bShutdown, 1
    
    mov rcx, g_hMasterLoadEvent
    call SetEvent
    
    mov rcx, g_hLoaderThread
    mov edx, 5000
    call WaitForSingleObject
    
    mov rcx, g_hIdleThread
    mov edx, 5000
    call WaitForSingleObject
    
    mov rsi, g_beaconArray
    mov rcx, g_beaconCount
    
@close_events:
    test rcx, rcx
    jz @events_closed
    
    mov rdx, [rsi+Beacon.hEvent]
    test rdx, rdx
    jz @skip_event_close
    
    push rcx
    push rsi
    mov rcx, rdx
    call CloseHandle
    pop rsi
    pop rcx
    
@skip_event_close:
    add rsi, BEACON_SIZE
    dec rcx
    jmp @close_events
    
@events_closed:
    mov rcx, g_hMasterLoadEvent
    test rcx, rcx
    jz @master_closed
    call CloseHandle
    
@master_closed:
    mov rcx, g_hLoaderThread
    test rcx, rcx
    jz @loader_closed
    call CloseHandle
    
@loader_closed:
    mov rcx, g_hIdleThread
    test rcx, rcx
    jz @idle_closed
    call CloseHandle
    
@idle_closed:
    mov rcx, g_beaconArray
    test rcx, rcx
    jz @done
    
    xor edx, edx
    mov r8d, MEM_RELEASE
    call VirtualFree
    
@done:
    add rsp, 32
    pop rsi
    pop rbp
    ret
Beacon_ShutdownSystem endp

align 16
LoaderThreadMain proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
@loader_loop:
    mov rcx, g_hMasterLoadEvent
    mov edx, -1
    call WaitForSingleObject
    
    cmp g_bShutdown, 1
    je @thread_exit
    
    mov rsi, g_beaconArray
    mov rcx, g_beaconCount
    
@scan_loop:
    test rcx, rcx
    jz @loader_loop
    
    mov eax, [rsi+Beacon.state]
    cmp eax, LOADING
    je @load_model
    
    cmp eax, EVICTING
    je @evict_model
    
    add rsi, BEACON_SIZE
    dec rcx
    jmp @scan_loop
    
@load_model:
    push rsi
    push rcx
    
    mov ecx, 10
    call Sleep
    
    pop rcx
    pop rsi
    
    mov rax, 12345678h
    mov [rsi+Beacon.modelPtr], rax
    mov dword ptr [rsi+Beacon.state], LOADED
    
    push rsi
    push rcx
    mov rcx, [rsi+Beacon.hEvent]
    call SetEvent
    pop rcx
    pop rsi
    
    add rsi, BEACON_SIZE
    dec rcx
    jmp @scan_loop
    
@evict_model:
    mov qword ptr [rsi+Beacon.modelPtr], 0
    mov dword ptr [rsi+Beacon.state], UNLOADED
    mov qword ptr [rsi+Beacon.lastAccessTime], 0
    mov dword ptr [rsi+Beacon.lockCount], 0
    
    add rsi, BEACON_SIZE
    dec rcx
    jmp @scan_loop
    
@thread_exit:
    add rsp, 32
    pop rbp
    xor eax, eax
    ret
LoaderThreadMain endp

align 16
IdleDetectorThreadMain proc
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
@idle_loop:
    mov ecx, 5000
    call Sleep
    
    cmp g_bShutdown, 1
    je @idle_exit
    
    mov rsi, g_beaconArray
    mov rcx, g_beaconCount
    
@check_idle:
    test rcx, rcx
    jz @idle_loop
    
    cmp dword ptr [rsi+Beacon.state], LOADED
    jne @next_model
    
    push rsi
    push rcx
    call GetTickCount64
    pop rcx
    pop rsi
    
    sub rax, [rsi+Beacon.lastAccessTime]
    
    cmp rax, 30000
    jl @next_model
    
    cmp dword ptr [rsi+Beacon.lockCount], 0
    jne @next_model
    
    mov dword ptr [rsi+Beacon.state], EVICTING
    push rsi
    push rcx
    mov rcx, g_hMasterLoadEvent
    call SetEvent
    pop rcx
    pop rsi
    
@next_model:
    add rsi, BEACON_SIZE
    dec rcx
    jmp @check_idle
    
@idle_exit:
    add rsp, 32
    pop rbp
    xor eax, eax
    ret
IdleDetectorThreadMain endp

end
