;-------------------------------------------------------------------------
; MONOLITHIC SOVEREIGN CORE - MASM64
;-------------------------------------------------------------------------
; Consolidated Assembly Core for RawrXD
; Includes:
; - Win32 IDE Shell & Message Dispatcher
; - Agentic Orchestra (Beacon & Task Routing)
; - QuadBuffer Inference Kernel (Tensor Pipelining)
; - VRAM Pressure Telemetry
;-------------------------------------------------------------------------

; --- Win32 API Externals ---
extern GetMessageA : proc
extern TranslateMessage : proc
extern DispatchMessageA : proc
extern PostQuitMessage : proc
extern DefWindowProcA : proc

; --- Agentic / Inference Externals ---
extern BeaconSend     : proc   ; BeaconSend(slot:DWORD, pData:QWORD, len:DWORD)
extern GetTickCount64 : proc   ; GetTickCount64() -> QWORD in RAX
extern RunInference   : proc   ; RunInference(pData:QWORD, dataLen:DWORD)

; --- Agentic Orchestra Externals ---
; Beacons for cross-agent signal routing
public szBeacon_Ready
public szBeacon_TaskPending
public szBeacon_InferenceStall

; --- Data Section ---
.data
    szAppName           db "RawrXD Sovereign IDE", 0
    szClassName         db "RawrXD_IDE_Class", 0
    
    ; Agentic Beacons
    szBeacon_Ready      db "AGENT_READY", 0
    szBeacon_TaskPending db "TASK_PENDING", 0
    szBeacon_InferenceStall db "INF_STALL", 0
    
    ; Pressure / Telemetry
    szBeacon_VramHigh   db "VRAM_PRESSURE_HIGH", 0
    vram_current_usage  dq 0            ; updated by the runtime telemetry pump

    ; QuadBuffer Meta
    VRAM_LIMIT_BYTES    dq 17179869184 ; 16GB
    
    ; Constants
    SLOT_STATE_OFFSET    equ 4
    SLOT_LAYER_ID_OFFSET equ 8
    SLOT_VRAM_ADDR_OFFSET equ 16

.code

;-------------------------------------------------------------------------
; SECTION 1: IDE SHELL & UI DISPATCHER
;-------------------------------------------------------------------------

rawrxd_run_monolithic_ui proc
    sub rsp, 40 ; Shadow space
    sub rsp, 48 ; MSG struct
    mov rdi, rsp 

@loop:
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    mov rcx, rdi
    call GetMessageA
    test eax, eax
    jz @exit
    mov rcx, rdi
    call TranslateMessage
    mov rcx, rdi
    call DispatchMessageA
    jmp @loop

@exit:
    add rsp, 48
    add rsp, 40
    ret
rawrxd_run_monolithic_ui endp

rawrxd_monolithic_wndproc proc
    ; RCX=hwnd, RDX=msg, R8=wparam, R9=lparam
    cmp edx, 0002h ; WM_DESTROY
    je @destroy
    jmp DefWindowProcA

@destroy:
    xor ecx, ecx
    call PostQuitMessage
    xor eax, eax
    ret
rawrxd_monolithic_wndproc endp

;-------------------------------------------------------------------------
; SECTION 2: QUADBUFFER INFERENCE KERNEL
;-------------------------------------------------------------------------

rawrxd_prefetch_tensor_async proc
    ; RCX = vram_addr, RDX = layer_id, R8 = node_id
    prefetchnta [rcx]
    rdtsc
    shl rdx, 32
    or rax, rdx
    ret
rawrxd_prefetch_tensor_async endp

rawrxd_rotate_slots proc
    ; RCX = pipeline_ptr, RDX = active_idx
    mov rax, rdx
    shl rax, 5 ; active_idx * 32
    add rax, rcx
    mov dword ptr [rax + SLOT_STATE_OFFSET], 4 ; RECYCLING
    
    inc edx
    and edx, 3 ; Mod 4
    
    mov rax, rdx
    shl rax, 5
    add rax, rcx
    mov dword ptr [rax + SLOT_STATE_OFFSET], 3 ; ACTIVE
    
    mov eax, edx ; Return new active_idx
    ret
rawrxd_rotate_slots endp

;-------------------------------------------------------------------------
; SECTION 3: AGENTIC ORCHESTRA (BEACON ROUTING)
;-------------------------------------------------------------------------

;-------------------------------------------------------------------------
; rawrxd_route_beacon
;   RCX = pStr  - pointer to NUL-terminated beacon string
;   RDX = agentID - 32-bit target agent slot identifier
;
;   Builds a 32-byte payload on the stack:
;     +0  dd  agentID
;     +4  dd  msgLen   (strlen of pStr)
;     +8  dq  pStr     (original pointer)
;     +16 dq  timestamp (GetTickCount64)
;     +24 dd  flags    (BEACON_ACTIVE = 1)
;     +28 dd  pad
;
;   Calls BeaconSend(slot, pPayload, 32)
;   Returns EAX = 0 on success, -1 on failure
;-------------------------------------------------------------------------
rawrxd_route_beacon proc FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    ; Stack after 3 pushes + retaddr: RSP % 16 == 0
    ; 64 = 32 shadow + 32 payload  (64 % 16 == 0 -> stays aligned)
    sub     rsp, 64
    .allocstack 64
    .endprolog

    mov     rbx, rdx            ; rbx = agentID (survives calls)
    mov     rsi, rcx            ; rsi = pStr   (survives calls)

    ;--- strlen(pStr) -> edi -----------------------------------------------
    xor     edi, edi
@@rb_strlen:
    cmp     byte ptr [rsi + rdi], 0
    je      @@rb_strlen_done
    inc     edi
    jmp     @@rb_strlen
@@rb_strlen_done:

    ;--- GetTickCount64() -> r10 --------------------------------------------
    call    GetTickCount64
    mov     r10, rax

    ;--- Populate 32-byte payload at [rsp+32] (after shadow space) ---------
    mov     dword ptr [rsp+32], ebx     ; +0  agentID
    mov     dword ptr [rsp+36], edi     ; +4  msgLen
    mov     qword ptr [rsp+40], rsi     ; +8  pStr
    mov     qword ptr [rsp+48], r10     ; +16 timestamp
    mov     dword ptr [rsp+56], 1       ; +24 flags = BEACON_ACTIVE
    mov     dword ptr [rsp+60], 0       ; +28 pad

    ;--- BeaconSend(slot=agentID, pData=payload, len=32) -------------------
    mov     ecx, ebx                    ; slot
    lea     rdx, [rsp+32]               ; pData
    mov     r8d, 32                     ; len
    call    BeaconSend

    test    eax, eax
    jz      @@rb_ok
    mov     eax, -1
    jmp     @@rb_ret
@@rb_ok:
    xor     eax, eax
@@rb_ret:
    add     rsp, 64
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_route_beacon endp

;-------------------------------------------------------------------------
; rawrxd_inference_dispatch
;   RCX = pTask  - pointer to task record:
;     +0  dd  taskType   (1=inference 2=hotswap 3=audit 4=generate)
;     +4  dd  priority
;     +8  dq  pData
;     +16 dd  dataLen
;
;   type 1 -> RunInference(pData, dataLen)
;   types 2-4 -> rawrxd_route_beacon with appropriate beacon string
;   Returns EAX = 0 success, -1 on null pTask or unknown type
;-------------------------------------------------------------------------
rawrxd_inference_dispatch proc FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    ; 3 pushes -> RSP % 16 == 0 before sub
    ; 48 = 32 shadow + 16 scratch (48 % 16 == 0)
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ;--- Validate pTask  ---------------------------------------------------
    mov     rbx, rcx
    test    rbx, rbx
    jz      @@id_fail

    ;--- Decode record fields into preserved registers ----------------------
    mov     eax,  dword ptr [rbx]       ; taskType
    mov     esi,  dword ptr [rbx+4]     ; priority  (informational)
    mov     rdi,  qword ptr [rbx+8]     ; pData
    mov     r10d, dword ptr [rbx+16]    ; dataLen

    ;--- Dispatch on taskType ----------------------------------------------
    cmp     eax, 1
    je      @@id_inference
    cmp     eax, 2
    je      @@id_hotswap
    cmp     eax, 3
    je      @@id_audit
    cmp     eax, 4
    je      @@id_generate
    jmp     @@id_fail

@@id_inference:
    ; RunInference(pData, dataLen)
    mov     rcx, rdi
    mov     edx, r10d
    call    RunInference
    jmp     @@id_done

@@id_hotswap:
    ; TASK_PENDING beacon -> hotswap orchestrator (agent 0xFE)
    lea     rcx, szBeacon_TaskPending
    mov     edx, 0FEh
    call    rawrxd_route_beacon
    jmp     @@id_done

@@id_audit:
    ; AGENT_READY beacon -> audit observer (agent 0xFD)
    lea     rcx, szBeacon_Ready
    mov     edx, 0FDh
    call    rawrxd_route_beacon
    jmp     @@id_done

@@id_generate:
    ; TASK_PENDING beacon -> generation worker (agent 0xFC)
    lea     rcx, szBeacon_TaskPending
    mov     edx, 0FCh
    call    rawrxd_route_beacon
    jmp     @@id_done

@@id_fail:
    mov     eax, -1
    jmp     @@id_ret

@@id_done:
    xor     eax, eax
@@id_ret:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_inference_dispatch endp

;-------------------------------------------------------------------------
; SECTION 4: VRAM TELEMETRY
;-------------------------------------------------------------------------

rawrxd_calc_pressure proc
    ; RCX = current_usage_bytes
    ; Returns EAX = pressure percentage (0-100, clamped)
    mov     rax, rcx
    mov     rdx, 100
    mul     rdx
    mov     r8,  [VRAM_LIMIT_BYTES]
    div     r8
    cmp     rax, 100
    jbe     @@cp_done
    mov     rax, 100
@@cp_done:
    ret
rawrxd_calc_pressure endp

;-------------------------------------------------------------------------
; rawrxd_pressure_alert
;   No parameters  - reads vram_current_usage global
;
;   Calls rawrxd_calc_pressure(vram_current_usage).
;   If returned pressure >= 85, fires:
;       rawrxd_route_beacon("VRAM_PRESSURE_HIGH", 0xFF)
;   Returns EAX = pressure percentage
;-------------------------------------------------------------------------
rawrxd_pressure_alert proc FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    ; 3 pushes -> RSP % 16 == 0 before sub
    ; 48 = 32 shadow + 16 scratch  (48 % 16 == 0)
    sub     rsp, 48
    .allocstack 48
    .endprolog

    ;--- Read current VRAM usage and compute pressure ----------------------
    mov     rcx, qword ptr [vram_current_usage]
    call    rawrxd_calc_pressure
    mov     ebx, eax            ; ebx = pressure % (preserved across next call)

    ;--- Threshold check: fire beacon if pressure >= 85 -------------------
    cmp     eax, 85
    jb      @@pa_done

    ; rawrxd_route_beacon("VRAM_PRESSURE_HIGH", 0xFF)
    lea     rcx, szBeacon_VramHigh
    mov     edx, 0FFh           ; agent 0xFF = pressure monitor
    call    rawrxd_route_beacon

@@pa_done:
    mov     eax, ebx            ; return pressure percentage
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_pressure_alert endp

end
