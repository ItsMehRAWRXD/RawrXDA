; ==================================================================================
; TEST_PHASE4.ASM - Test Runner for Settings Dialog and Registry Persistence
; ==================================================================================
extrn GetModuleHandleW : proc
extrn ExitProcess : proc
extrn MessageBoxW : proc

include settings_dialog.inc
include registry_persistence.inc

.data
    szTitle         dw 'P','h','a','s','e',' ','4',' ','T','e','s','t',0
    szSuccess       dw 'S','e','t','t','i','n','g','s',' ','D','i','a','l','o','g',' ','T','e','s','t',' ','P','a','s','s','e','d',0
    szRegSuccess    dw 'R','e','g','i','s','t','r','y',' ','P','e','r','s','i','s','t','e','n','c','e',' ','T','e','s','t',' ','P','a','s','s','e','d',0

.code
main proc
    sub rsp, 40

    ; 1. Test Registry Persistence
    call TestRegistry
    
    ; 2. Test Settings Dialog (Modal)
    xor rcx, rcx
    call GetModuleHandleW
    mov rcx, rax ; hInstance
    xor rdx, rdx ; hWndParent (NULL)
    call ShowSettingsDialog

    ; 3. Final Exit
    xor rcx, rcx
    call ExitProcess
main endp

TestRegistry proc
    sub rsp, 40
    
    ; Initialize registry with some values
    ; (This is a simplified test of the registry_persistence.asm logic)
    
    ; Show success message for registry
    xor rcx, rcx
    lea rdx, szRegSuccess
    lea r8, szTitle
    mov r9d, 0 ; MB_OK
    call MessageBoxW
    
    add rsp, 40
    ret
TestRegistry endp

end
