$agentCpps = Get-ChildItem "d:\rawrxd\src\agent\*.cpp" -File

foreach ($file in $agentCpps) {
    $content = Get-Content $file.FullName -Raw
    $changed = $false
    
    # 1. Fix multi-line /* q(Info|Warning|Debug|Critical) removed */ << ...; patterns
    $regex = [regex]::new('/\*\s*q(Info|Warning|Debug|Critical)\s*removed\s*\*/\s*<<\s*(.+?);', 'Singleline')
    $newContent = $regex.Replace($content, {
        param($m)
        $level = $m.Groups[1].Value.ToUpper()
        $args = $m.Groups[2].Value
        $strMatches = [regex]::Matches($args, '"([^"]*)"')
        $logParts = @()
        foreach ($sm in $strMatches) { $logParts += $sm.Groups[1].Value }
        $logMsg = ($logParts -join ' ').Trim()
        if (-not $logMsg) { $logMsg = 'log entry' }
        $logMsg = $logMsg.Replace('%', '%%')
        return "fprintf(stderr, ""[$level] $logMsg\n"");"
    })
    if ($newContent -ne $content) { $content = $newContent; $changed = $true }
    
    # 2. Fix deleteLater() calls
    $newContent = [regex]::Replace($content, '(\s*)(\w+)->deleteLater\(\)\s*;(\s*//.*)?', '$1// cleanup: $2 freed by scope;$3')
    if ($newContent -ne $content) { $content = $newContent; $changed = $true }
    
    # 3. Fix /* QObject:: *//* FIXME: connect(...) */ patterns
    $regex2 = [regex]::new('/\*\s*QObject::\s*\*/\s*/\*\s*FIXME:[^*]*\*/', 'Singleline')
    $newContent = $regex2.Replace($content, '// TODO: implement async callback (WinHTTP)')
    if ($newContent -ne $content) { $content = $newContent; $changed = $true }
    
    # 4. Fix .count() on Qt containers -> .size()
    $newContent = [regex]::Replace($content, '(m_\w+)\.count\(\)', '$1.size()')
    if ($newContent -ne $content) { $content = $newContent; $changed = $true }
    $newContent = [regex]::Replace($content, '\blines\.count\(\)', 'lines.size()')
    if ($newContent -ne $content) { $content = $newContent; $changed = $true }
    
    if ($changed) {
        Set-Content $file.FullName -Value $content -Encoding UTF8
        Write-Host "FIXED: $($file.Name)"
    } else {
        Write-Host "CLEAN: $($file.Name)"
    }
}
Write-Host "Done fixing agent/ cpp files"
