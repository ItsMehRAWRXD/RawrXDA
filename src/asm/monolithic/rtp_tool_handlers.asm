; =============================================================================
; rtp_tool_handlers.asm — x64 MASM — concrete RTP tool handlers (44)
; ABI: RCX=json_args, RDX=result_buf, R8=result_buf_size -> EAX status
; =============================================================================
OPTION CASEMAP:NONE

EXTERN Tool_Init    : PROC
EXTERN Tool_Execute : PROC

PUBLIC RawrXD_Tools_ReadFile
PUBLIC RawrXD_Tools_WriteFile
PUBLIC RawrXD_Tools_ReplaceInFile
PUBLIC RawrXD_Tools_ExecuteCommand
PUBLIC RawrXD_Tools_SearchCode
PUBLIC RawrXD_Tools_GetDiagnostics
PUBLIC RawrXD_Tools_ListDirectory
PUBLIC RawrXD_Tools_GetCoverage
PUBLIC RawrXD_Tools_RunBuild
PUBLIC RawrXD_Tools_ApplyHotpatch
PUBLIC RawrXD_Tools_DiskRecovery
PUBLIC RawrXD_Tools_Shell
PUBLIC RawrXD_Tools_PowerShell
PUBLIC RawrXD_Tools_WebSearch
PUBLIC RawrXD_Tools_GitStatus
PUBLIC RawrXD_Tools_TaskOrchestrator
PUBLIC RawrXD_Tools_RunSubagent
PUBLIC RawrXD_Tools_ManageTodoList
PUBLIC RawrXD_Tools_Chain
PUBLIC RawrXD_Tools_HexmagSwarm
PUBLIC RawrXD_Tools_GitDiff
PUBLIC RawrXD_Tools_GitLog
PUBLIC RawrXD_Tools_GitCommit
PUBLIC RawrXD_Tools_GitPush
PUBLIC RawrXD_Tools_GitPull
PUBLIC RawrXD_Tools_GitBranch
PUBLIC RawrXD_Tools_GitCheckout
PUBLIC RawrXD_Tools_GitMerge
PUBLIC RawrXD_Tools_GitStash
PUBLIC RawrXD_Tools_RunTests
PUBLIC RawrXD_Tools_AnalyzeCode
PUBLIC RawrXD_Tools_CompileBuild
PUBLIC RawrXD_Tools_CleanBuild
PUBLIC RawrXD_Tools_RunBenchmarks
PUBLIC RawrXD_Tools_GenerateCoverage
PUBLIC RawrXD_Tools_LintCode
PUBLIC RawrXD_Tools_FormatCode
PUBLIC RawrXD_Tools_RunProcess
PUBLIC RawrXD_Tools_RunShellScript
PUBLIC RawrXD_Tools_KillProcess
PUBLIC RawrXD_Tools_ProcessStatus
PUBLIC RawrXD_Tools_ExecuteWithTimeout
PUBLIC RawrXD_Tools_ListModels
PUBLIC RawrXD_Tools_LoadModel

.data
; Default JSON payloads for stage-2 real per-tool handlers.
; Handlers accept caller JSON when provided; otherwise use these contracts.
szCmdGitStatus      db '{"command":"git status --short --branch"}',0
szCmdGitDiff        db '{"command":"git diff --stat"}',0
szCmdGitLog         db '{"command":"git log --oneline -n 20"}',0
szCmdGitBranch      db '{"command":"git branch --all"}',0
szCmdRunTests       db '{"command":"ctest --output-on-failure"}',0
szCmdCompileBuild   db '{"command":"cmake --build . --config Release"}',0
szCmdLintCode       db '{"command":"clang-tidy --version"}',0
szCmdFormatCode     db '{"command":"clang-format --version"}',0
szCmdListModels     db '{"command":"ollama list"}',0
szCmdLoadModel      db '{"command":"ollama show phi4"}',0
szCmdTaskOrchestrator db '{"command":"python tools/task_orchestrator.py --status"}',0
szCmdRunSubagent    db '{"command":"python tools/subagent.py --run default"}',0
szCmdManageTodo     db '{"command":"python tools/todo.py --list"}',0
szCmdChain          db '{"command":"python tools/chain.py --dry-run"}',0
szCmdHexmagSwarm    db '{"command":"python tools/hexmag_swarm.py --status"}',0

.code

FORWARD_HANDLER MACRO fnName:req, legacyId:req
fnName PROC
    mov     r10, rcx
    mov     r11, rdx
    mov     r9,  r8
    call    Tool_Init
    mov     ecx, legacyId
    mov     rdx, r10
    mov     r8,  r11
    call    Tool_Execute
    ret
fnName ENDP
ENDM

; Native mappings (existing legacy handlers in agent_tools.asm)
FORWARD_HANDLER RawrXD_Tools_ReadFile,            0
FORWARD_HANDLER RawrXD_Tools_WriteFile,           1
FORWARD_HANDLER RawrXD_Tools_SearchCode,          4
FORWARD_HANDLER RawrXD_Tools_GetDiagnostics,      5
FORWARD_HANDLER RawrXD_Tools_ListDirectory,       2

; Semantic aliases
FORWARD_HANDLER RawrXD_Tools_ReplaceInFile,       1
FORWARD_HANDLER RawrXD_Tools_GetCoverage,         4

; Stage-1 split: family lanes (7..15) remove hard aliasing to legacy ID 3.
; Behavior remains backward-compatible because those lanes currently route
; to Tool_RunCommand in agent_tools.asm until per-tool contracts are added.
FORWARD_HANDLER RawrXD_Tools_ExecuteCommand,      3

; system / maintenance lane
FORWARD_HANDLER RawrXD_Tools_ApplyHotpatch,       15
FORWARD_HANDLER RawrXD_Tools_DiskRecovery,        15

; shell lane
FORWARD_HANDLER RawrXD_Tools_Shell,               11
FORWARD_HANDLER RawrXD_Tools_PowerShell,          11
FORWARD_HANDLER RawrXD_Tools_RunShellScript,      11

; web lane
FORWARD_HANDLER RawrXD_Tools_WebSearch,           14

; git lane
FORWARD_HANDLER RawrXD_Tools_GitCommit,           7
FORWARD_HANDLER RawrXD_Tools_GitPush,             7
FORWARD_HANDLER RawrXD_Tools_GitPull,             7
FORWARD_HANDLER RawrXD_Tools_GitCheckout,         7
FORWARD_HANDLER RawrXD_Tools_GitMerge,            7
FORWARD_HANDLER RawrXD_Tools_GitStash,            7

; build / static analysis lane
FORWARD_HANDLER RawrXD_Tools_RunBuild,            8
FORWARD_HANDLER RawrXD_Tools_CleanBuild,          8
FORWARD_HANDLER RawrXD_Tools_AnalyzeCode,         8

; test / benchmark lane
FORWARD_HANDLER RawrXD_Tools_RunBenchmarks,       9
FORWARD_HANDLER RawrXD_Tools_GenerateCoverage,    9

; process lane
FORWARD_HANDLER RawrXD_Tools_RunProcess,          10
FORWARD_HANDLER RawrXD_Tools_KillProcess,         10
FORWARD_HANDLER RawrXD_Tools_ProcessStatus,       10
FORWARD_HANDLER RawrXD_Tools_ExecuteWithTimeout,  10

; agent lane
; stage-3: all agent/subagent tools are explicit per-tool handlers

; model lane
; list/load model are now explicit stage-2 handlers

; -----------------------------------------------------------------------------
; Stage-2: true per-tool handlers (10 tools)
; -----------------------------------------------------------------------------

RawrXD_Tools_GitStatus PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 7
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdGitStatus
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_GitStatus ENDP

RawrXD_Tools_GitDiff PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 7
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdGitDiff
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_GitDiff ENDP

RawrXD_Tools_GitLog PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 7
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdGitLog
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_GitLog ENDP

RawrXD_Tools_GitBranch PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 7
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdGitBranch
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_GitBranch ENDP

RawrXD_Tools_RunTests PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 9
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdRunTests
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_RunTests ENDP

RawrXD_Tools_CompileBuild PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 8
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdCompileBuild
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_CompileBuild ENDP

RawrXD_Tools_LintCode PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 8
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdLintCode
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_LintCode ENDP

RawrXD_Tools_FormatCode PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 8
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdFormatCode
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_FormatCode ENDP

RawrXD_Tools_ListModels PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 13
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdListModels
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_ListModels ENDP

RawrXD_Tools_LoadModel PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 13
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdLoadModel
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_LoadModel ENDP

RawrXD_Tools_TaskOrchestrator PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 12
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdTaskOrchestrator
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_TaskOrchestrator ENDP

RawrXD_Tools_RunSubagent PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 12
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdRunSubagent
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_RunSubagent ENDP

RawrXD_Tools_ManageTodoList PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 12
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdManageTodo
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_ManageTodoList ENDP

RawrXD_Tools_Chain PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 12
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdChain
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_Chain ENDP

RawrXD_Tools_HexmagSwarm PROC
    push    rbx
    mov     r10, rcx
    mov     r11, rdx
    mov     rbx, r8
    call    Tool_Init
    mov     ecx, 12
    mov     rdx, r10
    test    rdx, rdx
    jz      @@use_default
    cmp     byte ptr [rdx], 0
    jne     @@dispatch
@@use_default:
    lea     rdx, szCmdHexmagSwarm
@@dispatch:
    mov     r8,  r11
    mov     r9,  rbx
    call    Tool_Execute
    pop     rbx
    ret
RawrXD_Tools_HexmagSwarm ENDP

END
