# Apply link-fix edits to CMakeLists.txt for RawrXD-Win32IDE
$path = Join-Path $PSScriptRoot "CMakeLists.txt"
$content = Get-Content $path -Raw

# 1. Add missing sources after agentic_executor.cpp (handle both CRLF and LF)
$marker = "src/agentic/agentic_executor.cpp"
$insert = @"
        src/agentic/FIMPromptBuilder.cpp
        src/agentic_observability.cpp
        src/core/rawrxd_subsystem_api.cpp
        src/core/local_parity_bridge.cpp
        src/core/layer_offload_manager.cpp
        src/runtime_core.cpp
        src/agent/telemetry_collector.cpp
        src/win32app/reverse_engineered_stubs.cpp

"@
if ($content.Contains($marker) -and -not $content.Contains("src/agentic/FIMPromptBuilder.cpp")) {
    $content = $content.Replace("        src/agentic/agentic_executor.cpp`r`n`r`n        # T6:", "        src/agentic/agentic_executor.cpp`r`n$insert        # T6:")
    if (-not $content.Contains("src/agentic/FIMPromptBuilder.cpp")) {
        $content = $content.Replace("        src/agentic/agentic_executor.cpp`n`n        # T6:", "        src/agentic/agentic_executor.cpp`n$insert        # T6:")
    }
    if ($content.Contains("src/agentic/FIMPromptBuilder.cpp")) { Write-Host "[OK] Added missing IDE sources." } else { Write-Host "[WARN] Insert pattern not found (check line endings)." }
} else {
    Write-Host "[SKIP] Block already patched or not found."
}

# 2. Remove duplicate main / DiagnosticUtils sources
$old2 = @"
        src/win32app/IDEDiagnosticAutoHealer.cpp
        src/win32app/IDEDiagnosticAutoHealer_Impl.cpp
        src/win32app/IDEAutoHealerLauncher.cpp
        src/win32app/ConsentPrompt.cpp
"@
$new2 = @"
        src/win32app/IDEDiagnosticAutoHealer.cpp
        # IDEDiagnosticAutoHealer_Impl.cpp excluded (duplicate DiagnosticUtils)
        # IDEAutoHealerLauncher.cpp excluded (defines main, conflicts with ASM)
        src/win32app/ConsentPrompt.cpp
"@
if ($content.Contains($old2)) {
    $content = $content.Replace($old2, $new2)
    Write-Host "[OK] Excluded duplicate IDE sources."
} else {
    Write-Host "[SKIP] Duplicate-sources block already patched or not found."
}

Set-Content $path $content -NoNewline
Write-Host "Done. Re-run cmake and build RawrXD-Win32IDE."
