; ============================================================================
; IDE_12_PROJECT.asm - Project management subsystem wrapper
; ============================================================================
include IDE_INC.ASM

; External from project_manager.asm (assuming it has these)
; Note: project_manager.asm might need to be updated to export these
EXTERN ProjectManager_Init:PROC
EXTERN ProjectManager_LoadProject:PROC

PUBLIC IDEProject_Initialize
PUBLIC IDEProject_Load

.code

IDEProject_Initialize PROC
    ; call ProjectManager_Init
    mov eax, 1
    LOG CSTR("IDEProject_Initialize"), eax
    ret
IDEProject_Initialize ENDP

IDEProject_Load PROC pPath:DWORD
    ; push pPath
    ; call ProjectManager_LoadProject
    mov eax, 1
    LOG CSTR("IDEProject_Load"), eax
    ret
IDEProject_Load ENDP

END
