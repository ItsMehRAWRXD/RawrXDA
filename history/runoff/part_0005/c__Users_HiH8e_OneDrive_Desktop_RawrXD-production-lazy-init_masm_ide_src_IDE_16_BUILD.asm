; ============================================================================
; IDE_16_BUILD.asm - Build system subsystem wrapper
; ============================================================================
include IDE_INC.ASM

; External from build_system.asm
; EXTERN BuildSystem_Init:PROC
; EXTERN BuildSystem_Compile:PROC

PUBLIC IDEBuild_Initialize
PUBLIC IDEBuild_Execute

.code

IDEBuild_Initialize PROC
    mov eax, 1
    LOG CSTR("IDEBuild_Initialize"), eax
    ret
IDEBuild_Initialize ENDP

IDEBuild_Execute PROC pTarget:DWORD
    mov eax, 1
    LOG CSTR("IDEBuild_Execute"), eax
    ret
IDEBuild_Execute ENDP

END
