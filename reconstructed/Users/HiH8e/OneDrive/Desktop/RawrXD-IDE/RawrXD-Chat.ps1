#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Chat System Module
    
.DESCRIPTION
    Provides chat functionality for the agentic IDE with message history,
    model integration, and response handling.
#>

# ============================================
# MODULE VARIABLES
# ============================================

$script:ChatSessions = @{}
$script:CurrentSession = $null
$script:MessageHistory = @()
$script:MaxHistoryPerSession = 1000
$script:ModelEndpoint = "http://localhost:11434"
$script:DefaultModel = "llama2"

# ============================================
# CHAT SESSION MANAGEMENT
# ============================================

function New-ChatSession {
    <#
    .SYNOPSIS
        Create a new chat session
    #>
    param(
        [Parameter(Mandatory = $true)][string]$SessionName,
        [string]$Model = $script:DefaultModel,
        [hashtable]$SystemPrompt = @{}
    )
    
    try {
        $sessionId = [guid]::NewGuid().ToString()
        
        $session = @{
            Id = $sessionId
            Name = $SessionName
            Model = $Model
            CreatedAt = Get-Date
            LastActivity = Get-Date
            MessageCount = 0
            Status = "Active"
            SystemPrompt = $SystemPrompt
        }
        
        $script:ChatSessions[$sessionId] = $session
        $script:CurrentSession = $sessionId
        
        Write-Host "[Chat] New session created: $SessionName (ID: $sessionId)" -ForegroundColor Cyan
        
        return @{
            Success = $true
            SessionId = $sessionId
            Name = $SessionName
            Model = $Model
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
        }
    }
}

function Get-ChatSession {
    <#
    .SYNOPSIS
        Get chat session information
    #>
    param(
        [string]$SessionId = $script:CurrentSession
    )
    
    if ($SessionId -and $script:ChatSessions.ContainsKey($SessionId)) {
        return $script:ChatSessions[$SessionId]
    }
    
    return $null
}

function Close-ChatSession {
    <#
    .SYNOPSIS
        Close a chat session
    #>
    param(
        [Parameter(Mandatory = $true)][string]$SessionId
    )
    
    try {
        if ($script:ChatSessions.ContainsKey($SessionId)) {
            $session = $script:ChatSessions[$SessionId]
            $session.Status = "Closed"
            $session.ClosedAt = Get-Date
            
            if ($script:CurrentSession -eq $SessionId) {
                $script:CurrentSession = $null
            }
            
            return @{
                Success = $true
                Message = "Session closed: $($session.Name)"
            }
        }
        else {
            return @{
                Success = $false
                Error = "Session not found: $SessionId"
            }
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
        }
    }
}

function Get-AllChatSessions {
    <#
    .SYNOPSIS
        Get all active chat sessions
    #>
    return @{
        Count = $script:ChatSessions.Count
        Sessions = $script:ChatSessions.Values
        CurrentSession = $script:CurrentSession
        Timestamp = Get-Date
    }
}

# ============================================
# MESSAGE MANAGEMENT
# ============================================

function Send-ChatMessage {
    <#
    .SYNOPSIS
        Send a message in the current chat session
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [string]$SessionId = $script:CurrentSession,
        [string]$Role = "user"
    )
    
    try {
        if (-not $SessionId -or -not $script:ChatSessions.ContainsKey($SessionId)) {
            return @{
                Success = $false
                Error = "No active session"
            }
        }
        
        $session = $script:ChatSessions[$SessionId]
        
        $messageEntry = @{
            Id = [guid]::NewGuid().ToString()
            SessionId = $SessionId
            Timestamp = Get-Date
            Role = $Role
            Content = $Message
            TokenCount = Measure-Tokens -Text $Message
        }
        
        $script:MessageHistory += $messageEntry
        $session.MessageCount++
        $session.LastActivity = Get-Date
        
        Write-Host "[Chat] Message sent ($Role): $(if($Message.Length -gt 50) { $Message.Substring(0, 50) + '...' } else { $Message })" -ForegroundColor Cyan
        
        # If this is a user message, generate assistant response
        if ($Role -eq "user") {
            $response = Invoke-ModelResponse -SessionId $SessionId -Message $Message
            return $response
        }
        
        return @{
            Success = $true
            MessageId = $messageEntry.Id
            Timestamp = Get-Date
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
        }
    }
}

function Get-ChatHistory {
    <#
    .SYNOPSIS
        Get chat history for a session
    #>
    param(
        [string]$SessionId = $script:CurrentSession,
        [int]$Last = 50
    )
    
    try {
        $history = $script:MessageHistory | Where-Object { $_.SessionId -eq $SessionId } | Select-Object -Last $Last
        
        return @{
            Success = $true
            SessionId = $SessionId
            MessageCount = $history.Count
            Messages = $history
            Timestamp = Get-Date
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
        }
    }
}

function Clear-ChatHistory {
    <#
    .SYNOPSIS
        Clear chat history for a session
    #>
    param(
        [string]$SessionId = $script:CurrentSession
    )
    
    try {
        $script:MessageHistory = $script:MessageHistory | Where-Object { $_.SessionId -ne $SessionId }
        
        if ($script:ChatSessions.ContainsKey($SessionId)) {
            $script:ChatSessions[$SessionId].MessageCount = 0
        }
        
        return @{
            Success = $true
            Message = "Chat history cleared"
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
        }
    }
}

# ============================================
# MODEL INTEGRATION
# ============================================

function Invoke-ModelResponse {
    <#
    .SYNOPSIS
        Generate response from AI model
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [string]$SessionId = $script:CurrentSession
    )
    
    try {
        if (-not $SessionId -or -not $script:ChatSessions.ContainsKey($SessionId)) {
            return @{
                Success = $false
                Error = "No active session"
            }
        }
        
        $session = $script:ChatSessions[$SessionId]
        
        Write-Host "[Chat] Generating response from model: $($session.Model)..." -ForegroundColor Yellow
        
        # In production, would call the actual model endpoint
        # For now, simulate response
        $response = "This is a simulated response from $($session.Model). In production, this would connect to Ollama at $script:ModelEndpoint"
        
        # Add assistant response to history
        $assistantMessage = @{
            Id = [guid]::NewGuid().ToString()
            SessionId = $SessionId
            Timestamp = Get-Date
            Role = "assistant"
            Content = $response
            TokenCount = Measure-Tokens -Text $response
        }
        
        $script:MessageHistory += $assistantMessage
        $session.MessageCount++
        
        return @{
            Success = $true
            Response = $response
            Model = $session.Model
            Timestamp = Get-Date
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
        }
    }
}

function Set-ChatModel {
    <#
    .SYNOPSIS
        Set the model for a chat session
    #>
    param(
        [Parameter(Mandatory = $true)][string]$Model,
        [string]$SessionId = $script:CurrentSession
    )
    
    try {
        if ($SessionId -and $script:ChatSessions.ContainsKey($SessionId)) {
            $script:ChatSessions[$SessionId].Model = $Model
            
            return @{
                Success = $true
                Message = "Model changed to: $Model"
            }
        }
        else {
            $script:DefaultModel = $Model
            
            return @{
                Success = $true
                Message = "Default model changed to: $Model"
            }
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_
        }
    }
}

# ============================================
# UTILITY FUNCTIONS
# ============================================

function Measure-Tokens {
    <#
    .SYNOPSIS
        Estimate token count for text (simplified)
    #>
    param(
        [string]$Text
    )
    
    # Simple approximation: 1 token ≈ 4 characters
    return [math]::Ceiling($Text.Length / 4)
}

function Test-ModelConnection {
    <#
    .SYNOPSIS
        Test connection to model endpoint
    #>
    try {
        $response = Invoke-RestMethod -Uri "$script:ModelEndpoint/api/tags" -Method Get -TimeoutSec 5 -ErrorAction Stop
        
        return @{
            Success = $true
            Connected = $true
            Models = $response.models.name
            Endpoint = $script:ModelEndpoint
        }
    }
    catch {
        return @{
            Success = $false
            Connected = $false
            Error = $_
            Endpoint = $script:ModelEndpoint
        }
    }
}

# ============================================
# INITIALIZATION
# ============================================

Write-Host "[RawrXD-Chat] Module loaded successfully" -ForegroundColor Green

# Note: This is a dot-sourced PS1 script, not a PSM1 module
# Functions are automatically available in parent scope
