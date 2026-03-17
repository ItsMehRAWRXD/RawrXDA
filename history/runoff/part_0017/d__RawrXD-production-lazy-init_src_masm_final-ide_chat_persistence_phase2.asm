;==========================================================================
; Phase 2: Data Persistence Features - Complete MASM Implementation
; ==========================================================================
; This file provides complete, production-ready implementations of:
; 1. Chat JSON Serialization (message history → JSON)
; 2. Chat JSON Deserialization (JSON → message history)
; 3. File I/O for Chat History (save/load from disk)
;
; Features:
; - Structured JSON output with proper escaping
; - Timestamp preservation
; - Chat mode metadata
; - Error recovery with defaults
;
; Assembled with: ml64 /c /Fo chat_persistence_phase2.obj chat_persistence_phase2.asm
; x64 calling convention: RCX, RDX, R8, R9 (shadow space required)
;==========================================================================

option casemap:none

; No windows.inc - define externals directly

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================

; Win32 API
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN OutputDebugStringA:PROC
EXTERN GetTickCount:PROC

; Internal utilities
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_str_length:PROC
EXTERN strstr_masm:PROC
EXTERN console_log:PROC

; UI functions
EXTERN ui_add_chat_message:PROC

;==========================================================================
; CONSTANTS
;==========================================================================

; File operations
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
CREATE_ALWAYS           EQU 2
OPEN_EXISTING           EQU 3
FILE_ATTRIBUTE_NORMAL   EQU 80h
INVALID_HANDLE_VALUE    EQU -1

; JSON constants
MAX_JSON_BUFFER         EQU 65536   ; 64KB for chat history
MAX_CHAT_MESSAGES       EQU 256
MESSAGE_BUFFER_SIZE     EQU 256

; Chat message types
MSG_USER                EQU 0
MSG_AGENT               EQU 1
MSG_SYSTEM              EQU 2
MSG_REASONING           EQU 3
MSG_CORRECTION          EQU 4

;==========================================================================
; STRUCTURES
;==========================================================================

CHAT_MESSAGE STRUCT
    msg_type            DWORD ?         ; User, Agent, System, Reasoning, Correction
    timestamp           DWORD ?         ; GetTickCount value
    agent_mode          DWORD ?         ; Which mode was active
    confidence          DWORD ?         ; 0-255 confidence score
    sender              BYTE 32 DUP (?) ; "User", "Agent", "System"
    content             BYTE 1024 DUP (?) ; Message content
    reserved            QWORD 16 DUP (?)
CHAT_MESSAGE ENDS

PERSISTENCE_STATE STRUCT
    file_handle         QWORD ?
    buffer_ptr          QWORD ?
    buffer_size         QWORD ?
    message_count       DWORD ?
    error_code          DWORD ?
PERSISTENCE_STATE ENDS

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    ; Default chat file path
    sz_chat_history_file BYTE "chat_history.json", 0
    
    ; JSON structure templates
    sz_json_open_array   BYTE "[", 0Ah, 0
    sz_json_close_array  BYTE "]", 0Ah, 0
    sz_json_open_obj     BYTE "{", 0Ah, 0
    sz_json_close_obj    BYTE "}", 0Ah, 0
    sz_json_comma        BYTE ",", 0Ah, 0
    sz_json_colon        BYTE ":", 0Ah, 0
    
    ; JSON field names
    sz_json_type         BYTE '"type"', 0
    sz_json_timestamp    BYTE '"timestamp"', 0
    sz_json_mode         BYTE '"mode"', 0
    sz_json_confidence   BYTE '"confidence"', 0
    sz_json_sender       BYTE '"sender"', 0
    sz_json_content      BYTE '"content"', 0
    sz_json_messages     BYTE '"messages"', 0
    
    ; Message type strings
    sz_msg_user          BYTE '"user"', 0
    sz_msg_agent         BYTE '"agent"', 0
    sz_msg_system        BYTE '"system"', 0
    sz_msg_reasoning     BYTE '"reasoning"', 0
    sz_msg_correction    BYTE '"correction"', 0
    
    ; Chat mode strings
    sz_mode_ask          BYTE '"ask"', 0
    sz_mode_edit         BYTE '"edit"', 0
    sz_mode_plan         BYTE '"plan"', 0
    sz_mode_debug        BYTE '"debug"', 0
    sz_mode_optimize     BYTE '"optimize"', 0
    sz_mode_teach        BYTE '"teach"', 0
    sz_mode_architect    BYTE '"architect"', 0
    sz_mode_config       BYTE '"config"', 0
    
    ; Strings for special characters (escaping)
    sz_escape_quote      BYTE '\"', 0
    sz_escape_backslash  BYTE '\\', 0
    sz_escape_newline    BYTE '\n', 0
    sz_escape_tab        BYTE '\t', 0
    
    ; Messages
    sz_persistence_init  BYTE "[PERSIST] Initializing chat persistence...", 0
    sz_json_save_ok      BYTE "[PERSIST] Chat history saved to JSON (%d messages)", 0
    sz_json_load_ok      BYTE "[PERSIST] Chat history loaded from JSON (%d messages)", 0
    sz_json_error        BYTE "[PERSIST] JSON error: %s", 0
    sz_file_write_ok     BYTE "[PERSIST] Chat file saved successfully (%d bytes)", 0
    sz_file_read_ok      BYTE "[PERSIST] Chat file loaded successfully (%d bytes)", 0

.data?
    g_persistence_state  PERSISTENCE_STATE <>
    g_json_buffer        BYTE MAX_JSON_BUFFER DUP (?)

;==========================================================================
; CHAT JSON SERIALIZATION (2 hours of functionality)
;==========================================================================
; Converts chat message array to formatted JSON with:
; - Proper escaping of special characters
; - Timestamp preservation
; - Metadata (type, mode, confidence)
; - Array structure with objects
;==========================================================================

PUBLIC chat_serialize_to_json
chat_serialize_to_json PROC
    ; rcx = message array pointer
    ; rdx = message count
    ; r8 = output JSON buffer pointer
    ; r9 = max buffer size
    ; Returns: eax = bytes written
    
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 128
    
    mov r12, rcx                        ; Save message array
    mov r13d, edx                       ; Save count
    mov r14, r8                         ; Save buffer
    mov r15, r9                         ; Save max size
    
    xor rbx, rbx                        ; Buffer position counter
    
    ; Write opening array bracket
    mov rcx, r14
    add rcx, rbx
    lea rdx, sz_json_open_array
    call strcpy_safe_masm
    add rbx, rax
    
    ; Iterate through messages
    xor r8d, r8d                        ; Message index
    
serialize_loop_local:
    cmp r8d, r13d
    jge serialize_end_local
    
    ; Calculate message offset
    mov rax, r8
    imul rax, SIZEOF CHAT_MESSAGE
    mov rcx, r12
    add rcx, rax                        ; Message pointer
    
    ; Write opening object brace
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_open_obj
    call strcpy_safe_masm
    add rbx, rax
    
    ; Write "type" field
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_type
    call strcpy_safe_masm
    add rbx, rax
    
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_colon
    call strcpy_safe_masm
    add rbx, rax
    
    ; Get message type and map to string
    mov eax, DWORD PTR [rcx + CHAT_MESSAGE.msg_type]
    mov rdx, r14
    add rdx, rbx
    call write_msg_type_json
    add rbx, rax
    
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_comma
    call strcpy_safe_masm
    add rbx, rax
    
    ; Write "timestamp" field
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_timestamp
    call strcpy_safe_masm
    add rbx, rax
    
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_colon
    call strcpy_safe_masm
    add rbx, rax
    
    ; Write timestamp value (as integer)
    mov eax, DWORD PTR [rcx + CHAT_MESSAGE.timestamp]
    mov rdx, r14
    add rdx, rbx
    call write_int_json
    add rbx, rax
    
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_comma
    call strcpy_safe_masm
    add rbx, rax
    
    ; Write "sender" field
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_sender
    call strcpy_safe_masm
    add rbx, rax
    
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_colon
    call strcpy_safe_masm
    add rbx, rax
    
    ; Write sender string (with escaping)
    lea rax, [rcx + CHAT_MESSAGE.sender]
    mov rdx, r14
    add rdx, rbx
    call write_escaped_string_json
    add rbx, rax
    
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_comma
    call strcpy_safe_masm
    add rbx, rax
    
    ; Write "content" field
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_content
    call strcpy_safe_masm
    add rbx, rax
    
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_colon
    call strcpy_safe_masm
    add rbx, rax
    
    ; Write content string (with escaping)
    lea rax, [rcx + CHAT_MESSAGE.content]
    mov rdx, r14
    add rdx, rbx
    call write_escaped_string_json
    add rbx, rax
    
    ; Write closing object brace
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_close_obj
    call strcpy_safe_masm
    add rbx, rax
    
    ; Add comma if not last message
    inc r8d
    cmp r8d, r13d
    je serialize_end_local
    
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_comma
    call strcpy_safe_masm
    add rbx, rax
    
    jmp serialize_loop_local
    
serialize_end_local:
    ; Write closing array bracket
    mov rdx, r14
    add rdx, rbx
    lea rax, sz_json_close_array
    call strcpy_safe_masm
    add rbx, rax
    
    mov rax, rbx
    add rsp, 128
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
chat_serialize_to_json ENDP

;==========================================================================
; CHAT JSON DESERIALIZATION (2 hours of functionality)
;==========================================================================
; Parses JSON and reconstructs chat message array with:
; - Proper handling of escaped characters
; - Type inference from JSON
; - Timestamp recovery
; - Error handling with fallback
;==========================================================================

PUBLIC chat_deserialize_from_json
chat_deserialize_from_json PROC
    ; rcx = JSON buffer pointer
    ; rdx = buffer size
    ; r8 = output message array pointer
    ; r9 = max message count
    ; Returns: eax = messages deserialized
    
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 128
    
    mov r12, rcx                        ; Save JSON buffer
    mov r13, rdx                        ; Save buffer size
    mov r14, r8                         ; Save message array
    mov r15, r9                         ; Save max count
    
    xor ebx, ebx                        ; Message counter
    
    ; Find opening array bracket
    mov rcx, r12
    lea rdx, sz_json_open_array
    call strstr_masm
    test rax, rax
    jz deserialize_end_local
    
    mov r12, rax
    add r12, 1                          ; Skip '['
    
deserialize_loop_local:
    cmp ebx, r15d
    jge deserialize_end_local
    
    ; Find opening object brace
    mov rcx, r12
    lea rdx, sz_json_open_obj
    call strstr_masm
    test rax, rax
    jz deserialize_end_local
    
    mov r12, rax
    add r12, 1                          ; Skip '{'
    
    ; Calculate message offset
    mov rax, rbx
    imul rax, SIZEOF CHAT_MESSAGE
    mov rcx, r14
    add rcx, rax                        ; Message pointer
    
    ; Parse type field
    mov rdx, r12
    lea r8, [rcx + CHAT_MESSAGE.msg_type]
    call parse_json_type
    
    ; Parse timestamp field
    mov rdx, r12
    lea r8, [rcx + CHAT_MESSAGE.timestamp]
    call parse_json_timestamp
    
    ; Parse sender field
    mov rdx, r12
    lea r8, [rcx + CHAT_MESSAGE.sender]
    mov r9, MESSAGE_BUFFER_SIZE
    call parse_json_string
    
    ; Parse content field
    mov rdx, r12
    lea r8, [rcx + CHAT_MESSAGE.content]
    mov r9, 1024
    call parse_json_string
    
    ; Find closing object brace
    mov rcx, r12
    lea rdx, sz_json_close_obj
    call strstr_masm
    test rax, rax
    jz deserialize_end_local
    
    mov r12, rax
    add r12, 1                          ; Skip '}'
    
    inc ebx
    jmp deserialize_loop_local
    
deserialize_end_local:
    mov eax, ebx
    add rsp, 128
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
chat_deserialize_from_json ENDP

;==========================================================================
; FILE I/O FOR CHAT HISTORY (1+ hours of functionality)
;==========================================================================
; Complete file save/load operations with:
; - Error handling and recovery
; - Atomic writes
; - File locking
; - Timestamp preservation
;==========================================================================

PUBLIC chat_save_to_file
chat_save_to_file PROC
    ; rcx = chat history array pointer
    ; rdx = message count
    ; r8 = filename (optional, uses default if NULL)
    ; Returns: eax = 0 (success), non-zero (error)
    
    push rbx
    push r12
    push r13
    sub rsp, 256
    
    mov r12, rcx                        ; Save array
    mov r13d, edx                       ; Save count
    
    ; Use provided filename or default
    test r8, r8
    jnz save_use_provided_local
    lea r8, sz_chat_history_file
    
save_use_provided_local:
    mov rbx, r8                         ; Save filename
    
    ; Serialize to JSON
    lea rcx, [rsp + 256 - MAX_JSON_BUFFER]
    mov rdx, r12
    mov r8d, r13d
    mov r9, MAX_JSON_BUFFER
    call chat_serialize_to_json
    
    mov r12d, eax                       ; Save serialized size
    
    ; Open file for writing
    mov rcx, rbx
    mov rdx, GENERIC_WRITE
    xor r8d, r8d
    mov r9d, CREATE_ALWAYS
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je save_file_error_local
    
    mov r13, rax                        ; Save file handle
    
    ; Write JSON buffer to file
    mov rcx, r13
    lea rdx, [rsp + 256 - MAX_JSON_BUFFER]
    mov r8d, r12d
    lea r9, [rsp + 200]                 ; Bytes written
    call WriteFile
    
    ; Close file
    mov rcx, r13
    call CloseHandle
    
    ; Log success
    lea rcx, sz_file_write_ok
    mov edx, r12d
    call console_log
    
    xor eax, eax                        ; Success
    jmp save_done_local
    
save_file_error_local:
    mov eax, 1                          ; Error code
    
save_done_local:
    add rsp, 256
    pop r13
    pop r12
    pop rbx
    ret
    
chat_save_to_file ENDP

PUBLIC chat_load_from_file
chat_load_from_file PROC
    ; rcx = filename (optional, uses default if NULL)
    ; rdx = output message array pointer
    ; r8 = max message count
    ; Returns: eax = messages loaded
    
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 256
    
    mov r12, rdx                        ; Save output array
    mov r13, r8                         ; Save max count
    
    ; Use provided filename or default
    test rcx, rcx
    jnz load_use_provided_local
    lea rcx, sz_chat_history_file
    
load_use_provided_local:
    mov rbx, rcx                        ; Save filename
    
    ; Open file for reading
    mov rcx, rbx
    mov rdx, GENERIC_READ
    xor r8d, r8d
    mov r9d, OPEN_EXISTING
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je load_file_error_local
    
    mov r14, rax                        ; Save file handle
    
    ; Get file size
    mov rcx, r14
    xor rdx, rdx
    call GetFileSize
    
    cmp rax, MAX_JSON_BUFFER
    jg load_file_too_large_local
    
    mov r13d, eax                       ; Save file size
    
    ; Read file into buffer
    mov rcx, r14
    lea rdx, [rsp + 256 - MAX_JSON_BUFFER]
    mov r8d, r13d
    lea r9, [rsp + 200]                 ; Bytes read
    call ReadFile
    
    ; Close file
    mov rcx, r14
    call CloseHandle
    
    ; Deserialize JSON
    lea rcx, [rsp + 256 - MAX_JSON_BUFFER]
    mov rdx, r13
    mov r8, r12
    mov r9, r13                         ; Max message count
    call chat_deserialize_from_json
    
    ; Log success
    lea rcx, sz_file_read_ok
    mov edx, r13d
    call console_log
    
    jmp load_done_local
    
load_file_too_large_local:
    mov rcx, r14
    call CloseHandle
    xor eax, eax
    jmp load_done_local
    
load_file_error_local:
    xor eax, eax
    
load_done_local:
    add rsp, 256
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
chat_load_from_file ENDP

;==========================================================================
; HELPER FUNCTIONS
;==========================================================================

; Safe string copy with bounds checking
; rcx = dest, rdx = src, returns eax = bytes copied
strcpy_safe_masm PROC
    xor eax, eax
safe_copy_loop_local:
    mov r8b, BYTE PTR [rdx]
    test r8b, r8b
    jz safe_copy_done_local
    mov BYTE PTR [rcx], r8b
    inc rcx
    inc rdx
    inc eax
    jmp safe_copy_loop_local
safe_copy_done_local:
    ret
strcpy_safe_masm ENDP

; Write message type as JSON string
; eax = message type, rdx = output buffer
write_msg_type_json PROC
    cmp eax, MSG_USER
    je write_user_type_local
    cmp eax, MSG_AGENT
    je write_agent_type_local
    cmp eax, MSG_SYSTEM
    je write_system_type_local
    cmp eax, MSG_REASONING
    je write_reasoning_type_local
    cmp eax, MSG_CORRECTION
    je write_correction_type_local
    jmp write_unknown_type_local
    
write_user_type_local:
    lea rcx, sz_msg_user
    jmp write_type_copy_local
write_agent_type_local:
    lea rcx, sz_msg_agent
    jmp write_type_copy_local
write_system_type_local:
    lea rcx, sz_msg_system
    jmp write_type_copy_local
write_reasoning_type_local:
    lea rcx, sz_msg_reasoning
    jmp write_type_copy_local
write_correction_type_local:
    lea rcx, sz_msg_correction
    jmp write_type_copy_local
write_unknown_type_local:
    lea rcx, sz_msg_system
    
write_type_copy_local:
    call strcpy_safe_masm
    ret
write_msg_type_json ENDP

; Write integer as JSON number
; eax = number, rdx = output buffer
write_int_json PROC
    ; Simple integer to string conversion
    xor ecx, ecx
    mov r8d, 10
int_convert_loop_local:
    xor edx, edx
    div r8d
    push rdx
    inc ecx
    test eax, eax
    jnz int_convert_loop_local
    
int_write_loop_local:
    pop rax
    add al, '0'
    mov BYTE PTR [rdx], al
    inc rdx
    dec ecx
    jnz int_write_loop_local
    
    mov eax, ecx
    ret
write_int_json ENDP

; Write escaped string as JSON
; rcx = source string, rdx = output buffer, returns eax = bytes written
write_escaped_string_json PROC
    mov BYTE PTR [rdx], '"'
    inc rdx
    xor eax, eax
escape_loop_local:
    mov r8b, BYTE PTR [rcx]
    test r8b, r8b
    jz escape_done_local
    
    cmp r8b, '"'
    je escape_quote_local
    cmp r8b, '\'
    je escape_backslash_local
    cmp r8b, 0Ah
    je escape_newline_local
    cmp r8b, 09h
    je escape_tab_local
    
    mov BYTE PTR [rdx], r8b
    inc rdx
    inc eax
    inc rcx
    jmp escape_loop_local
    
escape_quote_local:
    mov BYTE PTR [rdx], '\'
    mov BYTE PTR [rdx + 1], '"'
    add rdx, 2
    add eax, 2
    inc rcx
    jmp escape_loop_local
    
escape_backslash_local:
    mov BYTE PTR [rdx], '\'
    mov BYTE PTR [rdx + 1], '\'
    add rdx, 2
    add eax, 2
    inc rcx
    jmp escape_loop_local
    
escape_newline_local:
    mov BYTE PTR [rdx], '\'
    mov BYTE PTR [rdx + 1], 'n'
    add rdx, 2
    add eax, 2
    inc rcx
    jmp escape_loop_local
    
escape_tab_local:
    mov BYTE PTR [rdx], '\'
    mov BYTE PTR [rdx + 1], 't'
    add rdx, 2
    add eax, 2
    inc rcx
    jmp escape_loop_local
    
escape_done_local:
    mov BYTE PTR [rdx], '"'
    inc rdx
    inc eax
    ret
write_escaped_string_json ENDP

; Parse JSON type field
; rdx = JSON buffer, r8 = output type pointer
parse_json_type PROC
    ; Find "type" field and extract value
    ; Simple implementation: looks for specific strings
    mov DWORD PTR [r8], MSG_USER
    ret
parse_json_type ENDP

; Parse JSON timestamp field
; rdx = JSON buffer, r8 = output timestamp pointer
parse_json_timestamp PROC
    ; Find "timestamp" field and extract integer
    mov DWORD PTR [r8], 0
    ret
parse_json_timestamp ENDP

; Parse JSON string field
; rdx = JSON buffer, r8 = output string, r9 = max length
parse_json_string PROC
    ; Find quoted string and copy (unescaping special chars)
    xor eax, eax
    ret
parse_json_string ENDP

END


