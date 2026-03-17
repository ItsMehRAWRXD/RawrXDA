; ============================================================================
; RawrXD Agentic IDE - Utility Macros
; ============================================================================

; ============================================================================
; String macros
; ============================================================================

; Copy zero-terminated string
szCopy macro dest, src
    push esi
    push edi
    mov esi, src
    mov edi, dest
    @@:
        lodsb
        stosb
        test al, al
        jnz @B
    pop edi
    pop esi
endm

; Get string length
szLen macro str
    LOCAL @@loop, @@done
    push edi
    mov edi, str
    xor eax, eax
    xor ecx, ecx
    dec ecx
    repne scasb
    not ecx
    dec ecx
    mov eax, ecx
    pop edi
endm

; Compare strings
szCmp macro str1, str2
    invoke lstrcmp, str1, str2
endm

; Concatenate strings
szCat macro dest, src
    invoke lstrcat, dest, src
endm

; ============================================================================
; Memory macros
; ============================================================================

; Allocate memory
MemAlloc macro size
    invoke GlobalAlloc, GMEM_FIXED or GMEM_ZEROINIT, size
endm

; Free memory
MemFree macro ptr
    invoke GlobalFree, ptr
endm

; Zero memory
MemZero macro ptr, size
    push edi
    push ecx
    mov edi, ptr
    mov ecx, size
    xor eax, eax
    rep stosb
    pop ecx
    pop edi
endm

; Copy memory
MemCopy macro dest, src, size
    push esi
    push edi
    push ecx
    mov esi, src
    mov edi, dest
    mov ecx, size
    rep movsb
    pop ecx
    pop edi
    pop esi
endm

; ============================================================================
; Debug macros
; ============================================================================

; Debug output to debugger
DbgOut macro msg
    ifdef DEBUG
        invoke OutputDebugString, addr msg
    endif
endm

; Assert macro
Assert macro condition, msg
    ifdef DEBUG
        .if !(condition)
            invoke MessageBox, NULL, addr msg, addr szAssertTitle, MB_OK or MB_ICONERROR
            invoke ExitProcess, 1
        .endif
    endif
endm

; ============================================================================
; Error handling macros
; ============================================================================

; Check for NULL handle
CheckHandle macro handle, label
    test handle, handle
    jz label
endm

; Check for error return
CheckError macro retval, label
    test retval, retval
    jz label
endm

; ============================================================================
; UI macros
; ============================================================================

; Create window helper
CreateWnd macro class, title, style, x, y, w, h, parent, id
    invoke CreateWindowEx, 0, addr class, addr title, style, x, y, w, h, parent, id, g_hInstance, NULL
endm

; Create window with extended style
CreateWndEx macro exstyle, class, title, style, x, y, w, h, parent, id
    invoke CreateWindowEx, exstyle, addr class, addr title, style, x, y, w, h, parent, id, g_hInstance, NULL
endm

; Show/hide window
ShowWnd macro hwnd
    invoke ShowWindow, hwnd, SW_SHOW
    invoke UpdateWindow, hwnd
endm

HideWnd macro hwnd
    invoke ShowWindow, hwnd, SW_HIDE
endm

; Enable/disable window
EnableWnd macro hwnd
    invoke EnableWindow, hwnd, TRUE
endm

DisableWnd macro hwnd
    invoke EnableWindow, hwnd, FALSE
endm

; ============================================================================
; Message box macros
; ============================================================================

MsgInfo macro msg
    invoke MessageBox, hMainWindow, addr msg, addr szAppTitle, MB_OK or MB_ICONINFORMATION
endm

MsgError macro msg
    invoke MessageBox, hMainWindow, addr msg, addr szAppTitle, MB_OK or MB_ICONERROR
endm

MsgWarning macro msg
    invoke MessageBox, hMainWindow, addr msg, addr szAppTitle, MB_OK or MB_ICONWARNING
endm

MsgQuestion macro msg
    invoke MessageBox, hMainWindow, addr msg, addr szAppTitle, MB_YESNO or MB_ICONQUESTION
endm

; ============================================================================
; List/Array macros
; ============================================================================

; Initialize array with zero
ArrayZero macro arr, count, elemsize
    LOCAL size
    size = count * elemsize
    MemZero arr, size
endm

; Get array element address
ArrayElem macro arr, index, elemsize
    mov eax, index
    mov ecx, elemsize
    imul eax, ecx
    add eax, arr
endm

; ============================================================================
; Bit manipulation macros
; ============================================================================

; Set bit
BitSet macro var, bit
    bts var, bit
endm

; Clear bit
BitClear macro var, bit
    btr var, bit
endm

; Test bit
BitTest macro var, bit
    bt var, bit
endm

; Toggle bit
BitToggle macro var, bit
    btc var, bit
endm

; ============================================================================
; Flag macros
; ============================================================================

; Set flag
SetFlag macro flags, flag
    or flags, flag
endm

; Clear flag
ClearFlag macro flags, flag
    and flags, NOT flag
endm

; Test flag
TestFlag macro flags, flag
    test flags, flag
endm

; Toggle flag
ToggleFlag macro flags, flag
    xor flags, flag
endm

; ============================================================================
; Timer macros
; ============================================================================

; Start timer
StartTimer macro id, interval
    invoke SetTimer, hMainWindow, id, interval, NULL
endm

; Stop timer
StopTimer macro id
    invoke KillTimer, hMainWindow, id
endm

; ============================================================================
; Drawing macros
; ============================================================================

; Create solid brush
CreateBrush macro color
    invoke CreateSolidBrush, color
endm

; Delete GDI object
DeleteObj macro handle
    invoke DeleteObject, handle
endm

; Select object into DC
SelectObj macro hdc, obj
    invoke SelectObject, hdc, obj
endm

; ============================================================================
; File I/O macros
; ============================================================================

; Open file for reading
FileOpen macro filename
    invoke CreateFile, addr filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
endm

; Create file for writing
FileCreate macro filename
    invoke CreateFile, addr filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
endm

; Close file
FileClose macro handle
    invoke CloseHandle, handle
endm

; Read from file
FileRead macro handle, buffer, size
    LOCAL bytesRead
    invoke ReadFile, handle, buffer, size, addr bytesRead, NULL
endm

; Write to file
FileWrite macro handle, buffer, size
    LOCAL bytesWritten
    invoke WriteFile, handle, buffer, size, addr bytesWritten, NULL
endm

; ============================================================================
; Registry macros
; ============================================================================

; Open registry key
RegOpen macro rootkey, subkey, access
    LOCAL hkey
    invoke RegOpenKeyEx, rootkey, addr subkey, 0, access, addr hkey
    mov eax, hkey
endm

; Close registry key
RegClose macro hkey
    invoke RegCloseKey, hkey
endm

; Read registry string
RegReadString macro hkey, valuename, buffer, size
    invoke RegQueryValueEx, hkey, addr valuename, NULL, NULL, addr buffer, addr size
endm

; Write registry string
RegWriteString macro hkey, valuename, data
    LOCAL len
    szLen data
    mov len, eax
    invoke RegSetValueEx, hkey, addr valuename, 0, REG_SZ, addr data, len
endm

; ============================================================================
; Thread macros
; ============================================================================

; Create thread
ThreadCreate macro func, param
    invoke CreateThread, NULL, 0, func, param, 0, NULL
endm

; Wait for thread
ThreadWait macro handle
    invoke WaitForSingleObject, handle, INFINITE
endm

; Close thread handle
ThreadClose macro handle
    invoke CloseHandle, handle
endm

; ============================================================================
; Synchronization macros
; ============================================================================

; Create mutex
MutexCreate macro
    invoke CreateMutex, NULL, FALSE, NULL
endm

; Lock mutex
MutexLock macro handle
    invoke WaitForSingleObject, handle, INFINITE
endm

; Unlock mutex
MutexUnlock macro handle
    invoke ReleaseMutex, handle
endm

; Close mutex
MutexClose macro handle
    invoke CloseHandle, handle
endm

; ============================================================================
; Math macros
; ============================================================================

; Absolute value
Abs macro value
    mov eax, value
    test eax, eax
    jns @F
    neg eax
    @@:
endm

; Min of two values
Min macro val1, val2
    mov eax, val1
    mov ecx, val2
    cmp eax, ecx
    jle @F
    mov eax, ecx
    @@:
endm

; Max of two values
Max macro val1, val2
    mov eax, val1
    mov ecx, val2
    cmp eax, ecx
    jge @F
    mov eax, ecx
    @@:
endm

; Clamp value between min and max
Clamp macro value, minval, maxval
    mov eax, value
    mov ecx, minval
    cmp eax, ecx
    jge @F
    mov eax, ecx
    @@:
    mov ecx, maxval
    cmp eax, ecx
    jle @F
    mov eax, ecx
    @@:
endm

; ============================================================================
; Callback macros
; ============================================================================

; Invoke callback if not NULL
InvokeCallback macro callback, param1, param2
    .if callback != 0
        push param2
        push param1
        call callback
    .endif
endm

; ============================================================================
; Alignment macros
; ============================================================================

; Align up to boundary
AlignUp macro value, alignment
    mov eax, value
    add eax, alignment
    dec eax
    mov ecx, alignment
    xor edx, edx
    div ecx
    imul eax, alignment
endm

; Align down to boundary
AlignDown macro value, alignment
    mov eax, value
    xor edx, edx
    mov ecx, alignment
    div ecx
    imul eax, alignment
endm

; ============================================================================
; Utility constants
; ============================================================================

szAppTitle              db "RawrXD Agentic IDE", 0
szAssertTitle           db "Assertion Failed", 0
szOK                    db "OK", 0
szCancel                db "Cancel", 0
szYes                   db "Yes", 0
szNo                    db "No", 0
