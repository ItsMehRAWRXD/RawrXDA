; src/direct_io/adaptive_burst_router.asm
; RAWRXD v1.2.0 — ADAPTIVE BURST ROUTER (Stub for linking)


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.code

Router_Init PROC
    ; Initialize adaptive burst router: allocate route table, set defaults
    push rbx
    push rsi
    sub rsp, 32
    
    ; Allocate route table: 256 entries * 32 bytes = 8KB
    xor ecx, ecx
    mov edx, 8192
    mov r8d, 3000h                   ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 04h                     ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @@ri_fail
    
    mov QWORD PTR [g_route_table], rax
    mov DWORD PTR [g_route_count], 0
    mov DWORD PTR [g_max_routes], 256
    
    ; Initialize default route (index 0): round-robin, weight 1
    mov DWORD PTR [rax], 0           ; route_id = 0
    mov DWORD PTR [rax+4], 1         ; strategy = ROUND_ROBIN
    mov DWORD PTR [rax+8], 1         ; weight = 1
    mov DWORD PTR [rax+12], 0        ; hits = 0
    mov QWORD PTR [rax+16], 0        ; target_addr = NULL
    mov QWORD PTR [rax+24], 0        ; reserved
    mov DWORD PTR [g_route_count], 1
    
    ; Get timestamp for stats baseline
    call GetTickCount64
    mov QWORD PTR [g_router_start_tick], rax
    
    mov rax, 1                       ; success
    add rsp, 32
    pop rsi
    pop rbx
    ret
@@ri_fail:
    xor rax, rax
    add rsp, 32
    pop rsi
    pop rbx
    ret
Router_Init ENDP

Router_GetRoute PROC
    ; Select best route for a given destination hash
    ; ECX = destination hash
    ; Returns EAX = route index
    push rbx
    
    mov ebx, ecx                     ; dest hash
    mov rax, QWORD PTR [g_route_table]
    test rax, rax
    jz @@rgr_default
    
    mov edx, DWORD PTR [g_route_count]
    test edx, edx
    jz @@rgr_default
    
    ; Consistent hashing: route = hash % route_count
    mov eax, ebx
    xor edx, edx
    div DWORD PTR [g_route_count]
    ; edx = route index
    
    ; Increment hit counter for selected route
    mov rax, QWORD PTR [g_route_table]
    imul ecx, edx, 32               ; * entry size
    inc DWORD PTR [rax+rcx+12]       ; hits++
    
    mov eax, edx
    pop rbx
    ret
@@rgr_default:
    xor eax, eax                     ; route 0
    pop rbx
    ret
Router_GetRoute ENDP

Router_GetStats PROC
    ; Fill caller's buffer with router statistics
    ; RCX = output buffer (320 bytes)
    ; Returns EAX = number of active routes
    push rdi
    push rsi
    push rbx
    sub rsp, 32
    
    mov rdi, rcx                     ; output buffer
    
    ; Zero the output buffer first
    mov ecx, 320
    xor eax, eax
    push rdi
    rep stosb
    pop rdi
    
    ; Fill header: [0] magic, [4] route_count, [8] uptime
    mov DWORD PTR [rdi], 52545200h   ; 'RTR\0' magic
    mov eax, DWORD PTR [g_route_count]
    mov DWORD PTR [rdi+4], eax
    
    ; Calculate uptime
    call GetTickCount64
    sub rax, QWORD PTR [g_router_start_tick]
    mov QWORD PTR [rdi+8], rax       ; uptime in ms
    
    ; Copy per-route stats (hit counts) starting at offset 16
    mov rsi, QWORD PTR [g_route_table]
    test rsi, rsi
    jz @@rgs_done
    
    mov ecx, DWORD PTR [g_route_count]
    cmp ecx, 9                       ; max 9 routes in 320-16 bytes
    jbe @@rgs_copy
    mov ecx, 9
@@rgs_copy:
    xor edx, edx
@@rgs_entry:
    cmp edx, ecx
    jae @@rgs_done
    imul eax, edx, 32               ; source entry
    mov ebx, DWORD PTR [rsi+rax+12]  ; hits
    imul eax, edx, 32               ; dest offset
    mov DWORD PTR [rdi+16+rax], ebx
    inc edx
    jmp @@rgs_entry
    
@@rgs_done:
    mov eax, DWORD PTR [g_route_count]
    add rsp, 32
    pop rbx
    pop rsi
    pop rdi
    ret
Router_GetStats ENDP

END
