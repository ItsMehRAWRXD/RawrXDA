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
    mov dword ptr [ChatCount], 0
    mov dword ptr [CurrentMode], 0
    xor eax, eax
    ret
agent_chat_init ENDP

agent_chat_set_mode PROC
    mov dword ptr [CurrentMode], ecx
    xor eax, eax
    ret
agent_chat_set_mode ENDP

agent_chat_send_message PROC
    ; rcx = message
    inc dword ptr [ChatCount]
    xor eax, eax
    ret
agent_chat_send_message ENDP

agent_chat_add_message PROC
    ; rcx = message, edx = type
    inc dword ptr [ChatCount]
    xor eax, eax
    ret
agent_chat_add_message ENDP

agent_chat_clear PROC
    mov dword ptr [ChatCount], 0
    xor eax, eax
    ret
agent_chat_clear ENDP

END
