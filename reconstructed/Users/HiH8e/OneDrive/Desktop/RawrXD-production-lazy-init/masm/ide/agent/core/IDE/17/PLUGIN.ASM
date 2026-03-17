;  ============================================================================
;  IDE_17_PLUGIN.asm  –  Tool dispatch system with 44-tool switch table
;  ============================================================================
;  Implements:
;    - AgentTool_Dispatch (route to 44 different tools)
;    - AgentTool_ResultToMemory (auto-log outputs)
;    - PluginSystem_Load / Unload
;  ============================================================================

include ..\agent_core\IDE_INC.ASM

; ============================================================================
;  EXPORTS
; ============================================================================
PUBLIC AgentTool_Dispatch
PUBLIC AgentTool_ResultToMemory
PUBLIC PluginSystem_Load
PUBLIC PluginSystem_Unload

; ============================================================================
;  CONSTANTS
; ============================================================================
MAX_TOOLS         equ 44
MAX_TOOL_NAME     equ 64
MAX_RESULT_LEN    equ 2048

; ============================================================================
;  TOOL ENTRY STRUCTURE
; ============================================================================
TOOL_ENTRY STRUCT
    name        BYTE MAX_TOOL_NAME dup(?)
    func_ptr    DWORD ?
    enabled     DWORD ?
    call_count  DWORD ?
TOOL_ENTRY ENDS

; ============================================================================
;  DATA
; ============================================================================
.data
g_tool_table    TOOL_ENTRY MAX_TOOLS dup(<>)
g_tool_count    DWORD 0

szToolDispatch  db "AgentTool_Dispatch", 0
szToolResult    db "AgentTool_ResultToMemory", 0
szPluginLoad    db "PluginSystem_Load", 0
szPluginUnload  db "PluginSystem_Unload", 0

; Tool names (for registration)
tool_names      db "read_file", 0
                db "write_file", 0
                db "list_directory", 0
                db "search_code", 0
                db "run_command", 0
                db "http_get", 0
                db "http_post", 0
                db "parse_json", 0
                db "format_code", 0
                db "compile_code", 0
                ; ... (34 more tool names)

; ============================================================================
;  CODE
; ============================================================================
.code

; ============================================================================
;  PluginSystem_Load – Load a plugin/tool DLL
; ============================================================================
PluginSystem_Load PROC uses ebx esi, pPath:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL hModule:DWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    .if pPath == 0
        mov hr, E_INVALIDARG
        xor eax, eax
        mov hModule, eax
        jmp done
    .endif
    
    ; Load DLL
    invoke LoadLibraryA, pPath
    .if eax == NULL
        call _get_hresult
        mov hr, eax
        xor eax, eax
        mov hModule, eax
        jmp done
    .endif
    
    mov hModule, eax
    mov hr, 0
    
    ; TODO: GetProcAddress for standard entry points
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szPluginLoad, hr, dword ptr [dur]
    
    mov eax, hModule
    ret
PluginSystem_Load ENDP

; ============================================================================
;  PluginSystem_Unload – Unload a plugin
; ============================================================================
PluginSystem_Unload PROC uses ebx, hModule:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    
    PERF_INIT offset start
    call _enter_crit
    
    .if hModule == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    invoke FreeLibrary, hModule
    .if eax == 0
        call _get_hresult
        mov hr, eax
        jmp done
    .endif
    
    mov hr, 0
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szPluginUnload, hr, dword ptr [dur]
    
    mov eax, hr
    ret
PluginSystem_Unload ENDP

; ============================================================================
;  AgentTool_Dispatch – Dispatch to registered tool
;  Input:  toolId = tool index (0-43)
;          pInput = input JSON
;          pOutput = output buffer
;          cbOutput = output buffer size
;  Output: eax = HRESULT
; ============================================================================
AgentTool_Dispatch PROC uses ebx esi edi, toolId:DWORD, pInput:DWORD, pOutput:DWORD, cbOutput:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL pTool:PTR TOOL_ENTRY
    
    PERF_INIT offset start
    call _enter_crit
    
    ; Validate tool ID
    mov eax, toolId
    .if eax >= MAX_TOOLS
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; Get tool entry
    mov ecx, toolId
    imul ecx, sizeof TOOL_ENTRY
    lea esi, g_tool_table
    add esi, ecx
    mov pTool, esi
    
    ; Check if enabled
    .if (TOOL_ENTRY PTR [esi]).enabled == 0
        mov hr, E_NOTIMPL
        jmp done
    .endif
    
    ; Check if function pointer is set
    .if (TOOL_ENTRY PTR [esi]).func_ptr == 0
        mov hr, E_NOTIMPL
        jmp done
    .endif
    
    ; Increment call count
    inc (TOOL_ENTRY PTR [esi]).call_count
    
    ; Call the tool function
    ; Convention: stdcall with (pInput, pOutput, cbOutput) -> HRESULT
    push cbOutput
    push pOutput
    push pInput
    call (TOOL_ENTRY PTR [esi]).func_ptr
    mov hr, eax
    
done:
    call _leave_crit
    PERF_DELTA offset start, offset dur, offset dur
    
    LOG offset szToolDispatch, hr, dword ptr [dur]
    
    mov eax, hr
    ret
AgentTool_Dispatch ENDP

; ============================================================================
;  AgentTool_ResultToMemory – Log tool output to agent memory
; ============================================================================
AgentTool_ResultToMemory PROC uses ebx esi, toolId:DWORD, pResult:DWORD
    LOCAL start:QWORD, dur:QWORD, hr:SDWORD
    LOCAL keyBuf[128]:BYTE
    
    PERF_INIT offset start
    
    .if pResult == 0
        mov hr, E_INVALIDARG
        jmp done
    .endif
    
    ; Build key: "tool_result_<toolId>"
    invoke wsprintf, addr keyBuf, CSTR("tool_result_%d"), toolId
    
    ; Store in agent memory
    extern AgentMemory_Store:PROC
    invoke AgentMemory_Store, addr keyBuf, pResult, 0
    mov hr, eax
    
done:
    PERF_DELTA offset start, offset dur, offset dur
    LOG offset szToolResult, hr, dword ptr [dur]
    
    mov eax, hr
    ret
AgentTool_ResultToMemory ENDP

; ============================================================================
;  Tool stub implementations (44 total)
; ============================================================================

; Tool 0: read_file
tool_read_file PROC uses ebx esi, pInput:DWORD, pOutput:DWORD, cbOutput:DWORD
    ; TODO: Real file reading
    mov eax, 0
    ret
tool_read_file ENDP

; Tool 1: write_file
tool_write_file PROC uses ebx esi, pInput:DWORD, pOutput:DWORD, cbOutput:DWORD
    ; TODO: Real file writing
    mov eax, 0
    ret
tool_write_file ENDP

; ... (42 more tool implementations)

END
