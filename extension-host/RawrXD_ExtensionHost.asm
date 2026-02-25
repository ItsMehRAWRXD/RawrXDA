; RawrXD_ExtensionHost.asm - Native Extension Loader (Replaces VS Code/Electron)
; Loads .rawr extensions (DLLs) with standardized API. Zero JavaScript, Pure x64.
; Build: ml64 RawrXD_ExtensionHost.asm /link kernel32.lib user32.lib /out:RawrXD_ExtensionHost.obj

option casemap:none
option win64:3
includelib kernel32.lib
includelib user32.lib

; Extension API VTable
EXT_API struct
    pfnInit            DQ ?
    pfnActivate        DQ ?
    pfnDeactivate      DQ ?
    pfnExecuteCommand  DQ ?
    pfnProvideCompletion DQ ?
    pfnHandleChat      DQ ?
    pfnDebugAttach     DQ ?
EXT_API ends

EXTENSION struct
    hModule    DQ ?
    api        EXT_API <>
    szName     DB 64 DUP(0)
    szPublisher DB 64 DUP(0)
    bActive    DB 0
EXTENSION ends

WIN32_FIND_DATAA struct
    dwFileAttributes     DD ?
    ftCreationTime        DQ ?
    ftLastAccessTime      DQ ?
    ftLastWriteTime       DQ ?
    nFileSizeHigh         DD ?
    nFileSizeLow          DD ?
    dwReserved0           DD ?
    dwReserved1           DD ?
    cFileName            DB 260 DUP(?)
    cAlternateFileName   DB 14 DUP(?)
WIN32_FIND_DATAA ends

EXTERN FindFirstFileA:PROC
EXTERN FindNextFileA:PROC
EXTERN FindClose:PROC
EXTERN LoadLibraryA:PROC
EXTERN GetProcAddress:PROC
EXTERN lstrcmpA:PROC

.DATA
    g_Extensions    EXTENSION 16 DUP(<>)
    g_nExtensionCount DD 0
    g_hMainWnd      DQ 0
    g_szExtPattern  DB "Extensions\*.rawr", 0
    g_szExportInit  DB "ExtensionInit", 0
    g_szExportActivate DB "ExtensionActivate", 0
    g_szExportCmd   DB "ExtensionExecuteCommand", 0

.CODE
; =============================================================================
; ExtensionHost_Init - Scan .rawr files in Extensions\ directory
; =============================================================================
ExtensionHost_Init PROC FRAME hParentWnd:QWORD
    LOCAL hFind:QWORD
    LOCAL wfd:WIN32_FIND_DATAA

    mov g_hMainWnd, rcx
    lea rsp, [rsp-328h]

    ; FindFirstFileA(lpFileName, lpFindFileData) -> rcx=pattern, rdx=wfd
    lea rcx, g_szExtPattern
    lea rdx, wfd
    call FindFirstFileA
    cmp rax, INVALID_HANDLE_VALUE
    je @@DONE
    mov hFind, rax

@@LOAD_LOOP:
    mov r10d, g_nExtensionCount
    cmp r10d, 16
    jge @@CLOSE

    ; LoadLibrary(cFileName)
    lea rcx, wfd.cFileName
    call LoadLibraryA
    test rax, rax
    jz @@NEXT

    mov r10d, g_nExtensionCount
    mov r11, r10
    imul r11, r11, sizeof EXTENSION
    lea r11, g_Extensions[r11]
    mov [r11].EXTENSION.hModule, rax

    mov rcx, rax
    lea rdx, g_szExportInit
    call GetProcAddress
    mov [r11].EXTENSION.api.pfnInit, rax

    mov rcx, [r11].EXTENSION.hModule
    lea rdx, g_szExportActivate
    call GetProcAddress
    mov [r11].EXTENSION.api.pfnActivate, rax

    mov rax, [r11].EXTENSION.api.pfnInit
    test rax, rax
    jz @@SKIP_INIT
    mov rcx, g_hMainWnd
    call rax

@@SKIP_INIT:
    inc g_nExtensionCount

@@NEXT:
    mov rcx, hFind
    lea rdx, wfd
    call FindNextFileA
    test eax, eax
    jnz @@LOAD_LOOP

@@CLOSE:
    mov rcx, hFind
    call FindClose

@@DONE:
    lea rsp, [rsp+328h]
    ret
ExtensionHost_Init ENDP

; =============================================================================
; ExtensionHost_ExecuteCommand - Route command to extension by name
; =============================================================================
ExtensionHost_ExecuteCommand PROC FRAME pszExtension:QWORD, pszCommand:QWORD, pvData:QWORD
    xor r8d, r8d
    mov eax, g_nExtensionCount
@@FIND_LOOP:
    cmp r8d, eax
    jge @@NOT_FOUND
    mov r9d, r8d
    imul r9, r9, sizeof EXTENSION
    lea rdx, g_Extensions[r9].EXTENSION.szName
    mov rcx, pszExtension
    call lstrcmpA
    test eax, eax
    jz @@FOUND
    inc r8d
    jmp @@FIND_LOOP

@@FOUND:
    mov r9d, r8d
    imul r9, r9, sizeof EXTENSION
    lea r11, g_Extensions[r9]
    mov rax, [r11].EXTENSION.api.pfnExecuteCommand
    test rax, rax
    jz @@NOT_FOUND
    mov rcx, pszCommand
    mov rdx, pvData
    call rax
    ret

@@NOT_FOUND:
    xor eax, eax
    ret
ExtensionHost_ExecuteCommand ENDP

; =============================================================================
; ExtensionHost_HandleChat - Route chat to AI extension (Copilot or Agentic)
; =============================================================================
ExtensionHost_HandleChat PROC FRAME pszMessage:QWORD, pszModel:QWORD, pfnCallback:QWORD
    mov rax, pszModel
    test rax, rax
    jz @@USE_AGENTIC
    cmp byte ptr [rax], 'g'
    je @@USE_COPILOT
@@USE_AGENTIC:
    mov ecx, 0
    jmp @@ROUTE
@@USE_COPILOT:
    mov ecx, 1
@@ROUTE:
    cmp ecx, 16
    jge @@NO_HANDLER
    mov r11, rcx
    imul r11, r11, sizeof EXTENSION
    lea r11, g_Extensions[r11]
    mov rax, [r11].EXTENSION.api.pfnHandleChat
    test rax, rax
    jz @@NO_HANDLER
    mov rcx, pszMessage
    mov rdx, pfnCallback
    call rax
@@NO_HANDLER:
    ret
ExtensionHost_HandleChat ENDP

END
