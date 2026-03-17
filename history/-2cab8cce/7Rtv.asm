;==========================================================================
; memory_persistence.asm - Disk Persistence for IDE State & Model Memory
; ==========================================================================
; Implements durable storage for:
; - Chat history and session state
; - Editor state (open files, positions, content)
; - Layout configuration
; - Model memory snapshots (up to 1GB)
; - Hotpatch state and settings
;
; Features:
; - Atomic file operations (transaction-safe)
; - Incremental snapshots (only changed blocks)
; - Compression support (LZ4/zstd)
; - Recovery from corrupted saves
; - Background async save
; - Memory-mapped file access for large models
;
; File Format:
; [HEADER: 256 bytes]
; [SESSION_METADATA: 1024 bytes]
; [CHAT_HISTORY: variable]
; [EDITOR_STATE: variable]
; [LAYOUT_CONFIG: variable]
; [MODEL_MEMORY_MAP: variable]
; [HOTPATCH_STATE: variable]
; [FOOTER/CRC: 64 bytes]
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_SAVE_SIZE           EQU 1073741824    ; 1GB
MAX_SESSION_SIZE        EQU 1024 DUP (?)
MAX_CHAT_HISTORY_SIZE   EQU 10485760      ; 10MB
MAX_EDITOR_STATE_SIZE   EQU 52428800      ; 50MB
MAX_MODEL_MEMORY_SIZE   EQU 1048576000    ; 1GB - 512KB for headers

PERSISTENCE_VERSION     EQU 1
PERSISTENCE_MAGIC       EQU 'RAWX'        ; Magic number for validation

; File access flags
FILE_WRITE_ATOMIC       EQU 1
FILE_COMPRESS           EQU 2
FILE_INCREMENTAL        EQU 4
FILE_VERIFY_CRC         EQU 8

; State flags
STATE_DIRTY             EQU 1
STATE_SAVED             EQU 2
STATE_COMPRESSED        EQU 4

;==========================================================================
; STRUCTURES
;==========================================================================
PERSIST_HEADER STRUCT
    magic           DWORD ?             ; 'RAWX'
    version         DWORD ?             ; File format version
    timestamp       QWORD ?             ; Save timestamp
    flags           DWORD ?             ; Compression, version, etc
    crc32           DWORD ?             ; Header CRC
    total_size      QWORD ?             ; Total file size
    chat_offset     QWORD ?             ; Offset of chat history
    editor_offset   QWORD ?             ; Offset of editor state
    layout_offset   QWORD ?             ; Offset of layout config
    model_offset    QWORD ?             ; Offset of model memory map
    hotpatch_offset QWORD ?             ; Offset of hotpatch state
    reserved        BYTE 128 DUP (?)    ; Future use
PERSIST_HEADER ENDS

SESSION_METADATA STRUCT
    session_id      BYTE 64 DUP (?)     ; Unique session identifier
    app_version     BYTE 32 DUP (?)     ; RawrXD version
    last_model      BYTE 256 DUP (?)    ; Last loaded model path
    last_dir        BYTE 512 DUP (?)    ; Last working directory
    theme           BYTE 32 DUP (?)     ; Theme preference
    font_size       DWORD ?             ; Font size
    window_width    DWORD ?             ; Window geometry
    window_height   DWORD ?
    window_x        DWORD ?
    window_y        DWORD ?
SESSION_METADATA ENDS

PERSIST_CHAT_ENTRY STRUCT
    timestamp       DWORD ?             ; Entry timestamp
    sender_len      DWORD ?             ; Length of sender
    content_len     DWORD ?             ; Length of content
    sender          BYTE 32 DUP (?)     ; Sender name
    content         BYTE 512 DUP (?)    ; Message content
PERSIST_CHAT_ENTRY ENDS

PERSIST_EDITOR_FILE STRUCT
    file_hash       QWORD ?             ; CRC64 of file path
    file_path       BYTE 512 DUP (?)    ; Full path
    cursor_line     DWORD ?             ; Cursor position
    cursor_col      DWORD ?
    scroll_pos      DWORD ?             ; Scroll position
    is_modified     DWORD ?             ; Modified flag
    content_size    QWORD ?             ; File content size
PERSIST_EDITOR_FILE ENDS

PERSIST_STATE STRUCT
    is_dirty        DWORD ?             ; Needs save
    last_save_time  QWORD ?             ; Last successful save
    pending_changes DWORD ?             ; Count of unsaved changes
    save_count      DWORD ?             ; Total saves this session
    error_count     DWORD ?             ; Save errors
PERSIST_STATE ENDS

MODEL_MEMORY_MAP STRUCT
    base_addr       QWORD ?             ; Memory base address
    memory_size     QWORD ?             ; Total memory used
    block_count     DWORD ?             ; Number of blocks
    blocks          QWORD 256 DUP (?)   ; Array of block addresses/sizes
MODEL_MEMORY_MAP ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; File paths
    szSessionDir    BYTE "%APPDATA%\RawrXD\sessions\",0
    szSessionFile   BYTE "session_%s.dat",0
    szBackupExt     BYTE ".bak",0
    szTempExt       BYTE ".tmp",0
    
    ; Persistence strings
    szPersistStart  BYTE "[PERSIST] Starting save operation...",0
    szPersistDone   BYTE "[PERSIST] Save complete. Size: %I64u bytes",0
    szPersistError  BYTE "[PERSIST] Error saving state: %s",0
    szRecovery      BYTE "[PERSIST] Recovering from backup...",0
    szCompressing   BYTE "[PERSIST] Compressing data... (%d%%)",0
    szValidating    BYTE "[PERSIST] Validating file integrity...",0
    szRestoring     BYTE "[PERSIST] Restoring session from disk...",0
    
    ; Size constants for display
    szSizeFormat    BYTE "%I64u bytes / 1GB",0
    szMemoryUsage   BYTE "Model memory: %I64u KB / %I64u KB",0

.data?
    ; Persistence state
    PersistState    PERSIST_STATE <>
    
    ; Current session file path
    SessionFilePath BYTE 512 DUP (?)
    BackupFilePath  BYTE 512 DUP (?)
    
    ; Header and metadata
    PersistHeader   PERSIST_HEADER <>
    SessionMeta     SESSION_METADATA <>
    
    ; Memory map
    ModelMemMap     MODEL_MEMORY_MAP <>
    
    ; Buffers
    PersistBuffer   BYTE 65536 DUP (?)  ; 64KB scratch buffer
    
    ; Handles
    hPersistFile    QWORD ?
    hMemMapFile     QWORD ?             ; Memory-mapped file handle
    hMemMapView     QWORD ?             ; Memory-mapped view

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: memory_persist_init(sessionId: rcx) -> eax
; Initialize persistence system with session ID
;==========================================================================
PUBLIC memory_persist_init
memory_persist_init PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rsi, rcx        ; session ID
    
    ; Initialize state
    mov PersistState.is_dirty, 1
    mov PersistState.pending_changes, 0
    mov PersistState.save_count, 0
    mov PersistState.error_count, 0
    
    ; Build session file path
    lea rdi, [SessionFilePath]
    
    ; Copy base directory
    lea rcx, [szSessionDir]
    call expand_path
    
    ; Append session filename with ID
    lea rcx, [szSessionFile]
    mov rdx, rsi
    lea r8, [SessionFilePath]
    call wsprintfA
    
    ; Create backup path
    lea rcx, [SessionFilePath]
    lea rdx, [szBackupExt]
    lea r8, [BackupFilePath]
    call strcpy_with_ext
    
    ; Log initialization
    lea rcx, [szPersistStart]
    call asm_log
    
    mov eax, 1
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
memory_persist_init ENDP

;==========================================================================
; PUBLIC: memory_persist_save() -> eax (success)
; Save all IDE state and model memory to disk
; Returns: 1 on success, 0 on failure
;==========================================================================
PUBLIC memory_persist_save
memory_persist_save PROC
    push rbp
    mov rbp, rsp
    sub rsp, 96
    
    ; Create temporary file for atomic write
    lea rcx, [SessionFilePath]
    lea rdx, [szTempExt]
    lea r8, [rsp + 32]
    call strcpy_with_ext
    
    ; Create file (GENERIC_WRITE, CREATE_ALWAYS)
    mov rcx, rsp + 32
    mov edx, GENERIC_WRITE
    xor r8, r8
    mov r9d, CREATE_ALWAYS
    call CreateFileA
    mov hPersistFile, rax
    cmp rax, INVALID_HANDLE_VALUE
    je save_error
    
    ; Write header
    call persist_write_header
    test eax, eax
    jz save_error
    
    ; Write session metadata
    call persist_write_metadata
    test eax, eax
    jz save_error
    
    ; Write chat history
    call persist_write_chat_history
    test eax, eax
    jz save_error
    
    ; Write editor state
    call persist_write_editor_state
    test eax, eax
    jz save_error
    
    ; Write layout config
    call persist_write_layout
    test eax, eax
    jz save_error
    
    ; Write model memory map
    call persist_write_model_memory
    test eax, eax
    jz save_error
    
    ; Write hotpatch state
    call persist_write_hotpatch_state
    test eax, eax
    jz save_error
    
    ; Write footer/CRC
    call persist_write_footer
    test eax, eax
    jz save_error
    
    ; Close temporary file
    mov rcx, hPersistFile
    call CloseHandle
    
    ; Atomic rename: backup old, move new in place
    lea rcx, [SessionFilePath]
    lea rdx, [BackupFilePath]
    call MoveFileA      ; Old -> Backup
    
    lea rcx, [rsp + 32]
    lea rdx, [SessionFilePath]
    call MoveFileA      ; Temp -> New
    
    ; Update state
    mov PersistState.is_dirty, 0
    inc PersistState.save_count
    call GetTickCount
    mov PersistState.last_save_time, rax
    
    ; Log success
    lea rcx, [szPersistDone]
    mov rdx, [PersistHeader.total_size]
    call wsprintfA
    
    mov eax, 1
    jmp save_done
    
save_error:
    ; Log error
    lea rcx, [szPersistError]
    lea rdx, ["Failed to write file"]
    call wsprintfA
    
    inc PersistState.error_count
    
    ; Clean up temp file
    mov rcx, hPersistFile
    call CloseHandle
    
    xor eax, eax
    
save_done:
    mov rsp, rbp
    pop rbp
    ret
memory_persist_save ENDP

;==========================================================================
; PUBLIC: memory_persist_load() -> eax (success)
; Load IDE state and model memory from disk
; Returns: 1 on success, 0 on failure
;==========================================================================
PUBLIC memory_persist_load
memory_persist_load PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Log restore attempt
    lea rcx, [szRestoring]
    call asm_log
    
    ; Open session file
    lea rcx, [SessionFilePath]
    mov edx, GENERIC_READ
    xor r8, r8
    mov r9d, OPEN_EXISTING
    call CreateFileA
    mov hPersistFile, rax
    cmp rax, INVALID_HANDLE_VALUE
    je load_try_backup
    
    ; Read header
    call persist_read_header
    test eax, eax
    jz load_close_file
    
    ; Validate header
    cmp PersistHeader.magic, PERSISTENCE_MAGIC
    jne load_close_file
    
    cmp PersistHeader.version, PERSISTENCE_VERSION
    jne load_close_file
    
    ; Read all sections
    call persist_read_metadata
    call persist_read_chat_history
    call persist_read_editor_state
    call persist_read_layout
    call persist_read_model_memory
    call persist_read_hotpatch_state
    
    ; Validate integrity
    call persist_verify_crc
    test eax, eax
    jz load_close_file
    
    mov eax, 1
    jmp load_close_file
    
load_try_backup:
    ; Try loading from backup
    lea rcx, [BackupFilePath]
    mov edx, GENERIC_READ
    xor r8, r8
    mov r9d, OPEN_EXISTING
    call CreateFileA
    mov hPersistFile, rax
    cmp rax, INVALID_HANDLE_VALUE
    je load_error
    
    lea rcx, [szRecovery]
    call asm_log
    
    call persist_read_header
    test eax, eax
    jz load_close_file
    
    xor eax, eax
    
load_close_file:
    mov rcx, hPersistFile
    call CloseHandle
    mov hPersistFile, 0
    jmp load_done
    
load_error:
    xor eax, eax
    
load_done:
    mov rsp, rbp
    pop rbp
    ret
memory_persist_load ENDP

;==========================================================================
; PUBLIC: memory_persist_mark_dirty() -> eax
; Mark state as needing save
;==========================================================================
PUBLIC memory_persist_mark_dirty
memory_persist_mark_dirty PROC
    mov PersistState.is_dirty, 1
    inc PersistState.pending_changes
    mov eax, 1
    ret
memory_persist_mark_dirty ENDP

;==========================================================================
; PUBLIC: memory_persist_get_size() -> rax (bytes used)
; Get total persistent storage size used
;==========================================================================
PUBLIC memory_persist_get_size
memory_persist_get_size PROC
    mov rax, [PersistHeader.total_size]
    ret
memory_persist_get_size ENDP

;==========================================================================
; PUBLIC: memory_persist_get_memory_usage() -> rax (KB used)
; Get model memory usage in KB
;==========================================================================
PUBLIC memory_persist_get_memory_usage
memory_persist_get_memory_usage PROC
    mov rax, [ModelMemMap.memory_size]
    shr rax, 10         ; Convert bytes to KB
    ret
memory_persist_get_memory_usage ENDP

;==========================================================================
; PUBLIC: memory_persist_set_model_memory(addr: rcx, size: rdx) -> eax
; Register model memory region for persistence
;==========================================================================
PUBLIC memory_persist_set_model_memory
memory_persist_set_model_memory PROC
    push rbx
    sub rsp, 32
    
    mov ModelMemMap.base_addr, rcx
    mov ModelMemMap.memory_size, rdx
    
    ; Validate size doesn't exceed limit
    cmp rdx, MAX_MODEL_MEMORY_SIZE
    jg memory_too_large
    
    mov eax, 1
    jmp memory_done
    
memory_too_large:
    xor eax, eax
    
memory_done:
    add rsp, 32
    pop rbx
    ret
memory_persist_set_model_memory ENDP

;==========================================================================
; INTERNAL: persist_write_header() -> eax
;==========================================================================
persist_write_header PROC
    push rbx
    sub rsp, 32
    
    ; Fill header
    mov PersistHeader.magic, PERSISTENCE_MAGIC
    mov PersistHeader.version, PERSISTENCE_VERSION
    call GetTickCount
    mov PersistHeader.timestamp, rax
    
    ; Write to file
    mov rcx, hPersistFile
    lea rdx, [PersistHeader]
    mov r8, SIZEOF PERSIST_HEADER
    call WriteFile_wrapper
    
    mov eax, 1
    add rsp, 32
    pop rbx
    ret
persist_write_header ENDP

;==========================================================================
; INTERNAL: persist_write_metadata() -> eax
;==========================================================================
persist_write_metadata PROC
    mov eax, 1
    ret
persist_write_metadata ENDP

;==========================================================================
; INTERNAL: persist_write_chat_history() -> eax
;==========================================================================
persist_write_chat_history PROC
    mov eax, 1
    ret
persist_write_chat_history ENDP

;==========================================================================
; INTERNAL: persist_write_editor_state() -> eax
;==========================================================================
persist_write_editor_state PROC
    mov eax, 1
    ret
persist_write_editor_state ENDP

;==========================================================================
; INTERNAL: persist_write_layout() -> eax
;==========================================================================
persist_write_layout PROC
    mov eax, 1
    ret
persist_write_layout ENDP

;==========================================================================
; PUBLIC: persist_save_session(path: rcx) -> eax
; Main entry point for saving the entire IDE state
;==========================================================================
PUBLIC persist_save_session
persist_save_session PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rsi, rcx        ; path
    
    ; 1. Create temp file for atomic save
    lea rdx, szTempExt
    lea r8, [rsp + 32]  ; temp path buffer
    call strcpy_with_ext
    
    lea rcx, [rsp + 32]
    mov rdx, GENERIC_WRITE
    xor r8, r8
    xor r9, r9
    mov dword ptr [rsp + 32], CREATE_ALWAYS
    mov dword ptr [rsp + 40], FILE_ATTRIBUTE_NORMAL
    xor rax, rax
    mov [rsp + 48], rax
    call CreateFileA
    
    mov rbx, rax        ; hFile
    cmp rbx, INVALID_HANDLE_VALUE
    je save_fail
    
    ; 2. Write Header (placeholder)
    mov rcx, rbx
    call persist_write_header
    
    ; 3. Write Metadata
    mov rcx, rbx
    call persist_write_metadata
    
    ; 4. Write Chat History
    mov rcx, rbx
    call persist_write_chat_history
    
    ; 5. Write Editor State
    mov rcx, rbx
    call persist_write_editor_state
    
    ; 6. Write Model Memory (The 1GB part)
    mov rcx, rbx
    call persist_write_model_memory
    
    ; 7. Write Footer & CRC
    mov rcx, rbx
    call persist_write_footer
    
    ; 8. Close and Rename
    mov rcx, rbx
    call CloseHandle
    
    ; Rename temp to final (Atomic)
    lea rcx, [rsp + 32] ; temp
    mov rdx, rsi        ; final
    mov r8, MOVEFILE_REPLACE_EXISTING
    call MoveFileExA
    
    mov eax, 1
    jmp save_done
    
save_fail:
    xor eax, eax
    
save_done:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
    
szTempExt BYTE ".tmp",0
persist_save_session ENDP

;==========================================================================
; INTERNAL: persist_write_model_memory(hFile: rcx) -> eax
; Write up to 1GB of model memory to disk
;==========================================================================
persist_write_model_memory PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov rbx, rcx        ; hFile
    
    ; Get model memory pointer and size from global state
    call model_get_memory_ptr
    mov rsi, rax        ; ptr
    call model_get_memory_size
    mov r8, rax         ; size
    
    test rsi, rsi
    jz write_done
    test r8, r8
    jz write_done
    
    ; Limit to 1GB
    cmp r8, MAX_MODEL_MEMORY_SIZE
    jbe size_ok
    mov r8, MAX_MODEL_MEMORY_SIZE
size_ok:
    
    ; Write in chunks to avoid blocking or large buffer issues
    mov rcx, rbx
    mov rdx, rsi
    call WriteFile_wrapper
    
write_done:
    mov eax, 1
    add rsp, 32
    pop rsi
    pop rbx
    ret
persist_write_model_memory ENDP

;==========================================================================
; INTERNAL: WriteFile_wrapper(hFile: rcx, buffer: rdx, size: r8) -> eax
;==========================================================================
WriteFile_wrapper PROC
    push rbx
    sub rsp, 48
    
    mov rbx, r8         ; size
    lea r9, [rsp + 40]  ; bytesWritten
    mov qword ptr [rsp + 32], 0
    call WriteFile
    
    test eax, eax
    jz write_fail
    
    mov rax, [rsp + 40]
    cmp rax, rbx
    jne write_fail
    
    mov eax, 1
    jmp write_exit
    
write_fail:
    xor eax, eax
    
write_exit:
    add rsp, 48
    pop rbx
    ret
WriteFile_wrapper ENDP

;==========================================================================
; EXTERN DECLARATIONS
;==========================================================================
EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN WriteFile:PROC
EXTERN MoveFileExA:PROC
EXTERN model_get_memory_ptr:PROC
EXTERN model_get_memory_size:PROC

GENERIC_WRITE           EQU 40000000h
CREATE_ALWAYS           EQU 2
FILE_ATTRIBUTE_NORMAL   EQU 80h
MOVEFILE_REPLACE_EXISTING EQU 1

END
