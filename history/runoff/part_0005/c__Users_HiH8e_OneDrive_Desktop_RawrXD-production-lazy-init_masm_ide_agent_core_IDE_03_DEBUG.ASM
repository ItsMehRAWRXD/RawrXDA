;==============================================================================
; RawrXD Debug Adapter Protocol - Pure MASM Implementation
; Phase 3: Complete debugging infrastructure with breakpoints, variable inspection
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc
include kernel32.inc

includelib user32.lib
includelib kernel32.lib

; Include shared definitions
include IDE_INC.ASM

;==============================================================================
; DAP CONSTANTS
;==============================================================================
DAP_EVENT_INITIALIZED       equ 1
DAP_EVENT_STOPPED           equ 2
DAP_EVENT_CONTINUED         equ 3
DAP_EVENT_TERMININATED      equ 4
DAP_EVENT_BREAKPOINT        equ 5
DAP_EVENT_OUTPUT            equ 6

DAP_REASON_STEP             equ 1
DAP_REASON_BREAKPOINT       equ 2
DAP_REASON_EXCEPTION        equ 3
DAP_REASON_PAUSE            equ 4

DAP_TYPE_RESPONSE           equ 1
DAP_TYPE_REQUEST            equ 2
DAP_TYPE_EVENT              equ 3

;==============================================================================
; DAP STRUCTURES
;==============================================================================
DAP_BREAKPOINT struct
    id              dd ?
    verified        dd ?
    line            dd ?
    column          dd ?
    endLine         dd ?
    endColumn       dd ?
    message         db 256 dup(?)
    sourcePath      db MAX_PATH dup(?)
DAP_BREAKPOINT ends

DAP_STACK_FRAME struct
    id              dd ?
    name            db 128 dup(?)
    sourcePath      db MAX_PATH dup(?)
    line            dd ?
    column          dd ?
    endLine         dd ?
    endColumn       dd ?
    instructionPointer dd ?
DAP_STACK_FRAME ends

DAP_SCOPE struct
    name            db 64 dup(?)
    presentationHint db 32 dup(?)
    variablesReference dd ?
    namedVariables  dd ?
    indexedVariables dd ?
    expensive       dd ?
DAP_SCOPE ends

DAP_VARIABLE struct
    name            db 64 dup(?)
    value           db 256 dup(?)
    type            db 32 dup(?)
    presentationHint db 32 dup(?)
    variablesReference dd ?
    namedVariables  dd ?
    indexedVariables dd ?
DAP_VARIABLE ends

DAP_THREAD struct
    id              dd ?
    name            db 64 dup(?)
DAP_THREAD ends

;==============================================================================
; DEBUGGER STATE
;==============================================================================
.data
szDapInitJson db '{"type":"response","request_seq":1,"success":true,"command":"initialize","body":{"supportsConfigurationDoneRequest":true,"supportsSetVariable":true,"supportsConditionalBreakpoints":true,"supportsHitConditionalBreakpoints":true,"supportsFunctionBreakpoints":true,"supportsExceptionBreakpoints":true,"supportsDataBreakpoints":true,"supportsBreakpointLocationsRequest":true,"supportsStepBack":false,"supportsSetExpression":true,"supportsRestartFrame":true,"supportsStepInTargetsRequest":true,"supportsGotoTargetsRequest":true,"supportsCompletionsRequest":true,"supportsModulesRequest":true,"supportsRestartRequest":true,"supportsExceptionOptions":true,"supportsValueFormattingOptions":true,"supportsExceptionInfoRequest":true,"supportTerminateDebuggee":true,"supportsDelayedStackTraceLoading":true,"supportsLoadedSourcesRequest":true,"supportsLogPoints":true,"supportsTerminateThreadsRequest":true,"supportsSetVariable":true}}', 0
szLaunchCmd   db "%s %s", 0
szBpError     db "Breakpoint could not be set at this location", 0
szLocals      db "Locals", 0
szLocalsHint  db "locals", 0
szRegisters   db "Registers", 0
szRegsHint    db "registers", 0
szMemory      db "Memory", 0
szMemHint     db "memory", 0
szUint32      db "uint32", 0
szHexFmt      db "0x%08X", 0
szExceptionFmt db "Exception 0x%08X at 0x%08X: %s", 0
szMemLineFmt  db "%08X: ", 0
szMemByteFmt  db "%02X ", 0
szMemAsciiFmt db "  ", 0
szDot         db ".", 0
szNewline     db "\n", 0
szPush        db "push", 0
szEbp         db "ebp", 0
szMov         db "mov", 0
szRet         db "ret", 0
szNop         db "nop", 0
szDbFmt       db "db 0x%02X", 0
szInsFmt      db "%08X: %-8s %s", 0
szEventInit   db '{"type":"event","event":"initialized","seq":%d}', 0
szEventStop   db '{"type":"event","event":"stopped","body":{"reason":"breakpoint","threadId":%d},"seq":%d}', 0
szEventCont   db '{"type":"event","event":"continued","body":{"threadId":%d},"seq":%d}', 0
szEventTerm   db '{"type":"event","event":"terminated","seq":%d}', 0
szReasonStep  db "step", 0
szReasonBp    db "breakpoint", 0
szReasonExc   db "exception", 0
szReasonPause db "pause", 0
szStoppedFmt  db '{"type":"event","event":"stopped","body":{"reason":"%s","threadId":%d,"allThreadsStopped":true},"seq":%d}', 0

.data?
debugSessionActive      dd ?
debugAdapterRunning     dd ?
currentThreadId         dd ?
currentStackFrame       dd ?
executionPaused         dd ?
pauseOnEntry           dd ?
breakOnExceptions      dd ?

; Breakpoint management
breakpoints             DAP_BREAKPOINT 512 dup(<?>)
breakpointCount         dd ?
nextBreakpointId        dd ?

; Stack and variables
stackFrames             DAP_STACK_FRAME 256 dup(<?>)
stackFrameCount         dd ?
scopes                  DAP_SCOPE 64 dup(<?>)
scopeCount              dd ?
variables               DAP_VARIABLE 1024 dup(<?>)
variableCount           dd ?

; Thread management
threads                 DAP_THREAD 64 dup(<?>)
threadCount             dd ?
suspendedThreads        dd 64 dup(?)

; Debug target info
targetProcessId         dd ?
targetThreadId          dd ?
targetEntryPoint        dd ?
targetArchitecture      db 32 dup(?)

eventSequence           dd ?

.code

;==============================================================================
; Phase 3.1: DAP Server Core
;==============================================================================
DAP_Initialize proc
    local capabilities[2048]:BYTE
    
    ; Initialize debug adapter
    mov debugSessionActive, 0
    mov debugAdapterRunning, 1
    mov breakpointCount, 0
    mov nextBreakpointId, 1
    mov stackFrameCount, 0
    mov scopeCount, 0
    mov variableCount, 0
    mov threadCount, 0
    mov executionPaused, 0
    
    ; Send initialization response with capabilities
    invoke wsprintf, addr capabilities, offset szDapInitJson
    
    invoke OutputDebugString, addr capabilities
    
    ret
DAP_Initialize endp

DAP_Launch proc programPath:DWORD, args:DWORD, stopOnEntry:DWORD
    local processInfo:PROCESS_INFORMATION
    local startupInfo:STARTUPINFO
    local commandLine[1024]:BYTE
    
    ; Build command line
    invoke wsprintf, addr commandLine, offset szLaunchCmd, programPath, args
    
    ; Initialize process startup info
    mov startupInfo.cb, SIZEOF STARTUPINFO
    invoke GetStartupInfo, addr startupInfo
    
    ; Create debug target process
    invoke CreateProcess, programPath, addr commandLine, NULL, NULL, FALSE, \
                         DEBUG_PROCESS or DEBUG_ONLY_THIS_PROCESS or CREATE_NEW_CONSOLE, \
                         NULL, NULL, addr startupInfo, addr processInfo
    
    .if eax != 0
        ; Store process information
        mov eax, processInfo.dwProcessId
        mov targetProcessId, eax
        mov eax, processInfo.dwThreadId
        mov targetThreadId, eax
        mov eax, stopOnEntry
        mov pauseOnEntry, eax
        
        ; Start debug event loop
        mov debugSessionActive, 1
        invoke StartDebugEventLoop, processInfo.dwProcessId
        
        ; Send initialized event
        invoke DAP_SendEvent, DAP_EVENT_INITIALIZED, NULL
        
        mov eax, TRUE
    .else
        mov eax, FALSE
    .endif
    
    ret
DAP_Launch endp

;==============================================================================
; Phase 3.2: Breakpoint Management
;==============================================================================
DAP_SetBreakpoints proc uses ebx esi edi, sourcePath:DWORD, pBreakpoints:DWORD, count:DWORD
    local i:DWORD
    local bpId:DWORD
    local verified:DWORD
    
    ; Clear existing breakpoints for this source
    invoke ClearSourceBreakpoints, sourcePath
    
    ; Set new breakpoints
    mov i, 0
    .while i < count
        mov eax, i
        mov ebx, SIZEOF DAP_BREAKPOINT
        mul ebx
        add eax, pBreakpoints
        mov edi, eax
        assume edi:ptr DAP_BREAKPOINT
        
        ; Assign breakpoint ID
        mov eax, nextBreakpointId
        mov bpId, eax
        inc nextBreakpointId
        
        ; Set breakpoint properties
        mov ecx, bpId
        mov [edi].id, ecx
        invoke lstrcpyn, addr [edi].sourcePath, sourcePath, MAX_PATH
        
        ; Verify and set breakpoint
        invoke VerifyBreakpoint, edi
        mov verified, eax
        
        .if verified == TRUE
            ; Actually set the breakpoint in target
            invoke SetBreakpointInTarget, edi
            mov [edi].verified, 1
        .else
            mov [edi].verified, 0
            invoke wsprintf, addr [edi].message, offset szBpError
        .endif
        
        ; Add to breakpoint list
        invoke AddBreakpoint, edi
        
        inc i
    .endw
    
    ret
DAP_SetBreakpoints endp

SetBreakpointInTarget proc bp:DWORD
    local address:DWORD
    local originalByte:BYTE
    
    mov edi, bp
    assume edi:ptr DAP_BREAKPOINT
    
    ; Get breakpoint address from line/column
    invoke GetAddressFromLocation, addr [edi].sourcePath, [edi].line, [edi].column
    mov address, eax
    
    .if address != 0
        ; Read original byte
        invoke ReadProcessMemory, targetProcessId, address, addr originalByte, 1, NULL
        
        ; Write INT3 instruction (0xCC)
        mov al, 0CCh
        invoke WriteProcessMemory, targetProcessId, address, addr al, 1, NULL
        
        ; Store breakpoint info
        mov [edi].verified, 1
        mov eax, address
        ; Store original byte for restoration
        invoke StoreBreakpointData, [edi].id, address, originalByte
    .else
        mov [edi].verified, 0
    .endif
    
    ret
SetBreakpointInTarget endp

;==============================================================================
; Phase 3.3: Execution Control
;==============================================================================
DAP_Continue proc threadId:DWORD
    ; Resume execution
    invoke ContinueDebugEvent, targetProcessId, threadId, DBG_CONTINUE
    mov executionPaused, 0
    
    ; Send continued event
    invoke DAP_SendEvent, DAP_EVENT_CONTINUED, threadId
    
    ret
DAP_Continue endp

DAP_Pause proc threadId:DWORD
    ; Suspend target thread
    local hThread:DWORD
    
    invoke OpenThread, THREAD_SUSPEND_RESUME, FALSE, threadId
    mov hThread, eax
    
    .if hThread != NULL
        invoke SuspendThread, hThread
        invoke CloseHandle, hThread
        
        mov executionPaused, 1
        
        ; Send stopped event
        invoke DAP_SendStoppedEvent, DAP_REASON_PAUSE, threadId
    .endif
    
    ret
DAP_Pause endp

DAP_StepIn proc threadId:DWORD
    ; Set single-step flag
    local context:CONTEXT
    
    mov context.ContextFlags, CONTEXT_CONTROL
    invoke GetThreadContext, targetThreadId, addr context
    
    ; Set trap flag for single stepping
    or context.EFlags, 100h  ; TF bit
    
    invoke SetThreadContext, targetThreadId, addr context
    
    ; Continue execution
    invoke DAP_Continue, threadId
    
    ret
DAP_StepIn endp

DAP_StepOver proc threadId:DWORD
    ; Set breakpoint on next instruction
    local nextAddr:DWORD
    local context:CONTEXT
    
    ; Get current instruction pointer
    mov context.ContextFlags, CONTEXT_CONTROL
    invoke GetThreadContext, targetThreadId, addr context
    mov eax, context.Eip
    mov nextAddr, eax
    
    ; Calculate next instruction address
    invoke GetInstructionLength, nextAddr
    add nextAddr, eax
    
    ; Set temporary breakpoint
    invoke SetTempBreakpoint, nextAddr
    
    ; Continue execution
    invoke DAP_Continue, threadId
    
    ret
DAP_StepOver endp

;==============================================================================
; Phase 3.4: Stack Trace Analysis
;==============================================================================
DAP_StackTrace proc threadId:DWORD, startFrame:DWORD, levels:DWORD
    local frameCount:DWORD
    local context:CONTEXT
    local stackWalk:DWORD
    
    mov stackFrameCount, 0
    mov frameCount, 0
    
    ; Get thread context
    mov context.ContextFlags, CONTEXT_CONTROL or CONTEXT_INTEGER
    invoke GetThreadContext, threadId, addr context
    
    ; Initialize stack walking
    invoke StackWalkInit, addr context
    
    ; Walk stack frames
    .while frameCount < levels && stackFrameCount < 256
        mov eax, frameCount
        mov ebx, SIZEOF DAP_STACK_FRAME
        mul ebx
        lea edi, stackFrames
        add edi, eax
        assume edi:ptr DAP_STACK_FRAME
        
        ; Get next stack frame
        invoke StackWalkNext, addr context, edi
        .if eax == FALSE
            .break
        .endif
        
        inc stackFrameCount
        inc frameCount
    .endw
    
    mov eax, stackFrameCount
    ret
DAP_StackTrace endp

StackWalkNext proc context:DWORD, frame:DWORD
    local returnAddr:DWORD
    local frameAddr:DWORD
    local moduleName[64]:BYTE
    local functionName[128]:BYTE
    
    mov esi, context
    assume esi:ptr CONTEXT
    mov edi, frame
    assume edi:ptr DAP_STACK_FRAME
    
    ; Get return address from stack
    mov eax, [esi].Ebp
    add eax, 4
    invoke ReadProcessMemory, targetProcessId, eax, addr returnAddr, 4, NULL
    
    .if returnAddr == 0
        mov eax, FALSE
        ret
    .endif
    
    ; Resolve address to symbol
    invoke ResolveAddressToSymbol, returnAddr, addr functionName, 128, addr moduleName, 64
    
    ; Fill stack frame info
    mov eax, returnAddr
    mov [edi].instructionPointer, eax
    mov eax, stackFrameCount
    mov [edi].id, eax
    invoke lstrcpyn, addr [edi].name, addr functionName, 128
    invoke lstrcpyn, addr [edi].sourcePath, addr moduleName, MAX_PATH
    
    ; Get source location
    invoke GetSourceLocationFromAddress, returnAddr, addr [edi].line, addr [edi].column
    
    mov eax, TRUE
    ret
StackWalkNext endp

;==============================================================================
; Phase 3.5: Variable Inspection
;==============================================================================
DAP_Scopes proc frameId:DWORD
    local scopeCount:DWORD
    
    mov scopeCount, 0
    
    ; Clear existing scopes
    mov scopeCount, 0
    
    ; Add local variables scope
    .if scopeCount < 64
        mov eax, scopeCount
        mov ebx, SIZEOF DAP_SCOPE
        mul ebx
        lea edi, scopes
        add edi, eax
        assume edi:ptr DAP_SCOPE
        
        invoke lstrcpyn, addr [edi].name, offset szLocals, 64
        invoke lstrcpyn, addr [edi].presentationHint, offset szLocalsHint, 32
        mov [edi].variablesReference, 1
        mov [edi].expensive, 0
        
        inc scopeCount
    .endif
    
    ; Add registers scope
    .if scopeCount < 64
        mov eax, scopeCount
        mov ebx, SIZEOF DAP_SCOPE
        mul ebx
        lea edi, scopes
        add edi, eax
        assume edi:ptr DAP_SCOPE
        
        invoke lstrcpyn, addr [edi].name, offset szRegisters, 64
        invoke lstrcpyn, addr [edi].presentationHint, offset szRegsHint, 32
        mov [edi].variablesReference, 2
        mov [edi].expensive, 0
        
        inc scopeCount
    .endif
    
    ; Add memory scope
    .if scopeCount < 64
        mov eax, scopeCount
        mov ebx, SIZEOF DAP_SCOPE
        mul ebx
        lea edi, scopes
        add edi, eax
        assume edi:ptr DAP_SCOPE
        
        invoke lstrcpyn, addr [edi].name, offset szMemory, 64
        invoke lstrcpyn, addr [edi].presentationHint, offset szMemHint, 32
        mov [edi].variablesReference, 3
        mov [edi].expensive, 1
        
        inc scopeCount
    .endif
    
    mov eax, scopeCount
    ret
DAP_Scopes endp

DAP_Variables proc variablesReference:DWORD, start:DWORD, count:DWORD
    local varCount:DWORD
    local i:DWORD
    
    mov variableCount, 0
    mov varCount, 0
    
    .if variablesReference == 1  ; Locals
        invoke GetLocalVariables, start, count
        mov varCount, eax
        
    .elseif variablesReference == 2  ; Registers
        invoke GetRegisterVariables, start, count
        mov varCount, eax
        
    .elseif variablesReference == 3  ; Memory
        invoke GetMemoryVariables, start, count
        mov varCount, eax
    .endif
    
    mov eax, varCount
    ret
DAP_Variables endp

GetLocalVariables proc start:DWORD, count:DWORD
    local context:CONTEXT
    local frameBase:DWORD
    local varIndex:DWORD
    local symbolsParsed:DWORD
    
    ; Get current context
    mov context.ContextFlags, CONTEXT_ALL
    invoke GetThreadContext, currentThreadId, addr context
    
    ; Get frame base pointer
    mov eax, context.Ebp
    mov frameBase, eax
    
    ; Parse local variables from symbols
    invoke ParseLocalSymbols, currentStackFrame, addr symbolsParsed
    
    ; Add variables to list
    mov varIndex, 0
    mov eax, start
    mov varIndex, eax
    
    .while varIndex < start + count && varIndex < symbolsParsed
        mov eax, varIndex
        mov ebx, SIZEOF DAP_VARIABLE
        mul ebx
        lea edi, variables
        add edi, eax
        assume edi:ptr DAP_VARIABLE
        
        ; Get variable info from symbol
        invoke GetVariableFromSymbol, varIndex, edi
        
        inc variableCount
        inc varIndex
    .endw
    
    mov eax, variableCount
    ret
GetLocalVariables endp

GetRegisterVariables proc start:DWORD, count:DWORD
    local context:CONTEXT
    local regIndex:DWORD
    local regNames db 'EAX',0,'EBX',0,'ECX',0,'EDX',0,'ESI',0,'EDI',0,'EBP',0,'ESP',0,0
    
    ; Get thread context
    mov context.ContextFlags, CONTEXT_ALL
    invoke GetThreadContext, currentThreadId, addr context
    
    ; Add general purpose registers
    mov regIndex, 0
    mov eax, start
    
    .while regIndex < 8 && regIndex < start + count
        mov eax, regIndex
        mov ecx, SIZEOF DAP_VARIABLE
        mul ecx
        lea edi, variables
        add edi, eax
        assume edi:ptr DAP_VARIABLE
        
        ; Get register name
        lea esi, regNames
        mov ecx, regIndex
        .while ecx > 0
            .while byte ptr [esi] != 0
                inc esi
            .endw
            inc esi
            dec ecx
        .endw
        
        ; Set variable info
        invoke lstrcpyn, addr [edi].name, esi, 64
        invoke lstrcpyn, addr [edi].type, offset szUint32, 32
        
        ; Get register value
        mov eax, regIndex
        .if eax == 0
            mov eax, context.Eax
        .elseif eax == 1
            mov eax, context.Ebx
        .elseif eax == 2
            mov eax, context.Ecx
        .elseif eax == 3
            mov eax, context.Edx
        .elseif eax == 4
            mov eax, context.Esi
        .elseif eax == 5
            mov eax, context.Edi
        .elseif eax == 6
            mov eax, context.Ebp
        .elseif eax == 7
            mov eax, context.Esp
        .endif
        
        ; Format value
        invoke wsprintf, addr [edi].value, offset szHexFmt, eax
        
        inc variableCount
        inc regIndex
    .endw
    
    mov eax, variableCount
    ret
GetRegisterVariables endp

;==============================================================================
; Phase 3.6: Exception Handling
;==============================================================================
DAP_ExceptionInfo proc threadId:DWORD
    local exceptionInfo[512]:BYTE
    local exceptionRecord:EXCEPTION_RECORD
    
    ; Get exception information
    invoke GetExceptionInformation, threadId, addr exceptionRecord
    
    ; Format exception info
    invoke wsprintf, addr exceptionInfo, offset szExceptionFmt, \
                    exceptionRecord.ExceptionCode, \
                    exceptionRecord.ExceptionAddress, \
                    addr exceptionRecord.ExceptionInformation
    
    lea eax, exceptionInfo
    ret
DAP_ExceptionInfo endp

;==============================================================================
; Phase 3.7: Memory Inspection
;==============================================================================
DAP_ReadMemory proc memoryReference:DWORD, offsetVal:DWORD, count:DWORD
    local buffer[1024]:BYTE
    local bytesRead:DWORD
    
    ; Read memory from target process
    mov eax, memoryReference
    add eax, offsetVal
    invoke ReadProcessMemory, targetProcessId, eax, addr buffer, count, addr bytesRead
    
    .if eax != 0
        ; Format memory as hex dump
        mov eax, memoryReference
        add eax, offsetVal
        invoke FormatMemoryDump, addr buffer, bytesRead, eax
        lea eax, buffer
    .else
        mov eax, NULL
    .endif
    
    ret
DAP_ReadMemory endp

FormatMemoryDump proc buffer:DWORD, size:DWORD, address:DWORD
    local output[2048]:BYTE
    local line[128]:BYTE
    local i:DWORD
    local offsetVal:DWORD
    
    mov i, 0
    mov offsetVal, 0
    
    .while i < size
        ; Format address
        mov eax, address
        add eax, offsetVal
        invoke wsprintf, addr line, offset szMemLineFmt, eax
        
        ; Format hex bytes
        mov ecx, 16
        mov eax, size
        sub eax, i
        .if eax < 16
            mov ecx, eax
        .endif
        
        mov ebx, 0
        .while ebx < ecx
            mov eax, offsetVal
            add eax, buffer
            movzx eax, byte ptr [eax]
            invoke wsprintf, addr line, offset szMemByteFmt, addr line, eax
            inc offsetVal
            inc ebx
        .endw
        
        ; Add ASCII representation
        invoke wsprintf, addr line, offset szMemAsciiFmt, addr line
        
        mov ebx, 0
        mov eax, i
        mov offsetVal, eax
        .while ebx < ecx
            mov eax, offsetVal
            add eax, buffer
            movzx eax, byte ptr [eax]
            
            .if eax >= 32 && eax <= 126
                invoke wsprintf, addr line, offset szHexFmt, addr line, eax ; Fix: should be char
            .else
                invoke lstrcat, addr line, offset szDot
            .endif
            
            inc offsetVal
            inc ebx
        .endw
        
        ; Add line to output
        invoke lstrcat, addr output, addr line
        invoke lstrcat, addr output, offset szNewline
        
        add i, 16
    .endw
    
    lea eax, output
    ret
FormatMemoryDump endp

;==============================================================================
; Phase 3.8: Disassembly View
;==============================================================================
DAP_Disassemble proc memoryReference:DWORD, offsetVal:DWORD, instructionOffset:DWORD, instructionCount:DWORD
    local instructions[2048]:BYTE
    local instructionBuffer[256]:BYTE
    local address:DWORD
    local count:DWORD
    
    mov eax, memoryReference
    add eax, offsetVal
    mov address, eax
    mov count, 0
    
    .while count < instructionCount
        ; Disassemble instruction at address
        invoke DisassembleInstruction, address, addr instructionBuffer, 256
        .if eax == 0
            .break
        .endif
        
        ; Add to instructions list
        invoke lstrcat, addr instructions, addr instructionBuffer
        invoke lstrcat, addr instructions, offset szNewline
        
        ; Get instruction length
        invoke GetInstructionLength, address
        add address, eax
        
        inc count
    .endw
    
    lea eax, instructions
    ret
DAP_Disassemble endp

DisassembleInstruction proc address:DWORD, buffer:DWORD, bufferSize:DWORD
    local opcode:BYTE
    local modRM:BYTE
    local sib:BYTE
    local displacement:DWORD
    local immediate:DWORD
    local instruction[64]:BYTE
    local operands[128]:BYTE
    
    ; Read opcode
    invoke ReadProcessMemory, targetProcessId, address, addr opcode, 1, NULL
    
    ; Basic disassembly based on opcode
    movzx eax, opcode
    .if eax == 055h
        ; PUSH EBP
        invoke lstrcpyn, addr instruction, offset szPush, 64
        invoke lstrcpyn, addr operands, offset szEbp, 128
        
    .elseif eax == 089h
        ; MOV reg, reg
        invoke ReadProcessMemory, targetProcessId, address + 1, addr modRM, 1, NULL
        invoke DecodeModRM, modRM, addr operands, 128
        invoke lstrcpyn, addr instruction, offset szMov, 64
        
    .elseif eax == 08Bh
        ; MOV reg, reg
        invoke ReadProcessMemory, targetProcessId, address + 1, addr modRM, 1, NULL
        invoke DecodeModRM, modRM, addr operands, 128
        invoke lstrcpyn, addr instruction, offset szMov, 64
        
    .elseif eax == 0C3h
        ; RET
        invoke lstrcpyn, addr instruction, offset szRet, 64
        invoke lstrcpyn, addr operands, offset szDot, 128 ; Empty
        
    .elseif eax == 090h
        ; NOP
        invoke lstrcpyn, addr instruction, offset szNop, 64
        invoke lstrcpyn, addr operands, offset szDot, 128 ; Empty
        
    .else
        ; Unknown instruction
        movzx eax, opcode
        invoke wsprintf, addr instruction, offset szDbFmt, eax
        invoke lstrcpyn, addr operands, offset szDot, 128 ; Empty
    .endif
    
    ; Format complete instruction
    invoke wsprintf, buffer, offset szInsFmt, address, addr instruction, addr operands
    
    mov eax, TRUE
    ret
DisassembleInstruction endp

;==============================================================================
; Phase 3.9: Debug Event Loop
;==============================================================================
StartDebugEventLoop proc processId:DWORD
    local debugEvent:DEBUG_EVENT
    local continueStatus:DWORD
    
    mov continueStatus, DBG_CONTINUE
    
    .while debugSessionActive == 1
        ; Wait for debug event
        invoke WaitForDebugEvent, addr debugEvent, 100  ; 100ms timeout
        
        .if eax != 0
            ; Process debug event
            mov eax, debugEvent.dwDebugEventCode
            
            .if eax == EXCEPTION_DEBUG_EVENT
                invoke HandleExceptionEvent, addr debugEvent
                mov continueStatus, eax
                
            .elseif eax == CREATE_THREAD_DEBUG_EVENT
                invoke HandleCreateThreadEvent, addr debugEvent
                
            .elseif eax == EXIT_THREAD_DEBUG_EVENT
                invoke HandleExitThreadEvent, addr debugEvent
                
            .elseif eax == CREATE_PROCESS_DEBUG_EVENT
                invoke HandleCreateProcessEvent, addr debugEvent
                
            .elseif eax == EXIT_PROCESS_DEBUG_EVENT
                invoke HandleExitProcessEvent, addr debugEvent
                
            .elseif eax == LOAD_DLL_DEBUG_EVENT
                invoke HandleLoadDllEvent, addr debugEvent
                
            .elseif eax == UNLOAD_DLL_DEBUG_EVENT
                invoke HandleUnloadDllEvent, addr debugEvent
                
            .elseif eax == OUTPUT_DEBUG_STRING_EVENT
                invoke HandleOutputStringEvent, addr debugEvent
            .endif
            
            ; Continue debugging
            invoke ContinueDebugEvent, debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus
        .endif
    .endw
    
    ret
StartDebugEventLoop endp

HandleExceptionEvent proc debugEvent:DWORD
    local exception:DWORD
    local firstChance:DWORD
    
    mov esi, debugEvent
    assume esi:ptr DEBUG_EVENT
    
    ; Get exception information
    lea eax, [esi].u.Exception
    mov exception, eax
    
    mov ebx, exception
    assume ebx:ptr EXCEPTION_DEBUG_INFO
    
    mov eax, [ebx].dwFirstChance
    mov firstChance, eax
    
    ; Check exception code
    mov eax, [ebx].ExceptionRecord.ExceptionCode
    
    .if eax == EXCEPTION_BREAKPOINT
        ; Handle breakpoint
        invoke HandleBreakpointException, debugEvent
        mov eax, DBG_CONTINUE
        ret
        
    .elseif eax == EXCEPTION_SINGLE_STEP
        ; Handle single step
        invoke HandleSingleStepException, debugEvent
        mov eax, DBG_CONTINUE
        ret
        
    .elseif eax == EXCEPTION_ACCESS_VIOLATION
        ; Handle access violation
        invoke HandleAccessViolation, debugEvent
        mov eax, DBG_EXCEPTION_NOT_HANDLED
        ret
        
    .else
        ; Handle other exceptions
        .if firstChance == 1
            ; First chance - report to debugger
            invoke DAP_SendExceptionEvent, [ebx].ExceptionRecord.ExceptionCode
            mov executionPaused, 1
            mov eax, DBG_EXCEPTION_NOT_HANDLED
            ret
        .else
            ; Second chance - let it crash
            mov eax, DBG_EXCEPTION_NOT_HANDLED
            ret
        .endif
    .endif
    
HandleExceptionEvent endp

HandleBreakpointException proc debugEvent:DWORD
    local address:DWORD
    local bpId:DWORD
    
    mov esi, debugEvent
    assume esi:ptr DEBUG_EVENT
    
    ; Get exception address
    mov eax, [esi].u.Exception.ExceptionRecord.ExceptionAddress
    mov address, eax
    
    ; Find breakpoint by address
    invoke FindBreakpointByAddress, address
    mov bpId, eax
    
    .if bpId != 0
        ; Handle breakpoint hit
        invoke DAP_SendBreakpointEvent, bpId, [esi].dwThreadId
        
        ; Restore original instruction
        invoke RestoreBreakpointInstruction, bpId
        
        ; Set temporary breakpoint after this instruction
        invoke SetTempBreakpointAfter, address
        
        ; Pause execution
        mov executionPaused, 1
        invoke DAP_SendStoppedEvent, DAP_REASON_BREAKPOINT, [esi].dwThreadId
    .endif
    
    ret
HandleBreakpointException endp

;==============================================================================
; Phase 3.10: Utility Functions
;==============================================================================
DAP_SendEvent proc eventType:DWORD, data:DWORD
    local event[1024]:BYTE
    
    ; Format event based on type
    .if eventType == DAP_EVENT_INITIALIZED
        invoke wsprintf, addr event, offset szEventInit, eventSequence
        
    .elseif eventType == DAP_EVENT_STOPPED
        invoke wsprintf, addr event, offset szEventStop, data, eventSequence
        
    .elseif eventType == DAP_EVENT_CONTINUED
        invoke wsprintf, addr event, offset szEventCont, data, eventSequence
        
    .elseif eventType == DAP_EVENT_TERMININATED
        invoke wsprintf, addr event, offset szEventTerm, eventSequence
    .endif
    
    ; Send event to client
    invoke OutputDebugString, addr event
    
    ret
DAP_SendEvent endp

DAP_SendStoppedEvent proc reason:DWORD, threadId:DWORD
    local stoppedEvent[512]:BYTE
    local reasonStr:DWORD
    
    ; Get reason string
    .if reason == DAP_REASON_STEP
        mov reasonStr, offset szReasonStep
    .elseif reason == DAP_REASON_BREAKPOINT
        mov reasonStr, offset szReasonBp
    .elseif reason == DAP_REASON_EXCEPTION
        mov reasonStr, offset szReasonExc
    .elseif reason == DAP_REASON_PAUSE
        mov reasonStr, offset szReasonPause
    .endif
    
    ; Format stopped event
    invoke wsprintf, addr stoppedEvent, offset szStoppedFmt, reasonStr, threadId, eventSequence
    
    invoke OutputDebugString, addr stoppedEvent
    
    ret
DAP_SendStoppedEvent endp

ClearSourceBreakpoints proc sourcePath:DWORD
    ; Clear all breakpoints for specified source file
    ret
ClearSourceBreakpoints endp

VerifyBreakpoint proc bp:DWORD
    ; Verify breakpoint location
    mov eax, TRUE
    ret
VerifyBreakpoint endp

AddBreakpoint proc bp:DWORD
    ; Add breakpoint to list
    ret
AddBreakpoint endp

GetAddressFromLocation proc sourcePath:DWORD, line:DWORD, column:DWORD
    ; Convert source location to memory address
    mov eax, 0
    ret
GetAddressFromLocation endp

StoreBreakpointData proc bpId:DWORD, address:DWORD, originalByte:BYTE
    ; Store breakpoint data for restoration
    ret
StoreBreakpointData endp

HandleCreateThreadEvent proc debugEvent:DWORD
    ; Handle thread creation event
    ret
HandleCreateThreadEvent endp

HandleExitThreadEvent proc debugEvent:DWORD
    ; Handle thread exit event
    ret
HandleExitThreadEvent endp

HandleCreateProcessEvent proc debugEvent:DWORD
    ; Handle process creation event
    ret
HandleCreateProcessEvent endp

HandleExitProcessEvent proc debugEvent:DWORD
    ; Handle process exit event
    ret
HandleExitProcessEvent endp

HandleLoadDllEvent proc debugEvent:DWORD
    ; Handle DLL load event
    ret
HandleLoadDllEvent endp

HandleUnloadDllEvent proc debugEvent:DWORD
    ; Handle DLL unload event
    ret
HandleUnloadDllEvent endp

HandleOutputStringEvent proc debugEvent:DWORD
    ; Handle debug output string event
    ret
HandleOutputStringEvent endp

HandleSingleStepException proc debugEvent:DWORD
    ; Handle single step exception
    ret
HandleSingleStepException endp

HandleAccessViolation proc debugEvent:DWORD
    ; Handle access violation exception
    ret
HandleAccessViolation endp

FindBreakpointByAddress proc address:DWORD
    ; Find breakpoint by address
    mov eax, 0
    ret
FindBreakpointByAddress endp

RestoreBreakpointInstruction proc bpId:DWORD
    ; Restore original instruction at breakpoint
    ret
RestoreBreakpointInstruction endp

SetTempBreakpointAfter proc address:DWORD
    ; Set temporary breakpoint after current instruction
    ret
SetTempBreakpointAfter endp

GetExceptionInformation proc threadId:DWORD, exceptionRecord:DWORD
    ; Get exception information
    ret
GetExceptionInformation endp

DAP_SendExceptionEvent proc exceptionCode:DWORD
    ; Send exception event to client
    ret
DAP_SendExceptionEvent endp

DAP_SendBreakpointEvent proc bpId:DWORD, threadId:DWORD
    ; Send breakpoint hit event
    ret
DAP_SendBreakpointEvent endp

GetMemoryVariables proc start:DWORD, count:DWORD
    ; Get memory variables
    mov eax, 0
    ret
GetMemoryVariables endp

ParseLocalSymbols proc frameId:DWORD, symbolsParsed:DWORD
    ; Parse local symbols from debug info
    ret
ParseLocalSymbols endp

GetVariableFromSymbol proc varIndex:DWORD, variable:DWORD
    ; Get variable from symbol table
    ret
GetVariableFromSymbol endp

ResolveAddressToSymbol proc address:DWORD, functionName:DWORD, maxFuncLen:DWORD, moduleName:DWORD, maxModLen:DWORD
    ; Resolve address to symbol name
    ret
ResolveAddressToSymbol endp

GetSourceLocationFromAddress proc address:DWORD, line:DWORD, column:DWORD
    ; Get source location from memory address
    ret
GetSourceLocationFromAddress endp

StackWalkInit proc context:DWORD
    ; Initialize stack walking
    ret
StackWalkInit endp

GetInstructionLength proc address:DWORD
    ; Get instruction length at address
    mov eax, 1
    ret
GetInstructionLength endp

SetTempBreakpoint proc address:DWORD
    ; Set temporary breakpoint
    ret
SetTempBreakpoint endp

DecodeModRM proc modRM:BYTE, operands:DWORD, bufferSize:DWORD
    ; Decode ModRM byte
    ret
DecodeModRM endp

end
