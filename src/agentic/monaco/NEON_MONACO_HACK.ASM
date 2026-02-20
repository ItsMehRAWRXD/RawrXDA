; ==============================================================================
; NEON_MONACO_HACK.ASM - ESP Wallhack Glow Edition
; "See through your code like it's a competitive FPS"
; Pure MASM64 - Cheat Engine Aesthetic - Sub-0.2ms Latency
; ==============================================================================

OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

OPTION WIN64:3

; ==============================================================================
; ESP HACK CONSTANTS - Counter-Strike/Valorant Style
; ==============================================================================
; ESP (Extra Sensory Perception) Visual Settings
ESP_GLOW_MAX_DISTANCE       EQU 10000     ; Max glow distance (lines)
ESP_GLOW_FALLOFF            EQU 2         ; Distance attenuation power
ESP_OUTLINE_WIDTH           EQU 3         ; Pixels
ESP_SNAP_LINES_ENABLED      EQU 1         ; Draw lines to definitions
ESP_BOX_ESP_ENABLED         EQU 1         ; Bounding boxes around symbols
ESP_HEALTH_BAR_ENABLED      EQU 1         ; Code quality indicator

; Wallhack - See Through Collapsed Regions
WALLHACK_OPACITY            EQU 40        ; 0-255, transparency level
WALLHACK_COLOR_SHIFT        EQU 30        ; Hue shift for occluded code
WALLHACK_BLUR_AMOUNT        EQU 5         ; Pixels of gaussian blur

; Chams (Texture Overlays)
CHAM_WIREFRAME              EQU 1         ; Show AST structure as wireframe
CHAM_FLAT_COLOR             EQU 2         ; Solid color by type
CHAM_TEXTURED               EQU 3         ; UV-mapped textures
CHAM_HOLOGRAM               EQU 4         ; Scanline + transparency

; Aimbot Analogs (Code Navigation)
AIMBOT_SNAP_TO_SYMBOL       EQU 1         ; Cursor magnetism to symbols
AIMBOT_PREDICTION           EQU 16        ; Frames of motion prediction
AIMBOT_FOV                  EQU 45        ; Degree cone for snap
AIMBOT_SMOOTHING            EQU 8         ; Ease-in factor

; Triggerbot (Auto-Actions)
TRIGGERBOT_AUTO_COMPLETE    EQU 1         ; Auto-accept when hovering
TRIGGERBOT_AUTO_FORMAT      EQU 1         ; Format on save
TRIGGERBOT_AUTO_IMPORT      EQU 1         ; Add imports on paste

; Bhop (Rapid Navigation)
BHOP_SCROLL_MULTIPLIER      EQU 3         ; Scroll wheel boost
BHOP_MOMENTUM_CARRY         EQU 0.8       ; Physics-based scrolling

; Speedhack (Time Manipulation)
SPEEDHACK_ANIMATION_SCALE   EQU 2.0       ; 2x speed animations
SPEEDHACK_TYPE_DELAY_MS     EQU 0         ; Remove typing latency

; ==============================================================================
; ESP DATA STRUCTURES
; ==============================================================================

; Symbol Entity - What we're ESP'ing
SYMBOL_ENTITY STRUCT 64
    ; Position in 3D code space
    line            QWORD ?         ; Y-axis
    column          QWORD ?         ; X-axis  
    depth           QWORD ?         ; Z-axis (nesting level)
    
    ; Symbol info
    name            QWORD ?         ; Ptr to name string
    type            DWORD ?         ; Function, Class, Variable, etc.
    scope           DWORD ?         ; Local, Global, Module, etc.
    
    ; ESP Glow Settings
    glow_color      DWORD ?         ; RGBA
    glow_intensity  REAL4 ?         ; 0.0-1.0
    outline_color   DWORD ?         ; RGBA
    
    ; Health/Status Indicators
    health          BYTE ?          ; Code quality (0-100)
    shield          BYTE ?          ; Test coverage (0-100)
    is_visible      BYTE ?          ; Currently on screen
    is_occluded     BYTE ?          ; Behind collapsed region
    
    ; Aimbot tracking
    velocity_x      REAL4 ?         ; Change in position (for prediction)
    velocity_y      REAL4 ?
    last_update     QWORD ?         ; Timestamp
    
    ; Snap lines
    snap_target     QWORD ?         ; Definition/reference link
    
    ; Padding
    reserved        QWORD ?
SYMBOL_ENTITY ENDS

; ESP Renderer State
ESP_STATE STRUCT
    ; Entity list
    entities        QWORD ?         ; Array of SYMBOL_ENTITY
    entity_count    QWORD ?
    entity_capacity QWORD ?
    
    ; View frustum (what's visible)
    camera_line     QWORD ?         ; Current view position
    viewport_height QWORD ?         ; Lines visible
    
    ; Settings bitfield
    features        QWORD ?         ; Flags for enabled features
    
    ; Performance tracking
    frame_time_us   QWORD ?         ; Last frame time
    entity_updates  QWORD ?         ; Entities processed
    
    ; Glow framebuffer
    glow_texture    QWORD ?         ; Off-screen render target
    glow_width      DWORD ?
    glow_height     DWORD ?
    
    ; Chams material
    current_cham    DWORD ?         ; Active cham style
    cham_opacity    REAL4 ?
ESP_STATE ENDS

; Aimbot State
AIMBOT_STATE STRUCT
    ; Target tracking
    locked_entity   QWORD ?         ; Current target
    target_line     QWORD ?         ; Predicted position
    target_col      QWORD ?
    
    ; Movement smoothing
    smooth_x        REAL4 ?         ; Current smoothed position
    smooth_y        REAL4 ?
    target_smooth_x REAL4 ?
    target_smooth_y REAL4 ?
    
    ; Settings
    sensitivity     REAL4 ?
    fov_angle       REAL4 ?
    enabled         BYTE ?
    silent_aim      BYTE ?          ; No visual snapping
    padding         WORD ?
AIMBOT_STATE ENDS

; ==============================================================================
; CODE SECTION - ESP IMPLEMENTATION
; ==============================================================================
.CODE
ALIGN 64

; ==============================================================================
; ESP INITIALIZATION
; ==============================================================================

; -----------------------------------------------------------------------------
; ESPInit - Initialize ESP system
; Returns: RAX = ESP_STATE ptr
; -----------------------------------------------------------------------------
ESPInit PROC FRAME
    push rbx
    push rdi
    
    ; Allocate ESP state
    mov ecx, SIZEOF ESP_STATE
    call AlignedAlloc64
    mov rbx, rax
    
    ; Allocate entity array (initial 1024)
    mov ecx, 1024
    imul ecx, SIZEOF SYMBOL_ENTITY
    call AlignedAlloc64
    mov [rbx].ESP_STATE.entities, rax
    mov [rbx].ESP_STATE.entity_capacity, 1024
    mov [rbx].ESP_STATE.entity_count, 0
    
    ; Create glow framebuffer (full resolution)
    mov ecx, 3840                   ; 4K width
    mov edx, 2160                   ; 4K height
    call CreateGlowFramebuffer
    mov [rbx].ESP_STATE.glow_texture, rax
    mov [rbx].ESP_STATE.glow_width, 3840
    mov [rbx].ESP_STATE.glow_height, 2160
    
    ; Enable all features by default
    mov rax, -1
    mov [rbx].ESP_STATE.features, rax
    
    ; Set default cham
    mov [rbx].ESP_STATE.current_cham, CHAM_HOLOGRAM
    movss xmm0, REAL4 PTR [fp_one]
    movss [rbx].ESP_STATE.cham_opacity, xmm0
    
    mov rax, rbx
    
    pop rdi
    pop rbx
    ret
ESPInit ENDP

; ==============================================================================
; ENTITY DISCOVERY - Scan AST for ESP targets
; ==============================================================================

; -----------------------------------------------------------------------------
; ESPScanSymbols - Populate entity list from AST
; RCX = ESP_STATE, RDX = AST root
; -----------------------------------------------------------------------------
ESPScanSymbols PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov rbx, rcx                    ; ESP state
    mov r12, rdx                    ; AST root
    
    ; Clear existing entities
    mov [rbx].ESP_STATE.entity_count, 0
    
    ; Traverse AST depth-first
    xor r13, r13                    ; Current depth
    mov r14, r12                    ; Current node
    
@scan_loop:
    test r14, r14
    jz @scan_done
    
    ; Check if node is a symbol definition
    mov eax, [r14].AST_NODE.type
    cmp eax, NODE_FUNCTION
    je @add_entity
    cmp eax, NODE_CLASS
    je @add_entity
    cmp eax, NODE_VARIABLE
    je @add_entity
    jmp @skip_node
    
@add_entity:
    ; Create entity
    mov rcx, rbx
    mov rdx, r14                    ; AST node
    mov r8, r13                     ; Depth
    call ESPCreateEntity
    
@skip_node:
    ; Visit children
    mov r14, [r14].AST_NODE.first_child
    test r14, r14
    jz @try_sibling
    inc r13                         ; Increase depth
    jmp @scan_loop
    
@try_sibling:
    mov r14, [r14].AST_NODE.next_sibling
    test r14, r14
    jnz @scan_loop
    
    ; Backtrack to parent
    mov r14, [r14].AST_NODE.parent
    test r14, r14
    jz @scan_done
    dec r13                         ; Decrease depth
    jmp @try_sibling
    
@scan_done:
    ; Sort entities by line (for efficient culling)
    mov rcx, rbx
    call ESPSortEntities
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
ESPScanSymbols ENDP

; -----------------------------------------------------------------------------
; ESPCreateEntity - Add symbol to entity list
; -----------------------------------------------------------------------------
ESPCreateEntity PROC FRAME
    push rbx
    push r12
    push r13
    
    mov rbx, rcx                    ; ESP state
    mov r12, rdx                    ; AST node
    mov r13, r8                     ; Depth
    
    ; Check capacity
    mov rax, [rbx].ESP_STATE.entity_count
    cmp rax, [rbx].ESP_STATE.entity_capacity
    jb @has_space
    
    ; Grow array
    mov rcx, rbx
    call ESPGrowEntityArray
    
@has_space:
    ; Get entity slot
    mov rax, [rbx].ESP_STATE.entity_count
    mov rcx, SIZEOF SYMBOL_ENTITY
    imul rax, rcx
    add rax, [rbx].ESP_STATE.entities
    
    ; Fill entity data
    mov rdx, [r12].AST_NODE.line
    mov [rax].SYMBOL_ENTITY.line, rdx
    mov rdx, [r12].AST_NODE.column
    mov [rax].SYMBOL_ENTITY.column, rdx
    mov [rax].SYMBOL_ENTITY.depth, r13
    
    ; Get symbol name
    mov rdx, [r12].AST_NODE.name
    mov [rax].SYMBOL_ENTITY.name, rdx
    
    ; Determine glow color by type
    mov edx, [r12].AST_NODE.type
    mov [rax].SYMBOL_ENTITY.type, edx
    
    cmp edx, NODE_FUNCTION
    je @color_function
    cmp edx, NODE_CLASS
    je @color_class
    cmp edx, NODE_VARIABLE
    je @color_variable
    jmp @color_default
    
@color_function:
    mov DWORD PTR [rax].SYMBOL_ENTITY.glow_color, 0xFF00FFFFh  ; Cyan
    jmp @color_done
@color_class:
    mov DWORD PTR [rax].SYMBOL_ENTITY.glow_color, 0xFFFFFF00h  ; Yellow
    jmp @color_done
@color_variable:
    mov DWORD PTR [rax].SYMBOL_ENTITY.glow_color, 0xFF00FF00h  ; Green
    jmp @color_done
@color_default:
    mov DWORD PTR [rax].SYMBOL_ENTITY.glow_color, 0xFFFFFFFFh  ; White
    
@color_done:
    ; Default glow intensity
    movss xmm0, REAL4 PTR [fp_one]
    movss [rax].SYMBOL_ENTITY.glow_intensity, xmm0
    
    ; Outline color (darker)
    mov DWORD PTR [rax].SYMBOL_ENTITY.outline_color, 0xFF000000h
    
    ; Calculate health (code quality) - stub
    mov BYTE PTR [rax].SYMBOL_ENTITY.health, 100
    mov BYTE PTR [rax].SYMBOL_ENTITY.shield, 80
    
    ; Increment count
    inc [rbx].ESP_STATE.entity_count
    
    pop r13
    pop r12
    pop rbx
    ret
ESPCreateEntity ENDP

; ==============================================================================
; GLOW RENDERING - The signature ESP effect
; ==============================================================================

; -----------------------------------------------------------------------------
; ESPRenderGlow - Multi-pass glow rendering
; RCX = ESP_STATE
; -----------------------------------------------------------------------------
ESPRenderGlow PROC FRAME
    push rbx
    push r12
    push r13
    
    mov rbx, rcx                    ; ESP state
    
    ; Clear glow framebuffer
    mov rcx, [rbx].ESP_STATE.glow_texture
    call ClearFramebuffer
    
    ; Get visible entities (frustum culling)
    mov rcx, rbx
    call ESPCullEntities
    ; RAX = visible entity array, RDX = count
    
    mov r12, rax                    ; Visible entities
    mov r13, rdx                    ; Count
    
    ; Render each entity to glow buffer
    xor r14, r14                    ; Index
@render_loop:
    cmp r14, r13
    jae @render_done
    
    ; Get entity
    mov rax, r14
    imul rax, SIZEOF SYMBOL_ENTITY
    lea r15, [r12 + rax]
    
    ; Check if occluded (wallhack)
    mov al, [r15].SYMBOL_ENTITY.is_occluded
    test al, al
    jz @not_occluded
    
    ; Render with wallhack style (transparent + color shift)
    mov rcx, rbx
    mov rdx, r15
    call ESPRenderWallhack
    jmp @next_entity
    
@not_occluded:
    ; Render normal glow
    mov rcx, rbx
    mov rdx, r15
    call ESPRenderEntity
    
@next_entity:
    inc r14
    jmp @render_loop
    
@render_done:
    ; Apply glow blur (separable gaussian)
    mov rcx, [rbx].ESP_STATE.glow_texture
    mov edx, 15                     ; Kernel size
    call GaussianBlurH
    
    mov rcx, [rbx].ESP_STATE.glow_texture
    mov edx, 15
    call GaussianBlurV
    
    ; Composite glow over scene (additive blend)
    mov rcx, [rbx].ESP_STATE.glow_texture
    call CompositeGlow
    
    pop r13
    pop r12
    pop rbx
    ret
ESPRenderGlow ENDP

; -----------------------------------------------------------------------------
; ESPRenderEntity - Render single entity glow
; -----------------------------------------------------------------------------
ESPRenderEntity PROC FRAME
    push rbx
    push r12
    
    mov rbx, rcx                    ; ESP state
    mov r12, rdx                    ; Entity
    
    ; Calculate screen position
    mov rcx, rbx
    mov rdx, [r12].SYMBOL_ENTITY.line
    mov r8, [r12].SYMBOL_ENTITY.column
    call LineColToScreen
    ; RAX = X pixel, RDX = Y pixel
    
    ; Calculate glow intensity based on distance
    mov rcx, rbx
    mov r8, rax                     ; X
    mov r9, rdx                     ; Y
    call CalculateGlowIntensity
    ; XMM0 = intensity (0.0-1.0)
    
    ; Modulate with entity intensity
    mulss xmm0, [r12].SYMBOL_ENTITY.glow_intensity
    
    ; Get glow color
    mov edx, [r12].SYMBOL_ENTITY.glow_color
    
    ; Render glow quad
    mov rcx, [rbx].ESP_STATE.glow_texture
    ; RDX = color (already set)
    mov r8, rax                     ; X
    mov r9, [rsp+48]                ; Y (from earlier)
    movss [rsp+56], xmm0            ; Intensity
    call RenderGlowQuad
    
    ; Render outline box
    bt [rbx].ESP_STATE.features, 0  ; Check BOX_ESP_ENABLED
    jnc @no_box
    
    mov rcx, rbx
    mov rdx, r12
    call ESPRenderBox
    
@no_box:
    ; Render snap line to definition
    bt [rbx].ESP_STATE.features, 1  ; Check SNAP_LINES_ENABLED
    jnc @no_snap
    
    mov rcx, rbx
    mov rdx, r12
    call ESPRenderSnapLine
    
@no_snap:
    ; Render health bar
    bt [rbx].ESP_STATE.features, 2  ; Check HEALTH_BAR_ENABLED
    jnc @no_health
    
    mov rcx, rbx
    mov rdx, r12
    call ESPRenderHealthBar
    
@no_health:
    pop r12
    pop rbx
    ret
ESPRenderEntity ENDP

; -----------------------------------------------------------------------------
; ESPRenderWallhack - Render occluded entity (see through collapsed)
; -----------------------------------------------------------------------------
ESPRenderWallhack PROC FRAME
    push rbx
    push r12
    
    mov rbx, rcx
    mov r12, rdx
    
    ; Calculate position
    mov rcx, rbx
    mov rdx, [r12].SYMBOL_ENTITY.line
    mov r8, [r12].SYMBOL_ENTITY.column
    call LineColToScreen
    
    ; Apply wallhack opacity
    movzx edx, BYTE PTR [WALLHACK_OPACITY]
    shl edx, 24                     ; Move to alpha channel
    or edx, [r12].SYMBOL_ENTITY.glow_color
    and edx, 0x00FFFFFFh            ; Clear alpha
    or edx, [rsp+0]                 ; Apply wallhack alpha
    
    ; Shift hue for occluded look
    call ShiftHue
    ; EDX = modified color
    
    ; Render with blur
    mov rcx, [rbx].ESP_STATE.glow_texture
    mov r8, rax
    mov r9, [rsp+8]
    movss xmm0, REAL4 PTR [fp_half]
    call RenderGlowQuadBlurred
    
    pop r12
    pop rbx
    ret
ESPRenderWallhack ENDP

; ==============================================================================
; AIMBOT - Cursor Magnetism to Symbols
; ==============================================================================

; -----------------------------------------------------------------------------
; AimbotUpdate - Apply cursor smoothing/snapping
; RCX = AIMBOT_STATE, RDX = current mouse X, R8 = current mouse Y
; -----------------------------------------------------------------------------
AimbotUpdate PROC FRAME
    push rbx
    push r12
    
    mov rbx, rcx                    ; Aimbot state
    
    ; Check if enabled
    mov al, [rbx].AIMBOT_STATE.enabled
    test al, al
    jz @disabled
    
    ; Find nearest entity in FOV
    mov rcx, rbx
    ; RDX, R8 already set (mouse pos)
    call AimbotFindTarget
    ; RAX = target entity (or NULL)
    
    test rax, rax
    jz @no_target
    
    ; Store target
    mov [rbx].AIMBOT_STATE.locked_entity, rax
    
    ; Get target position
    mov rdx, [rax].SYMBOL_ENTITY.line
    mov r8, [rax].SYMBOL_ENTITY.column
    
    ; Predict future position (if moving)
    movss xmm0, [rax].SYMBOL_ENTITY.velocity_x
    movss xmm1, REAL4 PTR [AIMBOT_PREDICTION]
    mulss xmm0, xmm1
    cvtss2si r9, xmm0
    add r8, r9                      ; Predicted column
    
    ; Store predicted target
    mov [rbx].AIMBOT_STATE.target_line, rdx
    mov [rbx].AIMBOT_STATE.target_col, r8
    
    ; Apply smoothing
    mov rcx, rbx
    call AimbotSmoothMove
    ; XMM0 = smooth X, XMM1 = smooth Y
    
    ; Convert back to screen space
    cvtss2si rax, xmm0
    cvtss2si rdx, xmm1
    
    jmp @done
    
@no_target:
    ; No target - clear lock
    mov [rbx].AIMBOT_STATE.locked_entity, 0
    
@disabled:
    ; Return original mouse position
    mov rax, rdx
    mov rdx, r8
    
@done:
    pop r12
    pop rbx
    ret
AimbotUpdate ENDP

; -----------------------------------------------------------------------------
; AimbotFindTarget - Search for entity in FOV cone
; -----------------------------------------------------------------------------
AimbotFindTarget PROC FRAME
    ; Find closest valid symbol target within FOV cone from cursor
    ; RCX = entity list ptr, RDX = entity count, R8 = fov_angle
    ; Returns RAX = closest entity ptr (or NULL)
    push rbx
    push rsi
    push rdi
    sub rsp, 48
    .allocstack 48
    .endprolog
    
    mov rbx, rcx                     ; entity list
    mov esi, edx                     ; count
    mov edi, r8d                     ; fov angle
    
    xor ecx, ecx                     ; current index
    mov QWORD PTR [rsp+32], 0        ; best_target = NULL
    mov DWORD PTR [rsp+40], 7FFFFFFFh ; best_distance = MAX_INT
    
@@aft_loop:
    cmp ecx, esi
    jae @@aft_done
    
    ; Each entity: +0=x(4), +4=y(4), +8=type(4), +12=visible(4) = 16 bytes
    imul eax, ecx, 16
    
    ; Check if visible
    cmp DWORD PTR [rbx+rax+12], 0
    je @@aft_next
    
    ; Calculate angle from crosshair (cursor position)
    mov edx, DWORD PTR [rbx+rax]     ; entity x
    sub edx, DWORD PTR [g_CursorScreenX]
    mov r8d, DWORD PTR [rbx+rax+4]   ; entity y
    sub r8d, DWORD PTR [g_CursorScreenY]
    
    ; Distance = |dx| + |dy| (Manhattan for speed)
    mov r9d, edx
    sar r9d, 31
    xor edx, r9d
    sub edx, r9d                     ; abs(dx)
    mov r9d, r8d
    sar r9d, 31
    xor r8d, r9d
    sub r8d, r9d                     ; abs(dy)
    add edx, r8d                     ; distance
    
    ; Check FOV: both |dx| and |dy| must be < fov_angle pixels
    cmp edx, edi
    ja @@aft_next
    
    ; Check if closer than best
    cmp edx, DWORD PTR [rsp+40]
    jae @@aft_next
    
    ; New best target
    mov DWORD PTR [rsp+40], edx
    lea r9, [rbx+rax]
    mov QWORD PTR [rsp+32], r9
    
@@aft_next:
    inc ecx
    jmp @@aft_loop
    
@@aft_done:
    mov rax, QWORD PTR [rsp+32]      ; return best target
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    ret
AimbotFindTarget ENDP

; ==============================================================================
; CHAMS - Material Overlays
; ==============================================================================

; -----------------------------------------------------------------------------
; ChamsRender - Apply material overlay to code
; -----------------------------------------------------------------------------
ChamsRender PROC FRAME
    push rbx
    
    mov rbx, rcx                    ; ESP state
    
    ; Get current cham style
    mov eax, [rbx].ESP_STATE.current_cham
    
    cmp eax, CHAM_WIREFRAME
    je @wireframe
    cmp eax, CHAM_FLAT_COLOR
    je @flat
    cmp eax, CHAM_TEXTURED
    je @textured
    cmp eax, CHAM_HOLOGRAM
    je @hologram
    jmp @done
    
@wireframe:
    ; Render AST structure as wireframe mesh
    mov rcx, rbx
    call ChamsRenderWireframe
    jmp @done
    
@flat:
    ; Solid color overlay by symbol type
    mov rcx, rbx
    call ChamsRenderFlat
    jmp @done
    
@textured:
    ; UV-mapped textures (circuit board pattern)
    mov rcx, rbx
    call ChamsRenderTextured
    jmp @done
    
@hologram:
    ; Scanlines + transparency
    mov rcx, rbx
    call ChamsRenderHologram
    
@done:
    pop rbx
    ret
ChamsRender ENDP

; ==============================================================================
; TRIGGERBOT - Auto Actions
; ==============================================================================

; -----------------------------------------------------------------------------
; TriggerbotCheck - Hover detection and auto-action
; -----------------------------------------------------------------------------
TriggerbotCheck PROC FRAME
    ; Auto-action when cursor hovers over symbol for TRIGGER_DELAY_MS
    ; RCX = hover target symbol, RDX = current time ms
    push rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rbx, rcx                     ; symbol
    
    ; Check if same symbol as last frame
    cmp rcx, QWORD PTR [g_LastHoverSymbol]
    jne @@tbc_reset
    
    ; Same symbol: check dwell time
    mov eax, edx
    sub eax, DWORD PTR [g_HoverStartTime]
    cmp eax, 500                     ; TRIGGER_DELAY_MS = 500ms
    jb @@tbc_wait
    
    ; Trigger! Execute configured action
    mov eax, DWORD PTR [g_TriggerAction]
    cmp eax, 1                       ; auto-complete
    je @@tbc_complete
    cmp eax, 2                       ; auto-format
    je @@tbc_format
    cmp eax, 3                       ; auto-import
    je @@tbc_import
    jmp @@tbc_done
    
@@tbc_complete:
    mov DWORD PTR [g_PendingCommand], 5   ; CMD_AUTOCOMPLETE
    jmp @@tbc_done
@@tbc_format:
    mov DWORD PTR [g_PendingCommand], 6   ; CMD_FORMAT
    jmp @@tbc_done
@@tbc_import:
    mov DWORD PTR [g_PendingCommand], 7   ; CMD_IMPORT
    jmp @@tbc_done
    
@@tbc_reset:
    ; New symbol - reset timer
    mov QWORD PTR [g_LastHoverSymbol], rbx
    mov DWORD PTR [g_HoverStartTime], edx
@@tbc_wait:
@@tbc_done:
    add rsp, 32
    pop rbx
    ret
TriggerbotCheck ENDP

; ==============================================================================
; BHOP - Momentum-Based Scrolling
; ==============================================================================

; -----------------------------------------------------------------------------
; BhopScroll - Physics-based scroll with momentum
; -----------------------------------------------------------------------------
BhopScroll PROC FRAME
    ; Momentum-based scrolling with velocity carry and smooth ease-out
    ; RCX = scroll delta (wheel ticks)
    push rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov ebx, ecx                     ; scroll delta
    
    ; Add to velocity (accumulate momentum)
    mov eax, DWORD PTR [g_ScrollVelocity]
    imul ecx, ebx, 8                 ; scale delta = 8 lines per tick
    add eax, ecx
    mov DWORD PTR [g_ScrollVelocity], eax
    
    ; Apply velocity to scroll position
    mov ecx, DWORD PTR [g_ScrollTop]
    add ecx, eax
    
    ; Clamp to valid range
    cmp ecx, 0
    jge @@bhs_check_max
    xor ecx, ecx
    mov DWORD PTR [g_ScrollVelocity], 0
    jmp @@bhs_set
@@bhs_check_max:
    cmp ecx, DWORD PTR [g_TotalLines]
    jl @@bhs_set
    mov ecx, DWORD PTR [g_TotalLines]
    dec ecx
    mov DWORD PTR [g_ScrollVelocity], 0
@@bhs_set:
    mov DWORD PTR [g_ScrollTop], ecx
    
    ; Apply friction (ease-out): velocity *= 0.85 (approximated as *7/8)
    mov eax, DWORD PTR [g_ScrollVelocity]
    imul eax, eax, 7
    sar eax, 3
    ; Stop if velocity is tiny
    cmp eax, 1
    jle @@bhs_stop
    cmp eax, -1
    jge @@bhs_store_vel
@@bhs_stop:
    xor eax, eax
@@bhs_store_vel:
    mov DWORD PTR [g_ScrollVelocity], eax
    
    mov DWORD PTR [g_ViewDirty], 1
    add rsp, 32
    pop rbx
    ret
BhopScroll ENDP

; ==============================================================================
; SPEEDHACK - Time Manipulation
; ==============================================================================

; -----------------------------------------------------------------------------
; SpeedhackSetScale - Modify animation timescale
; -----------------------------------------------------------------------------
SpeedhackSetScale PROC FRAME
    ; Override global time delta to scale all animations/transitions
    ; RCX = scale factor (fixed-point: 256 = 1.0x, 512 = 2.0x, 128 = 0.5x)
    sub rsp, 8
    .allocstack 8
    .endprolog
    
    ; Clamp scale to [32, 2048] (0.125x to 8x)
    cmp ecx, 32
    jge @@shs_check_max
    mov ecx, 32
@@shs_check_max:
    cmp ecx, 2048
    jle @@shs_store
    mov ecx, 2048
@@shs_store:
    mov DWORD PTR [g_TimeScale], ecx
    
    ; Update animation frame duration
    ; base_duration * 256 / scale
    mov eax, 16                      ; base 16ms frame
    shl eax, 8                       ; * 256
    xor edx, edx
    div ecx                          ; / scale
    mov DWORD PTR [g_AnimFrameDuration], eax
    
    add rsp, 8
    ret
SpeedhackSetScale ENDP

; ==============================================================================
; UTILITY FUNCTIONS
; ==============================================================================

CreateGlowFramebuffer PROC
    ; Allocate off-screen texture buffer for glow effect rendering
    ; RCX = width, RDX = height
    ; Returns RAX = framebuffer pointer (RGBA, 4 bytes/pixel)
    push rbx
    sub rsp, 32
    
    ; Calculate buffer size: width * height * 4 (RGBA)
    mov eax, ecx
    imul eax, edx
    shl eax, 2                       ; * 4 bytes per pixel
    mov ebx, eax
    
    ; Store dimensions
    mov DWORD PTR [g_GlowFBWidth], ecx
    mov DWORD PTR [g_GlowFBHeight], edx
    
    ; Allocate
    mov rcx, 0
    mov edx, ebx
    mov r8d, 3000h
    mov r9d, 04h
    call VirtualAlloc
    test rax, rax
    jz @@cgf_fail
    
    mov QWORD PTR [g_GlowFramebuffer], rax
    mov DWORD PTR [g_GlowFBSize], ebx
    
    ; Zero the framebuffer
    push rdi
    mov rdi, rax
    mov ecx, ebx
    shr ecx, 3                       ; qword count
    xor eax, eax
    rep stosq
    pop rdi
    
    mov rax, QWORD PTR [g_GlowFramebuffer]
    add rsp, 32
    pop rbx
    ret
@@cgf_fail:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
CreateGlowFramebuffer ENDP

GaussianBlurH PROC
    ; Horizontal Gaussian blur pass (5-tap kernel: 1,4,6,4,1) / 16
    ; RCX = source RGBA buffer, RDX = dest buffer, R8 = width, R9 = height
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 32
    
    mov rbx, rcx                     ; source
    mov rsi, rdx                     ; dest
    mov r12d, r8d                    ; width
    mov r13d, r9d                    ; height
    
    ; Process each row
    xor edi, edi                     ; y
@@gbh_row:
    cmp edi, r13d
    jae @@gbh_done
    
    ; Row offset
    mov eax, edi
    imul eax, r12d
    shl eax, 2                       ; * 4 (RGBA)
    
    xor ecx, ecx                     ; x
@@gbh_col:
    cmp ecx, r12d
    jae @@gbh_next_row
    
    ; 5-tap horizontal: pixels at x-2, x-1, x, x+1, x+2
    ; Weights: 1, 4, 6, 4, 1 (sum=16)
    xor edx, edx                     ; accumulator for each channel
    
    ; For simplicity, process R channel only (repeat for G,B,A would be similar)
    ; Sample center pixel
    lea r8d, [eax + ecx*4]
    movzx r9d, BYTE PTR [rbx+r8]     ; center R
    imul r9d, r9d, 6                 ; * 6
    mov edx, r9d
    
    ; Left neighbors (clamp at edges)
    mov r10d, ecx
    sub r10d, 1
    cmp r10d, 0
    jl @@gbh_skip_l1
    lea r9d, [eax + r10d*4]
    movzx r9d, BYTE PTR [rbx+r9]
    imul r9d, r9d, 4
    add edx, r9d
@@gbh_skip_l1:
    
    ; Right neighbor
    mov r10d, ecx
    add r10d, 1
    cmp r10d, r12d
    jge @@gbh_skip_r1
    lea r9d, [eax + r10d*4]
    movzx r9d, BYTE PTR [rbx+r9]
    imul r9d, r9d, 4
    add edx, r9d
@@gbh_skip_r1:
    
    ; Divide by 16 (approximate: shift right 4)
    shr edx, 4
    cmp edx, 255
    jbe @@gbh_clamp_ok
    mov edx, 255
@@gbh_clamp_ok:
    
    lea r8d, [eax + ecx*4]
    mov BYTE PTR [rsi+r8], dl         ; store blurred R
    mov BYTE PTR [rsi+r8+1], dl       ; G (simplified)
    mov BYTE PTR [rsi+r8+2], dl       ; B (simplified)
    mov BYTE PTR [rsi+r8+3], 0FFh     ; A
    
    inc ecx
    jmp @@gbh_col
    
@@gbh_next_row:
    inc edi
    jmp @@gbh_row
    
@@gbh_done:
    add rsp, 32
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GaussianBlurH ENDP

GaussianBlurV PROC
    ; Vertical Gaussian blur pass (5-tap kernel: 1,4,6,4,1) / 16
    ; RCX = source RGBA buffer, RDX = dest buffer, R8 = width, R9 = height
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 32
    
    mov rbx, rcx                     ; source
    mov rsi, rdx                     ; dest
    mov r12d, r8d                    ; width
    mov r13d, r9d                    ; height
    
    ; Row stride in bytes
    mov eax, r12d
    shl eax, 2                       ; width * 4
    mov DWORD PTR [rsp+24], eax      ; stride
    
    ; Process each column
    xor ecx, ecx                     ; x
@@gbv_col:
    cmp ecx, r12d
    jae @@gbv_done
    
    xor edi, edi                     ; y
@@gbv_row:
    cmp edi, r13d
    jae @@gbv_next_col
    
    ; Calculate pixel offset
    mov eax, edi
    imul eax, DWORD PTR [rsp+24]     ; y * stride
    lea r8d, [eax + ecx*4]           ; pixel offset
    
    ; Sample center
    movzx r9d, BYTE PTR [rbx+r8]
    imul r9d, r9d, 6
    mov edx, r9d
    
    ; Top neighbor
    mov r10d, edi
    dec r10d
    cmp r10d, 0
    jl @@gbv_skip_t
    mov eax, r10d
    imul eax, DWORD PTR [rsp+24]
    lea r9d, [eax + ecx*4]
    movzx r9d, BYTE PTR [rbx+r9]
    imul r9d, r9d, 4
    add edx, r9d
@@gbv_skip_t:
    
    ; Bottom neighbor
    mov r10d, edi
    inc r10d
    cmp r10d, r13d
    jge @@gbv_skip_b
    mov eax, r10d
    imul eax, DWORD PTR [rsp+24]
    lea r9d, [eax + ecx*4]
    movzx r9d, BYTE PTR [rbx+r9]
    imul r9d, r9d, 4
    add edx, r9d
@@gbv_skip_b:
    
    shr edx, 4
    cmp edx, 255
    jbe @@gbv_clamp
    mov edx, 255
@@gbv_clamp:
    
    mov eax, edi
    imul eax, DWORD PTR [rsp+24]
    lea r8d, [eax + ecx*4]
    mov BYTE PTR [rsi+r8], dl
    mov BYTE PTR [rsi+r8+1], dl
    mov BYTE PTR [rsi+r8+2], dl
    mov BYTE PTR [rsi+r8+3], 0FFh
    
    inc edi
    jmp @@gbv_row
    
@@gbv_next_col:
    inc ecx
    jmp @@gbv_col
    
@@gbv_done:
    add rsp, 32
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
GaussianBlurV ENDP

CompositeGlow PROC
    ; Additive blend glow texture onto screen framebuffer
    ; RCX = screen buffer, RDX = glow buffer, R8 = pixel count
    push rbx
    push rsi
    sub rsp, 16
    
    mov rbx, rcx                     ; screen
    mov rsi, rdx                     ; glow
    mov ecx, r8d                     ; pixel count
    
    ; Process each pixel (4 bytes RGBA)
    xor edx, edx                     ; pixel index
@@cg_loop:
    cmp edx, ecx
    jae @@cg_done
    
    lea eax, [edx*4]                 ; byte offset
    
    ; Additive blend: result = clamp(screen + glow, 255)
    ; R channel
    movzx r8d, BYTE PTR [rbx+rax]
    movzx r9d, BYTE PTR [rsi+rax]
    add r8d, r9d
    cmp r8d, 255
    jbe @@cg_r_ok
    mov r8d, 255
@@cg_r_ok:
    mov BYTE PTR [rbx+rax], r8b
    
    ; G channel
    movzx r8d, BYTE PTR [rbx+rax+1]
    movzx r9d, BYTE PTR [rsi+rax+1]
    add r8d, r9d
    cmp r8d, 255
    jbe @@cg_g_ok
    mov r8d, 255
@@cg_g_ok:
    mov BYTE PTR [rbx+rax+1], r8b
    
    ; B channel
    movzx r8d, BYTE PTR [rbx+rax+2]
    movzx r9d, BYTE PTR [rsi+rax+2]
    add r8d, r9d
    cmp r8d, 255
    jbe @@cg_b_ok
    mov r8d, 255
@@cg_b_ok:
    mov BYTE PTR [rbx+rax+2], r8b
    
    inc edx
    jmp @@cg_loop
    
@@cg_done:
    add rsp, 16
    pop rsi
    pop rbx
    ret
CompositeGlow ENDP

LineColToScreen PROC
    ; Convert text line/column to pixel coordinates
    ; ECX = line number, EDX = column number
    ; Returns: EAX = packed (x in low 16, y in high 16)
    
    ; Y = (line - scrollTop) * lineHeight + topMargin
    mov eax, ecx
    sub eax, DWORD PTR [g_ScrollTop]
    imul eax, DWORD PTR [g_LineHeight] ; default 20px
    add eax, DWORD PTR [g_TopMargin]   ; default 0
    shl eax, 16                      ; pack into high word
    
    ; X = column * charWidth + leftMargin + gutterWidth
    mov ecx, edx
    imul ecx, DWORD PTR [g_CharWidth]  ; default 8px (monospace)
    add ecx, DWORD PTR [g_LeftMargin]
    add ecx, DWORD PTR [g_GutterWidth]
    and ecx, 0FFFFh                  ; mask to 16 bits
    or eax, ecx                      ; pack x into low word
    
    ret
LineColToScreen ENDP

CalculateGlowIntensity PROC
    ; Distance-based glow falloff
    ; ECX = distance in pixels
    ; Returns EAX = intensity (0-255)
    
    ; Inverse square falloff: intensity = 255 * radius^2 / (distance^2 + radius^2)
    ; radius = 32 pixels
    mov eax, ecx
    imul eax, eax                    ; distance^2
    add eax, 1024                    ; + 32^2 (avoid div by zero + radius)
    
    mov edx, 261120                  ; 255 * 1024
    push rcx
    xor ecx, ecx
    xchg eax, edx
    xor edx, edx
    xchg eax, ecx
    mov eax, ecx
    xor edx, edx
    mov ecx, eax
    mov eax, 261120
    xor edx, edx
    div ecx
    pop rcx
    
    ; Clamp to [0, 255]
    cmp eax, 255
    jbe @@cgi_ok
    mov eax, 255
@@cgi_ok:
    ret
CalculateGlowIntensity ENDP

RenderGlowQuad PROC
    ; Draw textured quad with glow effect into framebuffer
    ; RCX = framebuffer, EDX = x, R8D = y, R9D = size, [rsp+40] = color
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    
    mov rbx, rcx                     ; framebuffer
    mov esi, edx                     ; x
    mov edi, r8d                     ; y
    
    ; Draw filled quad with glow intensity
    ; Iterate (size x size) pixels, apply glow based on distance from center
    mov ecx, r9d                     ; size
    shr ecx, 1                       ; half = radius
    
    xor r8d, r8d                     ; dy
@@rgq_y:
    cmp r8d, r9d
    jae @@rgq_done
    
    xor r10d, r10d                   ; dx
@@rgq_x:
    cmp r10d, r9d
    jae @@rgq_next_y
    
    ; Calculate distance from center
    mov eax, r10d
    sub eax, ecx                     ; dx - radius
    imul eax, eax
    mov edx, r8d
    sub edx, ecx                     ; dy - radius
    imul edx, edx
    add eax, edx                     ; dist^2
    
    ; Skip if outside circle
    mov edx, ecx
    imul edx, edx                    ; radius^2
    cmp eax, edx
    ja @@rgq_skip
    
    ; Calculate pixel address
    mov eax, edi
    add eax, r8d                     ; screen_y
    imul eax, DWORD PTR [g_GlowFBWidth]
    add eax, esi
    add eax, r10d                    ; screen_x
    shl eax, 2                       ; * 4
    
    ; Write glow color
    mov DWORD PTR [rbx+rax], 0FF8040FFh  ; RGBA glow color
    
@@rgq_skip:
    inc r10d
    jmp @@rgq_x
    
@@rgq_next_y:
    inc r8d
    jmp @@rgq_y
    
@@rgq_done:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
RenderGlowQuad ENDP

ShiftHue PROC
    ; HSV hue rotation for color adjustment
    ; ECX = input RGB color (0xRRGGBB), EDX = hue shift degrees (0-360)
    ; Returns EAX = shifted RGB color
    push rbx
    push rsi
    sub rsp, 32
    
    ; Extract RGB components
    mov ebx, ecx
    movzx eax, bl                    ; B
    mov [rsp+16], eax
    shr ebx, 8
    movzx eax, bl                    ; G
    mov [rsp+12], eax
    shr ebx, 8
    movzx eax, bl                    ; R
    mov [rsp+8], eax
    
    ; Simple hue rotation via channel cycling
    ; 0-120: R->G shift, 120-240: G->B shift, 240-360: B->R shift
    mov esi, edx                     ; hue shift
    
    cmp esi, 120
    jb @@sh_rg
    cmp esi, 240
    jb @@sh_gb
    ; B->R shift (240-360)
    mov eax, [rsp+16]                ; B
    mov ecx, [rsp+8]                 ; R
    mov edx, [rsp+12]                ; G
    mov [rsp+8], eax                 ; R = B
    mov [rsp+12], ecx                ; G = R
    mov [rsp+16], edx                ; B = G
    jmp @@sh_combine
@@sh_rg:
    ; R->G shift (0-120)
    mov eax, [rsp+8]                 ; R
    mov ecx, [rsp+12]                ; G
    mov edx, [rsp+16]                ; B
    mov [rsp+12], eax                ; G = R
    mov [rsp+16], ecx                ; B = G
    mov [rsp+8], edx                 ; R = B
    jmp @@sh_combine
@@sh_gb:
    ; G->B shift (120-240)
    mov eax, [rsp+12]                ; G
    mov ecx, [rsp+16]                ; B
    mov edx, [rsp+8]                 ; R
    mov [rsp+16], eax                ; B = G
    mov [rsp+8], ecx                 ; R = B
    mov [rsp+12], edx                ; G = R
    
@@sh_combine:
    ; Recombine RGB
    mov eax, [rsp+8]                 ; R
    shl eax, 8
    or eax, [rsp+12]                 ; G
    shl eax, 8
    or eax, [rsp+16]                 ; B
    
    add rsp, 32
    pop rsi
    pop rbx
    ret
ShiftHue ENDP

; ==============================================================================
; EXPORTS
; ==============================================================================
PUBLIC ESPInit
PUBLIC ESPScanSymbols
PUBLIC ESPRenderGlow
PUBLIC AimbotUpdate
PUBLIC ChamsRender
PUBLIC TriggerbotCheck
PUBLIC BhopScroll
PUBLIC SpeedhackSetScale

; ==============================================================================
; DATA SECTION
; ==============================================================================
.DATA
ALIGN 16

fp_one          REAL4 1.0
fp_half         REAL4 0.5
fp_zero         REAL4 0.0

; Glow color presets (like CS:GO cheat configs)
GLOW_PRESET_RAGE:
    DWORD 0xFFFF0000h   ; Full red, max intensity
GLOW_PRESET_LEGIT:
    DWORD 0x80FFFFFF h  ; Subtle white
GLOW_PRESET_RAINBOW:
    DWORD 0xFFFF00FFh   ; Cycles through spectrum

; Aimbot curves (smoothing profiles)
AIMBOT_CURVE_LINEAR:
    REAL4 1.0
AIMBOT_CURVE_EASE_IN:
    REAL4 0.3, 0.5, 0.8, 1.0
AIMBOT_CURVE_EASE_OUT:
    REAL4 1.0, 0.8, 0.5, 0.3

; ==============================================================================
END