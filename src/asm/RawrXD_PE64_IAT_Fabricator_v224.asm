; =============================================================================
; RawrXD PE64 IAT Fabricator — Sovereign-Link reference v22.4.0
; =============================================================================
; Manual IMAGE_IMPORT_DESCRIPTOR + ILT + IAT + hint/name table for:
;   KERNEL32.dll, USER32.dll, GDI32.dll
;
; Build (NASM Win64 COFF → link with MSVC link.exe or lld-link):
;   nasm -f win64 -o RawrXD_PE64_IAT_Fabricator_v224.obj ^
;        src/asm/RawrXD_PE64_IAT_Fabricator_v224.asm
;   link /nologo /subsystem:windows /entry:main ^
;        your_entry.obj RawrXD_PE64_IAT_Fabricator_v224.obj kernel32.lib user32.lib gdi32.lib
;
; From code (DEFAULT REL recommended):
;   extern __imp_ExitProcess:qword
;   call [__imp_ExitProcess]
;
; Notes:
;   - All IDT / thunk slots use "wrt ..imagebase" so the linker emits correct RVAs.
;   - Hints are 0 (portable). The sample ordinal-like hints in informal writeups are
;     not guaranteed across Windows builds; the loader matches by name.
;   - ILT and IAT are pre-bound to the same hint/name RVAs; the loader overwrites IAT.
; =============================================================================

        default rel
        bits 64

; --- IMAGE_IMPORT_DESCRIPTOR-sized blocks (20 bytes each) --------------------
; UINT_PTR OriginalFirstThunk; UINT TimeDateStamp; UINT ForwarderChain;
; UINT Name; UINT FirstThunk;

section .idata align=16

        ; Descriptor 0 — KERNEL32
        dd      ilt_k32 wrt ..imagebase
        dd      0
        dd      0
        dd      name_k32 wrt ..imagebase
        dd      iat_k32 wrt ..imagebase

        ; Descriptor 1 — USER32
        dd      ilt_u32 wrt ..imagebase
        dd      0
        dd      0
        dd      name_u32 wrt ..imagebase
        dd      iat_u32 wrt ..imagebase

        ; Descriptor 2 — GDI32
        dd      ilt_g32 wrt ..imagebase
        dd      0
        dd      0
        dd      name_g32 wrt ..imagebase
        dd      iat_g32 wrt ..imagebase

        ; Null terminator (5 dwords = 20 bytes)
        dd      0, 0, 0, 0, 0

        align   8, db 0

; --- Import Lookup Tables (ILT) — PE32+ IMAGE_THUNK_DATA64 (QWORD = RVA, hi=0) -
%macro THUNK64 1
        dd      %1 wrt ..imagebase
        dd      0
%endmacro

ilt_k32:
        THUNK64 hn_ExitProcess
        THUNK64 hn_GetModuleHandleA
        THUNK64 hn_LoadLibraryA
        THUNK64 hn_GetProcAddress
        dq      0

ilt_u32:
        THUNK64 hn_MessageBoxA
        THUNK64 hn_CreateWindowExA
        THUNK64 hn_RegisterClassExA
        THUNK64 hn_DefWindowProcA
        dq      0

ilt_g32:
        THUNK64 hn_CreateSolidBrush
        THUNK64 hn_CreatePen
        THUNK64 hn_SelectObject
        THUNK64 hn_DeleteObject
        dq      0

        align   8, db 0

; --- Import Address Tables (IAT) — mirror ILT until load-time patch -----------
iat_k32:
global  __imp_ExitProcess
__imp_ExitProcess:
        THUNK64 hn_ExitProcess
global  __imp_GetModuleHandleA
__imp_GetModuleHandleA:
        THUNK64 hn_GetModuleHandleA
global  __imp_LoadLibraryA
__imp_LoadLibraryA:
        THUNK64 hn_LoadLibraryA
global  __imp_GetProcAddress
__imp_GetProcAddress:
        THUNK64 hn_GetProcAddress
        dq      0

iat_u32:
global  __imp_MessageBoxA
__imp_MessageBoxA:
        THUNK64 hn_MessageBoxA
global  __imp_CreateWindowExA
__imp_CreateWindowExA:
        THUNK64 hn_CreateWindowExA
global  __imp_RegisterClassExA
__imp_RegisterClassExA:
        THUNK64 hn_RegisterClassExA
global  __imp_DefWindowProcA
__imp_DefWindowProcA:
        THUNK64 hn_DefWindowProcA
        dq      0

iat_g32:
global  __imp_CreateSolidBrush
__imp_CreateSolidBrush:
        THUNK64 hn_CreateSolidBrush
global  __imp_CreatePen
__imp_CreatePen:
        THUNK64 hn_CreatePen
global  __imp_SelectObject
__imp_SelectObject:
        THUNK64 hn_SelectObject
global  __imp_DeleteObject
__imp_DeleteObject:
        THUNK64 hn_DeleteObject
        dq      0

; --- Hint/Name table entries (WORD hint + ASCIIZ) ----------------------------
        align   2, db 0

hn_ExitProcess:
        dw      0
        db      'ExitProcess', 0

hn_GetModuleHandleA:
        dw      0
        db      'GetModuleHandleA', 0

hn_LoadLibraryA:
        dw      0
        db      'LoadLibraryA', 0

hn_GetProcAddress:
        dw      0
        db      'GetProcAddress', 0

hn_MessageBoxA:
        dw      0
        db      'MessageBoxA', 0

hn_CreateWindowExA:
        dw      0
        db      'CreateWindowExA', 0

hn_RegisterClassExA:
        dw      0
        db      'RegisterClassExA', 0

hn_DefWindowProcA:
        dw      0
        db      'DefWindowProcA', 0

hn_CreateSolidBrush:
        dw      0
        db      'CreateSolidBrush', 0

hn_CreatePen:
        dw      0
        db      'CreatePen', 0

hn_SelectObject:
        dw      0
        db      'SelectObject', 0

hn_DeleteObject:
        dw      0
        db      'DeleteObject', 0

; --- DLL names (PE import name strings) --------------------------------------
name_k32:
        db      'KERNEL32.dll', 0

name_u32:
        db      'USER32.dll', 0

name_g32:
        db      'GDI32.dll', 0
