; ============================================================================
; dynapi_x86.inc - Prototypes for MASM-only dynamic API resolver (x86)
; Link with dynapi_x86.obj (src/dynapi_x86.asm)
; ============================================================================

DYN_FindModuleBaseA PROTO :DWORD
DYN_GetProcAddressA PROTO :DWORD,:DWORD

; Convenience macros
DYN_BIND MACRO pOut:req, hMod:req, pName:req
    invoke DYN_GetProcAddressA, hMod, pName
    mov pOut, eax
ENDM
