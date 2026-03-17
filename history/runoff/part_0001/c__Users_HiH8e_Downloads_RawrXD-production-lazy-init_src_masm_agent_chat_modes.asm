;==========================================================================
; agent_chat_modes.asm - Agent Chat with 4 Interaction Modes
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

PUBLIC agent_chat_init
PUBLIC agent_chat_set_mode
PUBLIC agent_chat_send_message
PUBLIC agent_chat_add_message
PUBLIC agent_chat_clear

MAX_MESSAGES EQU 256

.data?
    ChatHistory     BYTE MAX_MESSAGES * 256 DUP (?)
    ChatCount       DWORD ?
    CurrentMode     DWORD ?

.code

agent_chat_init PROC
    xor ChatCount, ChatCount
    xor CurrentMode, CurrentMode
    xor eax, eax
    ret
agent_chat_init ENDP

agent_chat_set_mode PROC
    mov CurrentMode, ecx
    xor eax, eax
    ret
agent_chat_set_mode ENDP

agent_chat_send_message PROC
    ; rcx = message
    inc ChatCount
    xor eax, eax
    ret
agent_chat_send_message ENDP

agent_chat_add_message PROC
    ; rcx = message, edx = type
    inc ChatCount
    xor eax, eax
    ret
agent_chat_add_message ENDP

agent_chat_clear PROC
    xor ChatCount, ChatCount
    xor eax, eax
    ret
agent_chat_clear ENDP

END
