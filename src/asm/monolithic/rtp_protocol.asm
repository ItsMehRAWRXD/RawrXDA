; =============================================================================
; rtp_protocol.asm  —  x64 MASM  —  RawrXD Tool Protocol (RTP) Batch 1
; Descriptor table + packet validator + dispatch bridge into Tool_Execute.
; =============================================================================
OPTION CASEMAP:NONE

EXTERN Tool_Init    : PROC
EXTERN Tool_Execute : PROC
EXTERN RawrXD_Tools_ReadFile:PROC
EXTERN RawrXD_Tools_WriteFile:PROC
EXTERN RawrXD_Tools_ReplaceInFile:PROC
EXTERN RawrXD_Tools_ExecuteCommand:PROC
EXTERN RawrXD_Tools_SearchCode:PROC
EXTERN RawrXD_Tools_GetDiagnostics:PROC
EXTERN RawrXD_Tools_ListDirectory:PROC
EXTERN RawrXD_Tools_GetCoverage:PROC
EXTERN RawrXD_Tools_RunBuild:PROC
EXTERN RawrXD_Tools_ApplyHotpatch:PROC
EXTERN RawrXD_Tools_DiskRecovery:PROC
EXTERN RawrXD_Tools_Shell:PROC
EXTERN RawrXD_Tools_PowerShell:PROC
EXTERN RawrXD_Tools_WebSearch:PROC
EXTERN RawrXD_Tools_GitStatus:PROC
EXTERN RawrXD_Tools_TaskOrchestrator:PROC
EXTERN RawrXD_Tools_RunSubagent:PROC
EXTERN RawrXD_Tools_ManageTodoList:PROC
EXTERN RawrXD_Tools_Chain:PROC
EXTERN RawrXD_Tools_HexmagSwarm:PROC
EXTERN RawrXD_Tools_GitDiff:PROC
EXTERN RawrXD_Tools_GitLog:PROC
EXTERN RawrXD_Tools_GitCommit:PROC
EXTERN RawrXD_Tools_GitPush:PROC
EXTERN RawrXD_Tools_GitPull:PROC
EXTERN RawrXD_Tools_GitBranch:PROC
EXTERN RawrXD_Tools_GitCheckout:PROC
EXTERN RawrXD_Tools_GitMerge:PROC
EXTERN RawrXD_Tools_GitStash:PROC
EXTERN RawrXD_Tools_RunTests:PROC
EXTERN RawrXD_Tools_AnalyzeCode:PROC
EXTERN RawrXD_Tools_CompileBuild:PROC
EXTERN RawrXD_Tools_CleanBuild:PROC
EXTERN RawrXD_Tools_RunBenchmarks:PROC
EXTERN RawrXD_Tools_GenerateCoverage:PROC
EXTERN RawrXD_Tools_LintCode:PROC
EXTERN RawrXD_Tools_FormatCode:PROC
EXTERN RawrXD_Tools_RunProcess:PROC
EXTERN RawrXD_Tools_RunShellScript:PROC
EXTERN RawrXD_Tools_KillProcess:PROC
EXTERN RawrXD_Tools_ProcessStatus:PROC
EXTERN RawrXD_Tools_ExecuteWithTimeout:PROC
EXTERN RawrXD_Tools_ListModels:PROC
EXTERN RawrXD_Tools_LoadModel:PROC

PUBLIC RTP_InitDescriptorTable
PUBLIC RTP_GetDescriptorTable
PUBLIC RTP_GetDescriptorCount
PUBLIC RTP_ValidatePacket
PUBLIC RTP_DispatchPacket
PUBLIC RTP_BuildContextBlob
PUBLIC RTP_GetContextBlobPtr
PUBLIC RTP_GetContextBlobSize
PUBLIC RTP_GetTelemetrySnapshot
PUBLIC RTP_GetLaneTelemetryPtr
PUBLIC RTP_GetToolTelemetryPtr
PUBLIC g_rtpPacketsValid
PUBLIC g_rtpDispatchOk
PUBLIC g_rtpDispatchErr
PUBLIC g_rtpAliasHits
PUBLIC g_rtpPolicyBlocks
PUBLIC g_rtpAgentLoopRounds
PUBLIC g_rtpLaneDispatchCounts
PUBLIC g_rtpLaneDispatchErr
PUBLIC g_rtpLanePolicyBlocks
PUBLIC g_rtpToolDispatchCounts
PUBLIC g_rtpToolDispatchErr
PUBLIC g_rtpToolPolicyBlocks

RTP_MAX_TOOLS           equ 44
RTP_DESCRIPTOR_SIZE     equ 64
RTP_PACKET_MAGIC        equ 021505452h ; 'RTP!'
RTP_PACKET_VERSION      equ 1
RTP_PACKET_HEADER_SIZE  equ 52
RTP_PAYLOAD_CAP         equ 4096
RTP_CONTEXT_MAGIC       equ 043505452h ; 'RTPC'
RTP_CONTEXT_VERSION     equ 1
RTP_CONTEXT_HEADER_SIZE equ 16
RTP_CONTEXT_SLOT_SIZE   equ 32
RTP_CONTEXT_CAP         equ 4096

; Packet header offsets
RTPP_magic              equ 0
RTPP_version            equ 4
RTPP_header_size        equ 6
RTPP_call_id            equ 8
RTPP_param_mask         equ 16
RTPP_payload_size       equ 24
RTPP_flags              equ 28
RTPP_tool_uuid          equ 32

; Descriptor offsets
RTPD_uuid               equ 0
RTPD_tool_id            equ 16
RTPD_legacy_tool_id     equ 20
RTPD_name_hash          equ 24
RTPD_name_ptr           equ 32
RTPD_desc_ptr           equ 40
RTPD_param_count        equ 48
RTPD_handler_rva        equ 56

; Validator return codes
RTP_OK                  equ 0
RTP_ERR_NULL            equ 1
RTP_ERR_SHORT           equ 2
RTP_ERR_MAGIC           equ 3
RTP_ERR_VERSION         equ 4
RTP_ERR_HEADER          equ 5
RTP_ERR_BOUNDS          equ 6
RTP_ERR_TOOL_NOT_FOUND  equ 20
RTP_ERR_TOOL_UNMAPPED   equ 21
RTP_ERR_PAYLOAD_CAP     equ 22
RTP_ERR_POLICY_BLOCK    equ 23

; Policy mask bits
RTP_POLICY_BLOCK_DEL            equ 00000001h
RTP_POLICY_BLOCK_FORMAT         equ 00000002h
RTP_POLICY_BLOCK_RM             equ 00000004h
RTP_POLICY_BLOCK_RM_RF          equ 00000008h
RTP_POLICY_BLOCK_GIT_RESET_HARD equ 00000010h

.data
ALIGN 4
g_rtpInitialized        dd 0

ALIGN 8
g_rtpDescriptors        db RTP_MAX_TOOLS * RTP_DESCRIPTOR_SIZE dup(0)
g_rtpPayloadScratch     db RTP_PAYLOAD_CAP dup(0)
g_rtpContextBlob        db RTP_CONTEXT_CAP dup(0)
g_rtpContextSize        dd 0
align 8
g_rtpPacketsValid       dq 0
g_rtpDispatchOk         dq 0
g_rtpDispatchErr        dq 0
g_rtpAliasHits          dq 0
g_rtpPolicyBlocks       dq 0
g_rtpAgentLoopRounds    dq 0
g_rtpLaneDispatchCounts dq 16 dup(0)
g_rtpLaneDispatchErr    dq 16 dup(0)
g_rtpLanePolicyBlocks   dq 16 dup(0)
g_rtpToolDispatchCounts dq RTP_MAX_TOOLS dup(0)
g_rtpToolDispatchErr    dq RTP_MAX_TOOLS dup(0)
g_rtpToolPolicyBlocks   dq RTP_MAX_TOOLS dup(0)

szPolicyDel             db "del ",0
szPolicyFormat          db "format ",0
szPolicyRm              db " rm ",0
szPolicyRmRf            db "rm -rf",0
szPolicyGitResetHard    db "reset --hard",0

; 44-tool sovereign descriptor metadata (stable order)
szTool00Name            db "read_file",0
szTool01Name            db "write_file",0
szTool02Name            db "replace_in_file",0
szTool03Name            db "execute_command",0
szTool04Name            db "search_code",0
szTool05Name            db "get_diagnostics",0
szTool06Name            db "list_directory",0
szTool07Name            db "get_coverage",0
szTool08Name            db "run_build",0
szTool09Name            db "apply_hotpatch",0
szTool10Name            db "disk_recovery",0
szTool11Name            db "shell",0
szTool12Name            db "powershell",0
szTool13Name            db "web_search",0
szTool14Name            db "git_status",0
szTool15Name            db "task_orchestrator",0
szTool16Name            db "runSubagent",0
szTool17Name            db "manage_todo_list",0
szTool18Name            db "chain",0
szTool19Name            db "hexmag_swarm",0
szTool20Name            db "git_diff",0
szTool21Name            db "git_log",0
szTool22Name            db "git_commit",0
szTool23Name            db "git_push",0
szTool24Name            db "git_pull",0
szTool25Name            db "git_branch",0
szTool26Name            db "git_checkout",0
szTool27Name            db "git_merge",0
szTool28Name            db "git_stash",0
szTool29Name            db "run_tests",0
szTool30Name            db "analyze_code",0
szTool31Name            db "compile_build",0
szTool32Name            db "clean_build",0
szTool33Name            db "run_benchmarks",0
szTool34Name            db "generate_coverage",0
szTool35Name            db "lint_code",0
szTool36Name            db "format_code",0
szTool37Name            db "run_process",0
szTool38Name            db "run_shell_script",0
szTool39Name            db "kill_process",0
szTool40Name            db "process_status",0
szTool41Name            db "execute_with_timeout",0
szTool42Name            db "list_models",0
szTool43Name            db "load_model",0

szTool00Desc            db "Read file content",0
szTool01Desc            db "Write file content",0
szTool02Desc            db "Replace exact text in file",0
szTool03Desc            db "Execute command and capture output",0
szTool04Desc            db "Search codebase",0
szTool05Desc            db "Get diagnostics",0
szTool06Desc            db "List directory entries",0
szTool07Desc            db "Get coverage data",0
szTool08Desc            db "Run project build",0
szTool09Desc            db "Apply runtime hotpatch",0
szTool10Desc            db "Operate disk recovery agent",0
szTool11Desc            db "Run shell command",0
szTool12Desc            db "Run powershell command",0
szTool13Desc            db "Search web resources",0
szTool14Desc            db "Show git status",0
szTool15Desc            db "Run task orchestration",0
szTool16Desc            db "Run subagent",0
szTool17Desc            db "Manage todo list",0
szTool18Desc            db "Run chained actions",0
szTool19Desc            db "Run HexMag swarm action",0
szTool20Desc            db "Show git diff",0
szTool21Desc            db "Show git log",0
szTool22Desc            db "Commit changes",0
szTool23Desc            db "Push changes",0
szTool24Desc            db "Pull changes",0
szTool25Desc            db "List/create branches",0
szTool26Desc            db "Checkout branch",0
szTool27Desc            db "Merge branch",0
szTool28Desc            db "Stash changes",0
szTool29Desc            db "Run tests",0
szTool30Desc            db "Analyze code quality",0
szTool31Desc            db "Compile build target",0
szTool32Desc            db "Clean build outputs",0
szTool33Desc            db "Run benchmark suite",0
szTool34Desc            db "Generate coverage report",0
szTool35Desc            db "Run linter",0
szTool36Desc            db "Run formatter",0
szTool37Desc            db "Run external process",0
szTool38Desc            db "Run shell script",0
szTool39Desc            db "Terminate process",0
szTool40Desc            db "Get process status",0
szTool41Desc            db "Execute with timeout",0
szTool42Desc            db "List available models",0
szTool43Desc            db "Load model",0

align 4
g_rtpLegacyMap          dd 0,1,0FFFFFFFFh,3,4,5,2,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh
                        dd 0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh
                        dd 0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh
                        dd 0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh
                        dd 0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh
                        dd 0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh
                        dd 0FFFFFFFFh,0FFFFFFFFh,0FFFFFFFFh

g_rtpParamCounts        db 1,2,3,2,2,1,1,2,2,3,2,1,1,1,0,1,1,1,1,1,1,0,2,0,0,1,1,1,0,1,1,2,1,0,1,1,1,1,1,1,1,2,0,1

; Lane ID per tool slot (0..43). Used for policy and telemetry.
; 0=file 1=command 2=code 3=build 4=git 5=test 6=process 7=agent 8=system 9=web 10=model
g_rtpToolLaneMap        db 0,0,0,1,2,2,0,2,3,8,8,1,1,9,4,7,7,7,7,7,4,4,4,4,4,4,4,4,4,5,3,3,3,5,5,3,3,6,1,6,6,6,10,10

; Policy masks per lane (16 lanes available).
g_rtpLanePolicyMask     dd 0
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF ; lane 1 command
                        dd 0 ; lane 2
                        dd 0 ; lane 3
                        dd RTP_POLICY_BLOCK_GIT_RESET_HARD ; lane 4 git
                        dd 0 ; lane 5
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF ; lane 6 process
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF ; lane 7 agent
                        dd 0 ; lane 8
                        dd 0 ; lane 9
                        dd 0 ; lane 10
                        dd 0,0,0,0,0

; Per-tool policy mask override (OR-ed with lane policy mask).
; Only high-risk command tools are gated individually.
g_rtpToolPolicyMask     dd 0,0,0,RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF
                        dd 0,0,0,0,0,0,0
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF ; shell
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF ; powershell
                        dd 0 ; web_search
                        dd 0 ; git_status
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF ; agent tools 15..19
                        dd 0,0,0,0,0,0,0,RTP_POLICY_BLOCK_GIT_RESET_HARD,0 ; git family (merge guarded)
                        dd 0,0,0,0,0,0,0,0 ; tests/build/lint/format
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF
                        dd RTP_POLICY_BLOCK_DEL or RTP_POLICY_BLOCK_FORMAT or RTP_POLICY_BLOCK_RM or RTP_POLICY_BLOCK_RM_RF
                        dd 0,0,0

align 8
g_rtpNamePtrTable       dq OFFSET szTool00Name, OFFSET szTool01Name, OFFSET szTool02Name, OFFSET szTool03Name
                        dq OFFSET szTool04Name, OFFSET szTool05Name, OFFSET szTool06Name, OFFSET szTool07Name
                        dq OFFSET szTool08Name, OFFSET szTool09Name, OFFSET szTool10Name, OFFSET szTool11Name
                        dq OFFSET szTool12Name, OFFSET szTool13Name, OFFSET szTool14Name, OFFSET szTool15Name
                        dq OFFSET szTool16Name, OFFSET szTool17Name, OFFSET szTool18Name, OFFSET szTool19Name
                        dq OFFSET szTool20Name, OFFSET szTool21Name, OFFSET szTool22Name, OFFSET szTool23Name
                        dq OFFSET szTool24Name, OFFSET szTool25Name, OFFSET szTool26Name, OFFSET szTool27Name
                        dq OFFSET szTool28Name, OFFSET szTool29Name, OFFSET szTool30Name, OFFSET szTool31Name
                        dq OFFSET szTool32Name, OFFSET szTool33Name, OFFSET szTool34Name, OFFSET szTool35Name
                        dq OFFSET szTool36Name, OFFSET szTool37Name, OFFSET szTool38Name, OFFSET szTool39Name
                        dq OFFSET szTool40Name, OFFSET szTool41Name, OFFSET szTool42Name, OFFSET szTool43Name

g_rtpDescPtrTable       dq OFFSET szTool00Desc, OFFSET szTool01Desc, OFFSET szTool02Desc, OFFSET szTool03Desc
                        dq OFFSET szTool04Desc, OFFSET szTool05Desc, OFFSET szTool06Desc, OFFSET szTool07Desc
                        dq OFFSET szTool08Desc, OFFSET szTool09Desc, OFFSET szTool10Desc, OFFSET szTool11Desc
                        dq OFFSET szTool12Desc, OFFSET szTool13Desc, OFFSET szTool14Desc, OFFSET szTool15Desc
                        dq OFFSET szTool16Desc, OFFSET szTool17Desc, OFFSET szTool18Desc, OFFSET szTool19Desc
                        dq OFFSET szTool20Desc, OFFSET szTool21Desc, OFFSET szTool22Desc, OFFSET szTool23Desc
                        dq OFFSET szTool24Desc, OFFSET szTool25Desc, OFFSET szTool26Desc, OFFSET szTool27Desc
                        dq OFFSET szTool28Desc, OFFSET szTool29Desc, OFFSET szTool30Desc, OFFSET szTool31Desc
                        dq OFFSET szTool32Desc, OFFSET szTool33Desc, OFFSET szTool34Desc, OFFSET szTool35Desc
                        dq OFFSET szTool36Desc, OFFSET szTool37Desc, OFFSET szTool38Desc, OFFSET szTool39Desc
                        dq OFFSET szTool40Desc, OFFSET szTool41Desc, OFFSET szTool42Desc, OFFSET szTool43Desc

g_rtpHandlerPtrTable    dq OFFSET RawrXD_Tools_ReadFile,       OFFSET RawrXD_Tools_WriteFile
                        dq OFFSET RawrXD_Tools_ReplaceInFile,  OFFSET RawrXD_Tools_ExecuteCommand
                        dq OFFSET RawrXD_Tools_SearchCode,     OFFSET RawrXD_Tools_GetDiagnostics
                        dq OFFSET RawrXD_Tools_ListDirectory,  OFFSET RawrXD_Tools_GetCoverage
                        dq OFFSET RawrXD_Tools_RunBuild,       OFFSET RawrXD_Tools_ApplyHotpatch
                        dq OFFSET RawrXD_Tools_DiskRecovery,   OFFSET RawrXD_Tools_Shell
                        dq OFFSET RawrXD_Tools_PowerShell,     OFFSET RawrXD_Tools_WebSearch
                        dq OFFSET RawrXD_Tools_GitStatus,      OFFSET RawrXD_Tools_TaskOrchestrator
                        dq OFFSET RawrXD_Tools_RunSubagent,    OFFSET RawrXD_Tools_ManageTodoList
                        dq OFFSET RawrXD_Tools_Chain,          OFFSET RawrXD_Tools_HexmagSwarm
                        dq OFFSET RawrXD_Tools_GitDiff,        OFFSET RawrXD_Tools_GitLog
                        dq OFFSET RawrXD_Tools_GitCommit,      OFFSET RawrXD_Tools_GitPush
                        dq OFFSET RawrXD_Tools_GitPull,        OFFSET RawrXD_Tools_GitBranch
                        dq OFFSET RawrXD_Tools_GitCheckout,    OFFSET RawrXD_Tools_GitMerge
                        dq OFFSET RawrXD_Tools_GitStash,       OFFSET RawrXD_Tools_RunTests
                        dq OFFSET RawrXD_Tools_AnalyzeCode,    OFFSET RawrXD_Tools_CompileBuild
                        dq OFFSET RawrXD_Tools_CleanBuild,     OFFSET RawrXD_Tools_RunBenchmarks
                        dq OFFSET RawrXD_Tools_GenerateCoverage, OFFSET RawrXD_Tools_LintCode
                        dq OFFSET RawrXD_Tools_FormatCode,     OFFSET RawrXD_Tools_RunProcess
                        dq OFFSET RawrXD_Tools_RunShellScript, OFFSET RawrXD_Tools_KillProcess
                        dq OFFSET RawrXD_Tools_ProcessStatus,  OFFSET RawrXD_Tools_ExecuteWithTimeout
                        dq OFFSET RawrXD_Tools_ListModels,     OFFSET RawrXD_Tools_LoadModel

.code

_RtpFindDescriptorByUuid PROC
    ; RCX = ptr to 16-byte uuid
    ; RAX = descriptor ptr or 0
    xor     r10d, r10d
    lea     r11, [g_rtpDescriptors]
@@loop:
    cmp     r10d, RTP_MAX_TOOLS
    jae     @@not_found
    mov     rax, qword ptr [r11 + RTPD_uuid]
    mov     rdx, qword ptr [rcx]
    cmp     rax, rdx
    jne     @@next
    mov     rax, qword ptr [r11 + RTPD_uuid + 8]
    mov     rdx, qword ptr [rcx + 8]
    cmp     rax, rdx
    jne     @@next
    mov     rax, r11
    ret
@@next:
    add     r11, RTP_DESCRIPTOR_SIZE
    inc     r10d
    jmp     @@loop
@@not_found:
    xor     rax, rax
    ret
_RtpFindDescriptorByUuid ENDP

_RtpFnv1a64 PROC
    ; RCX = ptr to null-terminated ascii string
    ; RAX = 64-bit FNV-1a hash
    mov     rax, 0CBF29CE484222325h
@@hash_loop:
    movzx   edx, byte ptr [rcx]
    test    dl, dl
    jz      @@done
    xor     rax, rdx
    mov     r8, 0100000001B3h
    imul    rax, r8
    inc     rcx
    jmp     @@hash_loop
@@done:
    ret
_RtpFnv1a64 ENDP

_RtpContainsSubstr PROC
    ; RCX = haystack (null-terminated), RDX = needle (null-terminated)
    ; EAX = 1 found, 0 not found
    push    rbx
    push    rsi
    push    rdi
    mov     rsi, rcx
    mov     rdi, rdx
@@outer:
    movzx   eax, byte ptr [rsi]
    test    al, al
    jz      @@not_found
    mov     rbx, rsi
    mov     rdi, rdx
@@inner:
    movzx   eax, byte ptr [rdi]
    test    al, al
    jz      @@found
    movzx   ecx, byte ptr [rbx]
    test    cl, cl
    jz      @@next
    cmp     cl, al
    jne     @@next
    inc     rbx
    inc     rdi
    jmp     @@inner
@@next:
    inc     rsi
    jmp     @@outer
@@found:
    mov     eax, 1
    pop     rdi
    pop     rsi
    pop     rbx
    ret
@@not_found:
    xor     eax, eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
_RtpContainsSubstr ENDP

_RtpEvaluatePolicy PROC
    ; RCX = payload json ptr, EDX = effective policy mask
    ; EAX = 1 blocked, 0 allowed
    push    rbx
    mov     rbx, rcx

    test    edx, RTP_POLICY_BLOCK_DEL
    jz      @@chk_format
    mov     rcx, rbx
    lea     r8, [szPolicyDel]
    mov     rdx, r8
    call    _RtpContainsSubstr
    test    eax, eax
    jnz     @@blocked

@@chk_format:
    test    edx, RTP_POLICY_BLOCK_FORMAT
    jz      @@chk_rm
    mov     rcx, rbx
    lea     r8, [szPolicyFormat]
    mov     rdx, r8
    call    _RtpContainsSubstr
    test    eax, eax
    jnz     @@blocked

@@chk_rm:
    test    edx, RTP_POLICY_BLOCK_RM
    jz      @@chk_rmrf
    mov     rcx, rbx
    lea     r8, [szPolicyRm]
    mov     rdx, r8
    call    _RtpContainsSubstr
    test    eax, eax
    jnz     @@blocked

@@chk_rmrf:
    test    edx, RTP_POLICY_BLOCK_RM_RF
    jz      @@chk_git_hard
    mov     rcx, rbx
    lea     r8, [szPolicyRmRf]
    mov     rdx, r8
    call    _RtpContainsSubstr
    test    eax, eax
    jnz     @@blocked

@@chk_git_hard:
    test    edx, RTP_POLICY_BLOCK_GIT_RESET_HARD
    jz      @@allow
    mov     rcx, rbx
    lea     r8, [szPolicyGitResetHard]
    mov     rdx, r8
    call    _RtpContainsSubstr
    test    eax, eax
    jnz     @@blocked

@@allow:
    xor     eax, eax
    pop     rbx
    ret
@@blocked:
    mov     eax, 1
    pop     rbx
    ret
_RtpEvaluatePolicy ENDP

_RtpResolveLegacyAlias PROC
    ; RCX = descriptor ptr
    ; EAX = legacy tool id, or 0FFFFFFFFh if no alias
    mov     eax, dword ptr [rcx + RTPD_tool_id]

    ; Direct semantic aliases
    cmp     eax, 2                       ; replace_in_file -> write_file
    je      @@alias_write
    cmp     eax, 7                       ; get_coverage -> search_code
    je      @@alias_search
    cmp     eax, 6                       ; list_directory -> list_dir
    je      @@alias_list

    ; Command-execution family aliases
    ; run_build/apply_hotpatch/disk_recovery + shell/powershell/web + git +
    ; orchestration + test/build/terminal/process/model controls.
    cmp     eax, 8
    jb      @@no_alias
    cmp     eax, 43
    jbe     @@alias_command

@@no_alias:
    mov     eax, 0FFFFFFFFh
    ret
@@alias_write:
    mov     eax, 1
    ret
@@alias_search:
    mov     eax, 4
    ret
@@alias_list:
    mov     eax, 2
    ret
@@alias_command:
    mov     eax, 3
    ret
_RtpResolveLegacyAlias ENDP

RTP_InitDescriptorTable PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    cmp     dword ptr [g_rtpInitialized], 1
    je      @@done

    ; Initialize all 44 descriptors with deterministic metadata.
    lea     rbx, [g_rtpDescriptors]
    xor     esi, esi
@@init_loop:
    cmp     esi, RTP_MAX_TOOLS
    jae     @@init_done

    ; UUID: first half = magic + slot, second half = FNV(name)
    mov     dword ptr [rbx + RTPD_uuid + 0], RTP_PACKET_MAGIC
    mov     byte ptr  [rbx + RTPD_uuid + 4], sil

    mov     dword ptr [rbx + RTPD_tool_id], esi
    mov     eax, dword ptr [g_rtpLegacyMap + rsi*4]
    mov     dword ptr [rbx + RTPD_legacy_tool_id], eax

    mov     rax, qword ptr [g_rtpNamePtrTable + rsi*8]
    mov     qword ptr [rbx + RTPD_name_ptr], rax
    mov     rcx, rax
    call    _RtpFnv1a64
    mov     qword ptr [rbx + RTPD_name_hash], rax
    mov     qword ptr [rbx + RTPD_uuid + 8], rax

    mov     rax, qword ptr [g_rtpDescPtrTable + rsi*8]
    mov     qword ptr [rbx + RTPD_desc_ptr], rax

    movzx   eax, byte ptr [g_rtpParamCounts + rsi]
    mov     byte ptr [rbx + RTPD_param_count], al
    mov     rax, qword ptr [g_rtpHandlerPtrTable + rsi*8]
    mov     qword ptr [rbx + RTPD_handler_rva], rax

    add     rbx, RTP_DESCRIPTOR_SIZE
    inc     esi
    jmp     @@init_loop

@@init_done:
    mov     dword ptr [g_rtpInitialized], 1
@@done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RTP_InitDescriptorTable ENDP

RTP_GetDescriptorTable PROC
    call    RTP_InitDescriptorTable
    lea     rax, [g_rtpDescriptors]
    ret
RTP_GetDescriptorTable ENDP

RTP_GetDescriptorCount PROC
    mov     eax, RTP_MAX_TOOLS
    ret
RTP_GetDescriptorCount ENDP

RTP_GetContextBlobPtr PROC
    lea     rax, [g_rtpContextBlob]
    ret
RTP_GetContextBlobPtr ENDP

RTP_GetContextBlobSize PROC
    mov     eax, dword ptr [g_rtpContextSize]
    ret
RTP_GetContextBlobSize ENDP

RTP_GetTelemetrySnapshot PROC
    ; RAX = ptr to telemetry block:
    ; base counters + lane arrays + tool arrays (contiguous from g_rtpPacketsValid)
    lea     rax, [g_rtpPacketsValid]
    ret
RTP_GetTelemetrySnapshot ENDP

RTP_GetLaneTelemetryPtr PROC
    ; RAX = ptr to lane telemetry arrays (counts/errs/policy-blocks)
    lea     rax, [g_rtpLaneDispatchCounts]
    ret
RTP_GetLaneTelemetryPtr ENDP

RTP_GetToolTelemetryPtr PROC
    ; RAX = ptr to tool telemetry arrays (counts/errs/policy-blocks)
    lea     rax, [g_rtpToolDispatchCounts]
    ret
RTP_GetToolTelemetryPtr ENDP

RTP_BuildContextBlob PROC FRAME
    ; RCX = out_buf (optional; NULL => internal), EDX = out_cap, R8 = out_written (optional)
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    .endprolog

    call    RTP_InitDescriptorTable

    mov     rbx, rcx                    ; out_buf
    mov     r12d, edx                   ; out_cap
    test    rbx, rbx
    jnz     @@have_out
    lea     rbx, [g_rtpContextBlob]
    mov     r12d, RTP_CONTEXT_CAP
@@have_out:

    mov     eax, RTP_CONTEXT_HEADER_SIZE
    mov     ecx, RTP_MAX_TOOLS
    imul    ecx, RTP_CONTEXT_SLOT_SIZE
    add     eax, ecx                    ; total bytes needed
    cmp     eax, r12d
    ja      @@short

    ; Header
    mov     dword ptr [rbx + 0], RTP_CONTEXT_MAGIC
    mov     word ptr  [rbx + 4], RTP_CONTEXT_VERSION
    mov     word ptr  [rbx + 6], RTP_CONTEXT_HEADER_SIZE
    mov     word ptr  [rbx + 8], RTP_MAX_TOOLS
    mov     word ptr  [rbx + 10], RTP_CONTEXT_SLOT_SIZE
    mov     dword ptr [rbx + 12], 0

    ; Slots: [uuid16][name_hash8][param_count1][flags1][legacy_id2][reserved4]
    lea     rsi, [g_rtpDescriptors]
    lea     rdi, [rbx + RTP_CONTEXT_HEADER_SIZE]
    xor     ecx, ecx
@@slot_loop:
    cmp     ecx, RTP_MAX_TOOLS
    jae     @@done

    mov     rax, qword ptr [rsi + RTPD_uuid]
    mov     qword ptr [rdi + 0], rax
    mov     rax, qword ptr [rsi + RTPD_uuid + 8]
    mov     qword ptr [rdi + 8], rax
    mov     rax, qword ptr [rsi + RTPD_name_hash]
    mov     qword ptr [rdi + 16], rax
    mov     al, byte ptr [rsi + RTPD_param_count]
    mov     byte ptr [rdi + 24], al
    mov     byte ptr [rdi + 25], 0
    mov     ax, word ptr [rsi + RTPD_legacy_tool_id]
    mov     word ptr [rdi + 26], ax
    mov     dword ptr [rdi + 28], 0

    add     rsi, RTP_DESCRIPTOR_SIZE
    add     rdi, RTP_CONTEXT_SLOT_SIZE
    inc     ecx
    jmp     @@slot_loop

@@done:
    ; record/context size
    mov     dword ptr [g_rtpContextSize], eax
    test    r8, r8
    jz      @@ok
    mov     dword ptr [r8], eax
@@ok:
    xor     eax, eax
    jmp     @@ret
@@short:
    mov     eax, -2
@@ret:
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RTP_BuildContextBlob ENDP

RTP_ValidatePacket PROC
    ; RCX = packet ptr, RDX = packet_bytes
    test    rcx, rcx
    jz      @@null
    cmp     edx, RTP_PACKET_HEADER_SIZE
    jb      @@short

    mov     eax, dword ptr [rcx + RTPP_magic]
    cmp     eax, RTP_PACKET_MAGIC
    jne     @@magic

    movzx   eax, word ptr [rcx + RTPP_version]
    cmp     eax, RTP_PACKET_VERSION
    jne     @@version

    movzx   r8d, word ptr [rcx + RTPP_header_size]
    cmp     r8d, RTP_PACKET_HEADER_SIZE
    jb      @@header
    cmp     r8d, edx
    ja      @@header

    mov     r9d, dword ptr [rcx + RTPP_payload_size]
    mov     eax, edx
    sub     eax, r8d
    cmp     r9d, eax
    ja      @@bounds

    lock inc qword ptr [g_rtpPacketsValid]
    xor     eax, eax
    ret
@@null:
    mov     eax, RTP_ERR_NULL
    ret
@@short:
    mov     eax, RTP_ERR_SHORT
    ret
@@magic:
    mov     eax, RTP_ERR_MAGIC
    ret
@@version:
    mov     eax, RTP_ERR_VERSION
    ret
@@header:
    mov     eax, RTP_ERR_HEADER
    ret
@@bounds:
    mov     eax, RTP_ERR_BOUNDS
    ret
RTP_ValidatePacket ENDP

RTP_DispatchPacket PROC FRAME
    ; RCX=packet, RDX=packet_bytes, R8=result_buf, R9=result_buf_size
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     r12, rcx ; packet
    mov     r13, rdx ; bytes
    mov     r14, r8  ; result buffer
    mov     rbx, r9  ; result cap
    mov     dword ptr [rsp+0], 0FFFFFFFFh ; current tool id (if resolved)
    mov     dword ptr [rsp+4], 0FFFFFFFFh ; current lane id (if resolved)

    ; Validate packet first.
    mov     rcx, r12
    mov     rdx, r13
    call    RTP_ValidatePacket
    test    eax, eax
    jne     @@ret

    ; Ensure descriptor table and legacy tool table are initialized.
    call    RTP_InitDescriptorTable
    call    Tool_Init

    ; Find descriptor by UUID.
    lea     rcx, [r12 + RTPP_tool_uuid]
    call    _RtpFindDescriptorByUuid
    test    rax, rax
    jz      @@tool_not_found
    mov     rdi, rax                    ; descriptor ptr
    mov     eax, dword ptr [rdi + RTPD_tool_id]
    mov     dword ptr [rsp+0], eax
    cmp     eax, RTP_MAX_TOOLS
    jae     @@tool_lane_skip
    movzx   ecx, byte ptr [g_rtpToolLaneMap + rax]
    mov     dword ptr [rsp+4], ecx
    lock inc qword ptr [g_rtpToolDispatchCounts + rax*8]
    cmp     ecx, 16
    jae     @@tool_lane_skip
    lock inc qword ptr [g_rtpLaneDispatchCounts + rcx*8]
@@tool_lane_skip:

    ; Pull mapped legacy tool ID.
    mov     esi, dword ptr [rdi + RTPD_legacy_tool_id]
    cmp     esi, 0FFFFFFFFh
    jne     @@legacy_ready
    ; Try alias mapping for tools not yet wired to dedicated legacy IDs.
    mov     rcx, rdi
    call    _RtpResolveLegacyAlias
    mov     esi, eax
    cmp     esi, 0FFFFFFFFh
    je      @@tool_unmapped
    lock inc qword ptr [g_rtpAliasHits]
@@legacy_ready:

    ; Copy payload to local scratch and null-terminate for Tool_Execute JSON args.
    mov     edi, dword ptr [r12 + RTPP_payload_size]
    cmp     edi, RTP_PAYLOAD_CAP - 1
    ja      @@payload_cap

    movzx   ecx, word ptr [r12 + RTPP_header_size]
    lea     rdx, [r12 + rcx] ; payload src
    lea     rcx, [g_rtpPayloadScratch] ; payload dst
    mov     r8d, edi
    test    r8d, r8d
    jz      @@payload_done
@@copy_loop:
    mov     al, byte ptr [rdx]
    mov     byte ptr [rcx], al
    inc     rdx
    inc     rcx
    dec     r8d
    jnz     @@copy_loop
@@payload_done:
    mov     byte ptr [rcx], 0

    ; Per-lane + per-tool policy gate.
    mov     eax, dword ptr [rsp+0]
    cmp     eax, RTP_MAX_TOOLS
    jae     @@dispatch
    mov     edx, dword ptr [g_rtpToolPolicyMask + rax*4]
    mov     ecx, dword ptr [rsp+4]
    cmp     ecx, 16
    jae     @@have_policy_mask
    or      edx, dword ptr [g_rtpLanePolicyMask + rcx*4]
@@have_policy_mask:
    test    edx, edx
    jz      @@dispatch
    lea     rcx, [g_rtpPayloadScratch]
    call    _RtpEvaluatePolicy
    test    eax, eax
    jnz     @@policy_block

@@dispatch:
    ; Prefer direct handler_rva when available.
    mov     rax, qword ptr [rdi + RTPD_handler_rva]
    test    rax, rax
    jz      @@legacy_dispatch
    lea     rcx, [g_rtpPayloadScratch]
    mov     rdx, r14
    mov     r8,  rbx
    call    rax
    test    eax, eax
    jne     @@dispatch_error
    lock inc qword ptr [g_rtpDispatchOk]
    jmp     @@ret

@@legacy_dispatch:
    ; Dispatch through legacy Tool_Execute.
    mov     ecx, esi
    lea     rdx, [g_rtpPayloadScratch]
    mov     r8,  r14
    mov     r9,  rbx
    call    Tool_Execute
    test    eax, eax
    jne     @@dispatch_error
    lock inc qword ptr [g_rtpDispatchOk]
    jmp     @@ret

@@dispatch_error:
    lock inc qword ptr [g_rtpDispatchErr]
    mov     eax, dword ptr [rsp+0]
    cmp     eax, RTP_MAX_TOOLS
    jae     @@ret
    lock inc qword ptr [g_rtpToolDispatchErr + rax*8]
    mov     ecx, dword ptr [rsp+4]
    cmp     ecx, 16
    jae     @@ret
    lock inc qword ptr [g_rtpLaneDispatchErr + rcx*8]
    jmp     @@ret
@@policy_block:
    lock inc qword ptr [g_rtpPolicyBlocks]
    mov     ecx, dword ptr [rsp+4]
    cmp     ecx, 16
    jae     @@policy_err_only
    lock inc qword ptr [g_rtpLanePolicyBlocks + rcx*8]
@@policy_err_only:
    mov     eax, RTP_ERR_POLICY_BLOCK
    lock inc qword ptr [g_rtpDispatchErr]
    mov     edx, dword ptr [rsp+0]
    cmp     edx, RTP_MAX_TOOLS
    jae     @@ret
    lock inc qword ptr [g_rtpToolPolicyBlocks + rdx*8]
    lock inc qword ptr [g_rtpToolDispatchErr + rdx*8]
    jmp     @@ret
@@tool_not_found:
    mov     eax, RTP_ERR_TOOL_NOT_FOUND
    lock inc qword ptr [g_rtpDispatchErr]
    jmp     @@ret
@@tool_unmapped:
    mov     eax, RTP_ERR_TOOL_UNMAPPED
    lock inc qword ptr [g_rtpDispatchErr]
    jmp     @@ret
@@payload_cap:
    mov     eax, RTP_ERR_PAYLOAD_CAP
    lock inc qword ptr [g_rtpDispatchErr]

@@ret:
    add     rsp, 32
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
RTP_DispatchPacket ENDP

END
