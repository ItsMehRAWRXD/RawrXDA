; ============================================================================
; tool_dispatcher_simple.asm - Tool Execution Dispatcher
; Routes tool calls to implementations (10 critical tools)
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

PUBLIC ToolDispatcher_Execute
PUBLIC ToolDispatcher_Register
PUBLIC ToolDispatcher_GetToolCount

; Tool IDs (10 critical tools)
TOOL_ID_READ_FILE           EQU 1
TOOL_ID_WRITE_FILE          EQU 2
TOOL_ID_LIST_DIRECTORY      EQU 3
TOOL_ID_EXECUTE_COMMAND     EQU 4
TOOL_ID_QUERY_REGISTRY      EQU 5
TOOL_ID_SET_ENV_VAR         EQU 6
TOOL_ID_GET_FILE_SIZE       EQU 7
TOOL_ID_DELETE_FILE         EQU 8
TOOL_ID_COPY_FILE           EQU 9
TOOL_ID_CREATE_DIRECTORY    EQU 10

MAX_TOOLS                   EQU 44

; Structures
ToolDefinition STRUCT
    dwToolId            DWORD ?
    szName[64]          BYTE ?
    szDescription[256]  BYTE ?
    pfnHandler          DWORD ?
    bEnabled            DWORD ?
ToolDefinition ENDS

ToolCall STRUCT
    dwToolId            DWORD ?
    pzArguments[16]     DWORD ?
    dwArgCount          DWORD ?
    pzResult            DWORD ?
    cbResult            DWORD ?
    dwStatus            DWORD ?
ToolCall ENDS

; ============================================================================
; Data Section
; ============================================================================

.data

    ; Tool registry
    g_ToolRegistry ToolDefinition MAX_TOOLS DUP(<>)
    g_dwToolCount DWORD 0

    ; Tool names and descriptions
    szTool1Name         db "ReadFile", 0
    szTool1Desc         db "Read contents of a text file", 0
    
    szTool2Name         db "WriteFile", 0
    szTool2Desc         db "Write text to a file (creates or overwrites)", 0
    
    szTool3Name         db "ListDirectory", 0
    szTool3Desc         db "List files and folders in a directory", 0
    
    szTool4Name         db "ExecuteCommand", 0
    szTool4Desc         db "Execute a system command (cmd.exe /c)", 0
    
    szTool5Name         db "QueryRegistry", 0
    szTool5Desc         db "Query Windows registry values", 0
    
    szTool6Name         db "SetEnvironmentVariable", 0
    szTool6Desc         db "Set environment variable for current session", 0
    
    szTool7Name         db "GetFileSize", 0
    szTool7Desc         db "Get size of a file in bytes", 0
    
    szTool8Name         db "DeleteFile", 0
    szTool8Desc         db "Delete a file", 0
    
    szTool9Name         db "CopyFile", 0
    szTool9Desc         db "Copy file from source to destination", 0
    
    szTool10Name        db "CreateDirectory", 0
    szTool10Desc        db "Create a new directory/folder", 0

    ; Error messages
    szInvalidTool       db "ERROR: Invalid tool ID", 0
    szToolDisabled      db "ERROR: Tool is disabled", 0
    szMissingArgs       db "ERROR: Missing required arguments", 0

.data?

    g_ResultBuffer      BYTE 4096 DUP(?)

; ============================================================================
; Code Section
; ============================================================================

.code

; ============================================================================
; ToolDispatcher_Init - Register all 10 tools
; ============================================================================

ToolDispatcher_Init PROC

    LOCAL i:DWORD

    ; Tool 1: ReadFile
    mov eax, TOOL_ID_READ_FILE
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    lea eax, szTool1Name
    mov ecx, 64
    mov esi, eax
    lea edi, [edx].ToolDefinition.szName
    rep movsb
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_ReadFile

    ; Tool 2: WriteFile
    mov eax, TOOL_ID_WRITE_FILE
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    lea eax, szTool2Name
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_WriteFile

    ; Tool 3: ListDirectory
    mov eax, TOOL_ID_LIST_DIRECTORY
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_ListDirectory

    ; Tool 4: ExecuteCommand
    mov eax, TOOL_ID_EXECUTE_COMMAND
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_ExecuteCommand

    ; Tool 5: QueryRegistry
    mov eax, TOOL_ID_QUERY_REGISTRY
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_QueryRegistry

    ; Tool 6: SetEnvironmentVariable
    mov eax, TOOL_ID_SET_ENV_VAR
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_SetEnvVar

    ; Tool 7: GetFileSize
    mov eax, TOOL_ID_GET_FILE_SIZE
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_GetFileSize

    ; Tool 8: DeleteFile
    mov eax, TOOL_ID_DELETE_FILE
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_DeleteFile

    ; Tool 9: CopyFile
    mov eax, TOOL_ID_COPY_FILE
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_CopyFile

    ; Tool 10: CreateDirectory
    mov eax, TOOL_ID_CREATE_DIRECTORY
    mov ecx, offset g_ToolRegistry
    lea edx, [ecx + eax*sizeof ToolDefinition]
    mov [edx].ToolDefinition.dwToolId, eax
    mov [edx].ToolDefinition.bEnabled, 1
    mov [edx].ToolDefinition.pfnHandler, offset Tool_CreateDirectory

    mov [g_dwToolCount], 10
    mov eax, 1
    ret

ToolDispatcher_Init ENDP

; ============================================================================
; ToolDispatcher_Execute - Execute a tool by ID
; Input: dwToolId, ppszArgs (pointer to arg array), dwArgCount
; Output: eax = result pointer, ecx = result size
; ============================================================================

ToolDispatcher_Execute PROC USES esi edi ebx dwToolId:DWORD, ppszArgs:DWORD, dwArgCount:DWORD

    LOCAL pTool:DWORD
    LOCAL pfnHandler:DWORD

    ; Find tool in registry
    mov esi, offset g_ToolRegistry
    xor ecx, ecx

@search_loop:
    cmp ecx, [g_dwToolCount]
    jge @not_found

    mov eax, [esi].ToolDefinition.dwToolId
    cmp eax, dwToolId
    je @found_tool

    add esi, sizeof ToolDefinition
    inc ecx
    jmp @search_loop

@found_tool:
    ; Verify tool is enabled
    cmp [esi].ToolDefinition.bEnabled, 0
    je @disabled

    ; Get handler function
    mov eax, [esi].ToolDefinition.pfnHandler
    mov pfnHandler, eax
    test eax, eax
    jz @not_found

    ; Call handler with arguments
    push dwArgCount
    push ppszArgs
    call pfnHandler

    ; Return result
    lea eax, g_ResultBuffer
    mov ecx, 4096
    ret

@disabled:
    lea eax, szToolDisabled
    mov ecx, 0
    ret

@not_found:
    lea eax, szInvalidTool
    mov ecx, 0
    ret

ToolDispatcher_Execute ENDP

; ============================================================================
; Tool Implementation: ReadFile
; Args: [0] = filename
; ============================================================================

Tool_ReadFile PROC USES esi edi ebx ppszArgs:DWORD, dwArgCount:DWORD

    LOCAL hFile:DWORD
    LOCAL dwRead:DWORD

    cmp dwArgCount, 0
    je @fail

    mov eax, ppszArgs
    mov esi, [eax]              ; filename

    invoke CreateFileA, esi, GENERIC_READ, FILE_SHARE_READ, NULL, \
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @fail

    mov hFile, eax

    invoke ReadFile, hFile, addr g_ResultBuffer, 4096, addr dwRead, NULL
    test eax, eax
    jz @close_fail

    invoke CloseHandle, hFile

    lea eax, g_ResultBuffer
    mov ecx, dwRead
    ret

@close_fail:
    invoke CloseHandle, hFile
@fail:
    xor eax, eax
    xor ecx, ecx
    ret

Tool_ReadFile ENDP

; ============================================================================
; Tool Implementation: WriteFile
; Args: [0] = filename, [1] = content
; ============================================================================

Tool_WriteFile PROC USES esi edi ebx ppszArgs:DWORD, dwArgCount:DWORD

    LOCAL hFile:DWORD
    LOCAL dwWritten:DWORD
    LOCAL cbContent:DWORD

    cmp dwArgCount, 1
    jl @fail

    mov eax, ppszArgs
    mov esi, [eax]              ; filename
    mov edi, [eax + 4]          ; content

    ; Get content length
    mov ecx, edi
@strlen_loop:
    cmp byte ptr [ecx], 0
    je @strlen_done
    inc ecx
    jmp @strlen_loop

@strlen_done:
    sub ecx, edi
    mov cbContent, ecx

    invoke CreateFileA, esi, GENERIC_WRITE, 0, NULL, \
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @fail

    mov hFile, eax

    invoke WriteFile, hFile, edi, cbContent, addr dwWritten, NULL
    test eax, eax
    jz @close_fail

    invoke CloseHandle, hFile

    lea eax, szTool2Name
    mov ecx, dwWritten
    ret

@close_fail:
    invoke CloseHandle, hFile
@fail:
    xor eax, eax
    xor ecx, ecx
    ret

Tool_WriteFile ENDP

; ============================================================================
; Tool Implementation: ListDirectory
; Args: [0] = path
; ============================================================================

Tool_ListDirectory PROC USES esi edi ebx ppszArgs:DWORD, dwArgCount:DWORD

    LOCAL findData:WIN32_FIND_DATAA
    LOCAL hFind:DWORD
    LOCAL pBuf:DWORD

    cmp dwArgCount, 0
    je @fail

    mov eax, ppszArgs
    mov esi, [eax]              ; path

    invoke FindFirstFileA, esi, addr findData
    cmp eax, INVALID_HANDLE_VALUE
    je @fail

    mov hFind, eax
    lea edi, g_ResultBuffer
    xor ecx, ecx

@list_loop:
    ; Copy filename to result buffer
    lea esi, findData.cFileName
    mov ecx, 256

@copy_name:
    mov al, [esi]
    mov [edi], al
    test al, al
    jz @copy_done
    inc esi
    inc edi
    loop @copy_name

@copy_done:
    mov byte ptr [edi], 10      ; newline
    inc edi

    invoke FindNextFileA, hFind, addr findData
    test eax, eax
    jnz @list_loop

    invoke FindClose, hFind

    lea eax, g_ResultBuffer
    mov ecx, edi
    sub ecx, eax
    ret

@fail:
    xor eax, eax
    xor ecx, ecx
    ret

Tool_ListDirectory ENDP

; ============================================================================
; Stub implementations for remaining tools
; ============================================================================

Tool_ExecuteCommand PROC ppszArgs:DWORD, dwArgCount:DWORD
    xor eax, eax
    ret
Tool_ExecuteCommand ENDP

Tool_QueryRegistry PROC ppszArgs:DWORD, dwArgCount:DWORD
    xor eax, eax
    ret
Tool_QueryRegistry ENDP

Tool_SetEnvVar PROC ppszArgs:DWORD, dwArgCount:DWORD
    xor eax, eax
    ret
Tool_SetEnvVar ENDP

Tool_GetFileSize PROC ppszArgs:DWORD, dwArgCount:DWORD
    xor eax, eax
    ret
Tool_GetFileSize ENDP

Tool_DeleteFile PROC ppszArgs:DWORD, dwArgCount:DWORD
    xor eax, eax
    ret
Tool_DeleteFile ENDP

Tool_CopyFile PROC ppszArgs:DWORD, dwArgCount:DWORD
    xor eax, eax
    ret
Tool_CopyFile ENDP

Tool_CreateDirectory PROC ppszArgs:DWORD, dwArgCount:DWORD
    xor eax, eax
    ret
Tool_CreateDirectory ENDP

; ============================================================================
; ToolDispatcher_Register - Register additional tool
; ============================================================================

ToolDispatcher_Register PROC dwToolId:DWORD, pszName:DWORD, pszDesc:DWORD, pfnHandler:DWORD

    cmp [g_dwToolCount], MAX_TOOLS
    jge @fail

    mov esi, offset g_ToolRegistry
    mov eax, [g_dwToolCount]
    lea edx, [esi + eax*sizeof ToolDefinition]

    mov [edx].ToolDefinition.dwToolId, dwToolId
    mov [edx].ToolDefinition.pfnHandler, pfnHandler
    mov [edx].ToolDefinition.bEnabled, 1

    inc [g_dwToolCount]
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

ToolDispatcher_Register ENDP

; ============================================================================
; ToolDispatcher_GetToolCount - Get number of registered tools
; Output: eax = tool count
; ============================================================================

ToolDispatcher_GetToolCount PROC

    mov eax, [g_dwToolCount]
    ret

ToolDispatcher_GetToolCount ENDP

END
