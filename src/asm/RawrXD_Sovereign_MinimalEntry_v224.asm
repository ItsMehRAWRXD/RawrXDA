; =============================================================================
; Sovereign-Link v22.4.0 — Minimal entry (NASM) for IAT smoke test
; =============================================================================
; Links with RawrXD_PE64_IAT_Fabricator_v224.asm only — no CRT, no import .lib.
; Entry: main (use /ENTRY:main). Subsystem: Windows (MessageBox).
; =============================================================================

        default rel
        bits 64

        extern  __imp_MessageBoxA
        extern  __imp_ExitProcess

section .rdata align=8
msg_text:
        db      'Sovereign-Link v22.4.0-IAT', 0
msg_title:
        db      'RawrXD', 0

section .text
        global  main

main:
        sub     rsp, 40
        xor     ecx, ecx
        lea     rdx, [msg_text]
        lea     r8, [msg_title]
        xor     r9d, r9d
        call    qword [__imp_MessageBoxA]
        xor     ecx, ecx
        call    qword [__imp_ExitProcess]
