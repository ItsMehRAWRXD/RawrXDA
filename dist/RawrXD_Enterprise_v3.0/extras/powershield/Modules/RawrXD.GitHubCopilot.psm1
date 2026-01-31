# RawrXD GitHub Copilot Integration Module
# Provides full integration with GitHub Copilot for IDE features
# Supports OAuth authentication and Copilot API access

# Module metadata
$ModuleInfo = @{
    Name = "RawrXD.GitHubCopilot"
    Version = "1.0.0"
    Description = "GitHub Copilot integration for RawrXD IDE"
    Author = "RawrXD Team"
    RequiredModules = @()
}

# Global state
$Script:CopilotConfig = @{
    Enabled = $false
    Authenticated = $false
    GitHubToken = $null
    GitHubUser = $null
    SubscriptionActive = $false
    LastError = $null
    RequestId = $null
    RateLimitRemaining = 0
    RateLimitReset = $null
}

# GitHub Copilot API endpoints
$Script:CopilotEndpoints = @{
    API = "https://api.githubcopilot.com"
    Auth = "https://github.com/login/oauth"
    CopilotAPI = "https://copilot-proxy.githubusercontent.com"
    OAuthClientId = "Iv1.8a61f9b3a7aba766"  # GitHub OAuth App Client ID
    OAuthRedirectUri = "http://localhost:8888/oauth/callback"
}

# ============================================
# Function: Initialize-GitHubCopilot
# ============================================
function Initialize-GitHubCopilot {
    <#
    .SYNOPSIS
        Initializes GitHub Copilot integration
    .DESCRIPTION
        Sets up GitHub Copilot with authentication and configuration
    .PARAMETER GitHubToken
        Optional GitHub personal access token (will prompt if not provided)
    .EXAMPLE
        Initialize-GitHubCopilot
        Initialize-GitHubCopilot -GitHubToken "ghp_xxxxxxxxxxxx"
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $false)]
        [string]$GitHubToken = $null
    )
    
    try {
        Write-Host "🔧 Initializing GitHub Copilot integration..." -ForegroundColor Cyan
        
        # Load saved token if available
        $configPath = Join-Path $env:APPDATA "RawrXD\copilot-config.json"
        if (Test-Path $configPath) {
            $savedConfig = Get-Content $configPath -Raw | ConvertFrom-Json
            if ($savedConfig.GitHubToken) {
                $Script:CopilotConfig.GitHubToken = $savedConfig.GitHubToken
                $Script:CopilotConfig.GitHubUser = $savedConfig.GitHubUser
                Write-Host "✅ Loaded saved GitHub Copilot configuration" -ForegroundColor Green
            }
        }
        
        # Use provided token or saved token
        if ($GitHubToken) {
            $Script:CopilotConfig.GitHubToken = $GitHubToken
        }
        
        # Verify token and subscription
        if ($Script:CopilotConfig.GitHubToken) {
            $status = Test-GitHubCopilotAuth
            if ($status.Authenticated) {
                $Script:CopilotConfig.Enabled = $true
                $Script:CopilotConfig.Authenticated = $true
                $Script:CopilotConfig.SubscriptionActive = $status.SubscriptionActive
                Write-Host "✅ GitHub Copilot initialized and authenticated" -ForegroundColor Green
                return $true
            } else {
                Write-Host "⚠️  Authentication failed. Please check your token." -ForegroundColor Yellow
                return $false
            }
        } else {
            Write-Host "🔐 GitHub Copilot requires authentication. Use Connect-GitHubCopilot to sign in." -ForegroundColor Yellow
            return $false
        }
    }
    catch {
        Write-Error "Failed to initialize GitHub Copilot: $($_.Exception.Message)"
        $Script:CopilotConfig.LastError = $_.Exception.Message
        return $false
    }
}

# ============================================
# Function: Connect-GitHubCopilot
# ============================================
function Connect-GitHubCopilot {
    <#
    .SYNOPSIS
        Connects to GitHub Copilot with OAuth authentication
    .DESCRIPTION
        Handles OAuth flow for GitHub Copilot access. Supports both OAuth flow and direct token input.
    .PARAMETER GitHubToken
        Optional GitHub personal access token with copilot scope
    .PARAMETER UseOAuth
        Use OAuth browser flow instead of manual token entry
    .EXAMPLE
        Connect-GitHubCopilot
        Connect-GitHubCopilot -GitHubToken "ghp_xxxxxxxxxxxx"
        Connect-GitHubCopilot -UseOAuth
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $false)]
        [string]$GitHubToken = $null,
        
        [Parameter(Mandatory = $false)]
        [switch]$UseOAuth = $false
    )
    
    try {
        if ($GitHubToken) {
            $Script:CopilotConfig.GitHubToken = $GitHubToken
        }
        elseif ($UseOAuth) {
            # OAuth flow
            Write-Host "🔐 Starting GitHub OAuth flow..." -ForegroundColor Cyan
            
            $state = [Guid]::NewGuid().ToString()
            $scope = "read:user user:email copilot"
            $authUrl = "https://github.com/login/oauth/authorize?client_id=$($Script:CopilotEndpoints.OAuthClientId)&redirect_uri=$($Script:CopilotEndpoints.OAuthRedirectUri)&scope=$scope&state=$state"
            
            Write-Host "Opening browser for OAuth authentication..." -ForegroundColor Yellow
            Start-Process $authUrl
            
            # Note: In production, you'd set up a local server to receive the callback
            # For now, fall back to manual token entry
            Write-Host "`nAfter authorizing, you'll receive a code. Enter it below:" -ForegroundColor Yellow
            $code = Read-Host "OAuth Code"
            
            # Exchange code for token (simplified - would need OAuth app secret in production)
            Write-Host "⚠️ OAuth code exchange requires OAuth app secret. Using manual token entry instead." -ForegroundColor Yellow
            $token = Read-Host "Enter your GitHub Personal Access Token" -AsSecureString
            $BSTR = [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($token)
            $Script:CopilotConfig.GitHubToken = [System.Runtime.InteropServices.Marshal]::PtrToStringAuto($BSTR)
        }
        else {
            Write-Host "🔐 GitHub Copilot Authentication" -ForegroundColor Cyan
            Write-Host "`nTo use GitHub Copilot, you need a GitHub Personal Access Token with 'copilot' scope." -ForegroundColor Yellow
            Write-Host "1. Go to: https://github.com/settings/tokens" -ForegroundColor Gray
            Write-Host "2. Click 'Generate new token (classic)'" -ForegroundColor Gray
            Write-Host "3. Select 'copilot' scope" -ForegroundColor Gray
            Write-Host "4. Copy the token and paste it below`n" -ForegroundColor Gray
            Write-Host "   Or use: Connect-GitHubCopilot -UseOAuth (for browser-based OAuth)" -ForegroundColor Gray
            Write-Host ""
            
            $token = Read-Host "Enter your GitHub Personal Access Token" -AsSecureString
            $BSTR = [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($token)
            $Script:CopilotConfig.GitHubToken = [System.Runtime.InteropServices.Marshal]::PtrToStringAuto($BSTR)
        }
        
        # Verify token
        $status = Test-GitHubCopilotAuth
        if ($status.Authenticated) {
            $Script:CopilotConfig.Authenticated = $true
            $Script:CopilotConfig.SubscriptionActive = $status.SubscriptionActive
            $Script:CopilotConfig.GitHubUser = $status.Username
            
            # Save configuration
            Save-CopilotConfig
            
            Write-Host "✅ GitHub Copilot authenticated successfully" -ForegroundColor Green
            Write-Host "   User: $($Script:CopilotConfig.GitHubUser)" -ForegroundColor Gray
            Write-Host "   Subscription: $(if ($Script:CopilotConfig.SubscriptionActive) { 'Active' } else { 'Inactive' })" -ForegroundColor Gray
            return $true
        } else {
            Write-Error "Authentication failed: $($status.Error)"
            return $false
        }
    }
    catch {
        Write-Error "Connection failed: $($_.Exception.Message)"
        $Script:CopilotConfig.LastError = $_.Exception.Message
        return $false
    }
}

# ============================================
# Function: Test-GitHubCopilotAuth
# ============================================
function Test-GitHubCopilotAuth {
    <#
    .SYNOPSIS
        Tests GitHub Copilot authentication and subscription status
    #>
    try {
        if (-not $Script:CopilotConfig.GitHubToken) {
            return @{
                Authenticated = $false
                SubscriptionActive = $false
                Error = "No token provided"
            }
        }
        
        $headers = @{
            "Authorization" = "Bearer $($Script:CopilotConfig.GitHubToken)"
            "Accept" = "application/vnd.github+json"
            "X-GitHub-Api-Version" = "2022-11-28"
        }
        
        # Test GitHub API access
        $userResponse = Invoke-RestMethod -Uri "https://api.github.com/user" -Method Get -Headers $headers -ErrorAction Stop
        
        # Check Copilot subscription
        $copilotResponse = Invoke-RestMethod -Uri "https://api.github.com/user/copilot/subscription" -Method Get -Headers $headers -ErrorAction Stop
        
        # Check rate limits
        $rateLimitResponse = Invoke-RestMethod -Uri "https://api.github.com/rate_limit" -Method Get -Headers $headers -ErrorAction Stop
        $Script:CopilotConfig.RateLimitRemaining = $rateLimitResponse.resources.core.remaining
        $Script:CopilotConfig.RateLimitReset = [DateTimeOffset]::FromUnixTimeSeconds($rateLimitResponse.resources.core.reset).DateTime
        
        return @{
            Authenticated = $true
            SubscriptionActive = $copilotResponse.subscription_active -eq $true
            Username = $userResponse.login
            RateLimitRemaining = $Script:CopilotConfig.RateLimitRemaining
        }
    }
    catch {
        return @{
            Authenticated = $false
            SubscriptionActive = $false
            Error = $_.Exception.Message
        }
    }
}

# ============================================
# Function: Invoke-CopilotChat
# ============================================
function Invoke-CopilotChat {
    <#
    .SYNOPSIS
        Sends a chat message to GitHub Copilot
    .DESCRIPTION
        Provides code suggestions, explanations, and assistance via Copilot Chat
    .PARAMETER Prompt
        User prompt/question
    .PARAMETER Context
        Additional context (file contents, code snippets, etc.)
    .PARAMETER ConversationId
        Optional conversation ID for maintaining context
    .EXAMPLE
        Invoke-CopilotChat -Prompt "Explain this code" -Context @{code = $code}
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Prompt,
        
        [Parameter(Mandatory = $false)]
        [hashtable]$Context = @{},
        
        [Parameter(Mandatory = $false)]
        [string]$ConversationId = $null
    )
    
    if (-not $Script:CopilotConfig.Enabled -or -not $Script:CopilotConfig.Authenticated) {
        Write-Error "GitHub Copilot is not authenticated. Run Connect-GitHubCopilot first."
        return $null
    }
    
    if (-not $Script:CopilotConfig.SubscriptionActive) {
        Write-Error "GitHub Copilot subscription is not active."
        return $null
    }
    
    try {
        $headers = @{
            "Authorization" = "Bearer $($Script:CopilotConfig.GitHubToken)"
            "Content-Type" = "application/json"
            "Accept" = "application/json"
        }
        
        # Build request body for Copilot Chat API
        $body = @{
            messages = @(
                @{
                    role = "user"
                    content = $Prompt
                }
            )
        }
        
        if ($Context.Count -gt 0) {
            $body["context"] = $Context
        }
        
        if ($ConversationId) {
            $body["conversation_id"] = $ConversationId
        }
        
        # GitHub Copilot Chat API endpoint
        $url = "$($Script:CopilotEndpoints.CopilotAPI)/v1/chat/completions"
        
        Write-Verbose "Sending request to GitHub Copilot: $url"
        
        $response = Invoke-RestMethod -Uri $url -Method Post -Headers $headers -Body ($body | ConvertTo-Json -Depth 10) -ErrorAction Stop
        
        return @{
            Success = $true
            Response = $response.choices[0].message.content
            ConversationId = $response.conversation_id
            Suggestions = $response.suggestions
        }
    }
    catch {
        $Script:CopilotConfig.LastError = $_.Exception.Message
        Write-Error "Copilot request failed: $($_.Exception.Message)"
        return @{
            Success = $false
            Error = $_.Exception.Message
        }
    }
}

# ============================================
# Function: Get-CopilotCodeSuggestions
# ============================================
function Get-CopilotCodeSuggestions {
    <#
    .SYNOPSIS
        Gets code completion suggestions from GitHub Copilot
    .DESCRIPTION
        Provides inline code suggestions (autocomplete)
    .PARAMETER FilePath
        Path to the current file
    .PARAMETER FileContent
        Current file content
    .PARAMETER CursorPosition
        Cursor position (line, column)
    .PARAMETER Language
        Programming language
    .PARAMETER MaxSuggestions
        Maximum number of suggestions to return
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        
        [Parameter(Mandatory = $true)]
        [string]$FileContent,
        
        [Parameter(Mandatory = $true)]
        [hashtable]$CursorPosition,
        
        [Parameter(Mandatory = $false)]
        [string]$Language = $null,
        
        [Parameter(Mandatory = $false)]
        [int]$MaxSuggestions = 3
    )
    
    if (-not $Script:CopilotConfig.Enabled -or -not $Script:CopilotConfig.Authenticated) {
        return @()
    }
    
    if (-not $Script:CopilotConfig.SubscriptionActive) {
        return @()
    }
    
    try {
        $headers = @{
            "Authorization" = "Bearer $($Script:CopilotConfig.GitHubToken)"
            "Content-Type" = "application/json"
            "Accept" = "application/json"
        }
        
        # Build request for Copilot completions API
        $body = @{
            file_path = $FilePath
            file_content = $FileContent
            cursor_line = $CursorPosition.Line
            cursor_column = $CursorPosition.Column
            max_suggestions = $MaxSuggestions
        }
        
        if ($Language) {
            $body["language"] = $Language
        }
        
        # GitHub Copilot Completions API endpoint
        $url = "$($Script:CopilotEndpoints.CopilotAPI)/v1/engines/copilot-codex/completions"
        
        Write-Verbose "Requesting code suggestions from GitHub Copilot"
        
        $response = Invoke-RestMethod -Uri $url -Method Post -Headers $headers -Body ($body | ConvertTo-Json -Depth 10) -ErrorAction Stop
        
        $suggestions = @()
        foreach ($choice in $response.choices) {
            $suggestions += @{
                Text = $choice.text
                Index = $choice.index
                FinishReason = $choice.finish_reason
            }
        }
        
        return $suggestions
    }
    catch {
        Write-Verbose "Failed to get code suggestions: $($_.Exception.Message)"
        return @()
    }
}

# ============================================
# Function: Invoke-CopilotCodeReview
# ============================================
function Invoke-CopilotCodeReview {
    <#
    .SYNOPSIS
        Performs code review using GitHub Copilot
    .DESCRIPTION
        Analyzes code for issues, security vulnerabilities, and improvements
    .PARAMETER FilePath
        Path to file to review
    .PARAMETER FileContent
        Content of file to review (optional if FilePath provided)
    .PARAMETER Language
        Programming language
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $false)]
        [string]$FilePath = $null,
        
        [Parameter(Mandatory = $false)]
        [string]$FileContent = $null,
        
        [Parameter(Mandatory = $false)]
        [string]$Language = $null
    )
    
    if (-not $Script:CopilotConfig.Enabled -or -not $Script:CopilotConfig.Authenticated) {
        Write-Error "GitHub Copilot is not authenticated."
        return $null
    }
    
    if ($FilePath -and -not $FileContent) {
        $FileContent = Get-Content $FilePath -Raw
    }
    
    if (-not $FileContent) {
        Write-Error "No file content provided"
        return $null
    }
    
    try {
        $context = @{
            fileContent = $FileContent
            language = $Language
            reviewType = "comprehensive"
        }
        
        $prompt = "Perform a comprehensive code review including security vulnerabilities, performance issues, best practices, and potential bugs. Provide specific recommendations."
        
        $result = Invoke-CopilotChat -Prompt $prompt -Context $context
        
        if ($result.Success) {
            return @{
                Success = $true
                Review = $result.Response
                Recommendations = $result.Suggestions
            }
        }
        
        return $result
    }
    catch {
        Write-Error "Code review failed: $($_.Exception.Message)"
        return @{
            Success = $false
            Error = $_.Exception.Message
        }
    }
}

# ============================================
# Function: Save-CopilotConfig
# ============================================
function Save-CopilotConfig {
    <#
    .SYNOPSIS
        Saves GitHub Copilot configuration to disk
    #>
    $configDir = Join-Path $env:APPDATA "RawrXD"
    if (-not (Test-Path $configDir)) {
        New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    }
    
    $configPath = Join-Path $configDir "copilot-config.json"
    
    $configToSave = @{
        GitHubToken = $Script:CopilotConfig.GitHubToken
        GitHubUser = $Script:CopilotConfig.GitHubUser
        SubscriptionActive = $Script:CopilotConfig.SubscriptionActive
        # Note: In production, tokens should be encrypted
    }
    
    $configToSave | ConvertTo-Json -Depth 10 | Set-Content $configPath -Encoding UTF8
}

# ============================================
# Function: Get-CopilotStatus
# ============================================
function Get-CopilotStatus {
    <#
    .SYNOPSIS
        Gets the current status of GitHub Copilot integration
    #>
    return @{
        Enabled = $Script:CopilotConfig.Enabled
        Authenticated = $Script:CopilotConfig.Authenticated
        SubscriptionActive = $Script:CopilotConfig.SubscriptionActive
        GitHubUser = $Script:CopilotConfig.GitHubUser
        RateLimitRemaining = $Script:CopilotConfig.RateLimitRemaining
        RateLimitReset = $Script:CopilotConfig.RateLimitReset
        LastError = $Script:CopilotConfig.LastError
    }
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-GitHubCopilot',
    'Connect-GitHubCopilot',
    'Test-GitHubCopilotAuth',
    'Invoke-CopilotChat',
    'Get-CopilotCodeSuggestions',
    'Invoke-CopilotCodeReview',
    'Get-CopilotStatus',
    'Save-CopilotConfig'
)

