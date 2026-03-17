; =====================================================================
; Chat Pane Component - Pure ASM
; Handles user <-> agent/model communication
; Message rendering, input handling, and backend integration
; =====================================================================

bits 64
default rel

section .data
    ; Chat message types
    MSG_TYPE_USER   equ 1
    MSG_TYPE_AGENT  equ 2
    MSG_TYPE_SYSTEM equ 3
    
    ; Message format strings
    szUserPrefix    db "[User] ", 0
    szAgentPrefix   db "[Agent] ", 0
    szSystemPrefix  db "[System] ", 0
    
    ; Chat UI labels
    szChatTitle     db "Chat with AI Agent", 0
    szInputPrompt   db "> ", 0
    
    ; Agent communication
    szAgentReady    db "Agent ready. Ask me anything about your code!", 0
    szProcessing    db "Processing...", 0
    
    ; Named pipe for agent communication
    szPipeName      db "\\.\pipe\nasm_ide_agent", 0

section .bss
    ; Chat message structure (each message is 256 bytes)
    struc ChatMessage
        .type:      resd 1          ; Message type (user/agent/system)
        .length:    resd 1          ; Message length
        .text:      resb 240        ; Message text
        .reserved:  resb 8          ; Reserved for alignment
    endstruc
    
    ; Chat state
    chatMessages    resb ChatMessage_size * 100  ; Store up to 100 messages
    messageCount    resd 1                       ; Current message count
    scrollPosition  resd 1                       ; Scroll position in chat
    
    ; Input buffer
    inputBuffer     resb 512        ; Current input being typed
    inputLength     resd 1          ; Length of current input
    inputCursorPos  resd 1          ; Cursor position in input
    
    ; Agent communication
    hPipe           resq 1          ; Named pipe handle
    agentConnected  resd 1          ; Is agent connected?
    
    ; Message being sent/received
    sendBuffer      resb 4096       ; Buffer for sending to agent
    recvBuffer      resb 4096       ; Buffer for receiving from agent
    
    ; File I/O counters (moved here before use)
    ioBytesWritten    resd 1
    ioBytesRead       resd 1

section .text

extern CreateNamedPipeA
extern ConnectNamedPipe
extern WriteFile
extern ReadFile
extern DisconnectNamedPipe
extern CloseHandle
extern GetTickCount

; =====================================================================
; Initialize Chat System
; =====================================================================
InitializeChat:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Clear all messages
    lea rdi, [chatMessages]
    xor eax, eax
    mov ecx, (ChatMessage_size * 100) / 8
    rep stosq
    
    ; Reset counters
    mov dword [messageCount], 0
    mov dword [scrollPosition], 0
    mov dword [inputLength], 0
    mov dword [inputCursorPos], 0
    mov dword [agentConnected], 0
    
    ; Clear input buffer
    lea rdi, [inputBuffer]
    xor eax, eax
    mov ecx, 512/8
    rep stosq
    
    ; Add welcome message
    lea rcx, [szAgentReady]
    mov edx, MSG_TYPE_SYSTEM
    call AddChatMessage
    
    ; Try to connect to agent
    call ConnectToAgent
    
    add rsp, 32
    pop rbp
    ret
; =====================================================================
; Render Chat Background
; =====================================================================
RenderChatBackground:
    push rbp
    mov rbp, rsp
    ; Placeholder: would fill chat pane area with background color
    pop rbp
    ret

; =====================================================================
; Render Chat Messages
; =====================================================================
RenderChatMessages:
    push rbp
    mov rbp, rsp
    ; Placeholder: would render each message with color coding
    pop rbp
    ret

; =====================================================================
; Render Chat Input
; =====================================================================
RenderChatInput:
    push rbp
    mov rbp, rsp
    ; Placeholder: would render input prompt and text
    pop rbp
    ret

; =====================================================================
; Render Chat Cursor
; =====================================================================
RenderChatCursor:
    push rbp
    mov rbp, rsp
    ; Placeholder: would render blinking cursor in input area
    pop rbp
    ret


; =====================================================================
; Add Chat Message
; Input: RCX = message text pointer, EDX = message type
; =====================================================================
AddChatMessage:
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    sub rsp, 16                     ; Align stack to 16 bytes
    
    ; Check if we have room
    mov eax, [messageCount]
    cmp eax, 100
    jge .scroll_up
    
.add_message:
    ; Get pointer to new message slot
    mov ebx, [messageCount]
    imul ebx, ChatMessage_size
    lea rdi, [chatMessages]
    add rdi, rbx
    
    ; Set message type
    mov [rdi + ChatMessage.type], edx
    
    ; Copy message text (preserve base pointer in RBX)
    mov rbx, rdi                    ; Save base pointer
    mov rsi, rcx
    lea rdi, [rbx + ChatMessage.text]
    xor ecx, ecx
    
.copy_loop:
    lodsb
    stosb
    inc ecx
    test al, al
    jz .copy_done
    cmp ecx, 239
    jge .copy_done
    jmp .copy_loop
    
.copy_done:
    ; Store length (use saved base pointer)
    mov [rbx + ChatMessage.length], ecx
    
    ; Increment message count
    inc dword [messageCount]
    
    ; Auto-scroll to bottom
    mov eax, [messageCount]
    sub eax, 10                     ; Show last 10 messages
    test eax, eax
    jns .set_scroll
    xor eax, eax
    
.set_scroll:
    mov [scrollPosition], eax
    jmp .exit
    
.scroll_up:
    ; Scroll messages up (remove oldest)
    lea rdi, [chatMessages]
    lea rsi, [chatMessages + ChatMessage_size]
    mov ecx, (ChatMessage_size * 99) / 8
    rep movsq
    
    ; Decrement count and retry
    dec dword [messageCount]
    jmp .add_message
    
.exit:
    add rsp, 16
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret

; =====================================================================
; Connect To Agent (via named pipe or direct API)
; Returns: EAX = 1 if connected, 0 if failed
; =====================================================================
ConnectToAgent:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Create named pipe for agent communication
    lea rcx, [szPipeName]
    mov edx, 0x00000003             ; PIPE_ACCESS_DUPLEX
    mov r8d, 0x00000000             ; PIPE_TYPE_BYTE
    mov r9d, 1                      ; nMaxInstances
    mov dword [rsp + 32], 4096      ; nOutBufferSize
    mov dword [rsp + 36], 4096      ; nInBufferSize
    mov dword [rsp + 40], 0         ; nDefaultTimeOut
    mov qword [rsp + 48], 0         ; lpSecurityAttributes
    call CreateNamedPipeA
    
    cmp rax, -1
    je .failed
    
    mov [hPipe], rax
    mov dword [agentConnected], 1
    
    ; Add connection message
    lea rcx, [.szConnected]
    mov edx, MSG_TYPE_SYSTEM
    call AddChatMessage
    
    mov eax, 1
    jmp .exit
    
.failed:
    mov dword [agentConnected], 0
    
    ; Add error message
    lea rcx, [.szNotConnected]
    mov edx, MSG_TYPE_SYSTEM
    call AddChatMessage
    
    xor eax, eax
    
.exit:
    add rsp, 64
    pop rbp
    ret

.szConnected    db "Connected to AI agent", 0
.szNotConnected db "Agent not available (running in offline mode)", 0

; =====================================================================
; Send Message To Agent
; Input: RCX = message text pointer
; Returns: EAX = 1 on success, 0 on failure
; =====================================================================
SendMessageToAgent:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    push rdi
    
    ; Check if agent is connected
    cmp dword [agentConnected], 0
    je .not_connected
    
    ; Copy message to send buffer
    mov rsi, rcx
    lea rdi, [sendBuffer]
    xor ecx, ecx
    
.copy_loop:
    lodsb
    stosb
    inc ecx
    test al, al
    jz .copy_done
    cmp ecx, 4095
    jge .copy_done
    jmp .copy_loop
    
.copy_done:
    mov ebx, ecx                    ; Save length
    
    ; Write to pipe
    mov rcx, [hPipe]
    lea rdx, [sendBuffer]
    mov r8d, ebx
    lea r9, [ioBytesWritten]
    mov qword [rsp + 32], 0
    call WriteFile
    
    test eax, eax
    jz .write_failed
    
    ; Start async read for response
    call ReceiveAgentResponse
    
    mov eax, 1
    jmp .exit
    
.not_connected:
    ; Simulate agent response locally (offline mode)
    call SimulateAgentResponse
    mov eax, 1
    jmp .exit
    
.write_failed:
    xor eax, eax
    
.exit:
    pop rdi
    pop rsi
    pop rbx
    add rsp, 64
    pop rbp
    ret

; =====================================================================
; Receive Agent Response
; Returns: EAX = 1 if response received, 0 otherwise
; =====================================================================
ReceiveAgentResponse:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Check if agent is connected
    cmp dword [agentConnected], 0
    je .not_connected
    
    ; Read from pipe
    mov rcx, [hPipe]
    lea rdx, [recvBuffer]
    mov r8d, 4096
    lea r9, [ioBytesRead]
    mov qword [rsp + 32], 0
    call ReadFile
    
    test eax, eax
    jz .failed
    
    ; Add response to chat
    lea rcx, [recvBuffer]
    mov edx, MSG_TYPE_AGENT
    call AddChatMessage
    
    mov eax, 1
    jmp .exit
    
.not_connected:
.failed:
    xor eax, eax
    
.exit:
    add rsp, 64
    pop rbp
    ret

; =====================================================================
; Simulate Agent Response (offline mode fallback)
; =====================================================================
SimulateAgentResponse:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Simple canned response for demo
    lea rcx, [.szResponse]
    mov edx, MSG_TYPE_AGENT
    call AddChatMessage
    
    add rsp, 32
    pop rbp
    ret

.szResponse db "I understand your request. In offline mode, connect to an AI model for full assistance.", 0

; =====================================================================
; Handle Chat Input (keyboard input for chat)
; Input: R8 = character or virtual key
; =====================================================================
HandleChatInput:
    push rbp
    mov rbp, rsp
    push rbx
    push rdi
    sub rsp, 32                     ; Align stack
    
    ; Check for Enter key
    cmp r8d, 0x0D               ; VK_RETURN
    je .send_message
    
    ; Check for Backspace
    cmp r8d, 0x08               ; VK_BACK
    je .backspace
    
    ; Regular character - add to input buffer
    mov eax, [inputLength]
    cmp eax, 511
    jge .exit                   ; Buffer full
    
    ; Add character
    lea rbx, [inputBuffer]
    add rbx, rax
    mov byte [rbx], r8b
    inc dword [inputLength]
    jmp .exit
    
.backspace:
    mov eax, [inputLength]
    test eax, eax
    jz .exit
    dec eax
    mov [inputLength], eax
    lea rbx, [inputBuffer]
    add rbx, rax
    mov byte [rbx], 0
    jmp .exit
    
.send_message:
    ; Check if input is not empty
    cmp dword [inputLength], 0
    je .exit
    
    ; Null-terminate input (with bounds check)
    mov eax, [inputLength]
    cmp eax, 511                ; Make sure we have room for null
    jge .exit
    lea rbx, [inputBuffer]
    add rbx, rax
    mov byte [rbx], 0
    
    ; Add user message to chat
    lea rcx, [inputBuffer]
    mov edx, MSG_TYPE_USER
    call AddChatMessage
    
    ; Send to agent
    lea rcx, [inputBuffer]
    call SendMessageToAgent
    
    ; Clear input buffer
    lea rdi, [inputBuffer]
    xor eax, eax
    mov ecx, 512/8
    rep stosq
    mov dword [inputLength], 0
    
.exit:
    add rsp, 32
    pop rdi
    pop rbx
    pop rbp
    ret

; =====================================================================
; Render Chat Pane (called by main render loop)
; This will be called from DirectX rendering code
; =====================================================================
RenderChatPane:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; 1. Render chat background
    ; Use GDI for background (compatible with all rendering paths)
    call RenderChatBackground
    
    ; 2. Render chat messages with scrolling
    call RenderChatMessages
    
    ; 3. Render input prompt
    call RenderChatInput
    
    ; 4. Render cursor in chat input
    call RenderChatCursor
    
    add rsp, 64
    
    pop rbp
    ret

; =====================================================================
; Get Chat Messages for Rendering
; Output: RAX = pointer to messages, EDX = count, R8D = scroll pos
; =====================================================================
global GetChatMessages
GetChatMessages:
    push rbp
    mov rbp, rsp
    
    lea rax, [chatMessages]
    mov edx, [messageCount]
    mov r8d, [scrollPosition]
    
    pop rbp
    ret

; =====================================================================
; Get Current Input Text
; Output: RAX = pointer to input buffer, EDX = length
; =====================================================================
global GetChatInput
GetChatInput:
    push rbp
    mov rbp, rsp
    
    lea rax, [inputBuffer]
    mov edx, [inputLength]
    
    pop rbp
    ret

; =====================================================================
; Expose global symbols for linking and inline assembly use
; API functions intended for external use:
global InitializeChat
global AddChatMessage
global SendMessageToAgent
global ReceiveAgentResponse
global HandleChatInput
global RenderChatPane
global ConnectToAgent
global GetChatMessages
global GetChatInput

; Internal rendering helpers (can be used by renderer modules):
global RenderChatBackground
global ioBytesWritten
global ioBytesRead
global RenderChatCursor
global SimulateAgentResponse

; Shared state variables for I/O:
global bytesWritten
global bytesRead
; =====================================================================
