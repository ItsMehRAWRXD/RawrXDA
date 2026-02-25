; RawrXD_Parasite.asm - Runs inside VS Code/Cursor
OPTION CASEMAP:NONE

includelib kernel32.lib
includelib user32.lib

.data
g_origWndProc   DQ 0
g_hWndHost      DQ 0
szExtHostClass  DB "Chrome_WidgetWin_1",0

.code
InitParasiteHost PROC
    ret
InitParasiteHost ENDP

PUBLIC InitParasiteHost
END
