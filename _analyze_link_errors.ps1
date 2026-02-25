$content = Get-Content "d:\rawrxd\link_output.txt"
Write-Host "Total lines: $($content.Count)"

$lnk2019 = $content | Where-Object { $_ -match 'LNK2019|LNK2001' }
Write-Host "Unresolved externals: $($lnk2019.Count)"

# Extract unique symbol names
$symbols = @()
foreach ($line in $lnk2019) {
    # Match C++ mangled or unmangled symbol names
    if ($line -match 'unresolved external symbol "([^"]+)"') {
        $symbols += $Matches[1]
    } elseif ($line -match 'unresolved external symbol (\S+)') {
        $symbols += $Matches[1]
    }
}

$unique = $symbols | Sort-Object -Unique
Write-Host "Unique symbols: $($unique.Count)"

# Categorize
$cats = @{}
foreach ($s in $unique) {
    if ($s -match 'handle[A-Z]') { $cat = 'CommandHandler' }
    elseif ($s -match 'WebView2|webview2') { $cat = 'WebView2' }
    elseif ($s -match 'Enterprise|License|Speciator') { $cat = 'Enterprise' }
    elseif ($s -match '__imp_') { $cat = 'ImportLib' }
    elseif ($s -match 'ggml_|gguf_') { $cat = 'GGML' }
    elseif ($s -match 'Circular|Beacon') { $cat = 'CircularBeacon' }
    elseif ($s -match 'Copilot|copilot|GapCloser') { $cat = 'CopilotGapCloser' }
    elseif ($s -match 'streaming|Streaming') { $cat = 'StreamingLoader' }
    elseif ($s -match 'Agent|agent|Agentic') { $cat = 'Agentic' }
    elseif ($s -match 'WinMain|wWinMain|main|wmain') { $cat = 'EntryPoint' }
    else { $cat = 'Other' }
    if (-not $cats.ContainsKey($cat)) { $cats[$cat] = @() }
    $cats[$cat] += $s
}

Write-Host "`n=== CATEGORY BREAKDOWN ==="
foreach ($key in ($cats.Keys | Sort-Object)) {
    Write-Host "`n[$key] ($($cats[$key].Count) symbols):"
    $cats[$key] | Select-Object -First 10 | ForEach-Object { Write-Host "    $_" }
    if ($cats[$key].Count -gt 10) { Write-Host "    ... and $($cats[$key].Count - 10) more" }
}

# Other link errors
$otherErrors = $content | Where-Object { $_ -match 'error LNK' -and $_ -notmatch 'LNK2019|LNK2001|LNK1120' }
Write-Host "`n=== OTHER LINK ERRORS ($($otherErrors.Count)) ==="
$otherErrors | ForEach-Object { Write-Host "  $_" }

# Which obj files reference unresolved symbols?
$refObjs = @{}
foreach ($line in $lnk2019) {
    if ($line -match 'referenced in function .+ \((.+\.obj)\)') {
        $obj = $Matches[1]
        if (-not $refObjs.ContainsKey($obj)) { $refObjs[$obj] = 0 }
        $refObjs[$obj]++
    }
}
Write-Host "`n=== TOP OBJ FILES WITH UNRESOLVED REFS ==="
$refObjs.GetEnumerator() | Sort-Object -Property Value -Descending | Select-Object -First 15 | ForEach-Object {
    Write-Host "  $($_.Value) errors in $($_.Key)"
}
