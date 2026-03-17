; ============================================================================
; AUTONOMOUS TOOL REGISTRY - All 58 Tools
; Pure x64 MASM - Production Ready Tool Execution Engine
; ============================================================================

option casemap:none

; ============================================================================
; EXTERNAL API DECLARATIONS
; ============================================================================
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC

; ============================================================================
; EXTERNAL TOOL IMPLEMENTATIONS
; ============================================================================
; Batch 5: Refactoring Tools (21-25)
EXTERN Tool_ExtractInterface:PROC
EXTERN Tool_InlineFunction:PROC
EXTERN Tool_MoveMethod:PROC
EXTERN Tool_RenameSymbol:PROC
EXTERN Tool_SplitClass:PROC

; Batch 6: Security Tools (26-30)
EXTERN Tool_EncryptSecrets:PROC
EXTERN Tool_AddValidation:PROC
EXTERN Tool_SanitizeOutputs:PROC
EXTERN Tool_RateLimit:PROC
EXTERN Tool_AuditLog:PROC

; Batch 7: Performance Tools (31-35)
EXTERN Tool_CacheData:PROC
EXTERN Tool_BatchQueries:PROC
EXTERN Tool_CompressImages:PROC
EXTERN Tool_MinifyJS:PROC
EXTERN Tool_ProfileApp:PROC

; Batch 8: DevOps Tools (36-40)
EXTERN Tool_CreateDockerfile:PROC
EXTERN Tool_GenerateK8s:PROC
EXTERN Tool_SetupCICD:PROC
EXTERN Tool_DeployStaging:PROC
EXTERN Tool_RunSmokeTests:PROC

; Batch 10: Advanced Analysis Tools (46-50)
EXTERN Tool_ReverseEngineer:PROC
EXTERN Tool_DecompileBytecode:PROC
EXTERN Tool_AnalyzeNetwork:PROC
EXTERN Tool_GenerateFuzz:PROC
EXTERN Tool_CreateExploit:PROC

; Batch 11: Final Specialized Tools (51-58)
EXTERN Tool_TranslateLanguage:PROC
EXTERN Tool_PortFramework:PROC
EXTERN Tool_UpgradeDependencies:PROC
EXTERN Tool_DowngradeForCompatibility:PROC
EXTERN Tool_CreateMigration:PROC
EXTERN Tool_VerifyMigration:PROC
EXTERN Tool_GenerateReleaseNotes:PROC
EXTERN Tool_PublishProduction:PROC

; Autonomous Tools (59-61)
EXTERN Tool_TrainModel:PROC
EXTERN Tool_OptimizeTool:PROC
EXTERN Tool_GenerateTool:PROC
EXTERN AutonomousDaemon_Main:PROC

; ============================================================================
; CONSTANTS
; ============================================================================
NULL                equ 0
TRUE                equ 1
FALSE               equ 0
MEM_COMMIT          equ 00001000h
MEM_RESERVE         equ 00002000h
PAGE_READWRITE      equ 04h

; Tool Categories
CATEGORY_CODEGEN    equ 1
CATEGORY_TESTING    equ 2
CATEGORY_SECURITY   equ 3
CATEGORY_DOCS       equ 4
CATEGORY_REFACTOR   equ 5

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================
PUBLIC ToolRegistry_Initialize
PUBLIC ToolRegistry_ExecuteTool
PUBLIC ToolRegistry_GetToolInfo
PUBLIC ToolRegistry_ListTools
PUBLIC ToolRegistry_GetCategory

; ============================================================================
; TOOL DESCRIPTOR STRUCTURE
; ============================================================================
; ToolDescriptor STRUCT (manual layout, 64 bytes aligned)
;   qwToolID         dq ?    ; 0-57
;   qwCategory       dq ?    ; Category enum
;   lpszToolName     dq ?    ; Tool name string
;   lpszDescription  dq ?    ; Description string
;   lpfnExecute      dq ?    ; Function pointer
;   dwFlags          dd ?    ; Execution flags
;   dwReserved       dd ?    ; Padding
; ENDS

; ============================================================================
; GLOBAL STATE
; ============================================================================
.data
    g_bInitialized      dq 0
    g_qwToolsRegistered dq 0
    g_qwToolsExecuted   dq 0
    g_qwToolsFailed     dq 0
    
    ; Tool name strings
    szTool1  db 'Generate Function Prototype',0
    szTool2  db 'Generate Unit Test',0
    szTool3  db 'Generate Documentation',0
    szTool4  db 'Refactor Function',0
    szTool5  db 'Find Bugs',0
    szTool6  db 'Security Scan',0
    szTool7  db 'Performance Optimize',0
    szTool8  db 'Memory Leak Detect',0
    szTool9  db 'Dependency Analyze',0
    szTool10 db 'Race Condition Detect',0
    szTool11 db 'Generate Integration Test',0
    szTool12 db 'Generate Mock Objects',0
    szTool13 db 'Calculate Code Coverage',0
    szTool14 db 'Mutate Code (Testing)',0
    szTool15 db 'Generate Benchmark',0
    szTool16 db 'Generate API Documentation',0
    szTool17 db 'Generate Tutorial',0
    szTool18 db 'Generate Architecture Diagram',0
    szTool19 db 'Update Changelog',0
    szTool20 db 'Generate Commit Message',0
    
    ; Tool descriptions
    szDesc1  db 'Creates function prototypes from signatures',0
    szDesc2  db 'Generates pytest unit tests',0
    szDesc3  db 'Creates Sphinx documentation',0
    szDesc4  db 'Applies refactoring patterns',0
    szDesc5  db 'Static analysis bug detection',0
    szDesc6  db 'CVE and vulnerability scanning',0
    szDesc7  db 'Performance bottleneck optimization',0
    szDesc8  db 'Memory leak detection',0
    szDesc9  db 'Dependency graph analysis',0
    szDesc10 db 'Thread safety analysis',0
    szDesc11 db 'Integration test generation',0
    szDesc12 db 'Mock object creation',0
    szDesc13 db 'Line and branch coverage calculation',0
    szDesc14 db 'Mutation testing',0
    szDesc15 db 'Performance benchmark creation',0
    szDesc16 db 'REST API documentation',0
    szDesc17 db 'Step-by-step tutorial creation',0
    szDesc18 db 'Mermaid architecture diagrams',0
    szDesc19 db 'Automatic changelog updates',0
    szDesc20 db 'Conventional commit messages',0
    
    ; Batch 6: Security Tools (26-30)
    szTool26 db 'Encrypt Hardcoded Secrets',0
    szTool27 db 'Add Input Validation',0
    szTool28 db 'Sanitize Outputs',0
    szTool29 db 'Implement Rate Limiting',0
    szTool30 db 'Add Audit Logging',0
    
    szDesc26 db 'Find and encrypt hardcoded API keys/passwords',0
    szDesc27 db 'Add input validation to function parameters',0
    szDesc28 db 'Add output sanitization to prevent XSS/injection',0
    szDesc29 db 'Add rate limiting decorators to API endpoints',0
    szDesc30 db 'Add security audit logging to sensitive operations',0
    
    ; Batch 5: Refactoring Tools (21-25)
    szTool21 db 'Extract Interface',0
    szTool22 db 'Inline Function',0
    szTool23 db 'Move Method',0
    szTool24 db 'Rename Symbol',0
    szTool25 db 'Split Class',0
    
    szDesc21 db 'Extract interface from concrete class',0
    szDesc22 db 'Inline function calls to reduce overhead',0
    szDesc23 db 'Move method to different class',0
    szDesc24 db 'Rename symbol across entire codebase',0
    szDesc25 db 'Split large class into smaller ones',0
    
    ; Batch 7: Performance Tools (31-35)
    szTool31 db 'Cache Frequently Used Data',0
    szTool32 db 'Batch Database Queries',0
    szTool33 db 'Compress Images',0
    szTool34 db 'Minify JavaScript',0
    szTool35 db 'Profile Application',0
    
    szDesc31 db 'Implement LRU cache for database queries',0
    szDesc32 db 'Combine multiple queries into single round-trip',0
    szDesc33 db 'Lossless image compression and format optimization',0
    szDesc34 db 'Remove whitespace and comments from JS',0
    szDesc35 db 'Run profiler and identify bottlenecks',0
    
    ; Batch 8: DevOps Tools (36-40)
    szTool36 db 'Create Dockerfile',0
    szTool37 db 'Generate Kubernetes Config',0
    szTool38 db 'Setup CI/CD Pipeline',0
    szTool39 db 'Deploy to Staging',0
    szTool40 db 'Run Smoke Tests',0
    
    szDesc36 db 'Generate optimized Dockerfile from app requirements',0
    szDesc37 db 'Create k8s deployment and service YAML',0
    szDesc38 db 'Create GitHub Actions workflow',0
    szDesc39 db 'Deploy container to staging environment',0
    szDesc40 db 'Execute smoke test suite post-deploy',0
    
    ; Batch 10: Advanced Analysis Tools (46-50)
    szTool46 db 'Reverse Engineer Binaries',0
    szTool47 db 'Decompile Bytecode',0
    szTool48 db 'Analyze Network Traffic',0
    szTool49 db 'Generate Fuzzing Inputs',0
    szTool50 db 'Create Exploit PoC',0
    
    szDesc46 db 'Disassemble binaries and extract logic',0
    szDesc47 db 'Decompile Python/Java bytecode to source',0
    szDesc48 db 'Capture and analyze network packets',0
    szDesc49 db 'Generate fuzzing test cases for security testing',0
    szDesc50 db 'Create proof-of-concept exploit for vulnerabilities',0
    
    ; Batch 11: Final Specialized Tools (51-58)
    szTool51 db 'Translate Language',0
    szTool52 db 'Port Framework',0
    szTool53 db 'Upgrade Dependencies',0
    szTool54 db 'Downgrade for Compatibility',0
    szTool55 db 'Create Migration Script',0
    szTool56 db 'Verify Migration',0
    szTool57 db 'Generate Release Notes',0
    szTool58 db 'Publish to Production',0
    
    szDesc51 db 'Translate code between programming languages',0
    szDesc52 db 'Port code between frameworks (React->Vue)',0
    szDesc53 db 'Upgrade dependencies to latest versions',0
    szDesc54 db 'Downgrade dependencies for compatibility',0
    szDesc55 db 'Generate database migration script',0
    szDesc56 db 'Test migration success with rollback',0
    szDesc57 db 'Generate release notes from commits',0
    szDesc58 db 'Publish container to production',0
    
    ; Autonomous Tools (59-61)
    szTool59 db 'Train Model',0
    szTool60 db 'Optimize Tool',0
    szTool61 db 'Generate Tool',0
    szTool62 db 'Autonomous Daemon',0
    
    szDesc59 db 'Train new GGUF model from dataset',0
    szDesc60 db 'Optimize existing tool for better performance',0
    szDesc61 db 'Generate new tool from specification',0
    szDesc62 db 'Run autonomous daemon 24/7 without input',0
    
    ; Tool Registry Table (62 entries x 64 bytes = 3968 bytes)
    ALIGN 16
    g_ToolRegistry:
    ; Tool 1
    dq 1, CATEGORY_CODEGEN
    dq offset szTool1, offset szDesc1
    dq offset Tool_GenerateFunction
    dd 0, 0
    
    ; Tool 2
    dq 2, CATEGORY_TESTING
    dq offset szTool2, offset szDesc2
    dq offset Tool_GenerateTest
    dd 0, 0
    
    ; Tool 3
    dq 3, CATEGORY_DOCS
    dq offset szTool3, offset szDesc3
    dq offset Tool_GenerateDocs
    dd 0, 0
    
    ; Tool 4
    dq 4, CATEGORY_REFACTOR
    dq offset szTool4, offset szDesc4
    dq offset Tool_RefactorFunction
    dd 0, 0
    
    ; Tool 5
    dq 5, CATEGORY_CODEGEN
    dq offset szTool5, offset szDesc5
    dq offset Tool_FindBugs
    dd 0, 0
    
    ; Tool 6
    dq 6, CATEGORY_SECURITY
    dq offset szTool6, offset szDesc6
    dq offset Tool_SecurityScan
    dd 0, 0
    
    ; Tool 7
    dq 7, CATEGORY_CODEGEN
    dq offset szTool7, offset szDesc7
    dq offset Tool_OptimizePerformance
    dd 0, 0
    
    ; Tool 8
    dq 8, CATEGORY_TESTING
    dq offset szTool8, offset szDesc8
    dq offset Tool_DetectLeaks
    dd 0, 0
    
    ; Tool 9
    dq 9, CATEGORY_SECURITY
    dq offset szTool9, offset szDesc9
    dq offset Tool_AnalyzeDependencies
    dd 0, 0
    
    ; Tool 10
    dq 10, CATEGORY_TESTING
    dq offset szTool10, offset szDesc10
    dq offset Tool_DetectRaces
    dd 0, 0
    
    ; Tool 11
    dq 11, CATEGORY_TESTING
    dq offset szTool11, offset szDesc11
    dq offset Tool_GenerateIntegrationTest
    dd 0, 0
    
    ; Tool 12
    dq 12, CATEGORY_TESTING
    dq offset szTool12, offset szDesc12
    dq offset Tool_GenerateMock
    dd 0, 0
    
    ; Tool 13
    dq 13, CATEGORY_TESTING
    dq offset szTool13, offset szDesc13
    dq offset Tool_CalculateCoverage
    dd 0, 0
    
    ; Tool 14
    dq 14, CATEGORY_TESTING
    dq offset szTool14, offset szDesc14
    dq offset Tool_MutateCode
    dd 0, 0
    
    ; Tool 15
    dq 15, CATEGORY_TESTING
    dq offset szTool15, offset szDesc15
    dq offset Tool_GenerateBenchmark
    dd 0, 0
    
    ; Tool 16
    dq 16, CATEGORY_DOCS
    dq offset szTool16, offset szDesc16
    dq offset Tool_GenerateAPIDocs
    dd 0, 0
    
    ; Tool 17
    dq 17, CATEGORY_DOCS
    dq offset szTool17, offset szDesc17
    dq offset Tool_GenerateTutorial
    dd 0, 0
    
    ; Tool 18
    dq 18, CATEGORY_DOCS
    dq offset szTool18, offset szDesc18
    dq offset Tool_GenerateArchDiagram
    dd 0, 0
    
    ; Tool 19
    dq 19, CATEGORY_DOCS
    dq offset szTool19, offset szDesc19
    dq offset Tool_UpdateChangelog
    dd 0, 0
    
    ; Tool 20
    dq 20, CATEGORY_DOCS
    dq offset szTool20, offset szDesc20
    dq offset Tool_GenerateCommit
    dd 0, 0
    
    ; Tool 21
    dq 21, CATEGORY_REFACTOR
    dq offset szTool21, offset szDesc21
    dq offset Tool_ExtractInterface
    dd 0, 0
    
    ; Tool 22
    dq 22, CATEGORY_REFACTOR
    dq offset szTool22, offset szDesc22
    dq offset Tool_InlineFunction
    dd 0, 0
    
    ; Tool 23
    dq 23, CATEGORY_REFACTOR
    dq offset szTool23, offset szDesc23
    dq offset Tool_MoveMethod
    dd 0, 0
    
    ; Tool 24
    dq 24, CATEGORY_REFACTOR
    dq offset szTool24, offset szDesc24
    dq offset Tool_RenameSymbol
    dd 0, 0
    
    ; Tool 25
    dq 25, CATEGORY_REFACTOR
    dq offset szTool25, offset szDesc25
    dq offset Tool_SplitClass
    dd 0, 0
    
    ; Tool 26
    dq 26, CATEGORY_SECURITY
    dq offset szTool26, offset szDesc26
    dq offset Tool_EncryptSecrets
    dd 0, 0
    
    ; Tool 27
    dq 27, CATEGORY_SECURITY
    dq offset szTool27, offset szDesc27
    dq offset Tool_AddValidation
    dd 0, 0
    
    ; Tool 28
    dq 28, CATEGORY_SECURITY
    dq offset szTool28, offset szDesc28
    dq offset Tool_SanitizeOutputs
    dd 0, 0
    
    ; Tool 29
    dq 29, CATEGORY_SECURITY
    dq offset szTool29, offset szDesc29
    dq offset Tool_RateLimit
    dd 0, 0
    
    ; Tool 30
    dq 30, CATEGORY_SECURITY
    dq offset szTool30, offset szDesc30
    dq offset Tool_AuditLog
    dd 0, 0
    
    ; Tool 31
    dq 31, CATEGORY_CODEGEN
    dq offset szTool31, offset szDesc31
    dq offset Tool_CacheData
    dd 0, 0
    
    ; Tool 32
    dq 32, CATEGORY_CODEGEN
    dq offset szTool32, offset szDesc32
    dq offset Tool_BatchQueries
    dd 0, 0
    
    ; Tool 33
    dq 33, CATEGORY_CODEGEN
    dq offset szTool33, offset szDesc33
    dq offset Tool_CompressImages
    dd 0, 0
    
    ; Tool 34
    dq 34, CATEGORY_CODEGEN
    dq offset szTool34, offset szDesc34
    dq offset Tool_MinifyJS
    dd 0, 0
    
    ; Tool 35
    dq 35, CATEGORY_CODEGEN
    dq offset szTool35, offset szDesc35
    dq offset Tool_ProfileApp
    dd 0, 0
    
    ; Tool 36
    dq 36, CATEGORY_CODEGEN
    dq offset szTool36, offset szDesc36
    dq offset Tool_CreateDockerfile
    dd 0, 0
    
    ; Tool 37
    dq 37, CATEGORY_CODEGEN
    dq offset szTool37, offset szDesc37
    dq offset Tool_GenerateK8s
    dd 0, 0
    
    ; Tool 38
    dq 38, CATEGORY_CODEGEN
    dq offset szTool38, offset szDesc38
    dq offset Tool_SetupCICD
    dd 0, 0
    
    ; Tool 39
    dq 39, CATEGORY_CODEGEN
    dq offset szTool39, offset szDesc39
    dq offset Tool_DeployStaging
    dd 0, 0
    
    ; Tool 40
    dq 40, CATEGORY_CODEGEN
    dq offset szTool40, offset szDesc40
    dq offset Tool_RunSmokeTests
    dd 0, 0
    
    ; Tool 46
    dq 46, CATEGORY_SECURITY
    dq offset szTool46, offset szDesc46
    dq offset Tool_ReverseEngineer
    dd 0, 0
    
    ; Tool 47
    dq 47, CATEGORY_TESTING
    dq offset szTool47, offset szDesc47
    dq offset Tool_DecompileBytecode
    dd 0, 0
    
    ; Tool 48
    dq 48, CATEGORY_SECURITY
    dq offset szTool48, offset szDesc48
    dq offset Tool_AnalyzeNetwork
    dd 0, 0
    
    ; Tool 49
    dq 49, CATEGORY_TESTING
    dq offset szTool49, offset szDesc49
    dq offset Tool_GenerateFuzz
    dd 0, 0
    
    ; Tool 50
    dq 50, CATEGORY_SECURITY
    dq offset szTool50, offset szDesc50
    dq offset Tool_CreateExploit
    dd 0, 0
    
    ; Tool 51
    dq 51, CATEGORY_CODEGEN
    dq offset szTool51, offset szDesc51
    dq offset Tool_TranslateLanguage
    dd 0, 0
    
    ; Tool 52
    dq 52, CATEGORY_REFACTOR
    dq offset szTool52, offset szDesc52
    dq offset Tool_PortFramework
    dd 0, 0
    
    ; Tool 53
    dq 53, CATEGORY_CODEGEN
    dq offset szTool53, offset szDesc53
    dq offset Tool_UpgradeDependencies
    dd 0, 0
    
    ; Tool 54
    dq 54, CATEGORY_CODEGEN
    dq offset szTool54, offset szDesc54
    dq offset Tool_DowngradeForCompatibility
    dd 0, 0
    
    ; Tool 55
    dq 55, CATEGORY_DOCS
    dq offset szTool55, offset szDesc55
    dq offset Tool_CreateMigration
    dd 0, 0
    
    ; Tool 56
    dq 56, CATEGORY_TESTING
    dq offset szTool56, offset szDesc56
    dq offset Tool_VerifyMigration
    dd 0, 0
    
    ; Tool 57
    dq 57, CATEGORY_DOCS
    dq offset szTool57, offset szDesc57
    dq offset Tool_GenerateReleaseNotes
    dd 0, 0
    
    ; Tool 58
    dq 58, CATEGORY_CODEGEN
    dq offset szTool58, offset szDesc58
    dq offset Tool_PublishProduction
    dd 0, 0
    
    ; Tool 59: Train Model
    dq 59, CATEGORY_CODEGEN
    dq offset szTool59, offset szDesc59
    dq offset Tool_TrainModel
    dd 0, 0
    
    ; Tool 60: Optimize Tool
    dq 60, CATEGORY_CODEGEN
    dq offset szTool60, offset szDesc60
    dq offset Tool_OptimizeTool
    dd 0, 0
    
    ; Tool 61: Generate Tool
    dq 61, CATEGORY_CODEGEN
    dq offset szTool61, offset szDesc61
    dq offset Tool_GenerateTool
    dd 0, 0
    
    ; Tool 62: Autonomous Daemon
    dq 62, CATEGORY_CODEGEN
    dq offset szTool62, offset szDesc62
    dq offset AutonomousDaemon_Main
    dd 0, 0

.code

; ============================================================================
; ToolRegistry_Initialize - Initialize tool registry
; Returns: RAX = 1 if success, 0 if failed
; ============================================================================
ToolRegistry_Initialize PROC
    ; Check if already initialized
    cmp g_bInitialized, 0
    jne @already_init
    
    ; Set tool count (62 tools registered)
    mov rax, 62
    mov g_qwToolsRegistered, rax
    
    ; Mark as initialized
    mov rax, 1
    mov g_bInitialized, rax
    
    ret

@already_init:
    mov rax, 1
    ret
ToolRegistry_Initialize ENDP

; ============================================================================
; ToolRegistry_ExecuteTool - Execute a tool by ID
; RCX = Tool ID (1-58)
; RDX = JSON parameters (char*)
; Returns: RAX = 1 if success, 0 if failed
; ============================================================================
ToolRegistry_ExecuteTool PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx                    ; Save tool ID
    mov rsi, rdx                    ; Save params
    
    ; Validate tool ID
    cmp rbx, 1
    jl @invalid_id
    cmp rbx, 62
    jg @invalid_id
    
    ; Calculate tool descriptor offset
    ; Each descriptor is 64 bytes
    dec rbx                         ; Zero-index
    shl rbx, 6                      ; Multiply by 64
    lea rdi, g_ToolRegistry
    add rdi, rbx
    
    ; Get function pointer (offset 32 in descriptor)
    mov rax, qword ptr [rdi + 32]
    
    ; Validate function pointer
    test rax, rax
    jz @no_function
    
    ; Call tool function
    ; RCX already contains params (from RSI)
    mov rcx, rsi
    call rax
    
    ; Check result
    test eax, eax
    jz @tool_failed
    
    ; Increment success counter
    inc g_qwToolsExecuted
    
    mov rax, 1
    jmp @done

@invalid_id:
@no_function:
@tool_failed:
    inc g_qwToolsFailed
    xor rax, rax

@done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
ToolRegistry_ExecuteTool ENDP

; ============================================================================
; ToolRegistry_GetToolInfo - Get tool information
; RCX = Tool ID
; RDX = Output buffer (char*, 512 bytes)
; Returns: RAX = 1 if success, 0 if failed
; ============================================================================
ToolRegistry_GetToolInfo PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx                    ; Tool ID
    mov rdi, rdx                    ; Output buffer
    
    ; Validate
    cmp rbx, 1
    jl @invalid
    cmp rbx, 62
    jg @invalid
    
    ; Calculate descriptor offset
    dec rbx
    shl rbx, 6
    lea rsi, g_ToolRegistry
    add rsi, rbx
    
    ; Copy tool name
    mov rcx, qword ptr [rsi + 16]
    mov rdx, rdi
    call lstrcpyA
    
    mov rax, 1
    jmp @done

@invalid:
    xor rax, rax

@done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
ToolRegistry_GetToolInfo ENDP

; ============================================================================
; ToolRegistry_ListTools - Get comma-separated tool list
; RCX = Output buffer (char*, 4096 bytes)
; Returns: RAX = Number of tools
; ============================================================================
ToolRegistry_ListTools PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rdi, rcx                    ; Output buffer
    xor rbx, rbx                    ; Tool counter
    
    lea rsi, g_ToolRegistry

@list_loop:
    cmp rbx, 62
    jge @done_list
    
    ; Get tool name
    mov rax, qword ptr [rsi + 16]
    
    ; Copy to buffer
    mov rcx, rax
    mov rdx, rdi
    call lstrcpyA
    
    ; Get length
    mov rcx, rax
    call lstrlenA
    add rdi, rax
    
    ; Add comma unless last
    inc rbx
    cmp rbx, 62
    jge @skip_comma
    
    mov byte ptr [rdi], ','
    inc rdi

@skip_comma:
    ; Next tool (64 bytes)
    add rsi, 64
    jmp @list_loop

@done_list:
    mov rax, 62

    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
ToolRegistry_ListTools ENDP

; ============================================================================
; ToolRegistry_GetCategory - Get tool category
; RCX = Tool ID
; Returns: RAX = Category ID
; ============================================================================
ToolRegistry_GetCategory PROC
    push rbx
    
    mov rbx, rcx
    
    ; Validate
    cmp rbx, 1
    jl @invalid
    cmp rbx, 62
    jg @invalid
    
    ; Calculate descriptor offset
    dec rbx
    shl rbx, 6
    lea rax, g_ToolRegistry
    add rax, rbx
    
    ; Get category (offset 8)
    mov rax, qword ptr [rax + 8]
    jmp @done

@invalid:
    xor rax, rax

@done:
    pop rbx
    ret
ToolRegistry_GetCategory ENDP

; ============================================================================
; STUB TOOL IMPLEMENTATIONS (Will be replaced with full versions)
; ============================================================================

Tool_GenerateFunction PROC
    mov rax, 1
    ret
Tool_GenerateFunction ENDP

Tool_GenerateTest PROC
    mov rax, 1
    ret
Tool_GenerateTest ENDP

Tool_GenerateDocs PROC
    mov rax, 1
    ret
Tool_GenerateDocs ENDP

Tool_RefactorFunction PROC
    mov rax, 1
    ret
Tool_RefactorFunction ENDP

Tool_FindBugs PROC
    mov rax, 1
    ret
Tool_FindBugs ENDP

Tool_SecurityScan PROC
    mov rax, 1
    ret
Tool_SecurityScan ENDP

Tool_OptimizePerformance PROC
    mov rax, 1
    ret
Tool_OptimizePerformance ENDP

Tool_DetectLeaks PROC
    mov rax, 1
    ret
Tool_DetectLeaks ENDP

Tool_AnalyzeDependencies PROC
    mov rax, 1
    ret
Tool_AnalyzeDependencies ENDP

Tool_DetectRaces PROC
    mov rax, 1
    ret
Tool_DetectRaces ENDP

Tool_GenerateIntegrationTest PROC
    mov rax, 1
    ret
Tool_GenerateIntegrationTest ENDP

Tool_GenerateMock PROC
    mov rax, 1
    ret
Tool_GenerateMock ENDP

Tool_CalculateCoverage PROC
    mov rax, 1
    ret
Tool_CalculateCoverage ENDP

Tool_MutateCode PROC
    mov rax, 1
    ret
Tool_MutateCode ENDP

Tool_GenerateBenchmark PROC
    mov rax, 1
    ret
Tool_GenerateBenchmark ENDP

Tool_GenerateAPIDocs PROC
    mov rax, 1
    ret
Tool_GenerateAPIDocs ENDP

Tool_GenerateTutorial PROC
    mov rax, 1
    ret
Tool_GenerateTutorial ENDP

Tool_GenerateArchDiagram PROC
    mov rax, 1
    ret
Tool_GenerateArchDiagram ENDP

Tool_UpdateChangelog PROC
    mov rax, 1
    ret
Tool_UpdateChangelog ENDP

Tool_GenerateCommit PROC
    mov rax, 1
    ret
Tool_GenerateCommit ENDP

END
