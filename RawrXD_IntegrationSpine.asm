; RawrXD_IntegrationSpine.asm
; Phase 2: High-level integration spine
; Wires Win32 shell to MASM engines via controlled boundary

option casemap:none
include rawr_rt.inc
include rawr_imports.inc
include rawr_api.inc

.data
spine_initialized    dq 0
dispatcher_ctx       dq 0
agent_ctx            dq 0

.code

;-----------------------------------------------------------------------------
; RawrSpine_Init
; Initializes TaskDispatcher and NativeAgent
;-----------------------------------------------------------------------------
PUBLIC RawrSpine_Init
RawrSpine_Init PROC
    RAWR_PROLOGUE 0

    ; Check if already initialized
    mov rax, spine_initialized
    test rax, rax
    jnz _already_init

    ; Init TaskDispatcher with 4 workers
    mov rcx, 4
    call TaskDispatcher_Initialize
    test eax, eax
    jz _fail
    mov dispatcher_ctx, rax

    ; Init NativeAgent (stub for now)
    ; call NativeAgent_Initialize
    ; mov agent_ctx, rax

    mov spine_initialized, 1
    mov eax, 1
    RAWR_EPILOGUE 0

_already_init:
    mov eax, 1
    RAWR_EPILOGUE 0

_fail:
    xor eax, eax
    RAWR_EPILOGUE 0
RawrSpine_Init ENDP

;-----------------------------------------------------------------------------
; RawrSpine_SubmitJob
; Submits a job to the dispatcher
; RCX = job_type (0=codex, 1=infer, etc.)
; RDX = payload_ptr
; R8 = payload_sz
; Returns task_id in RAX, 0 on fail
;-----------------------------------------------------------------------------
PUBLIC RawrSpine_SubmitJob
RawrSpine_SubmitJob PROC
    RAWR_PROLOGUE 0

    mov r9, rsp
    add r9, 20h + 8  ; out_task_id on stack
    call TaskDispatcher_SubmitTask
    ; Assume it returns task_id in rax

    RAWR_EPILOGUE 0
RawrSpine_SubmitJob ENDP

;-----------------------------------------------------------------------------
; RawrSpine_PollJob
; Polls for job completion
; RCX = task_id
; RDX = out_status*
; R8 = out_ptr*
; R9 = out_sz*
; Returns 1 if complete, 0 pending
;-----------------------------------------------------------------------------
PUBLIC RawrSpine_PollJob
RawrSpine_PollJob PROC
    RAWR_PROLOGUE 0

    ; Stub: assume complete
    mov dword ptr [rdx], 2  ; Done
    mov qword ptr [r8], 0
    mov dword ptr [r9], 0
    mov eax, 1

    RAWR_EPILOGUE 0
RawrSpine_PollJob ENDP

;-----------------------------------------------------------------------------
; RawrSpine_Shutdown
; Shuts down the spine
;-----------------------------------------------------------------------------
PUBLIC RawrSpine_Shutdown
RawrSpine_Shutdown PROC
    RAWR_PROLOGUE 0

    call TaskDispatcher_Shutdown
    mov spine_initialized, 0

    RAWR_EPILOGUE 0
RawrSpine_Shutdown ENDP

END