$path = Join-Path $PSScriptRoot "..\src\win32app\Win32IDE.cpp"
$content = Get-Content $path -Raw
$old = "    AppendMenuW(hSecurityMenu, MF_STRING, IDM_SECURITY_SCAN_DEPENDENCIES, L`"Scan &Dependencies (SCA)`");`r`n    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hSecurityMenu, L`"Secu&rity`");"
$new = @"
    AppendMenuW(hSecurityMenu, MF_STRING, IDM_SECURITY_SCAN_DEPENDENCIES, L"Scan &Dependencies (SCA)");
    AppendMenuW(hSecurityMenu, MF_STRING, IDM_SECURITY_DASHBOARD, L"Security &Dashboard");
    AppendMenuW(hSecurityMenu, MF_STRING, IDM_SECURITY_EXPORT_SBOM, L"Export &SBOM...");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hSecurityMenu, L"Secu&rity");
"@
if ($content -match [regex]::Escape($old)) { $content = $content.Replace($old, $new); Set-Content $path $content -NoNewline; "Win32IDE.cpp patched" } else { "Pattern not found" }
