;=============================================================================
; sovereign_macros.inc — MASM helpers for RawrXD PE64 manual-import binaries
;
; Pairs with tools/pe_emitter.asm layout (.rdata @ RVA 2000h):
;   ILT_K32 @ 2050h   IAT_K32 @ 20C8h  (KERNEL32 — 4 slots + null)
;   ILT_U32 @ 2078h   IAT_U32 @ 20F0h  (USER32   — 4 slots + null)
;   ILT_G32 @ 20A0h   IAT_G32 @ 2118h  (GDI32    — 4 slots + null)
;
; Each IAT slot is 8 bytes. Thunk RVAs below are the *slot* addresses (the
; QWORD the loader overwrites with the resolved VA).
;
; RIP-relative import call encoding:
;   FF 15 disp32   ; call qword ptr [rip+disp32]
;   disp32 = IAT_slot_RVA - (call_site_RVA + 6)
;   (+6 = length of FF 15 imm32)
;
; Build (example):
;   ml64 /c /nologo your.asm
;   link ... kernel32.lib user32.lib gdi32.lib
;   (Or emit bytes manually in a fabricator — no import lib required if loader
;    fills IAT from the PE import directory.)
;=============================================================================

IFNDEF SOVEREIGN_MACROS_INC
SOVEREIGN_MACROS_INC EQU 1

; --- Import directory anchors (RVA) ---
IFNDEF Sovereign_RVA_IDT
Sovereign_RVA_IDT       EQU 2000h
ENDIF
IFNDEF Sovereign_RVA_ILT_K32
Sovereign_RVA_ILT_K32   EQU 2050h
ENDIF
IFNDEF Sovereign_RVA_IAT_K32
Sovereign_RVA_IAT_K32   EQU 20C8h
ENDIF
IFNDEF Sovereign_RVA_ILT_U32
Sovereign_RVA_ILT_U32   EQU 2078h
ENDIF
IFNDEF Sovereign_RVA_IAT_U32
Sovereign_RVA_IAT_U32   EQU 20F0h
ENDIF
IFNDEF Sovereign_RVA_ILT_G32
Sovereign_RVA_ILT_G32   EQU 20A0h
ENDIF
IFNDEF Sovereign_RVA_IAT_G32
Sovereign_RVA_IAT_G32   EQU 2118h
ENDIF

; --- KERNEL32 IAT slots (RVA) ---
IFNDEF Sovereign_RVA_ExitProcess
Sovereign_RVA_ExitProcess      EQU 20C8h
ENDIF
IFNDEF Sovereign_RVA_GetModuleHandleA
Sovereign_RVA_GetModuleHandleA EQU 20D0h
ENDIF
IFNDEF Sovereign_RVA_LoadLibraryA
Sovereign_RVA_LoadLibraryA   EQU 20D8h
ENDIF
IFNDEF Sovereign_RVA_GetProcAddress
Sovereign_RVA_GetProcAddress EQU 20E0h
ENDIF

; --- USER32 IAT slots (RVA) ---
IFNDEF Sovereign_RVA_MessageBoxA
Sovereign_RVA_MessageBoxA      EQU 20F0h
ENDIF
IFNDEF Sovereign_RVA_CreateWindowExA
Sovereign_RVA_CreateWindowExA  EQU 20F8h
ENDIF
IFNDEF Sovereign_RVA_RegisterClassExA
Sovereign_RVA_RegisterClassExA EQU 2100h
ENDIF
IFNDEF Sovereign_RVA_DefWindowProcA
Sovereign_RVA_DefWindowProcA   EQU 2108h
ENDIF

; --- GDI32 IAT slots (RVA) ---
IFNDEF Sovereign_RVA_CreateSolidBrush
Sovereign_RVA_CreateSolidBrush EQU 2118h
ENDIF
IFNDEF Sovereign_RVA_CreatePen
Sovereign_RVA_CreatePen        EQU 2120h
ENDIF
IFNDEF Sovereign_RVA_SelectObject
Sovereign_RVA_SelectObject     EQU 2128h
ENDIF
IFNDEF Sovereign_RVA_DeleteObject
Sovereign_RVA_DeleteObject     EQU 2130h
ENDIF

; --- Legacy short names (optional aliases) ---
IFNDEF Sovereign_ExitProcess
Sovereign_ExitProcess          EQU Sovereign_RVA_ExitProcess
ENDIF
IFNDEF Sovereign_GetModuleHandleA
Sovereign_GetModuleHandleA     EQU Sovereign_RVA_GetModuleHandleA
ENDIF
IFNDEF Sovereign_LoadLibraryA
Sovereign_LoadLibraryA         EQU Sovereign_RVA_LoadLibraryA
ENDIF
IFNDEF Sovereign_GetProcAddress
Sovereign_GetProcAddress       EQU Sovereign_RVA_GetProcAddress
ENDIF
IFNDEF Sovereign_MessageBoxA
Sovereign_MessageBoxA          EQU Sovereign_RVA_MessageBoxA
ENDIF
IFNDEF Sovereign_CreateWindowExA
Sovereign_CreateWindowExA      EQU Sovereign_RVA_CreateWindowExA
ENDIF
IFNDEF Sovereign_RegisterClassExA
Sovereign_RegisterClassExA     EQU Sovereign_RVA_RegisterClassExA
ENDIF
IFNDEF Sovereign_DefWindowProcA
Sovereign_DefWindowProcA       EQU Sovereign_RVA_DefWindowProcA
ENDIF
IFNDEF Sovereign_CreateSolidBrush
Sovereign_CreateSolidBrush     EQU Sovereign_RVA_CreateSolidBrush
ENDIF
IFNDEF Sovereign_CreatePen
Sovereign_CreatePen            EQU Sovereign_RVA_CreatePen
ENDIF
IFNDEF Sovereign_SelectObject
Sovereign_SelectObject         EQU Sovereign_RVA_SelectObject
ENDIF
IFNDEF Sovereign_DeleteObject
Sovereign_DeleteObject         EQU Sovereign_RVA_DeleteObject
ENDIF

;-----------------------------------------------------------------------------
; SovereignCallRip — emit  call qword ptr [rip+disp32]  to an IAT slot
;   iat_slot_rva  = RVA of the IAT QWORD (see Sovereign_RVA_* above)
;   call_site_rva = RVA of this macro's first emitted byte (must be the FF)
;
; Example: entry at 1000h; first insn 4 bytes; xor 2 bytes; call at 1006h:
;   SovereignCallRip Sovereign_RVA_ExitProcess, 1006h
;-----------------------------------------------------------------------------
SovereignCallRip MACRO iat_slot_rva:REQ, call_site_rva:REQ
    LOCAL next_rip
    next_rip EQU (call_site_rva) + 6
    db 0FFh, 15h
    dd (iat_slot_rva) - next_rip
ENDM

;-----------------------------------------------------------------------------
; SovereignCallRip_Next — same as SovereignCallRip but pass next-instruction RVA
;   (address immediately after the 6-byte call). Equivalently: call_site+6.
;-----------------------------------------------------------------------------
SovereignCallRip_Next MACRO iat_slot_rva:REQ, next_instruction_rva:REQ
    db 0FFh, 15h
    dd (iat_slot_rva) - (next_instruction_rva)
ENDM

;=============================================================================
; Minimal invoke templates — shadow space + RIP-relative IAT call
;
; Windows x64 ABI: caller provides 32 bytes of "shadow space" (home space)
; above RSP for the callee; many APIs also expect RSP 16-byte aligned *before*
; CALL. The common leaf pattern is:
;   sub rsp, 28h    ; 32 shadow + 8 bytes to realign (entry RSP often 8 mod 16)
;   ... set RCX, RDX, R8, R9 (and stack args for 5+) yourself ...
;   call [IAT]
;   add rsp, 28h
;
; These macros do **not** move arguments into registers — you do that before
; `SovereignInvokeCallRip` / `SovereignInvoke_0` … `_4`. The `_N` names only
; document how many register arguments you intend (0–4 in RCX..R9).
;
; `block_start_rva` must be the **RVA of the first byte** of this block (the
; `sub rsp, 28h`). The embedded `SovereignCallRip` then uses call-site
; RVA = block_start_rva + 4 (immediately after the 4-byte sub).
;=============================================================================

IFNDEF SovereignInvoke_ShadowBytes
SovereignInvoke_ShadowBytes EQU 28h
ENDIF

; --- Frame only (set registers yourself, then SovereignCallRip) -------------
SovereignInvokeFrameOpen MACRO
    sub rsp, SovereignInvoke_ShadowBytes
ENDM

SovereignInvokeFrameClose MACRO
    add rsp, SovereignInvoke_ShadowBytes
ENDM

; --- sub rsp / FF 15 IAT / add rsp  (pass IAT slot + block base RVA) --------
SovereignInvokeCallRip MACRO iat_slot_rva:REQ, block_start_rva:REQ
    SovereignInvokeFrameOpen
    SovereignCallRip iat_slot_rva, (block_start_rva + 4)
    SovereignInvokeFrameClose
ENDM

; Semantic aliases — same expansion; you still load argument registers.
SovereignInvoke_0 MACRO iat_slot_rva:REQ, block_start_rva:REQ
    SovereignInvokeCallRip iat_slot_rva, block_start_rva
ENDM

SovereignInvoke_1 MACRO iat_slot_rva:REQ, block_start_rva:REQ
    SovereignInvokeCallRip iat_slot_rva, block_start_rva
ENDM

SovereignInvoke_2 MACRO iat_slot_rva:REQ, block_start_rva:REQ
    SovereignInvokeCallRip iat_slot_rva, block_start_rva
ENDM

SovereignInvoke_3 MACRO iat_slot_rva:REQ, block_start_rva:REQ
    SovereignInvokeCallRip iat_slot_rva, block_start_rva
ENDM

SovereignInvoke_4 MACRO iat_slot_rva:REQ, block_start_rva:REQ
    SovereignInvokeCallRip iat_slot_rva, block_start_rva
ENDM

; If you need mov/immediate loads *between* frame open and the call, do not use
; SovereignInvokeCallRip — use SovereignInvokeFrameOpen, your instructions, then
; SovereignCallRip iat_slot_rva, <RVA of FF15> and SovereignInvokeFrameClose.

ENDIF ; SOVEREIGN_MACROS_INC
