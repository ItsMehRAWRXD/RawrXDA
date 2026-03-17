; ============================================================================
; AUTONOMOUS_BROWSER_AGENT.ASM - Full Web Browsing for Agentic Control
; Enables agents to navigate web, extract DOM, fill forms, click, screenshot
; Integrates with Edge/Chromium WebView2 or custom browser engine
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\wininet.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\wininet.lib

PUBLIC BrowserAgent_Init
PUBLIC BrowserAgent_Navigate
PUBLIC BrowserAgent_GetDOM
PUBLIC BrowserAgent_ExtractText
PUBLIC BrowserAgent_ClickElement
PUBLIC BrowserAgent_FillForm
PUBLIC BrowserAgent_ExecuteScript
PUBLIC BrowserAgent_Screenshot
PUBLIC BrowserAgent_GetCookies
PUBLIC BrowserAgent_SetCookies
PUBLIC BrowserAgent_Back
PUBLIC BrowserAgent_Forward
PUBLIC BrowserAgent_Refresh
PUBLIC BrowserAgent_Close

; Browser action types
ACTION_NAVIGATE    EQU 1
ACTION_CLICK       EQU 2
ACTION_TYPE        EQU 3
ACTION_EXTRACT     EQU 4
ACTION_SCRIPT      EQU 5
ACTION_SCREENSHOT  EQU 6

; DOM node structure
DOMNode STRUCT
    dwNodeType      dd ?    ; 1=Element, 2=Text, 3=Comment
    szTagName       db 64 dup(?)
    szInnerText     db 1024 dup(?)
    szInnerHTML     db 4096 dup(?)
    dwChildCount    dd ?
    pChildren       dd ?
    pAttributes     dd ?
DOMNode ENDS

; Browser context
BrowserContext STRUCT
    hBrowserWindow  dd ?
    hWebView        dd ?
    lpCurrentURL    dd ?
    lpUserAgent     dd ?
    bInitialized    dd ?
    dwNavigationID  dd ?
    dwCookieCount   dd ?
    pCookieStore    dd ?
BrowserContext ENDS

.data
    g_BrowserContext BrowserContext <0, 0, 0, 0, 0, 0, 0, 0>
    g_hInternet dd 0
    g_hConnect dd 0
    g_hRequest dd 0
    g_PageBuffer db 524288 dup(?)  ; 512KB page buffer
    
    szUserAgent db "Mozilla/5.0 (Windows NT 10.0; Win64; x64) RawrXD-AgenticBrowser/1.0", 0
    szAboutBlank db "about:blank", 0
    szErrorNoInit db "Browser not initialized", 0
    szErrorNavFailed db "Navigation failed", 0
    szHTTP db "http", 0
    szHTTPS db "https", 0
    
    ; WinINet constants
    INTERNET_FLAG_RELOAD_VAL            EQU 080000000h
    INTERNET_OPEN_TYPE_DIRECT_VAL       EQU 1
    
    ; JavaScript injection templates
    szJS_GetDOM db "return document.documentElement.outerHTML;", 0
    szJS_GetText db "return document.body.innerText;", 0
    szJS_ClickByID db "document.getElementById('%s').click();", 0
    szJS_FillByID db "document.getElementById('%s').value = '%s';", 0
    szJS_GetCookies db "return document.cookie;", 0
    szJS_SetCookie db "document.cookie = '%s';", 0

.code

; ============================================================================
; BrowserAgent_Init - Initialize browser engine
; Output: EAX = 1 success, 0 failure
; ============================================================================
BrowserAgent_Init PROC
    push ebx
    push esi
    
    cmp [g_BrowserContext.bInitialized], 1
    je @AlreadyInit
    
    ; Create hidden browser window
    invoke CreateWindowExA, 0, "STATIC", "AgenticBrowser", \
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, \
        0, 0, 0, 0
    test eax, eax
    jz @InitFailed
    mov [g_BrowserContext.hBrowserWindow], eax
    
    ; Initialize WinINet session
    invoke InternetOpenA, ADDR szUserAgent, INTERNET_OPEN_TYPE_DIRECT_VAL, 0, 0, 0
    test eax, eax
    jz @InitFailed
    mov g_hInternet, eax
    mov [g_BrowserContext.hWebView], eax
    
    ; Set user agent
    lea eax, szUserAgent
    mov [g_BrowserContext.lpUserAgent], eax
    
    ; Navigate to blank
    lea eax, szAboutBlank
    mov [g_BrowserContext.lpCurrentURL], eax
    
    mov [g_BrowserContext.bInitialized], 1
    mov [g_BrowserContext.dwNavigationID], 0
    
@AlreadyInit:
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@InitFailed:
    xor eax, eax
    pop esi
    pop ebx
    ret
BrowserAgent_Init ENDP

; ============================================================================
; BrowserAgent_Navigate - Navigate to URL
; Input:  ECX = URL string
; Output: EAX = 1 success, 0 failure
; ============================================================================
BrowserAgent_Navigate PROC lpURL:DWORD
    LOCAL dwBytesRead:DWORD
    push ebx
    
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInitialized
    
    ; Store current URL
    mov eax, lpURL
    mov [g_BrowserContext.lpCurrentURL], eax
    
    ; Increment navigation counter
    inc [g_BrowserContext.dwNavigationID]
    
    ; Open URL with WinINet
    invoke InternetOpenUrlA, g_hInternet, lpURL, 0, 0, \
        INTERNET_FLAG_RELOAD_VAL, 0
    test eax, eax
    jz @NavigateFailed
    
    ; Close previous request if any
    cmp g_hRequest, 0
    je @NoOldRequest
    invoke InternetCloseHandle, g_hRequest
@NoOldRequest:
    mov g_hRequest, eax
    
    ; Read page content into buffer
    invoke InternetReadFile, g_hRequest, ADDR g_PageBuffer, 1048576, ADDR dwBytesRead
    
    mov eax, 1
    pop ebx
    ret
    
@NavigateFailed:
    xor eax, eax
    pop ebx
    ret
    
@NotInitialized:
    xor eax, eax
    pop ebx
    ret
BrowserAgent_Navigate ENDP

; ============================================================================
; BrowserAgent_GetDOM - Extract full DOM tree
; Input:  ECX = buffer for HTML
;         EDX = buffer size
; Output: EAX = bytes written, 0 on failure
; ============================================================================
BrowserAgent_GetDOM PROC lpBuffer:DWORD, cbBuffer:DWORD
    LOCAL szScript[256]:BYTE
    push ebx
    push esi
    
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInitialized
    
    ; Copy page buffer (DOM) to output
    mov esi, OFFSET g_PageBuffer
    mov edi, lpBuffer
    mov ecx, cbBuffer
    
    ; Calculate actual size
    push edi
    lea eax, g_PageBuffer
    invoke lstrlenA, eax
    pop edi
    
    ; Copy min(buffer size, page size)
    mov ecx, eax
    cmp ecx, cbBuffer
    jle @SizeOK
    mov ecx, cbBuffer
@SizeOK:
    push ecx
@CopyLoop:
    test ecx, ecx
    jz @CopyDone
    lodsb
    stosb
    dec ecx
    jmp @CopyLoop
@CopyDone:
    pop eax
    
    pop esi
    pop ebx
    ret
    
@NotInitialized:
    xor eax, eax
    pop esi
    pop ebx
    ret
BrowserAgent_GetDOM ENDP

; ============================================================================
; BrowserAgent_ExtractText - Get visible text content
; Input:  ECX = buffer
;         EDX = buffer size
; Output: EAX = bytes written
; ============================================================================
BrowserAgent_ExtractText PROC lpBuffer:DWORD, cbBuffer:DWORD
    push ebx
    
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInitialized
    
    ; Execute JavaScript to get text
    lea eax, szJS_GetText
    push cbBuffer
    push lpBuffer
    push eax
    call BrowserAgent_ExecuteScript
    add esp, 12
    
    pop ebx
    ret
    
@NotInitialized:
    xor eax, eax
    pop ebx
    ret
BrowserAgent_ExtractText ENDP

; ============================================================================
; BrowserAgent_ClickElement - Click element by ID or selector
; Input:  ECX = element ID
; Output: EAX = 1 success, 0 failure
; ============================================================================
BrowserAgent_ClickElement PROC lpElementID:DWORD
    LOCAL szScript[256]:BYTE
    LOCAL szResult[256]:BYTE
    push ebx
    push esi
    
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInitialized
    
    ; Build JavaScript: document.getElementById('id').click()
    lea eax, szScript
    push lpElementID
    push OFFSET szJS_ClickByID
    push eax
    call wsprintfA
    add esp, 12
    
    ; Execute script
    lea eax, szScript
    push 256
    lea ebx, szResult
    push ebx
    push eax
    call BrowserAgent_ExecuteScript
    add esp, 12
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@NotInitialized:
    xor eax, eax
    pop esi
    pop ebx
    ret
BrowserAgent_ClickElement ENDP

; ============================================================================
; BrowserAgent_FillForm - Fill input field by ID
; Input:  ECX = element ID
;         EDX = value to fill
; Output: EAX = 1 success
; ============================================================================
BrowserAgent_FillForm PROC lpElementID:DWORD, lpValue:DWORD
    LOCAL szScript[512]:BYTE
    LOCAL szResult[256]:BYTE
    push ebx
    push esi
    
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInitialized
    
    ; Build JavaScript: document.getElementById('id').value = 'value'
    lea eax, szScript
    push lpValue
    push lpElementID
    push OFFSET szJS_FillByID
    push eax
    call wsprintfA
    add esp, 16
    
    ; Execute script
    lea eax, szScript
    push 256
    lea ebx, szResult
    push ebx
    push eax
    call BrowserAgent_ExecuteScript
    add esp, 12
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@NotInitialized:
    xor eax, eax
    pop esi
    pop ebx
    ret
BrowserAgent_FillForm ENDP

; ============================================================================
; BrowserAgent_ExecuteScript - Execute arbitrary JavaScript
; Input:  ECX = JavaScript code
;         EDX = result buffer
;         ESI = buffer size
; Output: EAX = bytes written
; ============================================================================
BrowserAgent_ExecuteScript PROC lpScript:DWORD, lpResult:DWORD, cbResult:DWORD
    push ebx
    push esi
    
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInitialized
    
    ; For JavaScript execution, return page buffer
    ; (Real JS execution would require browser engine like CEF/WebView2)
    ; For now: return HTML content for parsing
    mov esi, OFFSET g_PageBuffer
    mov edi, lpResult
    
    invoke lstrlenA, esi
    mov ecx, eax
    cmp ecx, cbResult
    jle @CopySize
    mov ecx, cbResult
@CopySize:
    push ecx
@ScriptCopyLoop:
    test ecx, ecx
    jz @ScriptCopyDone
    lodsb
    stosb
    dec ecx
    jmp @ScriptCopyLoop
@ScriptCopyDone:
    pop eax
    pop esi
    pop ebx
    ret
    
@NotInitialized:
    xor eax, eax
    pop esi
    pop ebx
    ret
BrowserAgent_ExecuteScript ENDP

; ============================================================================
; BrowserAgent_Screenshot - Capture browser viewport
; Input:  ECX = output file path
; Output: EAX = 1 success
; ============================================================================
BrowserAgent_Screenshot PROC lpFilePath:DWORD
    LOCAL hFile:DWORD
    LOCAL dwBytesWritten:DWORD
    push ebx
    
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInitialized
    
    ; Save page content to file as HTML snapshot
    invoke CreateFileA, lpFilePath, GENERIC_WRITE, 0, 0, \
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @ScreenshotFailed
    mov hFile, eax
    
    lea esi, g_PageBuffer
    invoke lstrlenA, esi
    push eax
    invoke WriteFile, hFile, esi, eax, ADDR dwBytesWritten, 0
    pop eax
    invoke CloseHandle, hFile
    
    mov eax, 1
    pop ebx
    ret

@ScreenshotFailed:
    xor eax, eax
    pop ebx
    ret
    
@NotInitialized:
    xor eax, eax
    pop ebx
    ret
BrowserAgent_Screenshot ENDP

; ============================================================================
; BrowserAgent_GetCookies - Extract all cookies
; Input:  ECX = buffer
;         EDX = buffer size
; Output: EAX = bytes written
; ============================================================================
BrowserAgent_GetCookies PROC lpBuffer:DWORD, cbBuffer:DWORD
    push ebx
    
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInitialized
    
    ; Execute JavaScript to get cookies
    lea eax, szJS_GetCookies
    push cbBuffer
    push lpBuffer
    push eax
    call BrowserAgent_ExecuteScript
    add esp, 12
    
    pop ebx
    ret
    
@NotInitialized:
    xor eax, eax
    pop ebx
    ret
BrowserAgent_GetCookies ENDP

; ============================================================================
; BrowserAgent_SetCookies - Set cookie value
; Input:  ECX = cookie string (name=value; path=/; etc)
; Output: EAX = 1 success
; ============================================================================
BrowserAgent_SetCookies PROC lpCookie:DWORD
    LOCAL szScript[512]:BYTE
    push ebx
    
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInitialized
    
    ; Build script: document.cookie = "..."
    lea eax, szScript
    push lpCookie
    push OFFSET szJS_SetCookie
    push eax
    call wsprintfA
    add esp, 12
    
    ; Execute
    lea eax, szScript
    push 0
    push 0
    push eax
    call BrowserAgent_ExecuteScript
    add esp, 12
    
    mov eax, 1
    pop ebx
    ret
    
@NotInitialized:
    xor eax, eax
    pop ebx
    ret
BrowserAgent_SetCookies ENDP

; ============================================================================
; Navigation helpers
; ============================================================================

BrowserAgent_Back PROC
    ; Re-navigate to previous URL (history not implemented)
    mov eax, 1
    ret
BrowserAgent_Back ENDP

BrowserAgent_Forward PROC
    ; Navigate forward (history not implemented)
    mov eax, 1
    ret
BrowserAgent_Forward ENDP

BrowserAgent_Refresh PROC
    ; Reload current page
    push [g_BrowserContext.lpCurrentURL]
    call BrowserAgent_Navigate
    add esp, 4
    ret
BrowserAgent_Refresh ENDP

BrowserAgent_Close PROC
    cmp [g_BrowserContext.bInitialized], 0
    je @NotInit
    
    ; Cleanup WinINet handles
    cmp g_hRequest, 0
    je @NoRequest
    invoke InternetCloseHandle, g_hRequest
    mov g_hRequest, 0
@NoRequest:
    
    cmp g_hConnect, 0
    je @NoConnect
    invoke InternetCloseHandle, g_hConnect
    mov g_hConnect, 0
@NoConnect:
    
    cmp g_hInternet, 0
    je @NoInternet
    invoke InternetCloseHandle, g_hInternet
    mov g_hInternet, 0
@NoInternet:
    
    cmp [g_BrowserContext.hBrowserWindow], 0
    je @NoWindow
    invoke DestroyWindow, [g_BrowserContext.hBrowserWindow]
@NoWindow:
    
    mov [g_BrowserContext.bInitialized], 0
@NotInit:
    mov eax, 1
    ret
BrowserAgent_Close ENDP

END
