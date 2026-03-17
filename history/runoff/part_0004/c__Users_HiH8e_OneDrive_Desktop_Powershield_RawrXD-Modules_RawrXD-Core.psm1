# RawrXD-Core.psm1 - Core functionality and utilities

# Global variables for the application
$script:LogPath = Join-Path $PSScriptRoot "..\Logs"
$script:SettingsPath = Join-Path $PSScriptRoot "..\Settings"
$script:ConfigFile = Join-Path $script:SettingsPath "config.json"

# Ensure directories exist
if (-not (Test-Path $script:LogPath)) {
    New-Item -Path $script:LogPath -ItemType Directory -Force | Out-Null
}
if (-not (Test-Path $script:SettingsPath)) {
    New-Item -Path $script:SettingsPath -ItemType Directory -Force | Out-Null
}

# Logging functions
function Write-RawrXDLog {
    param(
        [string]$Message,
        [ValidateSet('INFO', 'WARNING', 'ERROR', 'SUCCESS')]
        [string]$Level = 'INFO',
        [string]$Component = 'Core'
    )
    
    $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    $logMessage = "[$timestamp] [$Level] [$Component] $Message"
    
    # Console output with colors
    switch ($Level) {
        'INFO' { Write-Host $logMessage -ForegroundColor White }
        'SUCCESS' { Write-Host $logMessage -ForegroundColor Green }
        'WARNING' { Write-Host $logMessage -ForegroundColor Yellow }
        'ERROR' { Write-Host $logMessage -ForegroundColor Red }
    }
    
    # File logging
    $logFile = Join-Path $script:LogPath "RawrXD-$(Get-Date -Format 'yyyy-MM-dd').log"
    try {
        $logMessage | Out-File -FilePath $logFile -Append -Encoding UTF8
    }
    catch {
        # Silent fail for logging to prevent infinite loops
    }
}

# Settings management
function Import-RawrXDSettings {
    try {
        if (Test-Path $script:ConfigFile) {
            $settings = Get-Content $script:ConfigFile -Raw | ConvertFrom-Json
            Write-RawrXDLog "Settings loaded from $script:ConfigFile" -Level SUCCESS
            return $settings
        }
        else {
            $defaultSettings = Get-DefaultSettings
            Export-RawrXDSettings -Settings $defaultSettings
            Write-RawrXDLog "Created default settings file" -Level INFO
            return $defaultSettings
        }
    }
    catch {
        Write-RawrXDLog "Failed to load settings: $($_.Exception.Message)" -Level ERROR
        return Get-DefaultSettings
    }
}

function Export-RawrXDSettings {
    param($Settings)
    
    try {
        $Settings | ConvertTo-Json -Depth 10 | Out-File -FilePath $script:ConfigFile -Encoding UTF8
        Write-RawrXDLog "Settings saved to $script:ConfigFile" -Level SUCCESS
    }
    catch {
        Write-RawrXDLog "Failed to save settings: $($_.Exception.Message)" -Level ERROR
    }
}

function Get-DefaultSettings {
    return @{
        Version = "2.0.0"
        LastModified = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
        UI = @{
            Theme = "Dark"
            FontFamily = "Consolas"
            FontSize = 10
            WindowSize = @{
                Width = 1200
                Height = 800
            }
            SplitterPositions = @{
                MainSplitter = 600
                LeftSplitter = 200
            }
            ShowLineNumbers = $true
            WordWrap = $false
        }
        Editor = @{
            TabSize = 4
            ConvertTabsToSpaces = $true
            AutoIndent = $true
            ShowWhitespace = $false
            HighlightCurrentLine = $true
            AutoSave = $true
            AutoSaveInterval = 30
        }
        AI = @{
            DefaultModel = "llama3.2"
            OllamaEndpoint = "http://localhost:11434"
            Temperature = 0.7
            MaxTokens = 2048
            AutoComplete = $true
        }
        FileExplorer = @{
            ShowHiddenFiles = $false
            DefaultPath = $env:USERPROFILE
            SortBy = "Name"
            SortOrder = "Ascending"
        }
        Git = @{
            AutoDetectRepos = $true
            ShowBranchInTitle = $true
            AutoCommit = $false
        }
    }
}

# Utility functions
function Test-OllamaConnection {
    try {
        $endpoint = $global:RawrXD.Settings.AI.OllamaEndpoint
        $response = Invoke-RestMethod -Uri "$endpoint/api/version" -Method Get -TimeoutSec 5
        Write-RawrXDLog "Ollama connection successful" -Level SUCCESS
        return $true
    }
    catch {
        Write-RawrXDLog "Ollama connection failed: $($_.Exception.Message)" -Level WARNING
        return $false
    }
}

function Get-OllamaModels {
    try {
        $endpoint = $global:RawrXD.Settings.AI.OllamaEndpoint
        $response = Invoke-RestMethod -Uri "$endpoint/api/tags" -Method Get -TimeoutSec 10
        Write-RawrXDLog "Retrieved $($response.models.Count) Ollama models" -Level SUCCESS
        return $response.models
    }
    catch {
        Write-RawrXDLog "Failed to get Ollama models: $($_.Exception.Message)" -Level ERROR
        return @()
    }
}

function Invoke-OllamaChat {
    param(
        [string]$Model,
        [string]$Prompt,
        [string]$System = "",
        [hashtable]$Options = @{}
    )
    
    try {
        $endpoint = $global:RawrXD.Settings.AI.OllamaEndpoint
        $body = @{
            model = $Model
            prompt = $Prompt
            stream = $false
        }
        
        if ($System) {
            $body.system = $System
        }
        
        if ($Options.Count -gt 0) {
            $body.options = $Options
        }
        
        $response = Invoke-RestMethod -Uri "$endpoint/api/generate" -Method Post -Body ($body | ConvertTo-Json) -ContentType "application/json" -TimeoutSec 60
        Write-RawrXDLog "AI response generated successfully" -Level SUCCESS
        return $response.response
    }
    catch {
        Write-RawrXDLog "AI request failed: $($_.Exception.Message)" -Level ERROR
        return "Sorry, I couldn't process your request. Please check the AI service connection."
    }
}

# File operations
function Get-FileExtensionLanguage {
    param([string]$FilePath)
    
    $extension = [System.IO.Path]::GetExtension($FilePath).ToLower()
    $languageMap = @{
        '.ps1' = 'PowerShell'
        '.psm1' = 'PowerShell'
        '.psd1' = 'PowerShell'
        '.js' = 'JavaScript'
        '.ts' = 'TypeScript'
        '.py' = 'Python'
        '.cs' = 'C#'
        '.cpp' = 'C++'
        '.c' = 'C'
        '.h' = 'C/C++'
        '.java' = 'Java'
        '.json' = 'JSON'
        '.xml' = 'XML'
        '.html' = 'HTML'
        '.htm' = 'HTML'
        '.css' = 'CSS'
        '.md' = 'Markdown'
        '.txt' = 'Text'
        '.log' = 'Log'
        '.yml' = 'YAML'
        '.yaml' = 'YAML'
    }
    
    return $languageMap[$extension] ?? 'Text'
}

function Format-FileSize {
    param([long]$Size)
    
    if ($Size -lt 1KB) { return "$Size B" }
    elseif ($Size -lt 1MB) { return "{0:N1} KB" -f ($Size / 1KB) }
    elseif ($Size -lt 1GB) { return "{0:N1} MB" -f ($Size / 1MB) }
    else { return "{0:N1} GB" -f ($Size / 1GB) }
}

# Git operations
function Get-GitStatus {
    param([string]$Path = (Get-Location).Path)
    
    try {
        Push-Location $Path
        $status = git status --porcelain 2>$null
        $branch = git branch --show-current 2>$null
        
        return @{
            Branch = $branch
            HasChanges = $status.Count -gt 0
            Changes = $status
            IsGitRepo = $true
        }
    }
    catch {
        return @{
            IsGitRepo = $false
        }
    }
    finally {
        Pop-Location
    }
}

# Initialize core module
function Initialize-RawrXDCore {
    Write-RawrXDLog "Initializing RawrXD Core module..." -Level INFO
    
    # Test Ollama connection
    $ollamaConnected = Test-OllamaConnection
    $global:RawrXD.OllamaAvailable = $ollamaConnected
    
    if ($ollamaConnected) {
        $models = Get-OllamaModels
        $global:RawrXD.AvailableModels = $models
        Write-RawrXDLog "Found $($models.Count) AI models available" -Level INFO
    }
    
    Write-RawrXDLog "Core module initialized successfully" -Level SUCCESS
}

# Export functions
Export-ModuleMember -Function @(
    'Write-RawrXDLog',
    'Import-RawrXDSettings',
    'Export-RawrXDSettings', 
    'Get-DefaultSettings',
    'Test-OllamaConnection',
    'Get-OllamaModels',
    'Invoke-OllamaChat',
    'Get-FileExtensionLanguage',
    'Format-FileSize',
    'Get-GitStatus',
    'Initialize-RawrXDCore'
)