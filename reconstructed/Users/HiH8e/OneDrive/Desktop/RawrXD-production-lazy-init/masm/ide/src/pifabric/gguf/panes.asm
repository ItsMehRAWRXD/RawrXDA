; pifabric_gguf_panes.asm - GGUF-Aware UI Panes
; THIS WEEK Task #2: Create visualization panes for GGUF models
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc
include gdi32.inc
include comctl32.inc

includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib comctl32.lib

EXTERN PiFabric_Stream:PROC
EXTERN GGUF_Bridge_IntegrateResolver:PROC

PUBLIC GgufPanes_CreateModelViewer
PUBLIC GgufPanes_CreateTensorInspector
PUBLIC GgufPanes_CreateMetadataViewer
PUBLIC GgufPanes_CreateLayerGraph
PUBLIC GgufPanes_UpdateAll

; Pane types
PANE_MODEL_VIEWER       EQU 1
PANE_TENSOR_INSPECTOR   EQU 2
PANE_METADATA_VIEWER    EQU 3
PANE_LAYER_GRAPH        EQU 4

; Pane structure
GgufPane STRUCT
    hWnd        dd ?
    paneType    dd ?
    x           dd ?
    y           dd ?
    width       dd ?
    height      dd ?
    pData       dd ?
GgufPane ENDS

.data
MAX_PANES EQU 8

g_Panes GgufPane MAX_PANES DUP(<0,0,0,0,0,0,0>)
g_PaneCount dd 0

szPaneClassModel    db "GgufModelViewer",0
szPaneClassTensor   db "GgufTensorInspector",0
szPaneClassMetadata db "GgufMetadataViewer",0
szPaneClassLayer    db "GgufLayerGraph",0

szTitleModel        db "Model Overview",0
szTitleTensor       db "Tensor Inspector",0
szTitleMetadata     db "Metadata",0
szTitleLayer        db "Layer Graph",0

; Colors for visualization
CLR_BACKGROUND      dd 001E1E1Eh
CLR_TEXT            dd 00D4D4D4h
CLR_TENSOR_Q4       dd 0056B4E9h  ; Blue for Q4
CLR_TENSOR_Q8       dd 0097C024h  ; Green for Q8
CLR_TENSOR_F16      dd 00FFB000h  ; Orange for F16
CLR_TENSOR_F32      dd 00FF6B6Bh  ; Red for F32

.code

; ============================================================
; GgufPanes_CreateModelViewer - Overview pane with stats
; Input:  ECX = parent window handle
; Output: EAX = pane window handle
; ============================================================
GgufPanes_CreateModelViewer PROC hParent:DWORD
    LOCAL wc:WNDCLASSEXA
    push ebx
    push esi
    push edi
    
    ; Register window class
    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    lea eax, ModelViewerWndProc
    mov wc.lpfnWndProc, eax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    invoke GetModuleHandleA, 0
    mov wc.hInstance, eax
    mov wc.hIcon, 0
    invoke LoadCursor, 0, IDC_ARROW
    mov wc.hCursor, eax
    mov eax, [CLR_BACKGROUND]
    invoke CreateSolidBrush, eax
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, 0
    lea eax, szPaneClassModel
    mov wc.lpszClassName, eax
    mov wc.hIconSm, 0
    
    lea eax, wc
    invoke RegisterClassExA, eax
    
    ; Create window
    invoke CreateWindowExA, WS_EX_TOOLWINDOW, ADDR szPaneClassModel, ADDR szTitleModel, \
        WS_OVERLAPPEDWINDOW or WS_VISIBLE, \
        100, 100, 400, 600, hParent, 0, 0, 0
    
    ; Add to panes array
    test eax, eax
    jz @done
    
    mov ebx, [g_PaneCount]
    imul ebx, SIZEOF GgufPane
    lea esi, [g_Panes + ebx]
    mov [esi].GgufPane.hWnd, eax
    mov [esi].GgufPane.paneType, PANE_MODEL_VIEWER
    inc [g_PaneCount]
    
@done:
    pop edi
    pop esi
    pop ebx
    ret
GgufPanes_CreateModelViewer ENDP

; ============================================================
; GgufPanes_CreateTensorInspector - View tensor details
; Input:  ECX = parent window handle
; Output: EAX = pane window handle
; ============================================================
GgufPanes_CreateTensorInspector PROC hParent:DWORD
    LOCAL wc:WNDCLASSEXA
    push ebx
    push esi
    
    ; Register window class
    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    lea eax, TensorInspectorWndProc
    mov wc.lpfnWndProc, eax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    invoke GetModuleHandleA, 0
    mov wc.hInstance, eax
    mov wc.hIcon, 0
    invoke LoadCursor, 0, IDC_ARROW
    mov wc.hCursor, eax
    mov eax, [CLR_BACKGROUND]
    invoke CreateSolidBrush, eax
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, 0
    lea eax, szPaneClassTensor
    mov wc.lpszClassName, eax
    mov wc.hIconSm, 0
    
    lea eax, wc
    invoke RegisterClassExA, eax
    
    ; Create window
    invoke CreateWindowExA, WS_EX_TOOLWINDOW, ADDR szPaneClassTensor, ADDR szTitleTensor, \
        WS_OVERLAPPEDWINDOW or WS_VISIBLE, \
        520, 100, 500, 600, hParent, 0, 0, 0
    
    test eax, eax
    jz @done
    
    mov ebx, [g_PaneCount]
    imul ebx, SIZEOF GgufPane
    lea esi, [g_Panes + ebx]
    mov [esi].GgufPane.hWnd, eax
    mov [esi].GgufPane.paneType, PANE_TENSOR_INSPECTOR
    inc [g_PaneCount]
    
@done:
    pop esi
    pop ebx
    ret
GgufPanes_CreateTensorInspector ENDP

; ============================================================
; GgufPanes_CreateMetadataViewer - Show model metadata
; Input:  ECX = parent window handle
; Output: EAX = pane window handle
; ============================================================
GgufPanes_CreateMetadataViewer PROC hParent:DWORD
    LOCAL wc:WNDCLASSEXA
    push ebx
    push esi
    
    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    lea eax, MetadataViewerWndProc
    mov wc.lpfnWndProc, eax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    invoke GetModuleHandleA, 0
    mov wc.hInstance, eax
    mov wc.hIcon, 0
    invoke LoadCursor, 0, IDC_ARROW
    mov wc.hCursor, eax
    mov eax, [CLR_BACKGROUND]
    invoke CreateSolidBrush, eax
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, 0
    lea eax, szPaneClassMetadata
    mov wc.lpszClassName, eax
    mov wc.hIconSm, 0
    
    lea eax, wc
    invoke RegisterClassExA, eax
    
    invoke CreateWindowExA, WS_EX_TOOLWINDOW, ADDR szPaneClassMetadata, ADDR szTitleMetadata, \
        WS_OVERLAPPEDWINDOW or WS_VISIBLE, \
        100, 720, 400, 300, hParent, 0, 0, 0
    
    test eax, eax
    jz @done
    
    mov ebx, [g_PaneCount]
    imul ebx, SIZEOF GgufPane
    lea esi, [g_Panes + ebx]
    mov [esi].GgufPane.hWnd, eax
    mov [esi].GgufPane.paneType, PANE_METADATA_VIEWER
    inc [g_PaneCount]
    
@done:
    pop esi
    pop ebx
    ret
GgufPanes_CreateMetadataViewer ENDP

; ============================================================
; GgufPanes_CreateLayerGraph - Visualize model layers
; Input:  ECX = parent window handle
; Output: EAX = pane window handle
; ============================================================
GgufPanes_CreateLayerGraph PROC hParent:DWORD
    LOCAL wc:WNDCLASSEXA
    push ebx
    push esi
    
    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, CS_HREDRAW or CS_VREDRAW
    lea eax, LayerGraphWndProc
    mov wc.lpfnWndProc, eax
    mov wc.cbClsExtra, 0
    mov wc.cbWndExtra, 0
    invoke GetModuleHandleA, 0
    mov wc.hInstance, eax
    mov wc.hIcon, 0
    invoke LoadCursor, 0, IDC_ARROW
    mov wc.hCursor, eax
    mov eax, [CLR_BACKGROUND]
    invoke CreateSolidBrush, eax
    mov wc.hbrBackground, eax
    mov wc.lpszMenuName, 0
    lea eax, szPaneClassLayer
    mov wc.lpszClassName, eax
    mov wc.hIconSm, 0
    
    lea eax, wc
    invoke RegisterClassExA, eax
    
    invoke CreateWindowExA, WS_EX_TOOLWINDOW, ADDR szPaneClassLayer, ADDR szTitleLayer, \
        WS_OVERLAPPEDWINDOW or WS_VISIBLE, \
        520, 720, 500, 300, hParent, 0, 0, 0
    
    test eax, eax
    jz @done
    
    mov ebx, [g_PaneCount]
    imul ebx, SIZEOF GgufPane
    lea esi, [g_Panes + ebx]
    mov [esi].GgufPane.hWnd, eax
    mov [esi].GgufPane.paneType, PANE_LAYER_GRAPH
    inc [g_PaneCount]
    
@done:
    pop esi
    pop ebx
    ret
GgufPanes_CreateLayerGraph ENDP

; ============================================================
; GgufPanes_UpdateAll - Refresh all panes with current data
; ============================================================
GgufPanes_UpdateAll PROC
    push ebx
    push esi
    
    mov ebx, 0
    
@update_loop:
    cmp ebx, [g_PaneCount]
    jae @done
    
    imul esi, ebx, SIZEOF GgufPane
    lea esi, [g_Panes + esi]
    
    mov eax, [esi].GgufPane.hWnd
    test eax, eax
    jz @next
    
    invoke InvalidateRect, eax, 0, TRUE
    
@next:
    inc ebx
    jmp @update_loop
    
@done:
    pop esi
    pop ebx
    ret
GgufPanes_UpdateAll ENDP

; ============================================================
; Window Procedures for each pane type
; ============================================================
ModelViewerWndProc PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:DWORD
    
    cmp uMsg, WM_PAINT
    je @paint
    cmp uMsg, WM_DESTROY
    je @destroy
    
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
    
@paint:
    invoke BeginPaint, hWnd, ADDR ps
    mov hdc, eax
    
    ; Draw model statistics
    invoke SetTextColor, hdc, [CLR_TEXT]
    invoke SetBkMode, hdc, TRANSPARENT
    
    ; Example: Draw model info
    ; (Would pull from GGUF metadata)
    
    invoke EndPaint, hWnd, ADDR ps
    xor eax, eax
    ret
    
@destroy:
    xor eax, eax
    ret
ModelViewerWndProc ENDP

TensorInspectorWndProc PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:DWORD
    
    cmp uMsg, WM_PAINT
    je @paint
    cmp uMsg, WM_DESTROY
    je @destroy
    
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
    
@paint:
    invoke BeginPaint, hWnd, ADDR ps
    mov hdc, eax
    
    ; Draw tensor details
    ; Color-coded by quantization type
    
    invoke EndPaint, hWnd, ADDR ps
    xor eax, eax
    ret
    
@destroy:
    xor eax, eax
    ret
TensorInspectorWndProc ENDP

MetadataViewerWndProc PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:DWORD
    
    cmp uMsg, WM_PAINT
    je @paint
    cmp uMsg, WM_DESTROY
    je @destroy
    
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
    
@paint:
    invoke BeginPaint, hWnd, ADDR ps
    mov hdc, eax
    
    ; Draw metadata key-value pairs
    
    invoke EndPaint, hWnd, ADDR ps
    xor eax, eax
    ret
    
@destroy:
    xor eax, eax
    ret
MetadataViewerWndProc ENDP

LayerGraphWndProc PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    LOCAL ps:PAINTSTRUCT
    LOCAL hdc:DWORD
    
    cmp uMsg, WM_PAINT
    je @paint
    cmp uMsg, WM_DESTROY
    je @destroy
    
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
    
@paint:
    invoke BeginPaint, hWnd, ADDR ps
    mov hdc, eax
    
    ; Draw layer graph visualization
    ; Show connections between layers
    
    invoke EndPaint, hWnd, ADDR ps
    xor eax, eax
    ret
    
@destroy:
    xor eax, eax
    ret
LayerGraphWndProc ENDP

END
