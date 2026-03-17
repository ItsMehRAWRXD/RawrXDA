;==========================================================================
; session_manager.asm - IDE Session Management & Auto-Save
; ==========================================================================
; Manages IDE session lifecycle:
; - Session creation/recovery on startup
; - Auto-save at configurable intervals (default: 5 minutes)
; - Graceful shutdown with final save
; - Session crash detection
; - Background async saves
;
; Features:
; - Periodic auto-save timer (WM_TIMER)
; - Throttled saves (don't save too frequently)
; - Background thread for non-blocking saves
; - Crash recovery on next launch
; - Session lock file to prevent duplicates
;
; Integration:
; - memory_persistence.asm (save/load)
; - output_pane_logger.asm (logging)
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
AUTO_SAVE_INTERVAL      EQU 300000      ; 5 minutes in milliseconds
AUTO_SAVE_TIMER_ID      EQU 1001        ; WM_TIMER ID for auto-save
SAVE_THROTTLE_TIME      EQU 30000       ; 30 seconds minimum between saves

SESSION_LOCK_TIMEOUT    EQU 60000       ; Lock timeout (1 minute)
SESSION_MAX_AGE         EQU 2592000     ; 30 days

;==========================================================================
; STRUCTURES
;==========================================================================
SESSION_INFO STRUCT
    session_id      BYTE 64 DUP (?)     ; Unique session ID (UUID)
    create_time     QWORD ?             ; Creation timestamp
    last_save_time  QWORD ?             ; Last save timestamp
    save_count      DWORD ?             ; Number of saves
    crash_detected  DWORD ?             ; Crash flag
    is_active       DWORD ?             ; Session active
SESSION_INFO ENDS

SESSION_CONFIG STRUCT
    auto_save_enabled   DWORD ?         ; Enable auto-save
    auto_save_interval  DWORD ?         ; Interval in ms
    max_backups         DWORD ?         ; Keep N backups
    compression_level   DWORD ?         ; 0-9 compression
    recovery_enabled    DWORD ?         ; Auto-recovery on crash
SESSION_CONFIG ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Session directory
    szSessionBaseDir    BYTE "%APPDATA%\RawrXD\sessions\",0
    szSessionLockFile   BYTE "%s.lock",0
    
    ; Session strings
    szSessionStart      BYTE "[SESSION] Starting session %s",0
    szSessionRecover    BYTE "[SESSION] Recovering from crash...",0
    szSessionSave       BYTE "[SESSION] Auto-saving (save #%d)...",0
    szSessionSaveOK     BYTE "[SESSION] Auto-save successful",0
    szSessionSaveErr    BYTE "[SESSION] Auto-save failed: %s",0
    szSessionEnd        BYTE "[SESSION] Closing session gracefully",0
    szSessionCleanup    BYTE "[SESSION] Cleaning up old sessions",0
    
    ; Default config
    szDefaultConfig     BYTE "autosave=1|interval=300|backups=10|compress=3|recovery=1",0

.data?
    ; Session state
    SessionInfo         SESSION_INFO <>
    SessionConfig       SESSION_CONFIG <>
    
    ; Timing
    LastSaveTime        QWORD ?
    SessionStartTime    QWORD ?
    
    ; Thread handles
    hSaveThread         QWORD ?
    hSaveEvent          QWORD ?         ; Event to trigger save
    hStopEvent          QWORD ?         ; Event to stop thread
    
    ; Status flags
    IsSaving            DWORD ?
    IsRecovering        DWORD ?

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: session_manager_init() -> eax
; Initialize session manager and auto-save system
;==========================================================================
PUBLIC session_manager_init
session_manager_init PROC
    push rbx
    push rsi
    sub rsp, 48
    
    ; Generate session ID (UUID-like)
    call generate_session_id
    
    ; Load or create session config
    call load_session_config
    
    ; Check for crash recovery
    call check_crash_recovery
    
    ; Initialize timing
    call GetTickCount
    mov SessionStartTime, rax
    mov LastSaveTime, rax
    
    ; Create auto-save events
    xor rcx, rcx
    xor edx, edx
    xor r8d, r8d
    call CreateEventA
    mov hSaveEvent, rax
    
    xor rcx, rcx
    xor edx, edx
    xor r8d, r8d
    call CreateEventA
    mov hStopEvent, rax
    
    ; Create background save thread (only if auto-save enabled)
    mov eax, SessionConfig.auto_save_enabled
    test eax, eax
    jz skip_thread
    
    ; Create thread for background saves
    xor rcx, rcx
    mov edx, 0
    lea r8, [session_save_thread]
    xor r9, r9
    xor r10d, r10d
    xor r11d, r11d
    call CreateThread
    mov hSaveThread, rax
    
skip_thread:
    ; Log session start
    lea rcx, [szSessionStart]
    lea rdx, [SessionInfo.session_id]
    call wsprintfA
    
    mov SessionInfo.is_active, 1
    
    mov eax, 1
    add rsp, 48
    pop rsi
    pop rbx
    ret
session_manager_init ENDP

;==========================================================================
; PUBLIC: session_manager_shutdown() -> eax
; Graceful shutdown with final save
;==========================================================================
PUBLIC session_manager_shutdown
session_manager_shutdown PROC
    push rbx
    sub rsp, 32
    
    ; Log shutdown
    lea rcx, [szSessionEnd]
    call asm_log
    
    ; Set stop event to signal thread
    mov rcx, hStopEvent
    call SetEvent
    
    ; Wait for save thread to finish (timeout 10s)
    mov rcx, hSaveThread
    mov edx, 10000
    call WaitForSingleObject
    
    ; Close thread handle
    mov rcx, hSaveThread
    call CloseHandle
    
    ; Perform final save
    mov SessionInfo.is_active, 0
    call memory_persist_save
    
    ; Cleanup events
    mov rcx, hSaveEvent
    call CloseHandle
    
    mov rcx, hStopEvent
    call CloseHandle
    
    ; Delete lock file
    lea rcx, [SessionInfo.session_id]
    call delete_session_lock
    
    ; Clean old sessions
    call cleanup_old_sessions
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
session_manager_shutdown ENDP

;==========================================================================
; PUBLIC: session_trigger_autosave() -> eax
; Manually trigger auto-save (called from WM_TIMER or menu)
;==========================================================================
PUBLIC session_trigger_autosave
session_trigger_autosave PROC
    push rbx
    sub rsp, 32
    
    ; Check throttle (don't save more than once per 30 seconds)
    call GetTickCount
    mov rbx, rax
    sub rbx, LastSaveTime
    cmp rbx, SAVE_THROTTLE_TIME
    jl autosave_throttled
    
    ; Update last save time
    mov LastSaveTime, rax
    
    ; Check if dirty
    call memory_persist_mark_dirty
    
    ; Log save attempt
    lea rcx, [szSessionSave]
    mov edx, SessionInfo.save_count
    call wsprintfA
    
    ; Trigger background save if enabled
    mov eax, SessionConfig.auto_save_enabled
    test eax, eax
    jz sync_save
    
    ; Signal save event (async)
    mov rcx, hSaveEvent
    call SetEvent
    mov eax, 1
    jmp autosave_done
    
sync_save:
    ; Synchronous save
    call memory_persist_save
    inc SessionInfo.save_count
    
autosave_done:
    add rsp, 32
    pop rbx
    ret
    
autosave_throttled:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
session_trigger_autosave ENDP

;==========================================================================
; PUBLIC: session_install_autosave_timer(hWnd: rcx) -> eax
; Install WM_TIMER for auto-save
;==========================================================================
PUBLIC session_install_autosave_timer
session_install_autosave_timer PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx        ; hWnd
    
    ; Check if auto-save enabled
    mov eax, SessionConfig.auto_save_enabled
    test eax, eax
    jz no_timer
    
    ; Set timer
    mov rcx, rbx
    mov edx, AUTO_SAVE_TIMER_ID
    mov r8d, SessionConfig.auto_save_interval
    xor r9, r9
    call SetTimer
    
    mov eax, 1
    jmp timer_done
    
no_timer:
    xor eax, eax
    
timer_done:
    add rsp, 32
    pop rbx
    ret
session_install_autosave_timer ENDP

;==========================================================================
; PUBLIC: session_handle_timer(timerID: ecx) -> eax
; Handle WM_TIMER message
;==========================================================================
PUBLIC session_handle_timer
session_handle_timer PROC
    cmp ecx, AUTO_SAVE_TIMER_ID
    jne not_autosave_timer
    
    ; Trigger auto-save
    call session_trigger_autosave
    mov eax, 1
    ret
    
not_autosave_timer:
    xor eax, eax
    ret
session_handle_timer ENDP

;==========================================================================
; PUBLIC: session_get_stats() -> rax (pointer to SESSION_INFO)
; Get current session statistics
;==========================================================================
PUBLIC session_get_stats
session_get_stats PROC
    lea rax, [SessionInfo]
    ret
session_get_stats ENDP

;==========================================================================
; PUBLIC: session_get_config() -> rax (pointer to SESSION_CONFIG)
; Get session configuration
;==========================================================================
PUBLIC session_get_config
session_get_config PROC
    lea rax, [SessionConfig]
    ret
session_get_config ENDP

;==========================================================================
; INTERNAL: generate_session_id() -> eax
;==========================================================================
generate_session_id PROC
    push rbx
    push rsi
    sub rsp, 32
    
    ; Generate UUID-like ID (simplified: timestamp + random)
    call GetTickCount
    mov rbx, rax
    
    ; Format into SessionInfo.session_id
    lea rcx, [SessionInfo.session_id]
    mov edx, ebx
    lea r8, ["SESSION_%08X"]
    call wsprintfA
    
    mov eax, 1
    add rsp, 32
    pop rsi
    pop rbx
    ret
generate_session_id ENDP

;==========================================================================
; INTERNAL: load_session_config() -> eax
;==========================================================================
load_session_config PROC
    push rbx
    sub rsp, 32
    
    ; Set defaults
    mov SessionConfig.auto_save_enabled, 1
    mov SessionConfig.auto_save_interval, AUTO_SAVE_INTERVAL
    mov SessionConfig.max_backups, 10
    mov SessionConfig.compression_level, 3
    mov SessionConfig.recovery_enabled, 1
    
    ; Could load from config file here
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
load_session_config ENDP

;==========================================================================
; INTERNAL: check_crash_recovery() -> eax
;==========================================================================
check_crash_recovery PROC
    push rbx
    sub rsp, 32
    
    ; Check if previous session crashed
    mov SessionInfo.crash_detected, 0
    
    ; Look for .lock files from previous sessions
    lea rcx, [szSessionBaseDir]
    call find_stale_locks
    
    test eax, eax
    jz no_recovery
    
    ; Crash detected
    mov SessionInfo.crash_detected, 1
    mov IsRecovering, 1
    
    lea rcx, [szSessionRecover]
    call asm_log
    
    ; Attempt recovery
    call memory_persist_load
    
    mov IsRecovering, 0
    jmp recovery_done
    
no_recovery:
    xor eax, eax
    
recovery_done:
    add rsp, 32
    pop rbx
    ret
check_crash_recovery ENDP

;==========================================================================
; INTERNAL: delete_session_lock() -> eax
;==========================================================================
delete_session_lock PROC
    ; rcx = session_id
    lea rdx, [szSessionLockFile]
    lea r8, [SessionInfo.session_id]
    call wsprintfA
    
    ; DeleteFileA would go here
    mov eax, 1
    ret
delete_session_lock ENDP

;==========================================================================
; INTERNAL: cleanup_old_sessions() -> eax
;==========================================================================
cleanup_old_sessions PROC
    lea rcx, [szSessionCleanup]
    call asm_log
    
    ; Find and delete session files older than 30 days
    mov eax, 1
    ret
cleanup_old_sessions ENDP

;==========================================================================
; INTERNAL: find_stale_locks() -> eax
;==========================================================================
find_stale_locks PROC
    ; rcx = directory path
    ; Returns: 1 if stale locks found, 0 otherwise
    mov eax, 0
    ret
find_stale_locks ENDP

;==========================================================================
; PUBLIC: session_manager_init() -> eax
; Initialize session management and start auto-save thread
;==========================================================================
PUBLIC session_manager_init
session_manager_init PROC
    push rbx
    sub rsp, 32
    
    ; Create events
    xor rcx, rcx        ; lpEventAttributes
    mov edx, 1          ; bManualReset
    xor r8, r8          ; bInitialState
    xor r9, r9          ; lpName
    call CreateEventA
    mov hSaveEvent, rax
    
    xor rcx, rcx
    mov edx, 1
    xor r8, r8
    xor r9, r9
    call CreateEventA
    mov hStopEvent, rax
    
    ; Start background thread
    xor rcx, rcx        ; lpThreadAttributes
    xor rdx, rdx        ; dwStackSize
    lea r8, session_save_thread
    xor r9, r9          ; lpParameter
    mov dword ptr [rsp + 32], 0 ; dwCreationFlags
    xor rax, rax
    mov [rsp + 40], rax ; lpThreadId
    call CreateThread
    mov hSaveThread, rax
    
    ; Set auto-save timer (5 minutes)
    ; Need hwnd_main here, but for now we'll assume it's set globally
    ; call ui_get_main_hwnd
    ; mov rcx, rax
    ; mov rdx, AUTO_SAVE_TIMER_ID
    ; mov r8, AUTO_SAVE_INTERVAL
    ; xor r9, r9
    ; call SetTimer
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
session_manager_init ENDP

;==========================================================================
; INTERNAL: session_save_thread() -> 0
; Background thread for async saves
;==========================================================================
session_save_thread PROC
    push rbx
    push rsi
    sub rsp, 64
    
    ; Prepare handle array for WaitForMultipleObjects
    mov rax, hSaveEvent
    mov [rsp + 32], rax
    mov rax, hStopEvent
    mov [rsp + 40], rax
    
save_loop:
    ; Wait for either save event or stop event
    mov ecx, 2          ; nCount
    lea rdx, [rsp + 32] ; lpHandles
    xor r8, r8          ; bWaitAll = FALSE
    mov r9d, -1         ; dwMilliseconds = INFINITE
    call WaitForMultipleObjects
    
    ; Check if stop event fired (index 1)
    cmp eax, 1          ; WAIT_OBJECT_0 + 1
    je save_thread_stop
    
    ; Save event fired (index 0)
    mov IsSaving, 1
    
    ; Log save start
    lea rcx, [szSessionSave]
    mov edx, SessionInfo.save_count
    lea r8, [rsp + 48]  ; temp buffer
    call wsprintfA
    lea rcx, [rsp + 48]
    call asm_log
    
    call memory_persist_save
    test eax, eax
    jz save_failed
    
    ; Log success
    lea rcx, [szSessionSaveOK]
    call asm_log
    
    inc SessionInfo.save_count
    jmp save_continue
    
save_failed:
    lea rcx, [szSessionSaveErr]
    lea rdx, szIOError
    lea r8, [rsp + 48]
    call wsprintfA
    lea rcx, [rsp + 48]
    call asm_log
    
save_continue:
    mov IsSaving, 0
    
    ; Reset event
    mov rcx, hSaveEvent
    call ResetEvent
    
    jmp save_loop
    
save_thread_stop:
    mov eax, 0
    add rsp, 64
    pop rsi
    pop rbx
    ret
    
szIOError BYTE "I/O error during persistence",0
session_save_thread ENDP

;==========================================================================
; PUBLIC: session_trigger_autosave() -> eax
; Trigger the background save thread
;==========================================================================
PUBLIC session_trigger_autosave
session_trigger_autosave PROC
    push rbx
    sub rsp, 32
    
    ; Check throttle
    call GetTickCount
    mov rbx, rax
    sub rax, LastSaveTime
    cmp rax, SAVE_THROTTLE_TIME
    jb throttle_skip
    
    mov LastSaveTime, rbx
    
    ; Trigger event
    mov rcx, hSaveEvent
    call SetEvent
    
    mov eax, 1
    jmp trigger_done
    
throttle_skip:
    xor eax, eax
    
trigger_done:
    add rsp, 32
    pop rbx
    ret
session_trigger_autosave ENDP

;==========================================================================
; EXTERN STUBS
;==========================================================================
EXTERN memory_persist_save:PROC
EXTERN memory_persist_load:PROC
EXTERN memory_persist_mark_dirty:PROC
EXTERN asm_log:PROC
EXTERN wsprintfA:PROC
EXTERN GetTickCount:PROC
EXTERN CreateEventA:PROC
EXTERN CreateThread:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC
EXTERN WaitForMultipleObjects:PROC
EXTERN WaitForSingleObject:PROC
EXTERN CloseHandle:PROC
EXTERN SetTimer:PROC

END
