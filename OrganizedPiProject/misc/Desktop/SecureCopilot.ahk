; AutoHotkey v2 script — Ctrl+Shift+C = compile code securely
; Requirements:
;  - secure-compile-api.js running on localhost:4040
;  - Docker installed and running

compileApiUrl := "http://127.0.0.1:4040/compile"

^+c:: {  ; Ctrl+Shift+C
    A_Clipboard := ""           ; clear clipboard
    Send "^c"                   ; copy selection
    if !ClipWait(1) {
        SoundBeep 1500, 100
        return
    }
    sel := A_Clipboard

    ; Detect language from filename or content
    WinGetActiveTitle, activeTitle
    language := detectLanguage(activeTitle, sel)
    
    if (language = "") {
        SoundBeep 1200, 200
        MsgBox("Could not detect language. Supported: c, cpp, java, node, python", "Language Detection Failed")
        return
    }

    filename := getFilename(language)
    
    ; Call secure compile API
    payload := '{"language":"' . language . '","filename":"' . filename . '","source":"' . escapeJson(sel) . '","timeout":10}'
    
    cmd := Format('powershell -NoProfile -Command "Invoke-RestMethod -Uri ''{1}'' -Method POST -Body ''{2}'' -ContentType ''application/json'' | ConvertTo-Json -Depth 5"', compileApiUrl, payload)
    
    result := ""
    RunWait cmd, , "Hide", &result
    
    if (result = "") {
        SoundBeep 1000, 120
        MsgBox("Compilation failed - no response from API", "Compile Error")
        return
    }

    ; Parse result and show compilation status
    try {
        ; Simple parsing - in real implementation you'd use JSON library
        if (InStr(result, '"ok":true')) {
            SoundBeep 800, 100
            MsgBox("Compilation successful!`n`n" . result, "Compile Success")
        } else {
            SoundBeep 600, 200
            MsgBox("Compilation failed!`n`n" . result, "Compile Error")
        }
    } catch {
        SoundBeep 500, 300
        MsgBox("Error parsing result: " . result, "Parse Error")
    }
}

detectLanguage(title, content) {
    ; Detect from filename extension in title
    if (InStr(title, ".c") || InStr(title, ".h"))
        return "c"
    if (InStr(title, ".cpp") || InStr(title, ".cc") || InStr(title, ".cxx") || InStr(title, ".hpp"))
        return "cpp"
    if (InStr(title, ".java"))
        return "java"
    if (InStr(title, ".js") || InStr(title, ".mjs"))
        return "node"
    if (InStr(title, ".py"))
        return "python"
    
    ; Detect from content patterns
    if (InStr(content, "#include") && InStr(content, "main("))
        return "c"
    if (InStr(content, "#include") && InStr(content, "std::") || InStr(content, "using namespace"))
        return "cpp"
    if (InStr(content, "public class") || InStr(content, "public static void main"))
        return "java"
    if (InStr(content, "function ") || InStr(content, "const ") || InStr(content, "let "))
        return "node"
    if (InStr(content, "def ") || InStr(content, "import ") || InStr(content, "print("))
        return "python"
    
    return ""
}

getFilename(language) {
    switch language {
        case "c": return "main.c"
        case "cpp": return "main.cpp"
        case "java": return "Main.java"
        case "node": return "app.js"
        case "python": return "main.py"
        default: return "main.txt"
    }
}

escapeJson(str) {
    str := StrReplace(str, "\", "\\")
    str := StrReplace(str, '"', '\"')
    str := StrReplace(str, "`n", '\n')
    str := StrReplace(str, "`r", '\r')
    str := StrReplace(str, "`t", '\t')
    return str
}
