; RawrXD_StreamRenderer.asm - Real-time Token Appending
; Appends tokens directly to the RichEdit or Custom Chat UI

include masm64_compat.inc
include RawrXD_Common.inc

.data
    szTokenAppended db "Token Appended: %s", 0

.code
; Appends a token to the Chat Display with Auto-scroll
; lpToken: pointer to token string
; tokenLen: length of string
AppendTokenToChat PROC FRAME
    .pushreg rbp
    mov rbp, rsp
    sub rsp, 32
    .endprolog

    ; 1. Set selection to end of RichEdit
    invoke SendMessageA, hChatDisplay, EM_SETSEL, -1, -1

    ; 2. Replace selection with token
    invoke SendMessageA, hChatDisplay, EM_REPLACESEL, 0, rcx

    ; 3. Auto-scroll to bottom
    invoke SendMessageA, hChatDisplay, WM_VSCROLL, SB_BOTTOM, 0

    ; 4. Force repaint (optional for performance)
    ; invoke InvalidateRect, hChatDisplay, NULL, FALSE

    add rsp, 32
    pop rbp
    ret
AppendTokenToChat ENDP

END
