; =============================================================================
; MASM_STACK_ALIGNMENT_AUDIT.inc — Reference Guide & Validation Rules
; =============================================================================
; Include this file in any new RawrXD MASM module to get compile-time
; alignment verification macros and canonical prolog/epilog templates.
;
; Microsoft x64 ABI Requirements:
;   1. RSP must be 16-byte aligned BEFORE every CALL instruction
;   2. At function entry, RSP = 16n+8 (because CALL pushed 8-byte ret addr)
;   3. 32-byte shadow space (home area) must be reserved above every CALL
;   4. Non-volatile registers: RBX, RBP, RDI, RSI, R12-R15, XMM6-XMM15
;   5. .pushreg MUST immediately follow each PUSH in the prolog
;   6. .allocstack MUST immediately follow the SUB RSP,imm
;   7. .endprolog MUST come before any non-prolog instruction
;
; Stack math cheat sheet:
;   pushes = N non-volatiles saved (each 8 bytes)
;   alloc  = SUB RSP, X
;   Total frame = N*8 + X (must be computed from entry-RSP = 16n+8)
;   After prolog, RSP = 16n+8 - N*8 - X
;   For 16-byte alignment at CALL: (8 + N*8 + X) mod 16 == 0
;
; Quick table (N pushes → minimum SUB RSP,X for alignment):
;   N=0 → X=28h (40)    Total=48  mod16=0  ✓
;   N=1 → X=20h (32)    Total=48  mod16=0  ✓
;   N=2 → X=28h (40)    Total=56  mod16=0  ✗ → 48 mod16=0 → X=20h(32)=48 ✓
;         Actually: 8(ret)+16(2 push)+X → (24+X) mod16==0 → X=28h=40→64 mod16=0 ✓
;   N=3 → X=20h (32)    8+24+32=64 mod16=0  ✓
;   N=4 → X=28h (40)    8+32+40=80 mod16=0  ✓
;   N=5 → X=30h (48)    8+40+48=96 mod16=0  ✓
;   N=6 → X=28h (40)    8+48+40=96 mod16=0  ✓
;   N=7 → X=20h (32)    8+56+32=96 mod16=0  ✓
;
; Summary: If N is even, X = 28h. If N is odd, X = 20h. (minimum)
;          Always ensure X >= 20h for shadow space.
;
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
; =============================================================================

; =============================================================================
; Canonical 5-push prolog (most common pattern in RawrXD)
; =============================================================================
; MyFunction PROC FRAME
;     push    rbx
;     .pushreg rbx
;     push    rsi
;     .pushreg rsi
;     push    rdi
;     .pushreg rdi
;     push    r12
;     .pushreg r12
;     push    r13
;     .pushreg r13
;     sub     rsp, 30h        ; 48 bytes (shadow + 16 for locals)
;     .allocstack 30h
;     .endprolog
;     ; ... body ...
;     add     rsp, 30h
;     pop     r13
;     pop     r12
;     pop     rdi
;     pop     rsi
;     pop     rbx
;     ret
; MyFunction ENDP

; =============================================================================
; Stack alignment verification constant
; =============================================================================
SHADOW_SPACE    EQU 20h     ; 32 bytes — minimum reserved above CALL target
