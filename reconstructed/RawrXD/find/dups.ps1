$files = Get-ChildItem "D:\rawrxd\src\asm\*.asm"
$allSyms = @{}

foreach ($f in $files) {
    $fname = $f.Name
    $lines = Get-Content $f.FullName
    $inData = $false
    foreach ($line in $lines) {
        # Track .data sections
        if ($line -match '^\s*\.data\b') { $inData = $true; continue }
        if ($line -match '^\s*\.code\b') { $inData = $false; continue }
        
        # PROC definitions (not in comments)
        if ($line -match '^\s*(\w+)\s+PROC\b' -and $line -notmatch '^\s*;') {
            $name = $Matches[1]
            if (-not $allSyms[$name]) { $allSyms[$name] = @() }
            $allSyms[$name] += "${fname}(PROC)"
        }
        # Data labels in .data/.data? sections
        if ($inData -and $line -match '^\s{0,4}(\w+)\s+(DD|DQ|DB|DW|DWORD|QWORD|BYTE|WORD|REAL4|REAL8)\b' -and $line -notmatch '^\s*;') {
            $name = $Matches[1]
            # Skip common MASM directives
            if ($name -notin @('align','ALIGN','option','OPTION','include','INCLUDE','PUBLIC','EXTERN','EXTERNDEF','IF','ENDIF','ELSE','ELSEIF','IFDEF','IFNDEF','END')) {
                if (-not $allSyms[$name]) { $allSyms[$name] = @() }
                $allSyms[$name] += "${fname}(DATA)"
            }
        }
    }
}

Write-Host "=== DUPLICATE SYMBOLS (case-sensitive) ==="
$allSyms.GetEnumerator() | Where-Object { $_.Value.Count -ge 2 } | Sort-Object { $_.Value.Count } -Descending | ForEach-Object {
    Write-Host "$($_.Key): $($_.Value -join ', ')"
}
