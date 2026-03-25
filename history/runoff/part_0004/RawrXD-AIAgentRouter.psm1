# RawrXD-AIAgentRouter.psm1
# Unified AI Agent Router
# Orchestrates Amazon Q, GitHub Copilot, and Ollama services

$script:RouterConfig = @{
    Services = @{
        AmazonQ = @{
            Enabled = $false
            Priority = 1
            Module = $null
        }
        GitHubCopilot = @{
            Enabled = $false
            Priority = 2
            Module = $null
        }
        Ollama = @{
            Enabled = $false
            Priority = 3
            Module = $null
        }
    }
    DefaultService = "Ollama"
    FallbackChain = @("GitHubCopilot", "AmazonQ", "Ollama")
    RequestHistory = @()
    MaxHistory = 100
}

# Initialize AI Agent Router
function Initialize-AIAgentRouter {
    param(
        [switch]$LoadAmazonQ,
        [switch]$LoadGitHubCopilot,
        [switch]$LoadOllama,
        [string]$DefaultService = "Ollama"
    )

    Write-DevConsole "[AI Router] Initializing..." "INFO"

    # Load Amazon Q if requested
    if ($LoadAmazonQ) {
        try {
            Import-Module (Join-Path $PSScriptRoot "RawrXD-AmazonQ.psm1") -ErrorAction Stop
            $script:RouterConfig.Services.AmazonQ.Module = "RawrXD-AmazonQ"
            $script:RouterConfig.Services.AmazonQ.Enabled = Initialize-AmazonQ
            if ($script:RouterConfig.Services.AmazonQ.Enabled) {
                Write-DevConsole "[AI Router] ✅ Amazon Q loaded" "SUCCESS"
            }
        }
        catch {
            Write-DevConsole "[AI Router] ⚠️ Amazon Q failed to load: $_" "WARNING"
        }
    }

    # Load GitHub Copilot if requested
    if ($LoadGitHubCopilot) {
        try {
            Import-Module (Join-Path $PSScriptRoot "RawrXD.GitHubCopilot.psm1") -ErrorAction Stop
            $script:RouterConfig.Services.GitHubCopilot.Module = "RawrXD.GitHubCopilot"
            $script:RouterConfig.Services.GitHubCopilot.Enabled = Initialize-GitHubCopilot
            if ($script:RouterConfig.Services.GitHubCopilot.Enabled) {
                Write-DevConsole "[AI Router] ✅ GitHub Copilot loaded" "SUCCESS"
            }
        }
        catch {
            Write-DevConsole "[AI Router] ⚠️ GitHub Copilot failed to load: $_" "WARNING"
        }
    }

    # Load Ollama (always available)
    if ($LoadOllama) {
        try {
            $ollamaRunning = Get-Process -Name "ollama" -ErrorAction SilentlyContinue
            if ($ollamaRunning) {
                $script:RouterConfig.Services.Ollama.Enabled = $true
                Write-DevConsole "[AI Router] ✅ Ollama detected (running)" "SUCCESS"
            }
            else {
                Write-DevConsole "[AI Router] ⚠️ Ollama not running" "WARNING"
            }
        }
        catch {
            Write-DevConsole "[AI Router] ⚠️ Ollama check failed: $_" "WARNING"
        }
    }

    $script:RouterConfig.DefaultService = $DefaultService
    Write-DevConsole "[AI Router] ✅ Router initialized (Default: $DefaultService)" "SUCCESS"
}

# Route chat request to appropriate service
function Invoke-AIChat {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,
        
        [Parameter(Mandatory = $false)]
        [string]$Service = $null,
        
        [Parameter(Mandatory = $false)]
        [hashtable]$Context = @{},
        
        [Parameter(Mandatory = $false)]
        [string]$ConversationId = $null
    )

    # Determine which service to use
    if (-not $Service) {
        $Service = $script:RouterConfig.DefaultService
    }

    # Try requested service first
    $result = Invoke-ServiceChat -Service $Service -Message $Message -Context $Context -ConversationId $ConversationId

    # If failed, try fallback chain
    if (-not $result -or -not $result.Success) {
        foreach ($fallbackService in $script:RouterConfig.FallbackChain) {
            if ($fallbackService -ne $Service -and $script:RouterConfig.Services[$fallbackService].Enabled) {
                Write-DevConsole "[AI Router] Trying fallback: $fallbackService" "INFO"
                $result = Invoke-ServiceChat -Service $fallbackService -Message $Message -Context $Context -ConversationId $ConversationId
                if ($result -and $result.Success) {
                    break
                }
            }
        }
    }

    # Log request
    $script:RouterConfig.RequestHistory += @{
        Timestamp = Get-Date
        Service = $Service
        Message = $Message
        Success = $result -and $result.Success
    }
    if ($script:RouterConfig.RequestHistory.Count -gt $script:RouterConfig.MaxHistory) {
        $script:RouterConfig.RequestHistory = $script:RouterConfig.RequestHistory | Select-Object -Last $script:RouterConfig.MaxHistory
    }

    return $result
}

# Invoke chat on specific service
function Invoke-ServiceChat {
    param(
        [string]$Service,
        [string]$Message,
        [hashtable]$Context,
        [string]$ConversationId
    )

    switch ($Service) {
        "AmazonQ" {
            if ($script:RouterConfig.Services.AmazonQ.Enabled) {
                try {
                    $response = Invoke-AmazonQChat -Message $Message -ConversationId $ConversationId -Context $Context
                    return @{
                        Success = $true
                        Service = "AmazonQ"
                        Response = $response.response
                        ConversationId = $response.conversationId
                    }
                }
                catch {
                    return @{ Success = $false; Error = $_.Exception.Message }
                }
            }
        }
        "GitHubCopilot" {
            if ($script:RouterConfig.Services.GitHubCopilot.Enabled) {
                try {
                    $response = Invoke-CopilotChat -Prompt $Message -Context $Context -ConversationId $ConversationId
                    return @{
                        Success = $response.Success
                        Service = "GitHubCopilot"
                        Response = $response.Response
                        ConversationId = $response.ConversationId
                    }
                }
                catch {
                    return @{ Success = $false; Error = $_.Exception.Message }
                }
            }
        }
        "Ollama" {
            if ($script:RouterConfig.Services.Ollama.Enabled) {
                try {
                    # Use existing Ollama integration from RawrXD.ps1
                    $response = Send-OllamaRequest -Message $Message -Context $Context
                    return @{
                        Success = $true
                        Service = "Ollama"
                        Response = $response.response
                    }
                }
                catch {
                    return @{ Success = $false; Error = $_.Exception.Message }
                }
            }
        }
    }

    return @{ Success = $false; Error = "Service $Service not available" }
}

# Get code suggestions from all available services
function Get-AICodeSuggestions {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Code,
        
        [Parameter(Mandatory = $false)]
        [string]$Language = "powershell",
        
        [Parameter(Mandatory = $false)]
        [hashtable]$CursorPosition = @{ Line = 1; Column = 1 },
        
        [Parameter(Mandatory = $false)]
        [string]$Service = $null
    )

    if (-not $Service) {
        $Service = $script:RouterConfig.DefaultService
    }

    $suggestions = @()

    # Try primary service
    $primaryResult = Get-ServiceCodeSuggestions -Service $Service -Code $Code -Language $Language -CursorPosition $CursorPosition
    if ($primaryResult) {
        $suggestions += $primaryResult
    }

    # Try other enabled services for additional suggestions
    foreach ($svc in $script:RouterConfig.Services.Keys) {
        if ($svc -ne $Service -and $script:RouterConfig.Services[$svc].Enabled) {
            $additional = Get-ServiceCodeSuggestions -Service $svc -Code $Code -Language $Language -CursorPosition $CursorPosition
            if ($additional) {
                $suggestions += $additional
            }
        }
    }

    return $suggestions | Sort-Object -Property Score -Descending | Select-Object -First 5
}

# Get code suggestions from specific service
function Get-ServiceCodeSuggestions {
    param(
        [string]$Service,
        [string]$Code,
        [string]$Language,
        [hashtable]$CursorPosition
    )

    switch ($Service) {
        "AmazonQ" {
            if ($script:RouterConfig.Services.AmazonQ.Enabled) {
                $result = Get-AmazonQCodeSuggestions -Code $Code -Language $Language -CursorPosition "$($CursorPosition.Line):$($CursorPosition.Column)"
                return @($result.suggestions | ForEach-Object { @{ Text = $_.text; Score = $_.score; Service = "AmazonQ" } })
            }
        }
        "GitHubCopilot" {
            if ($script:RouterConfig.Services.GitHubCopilot.Enabled) {
                $result = Get-CopilotCodeSuggestions -FileContent $Code -Language $Language -CursorPosition $CursorPosition
                return @($result | ForEach-Object { @{ Text = $_.Text; Score = 0.9; Service = "GitHubCopilot" } })
            }
        }
        "Ollama" {
            if ($script:RouterConfig.Services.Ollama.Enabled) {
                # Ollama doesn't have direct code suggestions API, use chat
                $prompt = "Complete this $Language code: `n$Code"
                $result = Send-OllamaRequest -Message $prompt
                return @(@{ Text = $result.response; Score = 0.7; Service = "Ollama" })
            }
        }
    }

    return @()
}

# Get router status
function Get-AIRouterStatus {
    return @{
        DefaultService = $script:RouterConfig.DefaultService
        Services = @{
            AmazonQ = @{
                Enabled = $script:RouterConfig.Services.AmazonQ.Enabled
                Priority = $script:RouterConfig.Services.AmazonQ.Priority
            }
            GitHubCopilot = @{
                Enabled = $script:RouterConfig.Services.GitHubCopilot.Enabled
                Priority = $script:RouterConfig.Services.GitHubCopilot.Priority
            }
            Ollama = @{
                Enabled = $script:RouterConfig.Services.Ollama.Enabled
                Priority = $script:RouterConfig.Services.Ollama.Priority
            }
        }
        RequestCount = $script:RouterConfig.RequestHistory.Count
        RecentRequests = $script:RouterConfig.RequestHistory | Select-Object -Last 10
    }
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-AIAgentRouter',
    'Invoke-AIChat',
    'Get-AICodeSuggestions',
    'Get-AIRouterStatus'
)

Write-Host "✅ AI Agent Router module loaded" -ForegroundColor Green

