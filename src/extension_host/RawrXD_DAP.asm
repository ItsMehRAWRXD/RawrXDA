;==============================================================================
; RawrXD Debug Adapter Protocol (DAP) Implementation
; Pure MASM64 Implementation of VS Code DAP
;
; Protocol Reference:
; - https://microsoft.github.io/debug-adapter-protocol/
; - vscode/src/vs/workbench/contrib/debug/common/debugProtocol.ts
;
; Features:
; - Full DAP message lifecycle (initialize, launch, disconnect)
; - Breakpoint management (set, remove, verify)
; - Thread tracking (start, exit, pause, continue)
; - Stack trace retrieval (stackTrace request)
; - Variable inspection (scopes, variables)
; - Exception handling (pause on exception)
; - Source mapping (MASM <-> source files)
;==============================================================================
.686
.xmm
.model flat, c
option casemap:none
option frame:auto

;==============================================================================
; INCLUDES
;==============================================================================
include windows.inc
include kernel32.inc
include ntdll.inc

includelib kernel32.lib
includelib ntdll.lib

;==============================================================================
; CONSTANTS - DAP Protocol
;==============================================================================
; DAP Message Types
DAP_REQUEST                     equ 1
DAP_RESPONSE                    equ 2
DAP_EVENT                       equ 3

; DAP Commands (request types)
CMD_INITIALIZE                  equ 1
CMD_LAUNCH                      equ 2
CMD_ATTACH                      equ 3
CMD_DISCONNECT                  equ 4
CMD_SET_BREAKPOINTS             equ 5
CMD_SET_FUNCTION_BREAKPOINTS    equ 6
CMD_SET_EXCEPTION_BREAKPOINTS   equ 7
CMD_CONFIGURATION_DONE          equ 8
CMD_CONTINUE                    equ 9
CMD_NEXT                        equ 10
CMD_STEP_IN                     equ 11
CMD_STEP_OUT                    equ 12
CMD_PAUSE                       equ 13
CMD_STACK_TRACE                 equ 14
CMD_SCOPES                      equ 15
CMD_VARIABLES                   equ 16
CMD_SET_VARIABLE                equ 17
CMD_SOURCE                      equ 18
CMD_THREADS                     equ 19
CMD_TERMINATE_THREADS           equ 20
CMD_MODULES                     equ 21
CMD_LOADED_SOURCES              equ 22
CMD_EVALUATE                    equ 23
CMD_SET_EXPRESSION              equ 24
CMD_STEP_BACK                   equ 25
CMD_REVERSE_CONTINUE            equ 26
CMD_GOTO                        equ 27
CMD_GOTO_TARGETS                equ 28
CMD_COMPLETIONS                 equ 29
CMD_EXCEPTION_INFO              equ 30
CMD_READ_MEMORY                 equ 31
CMD_DISASSEMBLE                 equ 32

; DAP Events
event_initialized               equ 1
event_stopped                   equ 2
event_continued                 equ 3
event_exited                    equ 4
event_terminated                equ 5
event_thread                    equ 6
event_output                    equ 7
event_breakpoint                equ 8
event_module                    equ 9
event_loaded_source             equ 10
event_process                   equ 11
event_capabilities              equ 12

; DAP Stopped Reasons
STOP_REASON_STEP                db 'step',0
STOP_REASON_BREAKPOINT          db 'breakpoint',0
STOP_REASON_EXCEPTION           db 'exception',0
STOP_REASON_PAUSE               db 'pause',0
STOP_REASON_ENTRY               db 'entry',0
STOP_REASON_GOTO                db 'goto',0
STOP_REASON_FUNCTION_BREAKPOINT db 'function breakpoint',0
STOP_REASON_DATA_BREAKPOINT     db 'data breakpoint',0
STOP_REASON_INSTRUCTION_BREAKPOINT db 'instruction breakpoint',0

; DAP Exception Breakpoint Filters
EXCEPTION_FILTER_ALL            db 'all',0
EXCEPTION_FILTER_UNCAUGHT       db 'uncaught',0
EXCEPTION_FILTER_USER_UNHANDLED db 'userUnhandled',0

;==============================================================================
; STRUCTURES
;==============================================================================

;------------------------------------------------------------------------------
; DAP Message Header (JSON-RPC style)
;------------------------------------------------------------------------------
DAP_MESSAGE_HEADER struct
    Type                dd ?           ; DAP_REQUEST, DAP_RESPONSE, DAP_EVENT
    Id                  dd ?           ; request/response ID
    Command             dd ?           ; CMD_* for requests
    Event               dd ?           ; event_* for events
    PayloadLength       dd ?           ; JSON payload size
    _padding            dd ?
DAP_MESSAGE_HEADER ends

;------------------------------------------------------------------------------
; DAP Request (with parameters)
;------------------------------------------------------------------------------
DAP_REQUEST struct
    Header              DAP_MESSAGE_HEADER <>
    CommandString       dq ?           ; command name string
    Arguments           dq ?           ; JSON object with params
    Seq                 dd ?           ; sequence number
    _padding            dd ?
DAP_REQUEST ends

;------------------------------------------------------------------------------
; DAP Response (with result)
;------------------------------------------------------------------------------
DAP_RESPONSE struct
    Header              DAP_MESSAGE_HEADER <>
    RequestId           dd ?           ; matching request ID
    Success             db ?           ; boolean
    _padding1           db 3 dup(?)
    Error               dq ?           ; error message if !success
    Body                dq ?           ; JSON result object
    Seq                 dd ?           ; sequence number
    _padding2           dd ?
DAP_RESPONSE ends

;------------------------------------------------------------------------------
; DAP Event (asynchronous notification)
;------------------------------------------------------------------------------
DAP_EVENT struct
    Header              DAP_MESSAGE_HEADER <>
    EventString         dq ?           ; event name string
    Body                dq ?           ; JSON event data
    Seq                 dd ?           ; sequence number
    _padding            dd ?
DAP_EVENT ends

;------------------------------------------------------------------------------
; Debug Session
;------------------------------------------------------------------------------
DEBUG_SESSION struct
    SessionId           dq ?           ; unique session identifier
    ProcessHandle       dq ?           ; debuggee process handle
    ProcessId           dd ?           ; debuggee PID
    _padding1           dd ?
    State               dd ?           ; session state (initialized, running, etc.)
    _padding2           dd ?
    
    ; Breakpoint management
    Breakpoints         dq ?           ; HashMap<filePath, Breakpoint[]>
    FunctionBreakpoints dq ?           ; HashMap<functionName, Breakpoint>
    ExceptionBreakpoints dd ?          ; enabled exception filters
    _padding3           dd ?
    
    ; Thread management
    Threads             dq ?           ; HashMap<threadId, DEBUG_THREAD>
    CurrentThreadId     dd ?           ; currently focused thread
    _padding4           dd ?
    
    ; Source mapping
    SourceMap           dq ?           ; HashMap<address, SOURCE_LOCATION>
    
    ; DAP protocol state
    NextRequestId       dd ?           ; next request ID to use
    NextSeq             dd ?           ; next sequence number
    _padding5           dd ?
    
    ; Capabilities
    Capabilities        DAP_CAPABILITIES <>
DEBUG_SESSION ends

;------------------------------------------------------------------------------
; DAP Capabilities (what the debug adapter supports)
;------------------------------------------------------------------------------
DAP_CAPABILITIES struct
    SupportsConfigurationDoneRequest  db ?
    SupportsFunctionBreakpoints       db ?
    SupportsConditionalBreakpoints    db ?
    SupportsHitConditionalBreakpoints db ?
    SupportsEvaluateForHovers         db ?
    ExceptionBreakpointFilters        dq ?           ; array of filters
    FilterCount                       dd ?
    SupportsStepBack                  db ?
    SupportsSetVariable               db ?
    SupportsRestartFrame              db ?
    SupportsGotoTargetsRequest        db ?
    SupportsStepInTargetsRequest      db ?
    SupportsCompletionsRequest        db ?
    CompletionTriggerCharacters       dq ?
    SupportsModulesRequest            db ?
    AdditionalModuleColumns           dq ?
    SupportedChecksumAlgorithms       dq ?
    SupportsRestartRequest            db ?
    SupportsExceptionOptions          db ?
    SupportsValueFormattingOptions    db ?
    SupportsExceptionInfoRequest      db ?
    SupportTerminateDebuggee          db ?
    SupportSuspendDebuggee            db ?
    SupportsDelayedStackTraceLoading  db ?
    SupportsLoadedSourcesRequest      db ?
    SupportsLogPoints                 db ?
    SupportsTerminateThreadsRequest   db ?
    SupportsSetExpression             db ?
    SupportsTerminateRequest          db ?
    SupportsDataBreakpoints           db ?
    SupportsReadMemoryRequest         db ?
    SupportsWriteMemoryRequest        db ?
    SupportsDisassembleRequest        db ?
    SupportsCancelRequest             db ?
    SupportsBreakpointLocationsRequest db ?
    SupportsClipboardContext          db ?
    SupportsSteppingGranularity       db ?
    SupportsInstructionBreakpoints    db ?
    SupportsExceptionFilterOptions    db ?
    _padding                          db 3 dup(?)
DAP_CAPABILITIES ends

;------------------------------------------------------------------------------
; Debug Thread
;------------------------------------------------------------------------------
DEBUG_THREAD struct
    ThreadId            dd ?           ; system thread ID
    _padding1           dd ?
    Name                dq ?           ; thread name
    State               dd ?           ; running, stopped, etc.
    _padding2           dd ?
    
    ; Call stack
    StackFrames         dq ?           ; array of STACK_FRAME
    FrameCount          dd ?
    _padding3           dd ?
    
    ; Current execution location
    CurrentAddress      dq ?           ; instruction pointer
    CurrentSource       dq ?           ; SOURCE_LOCATION
DEBUG_THREAD ends

;------------------------------------------------------------------------------
; Stack Frame
;------------------------------------------------------------------------------
STACK_FRAME struct
    Id                  dd ?           ; unique frame ID
    _padding1           dd ?
    Name                dq ?           ; function name
    Source              dq ?           ; SOURCE_LOCATION
    Line                dd ?           ; source line number
    Column              dd ?
    EndLine             dd ?
    EndColumn           dd ?
    _padding2           dd ?
    
    ; Presentation hints
    CanRestart          db ?
    _padding3           db 7 dup(?)
    
    ; Instruction pointer
    InstructionPointer  dq ?
    
    ; Module info
    ModuleId            dq ?
    _padding4           dq ?
STACK_FRAME ends

;------------------------------------------------------------------------------
; Source Location
;------------------------------------------------------------------------------
SOURCE_LOCATION struct
    Path                dq ?           ; full file path
    Name                dq ?           ; file name only
    SourceReference     dd ?           ; 0 = real file, >0 = dynamic
    _padding            dd ?
    
    ; Presentation
    PresentationHint    dd ?           ; normal, emphasize, deemphasize
    Origin              dq ?           ; origin info
    Sources             dq ?           ; alternative sources
    _padding2           dq ?
SOURCE_LOCATION ends

;------------------------------------------------------------------------------
; Breakpoint
;------------------------------------------------------------------------------
BREAKPOINT struct
    Id                  dd ?           ; unique breakpoint ID
    _padding1           dd ?
    Verified            db ?           ; verified by debugger
    _padding2           db 3 dup(?)
    
    ; Location
    Source              SOURCE_LOCATION <>
    Line                dd ?           ; source line
    Column              dd ?
    
    ; Conditions
    Condition           dq ?           ; conditional expression
    HitCondition        dq ?           ; hit count condition
    
    ; Statistics
    HitCount            dd ?           ; times hit
    _padding3           dd ?
    
    ; Type-specific
    Type                dd ?           ; source, function, exception, etc.
    FunctionName        dq ?           ; for function breakpoints
    InstructionReference dq ?        ; for instruction breakpoints
    
    ; State
    Enabled             db ?
    _padding4           db 7 dup(?)
BREAKPOINT ends

;------------------------------------------------------------------------------
; Scope (variable scope)
;------------------------------------------------------------------------------
SCOPE struct
    Name                dq ?           ; scope name (Locals, Globals, etc.)
    PresentationHint    dd ?           | 'locals', 'registers', etc.
    _padding1           dd ?
    VariablesReference  dq ?           ; reference to variables container
    NamedVariables      dd ?           ; number of named variables
    IndexedVariables    dd ?           ; number of indexed variables
    Expensive           db ?           ; expensive to retrieve
    _padding2           db 7 dup(?)
    Source              dq ?           ; optional source location
    Line                dd ?
    Column              dd ?
    EndLine             dd ?
    EndColumn           dd ?
    _padding3           dd ?
SCOPE ends

;------------------------------------------------------------------------------
; Variable
;------------------------------------------------------------------------------
VARIABLE struct
    Name                dq ?           ; variable name
    Value               dq ?           ; variable value as string
    Type                dq ?           ; type hint
    PresentationHint    dd ?           | 'property', 'method', etc.
    _padding1           dd ?
    
    ; Nested variables
    VariablesReference  dq ?           ; reference to child variables
    NamedVariables      dd ?           ; number of named children
    IndexedVariables    dd ?           ; number of indexed children
    
    ; Memory reference
    MemoryReference     dq ?           | memory reference or pointer
    
    ; Evaluation
    EvaluateName        dq ?           | expression for evaluation
    
    ; Modifiers
    CanAssign           db ?           ; can be assigned to
    _padding2           db 7 dup(?)
VARIABLE ends

;------------------------------------------------------------------------------
; DAP Bridge (extension host <-> debug adapter)
;------------------------------------------------------------------------------
DAP_BRIDGE struct
    SharedMemoryHandle  dq ?           ; shared memory for DAP messages
    SharedMemoryPtr     dq ?
    SharedMemorySize    dq ?
    
    ; Message queue (ring buffer)
    MessageQueue        dq ?
    QueueHead           dd ?           ; write index
    QueueTail           dd ?           ; read index
    QueueLock           dq ?           ; SRWLOCK
    
    ; Events
    MessageAvailable    dq ?           ; event handle
    AdapterReady        dq ?           ; adapter ready signal
    SessionTerminated   dq ?           ; session ended signal
    
    ; Session
    CurrentSession      dq ?           ; DEBUG_SESSION*
    IsSessionActive     db ?
    _padding            db 7 dup(?)
DAP_BRIDGE ends

;==============================================================================
; GLOBAL DATA
;==============================================================================
.data
align 16

; DAP Bridge singleton
g_DAPBridge             DAP_BRIDGE <>

; Current debug session
g_CurrentSession        dq 0

; Breakpoint ID counter
g_NextBreakpointId      dd 1

; Thread ID counter
g_NextThreadId          dd 1

; DAP message sequence
g_DAPSequence           dd 1

; Exception breakpoint filters
szFilterAll             db 'all',0
szFilterUncaught        db 'uncaught',0
szFilterUserUnhandled   db 'userUnhandled',0

; DAP capability defaults
g_DefaultCapabilities   DAP_CAPABILITIES <>

;==============================================================================
; CODE
;==============================================================================
.code

;==============================================================================
; DAP INITIALIZATION
;==============================================================================

;------------------------------------------------------------------------------
; Initialize DAP bridge and capabilities
;------------------------------------------------------------------------------
DAP_Initialize proc frame uses rbx rsi rdi
    mov rbx, offset g_DAPBridge
    
    ; Create shared memory for DAP messages (4MB)
    xor ecx, ecx
    mov edx, PAGE_READWRITE
    mov r8d, SEC_COMMIT
    mov r9d, 4*1024*1024
    call CreateFileMappingW
    test rax, rax
    jz failed
    mov [rbx].DAP_BRIDGE.SharedMemoryHandle, rax
    
    ; Map view
    mov rcx, rax
    xor edx, edx
    xor r8d, r8d
    mov r9d, 4*1024*1024
    call MapViewOfFile
    test rax, rax
    jz map_failed
    mov [rbx].DAP_BRIDGE.SharedMemoryPtr, rax
    mov [rbx].DAP_BRIDGE.SharedMemorySize, 4*1024*1024
    
    ; Setup message queue
    lea rax, [rax+64]           ; leave room for header
    mov [rbx].DAP_BRIDGE.MessageQueue, rax
    
    ; Initialize queue indices
    mov dword ptr [rbx].DAP_BRIDGE.QueueHead, 0
    mov dword ptr [rbx].DAP_BRIDGE.QueueTail, 0
    
    ; Create events
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    call CreateEventW
    mov [rbx].DAP_BRIDGE.MessageAvailable, rax
    
    xor ecx, ecx
    xor edx, edx
    mov r8d, 1                  ; manual reset
    call CreateEventW
    mov [rbx].DAP_BRIDGE.AdapterReady, rax
    
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    call CreateEventW
    mov [rbx].DAP_BRIDGE.SessionTerminated, rax
    
    ; Initialize capabilities
    call DAP_InitCapabilities
    
    mov rax, 1
    ret
    
map_failed:
    mov rcx, [rbx].DAP_BRIDGE.SharedMemoryHandle
    call CloseHandle
    
failed:
    xor rax, rax
    ret
DAP_Initialize endp

;------------------------------------------------------------------------------
; Initialize default DAP capabilities
;------------------------------------------------------------------------------
DAP_InitCapabilities proc frame uses rbx rsi rdi
    mov rbx, offset g_DefaultCapabilities
    
    ; Core capabilities
    mov [rbx].DAP_CAPABILITIES.SupportsConfigurationDoneRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportsFunctionBreakpoints, 1
    mov [rbx].DAP_CAPABILITIES.SupportsConditionalBreakpoints, 1
    mov [rbx].DAP_CAPABILITIES.SupportsHitConditionalBreakpoints, 1
    mov [rbx].DAP_CAPABILITIES.SupportsEvaluateForHovers, 1
    mov [rbx].DAP_CAPABILITIES.SupportsStepBack, 0          ; not supported yet
    mov [rbx].DAP_CAPABILITIES.SupportsSetVariable, 1
    mov [rbx].DAP_CAPABILITIES.SupportsRestartFrame, 0     ; not supported yet
    mov [rbx].DAP_CAPABILITIES.SupportsGotoTargetsRequest, 0
    mov [rbx].DAP_CAPABILITIES.SupportsStepInTargetsRequest, 0
    mov [rbx].DAP_CAPABILITIES.SupportsCompletionsRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportsModulesRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportsRestartRequest, 0
    mov [rbx].DAP_CAPABILITIES.SupportsExceptionOptions, 1
    mov [rbx].DAP_CAPABILITIES.SupportsValueFormattingOptions, 1
    mov [rbx].DAP_CAPABILITIES.SupportsExceptionInfoRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportTerminateDebuggee, 1
    mov [rbx].DAP_CAPABILITIES.SupportSuspendDebuggee, 1
    mov [rbx].DAP_CAPABILITIES.SupportsDelayedStackTraceLoading, 1
    mov [rbx].DAP_CAPABILITIES.SupportsLoadedSourcesRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportsLogPoints, 1
    mov [rbx].DAP_CAPABILITIES.SupportsTerminateThreadsRequest, 0
    mov [rbx].DAP_CAPABILITIES.SupportsSetExpression, 1
    mov [rbx].DAP_CAPABILITIES.SupportsTerminateRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportsDataBreakpoints, 0  ; not yet
    mov [rbx].DAP_CAPABILITIES.SupportsReadMemoryRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportsWriteMemoryRequest, 0
    mov [rbx].DAP_CAPABILITIES.SupportsDisassembleRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportsCancelRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportsBreakpointLocationsRequest, 1
    mov [rbx].DAP_CAPABILITIES.SupportsClipboardContext, 0
    mov [rbx].DAP_CAPABILITIES.SupportsSteppingGranularity, 1
    mov [rbx].DAP_CAPABILITIES.SupportsInstructionBreakpoints, 1
    mov [rbx].DAP_CAPABILITIES.SupportsExceptionFilterOptions, 1
    
    ; Exception filters
    mov [rbx].DAP_CAPABILITIES.FilterCount, 3
    
    ret
DAP_InitCapabilities endp

;==============================================================================
; DEBUG SESSION MANAGEMENT
;==============================================================================

;------------------------------------------------------------------------------
; Create new debug session
;------------------------------------------------------------------------------
DAP_CreateSession proc frame uses rbx rsi rdi
    ; Allocate session
    mov ecx, sizeof(DEBUG_SESSION)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; Initialize
    mov [rbx].DEBUG_SESSION.State, 0          ; uninitialized
    mov [rbx].DEBUG_SESSION.NextRequestId, 1
    mov [rbx].DEBUG_SESSION.NextSeq, 1
    mov [rbx].DEBUG_SESSION.CurrentThreadId, 0
    
    ; Create collections
    call HashMap_Create
    mov [rbx].DEBUG_SESSION.Breakpoints, rax
    
    call HashMap_Create
    mov [rbx].DEBUG_SESSION.FunctionBreakpoints, rax
    
    call HashMap_Create
    mov [rbx].DEBUG_SESSION.Threads, rax
    
    call HashMap_Create
    mov [rbx].DEBUG_SESSION.SourceMap, rax
    
    ; Copy capabilities
    lea rcx, g_DefaultCapabilities
    lea rdx, [rbx].DEBUG_SESSION.Capabilities
    mov r8d, sizeof(DAP_CAPABILITIES)
    call memcpy
    
    mov rax, rbx
    ret
    
failed:
    xor rax, rax
    ret
DAP_CreateSession endp

;------------------------------------------------------------------------------
; Start debug session (launch or attach)
;------------------------------------------------------------------------------
DAP_StartSession proc frame uses rbx rsi rdi r12 r13,
    pSession:ptr DEBUG_SESSION,
    launchConfig:qword        ; JSON launch configuration
    
    mov rbx, pSession
    mov r12, launchConfig
    
    ; Parse launch configuration
    mov rcx, r12
    call Json_ParseObject
    mov rsi, rax                ; parsed config
    
    ; Extract program path
    mov rcx, rsi
    lea rdx, szKeyProgram
    call Json_GetString
    mov rdi, rax                ; program path
    
    ; Extract arguments if any
    mov rcx, rsi
    lea rdx, szKeyArgs
    call Json_GetArray
    mov r13, rax                ; arguments array
    
    ; Create process for debugging
    call DAP_CreateDebuggeeProcess
    test rax, rax
    jz failed
    
    mov [rbx].DEBUG_SESSION.ProcessHandle, rax
    mov [rbx].DEBUG_SESSION.ProcessId, edx
    mov [rbx].DEBUG_SESSION.State, 1          ; initialized
    
    ; Set up debug event handling
    call DAP_SetupDebugEventLoop
    
    ; Send initialized event to client
    call DAP_SendInitializedEvent
    
    mov rax, 1
    ret
    
failed:
    xor rax, rax
    ret
DAP_StartSession endp

;------------------------------------------------------------------------------
; Create debuggee process with debugging enabled
;------------------------------------------------------------------------------
DAP_CreateDebuggeeProcess proc frame uses rbx rsi rdi r12 r13,
    programPath:qword,
    args:qword
    
    mov rsi, programPath
    mov rdi, args
    
    ; Build command line
    sub rsp, 4096
    mov rbx, rsp
    
    ; Copy program path
    mov rcx, rbx
    mov rdx, rsi
    call strcpy
    add rbx, rax
    
    ; Add arguments if any
    test rdi, rdi
    jz @F
    
    mov byte ptr [rbx], ' '
    inc rbx
    
    ; TODO: append arguments
    
@@:
    mov byte ptr [rbx], 0
    
    ; Create process with DEBUG_PROCESS flag
    lea rcx, startupInfo
    lea rdx, processInfo
    xor r8d, r8d              ; lpCurrentDirectory
    xor r9d, r9d              ; lpEnvironment
    
    push rsp                  ; lpProcessInformation
    push r9                   ; lpStartupInfo
    push CREATE_NEW_CONSOLE or DEBUG_PROCESS  ; dwCreationFlags
    push 0                    ; bInheritHandles
    push 0                    ; lpThreadAttributes
    push 0                    ; lpProcessAttributes
    push rsp                  ; lpCommandLine
    push 0                    ; lpApplicationName
    
    call CreateProcessW
    test eax, eax
    jz create_failed
    
    mov rax, [processInfo].hProcess
    mov edx, [processInfo].dwProcessId
    
    add rsp, 4096
    ret
    
create_failed:
    add rsp, 4096
    xor rax, rax
    ret
    
startupInfo STARTUPINFO <sizeof(STARTUPINFO)>
processInfo PROCESS_INFORMATION <>
DAP_CreateDebuggeeProcess endp

;------------------------------------------------------------------------------
; Set up debug event handling loop
;------------------------------------------------------------------------------
DAP_SetupDebugEventLoop proc frame uses rbx rsi rdi
    ; Create debug event thread
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    lea r9, DAP_DebugEventThreadProc
    push 0                      ; lpParameter
    push 0                      ; dwCreationFlags
    push 0                      ; lpThreadId
    call CreateThread
    test rax, rax
    jz failed
    
    ; Close thread handle (we don't need it)
    mov rcx, rax
    call CloseHandle
    
    mov rax, 1
    ret
    
failed:
    xor rax, rax
    ret
DAP_SetupDebugEventLoop endp

;------------------------------------------------------------------------------
; Debug event thread procedure
;------------------------------------------------------------------------------
DAP_DebugEventThreadProc proc frame uses rbx rsi rdi
    local debugEvent:DEBUG_EVENT
    
    mov rbx, g_CurrentSession
    
    ; Debug event loop
@@:
    lea rcx, debugEvent
    mov edx, INFINITE
    call WaitForDebugEvent
    test eax, eax
    jz exit_loop
    
    ; Process debug event
    lea rcx, debugEvent
    call DAP_ProcessDebugEvent
    
    ; Continue debuggee
    lea rcx, debugEvent
    mov rdx, DBG_CONTINUE
    call ContinueDebugEvent
    
    jmp @B
    
exit_loop:
    ret
DAP_DebugEventThreadProc endp

;------------------------------------------------------------------------------
; Process Windows debug events and convert to DAP
;------------------------------------------------------------------------------
DAP_ProcessDebugEvent proc frame uses rbx rsi rdi,
    pDebugEvent:ptr DEBUG_EVENT
    
    mov rbx, pDebugEvent
    
    ; Dispatch by event type
    mov ecx, [rbx].DEBUG_EVENT.dwDebugEventCode
    
    .if ecx == CREATE_PROCESS_DEBUG_EVENT
        jmp handle_create_process
    .elseif ecx == EXIT_PROCESS_DEBUG_EVENT
        jmp handle_exit_process
    .elseif ecx == CREATE_THREAD_DEBUG_EVENT
        jmp handle_create_thread
    .elseif ecx == EXIT_THREAD_DEBUG_EVENT
        jmp handle_exit_thread
    .elseif ecx == EXCEPTION_DEBUG_EVENT
        jmp handle_exception
    .elseif ecx == LOAD_DLL_DEBUG_EVENT
        jmp handle_load_dll
    .elseif ecx == UNLOAD_DLL_DEBUG_EVENT
        jmp handle_unload_dll
    .elseif ecx == OUTPUT_DEBUG_STRING_EVENT
        jmp handle_output_string
    .elseif ecx == RIP_EVENT
        jmp handle_rip
    .endif
    
    ret
    
handle_create_process:
    call DAP_HandleCreateProcess
    ret
    
handle_exit_process:
    call DAP_HandleExitProcess
    ret
    
handle_create_thread:
    call DAP_HandleCreateThread
    ret
    
handle_exit_thread:
    call DAP_HandleExitThread
    ret
    
handle_exception:
    call DAP_HandleException
    ret
    
handle_load_dll:
    call DAP_HandleLoadDll
    ret
    
handle_unload_dll:
    call DAP_HandleUnloadDll
    ret
    
handle_output_string:
    call DAP_HandleOutputString
    ret
    
handle_rip:
    call DAP_HandleRip
    ret
DAP_ProcessDebugEvent endp

;==============================================================================
; BREAKPOINT MANAGEMENT
;==============================================================================

;------------------------------------------------------------------------------
; Set breakpoints for a source file
;------------------------------------------------------------------------------
DAP_SetBreakpoints proc frame uses rbx rsi rdi r12 r13 r14,
    pSession:ptr DEBUG_SESSION,
    sourcePath:qword,
    breakpoints:qword,        ; array of breakpoint requests
    count:dword
    
    mov rbx, pSession
    mov r12, sourcePath
    mov r13, breakpoints
    mov r14d, count
    
    ; Get or create breakpoint list for this source
    mov rcx, [rbx].DEBUG_SESSION.Breakpoints
    mov rdx, r12
    call HashMap_Get
    test rax, rax
    jnz have_breakpoint_list
    
    ; Create new breakpoint list
    call ArrayList_Create
    mov rcx, [rbx].DEBUG_SESSION.Breakpoints
    mov rdx, r12
    mov r8, rax
    call HashMap_Put
    
have_breakpoint_list:
    mov rsi, rax                ; breakpoint list
    
    ; Clear existing breakpoints for this source
    call ArrayList_Clear
    
    ; Set new breakpoints
    xor ebx, ebx
    
bp_loop:
    cmp ebx, r14d
    jae bp_done
    
    ; Get breakpoint request
    mov rdi, [r13 + rbx*8]
    
    ; Create breakpoint
    call DAP_CreateBreakpoint
    test rax, rax
    jz next_bp
    
    ; Add to list
    mov rcx, rsi
    mov rdx, rax
    call ArrayList_Add
    
    ; Verify breakpoint (check if address is valid)
    mov rcx, rax
    call DAP_VerifyBreakpoint
    
    ; Send breakpoint event
    call DAP_SendBreakpointEvent
    
next_bp:
    inc ebx
    jmp bp_loop
    
bp_done:
    ; Return breakpoints array
    mov rax, rsi
    ret
DAP_SetBreakpoints endp

;------------------------------------------------------------------------------
; Create breakpoint from request
;------------------------------------------------------------------------------
DAP_CreateBreakpoint proc frame uses rbx rsi rdi,
    bpRequest:qword            ; JSON breakpoint request
    
    mov rsi, bpRequest
    
    ; Allocate breakpoint
    mov ecx, sizeof(BREAKPOINT)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; Initialize
    mov eax, g_NextBreakpointId
    mov [rbx].BREAKPOINT.Id, eax
    inc g_NextBreakpointId
    
    mov [rbx].BREAKPOINT.Enabled, 1
    
    ; Parse location
    mov rcx, rsi
    lea rdx, szKeyLine
    call Json_GetInt
    mov [rbx].BREAKPOINT.Line, eax
    
    ; Parse condition if present
    mov rcx, rsi
    lea rdx, szKeyCondition
    call Json_GetString
    mov [rbx].BREAKPOINT.Condition, rax
    
    ; Parse hit condition if present
    mov rcx, rsi
    lea rdx, szKeyHitCondition
    call Json_GetString
    mov [rbx].BREAKPOINT.HitCondition, rax
    
    mov rax, rbx
    ret
    
failed:
    xor rax, rax
    ret
DAP_CreateBreakpoint endp

;------------------------------------------------------------------------------
; Verify breakpoint (resolve to memory address)
;------------------------------------------------------------------------------
DAP_VerifyBreakpoint proc frame uses rbx rsi rdi,
    pBreakpoint:ptr BREAKPOINT
    
    mov rbx, pBreakpoint
    
    ; TODO: Resolve source line to memory address
    ; This requires debug info parsing (PDB or DWARF)
    ; For now, mark as unverified
    
    mov [rbx].BREAKPOINT.Verified, 0
    
    ret
DAP_VerifyBreakpoint endp

;------------------------------------------------------------------------------
; Set exception breakpoints
;------------------------------------------------------------------------------
DAP_SetExceptionBreakpoints proc frame uses rbx rsi rdi r12 r13,
    pSession:ptr DEBUG_SESSION,
    filters:qword,            ; array of filter names
    filterOptions:qword,      ; optional filter options
    count:dword
    
    mov rbx, pSession
    mov r12, filters
    mov r13, filterOptions
    mov r14d, count
    
    ; Clear existing exception breakpoints
    mov [rbx].DEBUG_SESSION.ExceptionBreakpoints, 0
    
    ; Enable requested filters
    xor ebx, ebx
    
filter_loop:
    cmp ebx, r14d
    jae filter_done
    
    mov rsi, [r12 + rbx*8]      ; filter name
    
    ; Check filter type
    mov rcx, rsi
    lea rdx, szFilterAll
    call strcmp
    test eax, eax
    jz enable_all
    
    mov rcx, rsi
    lea rdx, szFilterUncaught
    call strcmp
    test eax, eax
    jz enable_uncaught
    
    mov rcx, rsi
    lea rdx, szFilterUserUnhandled
    call strcmp
    test eax, eax
    jz enable_user_unhandled
    
next_filter:
    inc ebx
    jmp filter_loop
    
enable_all:
    or [rbx].DEBUG_SESSION.ExceptionBreakpoints, 1
    jmp next_filter
    
enable_uncaught:
    or [rbx].DEBUG_SESSION.ExceptionBreakpoints, 2
    jmp next_filter
    
enable_user_unhandled:
    or [rbx].DEBUG_SESSION.ExceptionBreakpoints, 4
    jmp next_filter
    
filter_done:
    mov rax, 1
    ret
DAP_SetExceptionBreakpoints endp

;==============================================================================
; THREAD MANAGEMENT
;==============================================================================

;------------------------------------------------------------------------------
; Handle create thread debug event
;------------------------------------------------------------------------------
DAP_HandleCreateThread proc frame uses rbx rsi rdi
    mov rbx, g_CurrentSession
    
    ; Create thread object
    mov ecx, sizeof(DEBUG_THREAD)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rsi, rax
    
    ; Initialize
    mov eax, g_NextThreadId
    mov [rsi].DEBUG_THREAD.ThreadId, eax
    inc g_NextThreadId
    
    mov [rsi].DEBUG_THREAD.State, 1     ; running
    
    ; Add to threads map
    mov rcx, [rbx].DEBUG_SESSION.Threads
    mov edx, [rsi].DEBUG_THREAD.ThreadId
    mov r8, rsi
    call HashMap_Put
    
    ; Send thread event
    call DAP_SendThreadEvent
    
    mov rax, 1
    ret
    
failed:
    xor rax, rax
    ret
DAP_HandleCreateThread endp

;------------------------------------------------------------------------------
; Handle exit thread debug event
;------------------------------------------------------------------------------
DAP_HandleExitThread proc frame uses rbx rsi rdi
    mov rbx, g_CurrentSession
    
    ; Get thread ID
    mov ecx, [rbx].DEBUG_EVENT.dwThreadId
    
    ; Remove from threads map
    mov rcx, [rbx].DEBUG_SESSION.Threads
    mov edx, ecx
    call HashMap_Remove
    
    ; Send thread event
    call DAP_SendThreadEvent
    
    ret
DAP_HandleExitThread endp

;------------------------------------------------------------------------------
; Get all threads (for threads request)
;------------------------------------------------------------------------------
DAP_GetThreads proc frame uses rbx rsi rdi,
    pSession:ptr DEBUG_SESSION
    
    mov rbx, pSession
    
    ; Create threads array
    call ArrayList_Create
    mov rsi, rax
    
    ; Iterate threads map
    mov rcx, [rbx].DEBUG_SESSION.Threads
    lea rdx, DAP_ThreadIterator
    mov r8, rsi
    call HashMap_ForEach
    
    mov rax, rsi
    ret
DAP_GetThreads endp

;------------------------------------------------------------------------------
; Thread iterator callback
;------------------------------------------------------------------------------
DAP_ThreadIterator proc frame uses rbx rsi rdi,
    threadId:dword,
    pThread:ptr DEBUG_THREAD,
    pArray:qword
    
    mov rbx, pThread
    mov rsi, pArray
    
    ; Add thread to array
    mov rcx, rsi
    mov rdx, rbx
    call ArrayList_Add
    
    ret
DAP_ThreadIterator endp

;==============================================================================
; STACK TRACE AND VARIABLES
;==============================================================================

;------------------------------------------------------------------------------
; Get stack trace for thread
;------------------------------------------------------------------------------
DAP_GetStackTrace proc frame uses rbx rsi rdi r12 r13,
    pSession:ptr DEBUG_SESSION,
    threadId:dword,
    startFrame:dword,
    levels:dword
    
    mov rbx, pSession
    mov r12d, threadId
    mov r13d, levels
    
    ; Get thread
    mov rcx, [rbx].DEBUG_SESSION.Threads
    mov edx, r12d
    call HashMap_Get
    test rax, rax
    jz failed
    mov rsi, rax                ; DEBUG_THREAD
    
    ; Get thread context (registers)
    call DAP_GetThreadContext
    
    ; Walk stack frames
    call DAP_WalkStackFrames
    
    ; Create stack frames array
    call ArrayList_Create
    mov rdi, rax
    
    ; Convert native frames to DAP frames
    mov rcx, rsi
    lea rdx, [rsi].DEBUG_THREAD.StackFrames
    mov r8, rdi
    call DAP_ConvertStackFrames
    
    mov rax, rdi
    ret
    
failed:
    xor rax, rax
    ret
DAP_GetStackTrace endp

;------------------------------------------------------------------------------
; Get thread context (registers)
;------------------------------------------------------------------------------
DAP_GetThreadContext proc frame uses rbx rsi rdi,
    pThread:ptr DEBUG_THREAD
    
    mov rbx, pThread
    
    ; Get thread handle from thread ID
    mov ecx, [rbx].DEBUG_THREAD.ThreadId
    call DAP_GetThreadHandle
    test rax, rax
    jz failed
    mov rsi, rax
    
    ; Get thread context
    lea rcx, context
    mov [rcx].CONTEXT.ContextFlags, CONTEXT_FULL
    mov rdx, rsi
    call GetThreadContext
    test eax, eax
    jz failed
    
    ; Store instruction pointer
    mov rax, [context].Rip
    mov [rbx].DEBUG_THREAD.CurrentAddress, rax
    
    mov rax, 1
    ret
    
failed:
    xor rax, rax
    ret
    
context CONTEXT <>
DAP_GetThreadContext endp

;------------------------------------------------------------------------------
; Walk stack frames (x64 stack walking)
;------------------------------------------------------------------------------
DAP_WalkStackFrames proc frame uses rbx rsi rdi
    ; TODO: Implement proper stack walking
    ; For now, create a single frame at current IP
    
    mov ecx, sizeof(STACK_FRAME)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; Initialize frame
    mov eax, 0                  ; frame ID
    mov [rbx].STACK_FRAME.Id, eax
    
    ; Set instruction pointer
    mov rax, [context].Rip
    mov [rbx].STACK_FRAME.InstructionPointer, rax
    
    ; Resolve to source location
    call DAP_ResolveSourceLocation
    mov [rbx].STACK_FRAME.Source, rax
    
    mov rax, rbx
    ret
    
failed:
    xor rax, rax
    ret
DAP_WalkStackFrames endp

;------------------------------------------------------------------------------
; Resolve instruction address to source location
;------------------------------------------------------------------------------
DAP_ResolveSourceLocation proc frame uses rbx rsi rdi,
    address:qword
    
    mov rax, address
    
    ; TODO: Implement debug info lookup
    ; This requires PDB or DWARF parsing
    ; For now, return unknown location
    
    mov ecx, sizeof(SOURCE_LOCATION)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; Mark as unknown
    mov [rbx].SOURCE_LOCATION.SourceReference, -1
    
    mov rax, rbx
    ret
    
failed:
    xor rax, rax
    ret
DAP_ResolveSourceLocation endp

;------------------------------------------------------------------------------
; Get scopes for frame
;------------------------------------------------------------------------------
DAP_GetScopes proc frame uses rbx rsi rdi,
    pFrame:ptr STACK_FRAME
    
    mov rbx, pFrame
    
    ; Create scopes array
    call ArrayList_Create
    mov rsi, rax
    
    ; Add locals scope
    call DAP_CreateLocalsScope
    mov rcx, rsi
    mov rdx, rax
    call ArrayList_Add
    
    ; Add registers scope
    call DAP_CreateRegistersScope
    mov rcx, rsi
    mov rdx, rax
    call ArrayList_Add
    
    mov rax, rsi
    ret
DAP_GetScopes endp

;------------------------------------------------------------------------------
; Create locals scope
;------------------------------------------------------------------------------
DAP_CreateLocalsScope proc frame uses rbx rsi rdi
    mov ecx, sizeof(SCOPE)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; Initialize
    lea rax, szLocalsScope
    mov [rbx].SCOPE.Name, rax
    mov [rbx].SCOPE.PresentationHint, 0       ; 'locals'
    mov [rbx].SCOPE.VariablesReference, 1     ; reference ID
    mov [rbx].SCOPE.NamedVariables, 0
    mov [rbx].SCOPE.IndexedVariables, 0
    mov [rbx].SCOPE.Expensive, 0
    
    mov rax, rbx
    ret
    
failed:
    xor rax, rax
    ret
DAP_CreateLocalsScope endp

;------------------------------------------------------------------------------
; Create registers scope
;------------------------------------------------------------------------------
DAP_CreateRegistersScope proc frame uses rbx rsi rdi
    mov ecx, sizeof(SCOPE)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; Initialize
    lea rax, szRegistersScope
    mov [rbx].SCOPE.Name, rax
    mov [rbx].SCOPE.PresentationHint, 1       ; 'registers'
    mov [rbx].SCOPE.VariablesReference, 2     ; reference ID
    mov [rbx].SCOPE.NamedVariables, 16        ; RAX, RBX, etc.
    mov [rbx].SCOPE.IndexedVariables, 0
    mov [rbx].SCOPE.Expensive, 0
    
    mov rax, rbx
    ret
    
failed:
    xor rax, rax
    ret
DAP_CreateRegistersScope endp

;------------------------------------------------------------------------------
; Get variables for scope
;------------------------------------------------------------------------------
DAP_GetVariables proc frame uses rbx rsi rdi r12 r13,
    variablesReference:qword,
    start:dword,
    count:dword
    
    mov r12, variablesReference
    mov r13d, start
    mov r14d, count
    
    ; Create variables array
    call ArrayList_Create
    mov rsi, rax
    
    ; Dispatch by reference ID
    .if r12 == 1          ; locals
        jmp get_locals
    .elseif r12 == 2      ; registers
        jmp get_registers
    .else
        jmp done
    .endif
    
get_locals:
    call DAP_GetLocalVariables
    jmp add_variables
    
get_registers:
    call DAP_GetRegisterVariables
    jmp add_variables
    
add_variables:
    ; Add variables to array
    ; TODO: implement
    
done:
    mov rax, rsi
    ret
DAP_GetVariables endp

;------------------------------------------------------------------------------
; Get local variables
;------------------------------------------------------------------------------
DAP_GetLocalVariables proc frame uses rbx rsi rdi
    ; TODO: Implement local variable enumeration
    ; This requires debug info and stack analysis
    
    ; For now, return empty
    xor rax, rax
    ret
DAP_GetLocalVariables endp

;------------------------------------------------------------------------------
; Get register variables
;------------------------------------------------------------------------------
DAP_GetRegisterVariables proc frame uses rbx rsi rdi
    ; Create register variables
    ; RAX, RBX, RCX, RDX, RSI, RDI, RBP, RSP
    ; R8-R15
    
    mov ecx, 16 * sizeof(VARIABLE)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; TODO: Fill in register values from context
    
    mov rax, rbx
    ret
    
failed:
    xor rax, rax
    ret
DAP_GetRegisterVariables endp

;==============================================================================
; DAP MESSAGE HANDLING
;==============================================================================

;------------------------------------------------------------------------------
; Send DAP message via bridge
;------------------------------------------------------------------------------
DAP_SendMessage proc frame uses rbx rsi rdi r12 r13,
    msgType:dword,
    payload:qword,
    payloadLen:dword
    
    mov r12d, msgType
    mov r13, payload
    mov r14d, payloadLen
    
    mov rbx, offset g_DAPBridge
    
    ; Acquire queue lock
    lea rcx, [rbx].DAP_BRIDGE.QueueLock
    call AcquireSRWLockExclusive
    
    ; Check space
    mov eax, [rbx].DAP_BRIDGE.QueueHead
    mov ecx, [rbx].DAP_BRIDGE.QueueTail
    
    sub ecx, eax
    jae @F
    add ecx, MAX_DAP_MESSAGES
    
@@:
    cmp ecx, MAX_DAP_MESSAGES - 1
    jae queue_full
    
    ; Write message header
    mov rdx, [rbx].DAP_BRIDGE.MessageQueue
    add rdx, rax
    
    mov [rdx].DAP_MESSAGE_HEADER.Type, r12d
    mov [rdx].DAP_MESSAGE_HEADER.PayloadLength, r14d
    
    ; Copy payload
    lea rcx, [rdx + sizeof(DAP_MESSAGE_HEADER)]
    mov rdx, r13
    mov r8d, r14d
    call memcpy
    
    ; Update head
    mov eax, [rbx].DAP_BRIDGE.QueueHead
    add eax, sizeof(DAP_MESSAGE_HEADER) + r14d
    and eax, MAX_DAP_MESSAGES - 1
    mov [rbx].DAP_BRIDGE.QueueHead, eax
    
    ; Release lock
    lea rcx, [rbx].DAP_BRIDGE.QueueLock
    call ReleaseSRWLockExclusive
    
    ; Signal message available
    mov rcx, [rbx].DAP_BRIDGE.MessageAvailable
    call SetEvent
    
    mov rax, 1
    ret
    
queue_full:
    lea rcx, [rbx].DAP_BRIDGE.QueueLock
    call ReleaseSRWLockExclusive
    
    xor rax, rax
    ret
DAP_SendMessage endp

;------------------------------------------------------------------------------
; Send initialized event
;------------------------------------------------------------------------------
DAP_SendInitializedEvent proc frame uses rbx rsi rdi
    mov ecx, sizeof(DAP_EVENT)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; Initialize event
    mov [rbx].DAP_EVENT.Header.Type, DAP_EVENT
    mov [rbx].DAP_EVENT.Event, event_initialized
    lea rax, szEventInitialized
    mov [rbx].DAP_EVENT.EventString, rax
    
    ; Send
    mov rcx, rbx
    call DAP_SendEvent
    
    mov rax, 1
    ret
    
failed:
    xor rax, rax
    ret
DAP_SendInitializedEvent endp

;------------------------------------------------------------------------------
; Send thread event
;------------------------------------------------------------------------------
DAP_SendThreadEvent proc frame uses rbx rsi rdi
    mov ecx, sizeof(DAP_EVENT)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; Initialize event
    mov [rbx].DAP_EVENT.Header.Type, DAP_EVENT
    mov [rbx].DAP_EVENT.Event, event_thread
    lea rax, szEventThread
    mov [rbx].DAP_EVENT.EventString, rax
    
    ; Send
    mov rcx, rbx
    call DAP_SendEvent
    
    mov rax, 1
    ret
    
failed:
    xor rax, rax
    ret
DAP_SendThreadEvent endp

;------------------------------------------------------------------------------
; Send breakpoint event
;------------------------------------------------------------------------------
DAP_SendBreakpointEvent proc frame uses rbx rsi rdi
    mov ecx, sizeof(DAP_EVENT)
    call HeapAlloc
    test rax, rax
    jz failed
    mov rbx, rax
    
    ; Initialize event
    mov [rbx].DAP_EVENT.Header.Type, DAP_EVENT
    mov [rbx].DAP_EVENT.Event, event_breakpoint
    lea rax, szEventBreakpoint
    mov [rbx].DAP_EVENT.EventString, rax
    
    ; Send
    mov rcx, rbx
    call DAP_SendEvent
    
    mov rax, 1
    ret
    
failed:
    xor rax, rax
    ret
DAP_SendBreakpointEvent endp

;------------------------------------------------------------------------------
; Send event via bridge
;------------------------------------------------------------------------------
DAP_SendEvent proc frame uses rbx rsi rdi,
    pEvent:ptr DAP_EVENT
    
    mov rbx, pEvent
    
    ; Serialize event to JSON
    mov rcx, rbx
    call DAP_SerializeEvent
    mov rsi, rax                ; JSON string
    
    ; Send as notification
    mov ecx, MSG_DAP_EVENT
    mov rdx, rsi
    mov r8d, -1                 ; length unknown, null-terminated
    call ExtensionHostBridge_SendMessage
    
    mov rax, 1
    ret
DAP_SendEvent endp

;------------------------------------------------------------------------------
; Serialize DAP event to JSON
;------------------------------------------------------------------------------
DAP_SerializeEvent proc frame uses rbx rsi rdi,
    pEvent:ptr DAP_EVENT
    
    mov rbx, pEvent
    
    ; Allocate JSON buffer
    sub rsp, 4096
    mov rdi, rsp
    
    ; Build JSON
    mov dword ptr [rdi], '{'
    inc rdi
    
    ; Add seq
    mov dword ptr [rdi], '"se'
    add rdi, 4
    mov dword ptr [rdi], 'q":'
    add rdi, 4
    
    mov ecx, [rbx].DAP_EVENT.Seq
    mov rdx, rdi
    call itoa
    add rdi, rax
    
    mov byte ptr [rdi], ','
    inc rdi
    
    ; Add type
    mov dword ptr [rdi], '"ty'
    add rdi, 4
    mov dword ptr [rdi], 'pe":'
    add rdi, 5
    mov dword ptr [rdi], '"ev'
    add rdi, 4
    mov byte ptr [rdi], 'ent"'
    add rdi, 5
    
    mov byte ptr [rdi], ','
    inc rdi
    
    ; Add event name
    mov dword ptr [rdi], '"ev'
    add rdi, 4
    mov dword ptr [rdi], 'ent":'
    add rdi, 6
    mov byte ptr [rdi], '"'
    inc rdi
    
    mov rcx, rdi
    mov rdx, [rbx].DAP_EVENT.EventString
    call strcpy
    add rdi, rax
    
    mov byte ptr [rdi], '"'
    inc rdi
    
    mov byte ptr [rdi], ','
    inc rdi
    
    ; Add body if present
    mov rcx, [rbx].DAP_EVENT.Body
    test rcx, rcx
    jz close_json
    
    mov dword ptr [rdi], '"bo'
    add rdi, 4
    mov dword ptr [rdi], 'dy":'
    add rdi, 5
    
    ; Copy body JSON
    mov rcx, rdi
    mov rdx, [rbx].DAP_EVENT.Body
    call strcpy
    add rdi, rax
    
close_json:
    mov byte ptr [rdi], '}'
    inc rdi
    mov byte ptr [rdi], 0
    
    mov rax, rsp
    ret
DAP_SerializeEvent endp

;==============================================================================
; UTILITY FUNCTIONS
;==============================================================================

; String constants
szKeyProgram            db 'program',0
szKeyArgs               db 'args',0
szKeyLine               db 'line',0
szKeyCondition          db 'condition',0
szKeyHitCondition       db 'hitCondition',0
szLocalsScope           db 'Locals',0
szRegistersScope        db 'Registers',0
szEventInitialized      db 'initialized',0
szEventThread           db 'thread',0
szEventBreakpoint       db 'breakpoint',0

; Constants
MAX_DAP_MESSAGES        equ 1024
MAX_BREAKPOINTS         equ 256
MAX_THREADS             equ 64
MAX_STACK_FRAMES        equ 256
MAX_VARIABLES           equ 1024

;==============================================================================
; EXPORTS
;==============================================================================
public DAP_Initialize
public DAP_CreateSession
public DAP_StartSession
public DAP_SetBreakpoints
public DAP_SetExceptionBreakpoints
public DAP_GetThreads
public DAP_GetStackTrace
public DAP_GetScopes
public DAP_GetVariables
public DAP_SendMessage
public DAP_SendEvent
public DAP_SendInitializedEvent
public DAP_SendThreadEvent
public DAP_SendBreakpointEvent

end
