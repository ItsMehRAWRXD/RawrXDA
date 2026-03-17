; pifabric_quality_controls.asm - UI Controls for Quality Tier
; THIS WEEK Task #3: Add sliders, buttons, and real-time adjustment
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc
include comctl32.inc

includelib kernel32.lib
includelib user32.lib
includelib comctl32.lib

EXTERN PiFabric_SetTier:PROC
EXTERN IdeIntegration_SetQualityTier:PROC

PUBLIC QualityControls_CreatePanel
PUBLIC QualityControls_CreateSlider
PUBLIC QualityControls_CreatePresets
PUBLIC QualityControls_UpdateDisplay
PUBLIC QualityControls_ApplyTier

; Quality tiers
TIER_FASTEST        EQU 0
TIER_FAST           EQU 1
TIER_BALANCED       EQU 2
TIER_QUALITY        EQU 3
TIER_MAXIMUM        EQU 4

; Control IDs
IDC_TIER_SLIDER     EQU 2001
IDC_BTN_FASTEST     EQU 2002
IDC_BTN_FAST        EQU 2003
IDC_BTN_BALANCED    EQU 2004
IDC_BTN_QUALITY     EQU 2005
IDC_BTN_MAXIMUM     EQU 2006
IDC_LBL_CURRENT     EQU 2007
IDC_LBL_STATS       EQU 2008

.data
g_hQualityPanel     dd 0
g_hTierSlider       dd 0
g_hCurrentLabel     dd 0
g_hStatsLabel       dd 0
g_CurrentTier       dd TIER_BALANCED

szPanelClass        db "QualityControlPanel",0
szPanelTitle        db "Quality / Speed Control",0

szTierNames         dd OFFSET szFastest
                    dd OFFSET szFast
                    dd OFFSET szBalanced
                    dd OFFSET szQuality
                    dd OFFSET szMaximum

szFastest           db "Fastest (Q4_0)",0
szFast              db "Fast (Q4_K)",0
szBalanced          db "Balanced (Q5_K)",0
szQuality           db "Quality (Q8_0)",0
szMaximum           db "Maximum (F16)",0

szCurrentFmt        db "Current: %s",0
szStatsFmt          db "Speed: %s | Memory: %s | Quality: %s",0

; Stats for each tier
szSpeedFast         db "Very Fast",0
szSpeedMed          db "Medium",0
szSpeedSlow         db "Slower",0
szMemLow            db "Low",0
szMemMed            db "Medium",0
szMemHigh           db "High",0
szQualLow           db "Lower",0
szQualMed           db "Good",0
szQualHigh          db "Best",0

.code

; ============================================================
; QualityControls_CreatePanel - Main control panel window
; Input:  ECX = parent window handle
; Output: EAX = panel window handle
; ============================================================
QualityControls_CreatePanel PROC hParent:DWORD
    LOCAL wc:WNDCLASSEXA
    push ebx
    push esi
    
    ; Register window class
    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    lea eax, QualityPanelWndProc
    mov wc.lpfnWndProc, eax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    invoke GetModuleHandleA, 0
    mov wc.hInstance, eax
    mov wc.hIcon, 0
    invoke LoadCursor, 0, IDC_ARROW
    mov wc.hCursor, eax
    invoke GetSysColorBrush, COLOR_BTNFACE
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, 0
    lea eax, szPanelClass
    mov wc.lpszClassName, eax
    mov wc.hIconSm, 0
    
    lea eax, wc
    invoke RegisterClassExA, eax
    
    ; Create panel window
    invoke CreateWindowExA, 0, ADDR szPanelClass, ADDR szPanelTitle, \
        WS_CHILD or WS_VISIBLE or WS_BORDER, \
        10, 50, 300, 250, hParent, 0, 0, 0
    
    mov [g_hQualityPanel], eax
    
    test eax, eax
    jz @done
    
    ; Create child controls
    push eax
    call QualityControls_CreateSlider
    push [g_hQualityPanel]
    call QualityControls_CreatePresets
    
@done:
    mov eax, [g_hQualityPanel]
    pop esi
    pop ebx
    ret
QualityControls_CreatePanel ENDP

; ============================================================
; QualityControls_CreateSlider - Tier adjustment slider
; Input:  ECX = parent window handle
; Output: EAX = slider handle
; ============================================================
QualityControls_CreateSlider PROC hParent:DWORD
    push ebx
    
    ; Initialize common controls
    invoke InitCommonControls
    
    ; Create trackbar (slider)
    invoke CreateWindowExA, 0, "msctls_trackbar32", 0, \
        WS_CHILD or WS_VISIBLE or TBS_HORZ or TBS_AUTOTICKS, \
        20, 30, 260, 40, hParent, IDC_TIER_SLIDER, 0, 0
    
    mov [g_hTierSlider], eax
    test eax, eax
    jz @done
    
    ; Set range (0-4 for 5 tiers)
    invoke SendMessageA, eax, TBM_SETRANGE, TRUE, 4
    
    ; Set initial position
    invoke SendMessageA, [g_hTierSlider], TBM_SETPOS, TRUE, TIER_BALANCED
    
    ; Set tick frequency
    invoke SendMessageA, [g_hTierSlider], TBM_SETTICFREQ, 1, 0
    
    ; Create current tier label
    invoke CreateWindowExA, 0, "STATIC", "Current: Balanced", \
        WS_CHILD or WS_VISIBLE or SS_CENTER, \
        20, 5, 260, 20, hParent, IDC_LBL_CURRENT, 0, 0
    mov [g_hCurrentLabel], eax
    
    ; Create stats label
    invoke CreateWindowExA, 0, "STATIC", "Speed: Medium | Memory: Medium | Quality: Good", \
        WS_CHILD or WS_VISIBLE or SS_CENTER, \
        10, 75, 280, 20, hParent, IDC_LBL_STATS, 0, 0
    mov [g_hStatsLabel], eax
    
@done:
    mov eax, [g_hTierSlider]
    pop ebx
    ret
QualityControls_CreateSlider ENDP

; ============================================================
; QualityControls_CreatePresets - Quick preset buttons
; Input:  ECX = parent window handle
; ============================================================
QualityControls_CreatePresets PROC hParent:DWORD
    push ebx
    push esi
    
    ; Button dimensions
    mov esi, 20         ; X start
    mov edi, 100        ; Y position
    mov ebx, 252        ; Button width
    mov edx, 25         ; Button height
    
    ; Fastest button
    invoke CreateWindowExA, 0, "BUTTON", "Fastest", \
        WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, \
        esi, edi, ebx, edx, hParent, IDC_BTN_FASTEST, 0, 0
    
    ; Fast button
    add edi, 30
    invoke CreateWindowExA, 0, "BUTTON", "Fast", \
        WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, \
        esi, edi, ebx, edx, hParent, IDC_BTN_FAST, 0, 0
    
    ; Balanced button (default highlighted)
    add edi, 30
    invoke CreateWindowExA, 0, "BUTTON", "Balanced (Default)", \
        WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON or BS_DEFPUSHBUTTON, \
        esi, edi, ebx, edx, hParent, IDC_BTN_BALANCED, 0, 0
    
    ; Quality button
    add edi, 30
    invoke CreateWindowExA, 0, "BUTTON", "Quality", \
        WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, \
        esi, edi, ebx, edx, hParent, IDC_BTN_QUALITY, 0, 0
    
    ; Maximum button
    add edi, 30
    invoke CreateWindowExA, 0, "BUTTON", "Maximum Quality", \
        WS_CHILD or WS_VISIBLE or BS_PUSHBUTTON, \
        esi, edi, ebx, edx, hParent, IDC_BTN_MAXIMUM, 0, 0
    
    pop esi
    pop ebx
    ret
QualityControls_CreatePresets ENDP

; ============================================================
; QualityControls_UpdateDisplay - Refresh tier display
; Input:  ECX = new tier (0-4)
; ============================================================
QualityControls_UpdateDisplay PROC tier:DWORD
    LOCAL szBuffer[128]:BYTE
    push ebx
    push esi
    
    mov ebx, tier
    mov [g_CurrentTier], ebx
    
    ; Update slider position
    invoke SendMessageA, [g_hTierSlider], TBM_SETPOS, TRUE, ebx
    
    ; Get tier name
    mov eax, ebx
    imul eax, 4
    lea esi, szTierNames
    add esi, eax
    mov esi, [esi]
    
    ; Format and update current label
    lea edi, szBuffer
    invoke wsprintf, edi, ADDR szCurrentFmt, esi
    invoke SetWindowTextA, [g_hCurrentLabel], edi
    
    ; Update stats based on tier
    call UpdateStatsLabel
    
    pop esi
    pop ebx
    ret
QualityControls_UpdateDisplay ENDP

; ============================================================
; QualityControls_ApplyTier - Apply tier to PiFabric
; Input:  ECX = tier (0-4)
; ============================================================
QualityControls_ApplyTier PROC tier:DWORD
    push ebx
    
    mov ebx, tier
    
    ; Update display
    push ebx
    call QualityControls_UpdateDisplay
    
    ; Apply via IDE integration
    push ebx
    call IdeIntegration_SetQualityTier
    
    pop ebx
    ret
QualityControls_ApplyTier ENDP

; ============================================================
; Helper: UpdateStatsLabel - Update statistics display
; ============================================================
UpdateStatsLabel PROC
    LOCAL szBuffer[256]:BYTE
    push ebx
    push esi
    push edi
    
    mov ebx, [g_CurrentTier]
    
    ; Determine stats based on tier
    lea esi, szSpeedFast
    lea edi, szMemLow
    lea ecx, szQualLow
    
    cmp ebx, TIER_FASTEST
    je @format
    
    lea esi, szSpeedFast
    lea edi, szMemMed
    lea ecx, szQualMed
    cmp ebx, TIER_FAST
    je @format
    
    lea esi, szSpeedMed
    lea edi, szMemMed
    lea ecx, szQualMed
    cmp ebx, TIER_BALANCED
    je @format
    
    lea esi, szSpeedSlow
    lea edi, szMemHigh
    lea ecx, szQualHigh
    cmp ebx, TIER_QUALITY
    je @format
    
    lea esi, szSpeedSlow
    lea edi, szMemHigh
    lea ecx, szQualHigh
    
@format:
    lea eax, szBuffer
    push ecx
    push edi
    push esi
    push OFFSET szStatsFmt
    push eax
    call wsprintf
    add esp, 20
    
    invoke SetWindowTextA, [g_hStatsLabel], ADDR szBuffer
    
    pop edi
    pop esi
    pop ebx
    ret
UpdateStatsLabel ENDP

; ============================================================
; QualityPanelWndProc - Main window procedure
; ============================================================
QualityPanelWndProc PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    push ebx
    
    cmp uMsg, WM_COMMAND
    je @command
    cmp uMsg, WM_HSCROLL
    je @scroll
    cmp uMsg, WM_DESTROY
    je @destroy
    
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    pop ebx
    ret
    
@command:
    mov eax, wParam
    and eax, 0FFFFh
    
    cmp eax, IDC_BTN_FASTEST
    je @set_fastest
    cmp eax, IDC_BTN_FAST
    je @set_fast
    cmp eax, IDC_BTN_BALANCED
    je @set_balanced
    cmp eax, IDC_BTN_QUALITY
    je @set_quality
    cmp eax, IDC_BTN_MAXIMUM
    je @set_maximum
    jmp @default
    
@set_fastest:
    push TIER_FASTEST
    call QualityControls_ApplyTier
    jmp @done
    
@set_fast:
    push TIER_FAST
    call QualityControls_ApplyTier
    jmp @done
    
@set_balanced:
    push TIER_BALANCED
    call QualityControls_ApplyTier
    jmp @done
    
@set_quality:
    push TIER_QUALITY
    call QualityControls_ApplyTier
    jmp @done
    
@set_maximum:
    push TIER_MAXIMUM
    call QualityControls_ApplyTier
    jmp @done
    
@scroll:
    ; Handle slider movement
    mov eax, lParam
    cmp eax, [g_hTierSlider]
    jne @default
    
    invoke SendMessageA, [g_hTierSlider], TBM_GETPOS, 0, 0
    push eax
    call QualityControls_ApplyTier
    jmp @done
    
@destroy:
    xor eax, eax
    pop ebx
    ret
    
@default:
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    pop ebx
    ret
    
@done:
    xor eax, eax
    pop ebx
    ret
QualityPanelWndProc ENDP

END
