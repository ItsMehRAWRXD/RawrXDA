; ============================================================================
; startup_trace_masm64.asm  --  Pure x64 MASM startup diagnostics
;
; Exports:
;   rawrxd_startup_trace(const char* step, const char* detail)
;   rawrxd_startup_peb_walk_snapshot()
;
; Rules:
;   option casemap:none, no .model
;   x64 ABI: RCX/RDX/R8/R9 + 32-byte shadow pre-allocated + 16-byte align
;   EXTERNDEF OutputDebugStringA:PROC / GetTickCount64:PROC
;   PROC FRAME / .ENDPROLOG for full unwind coverage on every non-volatile reg used
;   Zero CRT: no wsprintfA, no s_startupLog, no streams, no mutexes
;   String building done inline via byte loops
; ============================================================================

OPTION CASEMAP:NONE

EXTERNDEF OutputDebugStringA:PROC
EXTERNDEF GetTickCount64:PROC

PUBLIC rawrxd_startup_trace
PUBLIC rawrxd_startup_peb_walk_snapshot

; LDR offsets relative to InMemoryOrderLinks node pointer
; (LDR_DATA_TABLE_ENTRY.InMemoryOrderLinks sits at entry+0x10)
; So node-based offset = entry-offset - 0x10.
LDRE_DLLBASE        EQU 020h    ; DllBase    (entry+030h -> node+020h)
LDRE_FULLDLL_LEN    EQU 038h    ; FullDllName.Length USHORT bytes
LDRE_FULLDLL_BUF    EQU 040h    ; FullDllName.Buffer PWSTR
PEB_LDRDATA         EQU 018h    ; PEB.Ldr offset
LDR_INMOD_FLINK     EQU 020h    ; PEB_LDR_DATA.InMemoryOrderModuleList.Flink
MAX_MODULES         EQU 32
NAME_CLAMP          EQU 511     ; max wide chars to convert (leaves room for NUL)

.data
    align 8
    szRSTprefix     DB "[RAWR] ", 0
    szRSTcrlf       DB 13, 10, 0
    szPebWalk       DB "peb_walk", 0
    szPebBegin      DB "begin", 0
    szPebNoPeb      DB "no_peb", 0
    szPebNoLdr      DB "no_ldr", 0
    szPebMod        DB "peb_mod", 0
    szPebDonePfx    DB "done ", 0
    szUnnamed       DB "<unnamed>", 0

.code

; =============================================================================
; rawrxd_startup_trace
;
;   void rawrxd_startup_trace(const char* step, const char* detail)
;                              RCX                RDX
;
; Builds "[RAWR] <tick_ms> <step>[ <detail>]\r\n" and calls OutputDebugStringA.
; No CRT, no wsprintfA, no file I/O.
;
; Stack frame:
;   push rbx/rsi/rdi (3x8=24B) + sub rsp,576 (32 shadow+512 outBuf+32 scratch)
;   entry RSP=8%16 -> -24=0%16 -> -576(mod16=0)=0%16  (before CALL = aligned)
;   [rsp+  0.. 31]  shadow for callees
;   [rsp+ 32..543]  outBuf[512]
;   [rsp+544..575]  digit scratch[32]
; =============================================================================
rawrxd_startup_trace PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 576
    .allocstack 576
    .endprolog

    ; Non-volatile regs survive GetTickCount64 call below
    mov     rbx, rcx                ; rbx = step (const char*)
    mov     rsi, rdx                ; rsi = detail (const char*, may be NULL)

    ; timestamp
    call    GetTickCount64          ; RAX = tick_ms

    ; begin formatting into outBuf at [rsp+32]
    lea     rdi, [rsp+32]

    ; "[RAWR] "
    lea     rcx, szRSTprefix
RST_pfx:
    mov     al, byte ptr [rcx]
    test    al, al
    jz      RST_pfx_end
    mov     byte ptr [rdi], al
    inc     rcx
    inc     rdi
    jmp     RST_pfx
RST_pfx_end:

    ; decimal tick (RAX) -> outBuf  via digit scratch at [rsp+544]
    lea     r10, [rsp+544]
    xor     r11d, r11d
    mov     r9d, 10
    test    rax, rax
    jz      RST_tick_zero
RST_tick_loop:
    xor     rdx, rdx
    div     r9                      ; rax=quotient  rdx=remainder digit
    add     dl, '0'
    mov     byte ptr [r10+r11], dl
    inc     r11
    test    rax, rax
    jnz     RST_tick_loop
    mov     rcx, r11                ; digit count
RST_tick_rev:
    dec     rcx
    movzx   eax, byte ptr [r10+rcx]
    mov     byte ptr [rdi], al
    inc     rdi
    test    rcx, rcx
    jnz     RST_tick_rev
    jmp     RST_tick_end
RST_tick_zero:
    mov     byte ptr [rdi], '0'
    inc     rdi
RST_tick_end:

    ; " " + step
    mov     byte ptr [rdi], ' '
    inc     rdi
    mov     rcx, rbx
RST_step:
    mov     al, byte ptr [rcx]
    test    al, al
    jz      RST_step_end
    mov     byte ptr [rdi], al
    inc     rcx
    inc     rdi
    jmp     RST_step
RST_step_end:

    ; optional " " + detail
    test    rsi, rsi
    jz      RST_det_skip
    cmp     byte ptr [rsi], 0
    je      RST_det_skip
    mov     byte ptr [rdi], ' '
    inc     rdi
    mov     rcx, rsi
RST_det:
    mov     al, byte ptr [rcx]
    test    al, al
    jz      RST_det_skip
    mov     byte ptr [rdi], al
    inc     rcx
    inc     rdi
    jmp     RST_det
RST_det_skip:

    ; "\r\n\0"
    mov     byte ptr [rdi],   13
    mov     byte ptr [rdi+1], 10
    mov     byte ptr [rdi+2], 0

    ; OutputDebugStringA(outBuf)
    lea     rcx, [rsp+32]
    call    OutputDebugStringA

    add     rsp, 576
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_startup_trace ENDP

; =============================================================================
; rawrxd_startup_peb_walk_snapshot
;
;   void rawrxd_startup_peb_walk_snapshot(void)
;
; Reads GS:[60h]->PEB, walks PEB_LDR_DATA.InMemoryOrderModuleList,
; calls rawrxd_startup_trace("peb_mod","<N> <fullname>") per module.
; Max 32 modules.  No CRT, no wsprintfA, no heap.
;
; Offsets from node (InMemoryOrderLinks*), which is LDR_DATA_TABLE_ENTRY+0x10:
;   node+0x20 = DllBase
;   node+0x38 = FullDllName.Length (USHORT, byte count)
;   node+0x40 = FullDllName.Buffer (PWSTR)
;
; Stack frame:
;   push rbx/rsi/rdi/r12/r13/r14/r15 (7x8=56B) + sub rsp,320
;   entry=8%16 -> -56=0%16 (56%16=8,8-8=0) -> -320(mod16=0)=0%16  (before CALL = aligned)
;   [rsp+  0.. 31]  shadow for callees
;   [rsp+ 32..287]  nameBuf[256]
;   [rsp+288..319]  digit scratch[32]
;
; Non-volatile register map (all .pushreg declared):
;   r12 = PEB*
;   r13 = head sentinel (&Ldr->InMemoryOrderModuleList)
;   r14 = current node (InMemoryOrderLinks*)
;   r15 = module counter
;   rbx = write ptr into nameBuf (reset each iteration)
;   rsi = PWSTR / string source ptr
;   rdi = char count during wide->ASCII copy
; =============================================================================
rawrxd_startup_peb_walk_snapshot PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 320
    .allocstack 320
    .endprolog

    ; "peb_walk" "begin"
    lea     rcx, szPebWalk
    lea     rdx, szPebBegin
    call    rawrxd_startup_trace

    mov     r12, qword ptr gs:[60h]     ; r12 = PEB*
    test    r12, r12
    jz      RPWS_no_peb

    mov     rax, qword ptr [r12 + PEB_LDRDATA]  ; rax = Ldr*
    test    rax, rax
    jz      RPWS_no_ldr

    ; r13 = head sentinel = &Ldr->InMemoryOrderModuleList
    lea     r13, [rax + LDR_INMOD_FLINK]
    mov     r14, qword ptr [r13]        ; r14 = first node (head->Flink)
    xor     r15d, r15d                  ; r15 = module counter

RPWS_loop:
    cmp     r14, r13
    je      RPWS_done
    test    r14, r14
    jz      RPWS_done
    cmp     r15d, MAX_MODULES
    jge     RPWS_done

    ; build nameBuf: "<count> <fullname>"
    lea     rbx, [rsp+32]               ; rbx = write ptr

    ; decimal r15 -> nameBuf  via scratch at [rsp+288]
    mov     rax, r15
    lea     r10, [rsp+288]
    xor     r11d, r11d
    mov     r9d, 10
    test    rax, rax
    jz      RPWS_cnt_zero
RPWS_cnt_loop:
    xor     rdx, rdx
    div     r9
    add     dl, '0'
    mov     byte ptr [r10+r11], dl
    inc     r11
    test    rax, rax
    jnz     RPWS_cnt_loop
    mov     rcx, r11
RPWS_cnt_rev:
    dec     rcx
    movzx   eax, byte ptr [r10+rcx]
    mov     byte ptr [rbx], al
    inc     rbx
    test    rcx, rcx
    jnz     RPWS_cnt_rev
    jmp     RPWS_cnt_end
RPWS_cnt_zero:
    mov     byte ptr [rbx], '0'
    inc     rbx
RPWS_cnt_end:
    mov     byte ptr [rbx], ' '
    inc     rbx

    ; wide FullDllName -> ASCII (low byte per WCHAR)
    movzx   edi, word ptr [r14 + LDRE_FULLDLL_LEN]   ; byte length
    sar     edi, 1                                      ; char count
    mov     rsi, qword ptr [r14 + LDRE_FULLDLL_BUF]   ; PWSTR

    test    edi, edi
    jz      RPWS_unnamed
    test    rsi, rsi
    jz      RPWS_unnamed

    cmp     edi, NAME_CLAMP
    jle     RPWS_wide
    mov     edi, NAME_CLAMP
RPWS_wide:
    movzx   eax, word ptr [rsi]
    mov     byte ptr [rbx], al
    inc     rbx
    add     rsi, 2
    dec     edi
    jnz     RPWS_wide
    jmp     RPWS_name_end

RPWS_unnamed:
    lea     rsi, szUnnamed
RPWS_unn:
    mov     al, byte ptr [rsi]
    test    al, al
    jz      RPWS_name_end
    mov     byte ptr [rbx], al
    inc     rsi
    inc     rbx
    jmp     RPWS_unn

RPWS_name_end:
    mov     byte ptr [rbx], 0           ; NUL-terminate nameBuf

    lea     rcx, szPebMod
    lea     rdx, [rsp+32]
    call    rawrxd_startup_trace

    mov     r14, qword ptr [r14]        ; advance: r14 = r14->Flink
    inc     r15d
    jmp     RPWS_loop

RPWS_done:
    ; "peb_walk" "done <count>"
    lea     rbx, [rsp+32]
    lea     rsi, szPebDonePfx
RPWS_done_pfx:
    mov     al, byte ptr [rsi]
    test    al, al
    jz      RPWS_done_num
    mov     byte ptr [rbx], al
    inc     rsi
    inc     rbx
    jmp     RPWS_done_pfx
RPWS_done_num:
    mov     rax, r15
    lea     r10, [rsp+288]
    xor     r11d, r11d
    mov     r9d, 10
    test    rax, rax
    jz      RPWS_done_zero
RPWS_done_loop:
    xor     rdx, rdx
    div     r9
    add     dl, '0'
    mov     byte ptr [r10+r11], dl
    inc     r11
    test    rax, rax
    jnz     RPWS_done_loop
    mov     rcx, r11
RPWS_done_rev:
    dec     rcx
    movzx   eax, byte ptr [r10+rcx]
    mov     byte ptr [rbx], al
    inc     rbx
    test    rcx, rcx
    jnz     RPWS_done_rev
    jmp     RPWS_done_end
RPWS_done_zero:
    mov     byte ptr [rbx], '0'
    inc     rbx
RPWS_done_end:
    mov     byte ptr [rbx], 0

    lea     rcx, szPebWalk
    lea     rdx, [rsp+32]
    call    rawrxd_startup_trace
    jmp     RPWS_exit

RPWS_no_peb:
    lea     rcx, szPebWalk
    lea     rdx, szPebNoPeb
    call    rawrxd_startup_trace
    jmp     RPWS_exit

RPWS_no_ldr:
    lea     rcx, szPebWalk
    lea     rdx, szPebNoLdr
    call    rawrxd_startup_trace

RPWS_exit:
    add     rsp, 320
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
rawrxd_startup_peb_walk_snapshot ENDP
END
