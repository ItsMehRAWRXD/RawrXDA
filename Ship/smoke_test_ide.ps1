$ErrorActionPreference = "Continue"
$script:passed = 0; $script:failed = 0; $script:warnings = 0
function Test-Pass($n) { $script:passed++; Write-Host "  [PASS] $n" -ForegroundColor Green }
function Test-Fail($n,$d) { $script:failed++; Write-Host "  [FAIL] $n -- $d" -ForegroundColor Red }
function Test-Warn($n,$d) { $script:warnings++; Write-Host "  [WARN] $n -- $d" -ForegroundColor Yellow }
$shipDir="D:\rawrxd\Ship"; $srcFile=Join-Path $shipDir "RawrXD_Win32_IDE.cpp"; $exeFile=Join-Path $shipDir "RawrXD_IDE.exe"
Write-Host "`n===== RawrXD IDE Smoke Test =====`n" -ForegroundColor Cyan
# G1: Binary
Write-Host "[1] Binary" -ForegroundColor White
if(Test-Path $exeFile){$exe=Get-Item $exeFile;Test-Pass "exe exists ($($exe.Length)B)"}else{Test-Fail "exe" "missing"}
if(!(Test-Path $srcFile)){Test-Fail "src" "missing";exit 1}
$src=Get-Content $srcFile -Raw
# G2: WM_CREATE
Write-Host "`n[2] WM_CREATE 4-Pane" -ForegroundColor White
$wmC=[regex]::Match($src,'case WM_CREATE:\s*\{(.+?)(?=case WM_SIZE:)',[System.Text.RegularExpressions.RegexOptions]::Singleline).Groups[1].Value
if($wmC.Length -gt 100){Test-Pass "WM_CREATE block ($($wmC.Length)ch)"}else{Test-Fail "WM_CREATE" "small"}
@(@('g_hwndFileTree.*CreateWindowExW.*WC_TREEVIEWW','FileTree'),@('g_hwndEditor.*CreateWindowExW.*MSFTEDIT_CLASS','Editor'),@('g_hwndOutput.*CreateWindowExW.*MSFTEDIT_CLASS','Output'),@('g_hwndTerminal.*CreateWindowExW.*MSFTEDIT_CLASS','Terminal'),@('g_hwndChatPanel.*CreateWindowExW','ChatPanel'),@('g_hwndChatHistory.*CreateWindowExW.*MSFTEDIT_CLASS','ChatHistory'),@('g_hwndChatInput.*CreateWindowExW','ChatInput'),@('g_hwndChatSend.*CreateWindowExW.*BUTTON','ChatSend'))|ForEach-Object{if($wmC -match $_[0]){Test-Pass "$($_[1]) created"}else{Test-Fail "$($_[1])" "missing"}}
$ci1=$wmC.IndexOf('g_hwndChatInput = CreateWindowExW');$ci2=$wmC.IndexOf('SendMessageW(g_hwndChatInput')
if($ci1 -ge 0 -and ($ci2 -lt 0 -or $ci2 -gt $ci1)){Test-Pass "No NULL ChatInput crash"}else{Test-Fail "ChatInput order" "msg before create"}
# G3: WM_SIZE
Write-Host "`n[3] WM_SIZE Layout" -ForegroundColor White
$wmS=[regex]::Match($src,'case WM_SIZE:\s*\{(.+?)break;\s*\}',[System.Text.RegularExpressions.RegexOptions]::Singleline).Groups[1].Value
@('g_hwndFileTree','g_hwndEditor','g_hwndOutput','g_hwndTerminal','g_hwndChatPanel','g_hwndChatHistory','g_hwndChatInput','g_hwndChatSend')|ForEach-Object{if($wmS -match "MoveWindow\($_"){Test-Pass "Positions $_"}else{Test-Fail "Positions $_" "no MoveWindow"}}
@('g_bFileTreeVisible','g_bChatVisible','g_bOutputVisible','g_bTerminalVisible')|ForEach-Object{if($wmS -match [regex]::Escape($_)){Test-Pass "Uses $_"}else{Test-Fail "Uses $_" "missing"}}
# G4: WM_COMMAND
Write-Host "`n[4] WM_COMMAND Wiring" -ForegroundColor White
$wmCmd=[regex]::Match($src,'case WM_COMMAND:\s*\{(.+?)(?=case WM_USER)',[System.Text.RegularExpressions.RegexOptions]::Singleline).Groups[1].Value
@('IDM_FILE_NEW','IDM_FILE_OPEN','IDM_FILE_SAVE','IDM_FILE_SAVEAS','IDM_FILE_CLOSE','IDM_FILE_EXIT','IDM_EDIT_UNDO','IDM_EDIT_REDO','IDM_EDIT_CUT','IDM_EDIT_COPY','IDM_EDIT_PASTE','ID_EDIT_FIND','ID_EDIT_REPLACE','ID_EDIT_SELECT_ALL','ID_EDIT_MULTICURSOR','ID_EDIT_COLUMN_MODE','IDM_VIEW_OUTPUT','IDM_VIEW_TERMINAL','IDM_VIEW_CHAT','IDM_VIEW_ISSUES','ID_VIEW_FILETREE','ID_VIEW_MINIMAP','ID_VIEW_FOLDING','ID_AI_COMPLETE','IDM_AI_EXPLAIN','IDM_AI_REFACTOR','IDM_AI_FIX','IDM_AI_LOADMODEL','IDM_AI_LOADGGUF','IDM_AI_UNLOADMODEL','IDM_BUILD_RUN','IDM_BUILD_BUILD','IDM_BUILD_COMPILE','IDM_BUILD_DEBUG','IDM_BUILD_DLLS','ID_CHAT_SEND','IDM_HELP_ABOUT')|ForEach-Object{if($wmCmd -match "case\s+$_\s*:"){Test-Pass "$_ wired"}else{Test-Fail "$_ wired" "no case"}}
# G5: DLL
Write-Host "`n[5] DLL Export Match" -ForegroundColor White
if($src -match 'GetProcAddress\(g_hInferenceEngine,\s*"ForwardPass"\)'){Test-Pass "ForwardPass in source"}else{Test-Fail "ForwardPass" "wrong name"}
if($src -match 'ForwardPassInfer'){Test-Fail "ForwardPassInfer removed" "still in src"}else{Test-Pass "ForwardPassInfer gone"}
# G6: Chat spam
Write-Host "`n[6] Chat Spam Fix" -ForegroundColor White
$chatC=[regex]::Match($wmCmd,'case IDM_VIEW_CHAT:(.+?)break;',[System.Text.RegularExpressions.RegexOptions]::Singleline).Groups[1].Value
if($chatC -match 'g_chatServerRunning'){Test-Pass "Uses g_chatServerRunning"}else{Test-Fail "Chat flag" "wrong"}
if($chatC -match '!g_hChatServerProcess[^)]'){Test-Fail "Old check" "still present"}else{Test-Pass "Old check removed"}
# G7: Dead code
Write-Host "`n[7] Dead Code" -ForegroundColor White
if($src -match 'void HandleAIMenu\(WPARAM'){Test-Fail "HandleAIMenu(WPARAM)" "still present"}else{Test-Pass "HandleAIMenu(WPARAM) removed"}
if($src -match 'void HandleViewMenu\(WPARAM'){Test-Fail "HandleViewMenu(WPARAM)" "still present"}else{Test-Pass "HandleViewMenu(WPARAM) removed"}
if($src -match 'void HandleChatMenu\(WPARAM'){Test-Fail "HandleChatMenu(WPARAM)" "still present"}else{Test-Pass "HandleChatMenu(WPARAM) removed"}
$gc=([regex]::Matches($wmCmd,'case IDM_AI_GENERATE:')).Count
if($gc -eq 0){Test-Pass "No duplicate IDM_AI_GENERATE"}else{Test-Fail "Duplicate" "$gc"}
# G8: HandleViewMenu cleanup
Write-Host "`n[8] HandleViewMenu(HWND) Cleanup" -ForegroundColor White
$hvm=[regex]::Match($src,'void HandleViewMenu\(HWND hwnd, UINT id\)\s*\{(.+?)\n\}',[System.Text.RegularExpressions.RegexOptions]::Singleline).Groups[1].Value
if($hvm -match 'CreateTerminal\(\)'){Test-Fail "Lazy CreateTerminal" "present"}else{Test-Pass "No lazy CreateTerminal"}
if($hvm -match 'CreateChatPanel\(\)'){Test-Fail "Lazy CreateChatPanel" "present"}else{Test-Pass "No lazy CreateChatPanel"}
# G9: Launch
Write-Host "`n[9] Runtime Launch" -ForegroundColor White
try{$p=Start-Process -FilePath $exeFile -PassThru -WindowStyle Minimized;Start-Sleep -Seconds 3
if(!$p.HasExited){Test-Pass "IDE running (PID $($p.Id))";Stop-Process -Id $p.Id -Force -EA SilentlyContinue;Test-Pass "Killed cleanly"}
else{Test-Fail "IDE running" "exited code $($p.ExitCode)"}}catch{Test-Fail "Launch" $_.Exception.Message}
# Summary
Write-Host "`n===== RESULTS: $script:passed passed, $script:failed failed, $script:warnings warn =====" -ForegroundColor Cyan
if($script:failed -eq 0){Write-Host "  ALL TESTS PASSED!" -ForegroundColor Green}else{Write-Host "  $script:failed FAILURE(S)" -ForegroundColor Red}
