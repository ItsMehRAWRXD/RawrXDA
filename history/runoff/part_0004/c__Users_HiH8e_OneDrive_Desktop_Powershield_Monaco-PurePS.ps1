#Requires -Version 5.1
<#
.SYNOPSIS
  Pure-PowerShell launcher for Monaco Editor (WebView2 host)
.DESCRIPTION
  A drop-in replacement for HTML/JS Monaco page. Embeds Monaco via WebView2
  with full IntelliSense, themes, etc. - all launched from PowerShell.
.PARAMETER Content
  Initial text content for the editor
.PARAMETER Language
  Monaco language id (powershell, python, cpp, javascript, typescript, etc.)
.PARAMETER Theme
  vs-dark | vs | hc-black
.PARAMETER FontSize
  Editor font size in pixels
.EXAMPLE
  .\Monaco-PurePS.ps1 -Language powershell -Content (Get-Content .\MyScript.ps1 -Raw)
.EXAMPLE
  .\Monaco-PurePS.ps1 -Language python -Theme vs -FontSize 16
.NOTES
  Requires WebView2 Runtime. Install with: winget install Microsoft.WebView2.Runtime
#>
param(
    [string]$Content   = '',
    [string]$Language  = 'powershell',
    [string]$Theme     = 'vs-dark',
    [int]$FontSize     = 14
)

Add-Type -AssemblyName System.Windows.Forms

# Try to load WebView2 assemblies
$webView2Loaded = $false

# First, try loading from local WebView2Libs folder (has correct netcoreapp3.0 DLLs)
$localWvDir = Join-Path $PSScriptRoot "WebView2Libs"
$tempWvDir = "$env:TEMP\WVLibs"

# Determine which directory has the DLLs
$wvDir = $null
if (Test-Path "$localWvDir\Microsoft.Web.WebView2.WinForms.dll") {
    $wvDir = $localWvDir
    Write-Host "Using local WebView2Libs folder" -ForegroundColor Cyan
} elseif (Test-Path "$tempWvDir\Microsoft.Web.WebView2.WinForms.dll") {
    $wvDir = $tempWvDir
    Write-Host "Using temp WebView2 folder" -ForegroundColor Cyan
}

if ($wvDir) {
    try {
        # Load Core DLL first (dependency)
        $coreDll = Join-Path $wvDir "Microsoft.Web.WebView2.Core.dll"
        if (Test-Path $coreDll) {
            Add-Type -Path $coreDll -ErrorAction Stop
        }
        # Load WinForms DLL
        $winFormsDll = Join-Path $wvDir "Microsoft.Web.WebView2.WinForms.dll"
        Add-Type -Path $winFormsDll -ErrorAction Stop
        $webView2Loaded = $true
        Write-Host "✅ WebView2 assemblies loaded from: $wvDir" -ForegroundColor Green
    } catch {
        Write-Host "⚠️ Failed to load WebView2 from $wvDir : $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

# If not loaded from file, try system assemblies
if (-not $webView2Loaded) {
    try {
        Add-Type -AssemblyName Microsoft.Web.WebView2.Core -ErrorAction Stop
        Add-Type -AssemblyName Microsoft.Web.WebView2.WinForms -ErrorAction Stop
        $webView2Loaded = $true
        Write-Host "✅ WebView2 assemblies loaded from system" -ForegroundColor Green
    } catch {
        throw "WebView2 assemblies not found. Run RawrXD.ps1 first to download them, or install WebView2 Runtime with: winget install Microsoft.WebView2.Runtime"
    }
}

# ----------  check WebView2 runtime ----------
try {
    $wv2Ver = [Microsoft.Web.WebView2.Core.CoreWebView2Environment]::GetAvailableBrowserVersionString()
    Write-Host "✅ WebView2 Runtime found: $wv2Ver" -ForegroundColor Green
} catch {
    throw "WebView2 runtime not found – install with:  winget install Microsoft.WebView2.Runtime"
}

# ----------  Escape content for JavaScript ----------
function ConvertTo-JsString {
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) { return '' }
    
    # Escape for JavaScript string literal
    $escaped = $Text -replace '\\', '\\\\' `
                     -replace "`r`n", '\n' `
                     -replace "`n", '\n' `
                     -replace "`r", '\n' `
                     -replace '"', '\"' `
                     -replace "'", "\'" `
                     -replace '`', '\`'
    return $escaped
}

$escapedContent = ConvertTo-JsString -Text $Content

# ----------  form + WebView2 ----------
$form          = New-Object System.Windows.Forms.Form
$form.Text     = "Monaco Editor – RawrXD Pure PS"
$form.Width    = 1200
$form.Height   = 800
$form.StartPosition = 'CenterScreen'
$form.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)

# Status bar
$statusBar = New-Object System.Windows.Forms.StatusStrip
$statusBar.BackColor = [System.Drawing.Color]::FromArgb(0, 122, 204)
$statusLabel = New-Object System.Windows.Forms.ToolStripStatusLabel
$statusLabel.Text = "Monaco Editor | Language: $Language | Theme: $Theme"
$statusLabel.ForeColor = [System.Drawing.Color]::White
$statusBar.Items.Add($statusLabel) | Out-Null
$form.Controls.Add($statusBar)

$wv            = New-Object Microsoft.Web.WebView2.WinForms.WebView2
$wv.Dock       = [System.Windows.Forms.DockStyle]::Fill
$form.Controls.Add($wv)

# ----------  HTML + Monaco ----------
$html = @"
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'/>
  <title>Monaco – PS Host</title>
  <style>
    html, body { margin: 0; padding: 0; height: 100%; overflow: hidden; }
    #editor { height: 100%; width: 100%; }
    .loading {
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100%;
      background: #1e1e1e;
      color: #d4d4d4;
      font-family: 'Segoe UI', sans-serif;
      font-size: 18px;
    }
    .loading::after {
      content: '';
      width: 20px;
      height: 20px;
      border: 3px solid #569cd6;
      border-top-color: transparent;
      border-radius: 50%;
      animation: spin 1s linear infinite;
      margin-left: 10px;
    }
    @keyframes spin { to { transform: rotate(360deg); } }
  </style>
</head>
<body>
<div id='editor'><div class='loading'>Loading Monaco Editor</div></div>
<script src='https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs/loader.js'></script>
<script>
require.config({ paths: { vs: 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs' }});
require(['vs/editor/editor.main'], function () {
  document.getElementById('editor').innerHTML = '';
  
  window.editor = monaco.editor.create(document.getElementById('editor'), {
    value: "$escapedContent",
    language: '$Language',
    theme: '$Theme',
    fontSize: $FontSize,
    automaticLayout: true,
    minimap: { enabled: true },
    wordWrap: 'on',
    lineNumbers: 'on',
    scrollBeyondLastLine: false,
    renderWhitespace: 'selection',
    bracketPairColorization: { enabled: true },
    guides: { bracketPairs: true },
    suggestOnTriggerCharacters: true,
    quickSuggestions: true,
    parameterHints: { enabled: true },
    formatOnPaste: true,
    formatOnType: true
  });

  // Expose helpers for PowerShell communication
  window.getValue = function() { 
    return editor.getValue(); 
  };
  
  window.setValue = function(text, lang) { 
    editor.setValue(text); 
    if (lang) {
      monaco.editor.setModelLanguage(editor.getModel(), lang);
    }
  };
  
  window.setTheme = function(theme) {
    monaco.editor.setTheme(theme);
  };
  
  window.setLanguage = function(lang) {
    monaco.editor.setModelLanguage(editor.getModel(), lang);
  };
  
  window.setFontSize = function(size) {
    editor.updateOptions({ fontSize: size });
  };
  
  window.getSelection = function() {
    return editor.getModel().getValueInRange(editor.getSelection());
  };
  
  window.insertText = function(text) {
    editor.executeEdits('', [{ range: editor.getSelection(), text: text }]);
  };
  
  // Notify PowerShell that editor is ready
  window.chrome.webview.postMessage({ type: 'ready', version: monaco.version });
  
  // Send content changes to PowerShell
  editor.onDidChangeModelContent(function(e) {
    window.chrome.webview.postMessage({ type: 'contentChanged', length: editor.getValue().length });
  });
});
</script>
</body>
</html>
"@

# ----------  Store reference for external access ----------
$script:WebView = $wv
$script:EditorReady = $false

# ----------  load HTML ----------
$form.Add_Shown({
    $wv.CoreWebView2InitializationCompleted += {
        param($sender, $args)
        if ($args.IsSuccess) {
            # Handle messages from JavaScript
            $wv.CoreWebView2.WebMessageReceived += {
                param($s, $e)
                try {
                    $msg = $e.WebMessageAsJson | ConvertFrom-Json
                    switch ($msg.type) {
                        'ready' { 
                            $script:EditorReady = $true
                            $statusLabel.Text = "Monaco Editor v$($msg.version) | Language: $Language | Theme: $Theme | Ready"
                            Write-Host "✅ Monaco Editor ready (v$($msg.version))" -ForegroundColor Green
                        }
                        'contentChanged' {
                            $statusLabel.Text = "Monaco Editor | Language: $Language | Characters: $($msg.length)"
                        }
                    }
                } catch {
                    Write-Warning "WebView message error: $_"
                }
            }
            $wv.CoreWebView2.NavigateToString($html)
        } else {
            Write-Error "WebView2 initialization failed: $($args.InitializationException)"
        }
    }
    
    # Create a user data folder in TEMP to avoid access denied errors (especially on OneDrive paths)
    $monacoUserDataFolder = Join-Path $env:TEMP "RawrXD_Monaco_WebView2"
    if (-not (Test-Path $monacoUserDataFolder)) {
        New-Item -ItemType Directory -Path $monacoUserDataFolder -Force | Out-Null
    }
    
    try {
        # Create environment with explicit user data folder
        $envOptions = [Microsoft.Web.WebView2.Core.CoreWebView2EnvironmentOptions]::new()
        $createEnvTask = [Microsoft.Web.WebView2.Core.CoreWebView2Environment]::CreateAsync($null, $monacoUserDataFolder, $envOptions)
        $createEnvTask.Wait()
        $monacoEnv = $createEnvTask.Result
        $wv.EnsureCoreWebView2Async($monacoEnv) | Out-Null
    } catch {
        Write-Warning "Failed to create WebView2 environment, trying default: $_"
        $wv.EnsureCoreWebView2Async($null) | Out-Null
    }
})

# ----------  public helper functions ----------
function global:Get-MonacoContent {
    <#
    .SYNOPSIS
      Retrieves the current content from Monaco Editor
    #>
    if ($script:WebView -and $script:WebView.CoreWebView2 -and $script:EditorReady) {
        try {
            $result = $script:WebView.CoreWebView2.ExecuteScriptAsync("window.getValue()").GetAwaiter().GetResult()
            # Remove surrounding quotes and unescape
            $content = $result.Trim('"') -replace '\\n', "`n" -replace '\\r', "`r" -replace '\\"', '"' -replace '\\\\', '\'
            return $content
        } catch {
            Write-Warning "Failed to get Monaco content: $_"
            return $null
        }
    } else {
        Write-Warning "Monaco Editor not ready"
        return $null
    }
}

function global:Set-MonacoContent {
    <#
    .SYNOPSIS
      Sets content in Monaco Editor
    .PARAMETER Text
      The text content to set
    .PARAMETER Language
      Optional language mode to set
    #>
    param(
        [Parameter(Mandatory)]
        [string]$Text,
        [string]$Lang = ''
    )
    
    if ($script:WebView -and $script:WebView.CoreWebView2 -and $script:EditorReady) {
        try {
            $escapedText = $Text -replace '\\', '\\\\' -replace '"', '\"' -replace "`n", '\n' -replace "`r", '\r'
            $langParam = if ($Lang) { "'$Lang'" } else { "null" }
            $script:WebView.CoreWebView2.ExecuteScriptAsync("window.setValue(`"$escapedText`", $langParam)").GetAwaiter().GetResult() | Out-Null
        } catch {
            Write-Warning "Failed to set Monaco content: $_"
        }
    } else {
        Write-Warning "Monaco Editor not ready"
    }
}

function global:Set-MonacoTheme {
    <#
    .SYNOPSIS
      Changes the Monaco Editor theme
    .PARAMETER Theme
      Theme name: vs-dark, vs, hc-black
    #>
    param([ValidateSet('vs-dark', 'vs', 'hc-black')][string]$Theme)
    
    if ($script:WebView -and $script:WebView.CoreWebView2 -and $script:EditorReady) {
        $script:WebView.CoreWebView2.ExecuteScriptAsync("window.setTheme('$Theme')").GetAwaiter().GetResult() | Out-Null
    }
}

function global:Set-MonacoLanguage {
    <#
    .SYNOPSIS
      Changes the Monaco Editor language mode
    .PARAMETER Language
      Language id (powershell, python, javascript, etc.)
    #>
    param([string]$Language)
    
    if ($script:WebView -and $script:WebView.CoreWebView2 -and $script:EditorReady) {
        $script:WebView.CoreWebView2.ExecuteScriptAsync("window.setLanguage('$Language')").GetAwaiter().GetResult() | Out-Null
    }
}

function global:Get-MonacoSelection {
    <#
    .SYNOPSIS
      Gets the currently selected text in Monaco Editor
    #>
    if ($script:WebView -and $script:WebView.CoreWebView2 -and $script:EditorReady) {
        try {
            $result = $script:WebView.CoreWebView2.ExecuteScriptAsync("window.getSelection()").GetAwaiter().GetResult()
            return $result.Trim('"') -replace '\\n', "`n" -replace '\\"', '"'
        } catch {
            return $null
        }
    }
    return $null
}

function global:Insert-MonacoText {
    <#
    .SYNOPSIS
      Inserts text at the current cursor position or replaces selection
    .PARAMETER Text
      Text to insert
    #>
    param([string]$Text)
    
    if ($script:WebView -and $script:WebView.CoreWebView2 -and $script:EditorReady) {
        $escapedText = $Text -replace '\\', '\\\\' -replace '"', '\"' -replace "`n", '\n'
        $script:WebView.CoreWebView2.ExecuteScriptAsync("window.insertText(`"$escapedText`")").GetAwaiter().GetResult() | Out-Null
    }
}

# ----------  Keyboard shortcuts ----------
$form.KeyPreview = $true
$form.Add_KeyDown({
    param($sender, $e)
    
    # Ctrl+S - Save (placeholder - implement as needed)
    if ($e.Control -and $e.KeyCode -eq 'S') {
        $e.Handled = $true
        $content = Get-MonacoContent
        if ($content) {
            Write-Host "📝 Content ready to save ($($content.Length) characters)" -ForegroundColor Cyan
            # Implement save logic here or emit event
        }
    }
    
    # Ctrl+Shift+P - Command palette info
    if ($e.Control -and $e.Shift -and $e.KeyCode -eq 'P') {
        $e.Handled = $true
        Write-Host @"
Monaco Pure PS Commands:
  Get-MonacoContent      - Get editor content
  Set-MonacoContent      - Set editor content
  Set-MonacoTheme        - Change theme (vs-dark, vs, hc-black)
  Set-MonacoLanguage     - Change language mode
  Get-MonacoSelection    - Get selected text
  Insert-MonacoText      - Insert text at cursor
"@ -ForegroundColor Yellow
    }
})

# ----------  Cleanup on close ----------
$form.Add_FormClosing({
    param($sender, $e)
    try {
        if ($script:WebView) {
            $script:WebView.Dispose()
        }
    } catch {
        # Ignore cleanup errors
    }
})

# ----------  show form ----------
Write-Host @"
╔══════════════════════════════════════════════════════════════╗
║           Monaco Editor - Pure PowerShell Host               ║
╠══════════════════════════════════════════════════════════════╣
║  Language: $Language                                         
║  Theme: $Theme                                               
║  Font Size: ${FontSize}px                                    
╠══════════════════════════════════════════════════════════════╣
║  Ctrl+Shift+P - Show available PS commands                   ║
║  Ctrl+S       - Save notification                            ║
╚══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

[void]$form.ShowDialog()

# Return content when form closes
$finalContent = Get-MonacoContent
if ($finalContent) {
    Write-Host "✅ Editor closed. Content available in `$finalContent variable." -ForegroundColor Green
    return $finalContent
}
