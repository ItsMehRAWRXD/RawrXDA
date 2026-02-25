$content = Get-Content "d:\rawrxd\link_output.txt"
$lnk2019 = $content | Where-Object { $_ -match 'LNK2019|LNK2001' }

# Extract unique symbol names  
$symbols = @()
foreach ($line in $lnk2019) {
    if ($line -match 'unresolved external symbol "([^"]+)"') {
        $symbols += $Matches[1]
    } elseif ($line -match 'unresolved external symbol (\S+)') {
        $symbols += $Matches[1]
    }
}
$unique = $symbols | Sort-Object -Unique

# Sub-categorize "Other" (not matching command handler, webview2, etc.)
$crt = @()
$win32ide_methods = @()
$truly_other = @()

foreach ($s in $unique) {
    # Skip already categorized
    if ($s -match 'handle[A-Z]|WebView2|webview2|Enterprise|License|Speciator|__imp_|ggml_|gguf_|Circular|Beacon|Copilot|copilot|GapCloser|streaming|Streaming|Agent|agent|Agentic') { continue }
    
    if ($s -match '__acrt|__vcrt|__scrt|__std_|__CxxFrameHandler|_CxxThrow|__C_specific|_guard_|_RTC_|__security|__GSHandler|__report_|_purecall|__std_exception|_onexit|_atexit|_exit|_cexit|_c_exit|__dllonexit|_execute_onexit|_register_onexit|_crt_|_initialize|_thread_|_uninit|__current_exception|__intrinsic|_set_new|__std_type_info|_CRT_INIT|__p___argc|__p___argv|__p___wargv|__p__environ|__p__wenviron|__p__commode|__p__fmode|__wgetmainargs|__getmainargs|_configure_narrow|_configure_wide|_controlfp|__setusermatherr|_except_handler|__dyn_tls|_tls_init|_matherr') {
        $crt += $s
    }
    elseif ($s -match 'Win32IDE::') {
        $win32ide_methods += $s
    }
    else {
        $truly_other += $s
    }
}

Write-Host "=== CRT/Runtime Symbols ($($crt.Count)) ==="
$crt | ForEach-Object { Write-Host "  $_" }

Write-Host "`n=== Win32IDE methods (non-handler) ($($win32ide_methods.Count)) ==="
$win32ide_methods | Select-Object -First 30 | ForEach-Object { Write-Host "  $_" }
if ($win32ide_methods.Count -gt 30) { Write-Host "  ... and $($win32ide_methods.Count - 30) more" }

Write-Host "`n=== Truly Other ($($truly_other.Count)) ==="
$truly_other | ForEach-Object { Write-Host "  $_" }
