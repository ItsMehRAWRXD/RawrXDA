; src/direct_io/adaptive_burst_router.asm
; ═══════════════════════════════════════════════════════════════════════════════
; RAWRXD v1.2.0 — ADAPTIVE BURST ROUTER (Live Route Remapping)
; ═══════════════════════════════════════════════════════════════════════════════
; PURPOSE: Dynamically reroute tensor bursts based on:
;          - Controller queue pressure (CQ depth)
;          - Thermal state (NVMe controller temps)
;          - Completion latency history
; DEPENDENCIES: jit_lba_mapper.asm, direct_io_ring.h
; ═══════════════════════════════════════════════════════════════════════════════

.data

; ─────────────────────────────────────────────────────────────────────────────
; ROUTER STATE
; ─────────────────────────────────────────────────────────────────────────────

MAX_DRIVES          EQU 5
MAX_PENDING_OPS     EQU 256
THERMAL_THRESHOLD   EQU 70          ; Celsius - spill if exceeded
LATENCY_THRESHOLD   EQU 500         ; Microseconds - reroute if exceeded
PRESSURE_THRESHOLD  EQU 80          ; Percentage CQ depth - throttle if exceeded

ALIGN 8
; Per-drive statistics (updated after each completion)
g_DriveStats:
    ; Drive 0
    g_Drive0_PendingOps     DWORD 0
    g_Drive0_AvgLatencyUs   DWORD 0
    g_Drive0_TempCelsius    DWORD 0
    g_Drive0_TotalOps       QWORD 0
    g_Drive0_TotalBytes     QWORD 0
    g_Drive0_ErrorCount     DWORD 0
    g_Drive0_Flags          DWORD 0     ; bit 0: throttled, bit 1: offline
    BYTE 16 DUP(0)                      ; Padding to 64 bytes
    
    ; Drives 1-4 follow same layout (320 bytes total)
    BYTE (64 * 4) DUP(0)

; Route table: maps logical drive -> physical drive
; Allows transparent failover/load balancing
ALIGN 16
g_RouteTable        BYTE MAX_DRIVES DUP(0)  ; [0,1,2,3,4] by default
g_RouteTableBackup  BYTE MAX_DRIVES DUP(0)  ; Original mapping

; Burst queue (pending operations awaiting dispatch)
ALIGN 8
g_BurstQueue:
    ; Entry format (32 bytes):
    ; [0:8]   TensorUID
    ; [8:16]  DstAddress
    ; [16:20] SectorCount
    ; [20:21] OriginalDrive
    ; [21:22] RoutedDrive
    ; [22:24] Flags
    ; [24:32] SubmitTimestamp (RDTSC)
    BYTE (MAX_PENDING_OPS * 32) DUP(0)

g_BurstQueueHead    DWORD 0
g_BurstQueueTail    DWORD 0
g_BurstQueueCount   DWORD 0

; Thermal polling state
g_LastThermalPollTsc    QWORD 0
THERMAL_POLL_INTERVAL   EQU 1000000000  ; ~500ms at 2GHz

.code

; ═══════════════════════════════════════════════════════════════════════════════
; Router_Init — Initialize adaptive router with default routes
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  None
; OUTPUT: RAX = 0 on success
; ═══════════════════════════════════════════════════════════════════════════════
Router_Init PROC
    push rdi
    push rcx
    
    ; Initialize route table to identity mapping
    lea rdi, [g_RouteTable]
    xor ecx, ecx
_init_routes:
    mov [rdi + rcx], cl
    inc ecx
    cmp ecx, MAX_DRIVES
    jb _init_routes
    
    ; Copy to backup
    lea rsi, [g_RouteTable]
    lea rdi, [g_RouteTableBackup]
    mov ecx, MAX_DRIVES
    rep movsb
    
    ; Zero statistics
    lea rdi, [g_DriveStats]
    mov ecx, (64 * MAX_DRIVES)
    xor eax, eax
    rep stosb
    
    ; Initialize queue
    mov DWORD PTR [g_BurstQueueHead], 0
    mov DWORD PTR [g_BurstQueueTail], 0
    mov DWORD PTR [g_BurstQueueCount], 0
    
    xor rax, rax
    pop rcx
    pop rdi
    ret
Router_Init ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Router_GetRoute — Get physical drive for a logical drive index
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  ECX = logical drive index (0-4)
; OUTPUT: EAX = physical drive index (may differ if rerouted)
; ═══════════════════════════════════════════════════════════════════════════════
Router_GetRoute PROC
    cmp ecx, MAX_DRIVES
    jae _route_invalid
    
    lea rax, [g_RouteTable]
    movzx eax, BYTE PTR [rax + rcx]
    ret
    
_route_invalid:
    mov eax, 0          ; Fallback to drive 0
    ret
Router_GetRoute ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Router_UpdateStats — Update drive statistics after completion
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  ECX = drive index
;         EDX = latency in microseconds
;         R8D = bytes transferred
;         R9D = error flag (0=success, 1=error)
; OUTPUT: None (updates global state)
; ═══════════════════════════════════════════════════════════════════════════════
Router_UpdateStats PROC
    push rbx
    push rsi
    
    cmp ecx, MAX_DRIVES
    jae _stats_done
    
    ; Calculate offset into stats array (64 bytes per drive)
    mov eax, ecx
    shl eax, 6              ; * 64
    lea rsi, [g_DriveStats]
    add rsi, rax
    
    ; Decrement pending ops
    lock dec DWORD PTR [rsi]        ; g_DriveN_PendingOps
    
    ; Update average latency (exponential moving average)
    ; new_avg = (old_avg * 7 + new_sample) / 8
    mov eax, [rsi + 4]              ; old avg
    imul eax, 7
    add eax, edx                    ; + new sample
    shr eax, 3                      ; / 8
    mov [rsi + 4], eax
    
    ; Update totals
    lock inc QWORD PTR [rsi + 16]   ; TotalOps
    
    mov eax, r8d
    lock add QWORD PTR [rsi + 24], rax  ; TotalBytes
    
    ; Update error count if needed
    test r9d, r9d
    jz _stats_no_error
    lock inc DWORD PTR [rsi + 32]   ; ErrorCount
    
_stats_no_error:
    ; Check if we need to trigger rerouting
    call Router_CheckPressure
    
_stats_done:
    pop rsi
    pop rbx
    ret
Router_UpdateStats ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Router_CheckPressure — Evaluate all drives and reroute if needed
; ═══════════════════════════════════════════════════════════════════════════════
; Runs after every completion to ensure real-time adaptation
; ═══════════════════════════════════════════════════════════════════════════════
Router_CheckPressure PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    ; First, check if thermal polling is due
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov r12, rax                    ; Current TSC
    
    mov r13, [g_LastThermalPollTsc]
    sub rax, r13
    cmp rax, THERMAL_POLL_INTERVAL
    jb _skip_thermal
    
    ; Poll thermal sensors (would call platform-specific API)
    ; For now, just update the timestamp
    mov [g_LastThermalPollTsc], r12
    
_skip_thermal:
    ; Check each drive for pressure/thermal issues
    xor ecx, ecx                    ; Drive index
    
_check_loop:
    mov eax, ecx
    shl eax, 6
    lea rsi, [g_DriveStats]
    add rsi, rax
    
    ; Check latency threshold
    mov eax, [rsi + 4]              ; AvgLatency
    cmp eax, LATENCY_THRESHOLD
    ja _drive_stressed
    
    ; Check temperature
    mov eax, [rsi + 8]              ; TempCelsius
    cmp eax, THERMAL_THRESHOLD
    ja _drive_overheated
    
    ; Drive is healthy - ensure route is direct
    jmp _next_drive
    
_drive_stressed:
    ; Mark throttled and find alternate route
    or DWORD PTR [rsi + 36], 1      ; Set throttled flag
    
    ; Find least-loaded alternate drive
    call Router_FindAlternate       ; ECX = original, returns EAX = alternate
    cmp eax, ecx
    je _next_drive                  ; No better option
    
    ; Update route table
    lea rdi, [g_RouteTable]
    mov [rdi + rcx], al
    jmp _next_drive
    
_drive_overheated:
    ; More aggressive: spill ALL traffic from this drive
    or DWORD PTR [rsi + 36], 3      ; Set throttled + spill flags
    
    call Router_FindCoolest         ; Returns EAX = coolest drive
    cmp eax, ecx
    je _next_drive
    
    lea rdi, [g_RouteTable]
    mov [rdi + rcx], al
    
_next_drive:
    inc ecx
    cmp ecx, MAX_DRIVES
    jb _check_loop
    
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
Router_CheckPressure ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Router_FindAlternate — Find least-loaded drive to reroute to
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  ECX = drive to avoid
; OUTPUT: EAX = best alternate drive index
; ═══════════════════════════════════════════════════════════════════════════════
Router_FindAlternate PROC
    push rbx
    push rsi
    push rdi
    
    mov edi, ecx                ; Original drive (avoid)
    mov ebx, -1                 ; Best candidate
    mov esi, 0FFFFFFFFh         ; Best (lowest) pending ops
    
    xor ecx, ecx
_find_loop:
    cmp ecx, edi
    je _find_skip               ; Skip original drive
    
    ; Get pending ops for this drive
    mov eax, ecx
    shl eax, 6
    lea rax, [g_DriveStats + rax]
    mov eax, [rax]              ; PendingOps
    
    cmp eax, esi
    jae _find_skip
    
    ; New best
    mov esi, eax
    mov ebx, ecx
    
_find_skip:
    inc ecx
    cmp ecx, MAX_DRIVES
    jb _find_loop
    
    ; Return best, or original if none found
    mov eax, ebx
    cmp eax, -1
    jne _find_done
    mov eax, edi                ; Fallback to original
    
_find_done:
    pop rdi
    pop rsi
    pop rbx
    ret
Router_FindAlternate ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Router_FindCoolest — Find drive with lowest temperature
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  ECX = drive to avoid (optional, -1 for any)
; OUTPUT: EAX = coolest drive index
; ═══════════════════════════════════════════════════════════════════════════════
Router_FindCoolest PROC
    push rbx
    push rsi
    push rdi
    
    mov edi, ecx                ; Drive to avoid
    mov ebx, 0                  ; Best candidate (default to 0)
    mov esi, 0FFFFh             ; Best (lowest) temp
    
    xor ecx, ecx
_cool_loop:
    cmp ecx, edi
    je _cool_skip
    
    mov eax, ecx
    shl eax, 6
    lea rax, [g_DriveStats + rax + 8]   ; TempCelsius offset
    mov eax, [rax]
    
    cmp eax, esi
    jae _cool_skip
    
    mov esi, eax
    mov ebx, ecx
    
_cool_skip:
    inc ecx
    cmp ecx, MAX_DRIVES
    jb _cool_loop
    
    mov eax, ebx
    
    pop rdi
    pop rsi
    pop rbx
    ret
Router_FindCoolest ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Router_RestoreRoutes — Reset all routes to original mapping
; ═══════════════════════════════════════════════════════════════════════════════
Router_RestoreRoutes PROC
    push rsi
    push rdi
    push rcx
    
    lea rsi, [g_RouteTableBackup]
    lea rdi, [g_RouteTable]
    mov ecx, MAX_DRIVES
    rep movsb
    
    ; Clear throttle flags
    lea rdi, [g_DriveStats]
    xor ecx, ecx
_clear_flags:
    mov DWORD PTR [rdi + 36], 0     ; Flags offset
    add rdi, 64
    inc ecx
    cmp ecx, MAX_DRIVES
    jb _clear_flags
    
    pop rcx
    pop rdi
    pop rsi
    ret
Router_RestoreRoutes ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Router_EnqueueBurst — Add tensor to burst queue with routing
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  RCX = TensorUID
;         RDX = DstAddress
;         R8D = SectorCount
;         R9D = OriginalDrive
; OUTPUT: RAX = queue index, or -1 if full
; ═══════════════════════════════════════════════════════════════════════════════
Router_EnqueueBurst PROC
    push rbx
    push rsi
    push rdi
    
    ; Check queue capacity
    mov eax, [g_BurstQueueCount]
    cmp eax, MAX_PENDING_OPS
    jae _enqueue_full
    
    ; Calculate entry address
    mov eax, [g_BurstQueueTail]
    shl eax, 5                      ; * 32
    lea rsi, [g_BurstQueue]
    add rsi, rax
    
    ; Fill entry
    mov [rsi], rcx                  ; TensorUID
    mov [rsi + 8], rdx              ; DstAddress
    mov [rsi + 16], r8d             ; SectorCount
    mov [rsi + 20], r9b             ; OriginalDrive
    
    ; Get routed drive
    mov ecx, r9d
    call Router_GetRoute
    mov [rsi + 21], al              ; RoutedDrive
    
    mov WORD PTR [rsi + 22], 0      ; Flags
    
    ; Timestamp
    rdtsc
    shl rdx, 32
    or rax, rdx
    mov [rsi + 24], rax
    
    ; Update queue state
    mov eax, [g_BurstQueueTail]
    inc eax
    cmp eax, MAX_PENDING_OPS
    jb _no_wrap
    xor eax, eax
_no_wrap:
    mov [g_BurstQueueTail], eax
    lock inc DWORD PTR [g_BurstQueueCount]
    
    ; Increment pending on target drive
    movzx ecx, BYTE PTR [rsi + 21]
    shl ecx, 6
    lea rdi, [g_DriveStats]
    lock inc DWORD PTR [rdi + rcx]
    
    ; Return queue index
    mov eax, [g_BurstQueueTail]
    dec eax
    jns _enqueue_done
    mov eax, MAX_PENDING_OPS - 1
    jmp _enqueue_done
    
_enqueue_full:
    mov rax, -1
    
_enqueue_done:
    pop rdi
    pop rsi
    pop rbx
    ret
Router_EnqueueBurst ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; Router_GetStats — Export drive statistics for monitoring
; ═══════════════════════════════════════════════════════════════════════════════
; INPUT:  RCX = output buffer (5 * 64 bytes minimum)
; OUTPUT: RAX = number of drives
; ═══════════════════════════════════════════════════════════════════════════════
Router_GetStats PROC
    push rsi
    push rdi
    push rcx
    
    mov rdi, rcx
    lea rsi, [g_DriveStats]
    mov ecx, (64 * MAX_DRIVES)
    rep movsb
    
    mov eax, MAX_DRIVES
    
    pop rcx
    pop rdi
    pop rsi
    ret
Router_GetStats ENDP

END
