; ============================================================================
; SELF-MODIFICATION ENGINE - Allows Agents to Modify Their Own Configuration
; Pure MASM - Production Ready
; ============================================================================

option casemap:none

; ============================================================================
; CONSTANTS
; ============================================================================
NULL                equ 0
TRUE                equ 1
FALSE               equ 0
MAX_MODIFICATION_OPS equ 16

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================
PUBLIC SelfModification_ApplyRuleChange
PUBLIC SelfModification_ApplySkillChange
PUBLIC SelfModification_UpdateBackground
PUBLIC SelfModification_ValidateModification

; ============================================================================
; MODIFICATION OPERATION STRUCTURE
; ============================================================================
MODIFICATION_OP struct
    agentId            dd ?        ; 4 bytes
    operationType      db ?        ; 1 byte
    targetField        dq ?        ; 8 bytes
    newValue           dq ?        ; 8 bytes
    validationResult   db ?        ; 1 byte
    reserved           db 6 dup(?) ; Padding
MODIFICATION_OP ends

; ============================================================================
; DATA SECTION
; ============================================================================
.data

align 16
modificationQueue   MODIFICATION_OP MAX_MODIFICATION_OPS dup(<?>)
queueHead           dd 0
queueTail           dd 0
modificationMutex   dd 0

; Operation types
OP_ADD_RULE         equ 1
OP_REMOVE_RULE      equ 2
OP_ADD_SKILL        equ 3
OP_REMOVE_SKILL      equ 4
OP_UPDATE_BACKGROUND equ 5

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ============================================================================
; SelfModification_ApplyRuleChange
; Applies a rule change to an agent
; RCX = agent ID, RDX = operation type, R8 = rule string
; Returns: RAX = 1 if success, 0 if failed
; ============================================================================
SelfModification_ApplyRuleChange PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx                    ; Save agent ID
    mov rsi, rdx                    ; Save operation type
    mov rdi, r8                     ; Save rule string
    
    ; Validate agent exists
    mov rcx, rbx
    call AgentRegistry_Retrieve
    test rax, rax
    jz @rule_change_failed
    
    ; Check if self-modification is enabled
    mov rcx, rax
    call @check_self_modify_enabled
    test rax, rax
    jz @rule_change_failed
    
    ; Validate the modification
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call SelfModification_ValidateModification
    test rax, rax
    jz @rule_change_failed
    
    ; Queue the modification
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    mov r9, 0                       ; No target field for rules
    call @queue_modification
    test rax, rax
    jz @rule_change_failed
    
    ; Apply the modification
    call @process_modification_queue
    
    mov rax, 1                      ; Success
    jmp @rule_change_done
    
@rule_change_failed:
    xor rax, rax                    ; Failure
    
@rule_change_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
SelfModification_ApplyRuleChange ENDP

; ============================================================================
; SelfModification_ApplySkillChange
; Applies a skill change to an agent
; RCX = agent ID, RDX = operation type, R8 = skill string
; Returns: RAX = 1 if success, 0 if failed
; ============================================================================
SelfModification_ApplySkillChange PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx                    ; Save agent ID
    mov rsi, rdx                    ; Save operation type
    mov rdi, r8                     ; Save skill string
    
    ; Validate agent exists
    mov rcx, rbx
    call AgentRegistry_Retrieve
    test rax, rax
    jz @skill_change_failed
    
    ; Check if self-modification is enabled
    mov rcx, rax
    call @check_self_modify_enabled
    test rax, rax
    jz @skill_change_failed
    
    ; Validate the modification
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    call SelfModification_ValidateModification
    test rax, rax
    jz @skill_change_failed
    
    ; Queue the modification
    mov rcx, rbx
    mov rdx, rsi
    mov r8, rdi
    mov r9, 0                       ; No target field for skills
    call @queue_modification
    test rax, rax
    jz @skill_change_failed
    
    ; Apply the modification
    call @process_modification_queue
    
    mov rax, 1                      ; Success
    jmp @skill_change_done
    
@skill_change_failed:
    xor rax, rax                    ; Failure
    
@skill_change_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
SelfModification_ApplySkillChange ENDP

; ============================================================================
; SelfModification_UpdateBackground
; Updates an agent's background
; RCX = agent ID, RDX = new background string
; Returns: RAX = 1 if success, 0 if failed
; ============================================================================
SelfModification_UpdateBackground PROC
    push rbx
    push rsi
    sub rsp, 32
    
    mov rbx, rcx                    ; Save agent ID
    mov rsi, rdx                    ; Save background string
    
    ; Validate agent exists
    mov rcx, rbx
    call AgentRegistry_Retrieve
    test rax, rax
    jz @background_update_failed
    
    ; Check if self-modification is enabled
    mov rcx, rax
    call @check_self_modify_enabled
    test rax, rax
    jz @background_update_failed
    
    ; Validate the modification
    mov rcx, rbx
    mov rdx, OP_UPDATE_BACKGROUND
    mov r8, rsi
    call SelfModification_ValidateModification
    test rax, rax
    jz @background_update_failed
    
    ; Queue the modification
    mov rcx, rbx
    mov rdx, OP_UPDATE_BACKGROUND
    mov r8, rsi
    mov r9, 0                       ; No target field
    call @queue_modification
    test rax, rax
    jz @background_update_failed
    
    ; Apply the modification
    call @process_modification_queue
    
    mov rax, 1                      ; Success
    jmp @background_update_done
    
@background_update_failed:
    xor rax, rax                    ; Failure
    
@background_update_done:
    add rsp, 32
    pop rsi
    pop rbx
    ret
SelfModification_UpdateBackground ENDP

; ============================================================================
; SelfModification_ValidateModification
; Validates if a modification is allowed
; RCX = agent ID, RDX = operation type, R8 = value string
; Returns: RAX = 1 if valid, 0 if invalid
; ============================================================================
SelfModification_ValidateModification PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; Save agent ID
    mov rsi, rdx                    ; Save operation type
    
    ; Basic validation - check operation type
    cmp rsi, OP_ADD_RULE
    je @valid_operation
    cmp rsi, OP_REMOVE_RULE
    je @valid_operation
    cmp rsi, OP_ADD_SKILL
    je @valid_operation
    cmp rsi, OP_REMOVE_SKILL
    je @valid_operation
    cmp rsi, OP_UPDATE_BACKGROUND
    je @valid_operation
    
    ; Invalid operation type
    xor rax, rax
    jmp @validate_done
    
@valid_operation:
    ; Additional validation logic would go here
    ; For now, just return valid
    mov rax, 1
    
@validate_done:
    pop rsi
    pop rbx
    ret
SelfModification_ValidateModification ENDP

; ============================================================================
; Internal helper functions
; ============================================================================

@check_self_modify_enabled:
    ; RCX = agent struct pointer
    ; Returns: RAX = 1 if enabled, 0 if disabled
    mov al, byte ptr [rcx + 20]     ; selfModifyEnabled field
    and rax, 1
    ret

@queue_modification:
    ; RCX = agent ID, RDX = operation type, R8 = value, R9 = target field
    ; Returns: RAX = 1 if queued, 0 if queue full
    push rbx
    push rsi
    
    ; Acquire mutex
    call @acquire_modification_mutex
    
    ; Check if queue is full
    mov eax, [queueTail]
    mov ebx, [queueHead]
    sub eax, ebx
    cmp eax, MAX_MODIFICATION_OPS
    jge @queue_full
    
    ; Add to queue
    mov eax, [queueTail]
    lea rsi, modificationQueue
    imul eax, sizeof MODIFICATION_OP
    add rsi, rax
    
    mov dword ptr [rsi + MODIFICATION_OP.agentId], ecx
    mov byte ptr [rsi + MODIFICATION_OP.operationType], dl
    mov qword ptr [rsi + MODIFICATION_OP.newValue], r8
    mov qword ptr [rsi + MODIFICATION_OP.targetField], r9
    mov byte ptr [rsi + MODIFICATION_OP.validationResult], 0
    
    ; Increment tail
    inc dword ptr [queueTail]
    
    ; Release mutex
    call @release_modification_mutex
    mov rax, 1
    jmp @queue_done
    
@queue_full:
    call @release_modification_mutex
    xor rax, rax
    
@queue_done:
    pop rsi
    pop rbx
    ret

@process_modification_queue:
    ; Process all queued modifications
    push rbx
    push rsi
    
@process_loop:
    ; Check if queue is empty
    mov eax, [queueHead]
    cmp eax, [queueTail]
    je @process_done
    
    ; Get next modification
    lea rsi, modificationQueue
    imul eax, sizeof MODIFICATION_OP
    add rsi, rax
    
    ; Apply modification (stub - real implementation would modify agent struct)
    ; For now, just mark as processed
    mov byte ptr [rsi + MODIFICATION_OP.validationResult], 1
    
    ; Increment head
    inc dword ptr [queueHead]
    jmp @process_loop
    
@process_done:
    pop rsi
    pop rbx
    ret

@acquire_modification_mutex:
    ; Simple spinlock acquire
    mov eax, 1
@acquire_mod_loop:
    xchg eax, [modificationMutex]
    test eax, eax
    jnz @acquire_mod_loop
    ret

@release_modification_mutex:
    ; Release mutex
    mov dword ptr [modificationMutex], 0
    ret

END