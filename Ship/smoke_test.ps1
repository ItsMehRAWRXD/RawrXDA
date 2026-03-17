Set-Location "D:\rawrxd\Ship"
$src = Get-Content "RawrXD_Win32_IDE.cpp" -Raw
$p=0; $f=0
Write-Host "=== TEST 1: Binary ==="
$exe = Get-Item "RawrXD_IDE.exe"
if ($exe.Length -gt 500000) { Write-Host "PASS: Binary $($exe.Length)B"; $p++ } else { Write-Host "FAIL: too small"; $f++ }
Write-Host "=== TEST 2: Pane Markers ==="
foreach ($x in @('MSFTEDIT_CLASS','WC_TREEVIEWW','ChatSend','ChatInput','ChatHistory','STATUSCLASSNAME')) { if ($src -match [regex]::Escape($x)) { $p++ } else { Write-Host "FAIL: $x missing"; $f++ } }
Write-Host "PASS: 6 pane markers checked"
Write-Host "=== TEST 3: Visibility Flags ==="
foreach ($x in @('g_bFileTreeVisible','g_bOutputVisible','g_bTerminalVisible','g_bChatVisible','g_bIssuesVisible')) { if ($src -match $x) { $p++ } else { Write-Host "FAIL: $x"; $f++ } }
Write-Host "PASS: 5 flags checked"
Write-Host "=== TEST 4: MoveWindow ==="
$mw = ([regex]::Matches($src, 'MoveWindow\(')).Count
if ($mw -ge 8) { Write-Host "PASS: $mw MoveWindow calls"; $p++ } else { Write-Host "FAIL: $mw MoveWindow"; $f++ }
Write-Host "=== TEST 5: Menu Wiring ==="
$menuIDs = @('ID_FILE_NEW','ID_FILE_OPEN','ID_FILE_SAVE','ID_FILE_SAVEAS','ID_FILE_CLOSE','ID_FILE_EXIT','ID_EDIT_UNDO','ID_EDIT_REDO','ID_EDIT_CUT','ID_EDIT_COPY','ID_EDIT_PASTE','ID_EDIT_SELECTALL','ID_EDIT_FIND','ID_EDIT_REPLACE','ID_EDIT_GOTO','ID_VIEW_FILETREE','ID_VIEW_OUTPUT','ID_VIEW_TERMINAL','ID_VIEW_CHAT','ID_VIEW_ISSUES','ID_VIEW_MINIMAP','ID_VIEW_FOLDING','ID_BUILD_COMPILE','ID_BUILD_RUN','ID_BUILD_STOP','ID_BUILD_CLEAN','ID_AI_COMPLETE','ID_AI_EXPLAIN','ID_AI_REFACTOR','ID_AI_CHAT','ID_AI_MODEL','ID_TOOLS_SETTINGS','ID_TOOLS_PLUGINS','ID_TOOLS_TERMINAL','ID_TOOLS_SNIPPETS','ID_HELP_DOCS','ID_HELP_UPDATES','ID_HELP_ABOUT')
$wired = 0; $unwired = @()
foreach ($id in $menuIDs) { if ($src -match "case\s+${id}\s*:") { $wired++ } else { $unwired += $id } }
if ($wired -eq $menuIDs.Count) { Write-Host "PASS: All $wired/$($menuIDs.Count) menu items"; $p++ } else { Write-Host "WARN: $wired/$($menuIDs.Count). Missing: $($unwired -join ', ')"; $f++ }
Write-Host "=== TEST 6: DLL Export ==="
if ($src -match 'GetProcAddress\([^)]*"ForwardPass"') { Write-Host "PASS: ForwardPass"; $p++ } else { Write-Host "FAIL: ForwardPass"; $f++ }
Write-Host "=== TEST 7: Chat Spam ==="
if ($src -match 'g_chatServerRunning') { Write-Host "PASS: g_chatServerRunning"; $p++ } else { Write-Host "FAIL"; $f++ }
Write-Host "=== TEST 8: Dead Code ==="
$deadFound = 0
foreach ($fn in @('HandleWParam_FileNew','HandleWParam_FileOpen','HandleWParam_FileSave','HandleWParam_EditUndo')) { if ($src -match "void\s+$fn\s*\(") { $deadFound++ } }
if ($deadFound -eq 0) { Write-Host "PASS: Dead handlers removed"; $p++ } else { Write-Host "FAIL: $deadFound remain"; $f++ }
$gc = ([regex]::Matches($src, 'case\s+IDM_AI_GENERATE\s*:')).Count
if ($gc -eq 0) { Write-Host "PASS: No IDM_AI_GENERATE dup"; $p++ } else { Write-Host "FAIL"; $f++ }
Write-Host "=== TEST 9: HandleViewMenu ==="
if ($src -notmatch 'CreateTerminal\(\)') { Write-Host "PASS: No lazy CreateTerminal"; $p++ } else { Write-Host "FAIL: CreateTerminal present"; $f++ }
if ($src -notmatch 'CreateChatPanel\(\)') { Write-Host "PASS: No lazy CreateChatPanel"; $p++ } else { Write-Host "FAIL: CreateChatPanel present"; $f++ }
Write-Host ""
Write-Host "RESULTS: $p PASS / $f FAIL"
