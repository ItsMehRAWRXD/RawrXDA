; ============================================================================
; IDE_11_SETTINGS.asm - IDE settings theme application
; ============================================================================
include IDE_INC.ASM

; External from ideSettings.asm
EXTERN IDESettings_ApplyTheme_Impl:PROC

PUBLIC IDESettings_ApplyTheme

.code

IDESettings_ApplyTheme PROC hWnd:DWORD
    push hWnd
    call IDESettings_ApplyTheme_Impl
    ret
IDESettings_ApplyTheme ENDP

END
