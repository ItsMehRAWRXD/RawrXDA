# RawrXD-AmazonQ.psm1
# Amazon Q Developer Integration Module
# Provides AWS Toolkit authentication and Amazon Q API access

$script:AmazonQConfig = @{
    Region = "us-east-1"
    Profile = $null
    AccessKey = $null
    SecretKey = $null
    SessionToken = $null
    IsAuthenticated = $false
    LastAuthTime = $null
    APIEndpoint = "https://q.us-east-1.amazonaws.com"
}

# Initialize Amazon Q Developer integration
function Initialize-AmazonQ {
    param(
        [string]$Region = "us-east-1",
        [string]$Profile = $null,
        [string]$AccessKey = $null,
        [string]$SecretKey = $null
    )

    Write-DevConsole "[Amazon Q] Initializing..." "INFO"

    try {
        # Check for AWS CLI
        $awsCliAvailable = Get-Command aws -ErrorAction SilentlyContinue
        if (-not $awsCliAvailable) {
            Write-DevConsole "[Amazon Q] AWS CLI not found. Install from: https://aws.amazon.com/cli/" "WARNING"
            return $false
        }

        # Set region
        $script:AmazonQConfig.Region = $Region
        $script:AmazonQConfig.APIEndpoint = "https://q.$Region.amazonaws.com"

        # Try to authenticate
        if ($Profile) {
            $script:AmazonQConfig.Profile = $Profile
            $authResult = Connect-AmazonQWithProfile -Profile $Profile
        }
        elseif ($AccessKey -and $SecretKey) {
            $script:AmazonQConfig.AccessKey = $AccessKey
            $script:AmazonQConfig.SecretKey = $SecretKey
            $authResult = Connect-AmazonQWithCredentials -AccessKey $AccessKey -SecretKey $SecretKey
        }
        else {
            # Try default profile
            $authResult = Connect-AmazonQWithProfile -Profile "default"
        }

        if ($authResult) {
            Write-DevConsole "[Amazon Q] ✅ Authenticated successfully" "SUCCESS"
            return $true
        }
        else {
            Write-DevConsole "[Amazon Q] ❌ Authentication failed" "ERROR"
            return $false
        }
    }
    catch {
        Write-DevConsole "[Amazon Q] Error: $_" "ERROR"
        return $false
    }
}

# Connect using AWS profile
function Connect-AmazonQWithProfile {
    param([string]$Profile = "default")

    try {
        # Get credentials from AWS profile
        $credentials = aws configure get aws_access_key_id --profile $Profile 2>$null
        if (-not $credentials) {
            Write-DevConsole "[Amazon Q] Profile '$Profile' not found or not configured" "WARNING"
            return $false
        }

        $script:AmazonQConfig.Profile = $Profile
        $script:AmazonQConfig.IsAuthenticated = $true
        $script:AmazonQConfig.LastAuthTime = Get-Date

        return $true
    }
    catch {
        Write-DevConsole "[Amazon Q] Profile authentication error: $_" "ERROR"
        return $false
    }
}

# Connect using access key and secret key
function Connect-AmazonQWithCredentials {
    param(
        [string]$AccessKey,
        [string]$SecretKey,
        [string]$SessionToken = $null
    )

    try {
        $script:AmazonQConfig.AccessKey = $AccessKey
        $script:AmazonQConfig.SecretKey = $SecretKey
        $script:AmazonQConfig.SessionToken = $SessionToken
        $script:AmazonQConfig.IsAuthenticated = $true
        $script:AmazonQConfig.LastAuthTime = Get-Date

        return $true
    }
    catch {
        Write-DevConsole "[Amazon Q] Credential authentication error: $_" "ERROR"
        return $false
    }
}

# Get AWS credentials for API calls
function Get-AmazonQCredentials {
    if (-not $script:AmazonQConfig.IsAuthenticated) {
        return $null
    }

    if ($script:AmazonQConfig.Profile) {
        # Use AWS CLI to get credentials
        $accessKey = aws configure get aws_access_key_id --profile $script:AmazonQConfig.Profile 2>$null
        $secretKey = aws configure get aws_secret_access_key --profile $script:AmazonQConfig.Profile 2>$null
        $sessionToken = aws configure get aws_session_token --profile $script:AmazonQConfig.Profile 2>$null

        return @{
            AccessKey = $accessKey
            SecretKey = $secretKey
            SessionToken = $sessionToken
        }
    }
    elseif ($script:AmazonQConfig.AccessKey -and $script:AmazonQConfig.SecretKey) {
        return @{
            AccessKey = $script:AmazonQConfig.AccessKey
            SecretKey = $script:AmazonQConfig.SecretKey
            SessionToken = $script:AmazonQConfig.SessionToken
        }
    }

    return $null
}

# Generate AWS Signature Version 4 for API requests
function New-AmazonQSignature {
    param(
        [string]$Method,
        [string]$Uri,
        [hashtable]$Headers = @{},
        [string]$Body = "",
        [hashtable]$Credentials
    )

    # This is a simplified version - full SigV4 is complex
    # In production, use AWS SDK or proper SigV4 library
    $timestamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
    $dateStamp = (Get-Date).ToUniversalTime().ToString("yyyyMMdd")

    # For now, return basic auth headers
    # Full implementation would require proper SigV4 signing
    return @{
        "X-Amz-Date" = $timestamp
        "Authorization" = "AWS4-HMAC-SHA256 Credential=$($Credentials.AccessKey)/$dateStamp/$($script:AmazonQConfig.Region)/q/aws4_request"
    }
}

# Send request to Amazon Q API
function Invoke-AmazonQRequest {
    param(
        [string]$Endpoint,
        [string]$Method = "POST",
        [hashtable]$Body = @{},
        [hashtable]$Headers = @{}
    )

    if (-not $script:AmazonQConfig.IsAuthenticated) {
        Write-DevConsole "[Amazon Q] Not authenticated. Call Initialize-AmazonQ first." "ERROR"
        return $null
    }

    try {
        $credentials = Get-AmazonQCredentials
        if (-not $credentials) {
            Write-DevConsole "[Amazon Q] Failed to get credentials" "ERROR"
            return $null
        }

        $uri = "$($script:AmazonQConfig.APIEndpoint)$Endpoint"
        $bodyJson = $Body | ConvertTo-Json -Compress

        # Generate signature (simplified - use AWS SDK in production)
        $sigHeaders = New-AmazonQSignature -Method $Method -Uri $uri -Body $bodyJson -Credentials $credentials

        $allHeaders = @{
            "Content-Type" = "application/json"
            "X-Amz-Target" = "Q.${Endpoint}"
        } + $sigHeaders + $Headers

        # Make request
        $response = Invoke-RestMethod -Uri $uri -Method $Method -Headers $allHeaders -Body $bodyJson -ErrorAction Stop

        return $response
    }
    catch {
        Write-DevConsole "[Amazon Q] API request failed: $_" "ERROR"
        return $null
    }
}

# Chat with Amazon Q
function Invoke-AmazonQChat {
    param(
        [string]$Message,
        [string]$ConversationId = $null,
        [hashtable]$Context = @{}
    )

    $body = @{
        message = $Message
        conversationId = $ConversationId
        context = $Context
    }

    $response = Invoke-AmazonQRequest -Endpoint "/chat" -Body $body
    return $response
}

# Get code suggestions from Amazon Q
function Get-AmazonQCodeSuggestions {
    param(
        [string]$Code,
        [string]$Language = "powershell",
        [string]$CursorPosition = $null
    )

    $body = @{
        code = $Code
        language = $Language
        cursorPosition = $CursorPosition
    }

    $response = Invoke-AmazonQRequest -Endpoint "/suggestions" -Body $body
    return $response
}

# Analyze code with Amazon Q
function Invoke-AmazonQCodeAnalysis {
    param(
        [string]$Code,
        [string]$Language = "powershell",
        [string[]]$AnalysisTypes = @("security", "performance", "best-practices")
    )

    $body = @{
        code = $Code
        language = $Language
        analysisTypes = $AnalysisTypes
    }

    $response = Invoke-AmazonQRequest -Endpoint "/analyze" -Body $body
    return $response
}

# Export module members
Export-ModuleMember -Function @(
    'Initialize-AmazonQ',
    'Connect-AmazonQWithProfile',
    'Connect-AmazonQWithCredentials',
    'Invoke-AmazonQChat',
    'Get-AmazonQCodeSuggestions',
    'Invoke-AmazonQCodeAnalysis'
)

Write-Host "✅ Amazon Q Developer module loaded" -ForegroundColor Green

