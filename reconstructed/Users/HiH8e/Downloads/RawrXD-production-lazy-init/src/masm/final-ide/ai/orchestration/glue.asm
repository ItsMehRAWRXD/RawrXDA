; ai_orchestration_glue.asm - Bridge functions for Qt MainWindow
; Provides C-callable entry points used by MainWindow.cpp
; Wires into MASM orchestration coordinator and stores UI handles.

option casemap:none

.code

; External MASM subsystems
EXTERN ai_orchestration_coordinator_init:PROC
EXTERN autonomous_task_schedule:PROC
EXTERN output_pane_init:PROC

; Global handles shared across MASM modules
PUBLIC outputLogHandle
PUBLIC agenticChatHandle

.data?
outputLogHandle    QWORD ?
agenticChatHandle  QWORD ?

; void ai_orchestration_install(HWND hWindow)
PUBLIC ai_orchestration_install
ai_orchestration_install PROC
    ; rcx = hWindow
    ; initialize coordinator with main window handle
    call ai_orchestration_coordinator_init
    ret
ai_orchestration_install ENDP

; void ai_orchestration_poll()
PUBLIC ai_orchestration_poll
ai_orchestration_poll PROC
    ; placeholder: no-op poll
    ret
ai_orchestration_poll ENDP

; void ai_orchestration_shutdown()
PUBLIC ai_orchestration_shutdown
ai_orchestration_shutdown PROC
    ; placeholder: no-op shutdown
    ret
ai_orchestration_shutdown ENDP

; void ai_orchestration_set_handles(HWND hOutput, HWND hChat)
PUBLIC ai_orchestration_set_handles
ai_orchestration_set_handles PROC
    ; rcx = hOutput, rdx = hChat
    mov [outputLogHandle], rcx
    mov [agenticChatHandle], rdx
    ; initialize output pane logger with output handle
    mov rcx, [outputLogHandle]
    call output_pane_init
    ret
ai_orchestration_set_handles ENDP

; void ai_orchestration_schedule_task(const char* goal, int priority, bool autoRetry)
PUBLIC ai_orchestration_schedule_task
ai_orchestration_schedule_task PROC
    ; rcx = goal, edx = priority, r8b = autoRetry
    ; forward to autonomous task scheduler
    call autonomous_task_schedule
    ret
ai_orchestration_schedule_task ENDP

END
