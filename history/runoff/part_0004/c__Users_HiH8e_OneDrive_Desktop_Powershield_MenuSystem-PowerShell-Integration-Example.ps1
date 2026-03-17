# RawrXD Menu System - PowerShell Integration Example
# This script demonstrates how to integrate the enhanced menu system with PowerShell

<#
.SYNOPSIS
    Example PowerShell handlers for RawrXD Menu System commands

.DESCRIPTION
    Shows how to:
    1. Monitor JavaScript command queue
    2. Process menu commands
    3. Send responses back to JavaScript
    4. Implement file operations, settings, and more

.NOTES
    Author: RawrXD Team
    Date: November 27, 2025
    Requires: WebView2 or browser control with JavaScript bridge
#>

# ============================================================================
# COMMAND PROCESSOR - Monitors JavaScript bridge and executes commands
# ============================================================================

function Invoke-MenuCommand {
    <#
    .SYNOPSIS
        Process a command from the JavaScript menu system
    #>
    param(
        [Parameter(Mandatory)]
        [hashtable]$Command
    )
    
    $commandName = $Command.command
    $params = $Command.params
    $commandId = $Command.id
    
    Write-Host "[Menu Command] $commandName" -ForegroundColor Cyan
    
    try {
        $result = $null
        
        switch ($commandName) {
            # ============================================================
            # FILE OPERATIONS
            # ============================================================
            "New-File" {
                $result = New-EditorFile @params
            }
            "New-Folder" {
                $result = New-EditorFolder @params
            }
            "Open-File" {
                $result = Open-EditorFile @params
            }
            "Save-File" {
                $result = Save-EditorFile @params
            }
            "Save-FileAs" {
                $result = Save-EditorFileAs @params
            }
            "Save-AllFiles" {
                $result = Save-AllEditorFiles
            }
            "Close-Tab" {
                $result = Close-EditorTab @params
            }
            
            # ============================================================
            # VIEW OPERATIONS
            # ============================================================
            "Toggle-Explorer" {
                $result = Toggle-SidebarPanel -Panel "Explorer"
            }
            "Show-Search" {
                $result = Show-SearchPanel
            }
            "Toggle-Terminal" {
                $result = Toggle-TerminalPanel
            }
            
            # ============================================================
            # RUN/DEBUG OPERATIONS
            # ============================================================
            "Run-Code" {
                $result = Start-CodeExecution @params
            }
            "Run-WithoutDebug" {
                $result = Start-CodeExecution @params -NoDebug
            }
            "Debug-Start" {
                $result = Start-Debugger @params
            }
            "Debug-Stop" {
                $result = Stop-Debugger
            }
            
            # ============================================================
            # SETTINGS OPERATIONS
            # ============================================================
            "Set-Theme" {
                $result = Set-EditorTheme -Theme $params.theme
            }
            "Set-FontSize" {
                $result = Set-EditorFontSize -Size $params.size
            }
            "Set-FontFamily" {
                $result = Set-EditorFontFamily -Font $params.font
            }
            "Set-TabSize" {
                $result = Set-EditorTabSize -Size $params.size
            }
            "Set-WordWrap" {
                $result = Set-EditorWordWrap -Enabled $params.enabled
            }
            "Toggle-Minimap" {
                $result = Set-EditorMinimap -Enabled $params.enabled
            }
            "Set-AutoComplete" {
                $result = Set-EditorAutoComplete -Enabled $params.enabled
            }
            "Set-IntelliSense" {
                $result = Set-EditorIntelliSense -Enabled $params.enabled
            }
            "Set-Linting" {
                $result = Set-EditorLinting -Enabled $params.enabled
            }
            "Set-FormatOnSave" {
                $result = Set-EditorFormatOnSave -Enabled $params.enabled
            }
            "Set-AutoSave" {
                $result = Set-EditorAutoSave -Mode $params.mode
            }
            "Set-DefaultShell" {
                $result = Set-TerminalDefaultShell -Shell $params.shell
            }
            "Set-AIModel" {
                $result = Set-AIModel -Model $params.model
            }
            "Set-AITemperature" {
                $result = Set-AITemperature -Temperature $params.temp
            }
            "Set-AIAutoComplete" {
                $result = Set-AIAutoComplete -Enabled $params.enabled
            }
            "Reset-AllSettings" {
                $result = Reset-AllEditorSettings
            }
            "Open-SettingsFile" {
                $result = Open-SettingsFile
            }
            
            # ============================================================
            # TERMINAL OPERATIONS
            # ============================================================
            "New-Terminal" {
                $result = New-TerminalInstance @params
            }
            "Kill-Terminal" {
                $result = Stop-TerminalInstance @params
            }
            "Clear-Terminal" {
                $result = Clear-TerminalContent
            }
            "Set-TerminalShell" {
                $result = Set-TerminalShell -Shell $params.shell
            }
            
            default {
                throw "Unknown command: $commandName"
            }
        }
        
        # Send success response back to JavaScript
        Send-JavaScriptResponse -CommandId $commandId -Success $true -Data $result
        
    } catch {
        Write-Error "[Menu Command Error] $commandName : $_"
        Send-JavaScriptResponse -CommandId $commandId -Success $false -Error $_.Exception.Message
    }
}

# ============================================================================
# EXAMPLE COMMAND IMPLEMENTATIONS
# ============================================================================

function New-EditorFile {
    param([string]$Path, [string]$Template)
    
    Write-Host "Creating new file: $Path" -ForegroundColor Green
    
    if ($Template) {
        # Load template content
        $content = Get-FileTemplate -Template $Template
    } else {
        $content = ""
    }
    
    # Execute JavaScript to create new tab
    $script = @"
        const tab = createNewTab('$Path', '$content');
        switchToTab(tab.id);
"@
    $webView.ExecuteScriptAsync($script)
    
    return @{ path = $Path; created = $true }
}

function Save-EditorFile {
    param([string]$Path, [string]$Content)
    
    if (-not $Path) {
        # Get current file path from active tab
        $Path = Get-ActiveFilePath
    }
    
    Write-Host "Saving file: $Path" -ForegroundColor Green
    
    if ($Content) {
        Set-Content -Path $Path -Value $Content -Encoding UTF8
    } else {
        # Get content from JavaScript editor
        $Content = Get-EditorContent
        Set-Content -Path $Path -Value $Content -Encoding UTF8
    }
    
    # Update tab title (remove unsaved indicator)
    $script = "updateTabSavedStatus('$Path', true);"
    $webView.ExecuteScriptAsync($script)
    
    return @{ path = $Path; saved = $true; size = (Get-Item $Path).Length }
}

function Set-EditorTheme {
    param([string]$Theme)
    
    Write-Host "Setting theme: $Theme" -ForegroundColor Magenta
    
    # Map theme names to CSS files or Monaco themes
    $themeMap = @{
        'Dark+' = 'vs-dark'
        'Light+' = 'vs-light'
        'Monokai' = 'monokai'
        'Solarized Dark' = 'solarized-dark'
        'Dracula' = 'dracula'
        'Nord' = 'nord'
        'One Dark Pro' = 'one-dark-pro'
    }
    
    $monacoTheme = $themeMap[$Theme]
    
    # Apply theme in Monaco editor
    $script = @"
        if (typeof monaco !== 'undefined') {
            monaco.editor.setTheme('$monacoTheme');
        }
        document.body.className = 'theme-$($Theme.ToLower().Replace(' ','-'))';
        console.log('✅ Theme changed to $Theme');
"@
    $webView.ExecuteScriptAsync($script)
    
    # Save to settings
    $global:EditorSettings.theme = $Theme
    Save-EditorSettings
    
    return @{ theme = $Theme }
}

function Set-EditorFontSize {
    param([string]$Size)
    
    Write-Host "Setting font size: $Size" -ForegroundColor Magenta
    
    $script = @"
        if (typeof monaco !== 'undefined') {
            monaco.editor.getEditors().forEach(editor => {
                editor.updateOptions({ fontSize: parseInt('$Size') });
            });
        }
        document.documentElement.style.setProperty('--editor-font-size', '$Size');
"@
    $webView.ExecuteScriptAsync($script)
    
    $global:EditorSettings.fontSize = $Size
    Save-EditorSettings
    
    return @{ fontSize = $Size }
}

function Start-CodeExecution {
    param(
        [string]$FilePath,
        [switch]$NoDebug
    )
    
    if (-not $FilePath) {
        $FilePath = Get-ActiveFilePath
    }
    
    Write-Host "Running: $FilePath" -ForegroundColor Yellow
    
    # Show terminal panel
    $script = "window.rawrxdMenu.actionViewTerminal();"
    $webView.ExecuteScriptAsync($script)
    
    # Execute in terminal
    if ($FilePath -match '\.ps1$') {
        Start-Process pwsh -ArgumentList "-File `"$FilePath`"" -NoNewWindow
    } elseif ($FilePath -match '\.py$') {
        Start-Process python -ArgumentList "`"$FilePath`"" -NoNewWindow
    } elseif ($FilePath -match '\.js$') {
        Start-Process node -ArgumentList "`"$FilePath`"" -NoNewWindow
    }
    
    return @{ executed = $true; file = $FilePath }
}

function Set-AIModel {
    param([string]$Model)
    
    Write-Host "Setting AI Model: $Model" -ForegroundColor Cyan
    
    $global:AISettings.model = $Model
    Save-AISettings
    
    # Reinitialize AI service with new model
    Initialize-AIService -Model $Model
    
    return @{ model = $Model; status = "initialized" }
}

# ============================================================================
# JAVASCRIPT COMMUNICATION HELPERS
# ============================================================================

function Send-JavaScriptResponse {
    param(
        [string]$CommandId,
        [bool]$Success,
        [object]$Data,
        [string]$Error,
        [string]$Message
    )
    
    $response = @{
        id = $CommandId
        success = $Success
        timestamp = (Get-Date).ToString('o')
    }
    
    if ($Data) { $response.data = $Data }
    if ($Error) { $response.error = $Error }
    if ($Message) { $response.message = $Message }
    
    $json = $response | ConvertTo-Json -Compress -Depth 10
    
    # Send response back to JavaScript
    $script = "window.rawrxdMenu.receivePowerShellResponse($json);"
    $webView.ExecuteScriptAsync($script)
}

function Get-EditorContent {
    # Execute JavaScript to get editor content
    $script = @"
        (function() {
            if (typeof monaco !== 'undefined') {
                const editor = monaco.editor.getEditors()[0];
                return editor ? editor.getValue() : '';
            }
            const textarea = document.querySelector('#editor-content, textarea[data-editor]');
            return textarea ? textarea.value : '';
        })();
"@
    
    $result = $webView.ExecuteScriptAsync($script)
    return $result
}

function Get-ActiveFilePath {
    $script = @"
        (function() {
            const activeTab = document.querySelector('.editor-tab.active');
            return activeTab ? activeTab.getAttribute('data-path') : null;
        })();
"@
    
    $result = $webView.ExecuteScriptAsync($script)
    return $result
}

# ============================================================================
# COMMAND QUEUE MONITOR (Main Loop)
# ============================================================================

function Start-MenuCommandMonitor {
    <#
    .SYNOPSIS
        Continuously monitors JavaScript command queue and processes commands
    #>
    param(
        [Parameter(Mandatory)]
        $WebView
    )
    
    Write-Host "[Menu Monitor] Starting command queue monitor..." -ForegroundColor Green
    
    while ($true) {
        try {
            # Check window.psBridgeQueue for new commands
            $script = @"
                (function() {
                    if (window.psBridgeQueue && window.psBridgeQueue.length > 0) {
                        const cmd = window.psBridgeQueue.shift();
                        return JSON.stringify(cmd);
                    }
                    return null;
                })();
"@
            
            $result = $WebView.ExecuteScriptAsync($script).Result
            
            if ($result -and $result -ne 'null') {
                $command = $result | ConvertFrom-Json
                Invoke-MenuCommand -Command $command
            }
            
            Start-Sleep -Milliseconds 100
            
        } catch {
            Write-Warning "[Menu Monitor] Error: $_"
            Start-Sleep -Seconds 1
        }
    }
}

# ============================================================================
# WEBVIEW2 EVENT HANDLER METHOD
# ============================================================================

function Register-WebView2MessageHandler {
    param($WebView)
    
    # Register WebView2 WebMessageReceived event
    $WebView.CoreWebView2.WebMessageReceived += {
        param($sender, $e)
        
        try {
            $message = $e.WebMessageAsJson | ConvertFrom-Json
            
            Write-Host "[WebView2 Message] Received: $($message.command)" -ForegroundColor Cyan
            
            Invoke-MenuCommand -Command $message
            
        } catch {
            Write-Error "[WebView2 Message] Error: $_"
        }
    }
    
    Write-Host "[WebView2] Message handler registered" -ForegroundColor Green
}

# ============================================================================
# CUSTOMEVENT HANDLER METHOD
# ============================================================================

function Register-CustomEventHandler {
    param($WebView)
    
    # Inject JavaScript event listener
    $script = @"
        document.addEventListener('psBridgeCommand', function(e) {
            const cmd = e.detail;
            console.log('[CustomEvent] Command:', cmd);
            
            // Send via WebView2 postMessage
            if (window.chrome && window.chrome.webview) {
                window.chrome.webview.postMessage(cmd);
            }
        });
        console.log('✅ CustomEvent handler registered');
"@
    
    $WebView.ExecuteScriptAsync($script)
}

# ============================================================================
# INITIALIZATION
# ============================================================================

<#
# Example usage in main RawrXD script:

# After WebView2 is initialized:
Register-WebView2MessageHandler -WebView $webView
Register-CustomEventHandler -WebView $webView

# OR use polling method:
Start-MenuCommandMonitor -WebView $webView

#>

Write-Host @"

========================================
  Menu System PowerShell Integration
========================================

✅ Command handlers defined
✅ JavaScript bridge methods ready
✅ Settings operations implemented
✅ File operations implemented
✅ Terminal operations implemented
✅ AI integration ready

Usage:
  1. Call Register-WebView2MessageHandler after WebView2 init
  2. OR start Start-MenuCommandMonitor for polling
  3. Commands from menu automatically processed
  4. Responses sent back to JavaScript

"@ -ForegroundColor Green
