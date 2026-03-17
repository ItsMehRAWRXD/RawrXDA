; ============================================================================
; File 32: theme_system.asm - Dark/light theme system with JSON loading
; ============================================================================
; Purpose: Load JSON themes, map token types to colors, dynamic theme switching
; Uses: JSON parsing, color table LUT, real-time re-rendering
; Functions: Init, LoadTheme, SetActiveTheme, GetTokenColor, ReloadThemes
; ============================================================================

.code

; CONSTANTS
; ============================================================================

THEME_MAX_NAME          equ 64
THEME_MAX_COLORS        equ 256      ; max color entries per theme
THEME_COLOR_ENTRY_SIZE  equ 16       ; { type, bgra_color }
MAX_THEMES              equ 10

; THEME FILE FORMAT (JSON)
; ============================================================================
; {
;   "name": "Default Dark",
;   "colors": {
;     "keyword": "#A464A8",
;     "string": "#FF8040",
;     "comment": "#6CA955",
;     "identifier": "#D4D4D4",
;     "number": "#B5CEA8",
;     "operator": "#9CDCFE",
;     "background": "#1E1E1E",
;     "selection": "#264F78",
;     "error": "#FF6B6B"
;   }
; }

; INITIALIZATION
; ============================================================================

ThemeSystem_Init PROC USES rbx rcx rdx rsi rdi
    ; Returns: ThemeSystem* in rax
    ; { themes[], themeCount, activeTheme, colorLUT[], mutex }
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    
    ; Allocate main struct (96 bytes)
    mov rdx, 0
    mov r8, 96
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rbx, rax  ; rbx = ThemeSystem*
    
    ; Allocate theme array (10 themes × 512 bytes)
    mov r8, 5120
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rbx + 0], rax  ; themes
    
    ; Allocate color LUT (256 entries × 4 bytes = 1KB)
    mov r8, 1024
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov [rbx + 8], rax  ; colorLUT
    
    ; Initialize fields
    mov qword ptr [rbx + 16], 0  ; themeCount = 0
    mov qword ptr [rbx + 24], 0  ; activeTheme = 0
    
    ; Initialize CRITICAL_SECTION
    lea rdx, [rbx + 32]
    sub rsp, 40
    mov rcx, rdx
    call InitializeCriticalSection
    add rsp, 40
    
    ; Load default themes from disk
    call ThemeSystem_LoadDefaultThemes
    
    mov rax, rbx
    ret
ThemeSystem_Init ENDP

; LOAD DEFAULT THEMES
; ============================================================================

ThemeSystem_LoadDefaultThemes PROC USES rbx rcx rdx rsi rdi r8 r9
    ; Load built-in themes: default-dark, default-light, solarized-dark, etc.
    
    ; Theme 1: Default Dark
    lea rcx, [rel default_dark_json]
    call ThemeSystem_LoadThemeFromJson
    
    ; Theme 2: Default Light
    lea rcx, [rel default_light_json]
    call ThemeSystem_LoadThemeFromJson
    
    ; Theme 3: Solarized Dark
    lea rcx, [rel solarized_dark_json]
    call ThemeSystem_LoadThemeFromJson
    
    ; Theme 4: Solarized Light
    lea rcx, [rel solarized_light_json]
    call ThemeSystem_LoadThemeFromJson
    
    ret
ThemeSystem_LoadDefaultThemes ENDP

; LOAD THEME FROM JSON
; ============================================================================

ThemeSystem_LoadThemeFromJson PROC USES rbx rcx rdx rsi rdi r8 r9 jsonBuffer:PTR BYTE
    ; jsonBuffer = JSON theme string
    ; Parses and adds to theme array
    ; Returns: 1 if loaded
    
    mov rsi, jsonBuffer
    
    ; Mini-parser: extract theme name and color entries
    ; Look for "name": "..." pattern
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    mov rdx, 0
    mov r8, 512  ; theme struct size
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rbx, rax  ; rbx = theme struct
    
    ; Extract theme name
    mov rdi, rbx
    lea rax, [rel name_pattern]
    
    ; Parse "name": "..." 
    mov rcx, 0  ; pattern index
@name_search:
    mov al, byte ptr [rsi]
    cmp al, byte ptr [rax]
    jne @name_next
    
    ; Check if entire pattern matches
    ; (stub for now)
    
    jmp @name_found
    
@name_next:
    inc rsi
    jmp @name_search
    
@name_found:
    ; Extract name value (between quotes)
    mov rdi, rbx  ; store at name field
    
    ; Skip to opening quote of value
    mov rcx, 0
@find_name_quote:
    mov al, byte ptr [rsi]
    cmp al, '"'
    je @name_copy
    inc rsi
    jmp @find_name_quote
    
@name_copy:
    inc rsi  ; skip opening quote
    mov rcx, 0
@copy_name_loop:
    mov al, byte ptr [rsi]
    cmp al, '"'
    je @name_copied
    mov byte ptr [rdi + rcx], al
    inc rsi
    inc rcx
    cmp rcx, 64  ; max name length
    jl @copy_name_loop
    
@name_copied:
    mov byte ptr [rdi + rcx], 0  ; null terminate
    
    ; Extract colors array
    mov r8 = 0  ; color count
    mov rdi = rbx
    add rdi, 64  ; colors array offset
    
@colors_loop:
    cmp r8, THEME_MAX_COLORS
    jge @colors_done
    
    ; Look for pattern: "keyword": "#rrggbb"
    mov rcx, 0
    
    ; Parse color entries
    ; (TODO: implement full JSON color parsing)
    
    inc r8
    jmp @colors_loop
    
@colors_done:
    mov [rbx + 320], r8  ; store color count
    
    mov rax, 1
    ret
ThemeSystem_LoadThemeFromJson ENDP

; SET ACTIVE THEME
; ============================================================================

ThemeSystem_SetActiveTheme PROC USES rbx rcx rdx rsi rdi themeSystem:PTR DWORD, themeName:PTR BYTE
    ; themeSystem = ThemeSystem*
    ; themeName = "Default Dark", "Solarized Light", etc.
    ; Returns: 1 if successful
    
    mov rcx, themeSystem
    lea rdx, [rcx + 32]
    sub rsp, 40
    mov rcx, rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx, themeSystem
    mov rsi, themeName
    
    ; Find theme by name
    mov rax, [rcx + 0]   ; themes array
    mov r8, [rcx + 16]   ; themeCount
    xor r9, r9           ; theme index
    
@find_theme_loop:
    cmp r9, r8
    jge @theme_not_found
    
    ; Get theme name at index
    lea r10, [rax + r9*512]  ; theme at index
    mov r11, r10  ; theme.name
    
    ; Compare with requested name
    mov rdi, rsi
    mov rsi, r11
    
@name_cmp_loop:
    mov al, byte ptr [rdi]
    mov bl, byte ptr [rsi]
    cmp al, bl
    jne @next_theme
    cmp al, 0
    je @theme_found
    inc rdi
    inc rsi
    jmp @name_cmp_loop
    
@next_theme:
    inc r9
    jmp @find_theme_loop
    
@theme_found:
    ; Build color LUT for this theme
    mov rax, [rcx + 8]   ; colorLUT
    mov r10, [r10 + 64]  ; theme.colors
    mov r11, [r10 + 320] ; color count
    
    xor r12, r12  ; color index
@build_lut_loop:
    cmp r12, r11
    jge @lut_done
    
    ; Copy color to LUT
    ; (colors are already mapped by token type)
    mov r13, [r10 + r12*16]  ; color.bgra
    mov [rax + r12*4], r13d
    
    inc r12
    jmp @build_lut_loop
    
@lut_done:
    ; Update active theme
    mov rcx, themeSystem
    mov [rcx + 24], r9  ; activeTheme = theme index
    
    ; Trigger full re-render
    ; (would call InvalidateRect on editor window)
    
    mov rcx, themeSystem
    lea rdx, [rcx + 32]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 1
    ret
    
@theme_not_found:
    mov rcx, themeSystem
    lea rdx, [rcx + 32]
    sub rsp, 40
    mov rcx, rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax, 0
    ret
ThemeSystem_SetActiveTheme ENDP

; GET TOKEN COLOR
; ============================================================================

ThemeSystem_GetTokenColor PROC USES rbx rcx rdx tokenType:DWORD, themeSystem:PTR DWORD
    ; tokenType = 1-6 (keyword, string, comment, ident, number, operator)
    ; themeSystem = ThemeSystem*
    ; Returns: BGRA color in rax
    
    mov rcx, themeSystem
    mov edx, [tokenType]
    
    ; Get active theme's color LUT
    mov rax, [rcx + 8]   ; colorLUT
    
    ; Bounds check
    cmp edx, 0
    jle @color_default
    cmp edx, 256
    jge @color_default
    
    ; Look up color
    mov eax, [rax + edx*4]
    ret
    
@color_default:
    mov eax, 0xFFFFFFFF  ; white
    ret
ThemeSystem_GetTokenColor ENDP

; RELOAD THEMES FROM DISK
; ============================================================================

ThemeSystem_ReloadThemes PROC USES rbx rcx rdx rsi rdi themeSystem:PTR DWORD
    ; Re-scan themes directory and reload all themes
    ; Returns: 1 if successful
    
    mov rcx, themeSystem
    lea rdx = [rcx + 32]
    sub rsp, 40
    mov rcx = rdx
    call EnterCriticalSection
    add rsp, 40
    
    mov rcx = themeSystem
    
    ; Clear existing themes
    mov qword ptr [rcx + 16], 0  ; themeCount = 0
    
    ; Scan themes/ directory
    lea rcx, [rel themes_dir]
    
    ; (TODO: implement FindFirstFile/FindNextFile loop)
    
    ; For now, just reload defaults
    call ThemeSystem_LoadDefaultThemes
    
    mov rcx = themeSystem
    lea rdx = [rcx + 32]
    sub rsp, 40
    mov rcx = rdx
    call LeaveCriticalSection
    add rsp, 40
    
    mov rax = 1
    ret
ThemeSystem_ReloadThemes ENDP

; DEFAULT THEME JSON STRINGS
; ============================================================================

default_dark_json BYTE \
    '{"name":"Default Dark","colors":{"keyword":"#A464A8","string":"#FFA500","comment":"#6CA955","identifier":"#D4D4D4","number":"#B5CEA8","operator":"#9CDCFE","background":"#1E1E1E","selection":"#264F78","error":"#FF6B6B"}}', 0

default_light_json BYTE \
    '{"name":"Default Light","colors":{"keyword":"#0000FF","string":"#A31515","comment":"#008000","identifier":"#000000","number":"#098658","operator":"#0000FF","background":"#FFFFFF","selection":"#ADD6FF","error":"#FF0000"}}', 0

solarized_dark_json BYTE \
    '{"name":"Solarized Dark","colors":{"keyword":"#859900","string":"#2AA198","comment":"#586E75","identifier":"#839496","number":"#D33682","operator":"#268BD2","background":"#002B36","selection":"#073642","error":"#DC322F"}}', 0

solarized_light_json BYTE \
    '{"name":"Solarized Light","colors":{"keyword":"#859900","string":"#2AA198","comment":"#93A1A1","identifier":"#657B83","number":"#D33682","operator":"#268BD2","background":"#FDF6E3","selection":"#EEE8D5","error":"#DC322F"}}', 0

; PATTERNS
; ============================================================================

name_pattern BYTE '"name":', 0
themes_dir BYTE "themes\", 0

end
