#!/usr/bin/env python3
"""
Fix duplicate handler definitions in ssot_handlers_ext.cpp by commenting them out.
"""
import re
import shutil

source_file = r"D:\rawrxd\src\core\ssot_handlers_ext.cpp"
backup_file = r"D:\rawrxd\src\core\ssot_handlers_ext.cpp.backup"

# List of all 194 duplicate handlers
duplicate_handlers = [
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
]

def find_function_body(text, start_pos):
    """Find the matching closing brace for a function starting at start_pos."""
    brace_count = 0
    in_function = False
    
    for i in range(start_pos, len(text)):
        if text[i] == '{':
            brace_count += 1
            in_function = True
        elif text[i] == '}':
            brace_count -= 1
            if in_function and brace_count == 0:
                return i + 1
    return -1

def main():
    print(f"Starting duplicate handler removal...")
    print(f"Backing up to {backup_file}")
    
    # Create backup
    shutil.copy2(source_file, backup_file)
    
    # Read file
    with open(source_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    print(f"File size: {len(content)} bytes")
    print(f"Processing {len(duplicate_handlers)} handlers...")
    
    removed_count = 0
    for handler in duplicate_handlers:
        # Match: CommandResult handleXxx(const CommandContext& ctx) {
        pattern = rf'CommandResult\s+{re.escape(handler)}\s*\([^)]*\)\s*\{{'
        match = re.search(pattern, content)
        
        if match:
            start_pos = match.start()
            func_body_start = match.end() - 1  # Position of opening brace
            end_pos = find_function_body(content, func_body_start)
            
            if end_pos > 0:
                # Extract function
                function_text = content[start_pos:end_pos]
                
                # Comment it out
                commented = f"#if 0  // DUPLICATE REMOVED - defined elsewhere\n{function_text}\n#endif\n"
                
                # Replace
                content = content[:start_pos] + commented + content[end_pos:]
                print(f"  ✓ Removed {handler}")
                removed_count += 1
            else:
                print(f"  ✗ Could not find end of {handler}")
        else:
            print(f"  - Not found: {handler}")
    
    print(f"\nRemoved {removed_count}/{len(duplicate_handlers)} handlers")
    
    # Write back
    with open(source_file, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)
    
    print(f"Done! Modified file saved.")
    print(f"Backup at: {backup_file}")

if __name__ == '__main__':
    main()
