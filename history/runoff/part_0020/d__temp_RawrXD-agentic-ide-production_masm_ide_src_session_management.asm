; ============================================================================
; File 28: session_management.asm - Auto-save, crash recovery, session persistence
; ============================================================================
; Purpose: Auto-save every 30s, crash recovery via .bak file, JSON session persistence
; Uses: SetTimer callback, JSON mini-parser, file I/O
; Functions: Init, AutoSaveTimer, SaveSession, LoadSession, RecoverFromCrash
; ============================================================================

.code

; CONSTANTS
; ============================================================================

AUTOSAVE_INTERVAL_MS    equ 30000    ; 30 seconds
SESSION_FILE_NAME       BYTE "session.json", 0
SESSION_BACKUP_NAME     BYTE "session.bak", 0

; INITIALIZATION
; ============================================================================

SessionManager_Init PROC USES rbx rcx rdx rsi rdi
    ; Returns: SessionManager* in rax
    ; { lastSaveTime, sessionFile, backupFile, isDirty, timerID, mutex }
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    
    ; Allocate main struct (96 bytes)
    mov rdx, 0
    mov r8, 96
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rbx, rax  ; rbx = SessionManager*
    
    ; Get current time
    sub rsp, 40
    call GetTickCount
    add rsp, 40
    mov [rbx + 0], rax  ; lastSaveTime
    
    ; Initialize fields
    mov qword ptr [rbx + 8], 0      ; sessionFile path
    mov qword ptr [rbx + 16], 0     ; backupFile path
    mov qword ptr [rbx + 24], 0     ; isDirty
    mov qword ptr [rbx + 32], 0     ; timerID
    
    ; Initialize CRITICAL_SECTION
    lea rdx, [rbx + 56]
    sub rsp, 40
    mov rcx, rdx
    call InitializeCriticalSection
    add rsp, 40
    
    ; Start timer for auto-save
    mov rcx, rbx
    mov rdx, AUTOSAVE_INTERVAL_MS
    mov r8, 0  ; would be window handle
    mov r9, 0  ; would be timer callback
    
    ; call SetTimer(hwnd, 1, interval, callback)
    
    mov rax, rbx
    ret
SessionManager_Init ENDP

; AUTO-SAVE TIMER CALLBACK
; ============================================================================

SessionManager_AutoSaveTimer PROC USES rbx rcx rdx rsi rdi sessionManager:PTR DWORD
    ; Called every 30 seconds
    ; Returns: void (timer callback)
    
    mov rcx, sessionManager
    mov rax, [rcx + 24]  ; isDirty
    cmp rax, 0
    je @timer_no_save
    
    ; Save session to disk
    call SessionManager_SaveSession
    
@timer_no_save:
    ret
SessionManager_AutoSaveTimer ENDP

; SAVE SESSION (JSON)
; ============================================================================

SessionManager_SaveSession PROC USES rbx rcx rdx rsi rdi r8 r9 sessionManager:PTR DWORD
    ; Save open tabs, cursor positions, scroll offsets to JSON
    ; Returns: 1 if successful, 0 if failed
    
    mov rcx, sessionManager
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, sessionManager
    
    ; Step 1: Build JSON string in memory
    ; Format:
    ; {
    ;   "tabs": [
    ;     { "file": "path", "cursor": 1234, "scroll": 100 },
    ;     ...
    ;   ],
    ;   "activeTab": 0,
    ;   "timestamp": 1734096000
    ; }
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rbx, rax  ; rbx = process heap
    
    ; Allocate JSON buffer (64KB max for session)
    mov rcx, rbx
    mov rdx, 0
    mov r8, 65536
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rsi, rax  ; rsi = JSON buffer
    
    ; Start JSON object
    mov rdi, rsi
    lea rax, [rel json_header]
    mov rcx, 20  ; strlen("{\"tabs\":[")
    rep movsb
    
    ; Iterate open tabs and add to JSON array
    ; (stub: would iterate VirtualTabManager.tabs)
    mov r8, 0  ; tab index
    
@tab_loop:
    cmp r8, 10  ; max 10 tabs for now
    jge @tabs_done
    
    ; Get tab info
    ; Format: {"file":"C:\\path\\file.cpp","cursor":100,"scroll":50}
    
    ; TODO: Implement tab serialization
    
    inc r8
    jmp @tab_loop
    
@tabs_done:
    ; Close JSON array and object
    lea rax, [rel json_footer]
    mov rcx, 3  ; strlen("]}")
    rep movsb
    
    ; Get current time
    sub rsp, 40
    call GetTickCount
    add rsp, 40
    
    ; Step 2: Write JSON to file
    ; First create backup of previous session
    lea rcx, [rel SESSION_FILE_NAME]
    lea rdx, [rel SESSION_BACKUP_NAME]
    
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    call CopyFileA
    add rsp, 40
    
    ; Open session file for writing
    lea rcx, [rel SESSION_FILE_NAME]
    mov rdx, 0x40000000  ; GENERIC_WRITE
    mov r8, 0            ; no sharing
    mov r9, 2            ; CREATE_ALWAYS
    
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    mov r8, r8
    mov r9, r9
    call CreateFileA
    add rsp, 40
    
    cmp rax, -1
    je @save_failed
    
    mov r10, rax  ; r10 = file handle
    
    ; Write JSON buffer to file
    mov r11, 0  ; bytes to write (length of rsi)
    
    sub rsp, 40
    mov rcx, r10
    mov rdx, rsi
    mov r8, r11
    lea r9, [rsp + 32]
    mov [rsp + 32], r9
    call WriteFile
    add rsp, 40
    
    ; Close file
    sub rsp, 40
    mov rcx, r10
    call CloseHandle
    add rsp, 40
    
    ; Update lastSaveTime
    sub rsp, 40
    call GetTickCount
    add rsp, 40
    mov rcx, sessionManager
    mov [rcx + 0], rax
    
    ; Clear dirty flag
    mov qword ptr [rcx + 24], 0
    
    mov rcx, sessionManager
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
    
@save_failed:
    mov rcx, sessionManager
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 0
    ret
SessionManager_SaveSession ENDP

; LOAD SESSION (JSON)
; ============================================================================

SessionManager_LoadSession PROC USES rbx rcx rdx rsi rdi r8 r9 sessionManager:PTR DWORD, filePath:PTR BYTE
    ; Load session from JSON file
    ; Parse and restore tabs, cursor positions, etc.
    ; Returns: 1 if successful, 0 if failed
    
    mov rcx, sessionManager
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, sessionManager
    mov rsi, filePath
    
    ; Step 1: Open and read JSON file
    sub rsp, 40
    mov rcx, rsi
    mov rdx, 0x80000000  ; GENERIC_READ
    mov r8, 1            ; FILE_SHARE_READ
    mov r9, 3            ; OPEN_EXISTING
    call CreateFileA
    add rsp, 40
    
    cmp rax, -1
    je @load_failed
    
    mov rbx, rax  ; rbx = file handle
    
    ; Get file size
    mov r8, 0
    sub rsp, 40
    mov rcx, rbx
    mov rdx, 0
    call GetFileSize
    add rsp, 40
    mov r8, rax
    
    ; Allocate read buffer
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    mov rdx, 0
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rdi, rax  ; rdi = JSON buffer
    
    ; Read file
    mov r9, 0
    sub rsp, 40
    mov rcx, rbx
    mov rdx, rdi
    mov r8, r8
    lea r9, [rsp + 32]
    mov [rsp + 32], r9
    call ReadFile
    add rsp, 40
    
    ; Close file
    sub rsp, 40
    mov rcx, rbx
    call CloseHandle
    add rsp, 40
    
    ; Step 2: Mini-parser: extract tab entries from JSON
    ; Look for "file": patterns and parse field-by-field
    
    mov rsi, rdi  ; rsi = JSON string
    xor r8, r8    ; tab count
    
@parse_loop:
    cmp r8, 20   ; max 20 tabs
    jge @parse_done
    
    ; Find next "file": "..." entry
    mov rcx, 6
    lea rax, [rel json_file_pattern]
    
    ; TODO: Implement JSON parsing
    
    inc r8
    jmp @parse_loop
    
@parse_done:
    ; Mark session loaded
    mov rcx, sessionManager
    mov [rcx + 24], 0  ; isDirty = false
    
    mov rcx, sessionManager
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
    
@load_failed:
    mov rcx, sessionManager
    lea rdx, [rcx + 56]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 0
    ret
SessionManager_LoadSession ENDP

; RECOVER FROM CRASH
; ============================================================================

SessionManager_RecoverFromCrash PROC USES rbx rcx rdx rsi rdi sessionManager:PTR DWORD
    ; Detect if .bak file exists and is newer than .json
    ; If so, restore from backup
    ; Returns: 1 if recovered, 0 if no crash detected
    
    mov rcx, sessionManager
    
    ; Check if backup exists
    lea rdx, [rel SESSION_BACKUP_NAME]
    sub rsp, 40
    mov rcx, rdx
    call GetFileAttributesA
    add rsp, 40
    
    cmp eax, -1
    je @no_crash
    
    ; Backup exists - check timestamps
    ; Get backup creation time vs session.json
    
    ; TODO: Implement timestamp comparison
    
    ; If backup is newer, restore
    lea rcx, [rel SESSION_BACKUP_NAME]
    lea rdx, [rel SESSION_FILE_NAME]
    
    sub rsp, 40
    mov rcx, rcx
    mov rdx, rdx
    call CopyFileA
    add rsp, 40
    
    ; Load recovered session
    mov rcx, sessionManager
    lea rdx, [rel SESSION_FILE_NAME]
    call SessionManager_LoadSession
    
    mov rax, 1
    ret
    
@no_crash:
    mov rax, 0
    ret
SessionManager_RecoverFromCrash ENDP

; MARK DIRTY
; ============================================================================

SessionManager_MarkDirty PROC USES rbx rcx sessionManager:PTR DWORD
    ; Set isDirty flag to trigger next auto-save
    
    mov rcx, sessionManager
    mov qword ptr [rcx + 24], 1  ; isDirty = true
    
    ret
SessionManager_MarkDirty ENDP

; JSON HELPERS
; ============================================================================

json_header BYTE "{\"tabs\":[", 0
json_footer BYTE "]}", 0
json_file_pattern BYTE "\"file\":\"", 0
json_cursor_pattern BYTE "\"cursor\":", 0

end
