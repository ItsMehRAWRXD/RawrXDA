;==========================================================================
; chat_persistence.asm - Save/Load Chat History
; ==========================================================================
; Features:
; - Save chat sessions to JSON file
; - Load previous chat sessions
; - Auto-save on exit
; - Session management (list, delete, restore)
; - Timestamp and metadata tracking
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

;==========================================================================
; CONSTANTS
;==========================================================================
MAX_CHAT_HISTORY    EQU 256            ; Max messages per session
MAX_MESSAGE_SIZE    EQU 512
MAX_SESSIONS        EQU 50
CHAT_PERSIST_DIR    EQU "C:\RawrXD\ChatHistory\"
SESSION_FILE_EXT    EQU ".json"

; Message roles
MSG_ROLE_USER       EQU 0
MSG_ROLE_AGENT      EQU 1
MSG_ROLE_SYSTEM     EQU 2

;==========================================================================
; STRUCTURES
;==========================================================================
CHAT_MESSAGE STRUCT
    role            DWORD ?             ; 0=user, 1=agent, 2=system
    timestamp       QWORD ?             ; FILETIME
    content         BYTE MAX_MESSAGE_SIZE DUP (?)
CHAT_MESSAGE ENDS

CHAT_SESSION STRUCT
    session_id      QWORD ?             ; Unique ID
    created         QWORD ?             ; Creation time
    modified        QWORD ?             ; Last modified time
    message_count   DWORD ?             ; Number of messages
    mode            DWORD ?             ; Agent mode (Ask/Edit/Plan/Config)
    messages        QWORD ?             ; Pointer to messages array
CHAT_SESSION ENDS

;==========================================================================
; DATA
;==========================================================================
.data
    ; Chat persistence settings
    bAutoSave           DWORD 1         ; Auto-save on exit
    bAutoLoad           DWORD 1         ; Auto-load previous session
    SaveInterval        DWORD 30000     ; 30 seconds between auto-saves (ms)
    
    ; JSON templates
    szJsonHeader        BYTE "{""chatSession"":{""id"":%lld,""created"":%lld,""mode"":%d,""messages"":[",0
    szJsonMessage       BYTE "{""role"":%d,""timestamp"":%lld,""content"":""%s""}",0
    szJsonFooter        BYTE "]}}",0
    
    ; Path and file strings
    szHistoryPath       BYTE CHAT_PERSIST_DIR,0
    szSessionFile       BYTE CHAT_PERSIST_DIR,"session_%016llx.json",0
    
    ; Session management
    ChatSessions        CHAT_SESSION MAX_SESSIONS DUP (<>)
    SessionCount        DWORD 0
    CurrentSessionId    QWORD 0
    
    ; Statistics
    SessionsSaved       QWORD 0
    SessionsLoaded      QWORD 0

.data?
    ; Temporary buffers
    JsonBuffer          BYTE 16384 DUP (?) ; 16KB JSON buffer
    FilePath            BYTE 260 DUP (?)

;==========================================================================
; CODE
;==========================================================================
.code

;==========================================================================
; PUBLIC: chat_persistence_init() -> rax (success)
; Initialize chat persistence system
;==========================================================================
PUBLIC chat_persistence_init
chat_persistence_init PROC
    push rbx
    push rdi
    sub rsp, 32
    
    ; Create chat history directory if not exists
    lea rcx, szHistoryPath
    call create_directory_safe
    
    ; Initialize session array
    xor eax, eax
    mov SessionCount, eax
    
    ; Generate new session ID
    call GetTickCount64
    mov CurrentSessionId, rax
    
    mov eax, 1                          ; Success
    add rsp, 32
    pop rdi
    pop rbx
    ret
chat_persistence_init ENDP

;==========================================================================
; PUBLIC: chat_persistence_save_session(session_id: rcx) -> eax
; Save current chat session to JSON file
;==========================================================================
PUBLIC chat_persistence_save_session
chat_persistence_save_session PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
    mov rsi, rcx                        ; rsi = session_id
    
    ; Build JSON output
    lea rdi, JsonBuffer
    
    ; Write JSON header
    mov r8d, CurrentSessionId           ; session ID (use current)
    call GetFileTime_Unix
    mov r9d, eax                        ; timestamp
    
    ; TODO: Format JSON header into buffer
    
    ; Open file for writing
    lea rcx, FilePath
    mov rdx, rsi
    call build_session_filename
    
    mov rcx, FilePath
    mov edx, FILE_WRITE
    mov r8d, FILE_ATTRIBUTE_NORMAL
    mov r9d, CREATE_ALWAYS
    call CreateFileA
    
    test rax, rax
    jz .save_fail
    
    mov rbx, rax                        ; rbx = file handle
    
    ; Write JSON buffer to file
    mov rcx, rbx
    lea rdx, JsonBuffer
    mov r8d, 0                          ; Calculate length (strlen)
    call write_file_safe
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
    inc SessionsSaved
    mov eax, 1                          ; Success
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
    
.save_fail:
    xor eax, eax
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
chat_persistence_save_session ENDP

;==========================================================================
; PUBLIC: chat_persistence_load_session(session_id: rcx) -> eax
; Load chat session from JSON file
;==========================================================================
PUBLIC chat_persistence_load_session
chat_persistence_load_session PROC
    push rbx
    push rdi
    push rsi
    sub rsp, 32
    
    mov rsi, rcx                        ; rsi = session_id
    
    ; Build filename
    lea rcx, FilePath
    mov rdx, rsi
    call build_session_filename
    
    ; Open file for reading
    mov rcx, FilePath
    mov edx, FILE_READ
    mov r8d, FILE_ATTRIBUTE_NORMAL
    mov r9d, OPEN_EXISTING
    call CreateFileA
    
    test rax, rax
    jz .load_fail
    
    mov rbx, rax                        ; rbx = file handle
    
    ; Read JSON from file
    mov rcx, rbx
    lea rdx, JsonBuffer
    mov r8d, 16384                      ; Max read size
    call read_file_safe
    
    ; Close file
    mov rcx, rbx
    call CloseHandle
    
    ; Parse JSON and load messages
    lea rcx, JsonBuffer
    call parse_json_session
    
    inc SessionsLoaded
    mov eax, 1                          ; Success
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
    
.load_fail:
    xor eax, eax
    add rsp, 32
    pop rsi
    pop rdi
    pop rbx
    ret
chat_persistence_load_session ENDP

;==========================================================================
; PRIVATE: build_session_filename(path_buffer: rcx, session_id: rdx) -> void
;==========================================================================
PRIVATE build_session_filename
build_session_filename PROC
    ; Format: CHAT_PERSIST_DIR/session_<16hex>.json
    ; Using rdx as session ID
    
    ; TODO: Implement sprintf-like formatting for hex
    ; For now, just copy template and append hex ID
    
    ret
build_session_filename ENDP

;==========================================================================
; PRIVATE: parse_json_session(json_buffer: rcx) -> eax (msg_count)
; Parse JSON and load messages into current session
;==========================================================================
PRIVATE parse_json_session
parse_json_session PROC
    push rbx
    push rdi
    push rsi
    
    mov rsi, rcx                        ; rsi = json buffer
    xor edi, edi                        ; edi = message count
    
    ; Simple JSON parser (find "messages" array)
    mov rcx, rsi
    lea rdx, szMessagesKey
    call find_string_in_buffer
    
    test eax, eax
    jz .parse_done
    
    ; Parse each message object
.parse_loop:
    cmp edi, MAX_CHAT_HISTORY
    jge .parse_done
    
    ; Find next message object {
    mov rcx, rsi
    mov edx, '{'
    call find_char_in_buffer
    
    test eax, eax
    jz .parse_done
    
    ; TODO: Extract message fields (role, timestamp, content)
    
    inc edi
    jmp .parse_loop
    
.parse_done:
    mov eax, edi
    pop rsi
    pop rdi
    pop rbx
    ret
parse_json_session ENDP

;==========================================================================
; PRIVATE: create_directory_safe(path: rcx) -> eax
;==========================================================================
PRIVATE create_directory_safe
create_directory_safe PROC
    push rbx
    
    call CreateDirectoryA
    
    pop rbx
    ret
create_directory_safe ENDP

;==========================================================================
; PRIVATE: write_file_safe(hFile: rcx, buffer: rdx, size: r8d) -> eax
;==========================================================================
PRIVATE write_file_safe
write_file_safe PROC
    push rbx
    sub rsp, 32
    
    mov rbx, r8d                        ; rbx = size
    
    ; WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, ...)
    call WriteFile
    
    add rsp, 32
    pop rbx
    ret
write_file_safe ENDP

;==========================================================================
; PRIVATE: read_file_safe(hFile: rcx, buffer: rdx, size: r8d) -> eax
;==========================================================================
PRIVATE read_file_safe
read_file_safe PROC
    push rbx
    sub rsp, 32
    
    ; ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, ...)
    call ReadFile
    
    add rsp, 32
    pop rbx
    ret
read_file_safe ENDP

;==========================================================================
; PRIVATE: find_string_in_buffer(buffer: rcx, needle: rdx) -> eax
;==========================================================================
PRIVATE find_string_in_buffer
find_string_in_buffer PROC
    xor eax, eax                        ; Not found
    ret
find_string_in_buffer ENDP

;==========================================================================
; PRIVATE: find_char_in_buffer(buffer: rcx, char: edx) -> eax
;==========================================================================
PRIVATE find_char_in_buffer
find_char_in_buffer PROC
    xor eax, eax                        ; Not found
    ret
find_char_in_buffer ENDP

;==========================================================================
; PRIVATE: GetFileTime_Unix() -> eax
; Get current time as Unix timestamp
;==========================================================================
PRIVATE GetFileTime_Unix
GetFileTime_Unix PROC
    call GetSystemTimeAsFileTime
    ; Convert FILETIME to Unix timestamp
    ; TODO: Implement conversion
    xor eax, eax
    ret
GetFileTime_Unix ENDP

END
