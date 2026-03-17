;==============================================================================
; plugin_loader.asm - Plugin Hot-Loader & Registration
; Scans Plugins\ folder, loads DLLs, validates ABI, registers tools
; ~500 lines
;==============================================================================

option casemap:none

include windows.inc
include plugin_abi.inc
includelib kernel32.lib

PUBLIC PluginLoaderInit, PluginLoaderExecuteTool, PluginLoaderListTools

;==============================================================================
; CONSTANTS
;==============================================================================
MAX_PLUGINS         equ 32
MAX_TOOLS_TOTAL     equ 256
PLUGIN_MAGIC        equ 52584450h ; 'RXDP'

;==============================================================================
; DATA
;==============================================================================
.data
    g_pluginCount       DWORD   0
    g_toolCount         DWORD   0
    g_pluginDir         db  "Plugins\*.dll", 0
    g_pluginPath        db  "Plugins\", 0
    szPluginMetaData    db  "PluginMetaData", 0
    szToolNotFound      db  "Tool not found", 0

.data?
    g_plugins           QWORD   MAX_PLUGINS dup(?)        ; HMODULE array
    g_pluginMetas       QWORD   MAX_PLUGINS dup(?)        ; PLUGIN_META* array
    g_tools             AGENT_TOOL MAX_TOOLS_TOTAL dup(<>) ; Tool registry
    g_toolNames         QWORD   MAX_TOOLS_TOTAL dup(?)    ; Tool name strings

;==============================================================================
; CODE
;==============================================================================
.code

;==============================================================================
; PluginLoaderInit - Load all DLLs from Plugins\ folder
; Returns: EAX = number of tools registered
;==============================================================================
ALIGN 16
PluginLoaderInit PROC USES rbx r12 r13 r14 r15
    LOCAL   hFind:QWORD
    LOCAL   wfd:WIN32_FIND_DATA
    LOCAL   szFullPath[260]:BYTE
    LOCAL   hDll:QWORD
    LOCAL   pMeta:QWORD
    LOCAL   toolIdx:DWORD

    mov     g_pluginCount, 0
    mov     g_toolCount, 0

    ; Scan Plugins\ directory
    lea     rdx, wfd
    lea     rcx, g_pluginDir
    sub     rsp, 40
    call    FindFirstFileA
    add     rsp, 40
    mov     hFind, rax

    cmp     rax, INVALID_HANDLE_VALUE
    je      loader_done                 ; No plugins found is OK

    mov     r12, rax                    ; r12 = hFind

loader_loop:
    ; Build full path to DLL
    lea     rdi, szFullPath
    lea     rsi, g_pluginPath
    mov     ecx, 8                      ; "Plugins\" is 8 chars
    rep     movsb

    lea     rsi, wfd.cFileName
    ; edi is already at szFullPath + 8
    mov     ecx, 252                    ; Remaining space

copy_filename:
    lodsb
    stosb
    test    al, al
    jz      load_dll
    dec     ecx
    jnz     copy_filename

load_dll:
    ; LoadLibrary(szFullPath)
    lea     rcx, szFullPath
    sub     rsp, 40
    call    LoadLibraryA
    add     rsp, 40

    test    rax, rax
    jz      next_file
    mov     hDll, rax

    ; Get PluginMetaData export
    mov     rcx, rax
    lea     rdx, szPluginMetaData        ; exported symbol name
    sub     rsp, 40
    call    GetProcAddress
    add     rsp, 40

    test    rax, rax
    jz      next_file
    mov     pMeta, rax

    ; Validate magic
    mov     ecx, DWORD PTR [rax]        ; First DWORD of PLUGIN_META
    cmp     ecx, PLUGIN_MAGIC
    jne     next_file

    ; Validate version
    mov     ecx, DWORD PTR [rax+4]      ; Use DWORD for alignment
    cmp     ecx, 1
    jne     next_file

    ; Register tools from this plugin
    mov     r13, pMeta
    mov     r14d, (PLUGIN_META PTR [r13]).ToolCount
    mov     r15, (PLUGIN_META PTR [r13]).ToolsArrayPtr

    xor     edx, edx                    ; tool index in this plugin

register_tools:
    cmp     edx, r14d
    jge     next_file

    mov     ecx, g_toolCount
    cmp     ecx, MAX_TOOLS_TOTAL
    jge     next_file                   ; Full

    mov     eax, ecx
    imul    rax, rax, SIZEOF AGENT_TOOL
    lea     rdi, [g_tools + rax]        ; rdi = &g_tools[ecx]

    ; Copy tool from plugin
    mov     eax, edx
    imul    rax, rax, SIZEOF AGENT_TOOL
    lea     rsi, [r15 + rax]            ; rsi = &plugin_tools[edx]

    mov     ecx, SIZEOF AGENT_TOOL
    rep     movsb

    inc     g_toolCount
    inc     edx
    jmp     register_tools

next_file:
    ; FindNextFile
    mov     rcx, r12
    lea     rdx, wfd
    sub     rsp, 40
    call    FindNextFileA
    add     rsp, 40

    test    eax, eax
    jnz     loader_loop

    mov     rcx, r12
    sub     rsp, 40
    call    FindClose
    add     rsp, 40

loader_done:
    mov     eax, g_toolCount
    ret

PluginLoaderInit ENDP

;==============================================================================
; PluginLoaderExecuteTool - Execute a tool by name
; rcx = tool name, rdx = JSON params
; Returns: RAX = pointer to static result JSON
;==============================================================================
ALIGN 16
PluginLoaderExecuteTool PROC USES rbx r12 r13
    LOCAL   idx:DWORD

    mov     r12, rcx                    ; r12 = tool name
    mov     r13, rdx                    ; r13 = JSON params

    xor     ecx, ecx                    ; idx = 0

search_tool:
    cmp     ecx, g_toolCount
    jge     tool_not_found

    mov     eax, ecx
    imul    rax, rax, SIZEOF AGENT_TOOL
    lea     rsi, [g_tools + rax]

    mov     rbx, (AGENT_TOOL PTR [rsi]).NamePtr

    ; Compare strings
    mov     rdi, r12
    mov     rsi, rbx

compare_loop:
    mov     al, [rsi]
    mov     dl, [rdi]
    cmp     al, dl
    jne     next_search
    test    al, al
    jz      tool_found
    inc     rsi
    inc     rdi
    jmp     compare_loop

tool_found:
    ; Calculate tool pointer again
    mov     eax, ecx
    imul    rax, rax, SIZEOF AGENT_TOOL
    lea     rsi, [g_tools + rax]
    
    mov     rax, (AGENT_TOOL PTR [rsi]).HandlerFunc
    mov     rcx, r13
    sub     rsp, 40
    call    rax                         ; Call handler(pJson)
    add     rsp, 40
    ret

next_search:
    inc     ecx
    jmp     search_tool

tool_not_found:
    lea     rax, szToolNotFound
    ret

PluginLoaderExecuteTool ENDP

;==============================================================================
; PluginLoaderListTools - Get tool list for /tools command
; Returns: RAX = pointer to tool array
;==============================================================================
ALIGN 16
PluginLoaderListTools PROC
    lea     rax, [g_tools]
    ret
PluginLoaderListTools ENDP

END
