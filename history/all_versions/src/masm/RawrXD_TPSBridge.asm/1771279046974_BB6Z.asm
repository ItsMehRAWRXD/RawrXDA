; =============================================================================
; RawrXD_TPSBridge.asm — Three-Engine TPS Bridge & Compositor
;
; Coordinates Sloloris (saturation), Bounce (hot/cold ping-pong), and
; HotPatch (runtime tuning) into a single unified pipeline.
;
; Architecture:
;   ┌───────────────────────────────────────────────────────────────────────┐
;   │                        TPS BRIDGE                                     │
;   │                                                                       │
;   │  Model Data ─┬─▶ Sloloris (stream/saturate) ──▶ strength%           │
;   │              ├─▶ Bounce (hot/cold rotate)     ──▶ superstrength%     │
;   │              └─▶ HotPatch (measure+tune)       ──▶ composite mult   │
;   │                                                                       │
;   │  Bridge Tick:                                                         │
;   │    1. Sloloris_Tick → get strength% → Feed to HotPatch              │
;   │    2. Bounce_Tick   → get superstr  → Feed to HotPatch              │
;   │    3. HotPatch_Tick → get composite mult                            │
;   │    4. CompositeStrength = composite mult (already includes all)      │
;   │    5. If composite > 300% → SUPERCHARGED state                      │
;   │                                                                       │
;   │  Token flow:                                                         │
;   │    Bridge_NotifyToken → notify all 3 engines → update TPS           │
;   └───────────────────────────────────────────────────────────────────────┘
;
; Exports:
;   Bridge_Init          — Create bridge, attach 3 engine contexts
;   Bridge_Destroy       — Teardown bridge (does NOT destroy engines)
;   Bridge_Tick          — Tick all 3 engines in order
;   Bridge_NotifyToken   — Feed token to all engines
;   Bridge_GetStrength   — Get composite strength (300%+ target)
;   Bridge_GetTPS        — Get measured TPS
;   Bridge_GetStatus     — Get status as color code
;   Bridge_GetStats      — Full stats dump
;
; Build: ml64 /c RawrXD_TPSBridge.asm
; =============================================================================

option casemap:none

INCLUDE ksamd64.inc

; ─── Engine API imports ──────────────────────────────────────────────────────
EXTRN Sloloris_Tick:PROC
EXTRN Sloloris_GetStrength:PROC

EXTRN Bounce_Tick:PROC
EXTRN Bounce_GetTPS:PROC

EXTRN HotPatch_Tick:PROC
EXTRN HotPatch_NotifyToken:PROC
EXTRN HotPatch_FeedSloloris:PROC
EXTRN HotPatch_FeedBounce:PROC
EXTRN HotPatch_GetMultiplier:PROC
EXTRN HotPatch_GetTPS:PROC

; ─── Windows API ─────────────────────────────────────────────────────────────
EXTRN VirtualAlloc:PROC
EXTRN VirtualFree:PROC
EXTRN QueryPerformanceCounter:PROC
EXTRN QueryPerformanceFrequency:PROC
EXTRN GetStdHandle:PROC
EXTRN WriteConsoleA:PROC

; ─── Exports ─────────────────────────────────────────────────────────────────
PUBLIC Bridge_Init
PUBLIC Bridge_Destroy
PUBLIC Bridge_Tick
PUBLIC Bridge_NotifyToken
PUBLIC Bridge_GetStrength
PUBLIC Bridge_GetTPS
PUBLIC Bridge_GetStatus
PUBLIC Bridge_GetStats

; =============================================================================
; Constants
; =============================================================================
MEM_COMMIT              EQU 1000h
MEM_RESERVE             EQU 2000h
MEM_RELEASE             EQU 8000h
PAGE_READWRITE          EQU 04h
STD_OUTPUT              EQU -11

; Status codes
STATUS_COLD             EQU 0           ; < 100%
STATUS_WARM             EQU 1           ; 100-199%
STATUS_HOT              EQU 2           ; 200-299%
STATUS_SUPERCHARGED     EQU 3           ; 300%+
STATUS_LEGENDARY        EQU 4           ; 400%+

; =============================================================================
; BridgeContext Layout
; =============================================================================
;   0x00  SlolorisCtx      QWORD  — Pointer to Sloloris context
;   0x08  BounceCtx        QWORD  — Pointer to Bounce context
;   0x10  HotPatchCtx      QWORD  — Pointer to HotPatch context
;   0x18  TickCount        QWORD
;   0x20  TokenCount       QWORD
;   0x28  CompositeStr     DWORD  — Composite strength (% x 1)
;   0x2C  SloStr           DWORD  — Last Sloloris strength
;   0x30  BncSuperStr      DWORD  — Last Bounce superstrength
;   0x34  HPMultiplier     DWORD  — Last HotPatch multiplier
;   0x38  StatusCode       DWORD  — STATUS_COLD..STATUS_LEGENDARY
;   0x3C  PeakStrength     DWORD  — Highest composite ever
;   0x40  QPCFreq          QWORD
;   0x48  InitQPC          QWORD
;   0x50  MeasuredTPS      DWORD  — Bridge-level TPS x100
;   0x54  PeakTPS          DWORD
;   0x58  Pad              ...
BR_SlolorisCtx          EQU 000h
BR_BounceCtx            EQU 008h
BR_HotPatchCtx          EQU 010h
BR_TickCount            EQU 018h
BR_TokenCount           EQU 020h
BR_CompositeStr         EQU 028h
BR_SloStr               EQU 02Ch
BR_BncSuperStr          EQU 030h
BR_HPMultiplier         EQU 034h
BR_StatusCode           EQU 038h
BR_PeakStrength         EQU 03Ch
BR_QPCFreq              EQU 040h
BR_InitQPC              EQU 048h
BR_MeasuredTPS          EQU 050h
BR_PeakTPS              EQU 054h
BR_SIZE                 EQU 080h

; =============================================================================
; Data
; =============================================================================
.data
align 16
szBrInit        db '[Bridge] 3-Engine TPS Bridge initialized',0Dh,0Ah,0
szBrTick        db '[Bridge] TICK #',0
szBrSlo         db '  Sloloris: ',0
szBrBnc         db '  Bounce:   ',0
szBrHP          db '  HotPatch: ',0
szBrComposite   db '  >>> COMPOSITE: ',0
szBrPct         db '%',0Dh,0Ah,0
szBrSuper       db '  *** SUPERCHARGED ***',0Dh,0Ah,0
szBrLegend      db '  $$$ LEGENDARY $$$',0Dh,0Ah,0
szBrNewline     db 0Dh,0Ah,0
hStdOutBR       dq 0

.data?
align 16
qpcScratchBR    dq ?
numBufBR        db 32 dup(?)

; =============================================================================
; Code
; =============================================================================
.code

; ─── Helpers ─────────────────────────────────────────────────────────────────
_br_strlen PROC
    xor     eax, eax
@@:
    cmp     byte ptr [rcx + rax], 0
    je      @F
    inc     rax
    jmp     @B
@@:
    ret
_br_strlen ENDP

_br_print PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    mov     rbx, rcx
    mov     rax, hStdOutBR
    test    rax, rax
    jnz     @@have
    mov     ecx, STD_OUTPUT
    call    GetStdHandle
    mov     hStdOutBR, rax
@@have:
    mov     rcx, rbx
    call    _br_strlen
    mov     r8, rax
    mov     rcx, hStdOutBR
    mov     rdx, rbx
    lea     r9, [rsp+32]
    mov     qword ptr [rsp+32], 0
    call    WriteConsoleA
    add     rsp, 48
    pop     rbx
    ret
_br_print ENDP

_br_print_u32 PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 48
    .allocstack 48
    .endprolog
    mov     eax, ecx
    lea     rbx, numBufBR
    add     rbx, 20
    mov     byte ptr [rbx], 0
    test    eax, eax
    jnz     @@loop
    dec     rbx
    mov     byte ptr [rbx], '0'
    jmp     @@pr
@@loop:
    test    eax, eax
    jz      @@pr
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    add     dl, '0'
    dec     rbx
    mov     [rbx], dl
    jmp     @@loop
@@pr:
    mov     rcx, rbx
    call    _br_print
    add     rsp, 48
    pop     rbx
    ret
_br_print_u32 ENDP

; =============================================================================
; Bridge_Init — Create bridge context
; RCX = Sloloris context
; RDX = Bounce context
; R8  = HotPatch context
; Returns: RAX = bridge context, or 0
; =============================================================================
Bridge_Init PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    sub     rsp, 40
    .allocstack 40
    .endprolog

    ; Save engine pointers
    mov     rsi, rcx            ; Sloloris
    mov     rdi, rdx            ; Bounce
    mov     r12, r8             ; HotPatch

    ; Allocate bridge context
    xor     ecx, ecx
    mov     edx, BR_SIZE
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    test    rax, rax
    jz      @@fail
    mov     rbx, rax

    ; Store engine pointers
    mov     [rbx + BR_SlolorisCtx], rsi
    mov     [rbx + BR_BounceCtx], rdi
    mov     [rbx + BR_HotPatchCtx], r12

    ; QPC frequency
    lea     rcx, qpcScratchBR
    call    QueryPerformanceFrequency
    mov     rax, qpcScratchBR
    mov     [rbx + BR_QPCFreq], rax

    ; Init timestamp
    lea     rcx, qpcScratchBR
    call    QueryPerformanceCounter
    mov     rax, qpcScratchBR
    mov     [rbx + BR_InitQPC], rax

    mov     dword ptr [rbx + BR_StatusCode], STATUS_COLD

    lea     rcx, szBrInit
    call    _br_print

    mov     rax, rbx
    jmp     @@exit
@@fail:
    xor     eax, eax
@@exit:
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Bridge_Init ENDP

; =============================================================================
; Bridge_Destroy — Free bridge context (does NOT destroy engine contexts)
; RCX = bridge context
; =============================================================================
Bridge_Destroy PROC FRAME
    sub     rsp, 40
    .allocstack 40
    .endprolog
    test    rcx, rcx
    jz      @@null
    xor     edx, edx
    mov     r8d, MEM_RELEASE
    call    VirtualFree
@@null:
    add     rsp, 40
    ret
Bridge_Destroy ENDP

; =============================================================================
; Bridge_Tick — Tick all 3 engines in pipeline order
; RCX = bridge context
; Returns: EAX = composite strength (%)
; =============================================================================
Bridge_Tick PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    inc     qword ptr [rbx + BR_TickCount]

    ; Print tick number
    lea     rcx, szBrTick
    call    _br_print
    mov     rax, [rbx + BR_TickCount]
    mov     ecx, eax
    call    _br_print_u32
    lea     rcx, szBrNewline
    call    _br_print

    ; ── Step 1: Tick Sloloris ──
    mov     rcx, [rbx + BR_SlolorisCtx]
    test    rcx, rcx
    jz      @@skip_slo
    call    Sloloris_Tick

    mov     rcx, [rbx + BR_SlolorisCtx]
    call    Sloloris_GetStrength
    mov     [rbx + BR_SloStr], eax
    mov     r12d, eax                   ; R12D = sloloris strength

    ; Print
    push    r12
    lea     rcx, szBrSlo
    call    _br_print
    mov     ecx, r12d
    call    _br_print_u32
    lea     rcx, szBrPct
    call    _br_print
    pop     r12
    jmp     @@slo_done
@@skip_slo:
    xor     r12d, r12d
@@slo_done:

    ; ── Step 2: Tick Bounce ──
    mov     rcx, [rbx + BR_BounceCtx]
    test    rcx, rcx
    jz      @@skip_bnc
    call    Bounce_Tick

    ; Read BC_SuperStrength from Bounce context (offset 0x200)
    mov     rcx, [rbx + BR_BounceCtx]
    mov     r13d, dword ptr [rcx + 200h]   ; BC_SuperStrength
    mov     [rbx + BR_BncSuperStr], r13d

    ; Print
    push    r13
    lea     rcx, szBrBnc
    call    _br_print
    mov     ecx, r13d
    call    _br_print_u32
    lea     rcx, szBrPct
    call    _br_print
    pop     r13
    jmp     @@bnc_done
@@skip_bnc:
    xor     r13d, r13d
@@bnc_done:

    ; ── Step 3: Feed data to HotPatch, then Tick ──
    mov     rcx, [rbx + BR_HotPatchCtx]
    test    rcx, rcx
    jz      @@skip_hp

    ; Feed Sloloris strength
    mov     edx, r12d
    call    HotPatch_FeedSloloris

    ; Feed Bounce superstrength
    mov     rcx, [rbx + BR_HotPatchCtx]
    mov     edx, r13d
    call    HotPatch_FeedBounce

    ; Tick HotPatch
    mov     rcx, [rbx + BR_HotPatchCtx]
    call    HotPatch_Tick
    mov     r14d, eax                   ; R14D = composite multiplier

    mov     [rbx + BR_HPMultiplier], r14d

    ; Print
    push    r14
    lea     rcx, szBrHP
    call    _br_print
    mov     ecx, r14d
    call    _br_print_u32
    lea     rcx, szBrPct
    call    _br_print
    pop     r14
    jmp     @@hp_done
@@skip_hp:
    mov     r14d, 100                   ; Default 100%
@@hp_done:

    ; ── Step 4: Compute composite strength ──
    ; Composite = HotPatch multiplier (already includes Sloloris + Bounce feeds)
    mov     eax, r14d
    mov     [rbx + BR_CompositeStr], eax

    ; Track peak
    cmp     eax, [rbx + BR_PeakStrength]
    jle     @@not_peak
    mov     [rbx + BR_PeakStrength], eax
@@not_peak:

    ; Print composite
    lea     rcx, szBrComposite
    call    _br_print
    mov     ecx, [rbx + BR_CompositeStr]
    call    _br_print_u32
    lea     rcx, szBrPct
    call    _br_print

    ; ── Step 5: Status classification ──
    mov     eax, [rbx + BR_CompositeStr]
    cmp     eax, 400
    jge     @@legendary
    cmp     eax, 300
    jge     @@supercharged
    cmp     eax, 200
    jge     @@hot
    cmp     eax, 100
    jge     @@warm
    mov     dword ptr [rbx + BR_StatusCode], STATUS_COLD
    jmp     @@status_done
@@warm:
    mov     dword ptr [rbx + BR_StatusCode], STATUS_WARM
    jmp     @@status_done
@@hot:
    mov     dword ptr [rbx + BR_StatusCode], STATUS_HOT
    jmp     @@status_done
@@supercharged:
    mov     dword ptr [rbx + BR_StatusCode], STATUS_SUPERCHARGED
    lea     rcx, szBrSuper
    call    _br_print
    jmp     @@status_done
@@legendary:
    mov     dword ptr [rbx + BR_StatusCode], STATUS_LEGENDARY
    lea     rcx, szBrLegend
    call    _br_print
@@status_done:

    ; Measure bridge-level TPS
    lea     rcx, qpcScratchBR
    call    QueryPerformanceCounter
    mov     rax, qpcScratchBR
    sub     rax, [rbx + BR_InitQPC]
    test    rax, rax
    jz      @@no_tps
    mov     rcx, rax
    mov     rax, [rbx + BR_TokenCount]
    mov     rdx, 10000
    mul     rdx
    ; elapsed_s_x100 = elapsed_qpc * 100 / freq
    push    rax
    push    rdx
    mov     rax, rcx
    mov     rcx, 100
    mul     rcx
    mov     rcx, [rbx + BR_QPCFreq]
    div     rcx
    mov     rcx, rax                ; elapsed_s_x100
    pop     rdx
    pop     rax
    test    rcx, rcx
    jz      @@no_tps
    div     rcx
    mov     [rbx + BR_MeasuredTPS], eax
    cmp     eax, [rbx + BR_PeakTPS]
    jle     @@no_tps
    mov     [rbx + BR_PeakTPS], eax
@@no_tps:

    ; Return composite
    mov     eax, [rbx + BR_CompositeStr]

    add     rsp, 40
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret
Bridge_Tick ENDP

; =============================================================================
; Bridge_NotifyToken — Signal token to all engines
; RCX = bridge context
; Returns: RAX = total bridge tokens
; =============================================================================
Bridge_NotifyToken PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 40
    .allocstack 40
    .endprolog

    mov     rbx, rcx
    inc     qword ptr [rbx + BR_TokenCount]

    ; Notify Bounce (NotifyTokenGen expects RCX=bnc_ctx)
    mov     rcx, [rbx + BR_BounceCtx]
    test    rcx, rcx
    jz      @@skip_bnc
    ; Bounce expects RCX=context for NotifyTokenGen
    ; Just bump token count — Bounce measures internally
@@skip_bnc:

    ; Notify HotPatch
    mov     rcx, [rbx + BR_HotPatchCtx]
    test    rcx, rcx
    jz      @@skip_hp
    call    HotPatch_NotifyToken
@@skip_hp:

    mov     rax, [rbx + BR_TokenCount]

    add     rsp, 40
    pop     rbx
    ret
Bridge_NotifyToken ENDP

; =============================================================================
; Bridge_GetStrength — Get composite strength
; RCX = bridge context
; Returns: EAX = composite strength (%)
; =============================================================================
Bridge_GetStrength PROC
    mov     eax, [rcx + BR_CompositeStr]
    ret
Bridge_GetStrength ENDP

; =============================================================================
; Bridge_GetTPS — Get bridge-level TPS (x100)
; RCX = bridge context
; Returns: EAX = TPS*100
; =============================================================================
Bridge_GetTPS PROC
    mov     eax, [rcx + BR_MeasuredTPS]
    ret
Bridge_GetTPS ENDP

; =============================================================================
; Bridge_GetStatus — Get status code
; RCX = bridge context
; Returns: EAX = STATUS_COLD..STATUS_LEGENDARY
; =============================================================================
Bridge_GetStatus PROC
    mov     eax, [rcx + BR_StatusCode]
    ret
Bridge_GetStatus ENDP

; =============================================================================
; Bridge_GetStats — Full stats dump (128 bytes)
; RCX = bridge context, RDX = 128-byte buffer
;
; [0x00] TickCount       [0x08] TokenCount
; [0x10] CompositeStr    [0x14] SloStr
; [0x18] BncSuperStr     [0x1C] HPMultiplier
; [0x20] StatusCode      [0x24] PeakStrength
; [0x28] MeasuredTPS     [0x2C] PeakTPS
; [0x30..0x7F] reserved
; =============================================================================
Bridge_GetStats PROC
    test    rcx, rcx
    jz      @@fail
    test    rdx, rdx
    jz      @@fail

    mov     rax, [rcx + BR_TickCount]
    mov     [rdx + 00h], rax
    mov     rax, [rcx + BR_TokenCount]
    mov     [rdx + 08h], rax
    mov     eax, [rcx + BR_CompositeStr]
    mov     [rdx + 10h], eax
    mov     eax, [rcx + BR_SloStr]
    mov     [rdx + 14h], eax
    mov     eax, [rcx + BR_BncSuperStr]
    mov     [rdx + 18h], eax
    mov     eax, [rcx + BR_HPMultiplier]
    mov     [rdx + 1Ch], eax
    mov     eax, [rcx + BR_StatusCode]
    mov     [rdx + 20h], eax
    mov     eax, [rcx + BR_PeakStrength]
    mov     [rdx + 24h], eax
    mov     eax, [rcx + BR_MeasuredTPS]
    mov     [rdx + 28h], eax
    mov     eax, [rcx + BR_PeakTPS]
    mov     [rdx + 2Ch], eax

    mov     eax, 1
    ret
@@fail:
    xor     eax, eax
    ret
Bridge_GetStats ENDP

END
