# Script to fix duplicate handler definitions in ssot_handlers_ext.cpp
# Approach: Comment out the entire function body for each duplicate handler

$sourceFile = "D:\rawrxd\src\core\ssot_handlers_ext.cpp"
$backupFile = "D:\rawrxd\src\core\ssot_handlers_ext.cpp.backup"

# List of all duplicate handlers (194 total)
$duplicateHandlers = @(
    'handleAuditCheckMenus', 'handleAuditDashboard', 'handleAuditDetectStubs',
    'handleAuditExportReport', 'handleAuditQuickStats', 'handleAuditRunFull',
    'handleAuditRunTests', 'handleDebugContinue', 'handleDebugStart',
    'handleDebugStop', 'handleEditClipboardHist', 'handleEditCopy',
    'handleEditCopyFormat', 'handleEditCut', 'handleEditFind',
    'handleEditFindNext', 'handleEditFindPrev', 'handleEditGotoLine',
    'handleEditMulticursorAdd', 'handleEditMulticursorRemove',
    'handleEditorCycle', 'handleEditorMonacoCore', 'handleEditorRichEdit',
    'handleEditorStatus', 'handleEditorWebView2', 'handleEditPaste',
    'handleEditPastePlain', 'handleEditRedo', 'handleEditReplace',
    'handleEditSelectAll', 'handleEditSnippet', 'handleEditUndo',
    'handleFileClose', 'handleFileCloseFolder', 'handleFileCloseTab',
    'handleFileExit', 'handleFileLoadModel', 'handleFileModelFromHF',
    'handleFileModelFromOllama', 'handleFileModelFromURL', 'handleFileNew',
    'handleFileNewWindow', 'handleFileOpen', 'handleFileOpenFolder',
    'handleFileQuickLoad', 'handleFileRecentClear', 'handleFileRecentFiles',
    'handleFileSave', 'handleFileSaveAll', 'handleFileSaveAs',
    'handleFileUnifiedLoad', 'handleGauntletExport', 'handleGauntletRun',
    'handleGitCommit', 'handleGitDiff', 'handleGitPull', 'handleGitPush',
    'handleGitStatus', 'handleHelp', 'handleHelpAbout', 'handleHelpCmdRef',
    'handleHelpDocs', 'handleHelpPsDocs', 'handleHelpSearch',
    'handleHelpShortcuts', 'handleHotpatchByte', 'handleHotpatchByteSearch',
    'handleHotpatchEventLog', 'handleHotpatchMemory', 'handleHotpatchMemRevert',
    'handleHotpatchPresetLoad', 'handleHotpatchPresetSave',
    'handleHotpatchProxyBias', 'handleHotpatchProxyRewrite',
    'handleHotpatchProxyStats', 'handleHotpatchProxyTerminate',
    'handleHotpatchProxyValidate', 'handleHotpatchResetStats',
    'handleHotpatchServer', 'handleHotpatchServerRemove',
    'handleHotpatchStatus', 'handleHotpatchToggleAll', 'handleLspClearDiag',
    'handleLspDiagnostics', 'handleLspFindRefs', 'handleLspGotoDef',
    'handleLspHover', 'handleLspRename', 'handleLspRestart',
    'handleLspSrvConfig', 'handleLspSrvExportSymbols',
    'handleLspSrvLaunchStdio', 'handleLspSrvPublishDiag',
    'handleLspSrvReindex', 'handleLspSrvStart', 'handleLspSrvStats',
    'handleLspSrvStatus', 'handleLspSrvStop', 'handleLspStartAll',
    'handleLspStatus', 'handleLspStopAll', 'handleMonacoDevtools',
    'handleMonacoReload', 'handleMonacoSyncTheme', 'handleMonacoToggle',
    'handleMonacoZoomIn', 'handleMonacoZoomOut', 'handleMultiRespCompare',
    'handleMultiRespGenerate', 'handlePdbCacheClear', 'handlePdbEnable',
    'handlePdbExports', 'handlePdbFetch', 'handlePdbIatStatus',
    'handlePdbImports', 'handlePdbLoad', 'handlePdbResolve', 'handlePdbStatus',
    'handleQwAlertDismiss', 'handleQwAlertHistory', 'handleQwAlertMonitor',
    'handleQwAlertResourceStatus', 'handleQwBackupAutoToggle',
    'handleQwBackupCreate', 'handleQwBackupList', 'handleQwBackupPrune',
    'handleQwBackupRestore', 'handleQwShortcutEditor', 'handleQwShortcutReset',
    'handleQwSloDashboard', 'handleSettingsOpen', 'handleSwarmBlacklist',
    'handleSwarmBuildCmake', 'handleSwarmBuildSources', 'handleSwarmCacheClear',
    'handleSwarmCacheStatus', 'handleSwarmCancelBuild', 'handleSwarmConfig',
    'handleSwarmDiscovery', 'handleSwarmEvents', 'handleSwarmFitness',
    'handleSwarmJoin', 'handleSwarmLeave', 'handleSwarmNodes',
    'handleSwarmRemoveNode', 'handleSwarmResetStats', 'handleSwarmStartBuild',
    'handleSwarmStartHybrid', 'handleSwarmStartLeader', 'handleSwarmStartWorker',
    'handleSwarmStats', 'handleSwarmStatus', 'handleSwarmTaskGraph',
    'handleSwarmWorkerConnect', 'handleSwarmWorkerDisconnect',
    'handleSwarmWorkerStatus', 'handleTelemetryClear', 'handleTelemetryDashboard',
    'handleTelemetryExportCsv', 'handleTelemetryExportJson',
    'handleTelemetrySnapshot', 'handleTelemetryToggle', 'handleTerminalKill',
    'handleTerminalList', 'handleTerminalSplitCode', 'handleTerminalSplitH',
    'handleTerminalSplitV', 'handleThemeAbyss', 'handleThemeCatppuccin',
    'handleThemeCrimson', 'handleThemeCyberpunk', 'handleThemeDracula',
    'handleThemeGruvbox', 'handleThemeHighContrast', 'handleThemeLightPlus',
    'handleThemeList', 'handleThemeMonokai', 'handleThemeNord',
    'handleThemeOneDark', 'handleThemeSet', 'handleThemeSolDark',
    'handleThemeSolLight', 'handleThemeSynthwave', 'handleThemeTokyo',
    'handleVoiceDevices', 'handleVoiceJoinRoom', 'handleVoiceMetrics',
    'handleVoiceMode', 'handleVoiceModeContinuous', 'handleVoiceModeDisabled',
    'handleVoicePTT', 'handleVoiceRecord', 'handleVoiceSpeak', 'handleVoiceStatus'
)

Write-Host "Starting duplicate handler removal process..."
Write-Host "Backing up original file to $backupFile"

# Create backup
Copy-Item $sourceFile $backupFile -Force

# Read the entire file
$content = Get-Content $sourceFile -Raw

Write-Host "File size: $($content.Length) bytes"
Write-Host "Processing $($duplicateHandlers.Count) duplicate handlers..."

$removedCount = 0
foreach ($handler in $duplicateHandlers) {
    # Pattern to match: CommandResult handleXxx(const CommandContext& ctx) { ... }
    # This regex finds the function signature and its body (everything until the matching brace)
    $pattern = "(?s)(CommandResult\s+$handler\s*\([^)]*\)\s*\{)"
    
    if ($content -match $pattern) {
        Write-Host "  Found: $handler"
        
        # Find the start of the function
        $startPos = $content.IndexOf($matches[0])
        if ($startPos -ge 0) {
            # Find the matching closing brace
            $braceCount = 0
            $inFunction = $false
            $endPos = $startPos
            
            for ($i = $startPos; $i -lt $content.Length; $i++) {
                if ($content[$i] -eq '{') {
                    $braceCount++
                    $inFunction = $true
                } elseif ($content[$i] -eq '}') {
                    $braceCount--
                    if ($inFunction -and $braceCount -eq 0) {
                        $endPos = $i + 1
                        break
                    }
                }
            }
            
            # Extract the function
            $functionText = $content.Substring($startPos, $endPos - $startPos)
            
            # Comment it out
            $commentedFunction = "/* DUPLICATE REMOVED - defined elsewhere`r`n$functionText`r`n*/"
            
            # Replace in content
            $content = $content.Substring(0, $startPos) + $commentedFunction + $content.Substring($endPos)
            $removedCount++
        }
    }
}

Write-Host "`nRemoved $removedCount duplicate handlers"
Write-Host "Writing modified content back to file..."

# Write back to file
Set-Content -Path $sourceFile -Value $content -NoNewline

Write-Host "Done! Backup saved at: $backupFile"
Write-Host "`nNow run your build to verify the fix."
