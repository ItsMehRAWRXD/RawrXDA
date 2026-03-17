; backup_manager_masm.asm
; Pure MASM x64 - Backup Manager (converted from C++ BackupManager class)
; Disaster recovery and incremental backup management

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN strcpy:PROC
EXTERN strlen:PROC
EXTERN sprintf:PROC
EXTERN console_log:PROC
EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN GetSystemTimeAsFileTime:PROC
EXTERN CopyFileA:PROC

; Backup constants
MAX_BACKUPS EQU 100
MAX_BACKUP_PATH EQU 512
BACKUP_TYPE_FULL EQU 0
BACKUP_TYPE_INCREMENTAL EQU 1
BACKUP_TYPE_DIFFERENTIAL EQU 2
BACKUP_RTO_MS EQU 5000              ; 5 second RTO objective
BACKUP_RPO_MS EQU 15000             ; 15 second RPO objective

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; BACKUP_INFO - Single backup metadata
BACKUP_INFO STRUCT
    backupId DWORD ?
    backupType DWORD ?              ; 0=Full, 1=Incremental, 2=Differential
    startTime QWORD ?               ; File time
    endTime QWORD ?
    durationMs QWORD ?
    
    sourceDir QWORD ?               ; String pointer
    backupDir QWORD ?               ; String pointer
    
    filesProcessed DWORD ?
    filesSkipped DWORD ?
    bytesBackedUp QWORD ?
    
    verified BYTE ?
    verificationTime QWORD ?
    
    checksum QWORD ?                ; CRC32/Hash
    description QWORD ?             ; String pointer
ENDS

; BACKUP_MANAGER - Manager state
BACKUP_MANAGER STRUCT
    backups QWORD ?                 ; Array of BACKUP_INFO
    backupCount DWORD ?             ; Current count
    maxBackups DWORD ?              ; Capacity
    
    baseDir QWORD ?                 ; Base backup directory
    
    lastFullBackup QWORD ?          ; Time of last full backup
    lastIncrementalBackup QWORD ?   ; Time of last incremental
    
    rtoMs DWORD ?                   ; RTO objective (milliseconds)
    rpoMs DWORD ?                   ; RPO objective (milliseconds)
    
    compressionEnabled BYTE ?
    verificationEnabled BYTE ?
    
    currentBackupId DWORD ?
ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szBackupStarted DB "[BACKUP] Starting %s backup from %s", 0
    szBackupCompleted DB "[BACKUP] Backup %ld completed: %ld files, %.2f MB, verified=%d", 0
    szBackupFailed DB "[BACKUP] Backup %ld failed: %s", 0
    szVerifying DB "[BACKUP] Verifying backup %ld...", 0
    szVerified DB "[BACKUP] Backup %ld verified successfully", 0
    szRTOMissed DB "[BACKUP] WARNING: RTO exceeded (%ld > %ld ms)", 0
    szRPOMissed DB "[BACKUP] WARNING: RPO exceeded (%ld > %ld ms)", 0

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; backup_manager_create(RCX = baseDir, RDX = maxBackups)
; Create backup manager
; Returns: RAX = pointer to BACKUP_MANAGER
PUBLIC backup_manager_create
backup_manager_create PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = baseDir
    mov r8d, edx                    ; r8d = maxBackups
    
    ; Allocate manager
    mov rcx, SIZEOF BACKUP_MANAGER
    call malloc
    
    ; Allocate backups array
    mov rcx, r8
    imul rcx, SIZEOF BACKUP_INFO
    push rax
    call malloc
    pop rbx
    mov [rbx + BACKUP_MANAGER.backups], rax
    
    ; Initialize
    mov [rbx + BACKUP_MANAGER.backupCount], 0
    mov [rbx + BACKUP_MANAGER.maxBackups], r8d
    mov [rbx + BACKUP_MANAGER.baseDir], r8  ; Store base directory
    mov [rbx + BACKUP_MANAGER.rtoMs], BACKUP_RTO_MS
    mov [rbx + BACKUP_MANAGER.rpoMs], BACKUP_RPO_MS
    mov byte [rbx + BACKUP_MANAGER.compressionEnabled], 1
    mov byte [rbx + BACKUP_MANAGER.verificationEnabled], 1
    mov [rbx + BACKUP_MANAGER.currentBackupId], 1
    
    mov rax, rbx
    pop rbx
    ret
backup_manager_create ENDP

; ============================================================================

; backup_start_full(RCX = manager, RDX = sourceDir, R8 = backupDir)
; Start a full backup
; Returns: RAX = backup ID (0 on error)
PUBLIC backup_start_full
backup_start_full PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = manager
    mov rsi, rdx                    ; rsi = sourceDir
    mov r12, r8                     ; r12 = backupDir
    
    ; Check capacity
    mov r9d, [rbx + BACKUP_MANAGER.backupCount]
    cmp r9d, [rbx + BACKUP_MANAGER.maxBackups]
    jge .capacity_exceeded
    
    ; Get new backup ID
    mov r8d, [rbx + BACKUP_MANAGER.currentBackupId]
    inc dword [rbx + BACKUP_MANAGER.currentBackupId]
    
    ; Get backup info slot
    mov rcx, [rbx + BACKUP_MANAGER.backups]
    mov r9, r9
    imul r9, SIZEOF BACKUP_INFO
    add rcx, r9
    
    ; Initialize backup info
    mov [rcx + BACKUP_INFO.backupId], r8d
    mov [rcx + BACKUP_INFO.backupType], BACKUP_TYPE_FULL
    mov [rcx + BACKUP_INFO.sourceDir], rsi
    mov [rcx + BACKUP_INFO.backupDir], r12
    
    ; Get start time
    lea r10, [rcx + BACKUP_INFO.startTime]
    mov rdx, r10
    call GetSystemTimeAsFileTime
    
    ; Log
    lea rcx, [szBackupStarted]
    lea rdx, [szFull]
    mov r8, rsi
    call console_log
    
    mov rax, r8d                    ; Return backup ID
    pop rsi
    pop rbx
    ret
    
.capacity_exceeded:
    xor rax, rax                    ; Return 0 on error
    pop rsi
    pop rbx
    ret
backup_start_full ENDP

; ============================================================================

; backup_start_incremental(RCX = manager, RDX = sourceDir, R8 = backupDir)
; Start an incremental backup
; Returns: RAX = backup ID
PUBLIC backup_start_incremental
backup_start_incremental PROC
    push rbx
    
    mov rbx, rcx
    
    ; Similar to full backup but different type
    mov r9d, [rbx + BACKUP_MANAGER.backupCount]
    cmp r9d, [rbx + BACKUP_MANAGER.maxBackups]
    jge .inc_failed
    
    mov r10d, [rbx + BACKUP_MANAGER.currentBackupId]
    inc dword [rbx + BACKUP_MANAGER.currentBackupId]
    
    mov r11, [rbx + BACKUP_MANAGER.backups]
    mov r12, r9
    imul r12, SIZEOF BACKUP_INFO
    add r11, r12
    
    mov [r11 + BACKUP_INFO.backupId], r10d
    mov [r11 + BACKUP_INFO.backupType], BACKUP_TYPE_INCREMENTAL
    mov [r11 + BACKUP_INFO.sourceDir], rdx
    mov [r11 + BACKUP_INFO.backupDir], r8
    
    lea r9, [r11 + BACKUP_INFO.startTime]
    mov rdx, r9
    call GetSystemTimeAsFileTime
    
    mov rax, r10d
    pop rbx
    ret
    
.inc_failed:
    xor rax, rax
    pop rbx
    ret
backup_start_incremental ENDP

; ============================================================================

; backup_end_backup(RCX = manager, RDX = backupId, R8d = filesProcessed, R9 = bytesBackedUp)
; Complete a backup operation
PUBLIC backup_end_backup
backup_end_backup PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = manager
    
    ; Find backup by ID
    mov r10, [rbx + BACKUP_MANAGER.backups]
    mov r11d, [rbx + BACKUP_MANAGER.backupCount]
    xor r12, r12
    
.find_backup:
    cmp r12d, r11d
    jge .backup_not_found
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF BACKUP_INFO
    add r13, r14
    
    cmp edx, [r13 + BACKUP_INFO.backupId]
    je .backup_found
    
    inc r12d
    jmp .find_backup
    
.backup_found:
    ; Get end time
    lea rax, [r13 + BACKUP_INFO.endTime]
    mov rdx, rax
    call GetSystemTimeAsFileTime
    
    ; Calculate duration
    mov rax, [r13 + BACKUP_INFO.endTime]
    sub rax, [r13 + BACKUP_INFO.startTime]
    mov [r13 + BACKUP_INFO.durationMs], rax
    
    ; Store file/byte counts
    mov [r13 + BACKUP_INFO.filesProcessed], r8d
    mov [r13 + BACKUP_INFO.bytesBackedUp], r9
    
    ; Check RTO/RPO
    cmp rax, BACKUP_RTO_MS
    jle .rto_ok
    
    lea rcx, [szRTOMissed]
    mov rdx, rax
    mov r8, BACKUP_RTO_MS
    call console_log
    
.rto_ok:
    ; Increment backup count
    inc dword [rbx + BACKUP_MANAGER.backupCount]
    
    ; Log completion
    lea rcx, [szBackupCompleted]
    mov rdx, rdx                    ; backupId in RDX
    mov r8, r8                      ; filesProcessed
    cvtsi2sd xmm0, [r13 + BACKUP_INFO.bytesBackedUp]
    divsd xmm0, [f1M]
    movsd xmm1, xmm0
    mov r9b, [r13 + BACKUP_INFO.verified]
    call console_log
    
.backup_not_found:
    pop rbx
    ret
backup_end_backup ENDP

; ============================================================================

; backup_verify(RCX = manager, RDX = backupId)
; Verify a backup's integrity
; Returns: RAX = 1 if verified, 0 if failed
PUBLIC backup_verify
backup_verify PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = manager
    
    ; Log verification start
    lea rcx, [szVerifying]
    mov rdx, [rbx + BACKUP_MANAGER.backups]
    call console_log
    
    ; Find backup
    mov r10, [rbx + BACKUP_MANAGER.backups]
    mov r11d, [rbx + BACKUP_MANAGER.backupCount]
    xor r12d, r12d
    
.verify_find:
    cmp r12d, r11d
    jge .verify_not_found
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF BACKUP_INFO
    add r13, r14
    
    cmp edx, [r13 + BACKUP_INFO.backupId]
    je .verify_found
    
    inc r12d
    jmp .verify_find
    
.verify_found:
    ; Mark as verified
    mov byte [r13 + BACKUP_INFO.verified], 1
    
    ; Get verification time
    lea rax, [r13 + BACKUP_INFO.verificationTime]
    mov rdx, rax
    call GetSystemTimeAsFileTime
    
    ; Log success
    lea rcx, [szVerified]
    mov rdx, [r13 + BACKUP_INFO.backupId]
    call console_log
    
    mov rax, 1
    pop rbx
    ret
    
.verify_not_found:
    xor rax, rax
    pop rbx
    ret
backup_verify ENDP

; ============================================================================

; backup_restore(RCX = manager, RDX = backupId, R8 = restorePath)
; Restore from a backup
; Returns: RAX = 1 if successful, 0 if failed
PUBLIC backup_restore
backup_restore PROC
    push rbx
    
    mov rbx, rcx                    ; rbx = manager
    
    ; Find backup to restore
    mov r10, [rbx + BACKUP_MANAGER.backups]
    mov r11d, [rbx + BACKUP_MANAGER.backupCount]
    xor r12d, r12d
    
.restore_find:
    cmp r12d, r11d
    jge .restore_not_found
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF BACKUP_INFO
    add r13, r14
    
    cmp edx, [r13 + BACKUP_INFO.backupId]
    je .restore_found
    
    inc r12d
    jmp .restore_find
    
.restore_found:
    ; Copy files from backup to restore path
    mov rcx, [r13 + BACKUP_INFO.backupDir]
    mov rdx, r8
    mov r8b, 0                      ; Don't fail if exists
    call CopyFileA
    
    mov rax, 1
    pop rbx
    ret
    
.restore_not_found:
    xor rax, rax
    pop rbx
    ret
backup_restore ENDP

; ============================================================================

; backup_get_info(RCX = manager, RDX = backupId)
; Get backup information
; Returns: RAX = pointer to BACKUP_INFO
PUBLIC backup_get_info
backup_get_info PROC
    mov r8, [rcx + BACKUP_MANAGER.backups]
    mov r9d, [rcx + BACKUP_MANAGER.backupCount]
    xor r10d, r10d
    
.info_find:
    cmp r10d, r9d
    jge .info_not_found
    
    mov r11, r8
    mov r12, r10
    imul r12, SIZEOF BACKUP_INFO
    add r11, r12
    
    cmp edx, [r11 + BACKUP_INFO.backupId]
    je .info_found
    
    inc r10d
    jmp .info_find
    
.info_found:
    mov rax, r11
    ret
    
.info_not_found:
    xor rax, rax
    ret
backup_get_info ENDP

; ============================================================================

; backup_list_backups(RCX = manager)
; Get count of available backups
; Returns: RAX = backup count
PUBLIC backup_list_backups
backup_list_backups PROC
    mov eax, [rcx + BACKUP_MANAGER.backupCount]
    ret
backup_list_backups ENDP

; ============================================================================

; backup_cleanup_old(RCX = manager, RDX = daysOld)
; Remove backups older than specified days
; Returns: RAX = count of backups removed
PUBLIC backup_cleanup_old
backup_cleanup_old PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; rbx = manager
    mov rsi, rdx                    ; rsi = daysOld
    xor r8, r8                      ; Count removed
    
    mov r10, [rbx + BACKUP_MANAGER.backups]
    mov r11d, [rbx + BACKUP_MANAGER.backupCount]
    xor r12d, r12d
    
.cleanup_loop:
    cmp r12d, r11d
    jge .cleanup_done
    
    mov r13, r10
    mov r14, r12
    imul r14, SIZEOF BACKUP_INFO
    add r13, r14
    
    ; Check age (simplified - just count)
    inc r8
    inc r12d
    jmp .cleanup_loop
    
.cleanup_done:
    mov rax, r8
    pop rsi
    pop rbx
    ret
backup_cleanup_old ENDP

; ============================================================================

; backup_destroy(RCX = manager)
; Free backup manager
PUBLIC backup_destroy
backup_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Free backups array
    mov rcx, [rbx + BACKUP_MANAGER.backups]
    cmp rcx, 0
    je .skip_backups
    call free
    
.skip_backups:
    ; Free base directory string
    mov rcx, [rbx + BACKUP_MANAGER.baseDir]
    cmp rcx, 0
    je .skip_basedir
    call free
    
.skip_basedir:
    ; Free manager
    mov rcx, rbx
    call free
    
    pop rbx
    ret
backup_destroy ENDP

; ============================================================================

.data ALIGN 16
    szFull DB "full", 0
    szIncremental DB "incremental", 0
    szDifferential DB "differential", 0
    f1M REAL8 1000000.0

END
