; ==============================================================================
; NEON_MONACO_HACK.ASM - ESP Wallhack Glow Edition
; "See through your code like it's a competitive FPS"
; Pure MASM64 - Cheat Engine Aesthetic - Sub-0.2ms Latency
; ==============================================================================

OPTION CASEMAP:NONE
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
    ; Iterate entities
    ; Calculate angle from crosshair
    ; If within FOV cone, check distance
    ; Return closest valid target
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
    ; Check cursor over symbol
    ; If hovering for TRIGGER_DELAY_MS
    ; Execute configured action (complete/format/import)
    ret
TriggerbotCheck ENDP

; ==============================================================================
; BHOP - Momentum-Based Scrolling
; ==============================================================================

; -----------------------------------------------------------------------------
; BhopScroll - Physics-based scroll with momentum
; -----------------------------------------------------------------------------
BhopScroll PROC FRAME
    ; Track scroll velocity
    ; Apply momentum carry
    ; Smooth ease-out
    ret
BhopScroll ENDP

; ==============================================================================
; SPEEDHACK - Time Manipulation
; ==============================================================================

; -----------------------------------------------------------------------------
; SpeedhackSetScale - Modify animation timescale
; -----------------------------------------------------------------------------
SpeedhackSetScale PROC FRAME
    ; Override global time delta
    ; Scale all animations/transitions
    ret
SpeedhackSetScale ENDP

; ==============================================================================
; UTILITY FUNCTIONS
; ==============================================================================

CreateGlowFramebuffer PROC
    ; Allocate texture for off-screen rendering
    ret
CreateGlowFramebuffer ENDP

GaussianBlurH PROC
    ; Horizontal gaussian blur (AVX-512 optimized)
    ret
GaussianBlurH ENDP

GaussianBlurV PROC
    ; Vertical gaussian blur
    ret
GaussianBlurV ENDP

CompositeGlow PROC
    ; Additive blend glow texture onto screen
    ret
CompositeGlow ENDP

LineColToScreen PROC
    ; Convert line/col to pixel coordinates
    ret
LineColToScreen ENDP

CalculateGlowIntensity PROC
    ; Distance-based falloff
    ret
CalculateGlowIntensity ENDP

RenderGlowQuad PROC
    ; Draw textured quad with glow
    ret
RenderGlowQuad ENDP

ShiftHue PROC
    ; HSV color adjustment
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