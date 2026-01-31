# ============================================================================
# File: D:\lazy init ide\scripts\PatternTemplates.psm1
# Purpose: Domain-specific pattern templates for RawrXD TODO resolution
# ============================================================================

#region Security Patterns
$SecurityPatternTemplates = @{
    'JWTValidation' = @{
        Category = 'Security'
        Description = 'Implement JWT token validation middleware'
        DetectPattern = 'JWT|token.*validation|authentication.*middleware'
        CodeTemplate = @'
# Auto-generated JWT Validation Middleware
function Invoke-JWTValidation {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Token,

        [Parameter(Mandatory=$false)]
        [string]$Secret = $env:JWT_SECRET
    )

    begin {
        Add-Type -AssemblyName System.IdentityModel.Tokens.Jwt
    }

    process {
        try {
            $handler = [System.IdentityModel.Tokens.Jwt.JwtSecurityTokenHandler]::new()
            $validationParameters = [System.IdentityModel.Tokens.TokenValidationParameters]@{
                ValidateIssuer = $true
                ValidateAudience = $true
                ValidateLifetime = $true
                ValidateIssuerSigningKey = $true
                IssuerSigningKey = [Microsoft.IdentityModel.Tokens.SymmetricSecurityKey]::new(
                    [System.Text.Encoding]::UTF8.GetBytes($Secret)
                )
                ClockSkew = [TimeSpan]::FromMinutes(5)
            }

            $principal = $handler.ValidateToken($Token, $validationParameters, [ref]$null)
            return @{
                IsValid = $true
                Claims = $principal.Claims | ForEach-Object { @{ $_.Type = $_.Value } }
                Expires = $principal.Expires
            }
        }
        catch [System.IdentityModel.Tokens.SecurityTokenExpiredException] {
            return @{ IsValid = $false; Error = 'Token expired' }
        }
        catch {
            return @{ IsValid = $false; Error = $_.Exception.Message }
        }
    }
}
'@
        Dependencies = @('System.IdentityModel.Tokens.Jwt')
        Priority = 10
    }

    'InputSanitization' = @{
        Category = 'Security'
        Description = 'Add input sanitization and validation'
        DetectPattern = 'sanitize|validate.*input|anti.*XSS|SQL.*injection'
        CodeTemplate = @'
function Protect-UserInput {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true, ValueFromPipeline=$true)]
        [AllowEmptyString()]
        [string]$Input,

        [Parameter(Mandatory=$false)]
        [ValidateSet('HTML', 'SQL', 'LDAP', 'XPath', 'Command')]
        [string]$Context = 'HTML'
    )

    process {
        if ([string]::IsNullOrWhiteSpace($Input)) {
            return ''
        }

        switch ($Context) {
            'HTML' {
                # HTML entity encoding
                $Input -replace '&', '&amp;' `
                       -replace '<', '&lt;' `
                       -replace '>', '&gt;' `
                       -replace '"', '&quot;' `
                       -replace "'", '&#x27;'
            }
            'SQL' {
                # Parameterized query recommendation
                Write-Warning "Use parameterized queries instead of string concatenation"
                $Input -replace "'", "''" -replace ';', ''
            }
            'Command' {
                # Remove dangerous characters
                $Input -replace '[;&|`$]', ''
            }
            default { $Input }
        }
    }
}
'@
        Priority = 9
    }

    'SecureSecretStorage' = @{
        Category = 'Security'
        Description = 'Implement secure secret storage using DPAPI or Azure Key Vault'
        DetectPattern = 'secret.*storage|credential.*management|API.*key.*storage'
        CodeTemplate = @'
function Protect-Secret {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Secret,

        [Parameter(Mandatory=$false)]
        [string]$Scope = 'CurrentUser'
    )

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Secret)
    $protected = [System.Security.Cryptography.ProtectedData]::Protect(
        $bytes,
        $null,
        [System.Security.Cryptography.DataProtectionScope]::$Scope
    )
    return [Convert]::ToBase64String($protected)
}

function Unprotect-Secret {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$ProtectedSecret,

        [Parameter(Mandatory=$false)]
        [string]$Scope = 'CurrentUser'
    )

    $bytes = [Convert]::FromBase64String($ProtectedSecret)
    $unprotected = [System.Security.Cryptography.ProtectedData]::Unprotect(
        $bytes,
        $null,
        [System.Security.Cryptography.DataProtectionScope]::$Scope
    )
    return [System.Text.Encoding]::UTF8.GetString($unprotected)
}
'@
        Priority = 10
    }
}
#endregion

#region Performance Patterns
$PerformancePatternTemplates = @{
    'ConnectionPooling' = @{
        Category = 'Performance'
        Description = 'Implement connection pooling for database/HTTP clients'
        DetectPattern = 'connection.*pool|pooling|reuse.*connection'
        CodeTemplate = @'
class ConnectionPool {
    [System.Collections.Concurrent.ConcurrentBag[object]]$Pool
    [scriptblock]$Factory
    [int]$MaxSize
    [int]$CurrentSize
    [System.Threading.SemaphoreSlim]$Semaphore

    ConnectionPool([scriptblock]$factory, [int]$maxSize = 10) {
        $this.Pool = [System.Collections.Concurrent.ConcurrentBag[object]]::new()
        $this.Factory = $factory
        $this.MaxSize = $maxSize
        $this.CurrentSize = 0
        $this.Semaphore = [System.Threading.SemaphoreSlim]::new($maxSize, $maxSize)
    }

    [object] Acquire() {
        $this.Semaphore.Wait()

        [object]$item = $null
        if ($this.Pool.TryTake([ref]$item)) {
            return $item
        }

        # Create new if pool empty
        $item = & $this.Factory
        $this.CurrentSize++
        return $item
    }

    void Release([object]$item) {
        if ($item -ne $null) {
            $this.Pool.Add($item)
        }
        $this.Semaphore.Release()
    }

    void Dispose() {
        $this.Semaphore.Dispose()
        while ($this.Pool.TryTake([ref]$item)) {
            if ($item -is [IDisposable]) {
                $item.Dispose()
            }
        }
    }
}
'@
        Priority = 8
    }

    'LazyLoading' = @{
        Category = 'Performance'
        Description = 'Implement lazy loading with caching'
        DetectPattern = 'lazy.*load|deferred.*loading|on.*demand'
        CodeTemplate = @'
function Get-LazyValue {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$CacheKey,

        [Parameter(Mandatory=$true)]
        [scriptblock]$Factory,

        [Parameter(Mandatory=$false)]
        [TimeSpan]$TTL = [TimeSpan]::FromMinutes(5)
    )

    $cache = $Global:LazyLoadCache
    if (-not $cache) {
        $cache = @{}
        $Global:LazyLoadCache = $cache
    }

    $entry = $cache[$CacheKey]
    if ($entry -and $entry.Expiry -gt (Get-Date)) {
        Write-Verbose "[LazyLoad] Cache hit: $CacheKey"
        return $entry.Value
    }

    Write-Verbose "[LazyLoad] Cache miss: $CacheKey"
    $value = & $Factory
    $cache[$CacheKey] = @{
        Value = $value
        Expiry = (Get-Date).Add($TTL)
    }

    return $value
}
'@
        Priority = 7
    }

    'Memoization' = @{
        Category = 'Performance'
        Description = 'Add function memoization for expensive computations'
        DetectPattern = 'memoiz|cache.*result|expensive.*computation'
        CodeTemplate = @'
function Invoke-Memoized {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [scriptblock]$ScriptBlock,

        [Parameter(Mandatory=$true)]
        [array]$Arguments,

        [Parameter(Mandatory=$false)]
        [string]$CacheKey = $null
    )

    if (-not $CacheKey) {
        $CacheKey = ($ScriptBlock.ToString() + ($Arguments -join ',')) -as [byte[]] |
            ForEach-Object { $_.ToString('x2') } |
            Join-String
    }

    $cache = $Global:MemoizationCache
    if (-not $cache) {
        $cache = @{}
        $Global:MemoizationCache = $cache
    }

    if ($cache.ContainsKey($CacheKey)) {
        return $cache[$CacheKey]
    }

    $result = & $ScriptBlock @Arguments
    $cache[$CacheKey] = $result
    return $result
}
'@
        Priority = 7
    }
}
#endregion

#region Networking Patterns
$NetworkingPatternTemplates = @{
    'HTTPRetryPolicy' = @{
        Category = 'Networking'
        Description = 'Implement resilient HTTP retry policy with exponential backoff'
        DetectPattern = 'retry.*policy|exponential.*backoff|resilient.*HTTP'
        CodeTemplate = @'
function Invoke-ResilientRequest {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Uri,

        [Parameter(Mandatory=$false)]
        [ValidateSet('GET', 'POST', 'PUT', 'DELETE', 'PATCH')]
        [string]$Method = 'GET',

        [Parameter(Mandatory=$false)]
        [hashtable]$Headers = @{},

        [Parameter(Mandatory=$false)]
        [object]$Body = $null,

        [Parameter(Mandatory=$false)]
        [int]$MaxRetries = 3,

        [Parameter(Mandatory=$false)]
        [int]$InitialDelayMs = 100
    )

    $delay = $InitialDelayMs
    $lastError = $null

    for ($i = 0; $i -lt $MaxRetries; $i++) {
        try {
            $params = @{
                Uri = $Uri
                Method = $Method
                Headers = $Headers
                TimeoutSec = 30
            }
            if ($Body) { $params.Body = $Body }

            return Invoke-RestMethod @params
        }
        catch [System.Net.WebException] {
            $lastError = $_
            $status = $_.Exception.Response.StatusCode.value__

            # Don't retry on client errors (4xx)
            if ($status -ge 400 -and $status -lt 500) {
                throw
            }

            Write-Warning "Request failed (attempt $($i+1)/$MaxRetries): $($_.Exception.Message)"

            if ($i -lt $MaxRetries - 1) {
                Start-Sleep -Milliseconds $delay
                $delay *= 2  # Exponential backoff
            }
        }
    }

    throw "Max retries exceeded: $($lastError.Exception.Message)"
}
'@
        Priority = 8
    }

    'WebSocketClient' = @{
        Category = 'Networking'
        Description = 'Implement WebSocket client with auto-reconnect'
        DetectPattern = 'websocket|ws://|wss://|real.*time.*connection'
        CodeTemplate = @'
class WebSocketClient {
    [System.Net.WebSockets.ClientWebSocket]$Socket
    [string]$Uri
    [bool]$AutoReconnect
    [int]$ReconnectDelayMs
    [System.Threading.CancellationTokenSource]$CTS

    WebSocketClient([string]$uri, [bool]$autoReconnect = $true) {
        $this.Uri = $uri
        $this.AutoReconnect = $autoReconnect
        $this.ReconnectDelayMs = 5000
        $this.CTS = [System.Threading.CancellationTokenSource]::new()
    }

    [System.Threading.Tasks.Task] ConnectAsync() {
        $this.Socket = [System.Net.WebSockets.ClientWebSocket]::new()
        $uri = [System.Uri]::new($this.Uri)
        return $this.Socket.ConnectAsync($uri, $this.CTS.Token)
    }

    [System.Threading.Tasks.Task] SendAsync([string]$message) {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($message)
        $segment = [System.ArraySegment[byte]]::new($bytes)
        return $this.Socket.SendAsync(
            $segment,
            [System.Net.WebSockets.WebSocketMessageType]::Text,
            $true,
            $this.CTS.Token
        )
    }

    [System.Threading.Tasks.Task[string]] ReceiveAsync() {
        $buffer = [byte[]]::new(4096)
        $segment = [System.ArraySegment[byte]]::new($buffer)

        $result = $this.Socket.ReceiveAsync($segment, $this.CTS.Token)

        if ($result.Result.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) {
            if ($this.AutoReconnect) {
                Start-Sleep -Milliseconds $this.ReconnectDelayMs
                $this.ConnectAsync().Wait()
            }
            return $null
        }

        return [System.Text.Encoding]::UTF8.GetString($buffer, 0, $result.Result.Count)
    }

    void Close() {
        $this.CTS.Cancel()
        $this.Socket.CloseAsync(
            [System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure,
            'Closing',
            [System.Threading.CancellationToken]::None
        ).Wait()
    }
}
'@
        Priority = 7
    }
}
#endregion

#region Data Processing Patterns
$DataProcessingPatternTemplates = @{
    'StreamProcessing' = @{
        Category = 'DataProcessing'
        Description = 'Implement memory-efficient stream processing'
        DetectPattern = 'stream.*process|large.*file|memory.*efficient'
        CodeTemplate = @'
function Process-Stream {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Path,

        [Parameter(Mandatory=$true)]
        [scriptblock]$ProcessBlock,

        [Parameter(Mandatory=$false)]
        [int]$BufferSize = 65536
    )

    $stream = [System.IO.File]::OpenRead($Path)
    $reader = [System.IO.StreamReader]::new($stream)

    try {
        while ($null -ne ($line = $reader.ReadLine())) {
            & $ProcessBlock $line
        }
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}
'@
        Priority = 6
    }

    'ParallelPipeline' = @{
        Category = 'DataProcessing'
        Description = 'Implement parallel data processing pipeline'
        DetectPattern = 'parallel.*process|pipeline|ForEach-Object.*-Parallel'
        CodeTemplate = @'
function Invoke-ParallelPipeline {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true, ValueFromPipeline=$true)]
        [array]$InputObject,

        [Parameter(Mandatory=$true)]
        [scriptblock]$ProcessBlock,

        [Parameter(Mandatory=$false)]
        [int]$ThrottleLimit = 5,

        [Parameter(Mandatory=$false)]
        [switch]$UseRunspaces = $false
    )

    begin {
        $items = [System.Collections.Generic.List[object]]::new()
    }

    process {
        $items.Add($InputObject)
    }

    end {
        if ($UseRunspaces) {
            # Use runspace pool for fine-grained control
            $runspacePool = [runspacefactory]::CreateRunspacePool(1, $ThrottleLimit)
            $runspacePool.Open()

            $runspaces = foreach ($item in $items) {
                $powershell = [powershell]::Create().AddScript($ProcessBlock).AddArgument($item)
                $powershell.RunspacePool = $runspacePool

                @{
                    Pipe = $powershell
                    AsyncResult = $powershell.BeginInvoke()
                }
            }

            $results = foreach ($rs in $runspaces) {
                $rs.Pipe.EndInvoke($rs.AsyncResult)
                $rs.Pipe.Dispose()
            }

            $runspacePool.Close()
            return $results
        }
        else {
            # Use ForEach-Object -Parallel (PowerShell 7+)
            return $items | ForEach-Object -Parallel $ProcessBlock -ThrottleLimit $ThrottleLimit
        }
    }
}
'@
        Priority = 7
    }
}
#endregion

#region Error Handling Patterns
$ErrorHandlingPatternTemplates = @{
    'CircuitBreaker' = @{
        Category = 'ErrorHandling'
        Description = 'Implement circuit breaker pattern for fault tolerance'
        DetectPattern = 'circuit.*breaker|fault.*tolerance|fail.*fast'
        CodeTemplate = @'
class CircuitBreaker {
    [string]$Name
    [int]$FailureThreshold
    [TimeSpan]$Timeout
    [int]$FailureCount
    [datetime]$LastFailureTime
    [ValidateSet('Closed', 'Open', 'HalfOpen')]
    [string]$State

    CircuitBreaker([string]$name, [int]$threshold = 5, [int]$timeoutSeconds = 30) {
        $this.Name = $name
        $this.FailureThreshold = $threshold
        $this.Timeout = [TimeSpan]::FromSeconds($timeoutSeconds)
        $this.FailureCount = 0
        $this.State = 'Closed'
    }

    [object] Execute([scriptblock]$action) {
        if ($this.State -eq 'Open') {
            if ((Get-Date) - $this.LastFailureTime -gt $this.Timeout) {
                $this.State = 'HalfOpen'
                Write-Verbose "Circuit '$($this.Name)' entering Half-Open state"
            }
            else {
                throw "Circuit breaker is OPEN for '$($this.Name)'"
            }
        }

        try {
            $result = & $action
            $this.RecordSuccess()
            return $result
        }
        catch {
            $this.RecordFailure()
            throw
        }
    }

    hidden [void] RecordSuccess() {
        $this.FailureCount = 0
        $this.State = 'Closed'
    }

    hidden [void] RecordFailure() {
        $this.FailureCount++
        $this.LastFailureTime = Get-Date

        if ($this.FailureCount -ge $this.FailureThreshold) {
            $this.State = 'Open'
            Write-Warning "Circuit '$($this.Name)' is now OPEN due to $($this.FailureCount) failures"
        }
    }
}
'@
        Priority = 8
    }

    'GlobalErrorHandler' = @{
        Category = 'ErrorHandling'
        Description = 'Install global error handling with telemetry'
        DetectPattern = 'global.*error.*handler|unhandled.*exception|telemetry'
        CodeTemplate = @'
function Install-GlobalErrorHandler {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [scriptblock]$OnError = $null,

        [Parameter(Mandatory=$false)]
        [string]$LogPath = "$env:TEMP\error.log"
    )

    $global:ErrorActionPreference = 'Stop'

    # Capture unhandled errors
    $global:ExecutionContext.InvokeCommand.CommandNotFoundAction = {
        param($CommandName, $CommandLookupEventArgs)

        $errorRecord = [System.Management.Automation.ErrorRecord]::new(
            [Exception]::new("Command not found: $CommandName"),
            'CommandNotFound',
            [System.Management.Automation.ErrorCategory]::ObjectNotFound,
            $CommandName
        )

        Log-Error -Record $errorRecord -LogPath $LogPath
    }

    # Set trap for terminating errors
    trap {
        Log-Error -Record $_ -LogPath $LogPath
        if ($OnError) {
            & $OnError $_
        }
        continue
    }
}

function Log-Error {
    param(
        [System.Management.Automation.ErrorRecord]$Record,
        [string]$LogPath
    )

    $entry = @{
        Timestamp = Get-Date -Format 'o'
        Message = $Record.Exception.Message
        Category = $Record.CategoryInfo.Category
        Target = $Record.TargetObject
        ScriptStackTrace = $Record.ScriptStackTrace
    } | ConvertTo-Json

    Add-Content -Path $LogPath -Value $entry
}
'@
        Priority = 9
    }
}
#endregion

#region Template Resolution Engine
function Get-PatternTemplate {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$TODOContent,

        [Parameter(Mandatory=$false)]
        [ValidateSet('Security', 'Performance', 'Networking', 'DataProcessing', 'ErrorHandling')]
        [string]$Category = $null
    )

    $allTemplates = @{}
    $allTemplates += $SecurityPatternTemplates
    $allTemplates += $PerformancePatternTemplates
    $allTemplates += $NetworkingPatternTemplates
    $allTemplates += $DataProcessingPatternTemplates
    $allTemplates += $ErrorHandlingPatternTemplates

    $matches = @()

    foreach ($name in $allTemplates.Keys) {
        $template = $allTemplates[$name]

        # Check if pattern matches TODO content
        if ($TODOContent -match $template.DetectPattern) {
            $score = [regex]::Matches($TODOContent, $template.DetectPattern).Count

            # Boost score for category match
            if ($Category -and $template.Category -eq $Category) {
                $score += 5
            }

            $matches += [PSCustomObject]@{
                Name = $name
                Template = $template
                Score = $score
                Priority = $template.Priority
            }
        }
    }

    # Return best match
    return $matches | Sort-Object -Property Priority, Score -Descending | Select-Object -First 1
}

function Expand-PatternTemplate {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$TemplateName,

        [Parameter(Mandatory=$true)]
        [hashtable]$Parameters
    )

    $template = $SecurityPatternTemplates[$TemplateName] ??
                $PerformancePatternTemplates[$TemplateName] ??
                $NetworkingPatternTemplates[$TemplateName] ??
                $DataProcessingPatternTemplates[$TemplateName] ??
                $ErrorHandlingPatternTemplates[$TemplateName]

    if (-not $template) {
        throw "Template not found: $TemplateName"
    }

    $code = $template.CodeTemplate

    # Simple parameter substitution
    foreach ($key in $Parameters.Keys) {
        $code = $code -replace "\{\{$key\}\}", $Parameters[$key]
    }

    return $code
}

function Export-PatternLibrary {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$OutputPath
    )

    $library = @{
        Security = $SecurityPatternTemplates
        Performance = $PerformancePatternTemplates
        Networking = $NetworkingPatternTemplates
        DataProcessing = $DataProcessingPatternTemplates
        ErrorHandling = $ErrorHandlingPatternTemplates
        Metadata = @{
            Generated = Get-Date -Format 'o'
            Version = '1.0.0'
            TotalPatterns = ($SecurityPatternTemplates.Count + $PerformancePatternTemplates.Count +
                           $NetworkingPatternTemplates.Count + $DataProcessingPatternTemplates.Count +
                           $ErrorHandlingPatternTemplates.Count)
        }
    }

    $library | ConvertTo-Json -Depth 10 | Set-Content -Path $OutputPath
    Write-Host "Pattern library exported to: $OutputPath" -ForegroundColor Green
}
#endregion

Export-ModuleMember -Function @(
    'Get-PatternTemplate',
    'Expand-PatternTemplate',
    'Export-PatternLibrary'
)
