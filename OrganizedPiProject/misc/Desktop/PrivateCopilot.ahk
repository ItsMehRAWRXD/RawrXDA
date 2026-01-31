; AutoHotkey v2 script — Ctrl+Shift+Enter = ask AI on selection
; Requirements:
;  - PHP in PATH
;  - ai_cli.php saved somewhere (set cliPath)
;  - GEMINI_API_KEY set in user env (or edit ahk to pass --auth)

cliPath := "C:\Users\Garre\Desktop\Desktop\ultra_turbo_ai_cli.php"   ; TODO: set this
askArgs := "--stdin --env --stream"  ; add flags, e.g. --examples java

^+Enter:: {  ; Ctrl+Shift+Enter
    A_Clipboard := ""           ; clear clipboard
    Send "^c"                   ; copy selection
    if !ClipWait(1) {
        SoundBeep 1500, 100
        return
    }
    sel := A_Clipboard

    tmpIn  := A_Temp "\pc_in.txt"
    tmpOut := A_Temp "\pc_out.txt"
    FileDelete tmpIn, tmpOut
    FileAppend sel, tmpIn, "UTF-8"

    ; Build the command: type file -> php ai_cli.php --stdin
    cmd := Format('powershell -NoProfile -Command "Get-Content -Raw -Encoding UTF8 ''{1}'' | php ''{2}'' {3} | Out-File -FilePath ''{4}'' -Encoding UTF8"',
                  tmpIn, cliPath, askArgs, tmpOut)

    RunWait cmd, , "Hide"

    if !FileExist(tmpOut) {
        SoundBeep 1000, 120
        return
    }
    out := FileRead(tmpOut, "UTF-8")

    ; Put AI output on clipboard and paste over selection
    A_Clipboard := out
    ClipWait(1)
    Send "^v"
}
