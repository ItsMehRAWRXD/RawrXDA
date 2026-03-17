; ai_chat_panel_masm.asm
; Pure MASM x64 - AI Chat Panel (converted from C++ AIChatPanel class)
; Provides chat-style message interface with streaming support

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memcpy:PROC
EXTERN strlen:PROC
EXTERN strcpy:PROC
EXTERN console_log:PROC

; Message role enumeration
MSG_ROLE_USER EQU 0
MSG_ROLE_ASSISTANT EQU 1
MSG_ROLE_SYSTEM EQU 2

; Chat panel constants
MAX_MESSAGES EQU 1000
MAX_MESSAGE_SIZE EQU 8192
STREAMING_BUFFER_SIZE EQU 4096

; ============================================================================
; DATA STRUCTURES
; ============================================================================

; CHAT_MESSAGE - Single message in chat history
CHAT_MESSAGE STRUCT
    role DWORD ?                    ; MSG_ROLE_*
    content QWORD ?                 ; Pointer to message text (malloc'd)
    contentSize QWORD ?             ; Size of content
    timestamp QWORD ?               ; Unix timestamp
    isStreaming BYTE ?              ; 1 = currently streaming
CHAT_MESSAGE ENDS

; CHAT_CONTEXT - Complete chat panel state
CHAT_CONTEXT STRUCT
    messages QWORD ?                ; Pointer to array of CHAT_MESSAGE
    messageCount QWORD ?            ; Current message count
    maxMessages QWORD ?             ; Capacity
    
    streamingBuffer QWORD ?         ; Current streaming message buffer
    streamingSize QWORD ?           ; Size of streaming buffer
    
    context QWORD ?                 ; Code context (malloc'd)
    contextSize QWORD ?             ; Size of context
    filePath QWORD ?                ; Current file path (malloc'd)
    
    inputEnabled BYTE ?             ; Can user input
    cloudEnabled BYTE ?             ; Cloud API enabled
    localEnabled BYTE ?             ; Local inference enabled
    
    ; Callbacks
    addMessageCallback QWORD ?      ; Called when message added
    streamCallback QWORD ?          ; Called for streaming tokens
    errorCallback QWORD ?           ; Called on error
CHAT_CONTEXT ENDS

; ============================================================================
; GLOBAL DATA
; ============================================================================

.data
    szChatCreated DB "[CHAT] Chat panel created", 0
    szMessageAdded DB "[CHAT] Message added: role=%d, size=%lld bytes", 0
    szStreamingStarted DB "[CHAT] Streaming started", 0
    szStreamingFinished DB "[CHAT] Streaming finished (%lld bytes)", 0
    szContextSet DB "[CHAT] Context set (%lld bytes from %s)", 0

.code

; ============================================================================
; PUBLIC API
; ============================================================================

; chat_create(RCX = maxMessages)
; Create chat panel context
; Returns: RAX = pointer to CHAT_CONTEXT
PUBLIC chat_create
chat_create PROC
    push rbx
    
    ; Allocate context structure
    mov r8, rcx                    ; r8 = maxMessages
    mov rcx, SIZEOF CHAT_CONTEXT
    call malloc
    mov rbx, rax
    
    ; Allocate message array
    mov rcx, r8
    imul rcx, SIZEOF CHAT_MESSAGE
    call malloc
    mov [rbx + CHAT_CONTEXT.messages], rax
    
    ; Allocate streaming buffer
    mov rcx, STREAMING_BUFFER_SIZE
    call malloc
    mov [rbx + CHAT_CONTEXT.streamingBuffer], rax
    
    ; Set capacity
    mov [rbx + CHAT_CONTEXT.maxMessages], r8
    mov [rbx + CHAT_CONTEXT.messageCount], 0
    mov [rbx + CHAT_CONTEXT.streamingSize], 0
    
    ; Enable input by default
    mov byte [rbx + CHAT_CONTEXT.inputEnabled], 1
    
    lea rcx, [szChatCreated]
    call console_log
    
    mov rax, rbx
    pop rbx
    ret
chat_create ENDP

; ============================================================================

; chat_add_user_message(RCX = context, RDX = message, R8 = size)
; Add user message to chat history
PUBLIC chat_add_user_message
chat_add_user_message PROC
    push rbx
    push rsi
    
    mov rbx, rcx                   ; rbx = context
    mov rsi, rdx                   ; rsi = message
    mov r9, r8                     ; r9 = size
    
    ; Check if room for new message
    mov rax, [rbx + CHAT_CONTEXT.messageCount]
    cmp rax, [rbx + CHAT_CONTEXT.maxMessages]
    jge .no_room
    
    ; Get message slot
    mov rcx, [rbx + CHAT_CONTEXT.messages]
    imul rax, SIZEOF CHAT_MESSAGE
    add rcx, rax
    
    ; Set message properties
    mov dword [rcx + CHAT_MESSAGE.role], MSG_ROLE_USER
    
    ; Allocate and copy content
    mov rdx, r9
    inc rdx                        ; +1 for null terminator
    
    push rcx
    mov rcx, rdx
    call malloc
    pop rcx
    
    mov [rcx + CHAT_MESSAGE.content], rax
    mov [rcx + CHAT_MESSAGE.contentSize], r9
    
    ; Copy message text
    mov rcx, rax
    mov rdx, rsi
    mov r8, r9
    call memcpy
    
    ; Increment message count
    inc qword [rbx + CHAT_CONTEXT.messageCount]
    
    ; Log
    lea rcx, [szMessageAdded]
    mov edx, MSG_ROLE_USER
    mov r8, r9
    call console_log
    
    pop rsi
    pop rbx
    ret
    
.no_room:
    pop rsi
    pop rbx
    ret
chat_add_user_message ENDP

; ============================================================================

; chat_add_assistant_message(RCX = context, RDX = message, R8 = size, R9b = isStreaming)
; Add assistant message to chat history
PUBLIC chat_add_assistant_message
chat_add_assistant_message PROC
    push rbx
    push rsi
    
    mov rbx, rcx
    mov rsi, rdx
    mov r10, r8
    mov r11b, r9b
    
    ; Check room
    mov rax, [rbx + CHAT_CONTEXT.messageCount]
    cmp rax, [rbx + CHAT_CONTEXT.maxMessages]
    jge .no_room
    
    ; Get message slot
    mov rcx, [rbx + CHAT_CONTEXT.messages]
    imul rax, SIZEOF CHAT_MESSAGE
    add rcx, rax
    
    ; Set properties
    mov dword [rcx + CHAT_MESSAGE.role], MSG_ROLE_ASSISTANT
    mov byte [rcx + CHAT_MESSAGE.isStreaming], r11b
    
    ; Allocate and copy
    mov rdx, r10
    inc rdx
    
    push rcx
    mov rcx, rdx
    call malloc
    pop rcx
    
    mov [rcx + CHAT_MESSAGE.content], rax
    mov [rcx + CHAT_MESSAGE.contentSize], r10
    
    mov rcx, rax
    mov rdx, rsi
    mov r8, r10
    call memcpy
    
    inc qword [rbx + CHAT_CONTEXT.messageCount]
    
    cmp r11b, 1
    jne .not_streaming
    
    lea rcx, [szStreamingStarted]
    call console_log
    
.not_streaming:
    pop rsi
    pop rbx
    ret
    
.no_room:
    pop rsi
    pop rbx
    ret
chat_add_assistant_message ENDP

; ============================================================================

; chat_update_streaming(RCX = context, RDX = token, R8 = size)
; Update streaming message with new token
PUBLIC chat_update_streaming
chat_update_streaming PROC
    mov r9, [rcx + CHAT_CONTEXT.streamingSize]
    
    ; Append token to streaming buffer
    mov r10, [rcx + CHAT_CONTEXT.streamingBuffer]
    
    mov rax, r9
    add rax, r8
    cmp rax, STREAMING_BUFFER_SIZE
    jge .buffer_full
    
    ; Copy token
    mov rcx, r10
    add rcx, r9
    mov rdx, rdx                   ; token pointer
    call memcpy
    
    ; Update size
    add [rsp - 16], r8             ; Update streamingSize
    
    ret
    
.buffer_full:
    ret
chat_update_streaming ENDP

; ============================================================================

; chat_finish_streaming(RCX = context)
; Finalize current streaming message
PUBLIC chat_finish_streaming
chat_finish_streaming PROC
    mov r8, [rcx + CHAT_CONTEXT.streamingSize]
    
    ; Log completion
    lea rcx, [szStreamingFinished]
    mov rdx, r8
    call console_log
    
    ; Mark streaming as complete
    mov rax, [rcx + CHAT_CONTEXT.messageCount]
    dec rax
    
    mov r9, [rcx + CHAT_CONTEXT.messages]
    imul rax, SIZEOF CHAT_MESSAGE
    add r9, rax
    
    mov byte [r9 + CHAT_MESSAGE.isStreaming], 0
    
    ret
chat_finish_streaming ENDP

; ============================================================================

; chat_set_context(RCX = context, RDX = code, R8 = codeSize, R9 = filePath)
; Set code context for chat
PUBLIC chat_set_context
chat_set_context PROC
    push rbx
    
    mov rbx, rcx                   ; rbx = chat context
    
    ; Free old context
    mov rcx, [rbx + CHAT_CONTEXT.context]
    cmp rcx, 0
    je .skip_free
    call free
    
.skip_free:
    ; Allocate and copy new context
    mov rcx, r8
    inc rcx
    
    push r8
    push r9
    
    call malloc
    
    pop r9
    pop r8
    
    mov [rbx + CHAT_CONTEXT.context], rax
    mov [rbx + CHAT_CONTEXT.contextSize], r8
    
    mov rcx, rax
    mov rdx, rdx                   ; code pointer
    call memcpy
    
    ; Copy file path
    mov rcx, r9
    call strlen
    
    inc rax                        ; +1 for null terminator
    
    push r8
    push r9
    
    mov rcx, rax
    call malloc
    
    pop r9
    pop r8
    
    mov [rbx + CHAT_CONTEXT.filePath], rax
    
    mov rcx, rax
    mov rdx, r9
    call strcpy
    
    ; Log
    lea rcx, [szContextSet]
    mov rdx, r8
    mov r8, r9
    call console_log
    
    pop rbx
    ret
chat_set_context ENDP

; ============================================================================

; chat_set_input_enabled(RCX = context, RDX = enabled)
; Enable/disable user input
PUBLIC chat_set_input_enabled
chat_set_input_enabled PROC
    mov byte [rcx + CHAT_CONTEXT.inputEnabled], dl
    ret
chat_set_input_enabled ENDP

; ============================================================================

; chat_clear(RCX = context)
; Clear all messages from chat
PUBLIC chat_clear
chat_clear PROC
    mov r8, [rcx + CHAT_CONTEXT.messageCount]
    
    ; Free all message contents
    xor r9, r9
.clear_loop:
    cmp r9, r8
    jge .clear_done
    
    mov r10, [rcx + CHAT_CONTEXT.messages]
    imul r9, SIZEOF CHAT_MESSAGE
    add r10, r9
    
    mov rdx, [r10 + CHAT_MESSAGE.content]
    cmp rdx, 0
    je .next_msg
    
    mov rcx, rdx
    call free
    
.next_msg:
    inc r9
    jmp .clear_loop
    
.clear_done:
    mov [rcx + CHAT_CONTEXT.messageCount], 0
    
    ret
chat_clear ENDP

; ============================================================================

; chat_destroy(RCX = context)
; Free all chat resources
PUBLIC chat_destroy
chat_destroy PROC
    push rbx
    
    mov rbx, rcx
    
    ; Clear messages
    call chat_clear
    
    ; Free message array
    mov rcx, [rbx + CHAT_CONTEXT.messages]
    cmp rcx, 0
    je .skip_msgs
    call free
.skip_msgs:
    
    ; Free buffers
    mov rcx, [rbx + CHAT_CONTEXT.streamingBuffer]
    cmp rcx, 0
    je .skip_stream
    call free
.skip_stream:
    
    mov rcx, [rbx + CHAT_CONTEXT.context]
    cmp rcx, 0
    je .skip_ctx
    call free
.skip_ctx:
    
    mov rcx, [rbx + CHAT_CONTEXT.filePath]
    cmp rcx, 0
    je .skip_path
    call free
.skip_path:
    
    ; Free context
    mov rcx, rbx
    call free
    
    pop rbx
    ret
chat_destroy ENDP

; ============================================================================

END
