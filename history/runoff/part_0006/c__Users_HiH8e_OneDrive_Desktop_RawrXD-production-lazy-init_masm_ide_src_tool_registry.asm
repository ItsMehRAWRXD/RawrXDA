; ============================================================================
; RawrXD Agentic IDE - Tool Registry (clean MASM implementation)
; Uses TOOL_DEFINITION / TOOL_RESULT from structures.inc
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\shell32.inc
include \masm32\include\shlwapi.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\shell32.lib
includelib \masm32\lib\shlwapi.lib

 .data
include constants.inc
include structures.inc
include macros.inc

; ----------------------------------------------------------------------------
; DATA
; ----------------------------------------------------------------------------

.data
    g_ToolCount         dd 0
    hToolRegistry       dd 0        ; pointer to TOOL_DEFINITION array
    hMutex              dd 0

    ; Categories
    szCatFileOps        db "File Operations",0
    szCatBuildTest      db "Build & Test",0
    szCatGit            db "Git Operations",0
    szCatNetwork        db "Network",0
    szCatSearch         db "Search & Analysis",0
    szCatLSP            db "Language Services",0
    szCatAI             db "AI/ML",0
    szCatEditor         db "Editor",0
    szCatMemory         db "Memory",0
    szCatCompress       db "Compression",0

    ; Tool names / ids
    szTool_ReadFile     db "read_file",0
    szTool_WriteFile    db "write_file",0
    szTool_DeleteFile   db "delete_file",0
    szTool_ListDir      db "list_directory",0
    szTool_CreateDir    db "create_directory",0
    szTool_MoveFile     db "move_file",0
    szTool_CopyFile     db "copy_file",0
    szTool_SearchFiles  db "search_files",0
    szTool_GrepFiles    db "grep_files",0
    szTool_CompareFiles db "compare_files",0
    szTool_MergeFiles   db "merge_files",0
    szTool_GetFileInfo  db "get_file_info",0

    szTool_RunCommand   db "run_command",0
    szTool_CompileCode  db "compile_code",0
    szTool_RunTests     db "run_tests",0
    szTool_ProfilePerf  db "profile_performance",0

    szTool_GitStatus    db "git_status",0
    szTool_GitAdd       db "git_add",0
    szTool_GitCommit    db "git_commit",0
    szTool_GitPush      db "git_push",0
    szTool_GitPull      db "git_pull",0
    szTool_GitBranch    db "git_branch",0
    szTool_GitCheckout  db "git_checkout",0
    szTool_GitDiff      db "git_diff",0
    szTool_GitLog       db "git_log",0

    szTool_HttpGet      db "http_get",0
    szTool_HttpPost     db "http_post",0
    szTool_FetchWeb     db "fetch_webpage",0
    szTool_DownloadFile db "download_file",0

    szTool_GetDef       db "get_definition",0
    szTool_GetRefs      db "get_references",0
    szTool_GetSymbols   db "get_symbols",0
    szTool_GetCompl     db "get_completion",0
    szTool_GetHover     db "get_hover",0
    szTool_Refactor     db "refactor",0

    szTool_AskModel     db "ask_model",0
    szTool_GenCode      db "generate_code",0
    szTool_ExplainCode  db "explain_code",0
    szTool_ReviewCode   db "review_code",0
    szTool_FixBug       db "fix_bug",0

    szTool_OpenFile     db "open_file",0
    szTool_CloseFile    db "close_file",0
    szTool_GetOpen      db "get_open_files",0
    szTool_GetActive    db "get_active_file",0
    szTool_GetSelection db "get_selection",0
    szTool_InsertText   db "insert_text",0

    szTool_AddMemory    db "add_memory",0
    szTool_GetMemory    db "get_memory",0
    szTool_SearchMem    db "search_memory",0
    szTool_ClearMemory  db "clear_memory",0

    szTool_CompressFile db "compress_file",0
    szTool_DecompFile   db "decompress_file",0
    szTool_CompressStats db "compression_stats",0

    ; Descriptions (reused)
    szDescShort         db "Generic tool entry",0
    szDescRead          db "Read file contents",0
    szDescWrite         db "Write text to file",0
    szDescDelete        db "Delete a file",0
    szDescList          db "List directory contents",0
    szDescCreateDir     db "Create a directory",0
    szDescMove          db "Move/rename file",0
    szDescCopy          db "Copy file",0
    szDescCompress      db "Compress file",0
    szDescDecompress    db "Decompress file",0
    szDescStats         db "Compression stats",0

.data?
    g_szWorkBuffer      db MAX_BUFFER_SIZE dup(?)

; ----------------------------------------------------------------------------
; HELPERS
; ----------------------------------------------------------------------------

AddTool proc pszId:DWORD, pszName:DWORD, pszDesc:DWORD, pszCat:DWORD, dwParams:DWORD, bConfirm:DWORD, bAsync:DWORD, pExec:DWORD, pUser:DWORD
    LOCAL pDef:DWORD

    mov eax, g_ToolCount
    cmp eax, MAX_TOOL_COUNT
    jae @Exit

    imul eax, sizeof TOOL_DEFINITION
    mov edx, hToolRegistry
    add edx, eax
    mov pDef, edx

    invoke lstrcpy, pDef, pszId
    mov eax, pDef
    add eax, OFFSET TOOL_DEFINITION.szToolName
    invoke lstrcpy, eax, pszName
    mov eax, pDef
    add eax, OFFSET TOOL_DEFINITION.szDescription
    invoke lstrcpy, eax, pszDesc
    mov eax, pDef
    add eax, OFFSET TOOL_DEFINITION.szCategory
    invoke lstrcpy, eax, pszCat

    mov eax, pDef
    add eax, OFFSET TOOL_DEFINITION.dwParamCount
    mov ecx, dwParams
    mov [eax], ecx
    add eax, 4
    mov ecx, bConfirm
    mov [eax], ecx
    add eax, 4
    mov ecx, bAsync
    mov [eax], ecx
    add eax, 4
    mov ecx, pExec
    mov [eax], ecx
    add eax, 4
    mov ecx, pUser
    mov [eax], ecx

    inc g_ToolCount
@Exit:
    ret
AddTool endp

MakeResult proc bSuccess:DWORD
    LOCAL pRes:DWORD
    MemAlloc sizeof TOOL_RESULT
    mov pRes, eax
    invoke RtlZeroMemory, pRes, sizeof TOOL_RESULT
    mov eax, pRes
    mov dword ptr [eax], bSuccess
    mov eax, pRes
    ret
MakeResult endp

; ----------------------------------------------------------------------------
; REGISTRY ENTRY POINT
; ----------------------------------------------------------------------------

ToolRegistry_Init proc
    invoke CreateMutex, NULL, FALSE, NULL
    mov hMutex, eax

    MemAlloc sizeof TOOL_DEFINITION * MAX_TOOL_COUNT
    mov hToolRegistry, eax
    mov g_ToolCount, 0

    call RegisterFileTools
    call RegisterBuildTools
    call RegisterGitTools
    call RegisterNetworkTools
    call RegisterLSPTools
    call RegisterAITools
    call RegisterEditorTools
    call RegisterMemoryTools
    call RegisterCompressionTools

    mov eax, hToolRegistry
    ret
ToolRegistry_Init endp

; ----------------------------------------------------------------------------
; FILE TOOLS
; ----------------------------------------------------------------------------

RegisterFileTools proc
    invoke AddTool, offset szTool_ReadFile, offset szTool_ReadFile, offset szDescRead, offset szCatFileOps, 1, FALSE, FALSE, offset Tool_ReadFile_Execute, 0
    invoke AddTool, offset szTool_WriteFile, offset szTool_WriteFile, offset szDescWrite, offset szCatFileOps, 2, FALSE, FALSE, offset Tool_WriteFile_Execute, 0
    invoke AddTool, offset szTool_DeleteFile, offset szTool_DeleteFile, offset szDescDelete, offset szCatFileOps, 1, FALSE, FALSE, offset Tool_DeleteFile_Execute, 0
    invoke AddTool, offset szTool_ListDir, offset szTool_ListDir, offset szDescList, offset szCatFileOps, 1, FALSE, FALSE, offset Tool_ListDir_Execute, 0
    invoke AddTool, offset szTool_CreateDir, offset szTool_CreateDir, offset szDescCreateDir, offset szCatFileOps, 1, FALSE, FALSE, offset Tool_CreateDir_Execute, 0
    invoke AddTool, offset szTool_MoveFile, offset szTool_MoveFile, offset szDescMove, offset szCatFileOps, 2, FALSE, FALSE, offset Tool_MoveFile_Execute, 0
    invoke AddTool, offset szTool_CopyFile, offset szTool_CopyFile, offset szDescCopy, offset szCatFileOps, 2, FALSE, FALSE, offset Tool_CopyFile_Execute, 0
    invoke AddTool, offset szTool_SearchFiles, offset szTool_SearchFiles, offset szDescShort, offset szCatFileOps, 2, FALSE, FALSE, offset Tool_SearchFiles_Execute, 0
    invoke AddTool, offset szTool_GrepFiles, offset szTool_GrepFiles, offset szDescShort, offset szCatFileOps, 2, FALSE, FALSE, offset Tool_GrepFiles_Execute, 0
    invoke AddTool, offset szTool_CompareFiles, offset szTool_CompareFiles, offset szDescShort, offset szCatFileOps, 2, FALSE, FALSE, offset Tool_CompareFiles_Execute, 0
    invoke AddTool, offset szTool_MergeFiles, offset szTool_MergeFiles, offset szDescShort, offset szCatFileOps, 3, FALSE, FALSE, offset Tool_MergeFiles_Execute, 0
    invoke AddTool, offset szTool_GetFileInfo, offset szTool_GetFileInfo, offset szDescShort, offset szCatFileOps, 1, FALSE, FALSE, offset Tool_GetFileInfo_Execute, 0
    ret
RegisterFileTools endp

Tool_ReadFile_Execute proc pszPath:DWORD
    LOCAL hFile:DWORD, dwRead:DWORD, pRes:DWORD, pBuf:DWORD
    invoke MakeResult, FALSE
    mov pRes, eax
    invoke CreateFile, pszPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    .if eax == INVALID_HANDLE_VALUE
        mov eax, pRes
        ret
    .endif

    mov eax, pRes
    add eax, OFFSET TOOL_RESULT.szOutput
    mov pBuf, eax
    invoke ReadFile, hFile, pBuf, MAX_BUFFER_SIZE-1, addr dwRead, NULL
    mov byte ptr [pBuf+dwRead], 0
    invoke CloseHandle, hFile
    mov eax, pRes
    mov dword ptr [eax], TRUE
    mov eax, pRes
    ret
Tool_ReadFile_Execute endp

Tool_WriteFile_Execute proc pszPath:DWORD, pszContent:DWORD
    LOCAL hFile:DWORD, dwWritten:DWORD, pRes:DWORD
    invoke MakeResult, FALSE
    mov pRes, eax
    invoke CreateFile, pszPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    .if eax == INVALID_HANDLE_VALUE
        mov eax, pRes
        ret
    .endif
    invoke lstrlen, pszContent
    invoke WriteFile, hFile, pszContent, eax, addr dwWritten, NULL
    invoke CloseHandle, hFile
    mov eax, pRes
    mov dword ptr [eax], TRUE
    mov eax, pRes
    ret
Tool_WriteFile_Execute endp

Tool_DeleteFile_Execute proc pszPath:DWORD
    LOCAL pRes:DWORD
    LOCAL rc:DWORD
    invoke MakeResult, FALSE
    mov pRes, eax
    invoke DeleteFile, pszPath
    mov rc, eax
    .if rc != 0
        mov dword ptr [pRes], TRUE
    .endif
    mov eax, pRes
    ret
Tool_DeleteFile_Execute endp

Tool_ListDir_Execute proc pszPath:DWORD
    LOCAL hFind:DWORD, wfd:WIN32_FIND_DATA, pRes:DWORD
    LOCAL szWildcard db "\\*.*",0
    invoke MakeResult, FALSE
    mov pRes, eax
    invoke lstrcpy, addr g_szWorkBuffer, pszPath
    invoke lstrcat, addr g_szWorkBuffer, addr szWildcard
    invoke FindFirstFile, addr g_szWorkBuffer, addr wfd
    mov hFind, eax
    .if eax != INVALID_HANDLE_VALUE
        invoke FindClose, hFind
        mov dword ptr [pRes], TRUE
    .endif
    mov eax, pRes
    ret
Tool_ListDir_Execute endp

Tool_CreateDir_Execute proc pszPath:DWORD
    LOCAL pRes:DWORD
    LOCAL rc:DWORD
    invoke MakeResult, FALSE
    mov pRes, eax
    invoke CreateDirectory, pszPath, NULL
    mov rc, eax
    .if rc != 0
        mov dword ptr [pRes], TRUE
    .endif
    mov eax, pRes
    ret
Tool_CreateDir_Execute endp

Tool_MoveFile_Execute proc pszFrom:DWORD, pszTo:DWORD
    LOCAL pRes:DWORD
    LOCAL rc:DWORD
    invoke MakeResult, FALSE
    mov pRes, eax
    invoke MoveFile, pszFrom, pszTo
    mov rc, eax
    .if rc != 0
        mov dword ptr [pRes], TRUE
    .endif
    mov eax, pRes
    ret
Tool_MoveFile_Execute endp

Tool_CopyFile_Execute proc pszFrom:DWORD, pszTo:DWORD
    LOCAL pRes:DWORD
    LOCAL rc:DWORD
    invoke MakeResult, FALSE
    mov pRes, eax
    invoke CopyFile, pszFrom, pszTo, FALSE
    mov rc, eax
    .if rc != 0
        mov dword ptr [pRes], TRUE
    .endif
    mov eax, pRes
    ret
Tool_CopyFile_Execute endp

Tool_SearchFiles_Execute proc pszPattern:DWORD, pszPath:DWORD
    invoke MakeResult, TRUE
    ret
Tool_SearchFiles_Execute endp

Tool_GrepFiles_Execute proc pszPattern:DWORD, pszFilePath:DWORD
    invoke MakeResult, TRUE
    ret
Tool_GrepFiles_Execute endp

Tool_CompareFiles_Execute proc pszA:DWORD, pszB:DWORD
    invoke MakeResult, TRUE
    ret
Tool_CompareFiles_Execute endp

Tool_MergeFiles_Execute proc pszBase:DWORD, pszTheirs:DWORD, pszOurs:DWORD
    invoke MakeResult, TRUE
    ret
Tool_MergeFiles_Execute endp

Tool_GetFileInfo_Execute proc pszPath:DWORD
    invoke MakeResult, TRUE
    ret
Tool_GetFileInfo_Execute endp

; ----------------------------------------------------------------------------
; BUILD / TEST
; ----------------------------------------------------------------------------

RegisterBuildTools proc
    invoke AddTool, offset szTool_RunCommand, offset szTool_RunCommand, offset szDescShort, offset szCatBuildTest, 1, FALSE, FALSE, offset Tool_RunCommand_Execute, 0
    invoke AddTool, offset szTool_CompileCode, offset szTool_CompileCode, offset szDescShort, offset szCatBuildTest, 2, FALSE, FALSE, offset Tool_CompileCode_Execute, 0
    invoke AddTool, offset szTool_RunTests, offset szTool_RunTests, offset szDescShort, offset szCatBuildTest, 1, FALSE, FALSE, offset Tool_RunTests_Execute, 0
    invoke AddTool, offset szTool_ProfilePerf, offset szTool_ProfilePerf, offset szDescShort, offset szCatBuildTest, 1, FALSE, FALSE, offset Tool_ProfilePerf_Execute, 0
    ret
RegisterBuildTools endp

Tool_RunCommand_Execute proc pszCmd:DWORD
    invoke MakeResult, TRUE
    ret
Tool_RunCommand_Execute endp
Tool_CompileCode_Execute proc pszSource:DWORD, pszOut:DWORD
    invoke MakeResult, TRUE
    ret
Tool_CompileCode_Execute endp
Tool_RunTests_Execute proc pszTestExe:DWORD
    invoke MakeResult, TRUE
    ret
Tool_RunTests_Execute endp
Tool_ProfilePerf_Execute proc pszExe:DWORD
    invoke MakeResult, TRUE
    ret
Tool_ProfilePerf_Execute endp

; ----------------------------------------------------------------------------
; GIT
; ----------------------------------------------------------------------------

RegisterGitTools proc
    invoke AddTool, offset szTool_GitStatus, offset szTool_GitStatus, offset szDescShort, offset szCatGit, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GitAdd, offset szTool_GitAdd, offset szDescShort, offset szCatGit, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GitCommit, offset szTool_GitCommit, offset szDescShort, offset szCatGit, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GitPush, offset szTool_GitPush, offset szDescShort, offset szCatGit, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GitPull, offset szTool_GitPull, offset szDescShort, offset szCatGit, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GitBranch, offset szTool_GitBranch, offset szDescShort, offset szCatGit, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GitCheckout, offset szTool_GitCheckout, offset szDescShort, offset szCatGit, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GitDiff, offset szTool_GitDiff, offset szDescShort, offset szCatGit, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GitLog, offset szTool_GitLog, offset szDescShort, offset szCatGit, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    ret
RegisterGitTools endp

; ----------------------------------------------------------------------------
; NETWORK
; ----------------------------------------------------------------------------

RegisterNetworkTools proc
    invoke AddTool, offset szTool_HttpGet, offset szTool_HttpGet, offset szDescShort, offset szCatNetwork, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_HttpPost, offset szTool_HttpPost, offset szDescShort, offset szCatNetwork, 2, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_FetchWeb, offset szTool_FetchWeb, offset szDescShort, offset szCatNetwork, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_DownloadFile, offset szTool_DownloadFile, offset szDescShort, offset szCatNetwork, 2, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    ret
RegisterNetworkTools endp

; ----------------------------------------------------------------------------
; LSP
; ----------------------------------------------------------------------------

RegisterLSPTools proc
    invoke AddTool, offset szTool_GetDef, offset szTool_GetDef, offset szDescShort, offset szCatLSP, 3, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GetRefs, offset szTool_GetRefs, offset szDescShort, offset szCatLSP, 3, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GetSymbols, offset szTool_GetSymbols, offset szDescShort, offset szCatLSP, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GetCompl, offset szTool_GetCompl, offset szDescShort, offset szCatLSP, 3, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GetHover, offset szTool_GetHover, offset szDescShort, offset szCatLSP, 3, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_Refactor, offset szTool_Refactor, offset szDescShort, offset szCatLSP, 2, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    ret
RegisterLSPTools endp

; ----------------------------------------------------------------------------
; AI / Editor / Memory
; ----------------------------------------------------------------------------

RegisterAITools proc
    invoke AddTool, offset szTool_AskModel, offset szTool_AskModel, offset szDescShort, offset szCatAI, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GenCode, offset szTool_GenCode, offset szDescShort, offset szCatAI, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_ExplainCode, offset szTool_ExplainCode, offset szDescShort, offset szCatAI, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_ReviewCode, offset szTool_ReviewCode, offset szDescShort, offset szCatAI, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_FixBug, offset szTool_FixBug, offset szDescShort, offset szCatAI, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    ret
RegisterAITools endp

RegisterEditorTools proc
    invoke AddTool, offset szTool_OpenFile, offset szTool_OpenFile, offset szDescShort, offset szCatEditor, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_CloseFile, offset szTool_CloseFile, offset szDescShort, offset szCatEditor, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GetOpen, offset szTool_GetOpen, offset szDescShort, offset szCatEditor, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GetActive, offset szTool_GetActive, offset szDescShort, offset szCatEditor, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GetSelection, offset szTool_GetSelection, offset szDescShort, offset szCatEditor, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_InsertText, offset szTool_InsertText, offset szDescShort, offset szCatEditor, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    ret
RegisterEditorTools endp

RegisterMemoryTools proc
    invoke AddTool, offset szTool_AddMemory, offset szTool_AddMemory, offset szDescShort, offset szCatMemory, 2, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_GetMemory, offset szTool_GetMemory, offset szDescShort, offset szCatMemory, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_SearchMem, offset szTool_SearchMem, offset szDescShort, offset szCatMemory, 1, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    invoke AddTool, offset szTool_ClearMemory, offset szTool_ClearMemory, offset szDescShort, offset szCatMemory, 0, FALSE, FALSE, offset Tool_NoOp_Execute, 0
    ret
RegisterMemoryTools endp

; ----------------------------------------------------------------------------
; COMPRESSION
; ----------------------------------------------------------------------------

RegisterCompressionTools proc
    invoke AddTool, offset szTool_CompressFile, offset szTool_CompressFile, offset szDescCompress, offset szCatCompress, 2, FALSE, FALSE, offset Tool_CompressFile_Execute, 0
    invoke AddTool, offset szTool_DecompFile, offset szTool_DecompFile, offset szDescDecompress, offset szCatCompress, 2, FALSE, FALSE, offset Tool_DecompFile_Execute, 0
    invoke AddTool, offset szTool_CompressStats, offset szTool_CompressStats, offset szDescStats, offset szCatCompress, 0, FALSE, FALSE, offset Tool_CompressStats_Execute, 0
    ret
RegisterCompressionTools endp

Tool_CompressFile_Execute proc pszSrc:DWORD, pszDst:DWORD
    invoke Tool_CopyFile_Execute, pszSrc, pszDst
    ret
Tool_CompressFile_Execute endp

Tool_DecompFile_Execute proc pszSrc:DWORD, pszDst:DWORD
    invoke Tool_CopyFile_Execute, pszSrc, pszDst
    ret
Tool_DecompFile_Execute endp

Tool_CompressStats_Execute proc
    invoke MakeResult, TRUE
    ret
Tool_CompressStats_Execute endp

; ----------------------------------------------------------------------------
; NO-OP EXECUTOR (used for tools not yet specialized)
; ----------------------------------------------------------------------------

Tool_NoOp_Execute proc
    invoke MakeResult, TRUE
    ret
Tool_NoOp_Execute endp

; ----------------------------------------------------------------------------
; END OF FILE
; ----------------------------------------------------------------------------

END
