option casemap:none

.data
    align 8
    g_profile_last_tsc dq 0
    g_profile_max_tsc  dq 0

.code

; void __stdcall Rawr_Profile_Checkpoint(
;     uint64_t rf_data_seq,
;     uint64_t rf_consumed_seq,
;     uint64_t rf_frame_ready,
;     uint64_t* out_cycles,
;     uint64_t* out_drift,
;     uint64_t* out_max_cycles);
;
; Win64 calling convention:
;   rcx, rdx, r8, r9 = first 4 args
;   [rsp+20h] = out_drift (5th arg)
;   [rsp+28h] = out_max_cycles (6th arg)

public Rawr_Profile_Checkpoint
Rawr_Profile_Checkpoint proc
    ; Capture timestamp with serialization to reduce OoO skew.
    lfence
    rdtsc
    shl rdx, 32
    or  rax, rdx                   ; rax = now_tsc

    mov r11, rax                   ; preserve now_tsc
    mov r10, g_profile_last_tsc
    mov g_profile_last_tsc, r11

    xor rax, rax                   ; default delta=0 on first sample
    test r10, r10
    jz  profile_delta_ready

    mov rax, r11
    sub rax, r10                   ; rax = delta_tsc

profile_delta_ready:
    mov r10, g_profile_max_tsc
    cmp rax, r10
    jbe profile_max_ready
    mov g_profile_max_tsc, rax

profile_max_ready:
    ; out_cycles
    test r9, r9
    jz  profile_store_drift
    mov [r9], rax

profile_store_drift:
    ; out_drift = max(rf_data_seq - rf_consumed_seq, 0)
    mov r9, qword ptr [rsp+20h]
    test r9, r9
    jz  profile_store_max

    xor r10, r10
    cmp rcx, rdx
    jb  profile_write_drift
    mov r10, rcx
    sub r10, rdx

profile_write_drift:
    mov [r9], r10

profile_store_max:
    ; out_max_cycles
    mov r9, qword ptr [rsp+28h]
    test r9, r9
    jz  profile_done
    mov r10, g_profile_max_tsc
    mov [r9], r10

profile_done:
    ret
Rawr_Profile_Checkpoint endp

end
