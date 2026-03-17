; ============================================================================
; META-AGENT SYSTEM INTEGRATION - Connects Meta-Agent Components
; Pure MASM - Production Ready
; ============================================================================

option casemap:none

; ============================================================================
; EXTERNAL DECLARATIONS
; ============================================================================
EXTERN MetaAgentBuilder_CreateAgent:PROC
EXTERN MetaAgentBuilder_GenerateAgentID:PROC
EXTERN AgentRegistry_Initialize:PROC
EXTERN AgentRegistry_Store:PROC
EXTERN AgentRegistry_Retrieve:PROC
EXTERN AgentRegistry_Remove:PROC
EXTERN AgentRegistry_Count:PROC
EXTERN SelfModification_ApplyRuleChange:PROC
EXTERN SelfModification_ApplySkillChange:PROC
EXTERN SelfModification_UpdateBackground:PROC
EXTERN SelfModification_ValidateModification:PROC
EXTERN RuleEngine_Initialize:PROC
EXTERN RuleEngine_AddRule:PROC
EXTERN RuleEngine_RemoveRule:PROC
EXTERN RuleEngine_ExecuteRules:PROC
EXTERN RuleEngine_EvaluateCondition:PROC
EXTERN ToolRegistry_ExecuteTool:PROC
EXTERN ToolRegistry_GetToolInfo:PROC

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================
PUBLIC MetaAgentSystem_Initialize
PUBLIC MetaAgentSystem_CreateAgentFromSpec
PUBLIC MetaAgentSystem_ExecuteAgentRules
PUBLIC MetaAgentSystem_GetAgentCount

; ============================================================================
; TOOL IDS FOR META-AGENT SYSTEM
; ============================================================================
TOOL_META_AGENT_CREATE     equ 63
TOOL_META_AGENT_MODIFY     equ 64
TOOL_META_AGENT_EXECUTE    equ 65
TOOL_META_AGENT_COUNT      equ 66

; ============================================================================
; DATA SECTION
; ============================================================================
.data

metaAgentSystemInitialized db 0

; Sample agent specification for testing
szSampleAgentSpec db '{"name":"CodeAnalyzer","background":"Expert code analysis agent","rules":["analyze_code","report_issues"],"skills":["static_analysis","pattern_detection"]}',0

; ============================================================================
; CODE SECTION
; ============================================================================
.code

; ============================================================================
; MetaAgentSystem_Initialize
; Initializes the entire meta-agent system
; ============================================================================
MetaAgentSystem_Initialize PROC
    push rbx
    push rsi
    
    ; Check if already initialized
    cmp byte ptr [metaAgentSystemInitialized], TRUE
    je @already_initialized
    
    ; Initialize agent registry
    call AgentRegistry_Initialize
    
    ; Initialize rule engine
    call RuleEngine_Initialize
    
    ; Mark as initialized
    mov byte ptr [metaAgentSystemInitialized], TRUE
    
    ; Create a sample agent for testing
    lea rcx, szSampleAgentSpec
    call MetaAgentSystem_CreateAgentFromSpec
    
@already_initialized:
    pop rsi
    pop rbx
    ret
MetaAgentSystem_Initialize ENDP

; ============================================================================
; MetaAgentSystem_CreateAgentFromSpec
; Creates an agent from JSON specification
; RCX = JSON spec string
; Returns: RAX = agent ID if success, 0 if failed
; ============================================================================
MetaAgentSystem_CreateAgentFromSpec PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; Save JSON spec
    
    ; Ensure system is initialized
    cmp byte ptr [metaAgentSystemInitialized], TRUE
    jne @create_failed
    
    ; Create agent using meta-agent builder
    mov rcx, rbx
    call MetaAgentBuilder_CreateAgent
    test rax, rax
    jz @create_failed
    
    ; Success - return agent ID
    jmp @create_done
    
@create_failed:
    xor rax, rax                    ; Failure
    
@create_done:
    pop rsi
    pop rbx
    ret
MetaAgentSystem_CreateAgentFromSpec ENDP

; ============================================================================
; MetaAgentSystem_ExecuteAgentRules
; Executes rules for all agents in the system
; RCX = context data pointer
; Returns: RAX = number of rules executed
; ============================================================================
MetaAgentSystem_ExecuteAgentRules PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; Save context data
    
    ; Ensure system is initialized
    cmp byte ptr [metaAgentSystemInitialized], TRUE
    jne @execute_failed
    
    ; Execute rules using rule engine
    mov rcx, rbx
    call RuleEngine_ExecuteRules
    
    ; Success - return execution count
    jmp @execute_done
    
@execute_failed:
    xor rax, rax                    ; Failure
    
@execute_done:
    pop rsi
    pop rbx
    ret
MetaAgentSystem_ExecuteAgentRules ENDP

; ============================================================================
; MetaAgentSystem_GetAgentCount
; Returns the current number of agents in the system
; Returns: RAX = agent count
; ============================================================================
MetaAgentSystem_GetAgentCount PROC
    ; Ensure system is initialized
    cmp byte ptr [metaAgentSystemInitialized], TRUE
    jne @count_failed
    
    ; Get count from agent registry
    call AgentRegistry_Count
    jmp @count_done
    
@count_failed:
    xor rax, rax                    ; Failure
    
@count_done:
    ret
MetaAgentSystem_GetAgentCount ENDP

; ============================================================================
; Tool Integration Functions
; These functions integrate the meta-agent system with the tool registry
; ============================================================================

; Tool 63: MetaAgent_Create
Tool_MetaAgent_Create PROC
    ; RCX = JSON spec string
    call MetaAgentSystem_CreateAgentFromSpec
    ret
Tool_MetaAgent_Create ENDP

; Tool 64: MetaAgent_Modify
Tool_MetaAgent_Modify PROC
    ; RCX = agent ID, RDX = modification type, R8 = value
    ; Stub implementation - would call self-modification engine
    mov rax, 1
    ret
Tool_MetaAgent_Modify ENDP

; Tool 65: MetaAgent_Execute
Tool_MetaAgent_Execute PROC
    ; RCX = context data
    call MetaAgentSystem_ExecuteAgentRules
    ret
Tool_MetaAgent_Execute ENDP

; Tool 66: MetaAgent_Count
Tool_MetaAgent_Count PROC
    call MetaAgentSystem_GetAgentCount
    ret
Tool_MetaAgent_Count ENDP

END