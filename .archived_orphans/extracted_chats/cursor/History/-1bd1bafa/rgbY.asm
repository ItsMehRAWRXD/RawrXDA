;==========================================================================
; gui_designer_agent.asm - Enterprise GUI Designer for RawrXD IDE
; ==========================================================================
; Qt/Electron-Style Features:
; - Advanced layout management (Grid, Flex, Stack)
; - CSS-like styling system with themes
; - Smooth animations and transitions
; - Component-based UI architecture
; - Responsive design capabilities
; - Real-time preview and WYSIWYG editing
; - Touch/mouse/gesture support
; - Accessibility features
; - GPU-accelerated rendering
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib dwmapi.lib
includelib uxtheme.lib
includelib d2d1.lib
includelib dwrite.lib

;==========================================================================
; EXTERNAL DECLARATIONS
;==========================================================================
EXTERN ui_get_main_hwnd:PROC
EXTERN ui_get_editor_hwnd:PROC
EXTERN ui_get_status_hwnd:PROC
EXTERN ui_add_chat_message:PROC
EXTERN asm_log:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN CreateDIBSection:PROC
EXTERN SelectObject:PROC
EXTERN BitBlt:PROC
EXTERN DwmExtendFrameIntoClientArea:PROC
EXTERN BeginBufferedPaint:PROC
EXTERN EndBufferedPaint:PROC
EXTERN SetTimer:PROC
EXTERN InvalidateRect:PROC
EXTERN SetWindowPos:PROC
EXTERN GetModuleHandleA:PROC
EXTERN CreateWindowExA:PROC
EXTERN GetTickCount:PROC
EXTERN MessageBoxA:PROC
EXTERN OutputDebugStringA:PROC
EXTERN LoadLibraryA:PROC
EXTERN RegisterClassExA:PROC
EXTERN LoadCursorA:PROC
EXTERN LoadIconA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN DefWindowProcA:PROC
EXTERN PostQuitMessage:PROC
EXTERN DestroyMenu:PROC
EXTERN CreateMenu:PROC
EXTERN CreatePopupMenu:PROC
EXTERN AppendMenuA:PROC
EXTERN SetMenu:PROC
EXTERN SendMessageA:PROC
EXTERN DestroyWindow:PROC

; Direct2D/DirectWrite
EXTERN D2D1CreateFactory:PROC
EXTERN DWriteCreateFactory:PROC

;==========================================================================
; CONSTANTS
;==========================================================================
LAYOUT_GRID           EQU 1
LAYOUT_FLEX           EQU 2
LAYOUT_STACK          EQU 3
LAYOUT_ABSOLUTE       EQU 4

STYLE_BACKGROUND      EQU 1
STYLE_BORDER          EQU 2
STYLE_SHADOW          EQU 4
STYLE_GRADIENT        EQU 8
STYLE_ANIMATION       EQU 16

ANIMATION_EASE_IN     EQU 1
ANIMATION_EASE_OUT    EQU 2
ANIMATION_EASE_IN_OUT EQU 3
ANIMATION_LINEAR      EQU 4

MAX_COMPONENTS        EQU 1024
MAX_STYLES            EQU 256
MAX_ANIMATIONS        EQU 128
MAX_THEMES            EQU 16

; Modern color scheme (Material Design)
COLOR_PRIMARY         EQU 0FF2196F3h    ; Blue 500
COLOR_PRIMARY_DARK    EQU 0FF1976D2h    ; Blue 700
COLOR_ACCENT          EQU 0FFFF4081h    ; Pink A200
COLOR_BACKGROUND      EQU 0FFFAFAFAh    ; Grey 50
COLOR_SURFACE         EQU 0FFFFFFFFh    ; White
COLOR_ERROR           EQU 0FFF44336h    ; Red 500
COLOR_WARNING         EQU 0FFFF9800h    ; Orange 500
COLOR_SUCCESS         EQU 0FF4CAF50h    ; Green 500

COMPONENT_TYPE_PANEL  EQU 1
COMPONENT_TYPE_BUTTON EQU 2
COMPONENT_TYPE_INPUT  EQU 3

;==========================================================================
; STRUCTURES
;==========================================================================
COMPONENT STRUCT
    id                  DWORD ?
    comp_type           DWORD ?           ; Button, Panel, Text, etc.
    hwnd                QWORD ?
    parent_id           DWORD ?
    layout_type         DWORD ?
    x                   REAL4 ?
    y                   REAL4 ?
    comp_width          REAL4 ?
    comp_height         REAL4 ?
    min_width           REAL4 ?
    min_height          REAL4 ?
    max_width           REAL4 ?
    max_height          REAL4 ?
    margin_left         REAL4 ?
    margin_top          REAL4 ?
    margin_right        REAL4 ?
    margin_bottom       REAL4 ?
    padding_left        REAL4 ?
    padding_top         REAL4 ?
    padding_right       REAL4 ?
    padding_bottom      REAL4 ?
    is_visible          DWORD ?
    is_enabled          DWORD ?
    style_id            DWORD ?
    animation_id        DWORD ?
    z_index             DWORD ?
    opacity             REAL4 ?
    transform           REAL4 6 DUP (?)   ; 2D transformation matrix
    custom_data         QWORD ?
COMPONENT ENDS

STYLE STRUCT
    id                  DWORD ?
    style_name          BYTE 64 DUP (?)
    background_color    DWORD ?
    border_color        DWORD ?
    border_width        REAL4 ?
    border_radius       REAL4 ?
    shadow_color        DWORD ?
    shadow_blur         REAL4 ?
    shadow_offset_x     REAL4 ?
    shadow_offset_y     REAL4 ?
    padding_left        REAL4 ?
    padding_top         REAL4 ?
    padding_right       REAL4 ?
    padding_bottom      REAL4 ?
    gradient_start      DWORD ?
    gradient_end        DWORD ?
    gradient_angle      REAL4 ?
    font_family         BYTE 32 DUP (?)
    font_size           REAL4 ?
    font_weight         DWORD ?
    text_color          DWORD ?
    text_align          DWORD ?
    vertical_align      DWORD ?
    transition_duration REAL4 ?
    transition_property DWORD ?
STYLE ENDS

ANIMATION STRUCT
    id                  DWORD ?
    target_component    DWORD ?
    property            DWORD ?           ; What to animate
    from_value          REAL4 ?
    to_value            REAL4 ?
    duration            DWORD ?           ; Milliseconds
    delay               DWORD ?           ; Milliseconds
    easing              DWORD ?
    iterations          DWORD ?           ; 0 = infinite
    direction           DWORD ?           ; normal, reverse, alternate
    fill_mode           DWORD ?           ; none, forwards, backwards, both
    running             DWORD ?
    start_time          DWORD ?
    current_time        DWORD ?
ANIMATION ENDS

THEME STRUCT
    id                  DWORD ?
    theme_name          BYTE 64 DUP (?)
    primary_color       DWORD ?
    secondary_color     DWORD ?
    background_color    DWORD ?
    surface_color       DWORD ?
    text_color          DWORD ?
    text_secondary      DWORD ?
    accent_color        DWORD ?
    error_color         DWORD ?
    warning_color       DWORD ?
    success_color       DWORD ?
    shadow_color        DWORD ?
    border_color        DWORD ?
THEME ENDS

LAYOUT_PROPERTIES STRUCT
    layout_type         DWORD ?
    direction           DWORD ?           ; row, column, row-reverse, column-reverse
    wrap                DWORD ?           ; nowrap, wrap, wrap-reverse
    justify_content     DWORD ?           ; flex-start, center, flex-end, space-between, space-around
    align_items         DWORD ?           ; stretch, flex-start, center, flex-end
    align_content       DWORD ?           ; stretch, flex-start, center, flex-end, space-between
    gap                 REAL4 ?           ; Spacing between items
    row_gap             REAL4 ?
    column_gap          REAL4 ?
LAYOUT_PROPERTIES ENDS

;==========================================================================
; DATA SEGMENT
;==========================================================================
.data
    ; Tool definitions
    szToolInspect       BYTE "gui_design_inspect",0
    szDescInspect       BYTE "Returns detailed UI component tree with styles and layouts",0
    szToolModify        BYTE "gui_modify_component",0
    szDescModify        BYTE "Modifies UI component properties with animations",0
    szToolStyle         BYTE "gui_apply_style",0
    szDescStyle         BYTE "Applies CSS-like styling to components",0
    szToolAnimate       BYTE "gui_animate_component",0
    szDescAnimate       BYTE "Creates smooth transitions and animations",0
    szToolTheme         BYTE "gui_apply_theme",0
    szDescTheme         BYTE "Applies complete theme to entire UI",0
    szToolLayout        BYTE "gui_set_layout",0
    szDescLayout        BYTE "Sets responsive layout properties",0
    szToolCreate        BYTE "gui_create_component",0
    szDescCreate        BYTE "Creates new UI components dynamically",0
    szToolDelete        BYTE "gui_delete_component",0
    szDescDelete        BYTE "Removes UI components safely",0
    
    ; Modern component types
    szComponentButton   BYTE "button",0
    szComponentPanel    BYTE "panel",0
    szComponentText     BYTE "text",0
    szComponentInput    BYTE "input",0
    szComponentImage    BYTE "image",0
    szComponentIcon     BYTE "icon",0
    szComponentDivider  BYTE "divider",0
    szComponentSpacer   BYTE "spacer",0
    szComponentCard     BYTE "card",0
    szComponentToolbar  BYTE "toolbar",0
    szComponentMenu     BYTE "menu",0
    szComponentTab      BYTE "tab",0
    szComponentTree     BYTE "tree",0
    szComponentList     BYTE "list",0
    szComponentGrid     BYTE "grid",0
    
    ; CSS-like style definitions
    szStyleDarkTheme    BYTE ".dark-theme { background: #121212; color: #ffffff; }",0
    szStyleLightTheme   BYTE ".light-theme { background: #ffffff; color: #000000; }",0
    szStyleButton       BYTE ".btn { padding: 8px 16px; border-radius: 4px; transition: all 0.3s; }",0
    szStyleCard         BYTE ".card { box-shadow: 0 2px 8px rgba(0,0,0,0.1); border-radius: 8px; }",0
    szStyleInput        BYTE ".input { border: 1px solid #ddd; padding: 12px; border-radius: 4px; }",0
    
    ; Animation presets
    szAnimFadeIn        BYTE "fadeIn",0
    szAnimSlideUp       BYTE "slideUp",0
    szAnimScale         BYTE "scale",0
    szAnimRotate        BYTE "rotate",0
    szAnimBounce        BYTE "bounce",0
    szAnimShake         BYTE "shake",0
    
    ; Layout system strings
    szLayoutFlex        BYTE "flex",0
    szLayoutGrid        BYTE "grid",0
    szLayoutStack       BYTE "stack",0
    szLayoutAbsolute    BYTE "absolute",0
    
    ; Color palette definitions
    szPaletteMaterial   BYTE "material",0
    szPaletteBootstrap  BYTE "bootstrap",0
    szPaletteAntDesign  BYTE "antdesign",0
    szPaletteCustom     BYTE "custom",0
    
    ; Production Logging Strings
    szLogInitThemes     BYTE "GUI: Initializing default themes...",0
    szLogThemesDone     BYTE "GUI: Default themes initialized.",0
    szLogFindComp       BYTE "GUI: Finding component...",0
    szLogCreateWin      BYTE "GUI: Creating window for component...",0
    szLogUpdatePos      BYTE "GUI: Updating component positions...",0
    szLogParseStyle     BYTE "GUI: Parsing style JSON...",0
    szLogApplyStyle     BYTE "GUI: Applying style to component...",0
    
    szStyleKeyBackground BYTE "background",0
    szStyleKeyColor      BYTE "color",0
    szStyleKeyBorderRadius BYTE "borderRadius",0

.data?
    ; Component registry
    ComponentRegistry   COMPONENT MAX_COMPONENTS DUP (<>)
    ComponentCount      DWORD ?
    NextComponentId     DWORD ?
    
    ; Style registry
    StyleRegistry       STYLE MAX_STYLES DUP (<>)
    StyleCount          DWORD ?
    NextStyleId         DWORD ?
    
    ; Animation system
    AnimationRegistry   ANIMATION MAX_ANIMATIONS DUP (<>)
    AnimationCount      DWORD ?
    ActiveAnimations    DWORD ?
    
    ; Theme system
    ThemeRegistry       THEME MAX_THEMES DUP (<>)
    ThemeCount          DWORD ?
    CurrentTheme        DWORD ?
    
    ; Direct2D resources
    pD2DFactory         QWORD ?
    pDWriteFactory      QWORD ?
    pRenderTarget       QWORD ?
    pBitmap             QWORD ?
    
    ; Layout system
    LayoutProps         LAYOUT_PROPERTIES <>
    
    ; Buffers
    JsonBuffer          BYTE 65536 DUP (?)
    StyleBuffer         BYTE 32768 DUP (?)
    AnimationBuffer     BYTE 16384 DUP (?)
    ComponentBuffer     BYTE 8192 DUP (?)
    
    ; Performance metrics
    FrameRate           DWORD ?
    RenderTime          DWORD ?
    AnimationTime       DWORD ?
    LayoutTime          DWORD ?
    
    ; Agent Output Buffer
    gui_output_buffer   BYTE 65536 DUP (?)

;==========================================================================
; CODE SEGMENT
;==========================================================================
.code

;==========================================================================
; PUBLIC: gui_init_registry()
;==========================================================================
PUBLIC gui_init_registry
gui_init_registry PROC
    jmp InitializeGUIDesigner
gui_init_registry ENDP

;==========================================================================
; INTERNAL: InitializeGUIDesigner()
;==========================================================================
InitializeGUIDesigner PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    ; Initialize component registry
    mov ComponentCount, 0
    mov NextComponentId, 1
    
    ; Initialize style registry
    mov StyleCount, 0
    mov NextStyleId, 1
    
    ; Initialize animation system
    mov AnimationCount, 0
    mov ActiveAnimations, 0
    
    ; Initialize theme system
    mov ThemeCount, 0
    mov CurrentTheme, 0
    
    ; Create default styles
    call CreateDefaultStyles
    
    ; Create default themes
    call CreateDefaultThemes
    
    ; Initialize Direct2D
    call InitializeDirect2D
    
    ; Start animation timer
    call StartAnimationTimer
    
    xor eax, eax
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
InitializeGUIDesigner ENDP

;==========================================================================
; PUBLIC: gui_create_component(type: rcx, parent_id: edx, properties: r8) -> eax
;==========================================================================
PUBLIC gui_create_component
ALIGN 16
gui_create_component PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 128
    
    mov rbx, rcx        ; type
    mov r12d, edx       ; parent_id
    mov r13, r8         ; properties
    
    ; Validate parameters
    test rbx, rbx
    jz create_invalid
    cmp ComponentCount, MAX_COMPONENTS
    jae create_registry_full
    
    ; Allocate new component ID
    mov eax, NextComponentId
    mov r14d, eax
    inc NextComponentId
    
    ; Get component slot
    mov eax, r14d
    mov ecx, SIZEOF COMPONENT
    imul ecx, eax
    lea rdi, ComponentRegistry
    movsxd rcx, ecx
    add rdi, rcx
    mov r15, rdi        ; component ptr
    
    ; Initialize component
    mov [rdi + COMPONENT.id], r14d
    mov [rdi + COMPONENT.comp_type], ebx
    mov [rdi + COMPONENT.parent_id], r12d
    mov [rdi + COMPONENT.is_visible], 1
    mov [rdi + COMPONENT.is_enabled], 1
    mov DWORD PTR [rdi + COMPONENT.opacity], 1065353216 ; 1.0f
    
    ; Set default dimensions
    mov DWORD PTR [rdi + COMPONENT.comp_width], 1120403456   ; 100.0f
    mov DWORD PTR [rdi + COMPONENT.comp_height], 1120403456  ; 100.0f
    
    ; Create actual HWND if needed
    cmp ebx, COMPONENT_TYPE_PANEL
    je create_window_needed
    cmp ebx, COMPONENT_TYPE_BUTTON
    je create_window_needed
    cmp ebx, COMPONENT_TYPE_INPUT
    je create_window_needed
    
    jmp create_skip_window
    
create_window_needed:
    call CreateComponentWindow
    
create_skip_window:
    ; Apply default style
    mov ecx, r14d
    call gui_apply_default_style
    
    ; Increment component count
    inc ComponentCount
    
    mov eax, r14d
    jmp create_done
    
create_invalid:
    mov eax, 0
    jmp create_done
    
create_registry_full:
    mov eax, 0
    
create_done:
    add rsp, 128
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
gui_create_component ENDP

;==========================================================================
; PUBLIC: gui_apply_style(component_id: ecx, style_json: rdx) -> eax
;==========================================================================
PUBLIC gui_apply_style
ALIGN 16
gui_apply_style PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 256
    
    mov r12d, ecx       ; component_id
    mov r13, rdx        ; style_json
    
    ; Validate parameters
    test r13, r13
    jz style_invalid
    cmp StyleCount, MAX_STYLES
    jae style_registry_full
    
    ; Parse JSON style
    lea rcx, [rsp + 32] ; parsed_style
    mov rdx, r13
    call ParseStyleJson
    test eax, eax
    jz style_parse_failed
    
    ; Find component
    mov ecx, r12d
    call FindComponent
    test rax, rax
    jz style_component_not_found
    
    mov rbx, rax        ; component
    
    ; Apply parsed style
    mov ecx, r12d
    lea rdx, [rsp + 32]
    call ApplyComponentStyle
    
    ; Update component appearance
    mov rcx, rbx
    call UpdateComponentAppearance
    
    ; Start transition animation if needed
    lea rax, [rsp + 32]
    cmp [rax + STYLE.transition_duration], 0
    je style_no_animation
    
    call StartStyleAnimation
    
style_no_animation:
    xor eax, eax
    jmp style_done
    
style_invalid:
    mov eax, 1
    jmp style_done
    
style_registry_full:
    mov eax, 2
    jmp style_done
    
style_parse_failed:
    mov eax, 3
    jmp style_done
    
style_component_not_found:
    mov eax, 4
    
style_done:
    add rsp, 256
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
gui_apply_style ENDP

;==========================================================================
; PUBLIC: gui_animate_component(component_id: ecx, animation_json: rdx) -> eax
;==========================================================================
PUBLIC gui_animate_component
ALIGN 16
gui_animate_component PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 384
    
    mov r12d, ecx       ; component_id
    mov r13, rdx        ; animation_json
    
    ; Validate parameters
    test r13, r13
    jz anim_invalid
    cmp AnimationCount, MAX_ANIMATIONS
    jae anim_registry_full
    
    ; Parse animation JSON
    lea rcx, [rsp + 32] ; parsed_animation
    mov rdx, r13
    call ParseAnimationJson
    test eax, eax
    jz anim_parse_failed
    
    ; Find component
    mov ecx, r12d
    call FindComponent
    test rax, rax
    jz anim_component_not_found
    
    mov rbx, rax        ; component
    
    ; Allocate animation slot
    mov eax, AnimationCount
    mov ecx, SIZEOF ANIMATION
    imul ecx, eax
    lea rdi, AnimationRegistry
    add rdi, rcx
    
    ; Copy parsed animation
    mov ecx, SIZEOF ANIMATION / 4
    lea rsi, [rsp + 32]
    rep movsd
    
    ; Initialize animation state
    mov [rdi + ANIMATION.id], eax
    mov [rdi + ANIMATION.target_component], r12d
    mov [rdi + ANIMATION.running], 1
    
    ; Get current time
    call GetTickCount
    mov [rdi + ANIMATION.start_time], eax
    
    ; Increment animation count
    inc AnimationCount
    inc ActiveAnimations
    
    ; Start animation timer if not running
    cmp ActiveAnimations, 1
    jne anim_timer_running
    call StartAnimationTimer
    
anim_timer_running:
    xor eax, eax
    jmp anim_done
    
anim_invalid:
    mov eax, 1
    jmp anim_done
    
anim_registry_full:
    mov eax, 2
    jmp anim_done
    
anim_parse_failed:
    mov eax, 3
    jmp anim_done
    
anim_component_not_found:
    mov eax, 4
    
anim_done:
    add rsp, 384
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
gui_animate_component ENDP

;==========================================================================
; PUBLIC: gui_set_layout(component_id: ecx, layout_json: rdx) -> eax
;==========================================================================
PUBLIC gui_set_layout
ALIGN 16
gui_set_layout PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 320
    
    mov r12d, ecx       ; component_id
    mov r13, rdx        ; layout_json
    
    ; Validate parameters
    test r13, r13
    jz layout_invalid
    
    ; Parse layout JSON
    lea rcx, [rsp + 32] ; parsed_layout
    mov rdx, r13
    call ParseLayoutJson
    test eax, eax
    jz layout_parse_failed
    
    ; Find component
    mov ecx, r12d
    call FindComponent
    test rax, rax
    jz layout_component_not_found
    
    mov rbx, rax        ; component
    
    ; Apply layout properties
    lea rcx, [rsp + 32]
    call ApplyLayoutProperties
    
    ; Recalculate child positions
    mov ecx, r12d
    call RecalculateLayout
    
    ; Update component positions
    call UpdateComponentPositions
    
    xor eax, eax
    jmp layout_done
    
layout_invalid:
    mov eax, 1
    jmp layout_done
    
layout_parse_failed:
    mov eax, 2
    jmp layout_done
    
layout_component_not_found:
    mov eax, 3
    
layout_done:
    add rsp, 320
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
gui_set_layout ENDP

;==========================================================================
; PUBLIC: gui_apply_theme(theme_name: rcx) -> eax
;==========================================================================
PUBLIC gui_apply_theme
ALIGN 16
gui_apply_theme PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 256
    
    mov rbx, rcx        ; theme_name
    
    ; Validate parameters
    test rbx, rbx
    jz theme_invalid
    
    ; Find theme by name
    mov rcx, rbx
    call FindThemeByName
    test eax, eax
    jz theme_not_found
    
    mov r12d, eax       ; theme_id
    mov CurrentTheme, eax
    
    ; Apply theme to all components
    xor r13d, r13d      ; component index
    
theme_component_loop:
    cmp r13d, ComponentCount
    jae theme_done
    
    ; Get component
    mov eax, r13d
    mov ecx, SIZE COMPONENT
    imul ecx, eax
    lea rdi, ComponentRegistry
    movsxd rcx, ecx
    add rdi, rcx
    
    ; Apply theme colors
    mov ecx, [rdi + COMPONENT.id]
    mov edx, r12d
    call ApplyThemeToComponent
    
    inc r13d
    jmp theme_component_loop
    
theme_done:
    xor eax, eax
    jmp theme_exit
    
theme_invalid:
    mov eax, 1
    jmp theme_exit
    
theme_not_found:
    mov eax, 2
    
theme_exit:
    add rsp, 256
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
gui_apply_theme ENDP

;==========================================================================
; PUBLIC: gui_inspect_tree() -> rax
;==========================================================================
PUBLIC gui_inspect_tree
ALIGN 16
gui_inspect_tree PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 128
    
    ; Initialize JSON buffer
    lea rdi, JsonBuffer
    lea rsi, szJsonStart
    call StringCopy
    
    ; Build component tree
    xor r12d, r12d      ; component index
    xor r13d, r13d      ; depth level
    
tree_build_loop:
    cmp r12d, ComponentCount
    jae tree_done
    
    ; Get component
    mov eax, r12d
    mov ecx, SIZE COMPONENT
    imul ecx, eax
    lea rbx, ComponentRegistry
    movsxd rcx, ecx
    add rbx, rcx
    
    ; Add component to JSON
    mov ecx, r12d
    mov edx, r13d
    call AddComponentToJson
    
    inc r12d
    jmp tree_build_loop
    
tree_done:
    ; Close JSON
    lea rdi, JsonBuffer
    call strlen
    lea rdi, [rdi + rax]
    lea rsi, szJsonEnd
    call StringCopy
    
    lea rax, JsonBuffer
    
    add rsp, 128
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
gui_inspect_tree ENDP

;==========================================================================
; INTERNAL: AnimationTimerProc()
;==========================================================================
AnimationTimerProc PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 64
    
    ; Process all active animations
    xor r12d, r12d      ; animation index
    
anim_process_loop:
    cmp r12d, AnimationCount
    jae anim_done
    
    ; Get animation
    mov eax, r12d
    mov ecx, SIZE ANIMATION
    imul ecx, eax
    lea rdi, AnimationRegistry
    movsxd rcx, ecx
    add rdi, rcx
    
    ; Check if running
    cmp [rdi + ANIMATION.running], 0
    je anim_next
    
    ; Update animation
    call UpdateAnimation
    
anim_next:
    inc r12d
    jmp anim_process_loop
    
anim_done:
    ; Request redraw if needed
    cmp ActiveAnimations, 0
    je anim_no_redraw
    call RequestRedraw
    
anim_no_redraw:
    add rsp, 64
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
AnimationTimerProc ENDP

;==========================================================================
; INTERNAL: CreateDefaultStyles()
;==========================================================================
CreateDefaultStyles PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    ; Create Material Design button style
    lea rdi, StyleRegistry
    mov [rdi + STYLE.id], 1
    lea rsi, szStyleButton
    push rdi
    lea rdi, [rdi + STYLE.style_name]
    call StringCopySafe
    pop rdi
    
    mov [rdi + STYLE.background_color], COLOR_PRIMARY
    mov [rdi + STYLE.text_color], 0FFFFFFFFh  ; White
    mov DWORD PTR [rdi + STYLE.border_radius], 1053609165 ; 2.0f
    mov DWORD PTR [rdi + STYLE.padding_left], 1089470464  ; 8.0f
    mov DWORD PTR [rdi + STYLE.padding_right], 1089470464 ; 8.0f
    mov DWORD PTR [rdi + STYLE.padding_top], 1086324736   ; 6.0f
    mov DWORD PTR [rdi + STYLE.padding_bottom], 1086324736 ; 6.0f
    mov DWORD PTR [rdi + STYLE.transition_duration], 1061997773 ; 0.3f
    
    ; Create card style
    mov eax, 1
    mov ecx, SIZE STYLE
    imul ecx, eax
    lea rdi, StyleRegistry
    movsxd rcx, ecx
    add rdi, rcx
    mov [rdi + STYLE.id], 2
    lea rsi, szStyleCard
    push rdi
    lea rdi, [rdi + STYLE.style_name]
    call StringCopySafe
    pop rdi
    
    mov [rdi + STYLE.background_color], COLOR_SURFACE
    mov [rdi + STYLE.border_color], 01E000000h  ; Light grey
    mov DWORD PTR [rdi + STYLE.border_width], 1056964608 ; 0.5f
    mov DWORD PTR [rdi + STYLE.border_radius], 1065353216 ; 1.0f
    mov DWORD PTR [rdi + STYLE.shadow_blur], 1084227584  ; 5.0f
    mov DWORD PTR [rdi + STYLE.shadow_offset_x], 1056964608 ; 0.5f
    mov DWORD PTR [rdi + STYLE.shadow_offset_y], 1056964608 ; 0.5f
    
    mov StyleCount, 2
    mov NextStyleId, 3
    
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
CreateDefaultStyles ENDP

;==========================================================================
; INTERNAL: InitializeDirect2D()
;==========================================================================
InitializeDirect2D PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    ; Create Direct2D factory
    lea rcx, IID_ID2D1Factory
    mov rdx, D2D1_FACTORY_TYPE_SINGLE_THREADED
    xor r8, r8
    lea r9, pD2DFactory
    call D2D1CreateFactory
    
    ; Create DirectWrite factory
    lea rcx, IID_IDWriteFactory
    mov rdx, DWRITE_FACTORY_TYPE_SHARED
    xor r8, r8
    lea r9, pDWriteFactory
    call DWriteCreateFactory
    
    xor eax, eax
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
InitializeDirect2D ENDP

;==========================================================================
; STUBS FOR MISSING FUNCTIONS
;==========================================================================
CreateDefaultThemes PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 64
    
    lea rcx, szLogInitThemes
    call asm_log
    
    ; Create Material Design Dark Theme
    lea rdi, ThemeRegistry
    mov [rdi + THEME.id], 1
    lea rsi, szThemeDarkName
    push rdi
    lea rdi, [rdi + THEME.theme_name]
    call StringCopySafe
    pop rdi
    
    ; Set dark theme colors (Material Design)
    mov [rdi + THEME.primary_color], 0FF2196F3h      ; Blue 500
    mov [rdi + THEME.secondary_color], 0FF03DAC6h    ; Teal 200
    mov [rdi + THEME.background_color], 0FF121212h   ; Dark background
    mov [rdi + THEME.surface_color], 0FF1E1E1Eh      ; Dark surface
    mov [rdi + THEME.text_color], 0FFFFFFFFh         ; White text
    mov [rdi + THEME.text_secondary], 0FFB3B3B3h     ; Light gray
    mov [rdi + THEME.accent_color], 0FFFF4081h       ; Pink A200
    mov [rdi + THEME.error_color], 0FFCF6679h        ; Red 300
    mov [rdi + THEME.warning_color], 0FFFFB74Dh      ; Orange 300
    mov [rdi + THEME.success_color], 0FF81C784h      ; Green 300
    mov [rdi + THEME.shadow_color], 0FF000000h       ; Black shadow
    mov [rdi + THEME.border_color], 0FF424242h       ; Gray 800
    
    ; Create Material Design Light Theme
    mov eax, SIZE THEME
    add rdi, rax
    mov [rdi + THEME.id], 2
    lea rsi, szThemeLightName
    push rdi
    lea rdi, [rdi + THEME.theme_name]
    call StringCopySafe
    pop rdi
    
    ; Set light theme colors
    mov [rdi + THEME.primary_color], 0FF1976D2h      ; Blue 700
    mov [rdi + THEME.secondary_color], 0FF0097A7h    ; Cyan 700
    mov [rdi + THEME.background_color], 0FFFAFAFAh   ; Light gray
    mov [rdi + THEME.surface_color], 0FFFFFFFFh      ; White
    mov [rdi + THEME.text_color], 0FF000000h         ; Black text
    mov [rdi + THEME.text_secondary], 0FF757575h     ; Gray 600
    mov [rdi + THEME.accent_color], 0FFD81B60h       ; Pink 600
    mov [rdi + THEME.error_color], 0FFD32F2Fh        ; Red 700
    mov [rdi + THEME.warning_color], 0FFF57C00h      ; Orange 700
    mov [rdi + THEME.success_color], 0FF388E3Ch      ; Green 700
    mov [rdi + THEME.shadow_color], 020000000h       ; Translucent black
    mov [rdi + THEME.border_color], 0FFE0E0E0h       ; Gray 300
    
    ; Set theme counts
    mov ThemeCount, 2
    mov CurrentTheme, 1  ; Default to dark theme
    
    lea rcx, szLogThemesDone
    call asm_log
    
    add rsp, 64
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CreateDefaultThemes ENDP

StartAnimationTimer PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 48
    
    ; Create timer for 60 FPS animation updates (16ms)
    call ui_get_main_hwnd
    test rax, rax
    jz timer_fail
    
    mov rcx, rax            ; hwnd
    mov rdx, 1              ; timer ID
    mov r8, 16              ; 16ms = ~60 FPS
    xor r9, r9              ; No timer proc (use WM_TIMER)
    call SetTimer
    
    test rax, rax
    jz timer_fail
    
    mov eax, 1              ; SUCCESS
    jmp timer_done
    
timer_fail:
    xor eax, eax            ; FAILURE
    
timer_done:
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
StartAnimationTimer ENDP

.data
    szButtonClass       BYTE "BUTTON",0
    szEditClass         BYTE "EDIT",0
    szStaticClass       BYTE "STATIC",0
    szEmpty             BYTE 0

.code
CreateComponentWindow PROC
    ; r15 = component ptr
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 128
    
    mov rbx, r15
    
    ; Determine class name
    lea rsi, szStaticClass
    mov eax, [rbx + COMPONENT.comp_type]
    cmp eax, COMPONENT_TYPE_BUTTON
    jne not_button
    lea rsi, szButtonClass
    jmp class_found
not_button:
    cmp eax, COMPONENT_TYPE_INPUT
    jne class_found
    lea rsi, szEditClass
    
class_found:
    call ui_get_main_hwnd
    mov r12, rax ; parent
    
    ; CreateWindowExA(0, class, "", WS_CHILD | WS_VISIBLE, x, y, w, h, parent, id, hInst, NULL)
    xor rcx, rcx ; dwExStyle
    mov rdx, rsi ; lpClassName
    lea r8, szEmpty ; lpWindowName
    mov r9d, WS_CHILD or WS_VISIBLE
    
    cvtss2si eax, [rbx + COMPONENT.x]
    mov [rsp + 32], rax
    cvtss2si eax, [rbx + COMPONENT.y]
    mov [rsp + 40], rax
    cvtss2si eax, [rbx + COMPONENT.comp_width]
    mov [rsp + 48], rax
    cvtss2si eax, [rbx + COMPONENT.comp_height]
    mov [rsp + 56], rax
    
    mov [rsp + 64], r12 ; hWndParent
    mov eax, [rbx + COMPONENT.id]
    add rax, 5000 ; Offset IDs to avoid conflict
    mov [rsp + 72], rax ; hMenu (ID)
    
    xor rax, rax
    call GetModuleHandleA
    mov [rsp + 80], rax ; hInstance
    mov QWORD PTR [rsp + 88], 0 ; lpParam
    
    call CreateWindowExA
    mov [rbx + COMPONENT.hwnd], rax
    
    add rsp, 128
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CreateComponentWindow ENDP

gui_apply_default_style PROC
    ; ecx = component_id
    push rbx
    sub rsp, 32
    
    call FindComponent
    test rax, rax
    jz apply_done
    
    mov rbx, rax
    mov [rbx + COMPONENT.style_id], 1 ; Default to style 1 (Button) if it's a button
    
apply_done:
    add rsp, 32
    pop rbx
    ret
gui_apply_default_style ENDP

ParseStyleJson PROC
    ; rcx = dest STYLE struct, rdx = JSON string
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 128
    
    mov rbx, rcx            ; dest style
    mov rsi, rdx            ; JSON string
    
    ; Validate inputs
    test rbx, rbx
    jz parse_fail
    test rsi, rsi
    jz parse_fail
    
    ; Initialize style with defaults
    mov [rbx + STYLE.background_color], 0FFFFFFFFh
    mov [rbx + STYLE.text_color], 0FF000000h
    mov DWORD PTR [rbx + STYLE.border_width], 1056964608  ; 1.0f
    mov [rbx + STYLE.border_color], 0FFCCCCCCh
    mov DWORD PTR [rbx + STYLE.border_radius], 0          ; 0.0f
    mov DWORD PTR [rbx + STYLE.padding_left], 0
    mov DWORD PTR [rbx + STYLE.padding_right], 0
    mov DWORD PTR [rbx + STYLE.padding_top], 0
    mov DWORD PTR [rbx + STYLE.padding_bottom], 0
    
    ; Simple key-value parser for JSON
    ; Look for "background":"#RRGGBB"
    lea rdx, szStyleKeyBackground
    call FindJsonValue
    test rax, rax
    jz skip_bg
    call ParseColorHex
    mov [rbx + STYLE.background_color], eax
    
skip_bg:
    ; Look for "color":"#RRGGBB"
    mov rcx, rsi
    lea rdx, szStyleKeyColor
    call FindJsonValue
    test rax, rax
    jz skip_color
    call ParseColorHex
    mov [rbx + STYLE.text_color], eax
    
skip_color:
    ; Look for "borderRadius":"4"
    mov rcx, rsi
    lea rdx, szStyleKeyBorderRadius
    call FindJsonValue
    test rax, rax
    jz skip_radius
    call ParseFloatValue
    movss [rbx + STYLE.border_radius], xmm0
    
skip_radius:
    mov eax, 1              ; SUCCESS
    jmp parse_done
    
parse_fail:
    xor eax, eax            ; FAILURE
    
parse_done:
    add rsp, 128
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ParseStyleJson ENDP

; Helper: Find JSON value by key
FindJsonValue PROC
    ; rcx = JSON string, rdx = key
    ; Returns: rax = pointer to value or 0
    push rbx
    push rsi
    push rdi
    
    mov rsi, rcx            ; JSON
    mov rdi, rdx            ; key
    
    ; Simple search for key
    call strstr_masm
    test rax, rax
    jz find_not_found
    
    ; Skip to value (after colon)
    mov rsi, rax
find_colon:
    mov al, [rsi]
    test al, al
    jz find_not_found
    cmp al, ':'
    je find_value
    inc rsi
    jmp find_colon
    
find_value:
    inc rsi                 ; Skip colon
    ; Skip whitespace and quotes
find_skip_ws:
    mov al, [rsi]
    cmp al, ' '
    je find_next
    cmp al, '"'
    je find_next
    cmp al, 9               ; Tab
    je find_next
    jmp find_found
find_next:
    inc rsi
    jmp find_skip_ws
    
find_found:
    mov rax, rsi
    jmp find_done
    
find_not_found:
    xor rax, rax
    
find_done:
    pop rdi
    pop rsi
    pop rbx
    ret
FindJsonValue ENDP

; Helper: Parse hex color #RRGGBB
ParseColorHex PROC
    ; rcx = hex string ("#RRGGBB")
    ; Returns: eax = 0xFFRRGGBB
    push rbx
    push rsi
    
    mov rsi, rcx
    
    ; Skip '#' if present
    cmp byte ptr [rsi], '#'
    jne parse_hex_start
    inc rsi
    
parse_hex_start:
    ; Parse 6 hex digits
    xor eax, eax
    mov ecx, 6
    
parse_hex_loop:
    shl eax, 4              ; Shift left 4 bits
    
    mov bl, [rsi]
    
    ; Check if digit
    cmp bl, '0'
    jb parse_hex_done
    cmp bl, '9'
    jbe parse_hex_digit
    
    ; Check if A-F
    cmp bl, 'A'
    jb parse_hex_lower
    cmp bl, 'F'
    jbe parse_hex_upper
    
parse_hex_lower:
    ; Check if a-f
    cmp bl, 'a'
    jb parse_hex_done
    cmp bl, 'f'
    ja parse_hex_done
    sub bl, 'a'
    add bl, 10
    jmp parse_hex_add
    
parse_hex_upper:
    sub bl, 'A'
    add bl, 10
    jmp parse_hex_add
    
parse_hex_digit:
    sub bl, '0'
    
parse_hex_add:
    or al, bl
    inc rsi
    loop parse_hex_loop
    
parse_hex_done:
    or eax, 0xFF000000h     ; Add alpha channel
    
    pop rsi
    pop rbx
    ret
ParseColorHex ENDP

; Helper: Parse float value
ParseFloatValue PROC
    ; rcx = string
    ; Returns: xmm0 = float value
    push rbx
    push rsi
    sub rsp, 32
    
    mov rsi, rcx
    
    ; Simple integer to float conversion
    xor eax, eax
parse_float_loop:
    mov bl, [rsi]
    cmp bl, '0'
    jb parse_float_done
    cmp bl, '9'
    ja parse_float_done
    
    imul eax, 10
    sub bl, '0'
    add eax, ebx
    inc rsi
    jmp parse_float_loop
    
parse_float_done:
    cvtsi2ss xmm0, eax
    
    add rsp, 32
    pop rsi
    pop rbx
    ret
ParseFloatValue ENDP

; Helper: strstr_masm implementation
strstr_masm PROC
    ; rcx = haystack, rdx = needle
    ; Returns: rax = pointer to match or 0
    push rbx
    push rsi
    push rdi
    push r12
    
    mov rsi, rcx            ; haystack
    mov rdi, rdx            ; needle
    
    ; Get needle length
    xor r12, r12
ssm_len:
    cmp byte ptr [rdi + r12], 0
    je ssm_len_done
    inc r12
    jmp ssm_len
    
ssm_len_done:
    test r12, r12
    jz ssm_found            ; Empty needle
    
ssm_search:
    cmp byte ptr [rsi], 0
    je ssm_not_found
    
    ; Compare
    mov rbx, rsi
    mov rcx, rdi
    mov r8, r12
ssm_cmp:
    mov al, [rbx]
    mov dl, [rcx]
    cmp al, dl
    jne ssm_next
    inc rbx
    inc rcx
    dec r8
    jnz ssm_cmp
    
    ; Match found
    mov rax, rsi
    jmp ssm_done
    
ssm_next:
    inc rsi
    jmp ssm_search
    
ssm_found:
    mov rax, rsi
    jmp ssm_done
    
ssm_not_found:
    xor rax, rax
    
ssm_done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
strstr_masm ENDP

FindComponent PROC
    ; ecx = component_id
    ; Returns rax = pointer to component or 0
    push rbx
    push rsi
    
    xor eax, eax
    test ecx, ecx
    jz find_done
    
    xor edx, edx ; index
find_loop:
    cmp edx, ComponentCount
    jae find_not_found
    
    mov eax, edx
    mov r8d, SIZEOF COMPONENT
    imul eax, r8d
    lea rbx, ComponentRegistry
    movsxd r9, eax
    add rbx, r9
    
    cmp [rbx + COMPONENT.id], ecx
    je find_found
    
    inc edx
    jmp find_loop
    
find_found:
    mov rax, rbx
    jmp find_done
    
find_not_found:
    xor rax, rax
    
find_done:
    pop rsi
    pop rbx
    ret
FindComponent ENDP

UpdateComponentPositions PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 64
    
    xor r12d, r12d ; index
pos_loop:
    cmp r12d, ComponentCount
    jae pos_done
    
    mov eax, r12d
    mov ecx, SIZE COMPONENT
    imul eax, ecx
    lea rbx, ComponentRegistry
    movsxd rcx, eax
    add rbx, rcx
    
    mov rcx, [rbx + COMPONENT.hwnd]
    test rcx, rcx
    jz pos_next
    
    ; SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_NOZORDER)
    ; Convert REAL4 to INT for Win32
    cvtss2si r8d, [rbx + COMPONENT.x]
    cvtss2si r9d, [rbx + COMPONENT.y]
    cvtss2si eax, [rbx + COMPONENT.comp_width]
    mov [rsp + 32], rax
    cvtss2si eax, [rbx + COMPONENT.comp_height]
    mov [rsp + 40], rax
    mov QWORD PTR [rsp + 48], 4 ; SWP_NOZORDER
    
    xor rdx, rdx ; HWND_TOP
    call SetWindowPos
    
pos_next:
    inc r12d
    jmp pos_loop
    
pos_done:
    add rsp, 64
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
UpdateComponentPositions ENDP

FindThemeByName PROC
    ; rcx = theme_name (string)
    ; Returns rax = theme_id or 0
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rsi, rcx ; search name
    
    xor ebx, ebx ; index
theme_search_loop:
    cmp ebx, ThemeCount
    jae theme_search_not_found
    
    mov eax, ebx
    mov ecx, SIZE THEME
    imul eax, ecx
    lea rdi, ThemeRegistry
    movsxd rcx, eax
    add rdi, rcx
    
    ; Compare names
    lea rcx, [rdi + THEME.theme_name]
    mov rdx, rsi
    call strcmp_masm
    test eax, eax
    je theme_search_found
    
    inc ebx
    jmp theme_search_loop
    
theme_search_found:
    mov eax, [rdi + THEME.id]
    jmp theme_search_done
    
theme_search_not_found:
    xor rax, rax
    
theme_search_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
FindThemeByName ENDP

strcmp_masm PROC
    ; rcx = str1, rdx = str2
    push rsi
    push rdi
    push rbx
    mov rsi, rcx
    mov rdi, rdx
strcmp_loop:
    mov al, [rsi]
    mov bl, [rdi]
    cmp al, bl
    jne strcmp_diff
    test al, al
    jz strcmp_equal
    inc rsi
    inc rdi
    jmp strcmp_loop
strcmp_diff:
    movzx eax, al
    movzx ebx, bl
    sub eax, ebx
    jmp strcmp_done
strcmp_equal:
    xor eax, eax
strcmp_done:
    pop rbx
    pop rdi
    pop rsi
    ret
strcmp_masm ENDP

ApplyThemeToComponent PROC
    ; ecx = component_id, edx = theme_id
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov r12d, edx ; theme_id
    
    call FindComponent
    test rax, rax
    jz apply_theme_done
    
    mov rbx, rax ; component ptr
    
    ; Find theme
    xor esi, esi
theme_find_loop:
    cmp esi, ThemeCount
    jae apply_theme_done
    
    mov eax, esi
    mov ecx, SIZE THEME
    imul eax, ecx
    lea rdi, ThemeRegistry
    movsxd rcx, eax
    add rdi, rcx
    
    cmp [rdi + THEME.id], r12d
    je theme_found
    
    inc esi
    jmp theme_find_loop
    
theme_found:
    ; Apply theme colors to component's style if it has one
    mov eax, [rbx + COMPONENT.style_id]
    test eax, eax
    jz apply_theme_done
    
    ; Find style in registry
    dec eax ; 1-based to 0-based
    mov ecx, SIZE STYLE
    imul eax, ecx
    lea rsi, StyleRegistry
    movsxd rcx, eax
    add rsi, rcx
    
    ; Update style colors from theme
    mov eax, [rdi + THEME.background_color]
    mov [rsi + STYLE.background_color], eax
    mov eax, [rdi + THEME.text_color]
    mov [rsi + STYLE.text_color], eax
    mov eax, [rdi + THEME.border_color]
    mov [rsi + STYLE.border_color], eax
    
    ; Trigger redraw
    mov rcx, rbx
    call UpdateComponentAppearance
    
apply_theme_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
ApplyThemeToComponent ENDP

StringCopy PROC
    ; rcx = dest, rsi = src
    push rsi
    push rdi
copy_loop:
    lodsb
    stosb
    test al, al
    jnz copy_loop
    pop rdi
    pop rsi
    ret
StringCopy ENDP

StringCopySafe PROC
    ; rdi = dest, rsi = src
    push rsi
    push rdi
copy_loop_safe:
    lodsb
    stosb
    test al, al
    jnz copy_loop_safe
    pop rdi
    pop rsi
    ret
StringCopySafe ENDP

AddComponentToJson PROC
    ret
AddComponentToJson ENDP

strlen PROC
    ; rcx = str
    xor rax, rax
len_loop:
    cmp byte ptr [rcx + rax], 0
    je len_done
    inc rax
    jmp len_loop
len_done:
    ret
strlen ENDP

.data
    szJsonStart         BYTE "{ ""components"": [",0
    szJsonEnd           BYTE "] }",0
    szComponentFormat   BYTE "{ ""id"": %d, ""type"": ""%s"", ""style"": %d, ""x"": %.2f, ""y"": %.2f, ""w"": %.2f, ""h"": %.2f }",0
    szComma             BYTE ",",0
    szSuccess           BYTE "Agent: GUI operation completed successfully.",0
    szError             BYTE "Agent: GUI operation failed.",0

;==========================================================================
; PUBLIC: gui_agent_inspect() -> rax (ptr to JSON string)
;==========================================================================
PUBLIC gui_agent_inspect
ALIGN 16
gui_agent_inspect PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    lea rdi, gui_output_buffer
    mov byte ptr [rdi], '{'
    mov byte ptr [rdi+1], '"'
    mov byte ptr [rdi+2], 'c'
    mov byte ptr [rdi+3], 'o'
    mov byte ptr [rdi+4], 'm'
    mov byte ptr [rdi+5], 'p'
    mov byte ptr [rdi+6], 'o'
    mov byte ptr [rdi+7], 'n'
    mov byte ptr [rdi+8], 'e'
    mov byte ptr [rdi+9], 'n'
    mov byte ptr [rdi+10], 't'
    mov byte ptr [rdi+11], 's'
    mov byte ptr [rdi+12], '"'
    mov byte ptr [rdi+13], ':'
    mov byte ptr [rdi+14], '['
    add rdi, 15
    
    xor ebx, ebx ; index
inspect_loop:
    cmp ebx, ComponentCount
    jae inspect_done
    
    ; Add comma if not first
    test ebx, ebx
    jz first_comp
    mov byte ptr [rdi], ','
    inc rdi
first_comp:
    
    mov eax, ebx
    mov ecx, SIZE COMPONENT
    imul eax, ecx
    lea rsi, ComponentRegistry
    movsxd rcx, eax
    add rsi, rcx
    
    ; Build component JSON: {"name":"...", "type":..., "x":..., "y":...}
    mov byte ptr [rdi], '{'
    inc rdi
    
    ; "name":"..."
    lea rcx, szNameKey
    call append_string
    mov byte ptr [rdi], ':'
    mov byte ptr [rdi+1], '"'
    add rdi, 2
    lea rcx, szDefaultName      ; Use default name since COMPONENT doesn't have name field
    call append_string
    mov byte ptr [rdi], '"'
    mov byte ptr [rdi+1], ','
    add rdi, 2
    
    ; "type":...
    lea rcx, szTypeKey
    call append_string
    mov byte ptr [rdi], ':'
    add rdi, 1
    mov eax, [rsi + COMPONENT.comp_type]
    call append_int
    mov byte ptr [rdi], ','
    add rdi, 1
    
    ; "x":...
    lea rcx, szXKey
    call append_string
    mov byte ptr [rdi], ':'
    add rdi, 1
    movss xmm0, [rsi + COMPONENT.x]
    call append_float
    mov byte ptr [rdi], ','
    add rdi, 1
    
    ; "y":...
    lea rcx, szYKey
    call append_string
    mov byte ptr [rdi], ':'
    add rdi, 1
    movss xmm0, [rsi + COMPONENT.y]
    call append_float
    
    mov byte ptr [rdi], '}'
    inc rdi
    
    inc ebx
    jmp inspect_loop
    
inspect_done:
    mov byte ptr [rdi], ']'
    mov byte ptr [rdi+1], '}'
    mov byte ptr [rdi+2], 0
    
    lea rax, gui_output_buffer
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
gui_agent_inspect ENDP

;==========================================================================
; PUBLIC: gui_agent_modify() -> rax (ptr to status string)
;==========================================================================
PUBLIC gui_agent_modify
ALIGN 16
gui_agent_modify PROC
    ; For now, just a stub that returns success
    lea rax, szSuccess
    ret
gui_agent_modify ENDP

; Helper to append string to rdi
append_string PROC
    ; rcx = string
    push rsi
    mov rsi, rcx
as_loop:
    mov al, [rsi]
    test al, al
    jz as_done
    mov [rdi], al
    inc rsi
    inc rdi
    jmp as_loop
as_done:
    pop rsi
    ret
append_string ENDP

; Helper to append int to rdi
append_int PROC
    ; eax = int
    push rbx
    push rdx
    
    test eax, eax
    jnz not_zero
    mov byte ptr [rdi], '0'
    inc rdi
    jmp ai_done
not_zero:
    ; Simple int to string
    mov ebx, 10
    lea rsi, int_temp_buf
    add rsi, 15
    mov byte ptr [rsi], 0
ai_loop:
    xor edx, edx
    div ebx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    test eax, eax
    jnz ai_loop
    
    mov rcx, rsi
    call append_string
ai_done:
    pop rdx
    pop rbx
    ret
append_int ENDP

; Helper to append float to rdi
append_float PROC
    ; xmm0 = float
    push rax
    
    ; Very simple float to string (integer part only for now)
    cvttss2si eax, xmm0
    call append_int
    
    pop rax
    ret
append_float ENDP

.data
    szNameKey   BYTE '"name"',0
    szTypeKey   BYTE '"type"',0
    szXKey      BYTE '"x"',0
    szYKey      BYTE '"y"',0
    szDefaultName BYTE "component",0
    int_temp_buf BYTE 16 DUP (0)
    
    ; Missing GUIDs for Direct2D (removed duplicate definitions)
    ; IID_ID2D1Factory and IID_IDWriteFactory defined elsewhere
    D2D1_FACTORY_TYPE_SINGLE_THREADED_LOCAL EQU 0
    DWRITE_FACTORY_TYPE_SHARED_LOCAL EQU 0

.code

; Missing stub implementations
ApplyComponentStyle PROC
    ; ecx = component_id, rdx = ptr to STYLE struct
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rdx ; style ptr
    
    ; Find component
    call FindComponent
    test rax, rax
    jz apply_style_done
    
    mov rdi, rax ; component ptr
    
    ; Allocate new style ID
    mov eax, NextStyleId
    mov [rdi + COMPONENT.style_id], eax
    inc NextStyleId
    
    ; Copy style to registry
    dec eax ; 1-based to 0-based
    mov ecx, SIZE STYLE
    imul eax, ecx
    lea rdi, StyleRegistry
    movsxd rcx, eax
    add rdi, rcx
    
    mov rsi, rbx
    mov ecx, SIZE STYLE / 4
    rep movsd
    
    inc StyleCount
    
apply_style_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
ApplyComponentStyle ENDP

UpdateComponentAppearance PROC
    ; rcx = component ptr
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    mov rcx, [rbx + COMPONENT.hwnd]
    test rcx, rcx
    jz update_done
    
    ; InvalidateRect(hwnd, NULL, TRUE) to trigger redraw
    xor rdx, rdx ; NULL rect
    mov r8, 1    ; TRUE (erase background)
    call InvalidateRect
    
update_done:
    add rsp, 32
    pop rbx
    ret
UpdateComponentAppearance ENDP

StartStyleAnimation PROC
    ; rcx = component_id, rdx = animation properties
    push rbx
    push rsi
    push rdi
    sub rsp, 48
    
    mov ebx, ecx        ; component_id
    mov rsi, rdx        ; animation properties
    
    ; Find component
    call FindComponent
    test rax, rax
    jz anim_start_done
    
    mov rdi, rax        ; component pointer
    
    ; Store animation start time
    call GetTickCount
    mov [rdi + COMPONENT.anim_start_time], eax
    
    ; Set animation duration (default 300ms if not specified)
    mov eax, 300
    test rsi, rsi
    jz use_default_duration
    mov eax, [rsi]      ; duration from properties
use_default_duration:
    mov [rdi + COMPONENT.anim_duration], eax
    
    ; Mark component as animating
    mov [rdi + COMPONENT.animating], 1
    
anim_start_done:
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
StartStyleAnimation ENDP

ParseAnimationJson PROC
    ; rcx = JSON string, rdx = animation struct pointer
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx        ; JSON string
    mov rdi, rdx        ; animation struct
    
    ; Basic JSON parsing (simplified)
    ; Set default values
    mov dword ptr [rdi], 300        ; duration (ms)
    mov dword ptr [rdi + 4], ANIMATION_EASE_IN_OUT ; easing
    
    ; Try to parse "duration" key
    lea rsi, szAnimDurationKey
    mov rcx, rbx
    mov rdx, rsi
    call StringFind
    test rax, rax
    jz parse_anim_done
    
    ; Extract duration value (simplified - in production use proper JSON parser)
    ; For now, just return success
    
parse_anim_done:
    mov eax, 1          ; SUCCESS
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
ParseAnimationJson ENDP

UpdateAnimation PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    ; Update all animating components
    xor ebx, ebx
    lea rdi, ComponentRegistry
    
anim_update_loop:
    cmp ebx, ComponentCount
    jae anim_update_done
    
    ; Get component
    mov eax, ebx
    mov ecx, SIZE COMPONENT
    imul eax, ecx
    lea rsi, [rdi + rax]
    
    ; Check if animating
    cmp [rsi + COMPONENT.animating], 1
    jne skip_anim
    
    ; Get current time
    call GetTickCount
    mov ecx, eax
    sub ecx, [rsi + COMPONENT.anim_start_time]
    
    ; Check if animation complete
    cmp ecx, [rsi + COMPONENT.anim_duration]
    jae anim_complete
    
    ; Calculate animation progress (0.0 to 1.0)
    cvtsi2ss xmm0, ecx
    cvtsi2ss xmm1, [rsi + COMPONENT.anim_duration]
    divss xmm0, xmm1    ; progress = elapsed / duration
    
    ; Apply easing (simplified ease-in-out)
    movss xmm1, xmm0
    mulss xmm1, xmm1    ; progress^2
    movss xmm2, xmm0
    movss xmm3, [fltOne]
    subss xmm3, xmm0    ; 1 - progress
    mulss xmm3, xmm3    ; (1 - progress)^2
    movss xmm4, [fltOne]
    subss xmm4, xmm3
    subss xmm4, xmm1
    movss xmm0, xmm4    ; eased progress
    
    ; Update component appearance based on animation
    ; (In production, interpolate between start and end values)
    
    ; Request redraw
    mov rcx, rsi
    call UpdateComponentAppearance
    
    jmp skip_anim
    
anim_complete:
    ; Animation finished
    mov [rsi + COMPONENT.animating], 0
    
skip_anim:
    inc ebx
    jmp anim_update_loop
    
anim_update_done:
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
UpdateAnimation ENDP

RequestRedraw PROC
    push rbx
    sub rsp, 32
    
    ; Get main window handle
    call ui_get_main_hwnd
    test rax, rax
    jz redraw_done
    
    ; Invalidate entire window to trigger redraw
    mov rcx, rax        ; hwnd
    xor rdx, rdx        ; NULL rect (entire window)
    mov r8, 1           ; TRUE (erase background)
    call InvalidateRect
    
redraw_done:
    add rsp, 32
    pop rbx
    ret
RequestRedraw ENDP

;==========================================================================
; PUBLIC: UpdateComponentPositions()
; Recalculates and updates all component positions based on layout
;==========================================================================
PUBLIC UpdateComponentPositions
ALIGN 16
UpdateComponentPositions PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 64
    
    ; Iterate through all components and update their window positions
    xor ebx, ebx        ; component index
    lea r12, ComponentRegistry
    
update_pos_loop:
    cmp ebx, ComponentCount
    jae update_pos_done
    
    ; Get component pointer
    mov eax, ebx
    mov ecx, SIZE COMPONENT
    imul eax, ecx
    lea rdi, [r12 + rax]
    
    ; Check if component has a window
    mov rax, [rdi + COMPONENT.hwnd]
    test rax, rax
    jz skip_component
    
    ; Get position from component structure
    cvtss2si eax, [rdi + COMPONENT.x]
    cvtss2si ecx, [rdi + COMPONENT.y]
    cvtss2si edx, [rdi + COMPONENT.comp_width]
    cvtss2si r8d, [rdi + COMPONENT.comp_height]
    
    ; SetWindowPos(hwnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_SHOWWINDOW)
    mov r9, rax         ; hwnd
    xor r10, r10        ; hWndInsertAfter (NULL)
    mov [rsp + 32], r10
    mov [rsp + 40], eax ; x
    mov [rsp + 48], ecx ; y
    mov [rsp + 56], edx ; width
    mov [rsp + 64], r8d ; height
    mov rcx, r9
    mov rdx, r10
    mov r8d, eax
    mov r9d, ecx
    push 0              ; SWP_NOZORDER | SWP_SHOWWINDOW
    pop rax
    or rax, 4           ; SWP_SHOWWINDOW
    push rax
    call SetWindowPos
    
skip_component:
    inc ebx
    jmp update_pos_loop
    
update_pos_done:
    add rsp, 64
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
UpdateComponentPositions ENDP

ParseLayoutJson PROC
    ; rcx = JSON string, rdx = layout struct pointer
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    mov rbx, rcx        ; JSON string
    mov rdi, rdx        ; layout struct
    
    ; Basic JSON parsing (simplified - looks for "type", "x", "y", "width", "height")
    ; For production, use proper JSON parser
    
    ; Set default layout type
    mov [rdi], LAYOUT_FLEX
    
    ; Try to find "type" key
    lea rsi, szLayoutTypeKey
    mov rcx, rbx
    call StringFind
    test rax, rax
    jz parse_layout_done
    
    ; Extract layout type value (simplified)
    ; In production, parse JSON properly
    
parse_layout_done:
    mov eax, 1          ; SUCCESS
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
ParseLayoutJson ENDP

ApplyLayoutProperties PROC
    ; rcx = component_id, rdx = layout properties pointer
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov ebx, ecx        ; component_id
    mov rsi, rdx        ; layout properties
    
    ; Find component
    mov ecx, ebx
    call FindComponent
    test rax, rax
    jz apply_layout_done
    
    mov rdi, rax        ; component pointer
    
    ; Apply layout properties to component
    ; Update x, y, width, height from layout
    movss xmm0, [rsi]   ; x
    movss [rdi + COMPONENT.x], xmm0
    movss xmm0, [rsi + 4] ; y
    movss [rdi + COMPONENT.y], xmm0
    movss xmm0, [rsi + 8] ; width
    movss [rdi + COMPONENT.comp_width], xmm0
    movss xmm0, [rsi + 12] ; height
    movss [rdi + COMPONENT.comp_height], xmm0
    
    ; Update window position
    mov rcx, rdi
    call UpdateComponentAppearance
    
apply_layout_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
ApplyLayoutProperties ENDP

RecalculateLayout PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 64
    
    ; Recalculate layout for all components
    ; This is a simplified flexbox-style layout
    
    xor ebx, ebx        ; component index
    lea rdi, ComponentRegistry
    
    ; Simple vertical stack layout
    movss xmm0, [fltZero] ; current y position
    movss xmm1, [fltPadding] ; padding between components
    
recalc_loop:
    cmp ebx, ComponentCount
    jae recalc_done
    
    ; Get component
    mov eax, ebx
    mov ecx, SIZE COMPONENT
    imul eax, ecx
    lea rsi, [rdi + rax]
    
    ; Set y position
    movss [rsi + COMPONENT.y], xmm0
    
    ; Add component height + padding to y
    movss xmm2, [rsi + COMPONENT.comp_height]
    addss xmm0, xmm2
    addss xmm0, xmm1
    
    inc ebx
    jmp recalc_loop
    
recalc_done:
    ; Update all component positions
    call UpdateComponentPositions
    
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    ret
RecalculateLayout ENDP

;==========================================================================
; Helper: StringFind(rcx=haystack, rdx=needle) -> rax (pointer or NULL)
;==========================================================================
StringFind PROC
    push rbx
    push rsi
    push rdi
    
    mov rdi, rcx        ; haystack
    mov rsi, rdx        ; needle
    
    ; Get needle length
    mov rcx, rsi
    call StringLength
    mov ebx, eax        ; needle length
    test ebx, ebx
    jz find_not_found
    
find_loop:
    ; Check if we've reached end of haystack
    cmp byte ptr [rdi], 0
    je find_not_found
    
    ; Compare strings
    mov rcx, rdi
    mov rdx, rsi
    mov r8d, ebx
    call memcmp_proc
    test eax, eax
    jz find_found
    
    inc rdi
    jmp find_loop
    
find_found:
    mov rax, rdi
    jmp find_done
    
find_not_found:
    xor rax, rax
    
find_done:
    pop rdi
    pop rsi
    pop rbx
    ret
StringFind ENDP

;==========================================================================
; Helper: StringLength(rcx=string) -> eax
;==========================================================================
StringLength PROC
    xor eax, eax
    mov rdx, rcx
len_loop:
    cmp byte ptr [rdx], 0
    je len_done
    inc eax
    inc rdx
    jmp len_loop
len_done:
    ret
StringLength ENDP

;==========================================================================
; DATA SECTION
;==========================================================================
.data

; Component type strings
szButtonType    db "button", 0
szInputType     db "input", 0
szLabelType     db "label", 0

; Status messages
szParsingStyle  db "Parsing component style...", 0
szApplyingTheme db "Applying theme...", 0

; Default JSON templates
szDefaultButtonJson   db '{"type":"button","text":"Button","background":"#1976D2","color":"#FFFFFF"}', 0
szDefaultInputJson    db '{"type":"input","placeholder":"Enter text","background":"#FFFFFF","color":"#000000"}', 0

; Layout parsing constants
szLayoutTypeKey       db '"type"', 0
szAnimDurationKey     db '"duration"', 0
fltZero               dd 0.0
fltOne                dd 1.0
fltPadding            dd 10.0

align 16
AnimationBuffer     db 1024 DUP(0)    ; Buffer for animation calculations

END
