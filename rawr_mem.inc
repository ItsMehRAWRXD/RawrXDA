; =========================
; FILE: rawr_mem.inc  (PURE MASM64)
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

; g_rawr_heap is defined in rawr_mem.asm

RAWR_ARENA struct
    base        qword ?   ; reserved base
    reserve_sz  qword ?
    commit_sz   qword ?   ; currently committed
    bump_offset qword ?   ; current bump
    pagesz      dword ?   ; system page size
    _pad        dword ?
RAWR_ARENA ends

; -------------------------
; Helpers
; -------------------------
; Align up: (x + (a-1)) & ~(a-1), where a is power of two
RAWR_ALIGN_UP macro regX:req, regA:req, tmp:req
    ; tmp = a-1
    mov     tmp, regA
    dec     tmp
    add     regX, tmp
    not     tmp
    and     regX, tmp
endm