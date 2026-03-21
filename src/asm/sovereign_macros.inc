; =============================================================================
; sovereign_macros.inc — MASM (ml64) helpers for Sovereign-Link IAT/ILT layout
; =============================================================================
; Matches the fixed RVAs in tools/pe_emitter.asm (ImageBase 140000000h, 2 sections):
;   .text @ RVA 1000h   .rdata @ RVA 2000h (import dir + ILT + IAT + hint/name + DLLs)
;
; Two call styles:
;   1) Link with MSVC + import .lib: use extern __imp_* and SovereignCallImp
;   2) Manual PE / byte emitter: use SovereignEmitCallFF15 (disp32 = IAT_RVA - RIP_next)
;
; NASM + linker objects should keep using RawrXD_PE64_IAT_Fabricator_v224.asm
; (__imp_* globals); this file is for ml64 sources.
; =============================================================================

ifndef sovereign_macros_inc
sovereign_macros_inc equ 1

; --- Layout (must match tools/pe_emitter.asm) --------------------------------
SOVEREIGN_TEXT_RVA              equ 1000h
SOVEREIGN_RDATA_RVA            equ 2000h
SOVEREIGN_IMAGE_BASE_LOW       equ 40000000h   ; PE32+ lo dword of 0000000140000000h
SOVEREIGN_IMAGE_BASE_HIGH      equ 00000001h

; Import directory / IAT directory index [12] in pe_emitter optional header
SOVEREIGN_IMPORT_DIR_RVA       equ 2000h
SOVEREIGN_IAT_DIR_RVA        equ 20C8h       ; first IAT thunk (K32) — also dir size region

; --- IAT slot RVAs (IMAGE_THUNK_DATA64 qwords; loader patches these) ---------
; KERNEL32.dll — IAT block base RVA = 20C8h
SOVEREIGN_IAT_RVA_ExitProcess       equ 20C8h
SOVEREIGN_IAT_RVA_GetModuleHandleA  equ 20D0h
SOVEREIGN_IAT_RVA_LoadLibraryA      equ 20D8h
SOVEREIGN_IAT_RVA_GetProcAddress    equ 20E0h

; USER32.dll — IAT block base RVA = 20F0h
SOVEREIGN_IAT_RVA_MessageBoxA       equ 20F0h
SOVEREIGN_IAT_RVA_CreateWindowExA   equ 20F8h
SOVEREIGN_IAT_RVA_RegisterClassExA  equ 2100h
SOVEREIGN_IAT_RVA_DefWindowProcA    equ 2108h

; GDI32.dll — IAT block base RVA = 2118h
SOVEREIGN_IAT_RVA_CreateSolidBrush  equ 2118h
SOVEREIGN_IAT_RVA_CreatePen         equ 2120h
SOVEREIGN_IAT_RVA_SelectObject      equ 2128h
SOVEREIGN_IAT_RVA_DeleteObject      equ 2130h

; Example from pe_emitter: first FF 15 ends with RIP of next insn = 100Ch
; disp32 = 20C8h - 100Ch = 10BCh
SOVEREIGN_EXAMPLE_CALL_RIP_NEXT_RVA equ 100Ch
SOVEREIGN_EXAMPLE_EXIT_DISP32       equ (SOVEREIGN_IAT_RVA_ExitProcess - SOVEREIGN_EXAMPLE_CALL_RIP_NEXT_RVA)

; -----------------------------------------------------------------------------
; SovereignEmitCallFF15 disp32 — emit raw "call qword ptr [rip+disp32]"
; Use when building a buffer or matching pe_emitter byte-for-byte.
; -----------------------------------------------------------------------------
SovereignEmitCallFF15 MACRO disp32_
    db 0FFh, 015h
    dd disp32_
ENDM

; -----------------------------------------------------------------------------
; SovereignCallDispFromRip iat_rva, rip_next_rva
;   disp32 = iat_rva - rip_next_rva (RVAs only; no ImageBase term — cancels out)
; Only valid if this instruction's RIP-after-call equals rip_next_rva (e.g. ORG).
; -----------------------------------------------------------------------------
SovereignCallDispFromRip MACRO iat_rva_, rip_next_rva_
    SovereignEmitCallFF15 (iat_rva_ - rip_next_rva_)
ENDM

; -----------------------------------------------------------------------------
; Link-time: symbols from kernel32.lib / user32.lib / gdi32.lib or fabricator.
;   extern __imp_ExitProcess : qword
;   SovereignCallImp __imp_ExitProcess
; -----------------------------------------------------------------------------
SovereignCallImp MACRO imp_sym
    call qword ptr [imp_sym]
ENDM

; -----------------------------------------------------------------------------
; Optional aliases (same as MS __imp_ names — declare EXTERN in your .asm)
; -----------------------------------------------------------------------------
; extern __imp_ExitProcess       : qword
; extern __imp_GetModuleHandleA  : qword
; extern __imp_LoadLibraryA      : qword
; extern __imp_GetProcAddress    : qword
; extern __imp_MessageBoxA       : qword
; extern __imp_CreateWindowExA   : qword
; extern __imp_RegisterClassExA  : qword
; extern __imp_DefWindowProcA    : qword
; extern __imp_CreateSolidBrush  : qword
; extern __imp_CreatePen         : qword
; extern __imp_SelectObject      : qword
; extern __imp_DeleteObject      : qword

endif ; sovereign_macros_inc
