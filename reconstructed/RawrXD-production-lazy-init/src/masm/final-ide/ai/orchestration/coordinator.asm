; ai_orchestration_coordinator.asm - Clean, working MASM coordinator
; Provides C-callable entry points for orchestration
; NOTE: Symbols renamed with _core suffix to avoid conflicts with ai_orchestration_glue.asm

option casemap:none

.code

; External Win32 functions
EXTERN GetTickCount:PROC
EXTERN Sleep:PROC
EXTERN CreateEventA:PROC
EXTERN SetEvent:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN SetTimer:PROC

; External functions from other modules
EXTERN autonomous_task_schedule:PROC
EXTERN console_log:PROC

.data?
coordinatorInitialized  DWORD ?
hCoordinatorTimer       QWORD ?

.data
szCoordinatorInit       BYTE "[coordinator] Initialized successfully", 13, 10, 0
szCoordinatorPoll       BYTE "[coordinator] Poll tick", 13, 10, 0
szCoordinatorInstall    BYTE "[coordinator] Installed in main loop", 13, 10, 0

.code

; ai_orchestration_coordinator_init() - Initialize coordinator (CORE implementation)
PUBLIC ai_orchestration_coordinator_init
ai_orchestration_coordinator_init PROC
    
    ; Set initialized flag
    mov DWORD PTR [coordinatorInitialized], 1
    
    ; Log initialization
    lea rcx, szCoordinatorInit
    call console_log
    
    mov eax, 1                    ; Return success
    ret
ai_orchestration_coordinator_init ENDP

; ai_orchestration_poll_core() - Poll for updates (renamed to avoid conflict)
PUBLIC ai_orchestration_poll_core
ai_orchestration_poll_core PROC
    
    ; Check if initialized
    cmp DWORD PTR [coordinatorInitialized], 0
    je poll_done
    
    ; Log poll tick (optional - can be removed for production)
    lea rcx, szCoordinatorPoll
    call console_log
    
poll_done:
    mov eax, 1
    ret
ai_orchestration_poll_core ENDP

; ai_orchestration_install_core() - Install in main window (renamed to avoid conflict)
PUBLIC ai_orchestration_install_core
ai_orchestration_install_core PROC
    
    ; rcx = main window handle
    push rbx
    mov rbx, rcx                  ; Save window handle
    
    ; Initialize coordinator
    call ai_orchestration_coordinator_init
    
    ; Install timer (every 100ms)
    mov rcx, rbx                  ; window handle
    mov edx, 1001                 ; timer ID
    mov r8d, 100                  ; 100ms interval
    xor r9, r9                    ; no callback
    call SetTimer
    mov [hCoordinatorTimer], rax
    
    ; Log installation
    lea rcx, szCoordinatorInstall
    call console_log
    
    mov eax, 1
    pop rbx
    ret
ai_orchestration_install_core ENDP

; ai_orchestration_set_handles_core() - Set UI handles (renamed to avoid conflict)
PUBLIC ai_orchestration_set_handles_core
ai_orchestration_set_handles_core PROC
    ; rcx = output handle, rdx = chat handle
    ; Store handles if needed (currently no-op)
    mov eax, 1
    ret
ai_orchestration_set_handles_core ENDP

; ai_orchestration_schedule_task_core() - Schedule autonomous task (renamed to avoid conflict)
PUBLIC ai_orchestration_schedule_task_core
ai_orchestration_schedule_task_core PROC
    ; rcx = goal string, edx = priority, r8b = auto-retry
    ; Forward to autonomous task scheduler
    call autonomous_task_schedule
    ret
ai_orchestration_schedule_task_core ENDP

END

