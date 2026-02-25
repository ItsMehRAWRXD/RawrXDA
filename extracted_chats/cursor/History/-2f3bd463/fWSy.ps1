# Monaco Editor Bridge Functions for RawrXD.ps1
# Provides PowerShell ↔ Monaco Editor communication

# Global Monaco state
$script:MonacoEditor = @{
    WebView = $null
    ElementHost = $null
    IsReady = $false
    CurrentFile = $null
    ContentCallbacks = @{}
    RequestId = 0
}

# Initialize Monaco Editor WebView2
function Initialize-MonacoEditor {
    param(
        [System.Windows.Forms.Control]$ParentControl
    )

    Write-StartupLog "Initializing Monaco Editor..." "INFO"

    try {
        # Create ElementHost to embed WPF control in WinForms
        $script:MonacoEditor.ElementHost = New-Object System.Windows.Forms.Integration.ElementHost
        $script:MonacoEditor.ElementHost.Dock = [System.Windows.Forms.DockStyle]::Fill

        # Create WPF WebView2 control
        $wpfWebView = New-Object Microsoft.Web.WebView2.Wpf.WebView2
        $wpfWebView.Focusable = $true
        $script:MonacoEditor.ElementHost.Child = $wpfWebView
        $script:MonacoEditor.WebView = $wpfWebView

        # Create unique user data folder
        $monacoUserDataFolder = Join-Path $env:TEMP "RawrXD-Monaco-$($PID)"
        if (-not (Test-Path $monacoUserDataFolder)) {
            New-Item -ItemType Directory -Path $monacoUserDataFolder -Force | Out-Null
        }

        # Create WebView2 environment
        $monacoEnvironment = [Microsoft.Web.WebView2.Core.CoreWebView2Environment]::CreateAsync(
            $null,
            $monacoUserDataFolder,
            $null
        ).GetAwaiter().GetResult()

        # Set up initialization handler
        $wpfWebView.Add_CoreWebView2InitializationCompleted({
            param($sender, $args)
            if ($args.IsSuccess) {
                Write-StartupLog "✅ Monaco Editor WebView2 initialized!" "SUCCESS"
                $script:MonacoEditor.IsReady = $true

                # Set up message handler
                $sender.CoreWebView2.add_WebMessageReceived({
                    param($msgSender, $msgArgs)
                    try {
                        $messageJson = $msgArgs.WebMessageAsJson
                        if ($messageJson) {
                            $message = $messageJson | ConvertFrom-Json
                            Handle-MonacoMessage -Message $message
                        }
                    }
                    catch {
                        Write-DevConsole "[Monaco Bridge] Error handling message: $_" "ERROR"
                    }
                })

                # Load Monaco HTML
                $monacoHtmlPath = Join-Path $PSScriptRoot "Monaco-Editor-IDE.html"
                if (Test-Path $monacoHtmlPath) {
                    $fileUri = [System.Uri]::new("file:///$($monacoHtmlPath.Replace('\', '/'))")
                    $sender.CoreWebView2.Navigate($fileUri.AbsoluteUri)
                    Write-StartupLog "Loading Monaco Editor from: $monacoHtmlPath" "INFO"
                }
                else {
                    Write-StartupLog "❌ Monaco HTML not found: $monacoHtmlPath" "ERROR"
                }
            }
            else {
                Write-StartupLog "❌ Monaco WebView2 initialization failed: $($args.InitializationException.Message)" "ERROR"
            }
        })

        # Initialize WebView2
        $initTask = $wpfWebView.EnsureCoreWebView2Async($monacoEnvironment)
        Write-StartupLog "Monaco WebView2 initialization started..." "INFO"

        # Add to parent control
        $ParentControl.Controls.Add($script:MonacoEditor.ElementHost) | Out-Null
        $script:MonacoEditor.ElementHost.SendToBack()

        return $script:MonacoEditor.ElementHost
    }
    catch {
        Write-StartupLog "❌ Failed to initialize Monaco Editor: $_" "ERROR"
        return $null
    }
}

# Handle messages from Monaco
function Handle-MonacoMessage {
    param($Message)

    switch ($Message.type) {
        'monacoReady' {
            Write-DevConsole "[Monaco] Editor ready!" "SUCCESS"
            $script:MonacoEditor.IsReady = $true
        }
        'contentChanged' {
            # Content changed in Monaco
            if ($script:MonacoEditor.CurrentFile) {
                # Mark file as modified
                $script:fileModified = $true
            }
        }
        'contentResponse' {
            # Response to getContent request
            $requestId = $Message.requestId
            if ($script:MonacoEditor.ContentCallbacks.ContainsKey($requestId)) {
                $script:MonacoEditor.ContentCallbacks[$requestId] = $Message.content
            }
        }
        'selectedTextResponse' {
            # Response to getSelectedText request
            $requestId = $Message.requestId
            if ($script:MonacoEditor.ContentCallbacks.ContainsKey($requestId)) {
                $script:MonacoEditor.ContentCallbacks[$requestId] = $Message.text
            }
        }
        'cursorChanged' {
            # Cursor position changed
            # Could update status bar here
        }
        'error' {
            Write-DevConsole "[Monaco] Error: $($Message.message)" "ERROR"
        }
    }
}

# Set content in Monaco editor
function Set-MonacoContent {
    param(
        [string]$Content,
        [string]$Language = 'powershell'
    )

    if (-not $script:MonacoEditor.IsReady -or -not $script:MonacoEditor.WebView.CoreWebView2) {
        Write-DevConsole "[Monaco] Editor not ready, queuing content..." "WARNING"
        Start-Sleep -Milliseconds 500
        if (-not $script:MonacoEditor.IsReady) {
            Write-DevConsole "[Monaco] Editor still not ready after wait" "ERROR"
            return
        }
    }

    $command = @{
        command = 'setContent'
        content = $Content
        language = $Language
    } | ConvertTo-Json -Compress

    $script = "handlePowerShellMessage($command);"
    $script:MonacoEditor.WebView.CoreWebView2.ExecuteScriptAsync($script) | Out-Null
}

# Get content from Monaco editor
function Get-MonacoContent {
    if (-not $script:MonacoEditor.IsReady -or -not $script:MonacoEditor.WebView.CoreWebView2) {
        Write-DevConsole "[Monaco] Editor not ready" "WARNING"
        return ""
    }

    $requestId = [Guid]::NewGuid().ToString()
    $script:MonacoEditor.ContentCallbacks[$requestId] = $null

    $command = @{
        command = 'getContent'
        requestId = $requestId
    } | ConvertTo-Json -Compress

    $script = "handlePowerShellMessage($command);"
    $script:MonacoEditor.WebView.CoreWebView2.ExecuteScriptAsync($script) | Out-Null

    # Wait for response (with timeout)
    $timeout = 30
    $elapsed = 0
    while ($script:MonacoEditor.ContentCallbacks[$requestId] -eq $null -and $elapsed -lt $timeout) {
        Start-Sleep -Milliseconds 100
        $elapsed += 0.1
        [System.Windows.Forms.Application]::DoEvents()
    }

    $content = $script:MonacoEditor.ContentCallbacks[$requestId]
    $script:MonacoEditor.ContentCallbacks.Remove($requestId)

    return $content ?? ""
}

# Set Monaco theme
function Set-MonacoTheme {
    param([string]$Theme = 'vs-dark')

    if (-not $script:MonacoEditor.IsReady) { return }

    $command = @{
        command = 'setTheme'
        theme = $Theme
    } | ConvertTo-Json -Compress

    $script = "handlePowerShellMessage($command);"
    $script:MonacoEditor.WebView.CoreWebView2.ExecuteScriptAsync($script) | Out-Null
}

# Set Monaco language
function Set-MonacoLanguage {
    param([string]$Language = 'powershell')

    if (-not $script:MonacoEditor.IsReady) { return }

    $command = @{
        command = 'setLanguage'
        language = $Language
    } | ConvertTo-Json -Compress

    $script = "handlePowerShellMessage($command);"
    $script:MonacoEditor.WebView.CoreWebView2.ExecuteScriptAsync($script) | Out-Null
}

# Set Monaco font size
function Set-MonacoFontSize {
    param([int]$Size = 14)

    if (-not $script:MonacoEditor.IsReady) { return }

    $command = @{
        command = 'setFontSize'
        size = $Size
    } | ConvertTo-Json -Compress

    $script = "handlePowerShellMessage($command);"
    $script:MonacoEditor.WebView.CoreWebView2.ExecuteScriptAsync($script) | Out-Null
}

# Set Monaco font family
function Set-MonacoFontFamily {
    param([string]$Family = 'Consolas, monospace')

    if (-not $script:MonacoEditor.IsReady) { return }

    $command = @{
        command = 'setFontFamily'
        family = $Family
    } | ConvertTo-Json -Compress

    $script = "handlePowerShellMessage($command);"
    $script:MonacoEditor.WebView.CoreWebView2.ExecuteScriptAsync($script) | Out-Null
}

# Get file language from extension
function Get-FileLanguage {
    param([string]$FilePath)

    $extension = [System.IO.Path]::GetExtension($FilePath).ToLower()
    
    $languageMap = @{
        '.ps1' = 'powershell'
        '.psm1' = 'powershell'
        '.psd1' = 'powershell'
        '.py' = 'python'
        '.js' = 'javascript'
        '.ts' = 'typescript'
        '.html' = 'html'
        '.css' = 'css'
        '.json' = 'json'
        '.xml' = 'xml'
        '.md' = 'markdown'
        '.c' = 'c'
        '.cpp' = 'cpp'
        '.cs' = 'csharp'
        '.java' = 'java'
        '.go' = 'go'
        '.rs' = 'rust'
        '.sql' = 'sql'
        '.sh' = 'shell'
        '.bat' = 'bat'
        '.yml' = 'yaml'
        '.yaml' = 'yaml'
    }

    return $languageMap[$extension] ?? 'text'
}

Write-Host "✅ Monaco Bridge functions loaded" -ForegroundColor Green

