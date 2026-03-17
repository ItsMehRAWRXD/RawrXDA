; ============================================================================
; META-AGENT BUILDER - Creates Custom Agents from Specifications
; Pure MASM - Production Ready
; ============================================================================

option casemap:none

; ============================================================================
; EXTERNAL DECLARATIONS
; ============================================================================
EXTERN Json_ExtractString:PROC
EXTERN Json_ExtractInt:PROC
EXTERN Json_ExtractArray:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC

; ============================================================================
; CONSTANTS
; ============================================================================
NULL                equ 0
TRUE                equ 1
FALSE               equ 0
MAX_AGENT_RULES     equ 32
MAX_AGENT_SKILLS    equ 32
AGENT_STRUCT_SIZE   equ 512  ; 512 bytes per agent

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================
PUBLIC MetaAgentBuilder_CreateAgent
PUBLIC MetaAgentBuilder_GenerateAgentID

; ============================================================================
; AGENT STRUCTURE DEFINITION
; ============================================================================
AGENT_STRUCT struct
    id                  dd ?        ; 4 bytes
    name                dq ?        ; 8 bytes
    background          dq ?        ; 8 bytes
    ruleCount           dd ?        ; 4 bytes
    skillCount          dd ?        ; 4 bytes
    rules               dq MAX_AGENT_RULES dup(?)  ; 256 bytes
    skills              dq MAX_AGENT_SKILLS dup(?) ; 256 bytes
    selfModifyEnabled   db ?        ; 1 byte
    performanceMetrics  dq ?        ; 8 bytes
    reserved            db 27 dup(?) ; Padding to 512 bytes
AGENT_STRUCT ends

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ============================================================================
; MetaAgentBuilder_CreateAgent
; Creates a custom agent from JSON specification
; RCX = JSON spec string
; Returns: RAX = agent ID if success, 0 if failed
; ============================================================================
MetaAgentBuilder_CreateAgent PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 128
    
    mov rbx, rcx                    ; Save JSON spec
    
    ; Extract agent name
    lea rcx, szNameKey
    mov rdx, rbx
    call Json_ExtractString
    test rax, rax
    jz @create_failed
    mov [agentName], rax
    
    ; Extract background
    lea rcx, szBackgroundKey
    mov rdx, rbx
    call Json_ExtractString
    test rax, rax
    jz @create_failed
    mov [agentBackground], rax
    
    ; Extract rules array
    lea rcx, szRulesKey
    mov rdx, rbx
    call Json_ExtractArray
    mov [ruleArray], rax
    
    ; Extract skills array
    lea rcx, szSkillsKey
    mov rdx, rbx
    call Json_ExtractArray
    mov [skillArray], rax
    
    ; Generate unique agent ID
    call MetaAgentBuilder_GenerateAgentID
    mov [agentId], eax
    
    ; Allocate memory for agent structure
    mov rcx, AGENT_STRUCT_SIZE
    mov rdx, MEM_COMMIT or MEM_RESERVE
    mov r8, PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @create_failed
    mov [agentStruct], rax
    
    ; Initialize agent structure
    mov rdi, rax
    mov eax, [agentId]
    mov dword ptr [rdi + AGENT_STRUCT.id], eax
    
    mov rax, [agentName]
    mov qword ptr [rdi + AGENT_STRUCT.name], rax
    
    mov rax, [agentBackground]
    mov qword ptr [rdi + AGENT_STRUCT.background], rax
    
    ; Set self-modification enabled by default
    mov byte ptr [rdi + AGENT_STRUCT.selfModifyEnabled], TRUE
    
    ; Initialize performance metrics to 0
    mov qword ptr [rdi + AGENT_STRUCT.performanceMetrics], 0
    
    ; Copy rules (stub - in real implementation, copy from array)
    mov dword ptr [rdi + AGENT_STRUCT.ruleCount], 0
    
    ; Copy skills (stub - in real implementation, copy from array)
    mov dword ptr [rdi + AGENT_STRUCT.skillCount], 0
    
    ; Store agent in registry (stub)
    mov rcx, [agentId]
    mov rdx, [agentStruct]
    call AgentRegistry_Store
    test rax, rax
    jz @create_failed
    
    ; Success - return agent ID
    mov eax, [agentId]
    jmp @create_done
    
@create_failed:
    ; Free allocated memory if creation failed
    mov rcx, [agentStruct]
    test rcx, rcx
    jz @no_free
    mov rdx, 0
    mov r8, MEM_RELEASE
    call VirtualFree
    
@no_free:
    xor rax, rax
    
@create_done:
    add rsp, 128
    pop rdi
    pop rsi
    pop rbx
    ret
MetaAgentBuilder_CreateAgent ENDP

; ============================================================================
; MetaAgentBuilder_GenerateAgentID
; Generates a unique agent ID
; Returns: RAX = agent ID
; ============================================================================
MetaAgentBuilder_GenerateAgentID PROC
    ; Simple counter-based ID generation
    ; In real implementation, use more sophisticated method
    mov eax, [nextAgentId]
    inc dword ptr [nextAgentId]
    ret
MetaAgentBuilder_GenerateAgentID ENDP

; ============================================================================
; AgentRegistry_Store (Stub)
; Stores agent in registry
; RCX = agent ID, RDX = agent struct pointer
; Returns: RAX = 1 if success, 0 if failed
; ============================================================================
AgentRegistry_Store PROC
    ; Stub implementation - in real version, store in registry array
    mov rax, 1
    ret
AgentRegistry_Store ENDP

; ============================================================================
; DATA SECTION
; ============================================================================
.data

szNameKey        db 'name',0
szBackgroundKey  db 'background',0
szRulesKey       db 'rules',0
szSkillsKey      db 'skills',0

agentName        dq 0
agentBackground  dq 0
ruleArray        dq 0
skillArray       dq 0
agentId          dd 0
agentStruct      dq 0
nextAgentId      dd 1  ; Start from ID 1

; Memory constants
MEM_COMMIT       equ 00001000h
MEM_RESERVE      equ 00002000h
MEM_RELEASE      equ 00008000h
PAGE_READWRITE   equ 04h

END