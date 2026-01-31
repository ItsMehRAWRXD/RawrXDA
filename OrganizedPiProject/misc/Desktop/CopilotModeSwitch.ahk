; AutoHotkey v2 — simple mode picker for ai_cli.php
; Ctrl+Alt+M to open; persists default via --set-mode

cli := "C:\Users\Garre\Desktop\Desktop\ultra_turbo_ai_cli.php"

^!m:: {
    cur := GetDefaultMode()
    modes := ["online","local","offline"]
    choice := ChooseMode(modes, cur)
    if (choice = "") {
        SoundBeep 800, 80
        return
    }
    ; Persist
    RunWait Format('powershell -NoProfile -Command "php ''{1}'' --set-mode {2}"', cli, choice),, "Hide"
    MsgBox "Default mode set to: " choice
}

GetDefaultMode() {
    sp := EnvGet("USERPROFILE") "\.ai_cli_state.json"
    try {
        if FileExist(sp) {
            js := FileRead(sp, "UTF-8")
            o := Jxon_Load(js)
            if (o.Has("default_mode")) return o["default_mode"]
        }
    } catch {}
    return "online"
}

ChooseMode(arr, cur) {
    Gui, New, +AlwaysOnTop +OwnDialogs, Select Mode
    Gui.AddText,, "Select mode (current: " cur "):"
    lb := Gui.AddDropDownList(, arr)
    lb.Value := cur
    Gui.AddButton("Default w100", "OK").OnEvent("Click", (*) => Gui.Submit())
    Gui.AddButton("w100", "Cancel").OnEvent("Click", (*) => Gui.Destroy())
    Gui.Show
    Gui.Wait
    try return lb.Text
    catch return ""
}

; Minimal JSON loader (JXON-ish) — use a tiny eval trick via PowerShell
Jxon_Load(json) {
    tmp := A_Temp "\m.json"
    FileDelete tmp
    FileAppend json, tmp, "UTF-8"
    cmd := 'powershell -NoProfile -Command "(Get-Content -Raw ''' + tmp + ''') | ConvertFrom-Json | ConvertTo-Json -Compress"'
    RunWait cmd, , "Hide", &pid
    js := FileRead(tmp, "UTF-8")
    o := Map()
    for k, v in StrSplit(js.Trim("{}"), ",") {
        pair := StrSplit(v, ":")
        if (pair.Length >= 2) {
            key := Trim(pair[1], ' "')
            val := Trim(pair[2], ' "')
            o[key] := val
        }
    }
    return o
}
