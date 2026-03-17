; Phase 150A: Core Affinity Lock
; Feature Request: SwarmV150_Core_Affinity_Lock
; Maps the active thread to a high-performance physical core, bypassing SMT limits.

.data
    extern SetThreadAffinityMask: proc
    extern GetCurrentThread: proc

.code
SwarmV150_Core_Affinity_Lock proc
    ; RCX = Core index to pin to (0-15 on 7800X3D)
    
    ; 1. Calculate actual thread mask (1 << CoreIndex)
    mov rax, 1
    mov cl, cl
    shl rax, cl
    mov rdx, rax ; arg 2 for SetThreadAffinityMask

    ; 2. Fetch current process/thread handle
    sub rsp, 32
    call GetCurrentThread
    mov rcx, rax ; arg 1

    ; 3. Execute OS mask lock
    call SetThreadAffinityMask
    
    add rsp, 32
    ret
SwarmV150_Core_Affinity_Lock endp

end
