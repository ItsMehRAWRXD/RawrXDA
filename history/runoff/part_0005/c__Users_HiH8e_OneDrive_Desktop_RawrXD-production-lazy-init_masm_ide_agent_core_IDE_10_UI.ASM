;==============================================================================
; RawrXD Advanced UI Framework - Pure MASM Implementation
; Phase 10: Custom Controls, Double Buffering, and Component System
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc
include gdi32.inc
include kernel32.inc

includelib user32.lib
includelib gdi32.lib
includelib kernel32.lib

;==============================================================================
; UI CONSTANTS
;==============================================================================
MAX_COMPONENTS          equ 256
COMP_TYPE_BUTTON        equ 1
COMP_TYPE_TEXTBOX       equ 2
COMP_TYPE_PANEL         equ 3

;==============================================================================
; UI STRUCTURES
;==============================================================================
UI_COMPONENT struct
    compType        dd ?
    x               dd ?
    y               dd ?
    width           dd ?
    height          dd ?
    text            db 128 dup(?)
    isVisible       dd ?
    isEnabled       dd ?
    pOnClick        dd ?
    hWindow         dd ?
UI_COMPONENT ends

;==============================================================================
; UI STATE
;==============================================================================
.data?
components              UI_COMPONENT MAX_COMPONENTS dup(<?>)
compCount               dd ?
hMainWnd                dd ?
hBackBuffer             dd ?
hBackDC                 dd ?

.code

;==============================================================================
; Phase 10.1: UI Initialization and Double Buffering
;==============================================================================
UI_Init proc hWnd:DWORD
    mov eax, hWnd
    mov hMainWnd, eax
    
    invoke GetDC, hWnd
    mov ebx, eax
    invoke CreateCompatibleDC, ebx
    mov hBackDC, eax
    
    local rect:RECT
    invoke GetClientRect, hWnd, addr rect
    invoke CreateCompatibleBitmap, ebx, rect.right, rect.bottom
    mov hBackBuffer, eax
    
    invoke SelectObject, hBackDC, hBackBuffer
    invoke ReleaseDC, hWnd, ebx
    
    ret
UI_Init endp

;==============================================================================
; Phase 10.2: Component Management
;==============================================================================
UI_CreateComponent proc type:DWORD, x:DWORD, y:DWORD, w:DWORD, h:DWORD, pText:DWORD
    mov eax, compCount
    .if eax >= MAX_COMPONENTS
        ret
    .endif
    
    mov ebx, SIZEOF UI_COMPONENT
    mul ebx
    lea edi, components
    add edi, eax
    assume edi:ptr UI_COMPONENT
    
    mov eax, type
    mov [edi].compType, eax
    mov eax, x
    mov [edi].x, eax
    mov eax, y
    mov [edi].y, eax
    mov eax, w
    mov [edi].width, eax
    mov eax, h
    mov [edi].height, eax
    
    .if pText != 0
        invoke lstrcpyn, addr [edi].text, pText, 128
    .endif
    
    mov [edi].isVisible, TRUE
    mov [edi].isEnabled, TRUE
    
    inc compCount
    mov eax, compCount
    dec eax
    ret
UI_CreateComponent endp

;==============================================================================
; Phase 10.3: Rendering Engine
;==============================================================================
UI_Render proc
    local rect:RECT
    invoke GetClientRect, hMainWnd, addr rect
    
    ; Clear back buffer
    invoke GetStockObject, WHITE_BRUSH
    invoke FillRect, hBackDC, addr rect, eax
    
    ; Render all components
    local i:DWORD
    mov i, 0
    .while i < compCount
        mov eax, i
        mov ebx, SIZEOF UI_COMPONENT
        mul ebx
        lea esi, components
        add esi, eax
        assume esi:ptr UI_COMPONENT
        
        .if [esi].isVisible == TRUE
            invoke UI_RenderComponent, esi
        .endif
        
        inc i
    .endw
    
    ; Flip buffers
    invoke GetDC, hMainWnd
    mov ebx, eax
    invoke BitBlt, ebx, 0, 0, rect.right, rect.bottom, hBackDC, 0, 0, SRCCOPY
    invoke ReleaseDC, hMainWnd, ebx
    
    ret
UI_Render endp

UI_RenderComponent proc pComp:DWORD
    mov esi, pComp
    assume esi:ptr UI_COMPONENT
    
    local rect:RECT
    mov eax, [esi].x
    mov rect.left, eax
    mov eax, [esi].y
    mov rect.top, eax
    mov eax, [esi].x
    add eax, [esi].width
    mov rect.right, eax
    mov eax, [esi].y
    add eax, [esi].height
    mov rect.bottom, eax
    
    .if [esi].compType == COMP_TYPE_BUTTON
        invoke DrawFrameControl, hBackDC, addr rect, DFC_BUTTON, DFCS_BUTTONPUSH
        invoke DrawText, hBackDC, addr [esi].text, -1, addr rect, DT_CENTER or DT_VCENTER or DT_SINGLELINE
    .elseif [esi].compType == COMP_TYPE_TEXTBOX
        invoke GetStockObject, BLACK_PEN
        invoke SelectObject, hBackDC, eax
        invoke Rectangle, hBackDC, rect.left, rect.top, rect.right, rect.bottom
        invoke DrawText, hBackDC, addr [esi].text, -1, addr rect, DT_LEFT or DT_VCENTER or DT_SINGLELINE
    .endif
    
    ret
UI_RenderComponent endp

;==============================================================================
; Phase 10.4: Event Handling
;==============================================================================
UI_HandleMouseClick proc x:DWORD, y:DWORD
    local i:DWORD
    mov i, 0
    .while i < compCount
        mov eax, i
        mov ebx, SIZEOF UI_COMPONENT
        mul ebx
        lea esi, components
        add esi, eax
        assume esi:ptr UI_COMPONENT
        
        ; Hit test
        mov eax, x
        .if eax >= [esi].x
            add eax, [esi].width
            .if x <= eax
                mov eax, y
                .if eax >= [esi].y
                    add eax, [esi].height
                    .if y <= eax
                        ; Clicked!
                        .if [esi].pOnClick != 0
                            call [esi].pOnClick
                        .endif
                        ret
                    .endif
                .endif
            .endif
        .endif
        
        inc i
    .endw
    
    ret
UI_HandleMouseClick endp

end
