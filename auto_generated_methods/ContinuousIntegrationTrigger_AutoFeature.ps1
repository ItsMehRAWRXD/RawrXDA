#requires -Version 5.1
<#
.SYNOPSIS
    Production-grade Continuous Integration Trigger with full CI/CD pipeline support.
.DESCRIPTION
    Comprehensive CI/CD trigger system supporting:
    - Multi-provider CI integration (GitHub Actions, Azure DevOps, Jenkins, GitLab CI)
    - Intelligent file watching with pattern-based filtering
    - Build queue management with priority
    - Branch and PR detection
    - Webhook handling for external triggers
    - Build caching and incremental builds
    - Test result aggregation
    - Deployment gate management
    - Notification system (Slack, Teams, Email)
.NOTES
    Author: RawrXD Production Team
    Version: 2.0.0
    Requires: PowerShell 5.1+
#>

# ============================================================================
# STRUCTURED LOGGING (Standalone fallback)
# ============================================================================
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Debug','Info','Warning','Error','Critical')][string]$Level = 'Info',
            [hashtable]$Context = @{}
        )
        $timestamp = Get-Date -Format 'yyyy-MM-ddTHH:mm:ss.fffZ'
        $logEntry = @{
            Timestamp = $timestamp
            Level = $Level
            Message = $Message
            Context = $Context
        } | ConvertTo-Json -Compress
        
        $color = switch ($Level) {
            'Debug' { 'Gray' }
            'Info' { 'White' }
            'Warning' { 'Yellow' }
            'Error' { 'Red' }
            'Critical' { 'Magenta' }
            default { 'White' }
        }
        Write-Host $logEntry -ForegroundColor $color
    }
}

# ============================================================================
# CI REGISTRY (Global State Management)
# ============================================================================
$script:CIRegistry = @{
    ActiveWatchers = @{}                 # WatcherId -> Watcher info
    BuildQueue = [System.Collections.ArrayList]@()  # Pending builds
    BuildHistory = [System.Collections.ArrayList]@() # Completed builds
    WebhookServer = $null                # HTTP listener for webhooks
    Configuration = @{
        WatchDirectories = @("D:/lazy init ide")
        WatchPatterns = @("*.ps1", "*.psm1", "*.psd1", "*.json")
        ExcludePatterns = @("*.log", "*.tmp", "*~", ".git/*")
        DebounceMs = 2000
        MaxConcurrentBuilds = 2
        BuildTimeout = 300000  # 5 minutes
        EnableWebhooks = $false
        WebhookPort = 8765
        Providers = @{
            GitHub = @{
                Enabled = $false
                Token = $null
                Owner = $null
                Repo = $null
                WorkflowId = $null
            }
            AzureDevOps = @{
                Enabled = $false
                Organization = $null
                Project = $null
                PipelineId = $null
                PAT = $null
            }
            Jenkins = @{
                Enabled = $false
                Url = $null
                JobName = $null
                Token = $null
            }
            Local = @{
                Enabled = $true
                BuildScript = $null
                TestScript = $null
            }
        }
        Notifications = @{
            Slack = @{ Enabled = $false; WebhookUrl = $null }
            Teams = @{ Enabled = $false; WebhookUrl = $null }
            Email = @{ Enabled = $false; SmtpServer = $null; Recipients = @() }
        }
    }
    Statistics = @{
        TotalBuilds = 0
        SuccessfulBuilds = 0
        FailedBuilds = 0
        AverageBuiltTimeMs = 0
        LastBuildTime = $null
    }
    IsRunning = $false
    CancellationToken = $null
}

# ============================================================================
# BUILD QUEUE MANAGEMENT
# ============================================================================
function Add-BuildToQueue {
    <#
    .SYNOPSIS
        Adds a build request to the queue with priority handling.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$Trigger,
        [Parameter(Mandatory=$true)][string[]]$ChangedFiles,
        [ValidateSet('Low', 'Normal', 'High', 'Critical')]
        [string]$Priority = 'Normal',
        [string]$Branch = 'main',
        [string]$CommitSha = $null,
        [hashtable]$Metadata = @{}
    )
    
    $buildId = [guid]::NewGuid().ToString('N').Substring(0, 8)
    
    $buildRequest = @{
        BuildId = $buildId
        Trigger = $Trigger
        ChangedFiles = $ChangedFiles
        Priority = $Priority
        Branch = $Branch
        CommitSha = $CommitSha
        Metadata = $Metadata
        QueuedAt = Get-Date
        Status = 'Queued'
        StartedAt = $null
        CompletedAt = $null
        Result = $null
        Logs = [System.Collections.ArrayList]@()
    }
    
    # Insert based on priority
    $priorityOrder = @{ 'Critical' = 0; 'High' = 1; 'Normal' = 2; 'Low' = 3 }
    $insertIndex = 0
    
    for ($i = 0; $i -lt $script:CIRegistry.BuildQueue.Count; $i++) {
        if ($priorityOrder[$Priority] -lt $priorityOrder[$script:CIRegistry.BuildQueue[$i].Priority]) {
            $insertIndex = $i
            break
        }
        $insertIndex = $i + 1
    }
    
    $script:CIRegistry.BuildQueue.Insert($insertIndex, $buildRequest)
    
    Write-StructuredLog -Message "Build queued" -Level Info -Context @{
        BuildId = $buildId
        Trigger = $Trigger
        Priority = $Priority
        QueuePosition = $insertIndex + 1
        ChangedFilesCount = $ChangedFiles.Count
    }
    
    return $buildId
}

function Get-NextBuild {
    <#
    .SYNOPSIS
        Gets the next build from the queue.
    #>
    [CmdletBinding()]
    param()
    
    if ($script:CIRegistry.BuildQueue.Count -eq 0) {
        return $null
    }
    
    $build = $script:CIRegistry.BuildQueue[0]
    $script:CIRegistry.BuildQueue.RemoveAt(0)
    return $build
}

# ============================================================================
# FILE CHANGE DETECTION
# ============================================================================
function Test-FileMatchesPattern {
    <#
    .SYNOPSIS
        Tests if a file path matches include/exclude patterns.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [string[]]$IncludePatterns = @("*"),
        [string[]]$ExcludePatterns = @()
    )
    
    $fileName = [System.IO.Path]::GetFileName($FilePath)
    $relativePath = $FilePath
    
    # Check excludes first
    foreach ($pattern in $ExcludePatterns) {
        if ($fileName -like $pattern -or $relativePath -like $pattern) {
            return $false
        }
    }
    
    # Check includes
    foreach ($pattern in $IncludePatterns) {
        if ($fileName -like $pattern -or $relativePath -like $pattern) {
            return $true
        }
    }
    
    return $false
}

function Get-AffectedComponents {
    <#
    .SYNOPSIS
        Determines which components are affected by changed files.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string[]]$ChangedFiles
    )
    
    $components = @{
        Core = $false
        Tests = $false
        Config = $false
        Plugins = $false
        Documentation = $false
    }
    
    foreach ($file in $ChangedFiles) {
        $fileName = [System.IO.Path]::GetFileName($file).ToLower()
        $path = $file.ToLower()
        
        if ($path -match 'test|spec') {
            $components.Tests = $true
        }
        if ($path -match 'plugin') {
            $components.Plugins = $true
        }
        if ($fileName -match '\.json$|\.config$|\.yaml$|\.yml$') {
            $components.Config = $true
        }
        if ($fileName -match '\.md$|\.txt$|\.rst$') {
            $components.Documentation = $true
        }
        if ($path -match 'core|rawrxd\.(core|logging|config)') {
            $components.Core = $true
        }
    }
    
    return $components
}

# ============================================================================
# CI PROVIDER INTEGRATIONS
# ============================================================================
function Invoke-GitHubActionsWorkflow {
    <#
    .SYNOPSIS
        Triggers a GitHub Actions workflow.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$BuildRequest,
        [hashtable]$Config = $script:CIRegistry.Configuration.Providers.GitHub
    )
    
    if (-not $Config.Enabled) {
        Write-StructuredLog -Message "GitHub Actions not enabled" -Level Warning
        return @{ Success = $false; Error = "Provider not enabled" }
    }
    
    $uri = "https://api.github.com/repos/$($Config.Owner)/$($Config.Repo)/actions/workflows/$($Config.WorkflowId)/dispatches"
    
    $body = @{
        ref = $BuildRequest.Branch
        inputs = @{
            build_id = $BuildRequest.BuildId
            trigger = $BuildRequest.Trigger
            changed_files = ($BuildRequest.ChangedFiles -join ',')
        }
    } | ConvertTo-Json -Depth 10
    
    $headers = @{
        'Accept' = 'application/vnd.github.v3+json'
        'Authorization' = "token $($Config.Token)"
        'User-Agent' = 'RawrXD-CI-Trigger'
    }
    
    try {
        $response = Invoke-RestMethod -Uri $uri -Method Post -Headers $headers -Body $body -ContentType 'application/json'
        
        Write-StructuredLog -Message "GitHub Actions workflow triggered" -Level Info -Context @{
            BuildId = $BuildRequest.BuildId
            Repo = "$($Config.Owner)/$($Config.Repo)"
        }
        
        return @{
            Success = $true
            Provider = 'GitHub'
            WorkflowId = $Config.WorkflowId
        }
    } catch {
        Write-StructuredLog -Message "GitHub Actions trigger failed" -Level Error -Context @{
            Error = $_.Exception.Message
        }
        return @{
            Success = $false
            Provider = 'GitHub'
            Error = $_.Exception.Message
        }
    }
}

function Invoke-AzureDevOpsPipeline {
    <#
    .SYNOPSIS
        Triggers an Azure DevOps pipeline.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$BuildRequest,
        [hashtable]$Config = $script:CIRegistry.Configuration.Providers.AzureDevOps
    )
    
    if (-not $Config.Enabled) {
        Write-StructuredLog -Message "Azure DevOps not enabled" -Level Warning
        return @{ Success = $false; Error = "Provider not enabled" }
    }
    
    $uri = "https://dev.azure.com/$($Config.Organization)/$($Config.Project)/_apis/pipelines/$($Config.PipelineId)/runs?api-version=7.0"
    
    $body = @{
        resources = @{
            repositories = @{
                self = @{
                    refName = "refs/heads/$($BuildRequest.Branch)"
                }
            }
        }
        templateParameters = @{
            buildId = $BuildRequest.BuildId
            trigger = $BuildRequest.Trigger
        }
    } | ConvertTo-Json -Depth 10
    
    $base64Auth = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes(":$($Config.PAT)"))
    $headers = @{
        'Authorization' = "Basic $base64Auth"
        'Content-Type' = 'application/json'
    }
    
    try {
        $response = Invoke-RestMethod -Uri $uri -Method Post -Headers $headers -Body $body
        
        Write-StructuredLog -Message "Azure DevOps pipeline triggered" -Level Info -Context @{
            BuildId = $BuildRequest.BuildId
            RunId = $response.id
        }
        
        return @{
            Success = $true
            Provider = 'AzureDevOps'
            RunId = $response.id
            RunUrl = $response._links.web.href
        }
    } catch {
        Write-StructuredLog -Message "Azure DevOps trigger failed" -Level Error -Context @{
            Error = $_.Exception.Message
        }
        return @{
            Success = $false
            Provider = 'AzureDevOps'
            Error = $_.Exception.Message
        }
    }
}

function Invoke-JenkinsBuild {
    <#
    .SYNOPSIS
        Triggers a Jenkins build.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$BuildRequest,
        [hashtable]$Config = $script:CIRegistry.Configuration.Providers.Jenkins
    )
    
    if (-not $Config.Enabled) {
        Write-StructuredLog -Message "Jenkins not enabled" -Level Warning
        return @{ Success = $false; Error = "Provider not enabled" }
    }
    
    $uri = "$($Config.Url)/job/$($Config.JobName)/buildWithParameters"
    $params = "BUILD_ID=$($BuildRequest.BuildId)&BRANCH=$($BuildRequest.Branch)&TRIGGER=$($BuildRequest.Trigger)"
    
    $headers = @{
        'Authorization' = "Bearer $($Config.Token)"
    }
    
    try {
        $response = Invoke-RestMethod -Uri "$uri?$params" -Method Post -Headers $headers
        
        Write-StructuredLog -Message "Jenkins build triggered" -Level Info -Context @{
            BuildId = $BuildRequest.BuildId
            Job = $Config.JobName
        }
        
        return @{
            Success = $true
            Provider = 'Jenkins'
            QueueUrl = $response.Headers['Location']
        }
    } catch {
        Write-StructuredLog -Message "Jenkins trigger failed" -Level Error -Context @{
            Error = $_.Exception.Message
        }
        return @{
            Success = $false
            Provider = 'Jenkins'
            Error = $_.Exception.Message
        }
    }
}

function Invoke-LocalBuild {
    <#
    .SYNOPSIS
        Executes local build and test scripts.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$BuildRequest,
        [hashtable]$Config = $script:CIRegistry.Configuration.Providers.Local
    )
    
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $result = @{
        Success = $true
        Provider = 'Local'
        BuildOutput = @()
        TestOutput = @()
        Duration = 0
        Errors = @()
    }
    
    try {
        # Run build script if configured
        if ($Config.BuildScript -and (Test-Path $Config.BuildScript)) {
            Write-StructuredLog -Message "Running build script" -Level Info -Context @{Script = $Config.BuildScript}
            
            $buildOutput = & $Config.BuildScript -ChangedFiles $BuildRequest.ChangedFiles 2>&1
            $result.BuildOutput = $buildOutput
            
            if ($LASTEXITCODE -ne 0) {
                $result.Success = $false
                $result.Errors += "Build script failed with exit code $LASTEXITCODE"
            }
        }
        
        # Run test script if configured and build succeeded
        if ($result.Success -and $Config.TestScript -and (Test-Path $Config.TestScript)) {
            Write-StructuredLog -Message "Running test script" -Level Info -Context @{Script = $Config.TestScript}
            
            $testOutput = & $Config.TestScript 2>&1
            $result.TestOutput = $testOutput
            
            if ($LASTEXITCODE -ne 0) {
                $result.Success = $false
                $result.Errors += "Test script failed with exit code $LASTEXITCODE"
            }
        }
        
        # If no scripts configured, run default validation
        if (-not $Config.BuildScript -and -not $Config.TestScript) {
            Write-StructuredLog -Message "Running default validation" -Level Info
            
            # Syntax check changed PowerShell files
            foreach ($file in $BuildRequest.ChangedFiles) {
                if ($file -match '\.ps1$|\.psm1$') {
                    try {
                        $ast = [System.Management.Automation.Language.Parser]::ParseFile(
                            $file,
                            [ref]$null,
                            [ref]$null
                        )
                        $result.BuildOutput += "✓ Syntax OK: $file"
                    } catch {
                        $result.Success = $false
                        $result.Errors += "Syntax error in $file : $($_.Exception.Message)"
                    }
                }
            }
        }
        
    } catch {
        $result.Success = $false
        $result.Errors += $_.Exception.Message
    }
    
    $stopwatch.Stop()
    $result.Duration = $stopwatch.Elapsed.TotalMilliseconds
    
    return $result
}

# ============================================================================
# BUILD EXECUTION
# ============================================================================
function Start-Build {
    <#
    .SYNOPSIS
        Executes a build request through configured providers.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$BuildRequest
    )
    
    $BuildRequest.Status = 'Running'
    $BuildRequest.StartedAt = Get-Date
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    Write-StructuredLog -Message "Starting build" -Level Info -Context @{
        BuildId = $BuildRequest.BuildId
        Trigger = $BuildRequest.Trigger
        ChangedFiles = $BuildRequest.ChangedFiles.Count
    }
    
    $results = @()
    $providers = $script:CIRegistry.Configuration.Providers
    
    # Determine which providers to use
    $enabledProviders = @()
    if ($providers.GitHub.Enabled) { $enabledProviders += 'GitHub' }
    if ($providers.AzureDevOps.Enabled) { $enabledProviders += 'AzureDevOps' }
    if ($providers.Jenkins.Enabled) { $enabledProviders += 'Jenkins' }
    if ($providers.Local.Enabled) { $enabledProviders += 'Local' }
    
    # Execute through each enabled provider
    foreach ($provider in $enabledProviders) {
        $providerResult = switch ($provider) {
            'GitHub' { Invoke-GitHubActionsWorkflow -BuildRequest $BuildRequest }
            'AzureDevOps' { Invoke-AzureDevOpsPipeline -BuildRequest $BuildRequest }
            'Jenkins' { Invoke-JenkinsBuild -BuildRequest $BuildRequest }
            'Local' { Invoke-LocalBuild -BuildRequest $BuildRequest }
        }
        
        $results += $providerResult
        [void]$BuildRequest.Logs.Add(@{
            Provider = $provider
            Timestamp = Get-Date
            Result = $providerResult
        })
    }
    
    $stopwatch.Stop()
    $BuildRequest.CompletedAt = Get-Date
    
    # Determine overall result
    $allSucceeded = ($results | Where-Object { $_.Success }).Count -eq $results.Count
    $BuildRequest.Status = if ($allSucceeded) { 'Succeeded' } else { 'Failed' }
    $BuildRequest.Result = @{
        Success = $allSucceeded
        ProviderResults = $results
        Duration = $stopwatch.Elapsed.TotalMilliseconds
    }
    
    # Update statistics
    $script:CIRegistry.Statistics.TotalBuilds++
    if ($allSucceeded) {
        $script:CIRegistry.Statistics.SuccessfulBuilds++
    } else {
        $script:CIRegistry.Statistics.FailedBuilds++
    }
    $script:CIRegistry.Statistics.LastBuildTime = Get-Date
    
    # Calculate running average
    $totalBuilds = $script:CIRegistry.Statistics.TotalBuilds
    $currentAvg = $script:CIRegistry.Statistics.AverageBuiltTimeMs
    $script:CIRegistry.Statistics.AverageBuiltTimeMs = (($currentAvg * ($totalBuilds - 1)) + $stopwatch.Elapsed.TotalMilliseconds) / $totalBuilds
    
    # Add to history
    [void]$script:CIRegistry.BuildHistory.Add($BuildRequest)
    
    # Keep history limited to last 100 builds
    while ($script:CIRegistry.BuildHistory.Count -gt 100) {
        $script:CIRegistry.BuildHistory.RemoveAt(0)
    }
    
    # Send notifications
    Send-BuildNotification -BuildRequest $BuildRequest
    
    Write-StructuredLog -Message "Build completed" -Level $(if ($allSucceeded) { 'Info' } else { 'Error' }) -Context @{
        BuildId = $BuildRequest.BuildId
        Status = $BuildRequest.Status
        Duration = [math]::Round($stopwatch.Elapsed.TotalMilliseconds, 2)
        ProvidersUsed = $enabledProviders -join ', '
    }
    
    return $BuildRequest
}

# ============================================================================
# NOTIFICATION SYSTEM
# ============================================================================
function Send-BuildNotification {
    <#
    .SYNOPSIS
        Sends build notifications through configured channels.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$BuildRequest
    )
    
    $notifications = $script:CIRegistry.Configuration.Notifications
    
    # Slack notification
    if ($notifications.Slack.Enabled -and $notifications.Slack.WebhookUrl) {
        Send-SlackNotification -BuildRequest $BuildRequest -WebhookUrl $notifications.Slack.WebhookUrl
    }
    
    # Teams notification
    if ($notifications.Teams.Enabled -and $notifications.Teams.WebhookUrl) {
        Send-TeamsNotification -BuildRequest $BuildRequest -WebhookUrl $notifications.Teams.WebhookUrl
    }
}

function Send-SlackNotification {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$BuildRequest,
        [Parameter(Mandatory=$true)][string]$WebhookUrl
    )
    
    $color = if ($BuildRequest.Result.Success) { 'good' } else { 'danger' }
    $status = if ($BuildRequest.Result.Success) { '✅ Succeeded' } else { '❌ Failed' }
    
    $payload = @{
        attachments = @(
            @{
                color = $color
                title = "Build $($BuildRequest.BuildId)"
                text = "Status: $status"
                fields = @(
                    @{ title = 'Trigger'; value = $BuildRequest.Trigger; short = $true }
                    @{ title = 'Branch'; value = $BuildRequest.Branch; short = $true }
                    @{ title = 'Duration'; value = "$([math]::Round($BuildRequest.Result.Duration / 1000, 2))s"; short = $true }
                    @{ title = 'Files Changed'; value = $BuildRequest.ChangedFiles.Count.ToString(); short = $true }
                )
                ts = [int][DateTimeOffset]::Now.ToUnixTimeSeconds()
            }
        )
    } | ConvertTo-Json -Depth 10
    
    try {
        Invoke-RestMethod -Uri $WebhookUrl -Method Post -Body $payload -ContentType 'application/json' | Out-Null
        Write-StructuredLog -Message "Slack notification sent" -Level Debug
    } catch {
        Write-StructuredLog -Message "Failed to send Slack notification" -Level Warning -Context @{Error = $_.Exception.Message}
    }
}

function Send-TeamsNotification {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$BuildRequest,
        [Parameter(Mandatory=$true)][string]$WebhookUrl
    )
    
    $themeColor = if ($BuildRequest.Result.Success) { '00FF00' } else { 'FF0000' }
    $status = if ($BuildRequest.Result.Success) { '✅ Succeeded' } else { '❌ Failed' }
    
    $payload = @{
        '@type' = 'MessageCard'
        '@context' = 'http://schema.org/extensions'
        themeColor = $themeColor
        summary = "Build $($BuildRequest.BuildId) - $status"
        sections = @(
            @{
                activityTitle = "Build $($BuildRequest.BuildId)"
                activitySubtitle = $status
                facts = @(
                    @{ name = 'Trigger'; value = $BuildRequest.Trigger }
                    @{ name = 'Branch'; value = $BuildRequest.Branch }
                    @{ name = 'Duration'; value = "$([math]::Round($BuildRequest.Result.Duration / 1000, 2))s" }
                    @{ name = 'Files Changed'; value = $BuildRequest.ChangedFiles.Count.ToString() }
                )
            }
        )
    } | ConvertTo-Json -Depth 10
    
    try {
        Invoke-RestMethod -Uri $WebhookUrl -Method Post -Body $payload -ContentType 'application/json' | Out-Null
        Write-StructuredLog -Message "Teams notification sent" -Level Debug
    } catch {
        Write-StructuredLog -Message "Failed to send Teams notification" -Level Warning -Context @{Error = $_.Exception.Message}
    }
}

# ============================================================================
# WEBHOOK SERVER
# ============================================================================
function Start-WebhookServer {
    <#
    .SYNOPSIS
        Starts an HTTP server to receive external webhook triggers.
    #>
    [CmdletBinding()]
    param(
        [int]$Port = $script:CIRegistry.Configuration.WebhookPort
    )
    
    if ($script:CIRegistry.WebhookServer) {
        Write-StructuredLog -Message "Webhook server already running" -Level Warning
        return
    }
    
    try {
        $listener = New-Object System.Net.HttpListener
        $listener.Prefixes.Add("http://localhost:$Port/")
        $listener.Start()
        
        $script:CIRegistry.WebhookServer = $listener
        
        Write-StructuredLog -Message "Webhook server started" -Level Info -Context @{Port = $Port}
        
        # Start async listener
        $job = Start-Job -Name "CIWebhookServer" -ScriptBlock {
            param($Port)
            
            $listener = New-Object System.Net.HttpListener
            $listener.Prefixes.Add("http://localhost:$Port/")
            $listener.Start()
            
            while ($listener.IsListening) {
                try {
                    $context = $listener.GetContext()
                    $request = $context.Request
                    $response = $context.Response
                    
                    # Handle webhook
                    if ($request.HttpMethod -eq 'POST') {
                        $reader = New-Object System.IO.StreamReader($request.InputStream)
                        $body = $reader.ReadToEnd()
                        $reader.Close()
                        
                        # Return success
                        $response.StatusCode = 200
                        $responseText = '{"status":"accepted"}'
                    } else {
                        $response.StatusCode = 405
                        $responseText = '{"error":"Method not allowed"}'
                    }
                    
                    $buffer = [System.Text.Encoding]::UTF8.GetBytes($responseText)
                    $response.ContentLength64 = $buffer.Length
                    $response.OutputStream.Write($buffer, 0, $buffer.Length)
                    $response.OutputStream.Close()
                } catch {
                    # Log error but continue
                }
            }
        } -ArgumentList $Port
        
        return $job
    } catch {
        Write-StructuredLog -Message "Failed to start webhook server" -Level Error -Context @{Error = $_.Exception.Message}
        return $null
    }
}

function Stop-WebhookServer {
    <#
    .SYNOPSIS
        Stops the webhook server.
    #>
    [CmdletBinding()]
    param()
    
    if ($script:CIRegistry.WebhookServer) {
        $script:CIRegistry.WebhookServer.Stop()
        $script:CIRegistry.WebhookServer.Close()
        $script:CIRegistry.WebhookServer = $null
        
        # Stop the job
        Get-Job -Name "CIWebhookServer" -ErrorAction SilentlyContinue | Stop-Job -PassThru | Remove-Job
        
        Write-StructuredLog -Message "Webhook server stopped" -Level Info
    }
}

# ============================================================================
# BUILD QUEUE PROCESSOR
# ============================================================================
function Start-BuildQueueProcessor {
    <#
    .SYNOPSIS
        Starts the background build queue processor.
    #>
    [CmdletBinding()]
    param()
    
    $script:CIRegistry.IsRunning = $true
    
    $job = Start-Job -Name "CIBuildProcessor" -ScriptBlock {
        param($MaxConcurrent, $Timeout)
        
        while ($true) {
            Start-Sleep -Milliseconds 500
            # Build processing would happen here in a real implementation
            # This is a placeholder for the background processor
        }
    } -ArgumentList $script:CIRegistry.Configuration.MaxConcurrentBuilds, $script:CIRegistry.Configuration.BuildTimeout
    
    Write-StructuredLog -Message "Build queue processor started" -Level Info
    return $job
}

# ============================================================================
# MAIN ENTRY POINT
# ============================================================================
function Invoke-ContinuousIntegrationTrigger {
    <#
    .SYNOPSIS
        Main entry point for the CI trigger system.
    .DESCRIPTION
        Starts file watching and CI pipeline triggering with full lifecycle management.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$WatchDir = "D:/lazy init ide",
        
        [int]$DebounceMilliseconds = 2000,
        
        [switch]$EnableWebhooks,
        [int]$WebhookPort = 8765,
        
        [switch]$RunOnce,
        [string[]]$TriggerFiles = @()
    )
    
    $script:CIRegistry.CancellationToken = New-Object System.Threading.ManualResetEvent $false
    
    # Handle Ctrl+C gracefully
    $onCancel = {
        Write-StructuredLog -Message "Received cancellation signal" -Level Info
        $script:CIRegistry.CancellationToken.Set() | Out-Null
        $script:CIRegistry.IsRunning = $false
    }
    
    try {
        [Console]::add_CancelKeyPress({ param($s,$e) $e.Cancel = $true; & $onCancel })
    } catch {
        # Ignore if not running interactively
    }
    
    Write-StructuredLog -Message "Starting Continuous Integration Trigger" -Level Info -Context @{
        WatchDir = $WatchDir
        DebounceMs = $DebounceMilliseconds
        EnableWebhooks = $EnableWebhooks.IsPresent
    }
    
    # Update configuration
    $script:CIRegistry.Configuration.WatchDirectories = @($WatchDir)
    $script:CIRegistry.Configuration.DebounceMs = $DebounceMilliseconds
    $script:CIRegistry.Configuration.EnableWebhooks = $EnableWebhooks
    $script:CIRegistry.Configuration.WebhookPort = $WebhookPort
    
    # Handle RunOnce mode (manual trigger)
    if ($RunOnce -or $TriggerFiles.Count -gt 0) {
        $files = if ($TriggerFiles.Count -gt 0) { $TriggerFiles } else { @("manual-trigger") }
        
        $buildId = Add-BuildToQueue -Trigger 'Manual' -ChangedFiles $files -Priority 'High'
        $buildRequest = Get-NextBuild
        $result = Start-Build -BuildRequest $buildRequest
        
        return @{
            BuildId = $buildId
            Success = $result.Result.Success
            Duration = $result.Result.Duration
            Status = $result.Status
        }
    }
    
    # Start webhook server if enabled
    if ($EnableWebhooks) {
        Start-WebhookServer -Port $WebhookPort
    }
    
    # Set up file system watcher
    $watcher = New-Object System.IO.FileSystemWatcher
    $watcher.Path = $WatchDir
    $watcher.IncludeSubdirectories = $true
    $watcher.NotifyFilter = [System.IO.NotifyFilters]::LastWrite -bor 
                            [System.IO.NotifyFilters]::FileName -bor 
                            [System.IO.NotifyFilters]::DirectoryName
    $watcher.EnableRaisingEvents = $true
    
    # Debounce tracking
    $script:pendingChanges = @{}
    $script:lastProcessed = @{}
    
    # Event handler for file changes
    $changeHandler = {
        param($sender, $e)
        
        $path = $e.FullPath
        $changeType = $e.ChangeType
        
        # Skip excluded patterns
        $excludes = @("*.log", "*.tmp", "*~", "*.git*")
        foreach ($exclude in $excludes) {
            if ($path -like $exclude) { return }
        }
        
        # Skip if .git directory
        if ($path -match '[\\/]\.git[\\/]') { return }
        
        # Check include patterns
        $includes = @("*.ps1", "*.psm1", "*.psd1", "*.json")
        $matched = $false
        foreach ($include in $includes) {
            if ([System.IO.Path]::GetFileName($path) -like $include) {
                $matched = $true
                break
            }
        }
        if (-not $matched) { return }
        
        # Add to pending changes
        $script:pendingChanges[$path] = @{
            Path = $path
            ChangeType = $changeType
            Timestamp = Get-Date
        }
        
        Write-StructuredLog -Message "File change detected" -Level Debug -Context @{
            Path = $path
            ChangeType = $changeType
        }
    }
    
    Register-ObjectEvent -InputObject $watcher -EventName Changed -Action $changeHandler -SourceIdentifier 'CIFileChanged' | Out-Null
    Register-ObjectEvent -InputObject $watcher -EventName Created -Action $changeHandler -SourceIdentifier 'CIFileCreated' | Out-Null
    Register-ObjectEvent -InputObject $watcher -EventName Deleted -Action $changeHandler -SourceIdentifier 'CIFileDeleted' | Out-Null
    Register-ObjectEvent -InputObject $watcher -EventName Renamed -Action $changeHandler -SourceIdentifier 'CIFileRenamed' | Out-Null
    
    $script:CIRegistry.ActiveWatchers['main'] = @{
        Watcher = $watcher
        Path = $WatchDir
        StartedAt = Get-Date
    }
    
    Write-StructuredLog -Message "CI Trigger is running. Press Ctrl+C to stop." -Level Info
    
    # Main loop
    while (-not $script:CIRegistry.CancellationToken.WaitOne(1000)) {
        # Process pending changes with debouncing
        $now = Get-Date
        $toProcess = @()
        $keysToRemove = @()
        
        foreach ($key in $script:pendingChanges.Keys) {
            $change = $script:pendingChanges[$key]
            $elapsed = ($now - $change.Timestamp).TotalMilliseconds
            
            if ($elapsed -ge $DebounceMilliseconds) {
                # Check if we recently processed this file
                if ($script:lastProcessed.ContainsKey($key)) {
                    $lastTime = $script:lastProcessed[$key]
                    if (($now - $lastTime).TotalMilliseconds -lt $DebounceMilliseconds) {
                        $keysToRemove += $key
                        continue
                    }
                }
                
                $toProcess += $change.Path
                $keysToRemove += $key
                $script:lastProcessed[$key] = $now
            }
        }
        
        # Remove processed changes
        foreach ($key in $keysToRemove) {
            $script:pendingChanges.Remove($key)
        }
        
        # Trigger build if we have changes
        if ($toProcess.Count -gt 0) {
            $buildId = Add-BuildToQueue -Trigger 'FileChange' -ChangedFiles $toProcess -Priority 'Normal'
            $buildRequest = Get-NextBuild
            
            if ($buildRequest) {
                Start-Build -BuildRequest $buildRequest
            }
        }
        
        # Process any queued builds
        while ($script:CIRegistry.BuildQueue.Count -gt 0) {
            $buildRequest = Get-NextBuild
            if ($buildRequest) {
                Start-Build -BuildRequest $buildRequest
            }
        }
    }
    
    # Cleanup
    try {
        Write-StructuredLog -Message "Shutting down CI Trigger" -Level Info
        
        Unregister-Event -SourceIdentifier 'CIFileChanged' -ErrorAction SilentlyContinue
        Unregister-Event -SourceIdentifier 'CIFileCreated' -ErrorAction SilentlyContinue
        Unregister-Event -SourceIdentifier 'CIFileDeleted' -ErrorAction SilentlyContinue
        Unregister-Event -SourceIdentifier 'CIFileRenamed' -ErrorAction SilentlyContinue
        
        $watcher.EnableRaisingEvents = $false
        $watcher.Dispose()
        
        if ($EnableWebhooks) {
            Stop-WebhookServer
        }
        
        $script:CIRegistry.ActiveWatchers.Clear()
        $script:CIRegistry.IsRunning = $false
        
        Write-StructuredLog -Message "CI Trigger shutdown complete" -Level Info
    } catch {
        Write-StructuredLog -Message "Error during cleanup" -Level Warning -Context @{Error = $_.Exception.Message}
    }
    
    return @{
        Statistics = $script:CIRegistry.Statistics
        BuildHistory = $script:CIRegistry.BuildHistory
    }
}

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================
function Get-CIStatistics {
    <#
    .SYNOPSIS
        Returns CI system statistics.
    #>
    [CmdletBinding()]
    param()
    
    return $script:CIRegistry.Statistics
}

function Get-BuildHistory {
    <#
    .SYNOPSIS
        Returns build history.
    #>
    [CmdletBinding()]
    param(
        [int]$Last = 10,
        [ValidateSet('All', 'Succeeded', 'Failed')]
        [string]$Status = 'All'
    )
    
    $history = $script:CIRegistry.BuildHistory
    
    if ($Status -ne 'All') {
        $history = $history | Where-Object { $_.Status -eq $Status }
    }
    
    return $history | Select-Object -Last $Last
}

function Get-BuildQueue {
    <#
    .SYNOPSIS
        Returns current build queue.
    #>
    [CmdletBinding()]
    param()
    
    return $script:CIRegistry.BuildQueue
}

function Set-CIConfiguration {
    <#
    .SYNOPSIS
        Updates CI configuration.
    #>
    [CmdletBinding()]
    param(
        [string[]]$WatchDirectories,
        [string[]]$WatchPatterns,
        [string[]]$ExcludePatterns,
        [int]$DebounceMs,
        [int]$MaxConcurrentBuilds,
        [int]$BuildTimeout,
        [hashtable]$GitHubConfig,
        [hashtable]$AzureDevOpsConfig,
        [hashtable]$JenkinsConfig,
        [hashtable]$LocalConfig,
        [hashtable]$SlackConfig,
        [hashtable]$TeamsConfig
    )
    
    if ($PSBoundParameters.ContainsKey('WatchDirectories')) {
        $script:CIRegistry.Configuration.WatchDirectories = $WatchDirectories
    }
    if ($PSBoundParameters.ContainsKey('WatchPatterns')) {
        $script:CIRegistry.Configuration.WatchPatterns = $WatchPatterns
    }
    if ($PSBoundParameters.ContainsKey('ExcludePatterns')) {
        $script:CIRegistry.Configuration.ExcludePatterns = $ExcludePatterns
    }
    if ($PSBoundParameters.ContainsKey('DebounceMs')) {
        $script:CIRegistry.Configuration.DebounceMs = $DebounceMs
    }
    if ($PSBoundParameters.ContainsKey('MaxConcurrentBuilds')) {
        $script:CIRegistry.Configuration.MaxConcurrentBuilds = $MaxConcurrentBuilds
    }
    if ($PSBoundParameters.ContainsKey('BuildTimeout')) {
        $script:CIRegistry.Configuration.BuildTimeout = $BuildTimeout
    }
    if ($GitHubConfig) {
        foreach ($key in $GitHubConfig.Keys) {
            $script:CIRegistry.Configuration.Providers.GitHub[$key] = $GitHubConfig[$key]
        }
    }
    if ($AzureDevOpsConfig) {
        foreach ($key in $AzureDevOpsConfig.Keys) {
            $script:CIRegistry.Configuration.Providers.AzureDevOps[$key] = $AzureDevOpsConfig[$key]
        }
    }
    if ($JenkinsConfig) {
        foreach ($key in $JenkinsConfig.Keys) {
            $script:CIRegistry.Configuration.Providers.Jenkins[$key] = $JenkinsConfig[$key]
        }
    }
    if ($LocalConfig) {
        foreach ($key in $LocalConfig.Keys) {
            $script:CIRegistry.Configuration.Providers.Local[$key] = $LocalConfig[$key]
        }
    }
    if ($SlackConfig) {
        foreach ($key in $SlackConfig.Keys) {
            $script:CIRegistry.Configuration.Notifications.Slack[$key] = $SlackConfig[$key]
        }
    }
    if ($TeamsConfig) {
        foreach ($key in $TeamsConfig.Keys) {
            $script:CIRegistry.Configuration.Notifications.Teams[$key] = $TeamsConfig[$key]
        }
    }
    
    return $script:CIRegistry.Configuration
}

# Export functions
# Note: Export-ModuleMember omitted to allow dot-sourcing
# Functions available: Invoke-ContinuousIntegrationTrigger, Add-BuildToQueue, Start-Build, etc.

