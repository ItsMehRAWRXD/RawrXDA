; RawrXD_Enh5_ContinuousBatching.asm
OPTION CASEMAP:NONE

.DATA
g_total_pages QWORD 0
g_used_pages QWORD 0
g_max_requests QWORD 0
g_active_requests QWORD 0

.CODE
ContinuousBatching_Initialize PROC
    mov rax, rcx
    shr rax, 12
    mov g_total_pages, rax
    mov g_max_requests, rdx
    xor eax, eax
    ret
ContinuousBatching_Initialize ENDP

ContinuousBatching_ScheduleRequest PROC
    mov rax, g_active_requests
    cmp rax, g_max_requests
    jae full
    inc g_active_requests
    xor eax, eax
    ret
full:
    mov rax, -1
    ret
ContinuousBatching_ScheduleRequest ENDP

ContinuousBatching_Step PROC
    mov rax, g_active_requests
    ret
ContinuousBatching_Step ENDP

PagedKV_AllocatePages PROC
    mov eax, ecx
    add g_used_pages, rax
    mov eax, 1
    ret
PagedKV_AllocatePages ENDP
END
