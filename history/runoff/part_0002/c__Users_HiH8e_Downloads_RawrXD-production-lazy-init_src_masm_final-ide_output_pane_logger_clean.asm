; output_pane_logger_clean.asm - Simple, working output pane logger
; Minimal implementation to support orchestration without complex dependencies

option casemap:none

.code

; External functions we need
EXTERN console_log:PROC

; Simple output pane initialization
PUBLIC output_pane_init
output_pane_init PROC

    ; rcx = hOutput handle (ignored for now)
    ; Simple output pane initialization
    
    ; Log initialization
    lea rcx, szOutputPaneInit
    call console_log
    
    mov eax, 1                    ; Return success
    ret
output_pane_init ENDP

.data
szOutputPaneInit    BYTE "[output] Output pane initialized", 13, 10, 0

.code

END
