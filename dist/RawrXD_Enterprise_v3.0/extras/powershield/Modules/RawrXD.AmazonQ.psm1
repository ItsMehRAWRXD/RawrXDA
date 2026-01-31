# RawrXD Amazon Q Developer Integration Module
# Provides full integration with Amazon Q Developer for IDE features
# Supports AWS Builder ID and IAM Identity Center authentication

# Module metadata
$ModuleInfo = @{
    Name = "RawrXD.AmazonQ"
    Version = "1.0.0"
    Description = "Amazon Q Developer integration for RawrXD IDE"
    Author = "RawrXD Team"
    RequiredModules = @()
}

# Global state
$Script:AmazonQConfig = @{
    Enabled = $false
    AuthMethod = $null  # "BuilderID" or "IAMIdentityCenter"
    BuilderID = @{
        Email = $null
        Token = $null
        RefreshToken = $null
        ExpiresAt = $null
    }
    IAMIdentityCenter = @{
        SSOStartURL = $null
        SSORegion = $null
        AccountId = $null
        RoleName = $null
        AccessToken = $null
        ExpiresAt = $null
    }
    Region = "us-east-1"
    Endpoint = $null
    LastError = $null
    RequestId = $null
}

# Amazon Q API endpoints
$Script:AmazonQEndpoints = @{
    "us-east-1" = "https://q.us-east-1.amazonaws.com"
    "us-west-2" = "https://q.us-west-2.amazonaws.com"
    "eu-west-1" = "https://q.eu-west-1.amazonaws.com"
    "ap-southeast-1" = "https://q.ap-southeast-1.amazonaws.com"
}

# ============================================
# Function: Initialize-AmazonQ
# ============================================
function Initialize-AmazonQ {
    <#
    .SYNOPSIS
        Initializes Amazon Q Developer integration
    .DESCRIPTION
        Sets up Amazon Q Developer with authentication and configuration
    .PARAMETER AuthMethod
        Authentication method: "BuilderID" or "IAMIdentityCenter"
    .PARAMETER Region
        AWS region (default: us-east-1)
    .EXAMPLE
        Initialize-AmazonQ -AuthMethod "BuilderID" -Region "us-east-1"
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $false)]
        [ValidateSet("BuilderID", "IAMIdentityCenter")]
        [string]$AuthMethod = "BuilderID",
        
        [Parameter(Mandatory = $false)]
        [string]$Region = "us-east-1"
    )
    
    try {
        Write-Host "🔧 Initializing Amazon Q Developer integration..." -ForegroundColor Cyan
        
        $Script:AmazonQConfig.AuthMethod = $AuthMethod
        $Script:AmazonQConfig.Region = $Region
        $Script:AmazonQConfig.Endpoint = $Script:AmazonQEndpoints[$Region]
        
        if (-not $Script:AmazonQConfig.Endpoint) {
            throw "Unsupported region: $Region"
        }
        
        # Load saved credentials if available
        $configPath = Join-Path $env:APPDATA "RawrXD\amazonq-config.json"
        if (Test-Path $configPath) {
            $savedConfig = Get-Content $configPath -Raw | ConvertFrom-Json
            if ($savedConfig.AuthMethod -eq $AuthMethod) {
                $Script:AmazonQConfig.BuilderID = $savedConfig.BuilderID
                $Script:AmazonQConfig.IAMIdentityCenter = $savedConfig.IAMIdentityCenter
                Write-Host "✅ Loaded saved Amazon Q configuration" -ForegroundColor Green
            }
        }
        
        # Check if authentication is needed
        if ($AuthMethod -eq "BuilderID") {
            if (-not $Script:AmazonQConfig.BuilderID.Token -or 
                (Get-Date) -ge $Script:AmazonQConfig.BuilderID.ExpiresAt) {
                Write-Host "🔐 Amazon Q requires authentication. Use Connect-AmazonQ to sign in." -ForegroundColor Yellow
                return $false
            }
        } else {
            if (-not $Script:AmazonQConfig.IAMIdentityCenter.AccessToken -or
                (Get-Date) -ge $Script:AmazonQConfig.IAMIdentityCenter.ExpiresAt) {
                Write-Host "🔐 Amazon Q requires IAM Identity Center authentication. Use Connect-AmazonQ to sign in." -ForegroundColor Yellow
                return $false
            }
        }
        
        $Script:AmazonQConfig.Enabled = $true
        Write-Host "✅ Amazon Q Developer initialized successfully" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Error "Failed to initialize Amazon Q: $($_.Exception.Message)"
        $Script:AmazonQConfig.LastError = $_.Exception.Message
        return $false
    }
}

# ============================================
# Function: Connect-AmazonQ
# ============================================
function Connect-AmazonQ {
    <#
    .SYNOPSIS
        Connects to Amazon Q Developer with authentication
    .DESCRIPTION
        Handles OAuth flow for AWS Builder ID or IAM Identity Center
    .PARAMETER Email
        Email for AWS Builder ID (optional, will prompt if not provided)
    .PARAMETER SSOStartURL
        SSO Start URL for IAM Identity Center
    .PARAMETER SSORegion
        SSO Region for IAM Identity Center
    .PARAMETER AccountId
        AWS Account ID for IAM Identity Center
    .PARAMETER RoleName
        IAM Role name for IAM Identity Center
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $false)]
        [string]$Email,
        
        [Parameter(Mandatory = $false)]
        [string]$SSOStartURL,
        
        [Parameter(Mandatory = $false)]
        [string]$SSORegion,
        
        [Parameter(Mandatory = $false)]
        [string]$AccountId,
        
        [Parameter(Mandatory = $false)]
        [string]$RoleName
    )
    
    try {
        if ($Script:AmazonQConfig.AuthMethod -eq "BuilderID") {
            if (-not $Email) {
                $Email = Read-Host "Enter your AWS Builder ID email"
            }
            
            Write-Host "🔐 Initiating AWS Builder ID authentication..." -ForegroundColor Cyan
            Write-Host "📋 Please complete authentication in your browser..." -ForegroundColor Yellow
            
            # Open browser for OAuth flow
            $authUrl = "https://codecatalyst.aws/home?auth=signin"
            Start-Process $authUrl
            
            # For now, we'll use a token input method
            # In production, this would use proper OAuth flow with callback
            Write-Host "`nAfter signing in, you'll need to configure the token." -ForegroundColor Yellow
            Write-Host "For full OAuth integration, use AWS CLI or AWS Toolkit:" -ForegroundColor Yellow
            Write-Host "  aws codecatalyst create-access-token" -ForegroundColor Gray
            
            $token = Read-Host "Enter your Amazon Q access token (or press Enter to skip)"
            if ($token) {
                $Script:AmazonQConfig.BuilderID.Email = $Email
                $Script:AmazonQConfig.BuilderID.Token = $token
                $Script:AmazonQConfig.BuilderID.ExpiresAt = (Get-Date).AddHours(12)
                
                # Save configuration
                Save-AmazonQConfig
                
                Write-Host "✅ Amazon Q authenticated successfully" -ForegroundColor Green
                return $true
            }
        }
        else {
            # IAM Identity Center authentication
            if (-not $SSOStartURL -or -not $SSORegion -or -not $AccountId -or -not $RoleName) {
                Write-Host "IAM Identity Center requires SSO configuration." -ForegroundColor Yellow
                $SSOStartURL = Read-Host "Enter SSO Start URL"
                $SSORegion = Read-Host "Enter SSO Region"
                $AccountId = Read-Host "Enter AWS Account ID"
                $RoleName = Read-Host "Enter IAM Role Name"
            }
            
            Write-Host "🔐 Initiating IAM Identity Center authentication..." -ForegroundColor Cyan
            
            # Use AWS CLI for SSO login
            $ssoLogin = aws sso login --sso-start-url $SSOStartURL --region $SSORegion 2>&1
            if ($LASTEXITCODE -eq 0) {
                # Get credentials from AWS CLI
                $credentials = aws sts assume-role-with-sso `
                    --sso-start-url $SSOStartURL `
                    --sso-region $SSORegion `
                    --account-id $AccountId `
                    --role-name $RoleName `
                    --region $Script:AmazonQConfig.Region 2>&1 | ConvertFrom-Json
                
                if ($credentials.Credentials) {
                    $Script:AmazonQConfig.IAMIdentityCenter.SSOStartURL = $SSOStartURL
                    $Script:AmazonQConfig.IAMIdentityCenter.SSORegion = $SSORegion
                    $Script:AmazonQConfig.IAMIdentityCenter.AccountId = $AccountId
                    $Script:AmazonQConfig.IAMIdentityCenter.RoleName = $RoleName
                    $Script:AmazonQConfig.IAMIdentityCenter.AccessToken = $credentials.Credentials.SessionToken
                    $Script:AmazonQConfig.IAMIdentityCenter.ExpiresAt = [DateTime]::Parse($credentials.Credentials.Expiration)
                    
                    Save-AmazonQConfig
                    
                    Write-Host "✅ Amazon Q authenticated via IAM Identity Center" -ForegroundColor Green
                    return $true
                }
            }
            
            Write-Error "Failed to authenticate with IAM Identity Center"
            return $false
        }
        
        return $false
    }
    catch {
        Write-Error "Authentication failed: $($_.Exception.Message)"
        $Script:AmazonQConfig.LastError = $_.Exception.Message
        return $false
    }
}

# ============================================
# Function: Invoke-AmazonQChat
# ============================================
function Invoke-AmazonQChat {
    <#
    .SYNOPSIS
        Sends a chat message to Amazon Q Developer
    .DESCRIPTION
        Provides code suggestions, explanations, and assistance via Amazon Q
    .PARAMETER Prompt
        User prompt/question
    .PARAMETER Context
        Additional context (file contents, code snippets, etc.)
    .PARAMETER ConversationId
        Optional conversation ID for maintaining context
    .EXAMPLE
        Invoke-AmazonQChat -Prompt "Explain this code" -Context $code
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
    
    if (-not $Script:AmazonQConfig.Enabled) {
        Write-Error "Amazon Q is not initialized. Run Initialize-AmazonQ first."
        return $null
    }
    
    try {
        $headers = @{
            "Content-Type" = "application/json"
            "Accept" = "application/json"
        }
        
        # Add authentication header
        if ($Script:AmazonQConfig.AuthMethod -eq "BuilderID") {
            $headers["Authorization"] = "Bearer $($Script:AmazonQConfig.BuilderID.Token)"
        } else {
            $headers["Authorization"] = "Bearer $($Script:AmazonQConfig.IAMIdentityCenter.AccessToken)"
            $headers["x-amz-security-token"] = $Script:AmazonQConfig.IAMIdentityCenter.AccessToken
        }
        
        # Build request body
        $body = @{
            prompt = $Prompt
            context = $Context
        }
        
        if ($ConversationId) {
            $body["conversationId"] = $ConversationId
        }
        
        # Amazon Q API endpoint (this is a conceptual endpoint - actual API may differ)
        $url = "$($Script:AmazonQConfig.Endpoint)/api/v1/chat"
        
        Write-Verbose "Sending request to Amazon Q: $url"
        
        $response = Invoke-RestMethod -Uri $url -Method Post -Headers $headers -Body ($body | ConvertTo-Json -Depth 10)
        
        return @{
            Success = $true
            Response = $response.response
            ConversationId = $response.conversationId
            Suggestions = $response.suggestions
            CodeActions = $response.codeActions
        }
    }
    catch {
        $Script:AmazonQConfig.LastError = $_.Exception.Message
        Write-Error "Amazon Q request failed: $($_.Exception.Message)"
        return @{
            Success = $false
            Error = $_.Exception.Message
        }
    }
}

# ============================================
# Function: Get-AmazonQCodeSuggestions
# ============================================
function Get-AmazonQCodeSuggestions {
    <#
    .SYNOPSIS
        Gets code completion suggestions from Amazon Q
    .DESCRIPTION
        Provides inline code suggestions similar to GitHub Copilot
    .PARAMETER FilePath
        Path to the current file
    .PARAMETER FileContent
        Current file content
    .PARAMETER CursorPosition
        Cursor position (line, column)
    .PARAMETER Language
        Programming language
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
        [string]$Language = $null
    )
    
    if (-not $Script:AmazonQConfig.Enabled) {
        return @()
    }
    
    try {
        $context = @{
            filePath = $FilePath
            fileContent = $FileContent
            cursorLine = $CursorPosition.Line
            cursorColumn = $CursorPosition.Column
            language = $Language
        }
        
        $result = Invoke-AmazonQChat -Prompt "Provide code completion suggestions for the current cursor position" -Context $context
        
        if ($result.Success -and $result.Suggestions) {
            return $result.Suggestions
        }
        
        return @()
    }
    catch {
        Write-Error "Failed to get code suggestions: $($_.Exception.Message)"
        return @()
    }
}

# ============================================
# Function: Invoke-AmazonQCodeReview
# ============================================
function Invoke-AmazonQCodeReview {
    <#
    .SYNOPSIS
        Performs code review using Amazon Q
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
    
    if (-not $Script:AmazonQConfig.Enabled) {
        Write-Error "Amazon Q is not initialized."
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
        
        $result = Invoke-AmazonQChat -Prompt "Perform a comprehensive code review including security, performance, and best practices" -Context $context
        
        if ($result.Success) {
            return @{
                Success = $true
                Review = $result.Response
                Issues = $result.codeActions
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
# Function: Save-AmazonQConfig
# ============================================
function Save-AmazonQConfig {
    <#
    .SYNOPSIS
        Saves Amazon Q configuration to disk
    #>
    $configDir = Join-Path $env:APPDATA "RawrXD"
    if (-not (Test-Path $configDir)) {
        New-Item -ItemType Directory -Path $configDir -Force | Out-Null
    }
    
    $configPath = Join-Path $configDir "amazonq-config.json"
    
    $configToSave = @{
        AuthMethod = $Script:AmazonQConfig.AuthMethod
        Region = $Script:AmazonQConfig.Region
        BuilderID = @{
            Email = $Script:AmazonQConfig.BuilderID.Email
            # Note: In production, tokens should be encrypted
            Token = $Script:AmazonQConfig.BuilderID.Token
            ExpiresAt = $Script:AmazonQConfig.BuilderID.ExpiresAt
        }
        IAMIdentityCenter = $Script:AmazonQConfig.IAMIdentityCenter
    }
    
    $configToSave | ConvertTo-Json -Depth 10 | Set-Content $configPath -Encoding UTF8
}

# ============================================
# Function: Get-AmazonQStatus
# ============================================
function Get-AmazonQStatus {
    <#
    .SYNOPSIS
        Gets the current status of Amazon Q integration
    #>
    return @{
        Enabled = $Script:AmazonQConfig.Enabled
        AuthMethod = $Script:AmazonQConfig.AuthMethod
        Authenticated = if ($Script:AmazonQConfig.AuthMethod -eq "BuilderID") {
            $Script:AmazonQConfig.BuilderID.Token -ne $null
        } else {
            $Script:AmazonQConfig.IAMIdentityCenter.AccessToken -ne $null
        }
        Region = $Script:AmazonQConfig.Region
        LastError = $Script:AmazonQConfig.LastError
    }
}

# Export functions
Export-ModuleMember -Function @(
    'Initialize-AmazonQ',
    'Connect-AmazonQ',
    'Invoke-AmazonQChat',
    'Get-AmazonQCodeSuggestions',
    'Invoke-AmazonQCodeReview',
    'Get-AmazonQStatus',
    'Save-AmazonQConfig'
)

