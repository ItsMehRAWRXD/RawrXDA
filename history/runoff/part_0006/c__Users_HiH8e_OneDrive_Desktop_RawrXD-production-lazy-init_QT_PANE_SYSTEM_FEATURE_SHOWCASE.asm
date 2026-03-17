; ============================================================================
; QT_PANE_SYSTEM_FEATURE_SHOWCASE.ASM
; Complete feature demonstration and usage patterns
; ============================================================================

; ============================================================================
; FEATURE 1: CREATE AND MANAGE MULTIPLE PANES
; ============================================================================

DEMO_CreatePanes proc
    ; Initialize system
    invoke PANE_SYSTEM_INIT
    
    ; Create File Explorer (Pane 1)
    invoke PANE_CREATE, offset szFileExplorer, PANE_TYPE_FILE_TREE, 200, 800, DOCK_LEFT
    mov pane1, eax
    
    ; Create Code Editor (Pane 2)
    invoke PANE_CREATE, offset szCodeEditor, PANE_TYPE_EDITOR, 800, 800, DOCK_CENTER
    mov pane2, eax
    
    ; Create AI Chat (Pane 3)
    invoke PANE_CREATE, offset szAIChat, PANE_TYPE_CHAT, 280, 800, DOCK_RIGHT
    mov pane3, eax
    
    ; Create Terminal (Pane 4)
    invoke PANE_CREATE, offset szTerminal, PANE_TYPE_TERMINAL, 800, 200, DOCK_BOTTOM
    mov pane4, eax
    
    ; Position panes (with automatic constraint application)
    invoke PANE_SETPOSITION, pane1, 0, 0, 200, 800
    invoke PANE_SETPOSITION, pane2, 200, 0, 800, 800
    invoke PANE_SETPOSITION, pane3, 1000, 0, 280, 800
    invoke PANE_SETPOSITION, pane4, 0, 800, 1280, 200
    
    ret
DEMO_CreatePanes endp

; ============================================================================
; FEATURE 2: VS CODE-STYLE LAYOUT
; ============================================================================

DEMO_VSCodeLayout proc
    ; Clear existing panes
    invoke PANE_SERIALIZATION_RESET
    
    ; Apply VS Code layout
    ; File Explorer (20%) | Editor (60%) | Chat (20%)
    invoke LAYOUT_APPLY_VSCODE, hMainWindow, 1280, 800
    
    ret
DEMO_VSCodeLayout endp

; ============================================================================
; FEATURE 3: WEBSTORM-STYLE LAYOUT
; ============================================================================

DEMO_WebStormLayout proc
    ; File Explorer (15%) | Editor/Terminal (70%/30%) | Chat (full height)
    invoke LAYOUT_APPLY_WEBSTORM, hMainWindow, 1280, 800
    
    ret
DEMO_WebStormLayout endp

; ============================================================================
; FEATURE 4: VISUAL STUDIO LAYOUT
; ============================================================================

DEMO_VisualStudioLayout proc
    ; Complex layout with properties panel
    invoke LAYOUT_APPLY_VISUALSTUDIO, hMainWindow, 1280, 800
    
    ret
DEMO_VisualStudioLayout endp

; ============================================================================
; FEATURE 5: AI CHAT CONFIGURATION
; ============================================================================

DEMO_ConfigureChat proc
    ; Initialize settings
    invoke SETTINGS_INIT
    
    ; Set AI model to Claude (most capable)
    invoke SETTINGS_SETCHATMODEL, offset szModelClaude
    
    ; Enable streaming for better UX
    invoke SETTINGS_TOGGLEFEATURE, 3    ; Feature 3 = streaming
    
    ; Configure chat pane height
    mov g_Settings.dwChatHeight, 400
    
    ; Enable chat pane
    invoke SETTINGS_TOGGLEFEATURE, 0    ; Feature 0 = chat enabled
    
    ret
DEMO_ConfigureChat endp

; ============================================================================
; FEATURE 6: POWERSHELL TERMINAL CONFIGURATION
; ============================================================================

DEMO_ConfigureTerminal proc
    ; Enable auto-execute mode (commands run as typed)
    invoke SETTINGS_TOGGLEFEATURE, 2    ; Feature 2 = auto-execute
    
    ; Set terminal height
    mov g_Settings.dwTerminalHeight, 250
    
    ; Show command prompt
    mov g_Settings.bShowPrompt, TRUE
    
    ; Enable terminal
    invoke SETTINGS_TOGGLEFEATURE, 1    ; Feature 1 = terminal enabled
    
    ret
DEMO_ConfigureTerminal endp

; ============================================================================
; FEATURE 7: PANE VISIBILITY CONTROL
; ============================================================================

DEMO_PaneVisibility proc
    LOCAL paneID:DWORD
    
    ; Show a pane
    invoke PANE_SETSTATE, paneID, PANE_STATE_VISIBLE
    
    ; Hide a pane (preserves space in layout)
    invoke PANE_SETSTATE, paneID, PANE_STATE_HIDDEN
    
    ; Minimize a pane (collapse to titlebar)
    invoke PANE_SETSTATE, paneID, PANE_STATE_MINIMIZED
    
    ; Maximize a pane (fill parent area)
    invoke PANE_SETSTATE, paneID, PANE_STATE_MAXIMIZED
    
    ret
DEMO_PaneVisibility endp

; ============================================================================
; FEATURE 8: SIZE CONSTRAINTS
; ============================================================================

DEMO_SizeConstraints proc
    LOCAL paneID:DWORD
    
    ; Set minimum and maximum sizes
    ; Min: 150x100, Max: 800x1200
    invoke PANE_SETCONSTRAINTS, paneID, 150, 100, 800, 1200
    
    ; When resizing, these constraints are automatically applied
    invoke PANE_SETPOSITION, paneID, 0, 0, 50, 50   ; Too small! Will be constrained to 150x100
    
    ret
DEMO_SizeConstraints endp

; ============================================================================
; FEATURE 9: CUSTOM PANE COLORS
; ============================================================================

DEMO_CustomColors proc
    LOCAL paneID:DWORD
    
    ; Dark background (common for IDEs)
    invoke PANE_SETCOLOR, paneID, 001E1E1Eh
    
    ; Light background
    invoke PANE_SETCOLOR, paneID, 00FFFFFFh
    
    ; Custom theme color
    invoke PANE_SETCOLOR, paneID, 00365F8Fh    ; VS Code blue
    
    ret
DEMO_CustomColors endp

; ============================================================================
; FEATURE 10: Z-ORDER (DEPTH) MANAGEMENT
; ============================================================================

DEMO_ZOrder proc
    LOCAL pane1:DWORD
    LOCAL pane2:DWORD
    LOCAL pane3:DWORD
    
    ; Set panes at different depths
    invoke PANE_SETZORDER, pane1, 0     ; Back layer
    invoke PANE_SETZORDER, pane2, 1     ; Middle layer
    invoke PANE_SETZORDER, pane3, 99    ; Front layer (always visible)
    
    ret
DEMO_ZOrder endp

; ============================================================================
; FEATURE 11: SAVE CUSTOM LAYOUT
; ============================================================================

DEMO_SaveLayout proc
    ; User creates their custom layout:
    ; 1. Arrange panes how they want
    ; 2. Resize to preferred sizes
    ; 3. Set colors and options
    
    ; Save it to file
    invoke PANE_SERIALIZATION_SAVE, offset szLayoutFile
    
    ; File format: Version header + all pane configurations
    
    ret
DEMO_SaveLayout endp

; ============================================================================
; FEATURE 12: LOAD PREVIOUS LAYOUT
; ============================================================================

DEMO_LoadLayout proc
    ; User can restore their saved layout
    invoke PANE_SERIALIZATION_LOAD, offset szLayoutFile
    
    ; All panes are recreated with exact:
    ; - Positions
    ; - Sizes
    ; - Visibility states
    ; - Colors
    ; - Z-order
    
    ret
DEMO_LoadLayout endp

; ============================================================================
; FEATURE 13: PANE ENUMERATION
; ============================================================================

DEMO_EnumeratePanes proc
    ; Enumerate all open panes
    invoke PANE_ENUMALL, offset EnumCallback
    
    ret
DEMO_EnumeratePanes endp

; Callback receives each pane ID
EnumCallback proc dwPaneID:DWORD
    ; Process pane (log, display in UI, etc.)
    mov eax, TRUE   ; Continue enumeration
    ret
EnumCallback endp

; ============================================================================
; FEATURE 14: DRAG AND RESIZE
; ============================================================================

DEMO_DragAndResize proc
    ; User clicks on pane divider
    ; System detects hit on divider and starts resize
    
    invoke PANE_STARTRESIZE, paneID, EDGE_RIGHT   ; Resizing from right edge
    
    ; Cursor changes to resize cursor
    invoke CURSOR_SETRESIZEH
    
    ; During drag, positions update continuously
    ; On mouse up:
    invoke PANE_ENDRESIZE
    
    ; Cursor returns to normal
    invoke CURSOR_SETDEFAULT
    
    ret
DEMO_DragAndResize endp

; ============================================================================
; FEATURE 15: CURSOR FEEDBACK
; ============================================================================

DEMO_CursorFeedback proc
    ; Horizontal resize (between panels)
    invoke CURSOR_SETRESIZEH
    
    ; Vertical resize (between stacked panels)
    invoke CURSOR_SETRESIZEV
    
    ; Moving/dragging pane
    invoke CURSOR_SETMOVE
    
    ; Return to normal
    invoke CURSOR_SETDEFAULT
    
    ret
DEMO_CursorFeedback endp

; ============================================================================
; FEATURE 16: SETTINGS UI
; ============================================================================

DEMO_SettingsUI proc
    LOCAL hTabControl:DWORD
    
    ; Create settings tab in main tab control
    invoke SETTINGS_CREATETAB, hTabControl, 0
    
    ; User can now see and interact with:
    ; - Layout preset dropdown (VS Code, WebStorm, Visual Studio, Custom)
    ; - Cursor style selector
    ; - AI model selector (GPT-4, Claude, Llama 2, Mistral, NeuralChat)
    ; - Chat pane toggles
    ; - Terminal toggles
    ; - Editor settings
    ; - Color scheme selector
    ; - Live preview of current settings
    
    ret
DEMO_SettingsUI endp

; ============================================================================
; FEATURE 17: QUERY PANE INFO
; ============================================================================

DEMO_QueryPaneInfo proc
    LOCAL paneInfo:PANE_INFO
    
    ; Get all information about a pane
    invoke PANE_GETINFO, paneID, offset paneInfo
    
    ; Now we have access to:
    mov eax, paneInfo.dwPaneID          ; Pane ID
    mov eax, paneInfo.x                 ; X position
    mov eax, paneInfo.y                 ; Y position
    mov eax, paneInfo.width             ; Width
    mov eax, paneInfo.height            ; Height
    mov eax, paneInfo.zOrder            ; Z-depth
    mov eax, paneInfo.dwPaneState       ; Visible/Hidden/etc
    mov eax, paneInfo.dwCustomColor     ; Background color
    mov eax, paneInfo.pszTitle          ; Pane title
    
    ret
DEMO_QueryPaneInfo endp

; ============================================================================
; FEATURE 18: SETTINGS PERSISTENCE
; ============================================================================

DEMO_SettingsPersistence proc
    ; Save current settings to file
    invoke SETTINGS_SAVECONFIGURATION, offset szSettingsFile
    
    ; On next application launch:
    invoke SETTINGS_LOADCONFIGURATION, offset szSettingsFile
    
    ; All settings are restored:
    ; - Chosen layout
    ; - Cursor style
    ; - Chat model
    ; - Terminal options
    ; - Editor preferences
    ; - Color scheme
    
    ret
DEMO_SettingsPersistence endp

; ============================================================================
; FEATURE 19: RESET TO DEFAULTS
; ============================================================================

DEMO_ResetDefaults proc
    ; Complete reset to default state
    invoke PANE_SERIALIZATION_RESET
    
    ; All panes closed
    ; System reinitialized
    ; Default VS Code layout applied
    
    ret
DEMO_ResetDefaults endp

; ============================================================================
; FEATURE 20: MULTI-AI MODEL SUPPORT
; ============================================================================

DEMO_AIModels proc
    ; Available models for selection in chat pane:
    
    ; Model 1: GPT-4 (OpenAI)
    invoke SETTINGS_SETCHATMODEL, offset szModelGPT4
    
    ; Model 2: Claude 3.5 Sonnet (Anthropic) - Most capable
    invoke SETTINGS_SETCHATMODEL, offset szModelClaude
    
    ; Model 3: Llama 2 (Meta - Open source)
    invoke SETTINGS_SETCHATMODEL, offset szModelLlama
    
    ; Model 4: Mistral 7B (Mistral AI - Fast)
    invoke SETTINGS_SETCHATMODEL, offset szModelMistral
    
    ; Model 5: NeuralChat (Intel - Optimized)
    invoke SETTINGS_SETCHATMODEL, offset szModelNeuralChat
    
    ret
DEMO_AIModels endp

; ============================================================================
; COMPLETE INTEGRATION EXAMPLE
; ============================================================================

DEMO_CompleteSetup proc
    ; 1. Initialize pane system
    invoke PANE_SYSTEM_INIT
    
    ; 2. Initialize settings
    invoke SETTINGS_INIT
    
    ; 3. Apply VS Code layout
    invoke LAYOUT_APPLY_VSCODE, hMainWindow, 1280, 800
    
    ; 4. Create settings tab
    invoke SETTINGS_CREATETAB, hTabControl, 0
    
    ; 5. Configure chat for AI
    invoke SETTINGS_SETCHATMODEL, offset szModelClaude
    invoke SETTINGS_TOGGLEFEATURE, 0    ; Enable chat
    invoke SETTINGS_TOGGLEFEATURE, 3    ; Enable streaming
    
    ; 6. Configure terminal
    invoke SETTINGS_TOGGLEFEATURE, 1    ; Enable terminal
    invoke SETTINGS_TOGGLEFEATURE, 2    ; Enable auto-execute
    
    ; 7. Load user's saved layout (if exists)
    invoke PANE_SERIALIZATION_LOAD, offset szUserLayout
    
    ; IDE is now fully configured and ready for use!
    
    ret
DEMO_CompleteSetup endp

; ============================================================================
; DATA
; ============================================================================

.data
    ; File paths
    szLayoutFile        db "layout.cfg", 0
    szSettingsFile      db "settings.cfg", 0
    szUserLayout        db "user_layout.cfg", 0
    
    ; Pane titles
    szFileExplorer      db "File Explorer", 0
    szCodeEditor        db "Code Editor", 0
    szAIChat            db "AI Chat", 0
    szTerminal          db "Terminal", 0
    
    ; AI Models
    szModelGPT4         db "GPT-4", 0
    szModelClaude       db "Claude 3.5 Sonnet", 0
    szModelLlama        db "Llama 2", 0
    szModelMistral      db "Mistral 7B", 0
    szModelNeuralChat   db "NeuralChat", 0
    
    ; Local variables
    pane1               dd 0
    pane2               dd 0
    pane3               dd 0
    pane4               dd 0
    paneID              dd 0

end
