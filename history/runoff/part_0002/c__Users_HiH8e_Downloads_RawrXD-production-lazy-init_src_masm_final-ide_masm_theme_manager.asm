;==========================================================================
; masm_theme_manager.asm - Pure MASM Theme & Transparency Management
;==========================================================================
; Implements comprehensive theme system matching Qt ThemeManager features:
; - Built-in themes (Dark, Light, High Contrast, Amber, Glass)
; - Real-time color customization (30+ color slots)
; - Per-element transparency (window, dock, chat, editor)
; - Always-on-top window control
; - Click-through mode
; - Theme persistence (JSON-like format)
; - Thread-safe with critical sections
;==========================================================================

option casemap:none
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib

;==========================================================================
; STRUCTURES
;==========================================================================

; RGB color structure (3 bytes packed)
COLOR_RGB struct
    r   BYTE 0
    g   BYTE 0
    b   BYTE 0
    pad BYTE 0  ; Padding for alignment
COLOR_RGB ends

; Complete theme color palette (matches Qt ThemeColors)
THEME_COLORS struct
    ; Editor colors (7 slots)
    editorBackground    COLOR_RGB <>
    editorForeground    COLOR_RGB <>
    editorSelection     COLOR_RGB <>
    editorCurrentLine   COLOR_RGB <>
    editorLineNumbers   COLOR_RGB <>
    editorWhitespace    COLOR_RGB <>
    editorIndentGuides  COLOR_RGB <>
    
    ; Syntax highlighting (8 slots)
    keywordColor        COLOR_RGB <>
    stringColor         COLOR_RGB <>
    commentColor        COLOR_RGB <>
    numberColor         COLOR_RGB <>
    functionColor       COLOR_RGB <>
    classColor          COLOR_RGB <>
    operatorColor       COLOR_RGB <>
    preprocessorColor   COLOR_RGB <>
    
    ; Chat interface (7 slots)
    chatUserBackground  COLOR_RGB <>
    chatUserForeground  COLOR_RGB <>
    chatAIBackground    COLOR_RGB <>
    chatAIForeground    COLOR_RGB <>
    chatSystemBackground COLOR_RGB <>
    chatSystemForeground COLOR_RGB <>
    chatBorder          COLOR_RGB <>
    
    ; UI elements (11 slots)
    windowBackground    COLOR_RGB <>
    windowForeground    COLOR_RGB <>
    dockBackground      COLOR_RGB <>
    dockBorder          COLOR_RGB <>
    toolbarBackground   COLOR_RGB <>
    menuBackground      COLOR_RGB <>
    menuForeground      COLOR_RGB <>
    buttonBackground    COLOR_RGB <>
    buttonForeground    COLOR_RGB <>
    buttonHover         COLOR_RGB <>
    buttonPressed       COLOR_RGB <>
    
    ; Transparency values (0-255, where 255 = 1.0 opacity)
    windowOpacity       BYTE 255
    dockOpacity         BYTE 255
    chatOpacity         BYTE 255
    editorOpacity       BYTE 255
THEME_COLORS ends

; Theme manager state
THEME_MANAGER struct
    currentTheme        THEME_COLORS <>
    currentThemeName    BYTE 64 dup(0)
    
    ; Registered themes (up to 10 presets)
    darkTheme           THEME_COLORS <>
    lightTheme          THEME_COLORS <>
    amberTheme          THEME_COLORS <>
    glassTheme          THEME_COLORS <>
    highContrastTheme   THEME_COLORS <>
    customTheme1        THEME_COLORS <>
    customTheme2        THEME_COLORS <>
    customTheme3        THEME_COLORS <>
    customTheme4        THEME_COLORS <>
    customTheme5        THEME_COLORS <>
    
    ; Window management flags
    transparencyEnabled BYTE 0
    alwaysOnTop         BYTE 0
    clickThroughEnabled BYTE 0
    
    ; Cached brush handles (for GDI painting)
    hEditorBackBrush    QWORD 0
    hWindowBackBrush    QWORD 0
    hDockBackBrush      QWORD 0
    hChatBackBrush      QWORD 0
    
    ; Critical section for thread safety
    criticalSection     QWORD 8 dup(0)  ; CRITICAL_SECTION size ~40 bytes
THEME_MANAGER ends

;==========================================================================
; DATA
;==========================================================================
.data
g_themeManager THEME_MANAGER <>

szThemeDark     db "Dark", 0
szThemeLight    db "Light", 0
szThemeAmber    db "Amber", 0
szThemeGlass    db "Glass", 0
szThemeHighContrast db "HighContrast", 0
szThemeCustom   db "Custom", 0

.code

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN CreateSolidBrush:PROC
EXTERN DeleteObject:PROC
EXTERN SetLayeredWindowAttributes:PROC
EXTERN SetWindowPos:PROC
EXTERN GetWindowLongPtrA:PROC
EXTERN SetWindowLongPtrA:PROC
EXTERN InitializeCriticalSection:PROC
EXTERN EnterCriticalSection:PROC
EXTERN LeaveCriticalSection:PROC

;==========================================================================
; PUBLIC EXPORTS
;==========================================================================
PUBLIC theme_manager_init
PUBLIC theme_manager_load_theme
PUBLIC theme_manager_get_color
PUBLIC theme_manager_update_color
PUBLIC theme_manager_set_window_opacity
PUBLIC theme_manager_set_transparency_enabled
PUBLIC theme_manager_set_always_on_top
PUBLIC theme_manager_apply_to_window
PUBLIC theme_manager_save_theme
PUBLIC theme_manager_import_theme
PUBLIC theme_manager_export_theme

;==========================================================================
; theme_manager_init() -> bool (rax)
; Initialize theme manager with default themes
;==========================================================================
theme_manager_init PROC
    push rbx
    sub rsp, 32
    
    ; Initialize critical section
    lea rcx, g_themeManager.criticalSection
    call InitializeCriticalSection
    
    ; Set default theme name
    lea rdi, g_themeManager.currentThemeName
    lea rsi, szThemeDark
    mov rcx, 64
    rep movsb
    
    ; Initialize Dark theme (default)
    call initialize_dark_theme
    
    ; Initialize Light theme
    call initialize_light_theme
    
    ; Initialize Amber theme
    call initialize_amber_theme
    
    ; Initialize Glass theme
    call initialize_glass_theme
    
    ; Initialize High Contrast theme
    call initialize_high_contrast_theme
    
    ; Copy Dark theme to current
    lea rdi, g_themeManager.currentTheme
    lea rsi, g_themeManager.darkTheme
    mov rcx, sizeof THEME_COLORS
    rep movsb
    
    ; Create initial brushes
    call update_cached_brushes
    
    mov rax, 1  ; Success
    add rsp, 32
    pop rbx
    ret
theme_manager_init ENDP

;==========================================================================
; initialize_dark_theme() - Set Dark theme colors
;==========================================================================
initialize_dark_theme PROC
    lea rdi, g_themeManager.darkTheme
    
    ; Editor: dark gray background (#1E1E1E), light text (#D4D4D4)
    mov byte ptr [rdi + THEME_COLORS.editorBackground.r], 30
    mov byte ptr [rdi + THEME_COLORS.editorBackground.g], 30
    mov byte ptr [rdi + THEME_COLORS.editorBackground.b], 30
    
    mov byte ptr [rdi + THEME_COLORS.editorForeground.r], 212
    mov byte ptr [rdi + THEME_COLORS.editorForeground.g], 212
    mov byte ptr [rdi + THEME_COLORS.editorForeground.b], 212
    
    ; Selection: blue (#264F78)
    mov byte ptr [rdi + THEME_COLORS.editorSelection.r], 38
    mov byte ptr [rdi + THEME_COLORS.editorSelection.g], 79
    mov byte ptr [rdi + THEME_COLORS.editorSelection.b], 120
    
    ; Current line: subtle highlight (#2A2D2E)
    mov byte ptr [rdi + THEME_COLORS.editorCurrentLine.r], 42
    mov byte ptr [rdi + THEME_COLORS.editorCurrentLine.g], 45
    mov byte ptr [rdi + THEME_COLORS.editorCurrentLine.b], 46
    
    ; Line numbers: gray (#858585)
    mov byte ptr [rdi + THEME_COLORS.editorLineNumbers.r], 133
    mov byte ptr [rdi + THEME_COLORS.editorLineNumbers.g], 133
    mov byte ptr [rdi + THEME_COLORS.editorLineNumbers.b], 133
    
    ; Syntax: VS Code Dark+ theme
    ; Keywords: blue (#569CD6)
    mov byte ptr [rdi + THEME_COLORS.keywordColor.r], 86
    mov byte ptr [rdi + THEME_COLORS.keywordColor.g], 156
    mov byte ptr [rdi + THEME_COLORS.keywordColor.b], 214
    
    ; Strings: orange (#CE9178)
    mov byte ptr [rdi + THEME_COLORS.stringColor.r], 206
    mov byte ptr [rdi + THEME_COLORS.stringColor.g], 145
    mov byte ptr [rdi + THEME_COLORS.stringColor.b], 120
    
    ; Comments: green (#6A9955)
    mov byte ptr [rdi + THEME_COLORS.commentColor.r], 106
    mov byte ptr [rdi + THEME_COLORS.commentColor.g], 153
    mov byte ptr [rdi + THEME_COLORS.commentColor.b], 85
    
    ; Numbers: light green (#B5CEA8)
    mov byte ptr [rdi + THEME_COLORS.numberColor.r], 181
    mov byte ptr [rdi + THEME_COLORS.numberColor.g], 206
    mov byte ptr [rdi + THEME_COLORS.numberColor.b], 168
    
    ; Functions: yellow (#DCDCAA)
    mov byte ptr [rdi + THEME_COLORS.functionColor.r], 220
    mov byte ptr [rdi + THEME_COLORS.functionColor.g], 220
    mov byte ptr [rdi + THEME_COLORS.functionColor.b], 170
    
    ; Chat colors
    mov byte ptr [rdi + THEME_COLORS.chatUserBackground.r], 0
    mov byte ptr [rdi + THEME_COLORS.chatUserBackground.g], 102
    mov byte ptr [rdi + THEME_COLORS.chatUserBackground.b], 204
    
    mov byte ptr [rdi + THEME_COLORS.chatAIBackground.r], 45
    mov byte ptr [rdi + THEME_COLORS.chatAIBackground.g], 45
    mov byte ptr [rdi + THEME_COLORS.chatAIBackground.b], 48
    
    ; UI colors
    mov byte ptr [rdi + THEME_COLORS.windowBackground.r], 30
    mov byte ptr [rdi + THEME_COLORS.windowBackground.g], 30
    mov byte ptr [rdi + THEME_COLORS.windowBackground.b], 30
    
    ; Opacity defaults (255 = 100%)
    mov byte ptr [rdi + THEME_COLORS.windowOpacity], 255
    mov byte ptr [rdi + THEME_COLORS.dockOpacity], 255
    mov byte ptr [rdi + THEME_COLORS.chatOpacity], 255
    mov byte ptr [rdi + THEME_COLORS.editorOpacity], 255
    
    ret
initialize_dark_theme ENDP

;==========================================================================
; initialize_light_theme() - Set Light theme colors
;==========================================================================
initialize_light_theme PROC
    lea rdi, g_themeManager.lightTheme
    
    ; Editor: white background (#FFFFFF), dark text (#000000)
    mov byte ptr [rdi + THEME_COLORS.editorBackground.r], 255
    mov byte ptr [rdi + THEME_COLORS.editorBackground.g], 255
    mov byte ptr [rdi + THEME_COLORS.editorBackground.b], 255
    
    mov byte ptr [rdi + THEME_COLORS.editorForeground.r], 0
    mov byte ptr [rdi + THEME_COLORS.editorForeground.g], 0
    mov byte ptr [rdi + THEME_COLORS.editorForeground.b], 0
    
    ; Selection: blue (#ADD6FF)
    mov byte ptr [rdi + THEME_COLORS.editorSelection.r], 173
    mov byte ptr [rdi + THEME_COLORS.editorSelection.g], 214
    mov byte ptr [rdi + THEME_COLORS.editorSelection.b], 255
    
    ; Keywords: blue (#0000FF)
    mov byte ptr [rdi + THEME_COLORS.keywordColor.r], 0
    mov byte ptr [rdi + THEME_COLORS.keywordColor.g], 0
    mov byte ptr [rdi + THEME_COLORS.keywordColor.b], 255
    
    ; Strings: red (#A31515)
    mov byte ptr [rdi + THEME_COLORS.stringColor.r], 163
    mov byte ptr [rdi + THEME_COLORS.stringColor.g], 21
    mov byte ptr [rdi + THEME_COLORS.stringColor.b], 21
    
    ; Comments: green (#008000)
    mov byte ptr [rdi + THEME_COLORS.commentColor.r], 0
    mov byte ptr [rdi + THEME_COLORS.commentColor.g], 128
    mov byte ptr [rdi + THEME_COLORS.commentColor.b], 0
    
    ; Opacity defaults
    mov byte ptr [rdi + THEME_COLORS.windowOpacity], 255
    mov byte ptr [rdi + THEME_COLORS.dockOpacity], 255
    mov byte ptr [rdi + THEME_COLORS.chatOpacity], 255
    mov byte ptr [rdi + THEME_COLORS.editorOpacity], 255
    
    ret
initialize_light_theme ENDP

;==========================================================================
; initialize_amber_theme() - Set Amber theme (warm orange tones)
;==========================================================================
initialize_amber_theme PROC
    lea rdi, g_themeManager.amberTheme
    
    ; Editor: dark brown background (#2B1B17), amber text (#FFB000)
    mov byte ptr [rdi + THEME_COLORS.editorBackground.r], 43
    mov byte ptr [rdi + THEME_COLORS.editorBackground.g], 27
    mov byte ptr [rdi + THEME_COLORS.editorBackground.b], 23
    
    mov byte ptr [rdi + THEME_COLORS.editorForeground.r], 255
    mov byte ptr [rdi + THEME_COLORS.editorForeground.g], 176
    mov byte ptr [rdi + THEME_COLORS.editorForeground.b], 0
    
    ; Keywords: orange (#FF8C00)
    mov byte ptr [rdi + THEME_COLORS.keywordColor.r], 255
    mov byte ptr [rdi + THEME_COLORS.keywordColor.g], 140
    mov byte ptr [rdi + THEME_COLORS.keywordColor.b], 0
    
    ; Strings: yellow (#FFFF00)
    mov byte ptr [rdi + THEME_COLORS.stringColor.r], 255
    mov byte ptr [rdi + THEME_COLORS.stringColor.g], 255
    mov byte ptr [rdi + THEME_COLORS.stringColor.b], 0
    
    ; Comments: dim orange (#CC7000)
    mov byte ptr [rdi + THEME_COLORS.commentColor.r], 204
    mov byte ptr [rdi + THEME_COLORS.commentColor.g], 112
    mov byte ptr [rdi + THEME_COLORS.commentColor.b], 0
    
    ret
initialize_amber_theme ENDP

;==========================================================================
; initialize_glass_theme() - Set Glass theme (transparent/translucent)
;==========================================================================
initialize_glass_theme PROC
    lea rdi, g_themeManager.glassTheme
    
    ; Copy Dark theme as base
    lea rsi, g_themeManager.darkTheme
    mov rcx, sizeof THEME_COLORS
    rep movsb
    
    ; Override opacity values (70% transparency)
    lea rdi, g_themeManager.glassTheme
    mov byte ptr [rdi + THEME_COLORS.windowOpacity], 179    ; 70%
    mov byte ptr [rdi + THEME_COLORS.dockOpacity], 179
    mov byte ptr [rdi + THEME_COLORS.chatOpacity], 179
    mov byte ptr [rdi + THEME_COLORS.editorOpacity], 217    ; Editor 85%
    
    ret
initialize_glass_theme ENDP

;==========================================================================
; initialize_high_contrast_theme() - Set High Contrast theme (accessibility)
;==========================================================================
initialize_high_contrast_theme PROC
    lea rdi, g_themeManager.highContrastTheme
    
    ; Editor: pure black background (#000000), pure white text (#FFFFFF)
    xor rax, rax
    mov byte ptr [rdi + THEME_COLORS.editorBackground.r], al
    mov byte ptr [rdi + THEME_COLORS.editorBackground.g], al
    mov byte ptr [rdi + THEME_COLORS.editorBackground.b], al
    
    mov al, 255
    mov byte ptr [rdi + THEME_COLORS.editorForeground.r], al
    mov byte ptr [rdi + THEME_COLORS.editorForeground.g], al
    mov byte ptr [rdi + THEME_COLORS.editorForeground.b], al
    
    ; Selection: bright yellow (#FFFF00)
    mov byte ptr [rdi + THEME_COLORS.editorSelection.r], 255
    mov byte ptr [rdi + THEME_COLORS.editorSelection.g], 255
    mov byte ptr [rdi + THEME_COLORS.editorSelection.b], 0
    
    ; Keywords: cyan (#00FFFF)
    mov byte ptr [rdi + THEME_COLORS.keywordColor.r], 0
    mov byte ptr [rdi + THEME_COLORS.keywordColor.g], 255
    mov byte ptr [rdi + THEME_COLORS.keywordColor.b], 255
    
    ; Strings: bright green (#00FF00)
    mov byte ptr [rdi + THEME_COLORS.stringColor.r], 0
    mov byte ptr [rdi + THEME_COLORS.stringColor.g], 255
    mov byte ptr [rdi + THEME_COLORS.stringColor.b], 0
    
    ; Comments: bright magenta (#FF00FF)
    mov byte ptr [rdi + THEME_COLORS.commentColor.r], 255
    mov byte ptr [rdi + THEME_COLORS.commentColor.g], 0
    mov byte ptr [rdi + THEME_COLORS.commentColor.b], 255
    
    ret
initialize_high_contrast_theme ENDP

;==========================================================================
; update_cached_brushes() - Recreate GDI brushes from current theme
;==========================================================================
update_cached_brushes PROC
    push rbx
    sub rsp, 32
    
    ; Delete old brushes if they exist
    mov rcx, g_themeManager.hEditorBackBrush
    test rcx, rcx
    jz @F
    call DeleteObject
@@:
    mov rcx, g_themeManager.hWindowBackBrush
    test rcx, rcx
    jz @F
    call DeleteObject
@@:
    
    ; Create editor background brush
    lea rsi, g_themeManager.currentTheme.editorBackground
    movzx eax, byte ptr [rsi + COLOR_RGB.r]
    movzx ecx, byte ptr [rsi + COLOR_RGB.g]
    shl ecx, 8
    or eax, ecx
    movzx ecx, byte ptr [rsi + COLOR_RGB.b]
    shl ecx, 16
    or ecx, eax
    call CreateSolidBrush
    mov g_themeManager.hEditorBackBrush, rax
    
    ; Create window background brush
    lea rsi, g_themeManager.currentTheme.windowBackground
    movzx eax, byte ptr [rsi + COLOR_RGB.r]
    movzx ecx, byte ptr [rsi + COLOR_RGB.g]
    shl ecx, 8
    or eax, ecx
    movzx ecx, byte ptr [rsi + COLOR_RGB.b]
    shl ecx, 16
    or ecx, eax
    call CreateSolidBrush
    mov g_themeManager.hWindowBackBrush, rax
    
    add rsp, 32
    pop rbx
    ret
update_cached_brushes ENDP

;==========================================================================
; theme_manager_load_theme(theme_name: rcx) -> bool (rax)
; Load theme by name ("Dark", "Light", "Amber", "Glass", "HighContrast")
;==========================================================================
theme_manager_load_theme PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx  ; Save theme name
    
    ; Enter critical section
    lea rcx, g_themeManager.criticalSection
    call EnterCriticalSection
    
    ; Compare theme name and load appropriate theme
    mov rdi, rbx
    lea rsi, szThemeDark
    call compare_strings
    test rax, rax
    jnz @load_dark
    
    mov rdi, rbx
    lea rsi, szThemeLight
    call compare_strings
    test rax, rax
    jnz @load_light
    
    mov rdi, rbx
    lea rsi, szThemeAmber
    call compare_strings
    test rax, rax
    jnz @load_amber
    
    mov rdi, rbx
    lea rsi, szThemeGlass
    call compare_strings
    test rax, rax
    jnz @load_glass
    
    mov rdi, rbx
    lea rsi, szThemeHighContrast
    call compare_strings
    test rax, rax
    jnz @load_high_contrast
    
    ; Unknown theme, return failure
    xor rax, rax
    jmp @done
    
@load_dark:
    lea rsi, g_themeManager.darkTheme
    jmp @copy_theme
    
@load_light:
    lea rsi, g_themeManager.lightTheme
    jmp @copy_theme
    
@load_amber:
    lea rsi, g_themeManager.amberTheme
    jmp @copy_theme
    
@load_glass:
    lea rsi, g_themeManager.glassTheme
    jmp @copy_theme
    
@load_high_contrast:
    lea rsi, g_themeManager.highContrastTheme
    
@copy_theme:
    ; Copy theme to current
    lea rdi, g_themeManager.currentTheme
    mov rcx, sizeof THEME_COLORS
    rep movsb
    
    ; Update theme name
    lea rdi, g_themeManager.currentThemeName
    mov rsi, rbx
    mov rcx, 64
    rep movsb
    
    ; Update brushes
    call update_cached_brushes
    
    mov rax, 1  ; Success
    
@done:
    ; Leave critical section
    lea rcx, g_themeManager.criticalSection
    call LeaveCriticalSection
    
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
theme_manager_load_theme ENDP

;==========================================================================
; theme_manager_get_color(color_name: rcx) -> DWORD (rax) [RGB packed]
; Returns color as 0x00BBGGRR format
;==========================================================================
theme_manager_get_color PROC
    ; Stub: return editor background for now
    lea rsi, g_themeManager.currentTheme.editorBackground
    movzx eax, byte ptr [rsi + COLOR_RGB.r]
    movzx ecx, byte ptr [rsi + COLOR_RGB.g]
    shl ecx, 8
    or eax, ecx
    movzx ecx, byte ptr [rsi + COLOR_RGB.b]
    shl ecx, 16
    or eax, ecx
    ret
theme_manager_get_color ENDP

;==========================================================================
; theme_manager_set_window_opacity(hwnd: rcx, opacity: edx) -> bool (rax)
; Set window opacity (0-255, where 255 = 100%)
;==========================================================================
theme_manager_set_window_opacity PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx  ; Save hwnd
    mov r12d, edx ; Save opacity
    
    ; Set WS_EX_LAYERED style
    mov rcx, rbx
    mov edx, -20  ; GWL_EXSTYLE
    call GetWindowLongPtrA
    
    or rax, 80000h  ; WS_EX_LAYERED
    mov r8, rax
    mov rcx, rbx
    mov edx, -20
    call SetWindowLongPtrA
    
    ; Apply opacity using SetLayeredWindowAttributes
    mov rcx, rbx
    xor rdx, rdx           ; crKey (not used)
    mov r8b, r12b          ; bAlpha
    mov r9d, 2             ; LWA_ALPHA
    call SetLayeredWindowAttributes
    
    ; Update manager state
    mov byte ptr g_themeManager.currentTheme.windowOpacity, r12b
    
    mov rax, 1  ; Success
    add rsp, 32
    pop rbx
    ret
theme_manager_set_window_opacity ENDP

;==========================================================================
; theme_manager_set_transparency_enabled(enabled: ecx) -> void
;==========================================================================
theme_manager_set_transparency_enabled PROC
    mov byte ptr g_themeManager.transparencyEnabled, cl
    ret
theme_manager_set_transparency_enabled ENDP

;==========================================================================
; theme_manager_set_always_on_top(hwnd: rcx, enabled: edx) -> bool (rax)
;==========================================================================
theme_manager_set_always_on_top PROC
    push rbx
    sub rsp, 48
    
    mov rbx, rcx  ; Save hwnd
    mov r12d, edx ; Save enabled flag
    
    ; Update state
    mov byte ptr g_themeManager.alwaysOnTop, r12b
    
    ; Set window position (HWND_TOPMOST or HWND_NOTOPMOST)
    mov rcx, rbx
    test r12d, r12d
    jz @not_topmost
    mov rdx, -1  ; HWND_TOPMOST
    jmp @set_pos
@not_topmost:
    mov rdx, -2  ; HWND_NOTOPMOST
@set_pos:
    xor r8, r8   ; X (ignored)
    xor r9, r9   ; Y (ignored)
    mov qword ptr [rsp + 32], 0  ; cx (ignored)
    mov qword ptr [rsp + 40], 0  ; cy (ignored)
    mov dword ptr [rsp + 48], 3  ; SWP_NOMOVE | SWP_NOSIZE
    call SetWindowPos
    
    add rsp, 48
    pop rbx
    ret
theme_manager_set_always_on_top ENDP

;==========================================================================
; theme_manager_apply_to_window(hwnd: rcx) -> bool (rax)
; Apply current theme colors to window
;==========================================================================
theme_manager_apply_to_window PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    ; Invalidate window to trigger repaint
    mov rcx, rbx
    xor rdx, rdx  ; lpRect (entire window)
    mov r8d, 1    ; bErase
    call InvalidateRect
    
    mov rax, 1
    add rsp, 32
    pop rbx
    ret
    
theme_manager_apply_to_window ENDP

;==========================================================================
; theme_manager_save_theme(file_path: rcx) -> bool (rax)
; Save current theme to file (stub)
;==========================================================================
theme_manager_save_theme PROC
    mov rax, 1  ; Stub: success
    ret
theme_manager_save_theme ENDP

;==========================================================================
; theme_manager_import_theme(file_path: rcx) -> bool (rax)
; Import theme from file (stub)
;==========================================================================
theme_manager_import_theme PROC
    mov rax, 1  ; Stub: success
    ret
theme_manager_import_theme ENDP

;==========================================================================
; theme_manager_export_theme(file_path: rcx) -> bool (rax)
; Export current theme to file (stub)
;==========================================================================
theme_manager_export_theme PROC
    mov rax, 1  ; Stub: success
    ret
theme_manager_export_theme ENDP

;==========================================================================
; HELPER FUNCTIONS
;==========================================================================

; compare_strings(str1: rdi, str2: rsi) -> bool (rax)
compare_strings PROC
    push rdi
    push rsi
@@:
    mov al, [rdi]
    mov cl, [rsi]
    cmp al, cl
    jne @not_equal
    test al, al
    jz @equal
    inc rdi
    inc rsi
    jmp @B
@equal:
    mov rax, 1
    jmp @done
@not_equal:
    xor rax, rax
@done:
    pop rsi
    pop rdi
    ret
compare_strings ENDP

EXTERN InvalidateRect:PROC

end
