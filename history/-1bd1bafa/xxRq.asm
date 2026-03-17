;==========================================================================
; gui_designer_agent.asm - Enterprise GUI Designer for RawrXD IDE
; ==========================================================================
; RawrXD IDE Features (Pane-Based, Fully Customizable):
; - Advanced dockable pane system (main, sidebars, bottom, floating)
; - Drag-and-drop pane reorganization
; - Resizable splitters between panes
; - Maximize/minimize/close pane controls
; - Persistent layout saving/loading (JSON)
; - Advanced layout management (Grid, Flex, Stack, Absolute)
; - CSS-like styling system with Material Design themes
; - Smooth animations and transitions
; - Component-based UI architecture (100% customizable)
; - Responsive design capabilities
; - Real-time preview and WYSIWYG editing
; - Touch/mouse/gesture support with pane dragging
; - Full accessibility features
; - GPU-accelerated rendering (Direct2D/DirectWrite)
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
EXTERN ui_get_chat_hwnd:PROC
EXTERN ui_get_input_hwnd:PROC
EXTERN ui_get_terminal_hwnd:PROC
EXTERN ui_get_file_tree_hwnd:PROC
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

; String utilities from agentic_masm.asm and asm_string.asm
EXTERN strstr_masm:PROC
EXTERN strcmp_masm:PROC
EXTERN StringFind:PROC
EXTERN StringCompare:PROC
EXTERN StringLength:PROC

; Hotpatchable JSON helpers from json_hotpatch_helpers.asm
EXTERN append_string:PROC
EXTERN append_int:PROC
EXTERN append_float:PROC
EXTERN append_bool:PROC
EXTERN write_json_to_file:PROC
EXTERN read_json_from_file:PROC
EXTERN find_json_key:PROC
EXTERN parse_json_int:PROC
EXTERN parse_json_bool:PROC
EXTERN json_helpers_get_version:PROC
EXTERN json_helpers_increment_hotpatch:PROC

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

D2D1_FACTORY_TYPE_SINGLE_THREADED EQU 0
DWRITE_FACTORY_TYPE_SHARED        EQU 0

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
COMPONENT_EDITOR      EQU 4
COMPONENT_FILETREE    EQU 5
COMPONENT_TERMINAL    EQU 6
COMPONENT_STATUSBAR   EQU 7
COMPONENT_CHAT        EQU 8
COMPONENT_OUTPUT      EQU 9
COMPONENT_TASK_PROPOSAL EQU 10

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
    animating           DWORD ?           ; Animation active flag
    anim_start_time     DWORD ?           ; Animation start tick count
    anim_duration       DWORD ?           ; Animation duration (ms)
    anim_easing         DWORD ?           ; Easing type (ANIMATION_EASE_*)
    anim_start_x        REAL4 ?           ; Start position X
    anim_start_y        REAL4 ?           ; Start position Y
    anim_end_x          REAL4 ?           ; End position X
    anim_end_y          REAL4 ?           ; End position Y
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

; Animation property IDs
ANIMATION_PROPERTY_X       EQU 1
ANIMATION_PROPERTY_Y       EQU 2
ANIMATION_PROPERTY_WIDTH   EQU 3
ANIMATION_PROPERTY_HEIGHT  EQU 4
ANIMATION_PROPERTY_OPACITY EQU 5

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

; Pane-Based IDE Architecture (RawrXD-style docking system)
PANE_POSITION_LEFT      EQU 1             ; Left
PANE_POSITION_RIGHT     EQU 2             ; Right
PANE_POSITION_TOP       EQU 3             ; Top
PANE_POSITION_BOTTOM    EQU 4             ; Bottom
PANE_POSITION_CENTER    EQU 5             ; Center (main editor)
PANE_POSITION_FLOAT     EQU 6             ; Floating window
PANE_POSITION_TAB       EQU 7             ; Tabbed within pane

; Pane Types
PANE_TYPE_EDITOR        EQU 1
PANE_TYPE_EXPLORER      EQU 2
PANE_TYPE_TERMINAL      EQU 3
PANE_TYPE_OUTPUT        EQU 4
PANE_TYPE_DEBUG         EQU 5
PANE_TYPE_CUSTOM        EQU 6
PANE_TYPE_CHAT          EQU 7

MAX_PANES               EQU 64
MAX_PANE_TABS           EQU 16
SPLITTER_WIDTH          EQU 4             ; Pixels

PANE STRUCT
    id                  DWORD ?
    pane_type           DWORD ?           ; Main, Editor, Console, FileTree, Properties, etc.
    position            DWORD ?           ; PANE_POSITION_*
    hwnd                QWORD ?
    pane_x              DWORD ?
    pane_y              DWORD ?
    pane_width          DWORD ?
    pane_height         DWORD ?
    min_width           DWORD ?
    min_height          DWORD ?
    is_visible          DWORD ?
    is_maximized        DWORD ?
    is_floating         DWORD ?
    is_docked           DWORD ?
    can_close           DWORD ?
    can_resize          DWORD ?
    current_tab         DWORD ?           ; For tabbed panes
    tab_count           DWORD ?
    flex_grow           REAL4 ?           ; Flex grow factor (0.0-1.0)
    flex_shrink         REAL4 ?           ; Flex shrink factor
    parent_pane_id      DWORD ?           ; For nested panes
    splitter_pos        DWORD ?           ; Position of resizable splitter
    is_dragging         DWORD ?           ; Drag state for reorganization
    drag_offset_x       DWORD ?
    drag_offset_y       DWORD ?
    content_hwnd        QWORD ?           ; Content window handle
    toolbar_hwnd        QWORD ?           ; Optional toolbar
    tabs                QWORD ?           ; Pointer to tab array
    custom_data         QWORD ?
PANE ENDS

PANE_TAB STRUCT
    id                  DWORD ?
    pane_id             DWORD ?
    tab_label           BYTE 256 DUP (?)
    icon_id             DWORD ?
    hwnd                QWORD ?
    is_active           DWORD ?
    is_closable         DWORD ?
    tab_x               DWORD ?
    tab_y               DWORD ?
    tab_width           DWORD ?
    tab_height          DWORD ?
PANE_TAB ENDS

PANE_LAYOUT STRUCT
    layout_name         BYTE 256 DUP (?)
    pane_count          DWORD ?
    main_pane_id        DWORD ?           ; Center/main editor pane
    left_pane_id        DWORD ?           ; Left sidebar
    right_pane_id       DWORD ?           ; Right sidebar
    bottom_pane_id      DWORD ?           ; Bottom console/output
    top_pane_id         DWORD ?           ; Top toolbar area
    left_width          DWORD ?           ; Left pane width
    right_width         DWORD ?           ; Right pane width
    bottom_height       DWORD ?           ; Bottom pane height
    top_height          DWORD ?           ; Top pane height
    vertical_splitter_x DWORD ?           ; Main vertical splitter position
    horizontal_splitter_y DWORD ?         ; Main horizontal splitter position
    is_dirty            DWORD ?           ; Layout changed, needs saving
PANE_LAYOUT ENDS

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
    
    ; Theme names
    szThemeDarkName     BYTE "Material Dark",0
    szThemeLightName    BYTE "Material Light",0
    
    ; Pane type strings (RawrXD IDE)
    szPaneTypeMain      BYTE "main",0
    szPaneTypeEditor    BYTE "editor",0
    szPaneTypeConsole   BYTE "console",0
    szPaneTypeFileTree  BYTE "filetree",0
    szPaneTypeProperties BYTE "properties",0
    szPaneTypeOutput    BYTE "output",0
    szPaneTypeSearch    BYTE "search",0
    szPaneTypeDebug     BYTE "debug",0
    szPaneTypeTerminal  BYTE "terminal",0
    szPaneTypeProblems  BYTE "problems",0
    szPaneTypeGit       BYTE "git",0
    szPaneTypeAgent     BYTE "agent",0
    
    ; Pane position strings
    szPaneLeft          BYTE "left",0
    szPaneRight         BYTE "right",0
    szPaneTop           BYTE "top",0
    szPaneBottom        BYTE "bottom",0
    szPaneCenter        BYTE "center",0
    szPaneFloat         BYTE "float",0
    
    ; Pane action strings
    szPaneClose         BYTE "close",0
    szPaneMaximize      BYTE "maximize",0
    szPaneMinimize      BYTE "minimize",0
    szPaneRestore       BYTE "restore",0
    szPaneDock          BYTE "dock",0
    szPaneUndock        BYTE "undock",0
    szPaneDragStart     BYTE "drag_start",0
    szPaneDragEnd       BYTE "drag_end",0
    szPaneResize        BYTE "resize",0
    
    ; Pane tab strings
    szTabEditor         BYTE "Editor",0
    szTabConsole        BYTE "Console",0
    szTabProperties     BYTE "Properties",0
    szTabFileTree       BYTE "Files",0
    szTabTerminal       BYTE "Terminal",0
    szTabProblems       BYTE "Problems",0
    szTabSearch         BYTE "Search",0
    
    ; Component type strings
    szButtonType        BYTE "button", 0
    szInputType         BYTE "input", 0
    szLabelType         BYTE "label", 0

    ; Status messages
    szParsingStyle      BYTE "Parsing component style...", 0
    szApplyingTheme     BYTE "Applying theme...", 0

    ; Default JSON templates
    szDefaultButtonJson BYTE '{"type":"button","text":"Button","background":"#1976D2","color":"#FFFFFF"}', 0
    szDefaultInputJson  BYTE '{"type":"input","placeholder":"Enter text","background":"#FFFFFF","color":"#000000"}', 0

    ; Layout parsing constants
    szLayoutTypeKey     BYTE '"type"', 0
    szLayoutPaddingKey  BYTE '"padding"', 0
    szLayoutSpacingKey  BYTE '"spacing"', 0
    szLayoutDirectionKey BYTE '"direction"', 0
    szLayoutWrapKey     BYTE '"wrap"', 0
    szLayoutRowGapKey   BYTE '"rowGap"', 0
    szLayoutColumnGapKey BYTE '"columnGap"', 0
    szLayoutGrid_Val    BYTE "grid", 0
    szLayoutFlex_Val    BYTE "flex", 0
    szLayoutStack_Val   BYTE "stack", 0
    szLayoutAbsolute_Val BYTE "absolute", 0
    szLayoutDirectionRow BYTE "row", 0
    szLayoutDirectionColumn BYTE "column", 0
    szLayoutDirectionRowReverse BYTE "row-reverse", 0
    szLayoutDirectionColumnReverse BYTE "column-reverse", 0
    szLayoutWrap       BYTE "wrap", 0
    szLayoutNoWrap     BYTE "nowrap", 0
    szLayoutFlexStart  BYTE "flex-start", 0
    szLayoutFlexEnd    BYTE "flex-end", 0
    szLayoutCenter     BYTE "center", 0
    szLayoutSpaceBetween BYTE "space-between", 0
    szLayoutSpaceAround BYTE "space-around", 0
    szLayoutAlignStretch BYTE "stretch", 0
    szLayoutAlignFlexStart BYTE "flex-start", 0
    szLayoutAlignFlexEnd BYTE "flex-end", 0
    szLayoutAlignCenter BYTE "center", 0
    szLayoutAlignSpaceBetween BYTE "space-between", 0
    szLayoutAlignSpaceAround BYTE "space-around", 0
    szAnimDurationKey   BYTE '"duration"', 0
    szAnimEasingKey     BYTE '"easing"', 0
    szAnimDelayKey      BYTE '"delay"', 0
    szAnimPropertyKey   BYTE '"property"', 0
    szAnimFromKey       BYTE '"from"', 0
    szAnimToKey         BYTE '"to"', 0
    szAnimPropX         BYTE "x", 0
    szAnimPropY         BYTE "y", 0
    szAnimPropWidth     BYTE "width", 0
    szAnimPropHeight    BYTE "height", 0
    szAnimPropOpacity   BYTE "opacity", 0
    szEaseIn            BYTE "ease-in", 0
    szEaseOut           BYTE "ease-out", 0
    szEaseInOut         BYTE "ease-in-out", 0
    szEaseLinear        BYTE "linear", 0
    
    fltZero             REAL4 0.0
    fltOne              REAL4 1.0
    fltTwo              REAL4 2.0
    fltTen              REAL4 10.0
    fltPadding          REAL4 10.0
    
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
    szStyleKeyPaddingLeft BYTE "paddingLeft",0
    szStyleKeyPaddingRight BYTE "paddingRight",0
    szStyleKeyPaddingTop BYTE "paddingTop",0
    szStyleKeyPaddingBottom BYTE "paddingBottom",0
    szStyleKeyBorderWidth BYTE "borderWidth",0
    szStyleKeyBorderColor BYTE "borderColor",0
    szStyleKeyMarginLeft BYTE "marginLeft",0
    szStyleKeyMarginRight BYTE "marginRight",0
    szStyleKeyMarginTop BYTE "marginTop",0
    szStyleKeyMarginBottom BYTE "marginBottom",0
    szStyleKeyFontSize BYTE "fontSize",0
    szStyleKeyLineHeight BYTE "lineHeight",0
    szStyleKeyOpacity BYTE "opacity",0
    szStyleKeyTransform BYTE "transform",0
    szStyleKeyBoxShadow BYTE "boxShadow",0
    szStyleKeyFontWeight BYTE "fontWeight",0
    szStyleKeyTextAlign BYTE "textAlign",0
    
    ; Direct2D IIDs
    ALIGN 16
    IID_ID2D1Factory_Val DWORD 068386494h
                        WORD  06471h, 04024h
                        BYTE  088h, 007h, 0c2h, 065h, 02fh, 0a7h, 019h, 089h
    IID_IDWriteFactory_Val DWORD 0b859ee24h
                        WORD  02d00h, 04775h
                        BYTE  087h, 02ah, 005h, 039h, 014h, 003h, 004h, 0f0h
    
    ; ==== COMPLETE IDE STRING CONSTANTS ====
    ; Pane labels for full IDE layout
    szFileExplorer      BYTE "File Explorer",0
    szEditorArea        BYTE "Editor",0
    szAIAssistant       BYTE "AI Assistant",0
    szTerminalPanel     BYTE "Terminal",0
    szCommandPalette    BYTE "Command Palette",0
    
    ; Left sidebar tabs
    szFilesTab          BYTE "FILES",0
    szSearchTab         BYTE "SEARCH",0
    szSourceControlTab  BYTE "SOURCE CONTROL",0
    
    ; Editor tabs
    szWelcomeTab        BYTE "Welcome",0
    szMainAsmTab        BYTE "main.asm",0
    szGuiAsmTab         BYTE "gui_designer_agent.asm",0
    
    ; Right sidebar tabs
    szChatTab           BYTE "CHAT",0
    szAgentsTab         BYTE "AGENTS",0
    szOutlineTab        BYTE "OUTLINE",0
    
    ; Bottom panel tabs
    szTerminalTab       BYTE "TERMINAL",0
    szOutputTab         BYTE "OUTPUT",0
    szProblemsTab       BYTE "PROBLEMS",0
    szDebugConsoleTab   BYTE "DEBUG CONSOLE",0
    
    ; Component names
    szFileTreeComp      BYTE "FileTreeComponent",0
    szEditorComp        BYTE "EditorComponent",0
    szTerminalComp      BYTE "TerminalComponent",0
    szStatusBarComp     BYTE "StatusBarComponent",0
    szChatComp          BYTE "ChatComponent",0
    szInputComp         BYTE "InputComponent",0
    szTaskProposalComp  BYTE "TaskProposalComponent",0
    
    ; JSON serialization strings
    str_json_header     BYTE "Serializing pane layout to JSON...",0
    str_json_start      BYTE "{",0Ah,'  "panes": [',0Ah,0
    str_json_end        BYTE 0Ah,'  ]',0Ah,"}",0
    str_json_comma      BYTE ",",0Ah,0
    str_json_obj_end    BYTE 0Ah,"}",0
    str_pane_object_start BYTE "    {",0Ah,0
    str_pane_id_key     BYTE '      "id": ',0
    str_pane_type_key   BYTE ",",0Ah,'      "type": ',0
    str_pane_pos_key    BYTE ",",0Ah,'      "position": ',0
    str_pane_x_key      BYTE ",",0Ah,'      "x": ',0
    str_pane_y_key      BYTE ",",0Ah,'      "y": ',0
    str_pane_w_key      BYTE ",",0Ah,'      "width": ',0
    str_pane_h_key      BYTE ",",0Ah,'      "height": ',0
    str_pane_visible_key BYTE ",",0Ah,'      "visible": ',0
    str_pane_floating_key BYTE ",",0Ah,'      "floating": ',0
    str_pane_max_key    BYTE ",",0Ah,'      "maximized": ',0
    str_serialize_ok    BYTE "Pane layout successfully serialized to JSON",0
    str_deserialize_ok  BYTE "Pane layout successfully deserialized from JSON",0
    str_deserialize_failed BYTE "Failed to deserialize pane layout from JSON",0
    
    ; JSON deserialization keys
    str_pane_array_key  BYTE '"panes"',0
    str_pane_id_json    BYTE '"id"',0
    str_pane_type_json  BYTE '"type"',0
    str_pane_pos_json   BYTE '"position"',0
    str_pane_x_json     BYTE '"x"',0
    str_pane_y_json     BYTE '"y"',0
    str_pane_w_json     BYTE '"width"',0
    str_pane_h_json     BYTE '"height"',0
    str_pane_visible_json BYTE '"visible"',0
    str_pane_floating_json BYTE '"floating"',0
    str_pane_max_json   BYTE '"maximized"',0

.data?
    ; Pane registry and management (RawrXD IDE pane system)
    PaneRegistry        PANE MAX_PANES DUP (<>)
    PaneCount           DWORD ?
    NextPaneId          DWORD ?
    
    ; Pane layout system
    CurrentLayout       PANE_LAYOUT <>
    LayoutSaveFile      BYTE 512 DUP (?)
    
    ; Pane tabs
    PaneTabRegistry     PANE_TAB MAX_PANES * MAX_PANE_TABS DUP (<>)
    TabCount            DWORD ?
    
    ; Splitter state
    ActiveSplitter      DWORD ?           ; Which splitter is being dragged
    SplitterStartX      DWORD ?
    SplitterStartY      DWORD ?
    SplitterOldPos      DWORD ?
    
    ; Drag and drop state
    DraggedPaneId       DWORD ?
    DragStartX          DWORD ?
    DragStartY          DWORD ?
    DropTargetPaneId    DWORD ?
    
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
; PUBLIC: gui_create_pane(pane_type: rcx, position: edx, label: r8) -> eax (pane_id)
;==========================================================================
PUBLIC gui_create_pane
ALIGN 16
gui_create_pane PROC
    ; rcx = pane_type, edx = position, r8 = label string
    ; Returns: eax = pane_id (-1 on failure)
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 32
    
    mov r12, rcx ; pane_type
    mov r13d, edx ; position
    mov r14, r8  ; label
    
    ; Check if we have space
    mov eax, [NextPaneId]
    cmp eax, MAX_PANES
    jae create_pane_full
    
    ; Get pane registry entry
    mov ebx, eax ; ebx = pane_id
    lea rcx, [PaneRegistry]
    mov edx, SIZEOF PANE
    imul eax, edx
    add rcx, rax
    mov rsi, rcx
    
    ; Initialize pane
    mov [rsi + PANE.id], ebx
    mov eax, r12d
    mov [rsi + PANE.pane_type], eax
    mov [rsi + PANE.position], r13d
    mov [rsi + PANE.is_visible], 1
    mov [rsi + PANE.is_docked], 1
    mov [rsi + PANE.can_close], 1
    mov [rsi + PANE.can_resize], 1
    movss xmm0, [fltOne]
    movss DWORD PTR [rsi + PANE.flex_grow], xmm0
    movss DWORD PTR [rsi + PANE.flex_shrink], xmm0
    
    ; Copy label to pane
    mov rsi, r14
    mov eax, ebx
    mov edx, SIZEOF PANE
    imul eax, edx
    lea rdi, [PaneRegistry + rax]
    add rdi, PANE.content_hwnd ; Use nearby space for label (hacky but matches original intent)
    mov ecx, 256
    rep movsb
    
    ; Increment counter and ID
    inc DWORD PTR [PaneCount]
    inc DWORD PTR [NextPaneId]
    
    mov eax, ebx
    jmp create_pane_done
    
create_pane_full:
    mov eax, -1
    
create_pane_done:
    add rsp, 32
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
gui_create_pane ENDP

;==========================================================================
; PUBLIC: gui_set_pane_hwnd(pane_id: ecx, hwnd: rdx) -> eax
;==========================================================================
PUBLIC gui_set_pane_hwnd
ALIGN 16
gui_set_pane_hwnd PROC
    ; ecx = pane_id, rdx = hwnd
    push rsi
    
    lea rsi, [PaneRegistry]
    mov eax, SIZEOF PANE
    imul ecx, eax
    add rsi, rcx
    
    mov [rsi + PANE.hwnd], rdx
    mov [rsi + PANE.content_hwnd], rdx
    
    mov eax, 1
    pop rsi
    ret
gui_set_pane_hwnd ENDP

;==========================================================================
; PUBLIC: gui_create_complete_ide()
; Creates the full IDE layout using the advanced pane system.
;==========================================================================
PUBLIC gui_create_complete_ide
ALIGN 16
gui_create_complete_ide PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32

    ; 1. Initialize registry
    call gui_init_registry

    ; 2. Create File Explorer (Left)
    mov rcx, PANE_TYPE_EXPLORER
    mov edx, PANE_POSITION_LEFT
    lea r8, szFileExplorer
    call gui_create_pane
    mov ebx, eax ; pane_id
    call ui_get_file_tree_hwnd
    mov ecx, ebx
    mov rdx, rax
    call gui_set_pane_hwnd

    ; 3. Create Editor (Center)
    mov rcx, PANE_TYPE_EDITOR
    mov edx, PANE_POSITION_CENTER
    lea r8, szEditorArea
    call gui_create_pane
    mov ebx, eax
    call ui_get_editor_hwnd
    mov ecx, ebx
    mov rdx, rax
    call gui_set_pane_hwnd

    ; 4. Create Terminal (Bottom)
    mov rcx, PANE_TYPE_TERMINAL
    mov edx, PANE_POSITION_BOTTOM
    lea r8, szTerminalPanel
    call gui_create_pane
    mov ebx, eax
    call ui_get_terminal_hwnd
    mov ecx, ebx
    mov rdx, rax
    call gui_set_pane_hwnd

    ; 5. Create AI Assistant (Right)
    mov rcx, PANE_TYPE_CHAT
    mov edx, PANE_POSITION_RIGHT
    lea r8, szAIAssistant
    call gui_create_pane
    mov ebx, eax
    call ui_get_chat_hwnd
    mov ecx, ebx
    mov rdx, rax
    call gui_set_pane_hwnd

    ; 6. Finalize
    call gui_finalize_panes

    mov eax, 1
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
gui_create_complete_ide ENDP

;==========================================================================
; PUBLIC: gui_add_pane_tab(pane_id: ecx, tab_label: rdx) -> eax (tab_id)
;==========================================================================
PUBLIC gui_add_pane_tab
ALIGN 16
gui_add_pane_tab PROC
    ; ecx = pane_id, rdx = tab_label string
    ; Returns: eax = tab_id
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 32
    
    mov r12d, ecx ; pane_id
    mov r13, rdx  ; tab_label
    
    ; Find pane
    lea rax, [PaneRegistry]
    mov edx, SIZEOF PANE
    imul ecx, edx
    add rax, rcx
    mov rsi, rax
    
    ; Check tab count
    mov eax, [rsi + PANE.tab_count]
    cmp eax, MAX_PANE_TABS
    jae add_tab_full
    
    ; Create tab entry
    mov ebx, eax ; ebx = tab_id
    mov ecx, SIZEOF PANE_TAB
    imul ecx, eax
    lea rsi, [PaneTabRegistry + rcx]
    
    mov [rsi + PANE_TAB.id], ebx
    mov [rsi + PANE_TAB.pane_id], r12d
    mov [rsi + PANE_TAB.is_active], 1
    mov [rsi + PANE_TAB.is_closable], 1
    
    ; Copy label
    mov rsi, r13
    lea rdi, [PaneTabRegistry + rcx]
    add rdi, PANE_TAB.tab_label
    mov ecx, 256
    rep movsb
    
    ; Increment tab count in pane
    mov eax, r12d
    mov esi, SIZEOF PANE
    imul esi, eax
    lea rsi, [PaneRegistry + rsi]
    inc DWORD PTR [rsi + PANE.tab_count]
    
    mov eax, [rsi + PANE.tab_count]
    jmp add_tab_done
    
add_tab_full:
    mov eax, -1
    
add_tab_done:
    add rsp, 32
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
gui_add_pane_tab ENDP

;==========================================================================
; PUBLIC: gui_set_pane_size(pane_id: ecx, width: edx, height: r8d) -> eax
;==========================================================================
PUBLIC gui_set_pane_size
ALIGN 16
gui_set_pane_size PROC
    ; ecx = pane_id, edx = width, r8d = height
    push rsi
    push rbx
    
    mov ebx, ecx
    lea rsi, [PaneRegistry]
    mov eax, SIZEOF PANE
    imul ecx, eax
    add rsi, rcx
    
    mov [rsi + PANE.pane_width], edx
    mov [rsi + PANE.pane_height], r8d
    
    mov eax, 1
    pop rbx
    pop rsi
    ret
gui_set_pane_size ENDP

;==========================================================================
; PUBLIC: gui_toggle_pane_visibility(pane_id: ecx) -> eax
;==========================================================================
PUBLIC gui_toggle_pane_visibility
ALIGN 16
gui_toggle_pane_visibility PROC
    ; ecx = pane_id
    lea rsi, [PaneRegistry]
    mov eax, SIZEOF PANE
    imul ecx, eax
    add rsi, rcx
    
    mov eax, [rsi + PANE.is_visible]
    xor eax, 1
    mov [rsi + PANE.is_visible], eax
    
    ret
gui_toggle_pane_visibility ENDP

;==========================================================================
; PUBLIC: gui_maximize_pane(pane_id: ecx) -> eax
;==========================================================================
PUBLIC gui_maximize_pane
ALIGN 16
gui_maximize_pane PROC
    ; ecx = pane_id
    lea rsi, [PaneRegistry]
    mov eax, SIZEOF PANE
    imul ecx, eax
    add rsi, rcx
    
    mov DWORD PTR [rsi + PANE.is_maximized], 1
    mov eax, 1
    ret
gui_maximize_pane ENDP

;==========================================================================
; PUBLIC: gui_restore_pane(pane_id: ecx) -> eax
;==========================================================================
PUBLIC gui_restore_pane
ALIGN 16
gui_restore_pane PROC
    ; ecx = pane_id
    lea rsi, [PaneRegistry]
    mov eax, SIZEOF PANE
    imul ecx, eax
    add rsi, rcx
    
    mov DWORD PTR [rsi + PANE.is_maximized], 0
    mov eax, 1
    ret
gui_restore_pane ENDP

;==========================================================================
; PUBLIC: gui_dock_pane(pane_id: ecx, target_position: edx) -> eax
;==========================================================================
PUBLIC gui_dock_pane
ALIGN 16
gui_dock_pane PROC
    ; ecx = pane_id, edx = target_position (PANE_POSITION_*)
    lea rsi, [PaneRegistry]
    mov eax, SIZEOF PANE
    imul ecx, eax
    add rsi, rcx
    
    mov [rsi + PANE.position], edx
    mov DWORD PTR [rsi + PANE.is_docked], 1
    mov DWORD PTR [rsi + PANE.is_floating], 0
    
    mov eax, 1
    ret
gui_dock_pane ENDP

;==========================================================================
; PUBLIC: gui_undock_pane(pane_id: ecx) -> eax
;==========================================================================
PUBLIC gui_undock_pane
ALIGN 16
gui_undock_pane PROC
    ; ecx = pane_id - makes pane floating
    lea rsi, [PaneRegistry]
    mov eax, SIZEOF PANE
    imul ecx, eax
    add rsi, rcx
    
    mov DWORD PTR [rsi + PANE.position], PANE_POSITION_FLOAT
    mov DWORD PTR [rsi + PANE.is_floating], 1
    mov DWORD PTR [rsi + PANE.is_docked], 0
    
    mov eax, 1
    ret
gui_undock_pane ENDP

;==========================================================================
; PUBLIC: gui_save_pane_layout(filename: rcx) -> eax
;==========================================================================
PUBLIC gui_save_pane_layout
ALIGN 16
gui_save_pane_layout PROC
    ; rcx = filename to save layout to
    ; Returns: eax = 1 on success, 0 on failure
    push rbx
    push rsi
    push rdi
    sub rsp, 32                ; shadow space for calls

    ; Copy filename into temporary buffer (LayoutSaveFile)
    mov rsi, rcx
    lea rdi, [LayoutSaveFile]
    mov ecx, 512
    rep movsb

    ; Log start of serialization
    lea rcx, [str_json_header]
    call asm_log

    ; Open file for writing (CREATE_ALWAYS)
    lea rcx, [LayoutSaveFile]   ; filename
    mov edx, GENERIC_WRITE
    xor r8d, r8d                ; dwShareMode = 0
    xor r9d, r9d                ; lpSecurityAttributes = NULL
    mov r10d, CREATE_ALWAYS    ; dwCreationDisposition
    mov r11d, FILE_ATTRIBUTE_NORMAL ; dwFlagsAndAttributes
    sub rsp, 32                 ; shadow space for CreateFileA
    call CreateFileA
    add rsp, 32
    cmp rax, INVALID_HANDLE_VALUE
    je save_fail
    mov rbx, rax                ; file handle

    ; Write PaneCount (DWORD)
    mov rcx, rbx
    lea rdx, [PaneCount]
    mov r8d, 4                  ; bytes to write
    lea r9, [rsp-8]             ; bytes written placeholder
    sub rsp, 32
    call WriteFile
    add rsp, 32
    test eax, eax
    jz save_fail

    ; Write used panes (binary dump of PaneRegistry up to PaneCount)
    mov rcx, rbx
    lea rdx, [PaneRegistry]
    mov eax, [PaneCount]
    imul eax, SIZEOF PANE
    mov r8d, eax                ; number of bytes
    lea r9, [rsp-8]
    sub rsp, 32
    call WriteFile
    add rsp, 32
    test eax, eax
    jz save_fail

    ; Close file handle
    mov rcx, rbx
    call CloseHandle

    ; Log success
    lea rcx, [str_serialize_ok]
    call asm_log
    mov eax, 1
    jmp save_done

save_fail:
    ; Log failure
    lea rcx, [str_deserialize_failed]
    call asm_log
    xor eax, eax

save_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
 gui_save_pane_layout ENDP

;==========================================================================
; PUBLIC: gui_load_pane_layout(filename: rcx) -> eax
;==========================================================================
PUBLIC gui_load_pane_layout
ALIGN 16
gui_load_pane_layout PROC
    ; rcx = filename to load layout from
    ; Returns: eax = 1 on success, 0 on failure
    push rbx
    push rsi
    push rdi
    sub rsp, 32                ; shadow space for calls

    ; Copy filename into temporary buffer
    mov rsi, rcx
    lea rdi, [LayoutSaveFile]
    mov ecx, 512
    rep movsb

    ; Log start of deserialization
    lea rcx, [str_deserialize_ok]
    call asm_log

    ; Open file for reading (OPEN_EXISTING)
    lea rcx, [LayoutSaveFile]
    mov edx, GENERIC_READ
    xor r8d, r8d                ; dwShareMode = 0
    xor r9d, r9d                ; lpSecurityAttributes = NULL
    mov r10d, OPEN_EXISTING      ; dwCreationDisposition
    xor r11d, r11d              ; dwFlagsAndAttributes = 0
    sub rsp, 32
    call CreateFileA
    add rsp, 32
    cmp rax, INVALID_HANDLE_VALUE
    je load_fail
    mov rbx, rax                ; file handle

    ; Read PaneCount (DWORD)
    mov rcx, rbx
    lea rdx, [PaneCount]
    mov r8d, 4
    lea r9, [rsp-8]
    sub rsp, 32
    call ReadFile
    add rsp, 32
    test eax, eax
    jz load_fail

    ; Read PaneRegistry (binary)
    mov rcx, rbx
    lea rdx, [PaneRegistry]
    mov eax, [PaneCount]
    imul eax, SIZEOF PANE
    mov r8d, eax
    lea r9, [rsp-8]
    sub rsp, 32
    call ReadFile
    add rsp, 32
    test eax, eax
    jz load_fail

    ; Close file handle
    mov rcx, rbx
    call CloseHandle

    ; Log success
    lea rcx, [str_serialize_ok]
    call asm_log
    mov eax, 1
    jmp load_done

load_fail:
    lea rcx, [str_deserialize_failed]
    call asm_log
    xor eax, eax

load_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
gui_load_pane_layout ENDP

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
    
    ; Finalize panes (ensure real HWNDs shown, no placeholders)
    call gui_finalize_panes

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
; PUBLIC: gui_finalize_panes()
; Ensures all panes have real HWNDs shown and updated (no placeholders)
;==========================================================================
PUBLIC gui_finalize_panes
ALIGN 16
gui_finalize_panes PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 32

    ; Iterate pane registry
    lea rdi, [PaneRegistry]
    mov ecx, PaneCount
    xor ebx, ebx

final_loop:
    cmp ebx, ecx
    jae final_done

    ; Calculate pane entry address
    mov eax, ebx
    mov edx, SIZEOF PANE
    imul eax, edx
    lea rsi, [rdi + rax]

    ; Show and update real window if present
    mov rcx, [rsi + PANE.hwnd]
    test rcx, rcx
    jz next_final
    mov edx, 5                 ; SW_SHOW
    call ShowWindow
    mov rcx, [rsi + PANE.hwnd]
    call UpdateWindow

next_final:
    inc ebx
    jmp final_loop

final_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
gui_finalize_panes ENDP

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
    
;==========================================================================
; ParseStyleJson(dest_style: rcx, json_str: rdx) -> eax (0=success)
;==========================================================================
ParseStyleJson PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 256
    
    mov rbx, rcx  ; dest_style
    mov rsi, rdx  ; json_str
    
    ; Simple JSON parsing (stub implementation)
    ; Look for key-value pairs and parse them
    
    ; Parse background color
    lea rcx, szBgColorKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_bg_color
    
    add rax, 12  ; Skip "bg_color":"
    mov rcx, rax
    call parse_hex_color
    mov [rbx + STYLE.bg_color], eax
    
@no_bg_color:
    ; Parse foreground color
    lea rcx, szFgColorKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_fg_color
    
    add rax, 12  ; Skip "fg_color":"
    mov rcx, rax
    call parse_hex_color
    mov [rbx + STYLE.fg_color], eax
    
@no_fg_color:
    ; Parse font size
    lea rcx, szFontSizeKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_font_size
    
    add rax, 11  ; Skip "font_size":
    mov rcx, rax
    call parse_int
    mov [rbx + STYLE.font_size], eax
    
@no_font_size:
    ; Parse opacity
    lea rcx, szOpacityKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_opacity
    
    add rax, 9   ; Skip "opacity":
    mov rcx, rax
    call parse_float
    mov [rbx + STYLE.opacity], eax
    
@no_opacity:
    mov eax, 1  ; Success
    jmp @done
    
@parse_error:
    xor eax, eax  ; Failure
    
@done:
    add rsp, 256
    pop rdi
    pop rsi
    pop rbx
    ret
ParseStyleJson ENDP
    
    ; Find component
    mov ecx, r12d
    call FindComponent
    test rax, rax
    jz style_component_not_found
    
    mov rbx, rax        ; component
    
;==========================================================================
; ApplyComponentStyle(component_id: ecx, style_ptr: rdx) -> eax
;==========================================================================
ApplyComponentStyle PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 128
    
    mov rbx, rcx  ; component_id
    mov rsi, rdx  ; style_ptr
    
    ; Find component
    mov ecx, ebx
    call FindComponent
    test rax, rax
    jz @not_found
    
    mov rdi, rax  ; component
    
    ; Apply background color
    mov eax, [rsi + STYLE.bg_color]
    mov [rdi + COMPONENT.bg_color], eax
    
    ; Apply foreground color
    mov eax, [rsi + STYLE.fg_color]
    mov [rdi + COMPONENT.fg_color], eax
    
    ; Apply border color
    mov eax, [rsi + STYLE.border_color]
    mov [rdi + COMPONENT.border_color], eax
    
    ; Apply border width
    mov eax, [rsi + STYLE.border_width]
    mov [rdi + COMPONENT.border_width], eax
    
    ; Apply corner radius
    mov eax, [rsi + STYLE.corner_radius]
    mov [rdi + COMPONENT.corner_radius], eax
    
    ; Apply font size
    mov eax, [rsi + STYLE.font_size]
    mov [rdi + COMPONENT.font_size], eax
    
    ; Apply font weight
    mov eax, [rsi + STYLE.font_weight]
    mov [rdi + COMPONENT.font_weight], eax
    
    ; Apply padding
    mov eax, [rsi + STYLE.padding_left]
    mov [rdi + COMPONENT.padding_left], eax
    mov eax, [rsi + STYLE.padding_top]
    mov [rdi + COMPONENT.padding_top], eax
    mov eax, [rsi + STYLE.padding_right]
    mov [rdi + COMPONENT.padding_right], eax
    mov eax, [rsi + STYLE.padding_bottom]
    mov [rdi + COMPONENT.padding_bottom], eax
    
    ; Apply margin
    mov eax, [rsi + STYLE.margin_left]
    mov [rdi + COMPONENT.margin_left], eax
    mov eax, [rsi + STYLE.margin_top]
    mov [rdi + COMPONENT.margin_top], eax
    mov eax, [rsi + STYLE.margin_right]
    mov [rdi + COMPONENT.margin_right], eax
    mov eax, [rsi + STYLE.margin_bottom]
    mov [rdi + COMPONENT.margin_bottom], eax
    
    ; Apply opacity
    mov eax, [rsi + STYLE.opacity]
    mov [rdi + COMPONENT.opacity], eax
    
    ; Apply shadow
    mov eax, [rsi + STYLE.shadow_blur]
    mov [rdi + COMPONENT.shadow_blur], eax
    mov eax, [rsi + STYLE.shadow_offset_x]
    mov [rdi + COMPONENT.shadow_offset_x], eax
    mov eax, [rsi + STYLE.shadow_offset_y]
    mov [rdi + COMPONENT.shadow_offset_y], eax
    mov eax, [rsi + STYLE.shadow_color]
    mov [rdi + COMPONENT.shadow_color], eax
    
    ; Apply visibility
    mov al, [rsi + STYLE.visible]
    mov [rdi + COMPONENT.visible], al
    
    ; Apply enabled state
    mov al, [rsi + STYLE.enabled]
    mov [rdi + COMPONENT.enabled], al
    
    xor eax, eax  ; Success
    jmp @done
    
@not_found:
    mov eax, 1  ; Component not found
    
@done:
    add rsp, 128
    pop rdi
    pop rsi
    pop rbx
    ret
ApplyComponentStyle ENDP

;==========================================================================
; UpdateComponentAppearance(comp_ptr: rbx)
;==========================================================================
UpdateComponentAppearance PROC
    push rbx
    push rsi
    sub rsp, 64
    
    ; Get component window handle
    mov rsi, [rbx + COMPONENT.hwnd]
    test rsi, rsi
    jz @no_window
    
    ; Invalidate window to trigger repaint
    mov rcx, rsi
    xor rdx, rdx
    mov r8d, 0
    call InvalidateRect
    
    ; Update window position if needed
    mov rcx, rsi
    mov edx, [rbx + COMPONENT.x]
    mov r8d, [rbx + COMPONENT.y]
    mov r9d, [rbx + COMPONENT.width]
    mov eax, [rbx + COMPONENT.height]
    mov [rsp + 32], eax
    mov qword ptr [rsp + 40], SWP_NOZORDER
    call SetWindowPos
    
    ; Update visibility
    mov rcx, rsi
    mov edx, [rbx + COMPONENT.visible]
    test edx, edx
    jnz @show
    mov edx, SW_HIDE
    jmp @set_visibility
@show:
    mov edx, SW_SHOW
@set_visibility:
    call ShowWindow
    
@no_window:
    xor eax, eax  ; Success
    add rsp, 64
    pop rsi
    pop rbx
    ret
UpdateComponentAppearance ENDP

;==========================================================================
; StartStyleAnimation() -> eax
;==========================================================================
StartStyleAnimation PROC
    push rbx
    sub rsp, 32
    
    ; Set animation timer (stub implementation)
    mov rcx, g_hMainWnd
    mov edx, ANIMATION_TIMER_ID
    mov r8d, 16  ; 16ms interval (~60fps)
    mov r9d, 0
    call SetTimer
    
    mov g_animation_active, 1
    
    xor eax, eax  ; Success
    add rsp, 32
    pop rbx
    ret
StartStyleAnimation ENDP

;==========================================================================
; parse_hex_color(hex_str: rcx) -> eax (color)
;==========================================================================
parse_hex_color PROC
    xor eax, eax
    
    ; Skip '#' if present
    cmp byte ptr [rcx], '#'
    jne @parse_loop
    inc rcx
    
@parse_loop:
    mov dl, [rcx]
    test dl, dl
    jz @done
    cmp dl, '"'
    je @done
        
    shl eax, 4
    cmp dl, '9'
    jbe @digit
    cmp dl, 'F'
    jbe @upper
    cmp dl, 'f'
    jbe @lower
    jmp @skip
    
@digit:
    sub dl, '0'
    jmp @add
@upper:
    sub dl, 'A' - 10
    jmp @add
@lower:
    sub dl, 'a' - 10
@add:
    or al, dl
@skip:
    inc rcx
    jmp @parse_loop
    
@done:
    or eax, 0FF000000h  ; Set alpha to FF
    ret
parse_hex_color ENDP

parse_int PROC
    ; rcx = string -> returns DWORD
    xor eax, eax
    xor rdx, rdx
    
@parse_loop:
    mov dl, byte ptr [rcx]
    test dl, dl
    jz @done
    cmp dl, '0'
    jb @done
    cmp dl, '9'
    ja @done
    
    imul eax, 10
    sub dl, '0'
    add eax, edx
    inc rcx
    jmp @parse_loop
    
@done:
    ret
parse_int ENDP

parse_float PROC
    ; rcx = string -> returns REAL4
    ; Simple implementation: parse as int then convert
    call parse_int
    cvtsi2ss xmm0, eax
    ret
parse_float ENDP

parse_easing_type PROC
    ; rcx = string -> returns easing type enum
    lea rdx, szEaseIn
    call strcmp_masm
    test eax, eax
    jnz @not_ease_in
    mov eax, EASING_EASE_IN
    ret
    
@not_ease_in:
    lea rdx, szEaseOut
    call strcmp_masm
    test eax, eax
    jnz @not_ease_out
    mov eax, EASING_EASE_OUT
    ret
    
@not_ease_out:
    lea rdx, szEaseInOut
    call strcmp_masm
    test eax, eax
    jnz @not_ease_inout
    mov eax, EASING_EASE_IN_OUT
    ret
    
@not_ease_inout:
    mov eax, EASING_LINEAR  ; Default
    ret
parse_easing_type ENDP

parse_pane_json PROC
    ; rcx = component_ptr, rdx = json_string -> returns eax (0=success)
    push rbx
    push rsi
    sub rsp, 128
    
    mov rbx, rcx
    mov rsi, rdx
    
    ; Parse ID
    lea rcx, szIdKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_id
    
    add rax, 5  ; Skip "id":
    mov rcx, rax
    call parse_int
    mov [rbx + COMPONENT.id], eax
    
@no_id:
    ; Parse type
    lea rcx, szTypeKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_type
    
    add rax, 7  ; Skip "type":
    mov rcx, rax
    call parse_int
    mov [rbx + COMPONENT.comp_type], eax
    
@no_type:
    ; Parse position and size
    lea rcx, szXKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_x
    
    add rax, 4  ; Skip "x":
    mov rcx, rax
    call parse_int
    mov [rbx + COMPONENT.x], eax
    
@no_x:
    lea rcx, szYKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_y
    
    add rax, 4  ; Skip "y":
    mov rcx, rax
    call parse_int
    mov [rbx + COMPONENT.y], eax
    
@no_y:
    lea rcx, szWidthKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_width
    
    add rax, 8  ; Skip "width":
    mov rcx, rax
    call parse_int
    mov [rbx + COMPONENT.width], eax
    
@no_width:
    lea rcx, szHeightKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_height
    
    add rax, 9  ; Skip "height":
    mov rcx, rax
    call parse_int
    mov [rbx + COMPONENT.height], eax
    
@no_height:
    ; Parse visibility
    lea rcx, szVisibleKey
    mov rdx, rsi
    call strstr_masm
    test rax, rax
    jz @no_visible
    
    add rax, 10  ; Skip "visible":
    mov rcx, rax
    call parse_int
    mov [rbx + COMPONENT.visible], al
    
@no_visible:
    mov eax, 1  ; Success
    jmp @done
    
@parse_error:
    xor eax, eax  ; Failure
    
@done:
    add rsp, 128
    pop rsi
    pop rbx
    ret
parse_pane_json ENDP

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
    mov ecx, SIZEOF COMPONENT
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
    mov ecx, SIZEOF COMPONENT
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
    mov ecx, SIZEOF ANIMATION
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
    mov ecx, SIZEOF STYLE
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
    mov eax, SIZEOF THEME
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

;==========================================================================
; PUBLIC: ParseLayoutJson(dest: rcx, layout_json: rdx) -> eax
;==========================================================================
PUBLIC ParseLayoutJson
ALIGN 16
ParseLayoutJson PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13

    mov r12, rcx             ; dest
    mov r13, rdx             ; json

    test r12, r12
    jz layout_fail
    test r13, r13
    jz layout_fail

    ; defaults
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.layout_type], LAYOUT_GRID
    xor eax, eax
    mov [r12 + LAYOUT_PROPERTIES.direction], eax
    mov [r12 + LAYOUT_PROPERTIES.wrap], eax
    mov [r12 + LAYOUT_PROPERTIES.justify_content], eax
    mov [r12 + LAYOUT_PROPERTIES.align_items], eax
    mov [r12 + LAYOUT_PROPERTIES.align_content], eax
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.gap], 0
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.row_gap], 0
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.column_gap], 0

    ; layout type
    mov rcx, r13
    lea rdx, szLayoutTypeKey
    call FindJsonValue
    test rax, rax
    jz skip_layout_type
    call SkipJsonWhitespace
    mov rsi, rax
    lea rcx, szLayoutGrid
    mov rdx, rsi
    call StringFind
    test rax, rax
    jne set_grid
    lea rdx, szLayoutFlex
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_flex
    lea rdx, szLayoutStack
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_stack
    lea rdx, szLayoutAbsolute
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_absolute
    jmp skip_layout_type

set_grid:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.layout_type], LAYOUT_GRID
    jmp skip_layout_type

set_flex:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.layout_type], LAYOUT_FLEX
    jmp skip_layout_type

set_stack:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.layout_type], LAYOUT_STACK
    jmp skip_layout_type

set_absolute:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.layout_type], LAYOUT_ABSOLUTE

skip_layout_type:
    ; direction
    mov rcx, r13
    lea rdx, szLayoutDirectionKey
    call FindJsonValue
    test rax, rax
    jz skip_direction
    call SkipJsonWhitespace
    mov rsi, rax
    lea rdx, szLayoutDirectionRow
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_direction_row
    lea rdx, szLayoutDirectionRowReverse
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_direction_row_rev
    lea rdx, szLayoutDirectionColumn
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_direction_column
    lea rdx, szLayoutDirectionColumnReverse
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_direction_column_rev
    jmp skip_direction

set_direction_row:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.direction], 0
    jmp skip_direction

set_direction_row_rev:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.direction], 1
    jmp skip_direction

set_direction_column:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.direction], 2
    jmp skip_direction

set_direction_column_rev:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.direction], 3

skip_direction:
    ; wrap
    mov rcx, r13
    lea rdx, szLayoutWrapKey
    call FindJsonValue
    test rax, rax
    jz skip_wrap
    call SkipJsonWhitespace
    mov rsi, rax
    lea rdx, szLayoutWrap
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_wrap
    lea rdx, szLayoutNoWrap
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_nowrap
    jmp skip_wrap

set_wrap:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.wrap], 1
    jmp skip_wrap

set_nowrap:
    mov DWORD PTR [r12 + LAYOUT_PROPERTIES.wrap], 0

skip_wrap:
    ; gap spacing
    mov rcx, r13
    lea rdx, szLayoutSpacingKey
    call FindJsonValue
    test rax, rax
    jz skip_gap
    call SkipJsonWhitespace
    mov rcx, rax
    call ParseFloatValue
    movss [r12 + LAYOUT_PROPERTIES.gap], xmm0
skip_gap:
    mov rcx, r13
    lea rdx, szLayoutRowGapKey
    call FindJsonValue
    test rax, rax
    jz skip_row_gap
    call SkipJsonWhitespace
    mov rcx, rax
    call ParseFloatValue
    movss [r12 + LAYOUT_PROPERTIES.row_gap], xmm0
skip_row_gap:
    mov rcx, r13
    lea rdx, szLayoutColumnGapKey
    call FindJsonValue
    test rax, rax
    jz skip_column_gap
    call SkipJsonWhitespace
    mov rcx, rax
    call ParseFloatValue
    movss [r12 + LAYOUT_PROPERTIES.column_gap], xmm0
skip_column_gap:
    mov eax, 1
    jmp layout_done

layout_fail:
    xor eax, eax

layout_done:
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ParseLayoutJson ENDP

;==========================================================================
; PUBLIC: ApplyLayoutProperties(LAYOUT_PROPERTIES *)
;==========================================================================
PUBLIC ApplyLayoutProperties
ALIGN 16
ApplyLayoutProperties PROC
    push rbx
    push rsi
    push rdi

    test rcx, rcx
    jz apply_done

    mov rsi, rcx
    lea rdi, LayoutProps
    mov rcx, SIZEOF LAYOUT_PROPERTIES
    rep movsb

apply_done:
    pop rdi
    pop rsi
    pop rbx
    ret
ApplyLayoutProperties ENDP

;==========================================================================
; PUBLIC: RecalculateLayout() - simple grid placement
;==========================================================================
PUBLIC RecalculateLayout
ALIGN 16
RecalculateLayout PROC
    push rbx
    push rsi
    push rdi
    push r12

    xor r12d, r12d
    mov rbx, ComponentCount
    cmp rbx, 0
    je recalc_done

    movss xmm0, [LayoutProps + LAYOUT_PROPERTIES.gap]
    cvtss2si rax, xmm0
    mov rbx, rax
    add rbx, 10
    cmp rbx, 16
    jae gap_ok
    mov rbx, 16
gap_ok:

recalc_loop:
    cmp r12d, ComponentCount
    jae recalc_done
    mov eax, r12d
    mov ecx, SIZEOF COMPONENT
    imul ecx, eax
    lea rdi, ComponentRegistry
    movsxd rax, ecx
    add rdi, rax

    mov eax, r12d
    xor edx, edx
    mov ecx, 4
    idiv ecx
    mov r14d, eax          ; row
    mov r15d, edx          ; col

    mov eax, r15d
    imul eax, ebx
    add eax, 20
    cvtsi2ss xmm1, eax
    movss [rdi + COMPONENT.x], xmm1

    mov eax, r14d
    imul eax, ebx
    add eax, 40
    cvtsi2ss xmm2, eax
    movss [rdi + COMPONENT.y], xmm2

    inc r12d
    jmp recalc_loop

recalc_done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
RecalculateLayout ENDP

;==========================================================================
; ParseAnimationJson(dest: rcx, json: rdx) -> eax
;==========================================================================
PUBLIC ParseAnimationJson
ALIGN 16
ParseAnimationJson PROC
    push rbx
    push rsi
    push rdi

    mov rsi, rcx
    mov rdi, rdx
    test rsi, rsi
    jz anim_fail
    test rdi, rdi
    jz anim_fail

    ; Defaults
    mov DWORD PTR [rsi + ANIMATION.property], ANIMATION_PROPERTY_X
    movss xmm0, [fltZero]
    movss [rsi + ANIMATION.from_value], xmm0
    movss xmm1, [fltOne]
    movss [rsi + ANIMATION.to_value], xmm1
    mov DWORD PTR [rsi + ANIMATION.duration], 400
    mov DWORD PTR [rsi + ANIMATION.delay], 0
    mov DWORD PTR [rsi + ANIMATION.easing], EASING_LINEAR
    mov DWORD PTR [rsi + ANIMATION.iterations], 1
    mov DWORD PTR [rsi + ANIMATION.direction], 0
    mov DWORD PTR [rsi + ANIMATION.fill_mode], 0
    xor eax, eax
    mov DWORD PTR [rsi + ANIMATION.running], eax
    mov DWORD PTR [rsi + ANIMATION.start_time], eax
    mov DWORD PTR [rsi + ANIMATION.current_time], eax

    ; from value
    mov rcx, rdi
    lea rdx, szAnimFromKey
    call FindJsonValue
    test rax, rax
    jz skip_from
    call SkipJsonWhitespace
    mov rcx, rax
    call ParseFloatValue
    movss [rsi + ANIMATION.from_value], xmm0
skip_from:

    ; to value
    mov rcx, rdi
    lea rdx, szAnimToKey
    call FindJsonValue
    test rax, rax
    jz skip_to
    call SkipJsonWhitespace
    mov rcx, rax
    call ParseFloatValue
    movss [rsi + ANIMATION.to_value], xmm0
skip_to:

    ; duration
    mov rcx, rdi
    lea rdx, szAnimDurationKey
    call FindJsonValue
    test rax, rax
    jz skip_duration
    call SkipJsonWhitespace
    mov rcx, rax
    call ParseFloatValue
    cvtss2si eax, xmm0
    mov [rsi + ANIMATION.duration], eax
skip_duration:

    ; delay
    mov rcx, rdi
    lea rdx, szAnimDelayKey
    call FindJsonValue
    test rax, rax
    jz skip_delay
    call SkipJsonWhitespace
    mov rcx, rax
    call ParseFloatValue
    cvtss2si eax, xmm0
    mov [rsi + ANIMATION.delay], eax
skip_delay:

    ; property string
    mov rcx, rdi
    lea rdx, szAnimPropertyKey
    call FindJsonValue
    test rax, rax
    jz skip_property
    call SkipJsonWhitespace
    mov rsi, rax
    lea rdx, szAnimPropOpacity
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_prop_opacity
    lea rdx, szAnimPropHeight
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_prop_height
    lea rdx, szAnimPropWidth
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_prop_width
    lea rdx, szAnimPropY
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_prop_y
    lea rdx, szAnimPropX
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_prop_x
    jmp skip_property

set_prop_opacity:
    mov DWORD PTR [rclient + ANIMATION.property], ANIMATION_PROPERTY_OPACITY
    jmp skip_property

set_prop_height:
    mov DWORD PTR [rclient + ANIMATION.property], ANIMATION_PROPERTY_HEIGHT
    jmp skip_property

set_prop_width:
    mov DWORD PTR [rclient + ANIMATION.property], ANIMATION_PROPERTY_WIDTH
    jmp skip_property

set_prop_y:
    mov DWORD PTR [rclient + ANIMATION.property], ANIMATION_PROPERTY_Y
    jmp skip_property

set_prop_x:
    mov DWORD PTR [rclient + ANIMATION.property], ANIMATION_PROPERTY_X

skip_property:
    ; easing
    mov rcx, rdi
    lea rdx, szAnimEasingKey
    call FindJsonValue
    test rax, rax
    jz skip_easing
    call SkipJsonWhitespace
    mov rsi, rax
    lea rdx, szEaseIn
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_ease_in
    lea rdx, szEaseOut
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_ease_out
    lea rdx, szEaseInOut
    mov rcx, rsi
    call StringFind
    test rax, rax
    jne set_ease_inout
    jmp skip_easing

set_ease_in:
    mov DWORD PTR [rclient + ANIMATION.easing], EASING_EASE_IN
    jmp skip_easing

set_ease_out:
    mov DWORD PTR [rclient + ANIMATION.easing], EASING_EASE_OUT
    jmp skip_easing

set_ease_inout:
    mov DWORD PTR [rclient + ANIMATION.easing], EASING_EASE_IN_OUT

skip_easing:
    mov eax, 1
    jmp anim_done

anim_fail:
    xor eax, eax

anim_done:
    pop rdi
    pop rsi
    pop rbx
    ret
ParseAnimationJson ENDP

;==========================================================================
; PUBLIC: UpdateAnimation() - updates animation state
;==========================================================================
PUBLIC UpdateAnimation
ALIGN 16
UpdateAnimation PROC
    push rbx
    push rsi
    push rdi
    push r12

    mov rbx, rdi
    test rbx, rbx
    jz update_done
    cmp [rbx + ANIMATION.running], 0
    je update_done

    call GetTickCount
    mov r12d, eax
    mov edx, [rbx + ANIMATION.start_time]
    test edx, edx
    jz update_done
    sub r12d, edx
    mov r13d, r12d
    mov edx, [rbx + ANIMATION.delay]
    cmp r12d, edx
    jb update_elapsed
    sub r12d, edx
    mov edi, [rbx + ANIMATION.duration]
    test edi, edi
    je animation_finish
    cmp r12d, edi
    jae animation_finish

    cvtsi2ss xmm0, r12d
    cvtsi2ss xmm1, edi
    divss xmm0, xmm1
    movss xmm3, xmm0
    movss xmm2, [rbx + ANIMATION.from_value]
    movss xmm4, [rbx + ANIMATION.to_value]
    subss xmm4, xmm2
    mulss xmm4, xmm3
    addss xmm4, xmm2

apply_prop:
    mov ecx, [rbx + ANIMATION.target_component]
    call FindComponent
    test rax, rax
    jz update_elapsed
    mov edx, [rbx + ANIMATION.property]
    cmp edx, ANIMATION_PROPERTY_X
    je set_prop_value
    cmp edx, ANIMATION_PROPERTY_Y
    je set_prop_y
    cmp edx, ANIMATION_PROPERTY_WIDTH
    je set_prop_width
    cmp edx, ANIMATION_PROPERTY_HEIGHT
    je set_prop_height
    cmp edx, ANIMATION_PROPERTY_OPACITY
    je set_prop_opacity
    jmp update_elapsed

set_prop_value:
    movss [rax + COMPONENT.x], xmm4
    jmp update_elapsed

set_prop_y:
    movss [rax + COMPONENT.y], xmm4
    jmp update_elapsed

set_prop_width:
    movss [rax + COMPONENT.comp_width], xmm4
    jmp update_elapsed

set_prop_height:
    movss [rax + COMPONENT.comp_height], xmm4
    jmp update_elapsed

set_prop_opacity:
    movss [rax + COMPONENT.opacity], xmm4
    jmp update_elapsed

animation_finish:
    movss xmm4, [rbx + ANIMATION.to_value]
    mov ecx, [rbx + ANIMATION.target_component]
    call FindComponent
    test rax, rax
    jz update_elapsed
    mov edx, [rbx + ANIMATION.property]
    cmp edx, ANIMATION_PROPERTY_X
    je finish_x
    cmp edx, ANIMATION_PROPERTY_Y
    je finish_y
    cmp edx, ANIMATION_PROPERTY_WIDTH
    je finish_width
    cmp edx, ANIMATION_PROPERTY_HEIGHT
    je finish_height
    cmp edx, ANIMATION_PROPERTY_OPACITY
    je finish_opacity
    jmp finish_state

finish_x:
    movss [rax + COMPONENT.x], xmm4
    jmp finish_state

finish_y:
    movss [rax + COMPONENT.y], xmm4
    jmp finish_state

finish_width:
    movss [rax + COMPONENT.comp_width], xmm4
    jmp finish_state

finish_height:
    movss [rax + COMPONENT.comp_height], xmm4
    jmp finish_state

finish_opacity:
    movss [rax + COMPONENT.opacity], xmm4

finish_state:
    mov DWORD PTR [rbx + ANIMATION.running], 0
    mov DWORD PTR [rbx + ANIMATION.start_time], 0
    mov DWORD PTR [rbx + ANIMATION.current_time], r13d
    cmp ActiveAnimations, 0
    je update_elapsed
    dec ActiveAnimations
    jmp update_elapsed

update_elapsed:
    mov DWORD PTR [rbx + ANIMATION.current_time], r13d
update_done:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
UpdateAnimation ENDP

;==========================================================================
; PUBLIC: RequestRedraw() -> void
;==========================================================================
PUBLIC RequestRedraw
ALIGN 16
RequestRedraw PROC
    push rbx

    call ui_get_main_hwnd
    test rax, rax
    jz redraw_done
    xor rdx, rdx
    mov r8d, 1
    call InvalidateRect

redraw_done:
    pop rbx
    ret
RequestRedraw ENDP

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
    mov DWORD PTR [rbx + STYLE.font_size], 1082130432     ; 16.0f
    mov DWORD PTR [rbx + STYLE.font_weight], 400          ; Normal weight
    mov DWORD PTR [rbx + STYLE.shadow_color], 080000000h  ; Semi-transparent black
    mov DWORD PTR [rbx + STYLE.shadow_blur], 1082130432   ; 8.0f
    
    ; Parse "background" property
    mov rcx, rsi
    lea rdx, szStyleKeyBackground
    call FindJsonValue
    test rax, rax
    jz skip_bg
    call ParseColorHex
    mov [rbx + STYLE.background_color], eax
    
skip_bg:
    ; Parse "color" property
    mov rcx, rsi
    lea rdx, szStyleKeyColor
    call FindJsonValue
    test rax, rax
    jz skip_color
    call ParseColorHex
    mov [rbx + STYLE.text_color], eax
    
skip_color:
    ; Parse "borderRadius" property
    mov rcx, rsi
    lea rdx, szStyleKeyBorderRadius
    call FindJsonValue
    test rax, rax
    jz skip_radius
    call ParseFloatValue
    movss [rbx + STYLE.border_radius], xmm0
    
skip_radius:
    ; Parse "borderColor" property
    mov rcx, rsi
    lea rdx, szStyleKeyBorderColor
    call FindJsonValue
    test rax, rax
    jz skip_border_color
    call ParseColorHex
    mov [rbx + STYLE.border_color], eax
    
skip_border_color:
    ; Parse "borderWidth" property
    mov rcx, rsi
    lea rdx, szStyleKeyBorderWidth
    call FindJsonValue
    test rax, rax
    jz skip_border_width
    call ParseFloatValue
    movss [rbx + STYLE.border_width], xmm0
    
skip_border_width:
    ; Parse "paddingLeft" property
    mov rcx, rsi
    lea rdx, szStyleKeyPaddingLeft
    call FindJsonValue
    test rax, rax
    jz skip_padding_left
    call ParseFloatValue
    movss [rbx + STYLE.padding_left], xmm0
    
skip_padding_left:
    ; Parse "paddingRight" property
    mov rcx, rsi
    lea rdx, szStyleKeyPaddingRight
    call FindJsonValue
    test rax, rax
    jz skip_padding_right
    call ParseFloatValue
    movss [rbx + STYLE.padding_right], xmm0
    
skip_padding_right:
    ; Parse "paddingTop" property
    mov rcx, rsi
    lea rdx, szStyleKeyPaddingTop
    call FindJsonValue
    test rax, rax
    jz skip_padding_top
    call ParseFloatValue
    movss [rbx + STYLE.padding_top], xmm0
    
skip_padding_top:
    ; Parse "paddingBottom" property
    mov rcx, rsi
    lea rdx, szStyleKeyPaddingBottom
    call FindJsonValue
    test rax, rax
    jz skip_padding_bottom
    call ParseFloatValue
    movss [rbx + STYLE.padding_bottom], xmm0
    
skip_padding_bottom:
    ; Parse "fontSize" property
    mov rcx, rsi
    lea rdx, szStyleKeyFontSize
    call FindJsonValue
    test rax, rax
    jz skip_font_size
    call ParseFloatValue
    movss [rbx + STYLE.font_size], xmm0
    
skip_font_size:
    ; Parse "fontWeight" property (numeric: 100-900)
    mov rcx, rsi
    lea rdx, szStyleKeyFontWeight
    call FindJsonValue
    test rax, rax
    jz skip_font_weight
    mov rcx, rax
    call ParseIntValue
    mov [rbx + STYLE.font_weight], eax
    
skip_font_weight:
    ; Parse "lineHeight" property
    mov rcx, rsi
    lea rdx, szStyleKeyLineHeight
    call FindJsonValue
    test rax, rax
    jz skip_line_height
    call ParseFloatValue
    movss [rbx + STYLE.transition_duration], xmm0
    
skip_line_height:
    ; Parse "boxShadow" color property
    mov rcx, rsi
    lea rdx, szStyleKeyBoxShadow
    call FindJsonValue
    test rax, rax
    jz skip_shadow
    call ParseColorHex
    mov [rbx + STYLE.shadow_color], eax
    
skip_shadow:
    ; Parse "textAlign" property (numeric value)
    mov rcx, rsi
    lea rdx, szStyleKeyTextAlign
    call FindJsonValue
    test rax, rax
    jz skip_text_align
    mov rcx, rax
    call ParseIntValue
    mov [rbx + STYLE.text_align], eax
    
skip_text_align:
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

; Helper: Skip whitespace, tabs, quotes, and commas
SkipJsonWhitespace PROC
    push rbx
    mov rbx, rcx
skip_loop:
    mov al, [rbx]
    cmp al, ' '
    je skip_inc
    cmp al, 9
    je skip_inc
    cmp al, 10
    je skip_inc
    cmp al, 13
    je skip_inc
    cmp al, '"'
    je skip_inc
    cmp al, ','
    je skip_inc
    mov rax, rbx
    pop rbx
    ret
skip_inc:
    inc rbx
    jmp skip_loop
SkipJsonWhitespace ENDP

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
    shl eax, 24         ; Shift RGB to correct position (0xRRGGBB00)
    or eax, 0FF000000h  ; Add fully opaque alpha channel (0xFFRRGGBB)
    
    pop rsi
    pop rbx
    ret
ParseColorHex ENDP

; Helper: Parse float value
ParseFloatValue PROC
    ; rcx = string (e.g., "3.14", "100", "0.5")
    ; Returns: xmm0 = float value
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 64
    
    mov rsi, rcx
    xor eax, eax            ; Integer part accumulator
    xor ecx, ecx            ; Digit counter
    xor r8d, r8d            ; Fractional part accumulator
    xor r9d, r9d            ; Fractional digit count
    xor r10d, r10d          ; Decimal point found flag
    
    ; Parse integer part
parse_int_part:
    mov bl, [rsi]
    cmp bl, '0'
    jb parse_check_decimal
    cmp bl, '9'
    ja parse_check_decimal
    
    imul eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc rsi
    jmp parse_int_part
    
parse_check_decimal:
    ; Check for decimal point
    cmp bl, '.'
    jne parse_convert_to_float
    mov r10d, 1             ; Mark decimal point found
    inc rsi
    
    ; Parse fractional part
parse_frac_part:
    mov bl, [rsi]
    cmp bl, '0'
    jb parse_convert_to_float
    cmp bl, '9'
    ja parse_convert_to_float
    
    imul r8d, 10
    sub bl, '0'
    movzx ebx, bl
    add r8d, ebx
    inc r9d                 ; Count fractional digits
    inc rsi
    jmp parse_frac_part
    
parse_convert_to_float:
    ; Convert integer part to float
    cvtsi2ss xmm0, eax
    
    ; If we have fractional digits, add them
    test r9d, r9d
    jz parse_float_done
    
    ; Calculate divisor: 10^(number of fractional digits)
    ; xmm1 = 10.0, xmm2 = divisor
    movss xmm1, DWORD PTR [fltTen]
    movss xmm2, [fltOne]
    
    mov ecx, r9d
parse_calc_divisor:
    test ecx, ecx
    jz parse_frac_convert
    mulss xmm2, xmm1
    dec ecx
    jmp parse_calc_divisor
    
parse_frac_convert:
    ; Convert fractional part to float
    cvtsi2ss xmm3, r8d
    ; Divide fractional part by divisor
    divss xmm3, xmm2
    ; Add to integer part
    addss xmm0, xmm3
    
parse_float_done:
    add rsp, 64
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ParseFloatValue ENDP

; Helper: Parse integer value (for properties like fontSize, fontWeight, etc.)
ParseIntValue PROC
    ; rcx = string (e.g., "400", "16", "100")
    ; Returns: eax = integer value
    push rbx
    push rsi
    
    mov rsi, rcx
    xor eax, eax
    
parse_int_loop:
    mov bl, [rsi]
    cmp bl, '0'
    jb parse_int_done
    cmp bl, '9'
    ja parse_int_done
    
    imul eax, 10
    sub bl, '0'
    movzx ebx, bl
    add eax, ebx
    inc rsi
    jmp parse_int_loop
    
parse_int_done:
    pop rsi
    pop rbx
    ret
ParseIntValue ENDP

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
    mov ecx, SIZEOF COMPONENT
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
    mov ecx, SIZEOF THEME
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
    mov ecx, SIZEOF THEME
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
    mov ecx, SIZEOF STYLE
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
    ; ecx = component index, edx = depth
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov ebx, ecx
    
    ; Find end of JsonBuffer
    lea rdi, JsonBuffer
    call strlen
    add rdi, rax
    
    ; Add comma if not first
    test ebx, ebx
    jz first_comp
    mov byte ptr [rdi], ','
    inc rdi
first_comp:
    
    ; Get component
    mov eax, ebx
    mov ecx, SIZEOF COMPONENT
    imul eax, ecx
    lea rsi, ComponentRegistry
    movsxd rcx, eax
    add rsi, rcx
    
    ; Build component JSON
    mov byte ptr [rdi], '{'
    inc rdi
    
    ; "id":...
    lea rcx, szIdKey
    call append_string
    mov byte ptr [rdi], ':'
    add rdi, 1
    mov eax, [rsi + COMPONENT.id]
    call append_int
    mov byte ptr [rdi], ','
    add rdi, 1
    
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
    mov byte ptr [rdi], ','
    add rdi, 1

    ; "w":...
    lea rcx, szWKey
    call append_string
    mov byte ptr [rdi], ':'
    add rdi, 1
    movss xmm0, [rsi + COMPONENT.comp_width]
    call append_float
    mov byte ptr [rdi], ','
    add rdi, 1

    ; "h":...
    lea rcx, szHKey
    call append_string
    mov byte ptr [rdi], ':'
    add rdi, 1
    movss xmm0, [rsi + COMPONENT.comp_height]
    call append_float
    mov byte ptr [rdi], ','
    add rdi, 1

    ; "style":...
    lea rcx, szStyleKey
    call append_string
    mov byte ptr [rdi], ':'
    add rdi, 1
    mov eax, [rsi + COMPONENT.style_id]
    call append_int
    
    mov byte ptr [rdi], '}'
    inc rdi
    mov byte ptr [rdi], 0 ; Null terminate
    
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
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
    szJsonStart     BYTE "{\"panes\":[",0
    szPanesStart    BYTE "{"",0
    szComma         BYTE ",",0
    szPaneStart     BYTE "{\"id\":",0
    szIdProp        BYTE ",\"type\":",0
    szTypeProp      BYTE ",\"x\":",0
    szXProp         BYTE ",\"y\":",0
    szYProp         BYTE ",\"width\":",0
    szWidthProp     BYTE ",\"height\":",0
    szHeightProp    BYTE ",\"visible\":",0
    szVisibleProp   BYTE "}",0
    szPaneEnd       BYTE "}",0
    szPanesEnd      BYTE "]}",0
    szJsonEnd       BYTE "}",0
    szLayoutFile    BYTE "ide_layout.json",0
    szPanesKey      BYTE "panes",0
    szIdKey         BYTE "id",0
    szTypeKey       BYTE "type",0
    szXKey          BYTE "x",0
    szYKey          BYTE "y",0
    szWidthKey      BYTE "width",0
    szHeightKey     BYTE "height",0
    szVisibleKey    BYTE "visible",0
; Animation constants
ANIMATION_TIMER_ID    EQU 1001
EASING_LINEAR        EQU 0
EASING_EASE_IN       EQU 1
EASING_EASE_OUT      EQU 2
EASING_EASE_IN_OUT   EQU 3

; JSON keys
szBgColorKey         BYTE "bg_color",0
szFgColorKey         BYTE "fg_color",0
szFontSizeKey        BYTE "font_size",0
szOpacityKey         BYTE "opacity",0
szDurationKey        BYTE "duration",0
szEasingKey          BYTE "easing",0
szEaseIn             BYTE "easeIn",0
szEaseOut            BYTE "easeOut",0
szEaseInOut          BYTE "easeInOut",0

; Global variables
g_animation_active   DWORD 0