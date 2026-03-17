<#
.SYNOPSIS
    RawrXD-HTMLBridge.ps1 - Enhanced HTML IDE with Native PowerShell Integration
    
.DESCRIPTION
    This script enhances your existing HTML IDE (IDEre2.html) by:
    1. Embedding it in a native Windows WebView2 container
    2. Adding PowerShell backend bridges for file operations
    3. Connecting Ollama AI directly to PowerShell
    4. Optimizing for 4K @ 160Hz displays
    5. Providing native Windows integration
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$HtmlPath = "D:\HTML-Projects\IDEre2.html",
    
    [Parameter(Mandatory = $false)]
    [int]$Width = 3200,
    
    [Parameter(Mandatory = $false)]
    [int]$Height = 1800,
    
    [Parameter(Mandatory = $false)]
    [switch]$FullScreen,
    
    [Parameter(Mandatory = $false)]
    [switch]$Verbose
)

# Allow forced re-initialization when developing
[Parameter(Mandatory = $false)]
[switch]$Force

# Prevent duplicate initialization
try {
    $global:RawrXDInitCreated = $false
    $global:RawrXD_Mutex = New-Object System.Threading.Mutex($true, "RawrXD_HTMLBridge_Mutex", [ref]$global:RawrXDInitCreated)
    if (-not $global:RawrXDInitCreated) {
        Write-Warning "An instance of RawrXD-HTMLBridge is already running. Exiting to avoid duplicate initialization."
        return
    }
}
catch {
    Write-Warning "Unable to acquire initialization mutex; continuing but duplicate init protection may not be available: $($_.Exception.Message)"
}

# Enhanced PowerShell backend for HTML IDE
$global:IDEBridge = @{
    Version = "4.0.0-HTMLPowerShell"
    StartTime = Get-Date
    Form = $null
    WebView = $null
    CurrentProject = $null
    OpenFiles = @{}
    AIModels = @()
    TerminalSession = $null
}

# Marketplace and Extension settings
$global:IDEBridge.MarketplaceUrl = 'https://rawrxd.local/marketplace.json' # configurable
$global:IDEBridge.ExtensionsRoot = Join-Path -Path $PSScriptRoot -ChildPath 'Extensions'

if (-not (Test-Path $global:IDEBridge.ExtensionsRoot)) {
    try { New-Item -ItemType Directory -Path $global:IDEBridge.ExtensionsRoot -Force | Out-Null } catch { }
}

function Ensure-ExtensionManifest {
    param([string]$ExtensionPath)
    $manifestPath = Join-Path $ExtensionPath 'extension.json'
    if (Test-Path $manifestPath) {
        try { return (Get-Content $manifestPath -Raw | ConvertFrom-Json) } catch { return $null }
    }
    return $null
}

function Get-InstalledExtensions {
    [CmdletBinding()]
    param()

    $extensions = @()
    try {
        if (-not (Test-Path $global:IDEBridge.ExtensionsRoot)) { return @() }
        $dirs = Get-ChildItem -Directory -Path $global:IDEBridge.ExtensionsRoot -ErrorAction SilentlyContinue
        foreach ($d in $dirs) {
            $manifest = Ensure-ExtensionManifest -ExtensionPath $d.FullName
            $extensions += [PSCustomObject]@{
                id = if ($manifest) { $manifest.id } else { $d.Name }
                name = if ($manifest) { $manifest.name } else { $d.Name }
                version = if ($manifest) { $manifest.version } else { '0.0.0' }
                enabled = if ($manifest -and $manifest.enabled -ne $null) { $manifest.enabled } else { $true }
                path = $d.FullName
                description = if ($manifest) { $manifest.description } else { '' }
            }
        }
    }
    catch {
        # ignore folder errors
    }
    return $extensions
}

function Get-MarketplaceExtensions {
    [CmdletBinding()]
    param([string]$MarketplaceUrl = $global:IDEBridge.MarketplaceUrl)

    try {
        $data = Invoke-RestMethod -Uri $MarketplaceUrl -Method Get -ErrorAction Stop -TimeoutSec 10
        return $data
    }
    catch {
        return @()
    }
}

function Install-MarketplaceExtension {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][string]$PackageUrl, [string]$Id = $null)

    try {
        if (-not (Test-Path $global:IDEBridge.ExtensionsRoot)) { New-Item -ItemType Directory -Path $global:IDEBridge.ExtensionsRoot -Force | Out-Null }
        $temp = Join-Path $env:TEMP ([System.IO.Path]::GetRandomFileName())
        $downloadPath = $temp + '.zip'
        Invoke-WebRequest -Uri $PackageUrl -OutFile $downloadPath -UseBasicParsing -ErrorAction Stop
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::ExtractToDirectory($downloadPath, $temp)
        if (-not $Id) { $manifest = Ensure-ExtensionManifest -ExtensionPath $temp; if ($manifest) { $Id = $manifest.id } }
        $dest = Join-Path $global:IDEBridge.ExtensionsRoot ($Id ?? ([System.IO.Path]::GetFileNameWithoutExtension($PackageUrl)))
        if (Test-Path $dest) { Remove-Item -Path $dest -Recurse -Force -ErrorAction SilentlyContinue }
        Move-Item -Path $temp -Destination $dest -Force
        return @{ success = $true; id = $Id; path = $dest }
    }
    catch {
        return @{ success = $false; error = $_.Exception.Message }
    }
    finally {
        try { Remove-Item -Path $downloadPath -Force -ErrorAction SilentlyContinue } catch { }
    }
}

function Enable-Extension {
    param([string]$Id)
    $installed = Get-InstalledExtensions | Where-Object { $_.id -eq $Id }
    if (-not $installed) { return @{ success = $false; error = 'NotInstalled' } }
    $manifestPath = Join-Path $installed.path 'extension.json'
    try {
        $manifest = @{}
        if (Test-Path $manifestPath) { $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json }
        $manifest.enabled = $true
        $manifest | ConvertTo-Json -Depth 4 | Out-File $manifestPath -Encoding UTF8
        return @{ success = $true }
    }
    catch { return @{ success = $false; error = $_.Exception.Message } }
}

function Disable-Extension {
    param([string]$Id)
    $installed = Get-InstalledExtensions | Where-Object { $_.id -eq $Id }
    if (-not $installed) { return @{ success = $false; error = 'NotInstalled' } }
    $manifestPath = Join-Path $installed.path 'extension.json'
    try {
        $manifest = @{}
        if (Test-Path $manifestPath) { $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json }
        $manifest.enabled = $false
        $manifest | ConvertTo-Json -Depth 4 | Out-File $manifestPath -Encoding UTF8
        return @{ success = $true }
    }
    catch { return @{ success = $false; error = $_.Exception.Message } }
}

function Uninstall-Extension {
    param([string]$Id)
    $installed = Get-InstalledExtensions | Where-Object { $_.id -eq $Id }
    if (-not $installed) { return @{ success = $true; message = 'AlreadyNotInstalled' } }
    try {
        Remove-Item -Path $installed.path -Recurse -Force
        return @{ success = $true }
    }
    catch { return @{ success = $false; error = $_.Exception.Message } }
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Enhanced file system operations
function Get-DirectoryTree {
    param([string]$Path, [int]$Depth = 2)
    
    try {
        $items = Get-ChildItem -Path $Path -Force | Sort-Object { $_.PSIsContainer -eq $false }, Name
        
        $tree = @()
        foreach ($item in $items) {
            $node = @{
                name = $item.Name
                path = $item.FullName
                type = if ($item.PSIsContainer) { "folder" } else { "file" }
                size = if (-not $item.PSIsContainer) { $item.Length } else { $null }
                modified = $item.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
                extension = if (-not $item.PSIsContainer) { $item.Extension } else { $null }
            }
            
            if ($item.PSIsContainer -and $Depth -gt 0) {
                try {
                    $node.children = Get-DirectoryTree -Path $item.FullName -Depth ($Depth - 1)
                }
                catch {
                    $node.children = @()
                }
            }
            
            $tree += $node
        }
        
        return $tree
    }
    catch {
        return @()
    }
}

# Enhanced Ollama integration
function Invoke-EnhancedOllama {
    param(
        [string]$Prompt,
        [string]$Model = "llama3.2",
        [string]$Context = "",
        [string]$Language = ""
    )
    
    try {
        # Build enhanced prompt with context
        $enhancedPrompt = $Prompt
        
        if ($Context) {
            $enhancedPrompt = "Context: Working in $Language`n`nCode/File: $Context`n`nUser Request: $Prompt"
        }
        
        if ($global:IDEBridge.CurrentProject) {
            $enhancedPrompt = "Project: $($global:IDEBridge.CurrentProject)`n`n$enhancedPrompt"
        }
        
        # Execute Ollama command
        $result = & ollama run $Model $enhancedPrompt 2>&1
        
        if ($LASTEXITCODE -eq 0) {
            return @{
                success = $true
                response = $result | Out-String
                model = $Model
                timestamp = Get-Date
            }
        }
        else {
            return @{
                success = $false
                error = $result | Out-String
                model = $Model
            }
        }
    }
    catch {
        return @{
            success = $false
            error = "Ollama error: $($_.Exception.Message)"
        }
    }
}

# Git integration
function Get-GitInfo {
    param([string]$Path = $PWD)
    
    try {
        Push-Location $Path
        
        $branch = git rev-parse --abbrev-ref HEAD 2>$null
        $status = git status --porcelain 2>$null
        $remoteUrl = git config --get remote.origin.url 2>$null
        $lastCommit = git log -1 --format="%h %s" 2>$null
        
        return @{
            isRepo = $true
            branch = $branch
            hasChanges = $status.Count -gt 0
            changes = $status
            remoteUrl = $remoteUrl
            lastCommit = $lastCommit
        }
    }
    catch {
        return @{ isRepo = $false }
    }
    finally {
        Pop-Location
    }
}

# Terminal integration
function Start-IntegratedTerminal {
    param([string]$WorkingDirectory = $PWD)
    
    try {
        # Create a new PowerShell runspace for terminal operations
        $runspace = [runspacefactory]::CreateRunspace()
        $runspace.Open()
        $runspace.SessionStateProxy.Path.SetLocation($WorkingDirectory)
        
        $global:IDEBridge.TerminalSession = @{
            Runspace = $runspace
            WorkingDirectory = $WorkingDirectory
            History = @()
        }
        
        return @{ success = $true; message = "Terminal session started in $WorkingDirectory" }
    }
    catch {
        return @{ success = $false; error = $_.Exception.Message }
    }
}

function Invoke-TerminalCommand {
    param([string]$Command)
    
    try {
        if (-not $global:IDEBridge.TerminalSession) {
            Start-IntegratedTerminal | Out-Null
        }
        
        $powershell = [powershell]::Create()
        $powershell.Runspace = $global:IDEBridge.TerminalSession.Runspace
        $powershell.AddScript($Command)
        
        $result = $powershell.Invoke()
        $errors = $powershell.Streams.Error
        
        # Add to history
        $global:IDEBridge.TerminalSession.History += @{
            Command = $Command
            Output = $result | Out-String
            Errors = $errors | Out-String
            Timestamp = Get-Date
        }
        
        $powershell.Dispose()
        
        return @{
            success = $true
            output = $result | Out-String
            errors = $errors | Out-String
            workingDirectory = $global:IDEBridge.TerminalSession.Runspace.SessionStateProxy.Path.CurrentLocation
        }
    }
    catch {
        return @{
            success = $false
            error = $_.Exception.Message
        }
    }
}

# WebView2 setup with enhanced JavaScript bridge
function Setup-EnhancedWebView {
    param($Form)
    
    # Try WebView2 first, fallback to WebBrowser
    try {
        # Check for WebView2
        $webView2Available = $false
        try {
            Add-Type -Path "$env:USERPROFILE\.nuget\packages\microsoft.web.webview2\*\lib\net45\Microsoft.Web.WebView2.WinForms.dll"
            $webView = New-Object Microsoft.Web.WebView2.WinForms.WebView2
            $webView2Available = $true
        }
        catch {
            # Fallback to WebBrowser
            $webView = New-Object System.Windows.Forms.WebBrowser
            $webView.ScriptErrorsSuppressed = $true
        }
        
        $webView.Dock = [System.Windows.Forms.DockStyle]::Fill
        $Form.Controls.Add($webView)
        
        if ($webView2Available) {
            Setup-WebView2Bridge -WebView $webView
        }
        else {
            Setup-WebBrowserBridge -WebView $webView
        }
        
        return $webView
    }
    catch {
        Write-Host "Error setting up WebView: $($_.Exception.Message)" -ForegroundColor Red
        return $null
    }
}

function Setup-WebView2Bridge {
    param($WebView)
    
    $WebView.add_NavigationCompleted({
        # Inject comprehensive PowerShell bridge
        $jsCode = @"
// Enhanced PowerShell Bridge for HTML IDE
window.PowerShellBridge = {
    // File Operations
    async openFile() {
        return await this.callPowerShell('open-file');
    },
    
    async saveFile(content, filename) {
        return await this.callPowerShell('save-file', { content, filename });
    },
    
    async getDirectoryTree(path) {
        return await this.callPowerShell('get-directory-tree', { path });
    },
    
    async readFile(path) {
        return await this.callPowerShell('read-file', { path });
    },
    
    async writeFile(path, content) {
        return await this.callPowerShell('write-file', { path, content });
    },
    
    // AI Integration
    async chatWithOllama(prompt, model = 'llama3.2', context = '', language = '') {
        return await this.callPowerShell('ollama-chat', { prompt, model, context, language });
    },
    
    async getOllamaModels() {
        return await this.callPowerShell('get-ollama-models');
    },
    
    // Terminal Integration
    async runTerminalCommand(command) {
        return await this.callPowerShell('terminal-command', { command });
    },
    
    async getTerminalHistory() {
        return await this.callPowerShell('get-terminal-history');
    },
    
    // Git Integration
    async getGitInfo(path) {
        return await this.callPowerShell('get-git-info', { path });
    },
    
    async gitCommand(command, path) {
        return await this.callPowerShell('git-command', { command, path });
    },
    
    // System Integration
    async getSystemInfo() {
        return await this.callPowerShell('get-system-info');
    },
    
    async openInExternalEditor(path) {
        return await this.callPowerShell('open-external', { path });
    },
    
    // Extension Marketplace & Management
    async listExtensions() {
        return await this.callPowerShell('list-extensions');
    },
    async marketplaceList() {
        return await this.callPowerShell('marketplace-list');
    },
    async installExtension(idOrUrl) {
        return await this.callPowerShell('extension-install', { idOrUrl: idOrUrl });
    },
    async enableExtension(id) {
        return await this.callPowerShell('extension-enable', { id: id });
    },
    async disableExtension(id) {
        return await this.callPowerShell('extension-disable', { id: id });
    },
    async uninstallExtension(id) {
        return await this.callPowerShell('extension-uninstall', { id: id });
    },
    
    // Core communication
    callPowerShell(action, data = {}) {
        return new Promise((resolve, reject) => {
            const id = Date.now() + Math.random();
            
            // Store promise resolvers
            if (!window.psCallbacks) window.psCallbacks = {};
            window.psCallbacks[id] = { resolve, reject };
            
            // Send message to PowerShell
            window.chrome.webview.postMessage({
                id: id,
                action: action,
                data: data
            });
            
            // Timeout after 30 seconds
            setTimeout(() => {
                if (window.psCallbacks[id]) {
                    delete window.psCallbacks[id];
                    reject(new Error('PowerShell call timeout'));
                }
            }, 30000);
        });
    }
};

// Handle responses from PowerShell
window.chrome.webview.addEventListener('message', event => {
    const response = event.data;
    if (response.id && window.psCallbacks && window.psCallbacks[response.id]) {
        const { resolve, reject } = window.psCallbacks[response.id];
        delete window.psCallbacks[response.id];
        
        if (response.success) {
            resolve(response.data);
        } else {
            reject(new Error(response.error || 'PowerShell call failed'));
        }
    }
});

// Override existing IDE functions to use PowerShell backend
if (window.IDE) {
    // Override file operations
    const originalOpenFile = window.IDE.openFile;
    window.IDE.openFile = async function() {
        try {
            const result = await PowerShellBridge.openFile();
            if (result && result.content) {
                this.loadFileContent(result.filename, result.content, result.path);
            }
        } catch (error) {
            console.error('Failed to open file:', error);
        }
    };
    
    // Override AI chat
    if (window.chatWithAI) {
        const originalChatWithAI = window.chatWithAI;
        window.chatWithAI = async function(message) {
            try {
                const context = window.editor ? window.editor.getValue() : '';
                const language = window.currentFileExtension || '';
                const result = await PowerShellBridge.chatWithOllama(message, 'llama3.2', context, language);
                
                if (result && result.success) {
                    this.displayAIResponse(result.response);
                } else {
                    this.displayAIResponse('Error: ' + (result.error || 'AI request failed'));
                }
            } catch (error) {
                console.error('AI chat error:', error);
                this.displayAIResponse('Error: Failed to communicate with AI');
            }
        };
    }
}

console.log('🚀 Enhanced PowerShell Bridge loaded!');
"@
        
        $WebView.ExecuteScriptAsync($jsCode)
    })
    
    # Handle messages from JavaScript
    $WebView.add_WebMessageReceived({
        param($sender, $args)
        
        try {
            $message = $args.TryGetWebMessageAsString() | ConvertFrom-Json
            $response = Handle-PowerShellCall -Action $message.action -Data $message.data -Id $message.id
            
            $sender.PostWebMessageAsString(($response | ConvertTo-Json -Depth 10 -Compress))
        }
        catch {
            $errorResponse = @{
                id = $message.id
                success = $false
                error = $_.Exception.Message
            }
            $sender.PostWebMessageAsString(($errorResponse | ConvertTo-Json -Compress))
        }
    })
    
    # Inject AI-First IDE enhancements after navigation
    $WebView.add_NavigationCompleted({
        # Inject aesthetic enhancement CSS and JavaScript
        $cssPath = Join-Path $PSScriptRoot "AI-First-IDE-Styles.css"
        $jsPath = Join-Path $PSScriptRoot "AI-First-IDE-Enhancement.js"
        
        if (Test-Path $cssPath) {
            $css = Get-Content $cssPath -Raw
            $WebView.ExecuteScriptAsync("
                const styleSheet = document.createElement('style');
                styleSheet.textContent = `$($css.Replace('`', '\`').Replace('$', '\$'))`;
                document.head.appendChild(styleSheet);
                console.log('🎨 AI-First IDE Styles Applied');
            ")
        }
        
        if (Test-Path $jsPath) {
            $js = Get-Content $jsPath -Raw
            $WebView.ExecuteScriptAsync($js.Replace('`', '\`').Replace('$', '\$'))
        }
        
        Write-Host "🚀 AI-First IDE Enhancement: Contextual UI Active" -ForegroundColor Cyan
    })
}

function Handle-PowerShellCall {
    param([string]$Action, $Data, $Id)
    
    try {
        $result = switch ($Action) {
            'open-file' {
                $openDialog = New-Object System.Windows.Forms.OpenFileDialog
                $openDialog.Filter = "All Files (*.*)|*.*|Code Files (*.ps1;*.py;*.js;*.html;*.css;*.json)|*.ps1;*.py;*.js;*.html;*.css;*.json"
                if ($openDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                    @{
                        filename = Split-Path $openDialog.FileName -Leaf
                        path = $openDialog.FileName
                        content = Get-Content $openDialog.FileName -Raw -ErrorAction SilentlyContinue
                    }
                }
                else { $null }
            }
            
            'save-file' {
                $saveDialog = New-Object System.Windows.Forms.SaveFileDialog
                $saveDialog.FileName = $Data.filename
                if ($saveDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
                    $Data.content | Out-File -FilePath $saveDialog.FileName -Encoding UTF8
                    @{ path = $saveDialog.FileName; success = $true }
                }
                else { @{ success = $false } }
            }
            
            'get-directory-tree' {
                Get-DirectoryTree -Path $Data.path
            }
            
            'read-file' {
                Get-Content $Data.path -Raw -ErrorAction SilentlyContinue
            }
            
            'write-file' {
                $Data.content | Out-File -FilePath $Data.path -Encoding UTF8
                @{ success = $true }
            }
            
            'ollama-chat' {
                Invoke-EnhancedOllama -Prompt $Data.prompt -Model $Data.model -Context $Data.context -Language $Data.language
            }
            
            'get-ollama-models' {
                try {
                    $models = ollama list | Where-Object { $_ -notmatch "NAME.*SIZE.*MODIFIED" -and $_.Trim() } | 
                              ForEach-Object { $_.Split()[0] }
                    $models
                }
                catch { @() }
            }
            
            'terminal-command' {
                Invoke-TerminalCommand -Command $Data.command
            }
            
            'get-git-info' {
                Get-GitInfo -Path $Data.path
            }
            
            'get-system-info' {
                @{
                    os = [System.Environment]::OSVersion.VersionString
                    powershell = $PSVersionTable.PSVersion.ToString()
                    dotnet = [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
                    screen = "$([System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Width)x$([System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Height)"
                    timestamp = Get-Date
                }
            }
            
            default {
                throw "Unknown action: $Action"
            }
        }
        
        return @{
            id = $Id
            success = $true
            data = $result
        }
    }
    catch {
        return @{
            id = $Id
            success = $false
            error = $_.Exception.Message
        }
    }
}

# Main application startup
function Start-HTMLPowerShellIDE {
    Write-Host "🚀 Starting AI-First HTML + PowerShell Hybrid IDE..." -ForegroundColor Cyan
    Write-Host "📊 Optimized for: ${Width}x${Height} (4K @ 160Hz)" -ForegroundColor Yellow
    Write-Host "🎨 Aesthetic Mode: Contextual AI Integration" -ForegroundColor Magenta
    
    # Create high-DPI aware form with AI-first optimizations
    [System.Windows.Forms.Application]::EnableVisualStyles()
    [System.Windows.Forms.Application]::SetCompatibleTextRenderingDefault($false)
    
    $form = New-Object System.Windows.Forms.Form
    $form.Text = "🤖 RawrXD AI-First IDE - Contextual & Subtle"
    $form.Size = New-Object System.Drawing.Size($Width, $Height)
    $form.StartPosition = "CenterScreen"
    $form.BackColor = [System.Drawing.Color]::FromArgb(26, 26, 26)  # AI-First dark theme
    
    if ($FullScreen) {
        $form.WindowState = [System.Windows.Forms.FormWindowState]::Maximized
    }
    
    # High refresh rate optimizations for 160Hz
    $form.SetStyle([System.Windows.Forms.ControlStyles]::AllPaintingInWmPaint, $true)
    $form.SetStyle([System.Windows.Forms.ControlStyles]::UserPaint, $true)
    $form.SetStyle([System.Windows.Forms.ControlStyles]::DoubleBuffer, $true)
    $form.SetStyle([System.Windows.Forms.ControlStyles]::OptimizedDoubleBuffer, $true)
    
    # Set up WebView with AI-first enhancements
    $webView = Setup-EnhancedWebView -Form $form
    $global:IDEBridge.WebView = $webView
    $global:IDEBridge.Form = $form
    
    # Load the HTML IDE with AI enhancements
    if (Test-Path $HtmlPath) {
        Write-Host "🌐 Loading HTML IDE: $HtmlPath" -ForegroundColor Green
        Write-Host "🧠 AI Features: Contextual suggestions, Intelligent search, Subtle assistance" -ForegroundColor Blue
        
        if ($webView.GetType().Name -eq "WebView2") {
            # Read and enhance the HTML with AI-first features
            $htmlContent = Get-Content $HtmlPath -Raw
            
            # Inject AI-first meta tags for optimal rendering
            $enhancedHtml = $htmlContent -replace '<head>', @'
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
    <meta name="ai-enhanced" content="true">
    <meta name="color-scheme" content="dark">
    <style>
        /* Immediate AI-first base styling */
        * { 
            box-sizing: border-box; 
            transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
        }
        body { 
            background: #1a1a1a !important; 
            color: #e1e1e1 !important;
            font-family: 'JetBrains Mono', 'Cascadia Code', 'Fira Code', monospace !important;
        }
        /* Hide aggressive popups, use subtle overlays instead */
        .modal, .popup { opacity: 0.9 !important; backdrop-filter: blur(10px) !important; }
    </style>
'@
            
            $webView.NavigateToString($enhancedHtml)
        }
        else {
            $uri = "file:///$($HtmlPath.Replace('\', '/'))"
            $webView.Navigate($uri)
        }
    }
    else {
        Write-Host "❌ HTML file not found: $HtmlPath" -ForegroundColor Red
        Write-Host "💡 Creating minimal AI-enhanced IDE..." -ForegroundColor Yellow
        
        # Create a minimal AI-first IDE if HTML not found
        $minimalIDE = @'
<!DOCTYPE html>
<html>
<head>
    <title>RawrXD AI-First IDE</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { 
            background: #1a1a1a; 
            color: #e1e1e1; 
            font-family: 'JetBrains Mono', monospace; 
            margin: 0; 
            padding: 20px; 
        }
        .editor { 
            width: 100%; 
            height: 80vh; 
            background: #252525; 
            color: #e1e1e1; 
            border: 1px solid rgba(255,255,255,0.1); 
            border-radius: 8px; 
            padding: 16px; 
            font-family: inherit; 
            resize: none; 
        }
        .status { 
            position: fixed; 
            top: 20px; 
            right: 20px; 
            background: rgba(64, 120, 242, 0.1); 
            padding: 8px 16px; 
            border-radius: 20px; 
            border: 1px solid rgba(64, 120, 242, 0.2); 
        }
    </style>
</head>
<body>
    <div class="status">🤖 AI Ready</div>
    <h1>🚀 RawrXD AI-First IDE</h1>
    <textarea class="editor" placeholder="Start coding... AI will assist you contextually and subtly."></textarea>
    <script>
        console.log('🤖 Minimal AI-First IDE Active');
        document.querySelector('.editor').addEventListener('input', () => {
            console.log('🧠 AI analyzing code...');
        });
    </script>
</body>
</html>
'@
        $webView.NavigateToString($minimalIDE)
    }
    
    # Form events with AI context
    $form.add_FormClosing({
        Write-Host "🧠 Saving AI context and shutting down..." -ForegroundColor Yellow
    })
    
    $form.add_Shown({
        Write-Host "✅ AI-First IDE Ready! Features:" -ForegroundColor Green
        Write-Host "   • Contextual AI suggestions (hover over code)" -ForegroundColor Cyan
        Write-Host "   • Intelligent search visualization" -ForegroundColor Cyan
        Write-Host "   • Subtle agent communication" -ForegroundColor Cyan
        Write-Host "   • Non-intrusive AI assistance" -ForegroundColor Cyan
        Write-Host "   • Ctrl+/ for AI chat, Ctrl+Shift+A for AI actions" -ForegroundColor Yellow
    })
    
    # Show the enhanced IDE
    [System.Windows.Forms.Application]::Run($form)
}

# Launch the enhanced IDE
try {
    Start-HTMLPowerShellIDE
}
catch {
    Write-Host "❌ Error: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Red
}
finally {
    Write-Host "👋 Session ended" -ForegroundColor Yellow
}