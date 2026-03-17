; ============================================================================
; AGENT REGISTRY - Manages Up to 100 Custom Agents
; Pure MASM - Production Ready
; ============================================================================

option casemap:none

; ============================================================================
; CONSTANTS
; ============================================================================
MAX_AGENTS           equ 100
AGENT_STRUCT_SIZE    equ 512
NULL                equ 0

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================
PUBLIC AgentRegistry_Initialize
PUBLIC AgentRegistry_Store
PUBLIC AgentRegistry_Retrieve
PUBLIC AgentRegistry_Remove
PUBLIC AgentRegistry_Count

; ============================================================================
; DATA SECTION
; ============================================================================
.data

align 16
agentRegistry     dq MAX_AGENTS dup(0)  ; Array of agent pointers
agentCount        dd 0                   ; Current agent count
registryMutex     dd 0                   ; Simple mutex for thread safety

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ============================================================================
; AgentRegistry_Initialize
; Initializes the agent registry
; ============================================================================
AgentRegistry_Initialize PROC
    ; Clear registry array
    lea rdi, agentRegistry
    mov rcx, MAX_AGENTS
    xor rax, rax
    rep stosq
    
    ; Reset count
    mov dword ptr [agentCount], 0
    
    ; Initialize mutex (simple flag-based)
    mov dword ptr [registryMutex], 0
    
    ret
AgentRegistry_Initialize ENDP

; ============================================================================
; AgentRegistry_Store
; Stores an agent in the registry
; RCX = agent ID, RDX = agent struct pointer
; Returns: RAX = 1 if success, 0 if failed
; ============================================================================
AgentRegistry_Store PROC
    push rbx
    push rsi
    push rdi
    
    ; Acquire mutex
    call @acquire_mutex
    
    mov rbx, rcx                    ; Save agent ID
    mov rsi, rdx                    ; Save agent struct pointer
    
    ; Check if registry is full
    mov eax, [agentCount]
    cmp eax, MAX_AGENTS
    jge @store_failed
    
    ; Find empty slot
    lea rdi, agentRegistry
    mov rcx, MAX_AGENTS
    xor rax, rax
    repne scasq
    jnz @store_failed               ; No empty slot found
    
    ; Store agent pointer (rdi now points to empty slot)
    sub rdi, 8                      ; Back up to the empty slot
    mov [rdi], rsi
    
    ; Increment count
    inc dword ptr [agentCount]
    
    ; Release mutex
    call @release_mutex
    
    mov rax, 1                      ; Success
    jmp @store_done
    
@store_failed:
    ; Release mutex
    call @release_mutex
    xor rax, rax                    ; Failure
    
@store_done:
    pop rdi
    pop rsi
    pop rbx
    ret
AgentRegistry_Store ENDP

; ============================================================================
; AgentRegistry_Retrieve
; Retrieves an agent by ID
; RCX = agent ID
; Returns: RAX = agent struct pointer if found, 0 if not found
; ============================================================================
AgentRegistry_Retrieve PROC
    push rbx
    push rsi
    
    ; Acquire mutex
    call @acquire_mutex
    
    mov rbx, rcx                    ; Save agent ID
    
    ; Search registry
    lea rsi, agentRegistry
    mov rcx, MAX_AGENTS
    
@search_loop:
    mov rax, [rsi]
    test rax, rax
    jz @next_agent                  ; Skip empty slots
    
    ; Check agent ID
    mov edx, dword ptr [rax]        ; First dword is agent ID
    cmp edx, ebx
    je @found_agent
    
@next_agent:
    add rsi, 8
    loop @search_loop
    
    ; Agent not found
    call @release_mutex
    xor rax, rax
    jmp @retrieve_done
    
@found_agent:
    call @release_mutex
    mov rax, [rsi]                  ; Return agent pointer
    
@retrieve_done:
    pop rsi
    pop rbx
    ret
AgentRegistry_Retrieve ENDP

; ============================================================================
; AgentRegistry_Remove
; Removes an agent from the registry
; RCX = agent ID
; Returns: RAX = 1 if success, 0 if not found
; ============================================================================
AgentRegistry_Remove PROC
    push rbx
    push rsi
    
    ; Acquire mutex
    call @acquire_mutex
    
    mov rbx, rcx                    ; Save agent ID
    
    ; Search registry
    lea rsi, agentRegistry
    mov rcx, MAX_AGENTS
    
@remove_search_loop:
    mov rax, [rsi]
    test rax, rax
    jz @remove_next_agent           ; Skip empty slots
    
    ; Check agent ID
    mov edx, dword ptr [rax]        ; First dword is agent ID
    cmp edx, ebx
    je @remove_agent
    
@remove_next_agent:
    add rsi, 8
    loop @remove_search_loop
    
    ; Agent not found
    call @release_mutex
    xor rax, rax
    jmp @remove_done
    
@remove_agent:
    ; Clear the slot
    mov qword ptr [rsi], 0
    
    ; Decrement count
    dec dword ptr [agentCount]
    
    call @release_mutex
    mov rax, 1                      ; Success
    
@remove_done:
    pop rsi
    pop rbx
    ret
AgentRegistry_Remove ENDP

; ============================================================================
; AgentRegistry_Count
; Returns the current number of agents in registry
; Returns: RAX = agent count
; ============================================================================
AgentRegistry_Count PROC
    mov eax, [agentCount]
    ret
AgentRegistry_Count ENDP

; ============================================================================
; Internal mutex functions
; ============================================================================

@acquire_mutex:
    ; Simple spinlock acquire
    mov eax, 1
@acquire_loop:
    xchg eax, [registryMutex]
    test eax, eax
    jnz @acquire_loop
    ret

@release_mutex:
    ; Release mutex
    mov dword ptr [registryMutex], 0
    ret

END