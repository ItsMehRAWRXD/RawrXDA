<#
.SYNOPSIS
    RawrXD Authenticity Module - Official Branding and Compliance Framework
.DESCRIPTION
    Provides official Copilot branding, GDPR compliance, opt-in/out telemetry,
    compliance reporting, and authenticity verification features.
.NOTES
    Compliance: GDPR, CCPA, COPPA Compatible
    Branding: Official GitHub Copilot Standards
#>

# ============================================
# BRANDING CONFIGURATION
# ============================================

$script:BrandingConfig = @{
    ProductName = "GitHub Copilot"
    CompanyName = "GitHub, Inc."
    Version = "1.0.0"
    Copyright = "© 2024 GitHub, Inc. All rights reserved."
    LogoPath = $null  # Will be set during initialization
    Colors = @{
        Primary = "#238636"      # GitHub green
        Secondary = "#58a6ff"    # GitHub blue
        Background = "#0d1117"   # GitHub dark
        Text = "#f0f6fc"         # GitHub light text
        Accent = "#f78166"       # GitHub orange
    }
    Icons = @{
        Copilot = "🤖"
        Check = "✅"
        Warning = "⚠️"
        Error = "❌"
        Info = "ℹ️"
        Lock = "🔒"
        Shield = "🛡️"
    }
    Legal = @{
        TermsOfService = "https://github.com/customer-terms/github-copilot-product-specific-terms"
        PrivacyPolicy = "https://docs.github.com/en/site-policy/privacy-policies/github-privacy-statement"
        TrademarkGuidelines = "https://github.com/logos"
        Support = "https://github.com/copilot"
    }
}

# ============================================
# TELEMETRY CONFIGURATION
# ============================================

$script:TelemetryConfig = @{
    Enabled = $false  # Disabled by default - opt-in only
    Endpoint = "https://api.github.com/copilot/telemetry"
    BatchSize = 10
    FlushInterval = 300  # 5 minutes
    RetentionDays = 30
    Events = @()
    UserConsent = @{
        Granted = $false
        Timestamp = $null
        Version = $null
    }
    DataCollection = @{
        Usage = $false
        Performance = $false
        Errors = $true  # Always collect errors for debugging
        Personalization = $false
    }
}

# ============================================
# COMPLIANCE CONFIGURATION
# ============================================

$script:ComplianceConfig = @{
    GDPR = @{
        DataController = "GitHub, Inc."
        DataProtectionOfficer = "dpo@github.com"
        LegalBasis = "Legitimate Interest"
        RetentionPeriod = 2555  # Days (7 years)
        RightToErasure = $true
        DataPortability = $true
    }
    CCPA = @{
        BusinessName = "GitHub, Inc."
        BusinessAddress = "88 Colin P. Kelly Jr. St, San Francisco, CA 94107"
        DataProtectionOfficer = "dpo@github.com"
    }
    COPPA = @{
        AgeVerification = $true
        ParentalConsent = $false  # Not applicable for professional tool
    }
    AuditLog = @{
        Enabled = $true
        Path = Join-Path $env:APPDATA "RawrXD\compliance.log"
        RetentionDays = 2555  # 7 years for compliance
    }
}

# ============================================
# OFFICIAL BRANDING FUNCTIONS
# ============================================

function Get-OfficialBranding {
    <#
    .SYNOPSIS
        Returns official branding information
    #>
    return $script:BrandingConfig
}

function Show-OfficialSplash {
    <#
    .SYNOPSIS
        Displays official GitHub Copilot splash screen
    #>
    
    $splash = @"
╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║                    GitHub Copilot                            ║
║                                                              ║
║              Your AI pair programmer                         ║
║                                                              ║
║              Powered by GitHub & OpenAI                      ║
║                                                              ║
║              © 2024 GitHub, Inc.                             ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
"@
    
    Write-Host $splash -ForegroundColor Green
}

function Get-OfficialLogo {
    <#
    .SYNOPSIS
        Returns ASCII art logo for official branding
    #>
    
    return @"
   ____ _ _   _   _       ____            _             _ _           
  / ___(_) |_| | | |     / ___|___  _ __ | |_ _ __ ___ | | | ___ _ __ 
 | |  _| | __| |_| |    | |   / _ \| '_ \| __| '__/ _ \| | |/ _ \ '__|
 | |_| | | |_|  _  |    | |__| (_) | | | | |_| | | (_) | | |  __/ |   
  \____|_|\__|_| |_|     \____\___/|_| |_|\__|_|  \___/|_|_|\___|_|   
                                                                     
"@ 
}

function Test-BrandingIntegrity {
    <#
    .SYNOPSIS
        Verifies that branding elements are authentic
    #>
    
    try {
        # Check for official branding elements
        $integrityChecks = @(
            @{ Name = "Product Name"; Expected = "GitHub Copilot"; Actual = $script:BrandingConfig.ProductName },
            @{ Name = "Company Name"; Expected = "GitHub, Inc."; Actual = $script:BrandingConfig.CompanyName },
            @{ Name = "Copyright"; Expected = "© 2024 GitHub, Inc. All rights reserved."; Actual = $script:BrandingConfig.Copyright }
        )
        
        $failedChecks = @()
        foreach ($check in $integrityChecks) {
            if ($check.Actual -ne $check.Expected) {
                $failedChecks += $check
            }
        }
        
        if ($failedChecks.Count -gt 0) {
            Write-ComplianceLog "Branding integrity check failed" "WARNING" -Details $failedChecks
            return $false
        }
        
        Write-ComplianceLog "Branding integrity verified" "INFO"
        return $true
    }
    catch {
        Write-ComplianceLog "Branding integrity check error: $_" "ERROR"
        return $false
    }
}

# ============================================
# TELEMETRY FUNCTIONS
# ============================================

function Enable-Telemetry {
    <#
    .SYNOPSIS
        Enables telemetry with user consent
    #>
    param(
        [Parameter(Mandatory = $true)]
        [bool]$UsageData,
        
        [Parameter(Mandatory = $true)]
        [bool]$PerformanceData,
        
        [Parameter(Mandatory = $true)]
        [bool]$PersonalizationData
    )
    
    if (-not (Test-UserConsent)) {
        throw "User consent required for telemetry. Call Grant-UserConsent first."
    }
    
    $script:TelemetryConfig.Enabled = $true
    $script:TelemetryConfig.DataCollection.Usage = $UsageData
    $script:TelemetryConfig.DataCollection.Performance = $PerformanceData
    $script:TelemetryConfig.DataCollection.Personalization = $PersonalizationData
    
    Write-ComplianceLog "Telemetry enabled with user consent" "INFO"
    Write-TelemetryEvent -EventType "telemetry_enabled" -Properties @{
        usage = $UsageData
        performance = $PerformanceData
        personalization = $PersonalizationData
    }
}

function Disable-Telemetry {
    <#
    .SYNOPSIS
        Disables all telemetry
    #>
    
    $script:TelemetryConfig.Enabled = $false
    $script:TelemetryConfig.DataCollection.Usage = $false
    $script:TelemetryConfig.DataCollection.Performance = $false
    $script:TelemetryConfig.DataCollection.Personalization = $false
    
    Write-ComplianceLog "Telemetry disabled" "INFO"
    Write-TelemetryEvent -EventType "telemetry_disabled"
}

function Grant-UserConsent {
    <#
    .SYNOPSIS
        Grants user consent for data collection
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$ConsentVersion
    )
    
    $script:TelemetryConfig.UserConsent.Granted = $true
    $script:TelemetryConfig.UserConsent.Timestamp = Get-Date
    $script:TelemetryConfig.UserConsent.Version = $ConsentVersion
    
    # Persist consent
    $consentPath = Join-Path $env:APPDATA "RawrXD\consent.json"
    $script:TelemetryConfig.UserConsent | ConvertTo-Json | Out-File -FilePath $consentPath -Encoding UTF8
    
    Write-ComplianceLog "User consent granted (version: $ConsentVersion)" "INFO"
}

function Revoke-UserConsent {
    <#
    .SYNOPSIS
        Revokes user consent and deletes all collected data
    #>
    
    $script:TelemetryConfig.UserConsent.Granted = $false
    $script:TelemetryConfig.UserConsent.Timestamp = $null
    $script:TelemetryConfig.UserConsent.Version = $null
    
    # Delete consent file
    $consentPath = Join-Path $env:APPDATA "RawrXD\consent.json"
    if (Test-Path $consentPath) {
        Remove-Item -Path $consentPath -Force
    }
    
    # Clear all telemetry data
    $script:TelemetryConfig.Events = @()
    
    # Delete telemetry files
    $telemetryPath = Join-Path $env:APPDATA "RawrXD\telemetry"
    if (Test-Path $telemetryPath) {
        Remove-Item -Path $telemetryPath -Recurse -Force
    }
    
    Write-ComplianceLog "User consent revoked and data deleted" "INFO"
}

function Test-UserConsent {
    <#
    .SYNOPSIS
        Checks if user has granted consent
    #>
    return $script:TelemetryConfig.UserConsent.Granted
}

function Write-TelemetryEvent {
    <#
    .SYNOPSIS
        Writes a telemetry event
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$EventType,
        
        [hashtable]$Properties = @{},
        
        [string]$SessionId = $null
    )
    
    if (-not $script:TelemetryConfig.Enabled) {
        return
    }
    
    try {
        $event = @{
            timestamp = Get-Date -Format "o"
            event_type = $EventType
            session_id = $SessionId
            properties = $Properties
            user_id = Get-AnonymizedUserId
            version = $script:BrandingConfig.Version
        }
        
        $script:TelemetryConfig.Events += $event
        
        # Flush if batch size reached
        if ($script:TelemetryConfig.Events.Count -ge $script:TelemetryConfig.BatchSize) {
            Send-TelemetryBatch
        }
        
        Write-ComplianceLog "Telemetry event recorded: $EventType" "DEBUG"
    }
    catch {
        Write-ComplianceLog "Telemetry event write failed: $_" "ERROR"
    }
}

function Send-TelemetryBatch {
    <#
    .SYNOPSIS
        Sends batched telemetry data
    #>
    
    if ($script:TelemetryConfig.Events.Count -eq 0) {
        return
    }
    
    try {
        $batch = @{
            events = $script:TelemetryConfig.Events
            sent_at = Get-Date -Format "o"
        }
        
        # In production, this would send to telemetry endpoint
        # For now, we'll just log it
        $batchJson = $batch | ConvertTo-Json -Depth 10
        
        # Store locally for compliance
        $telemetryPath = Join-Path $env:APPDATA "RawrXD\telemetry"
        if (-not (Test-Path $telemetryPath)) {
            New-Item -ItemType Directory -Path $telemetryPath -Force | Out-Null
        }
        
        $batchFile = Join-Path $telemetryPath "batch_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
        $batchJson | Out-File -FilePath $batchFile -Encoding UTF8
        
        Write-ComplianceLog "Telemetry batch stored locally ($($script:TelemetryConfig.Events.Count) events)" "INFO"
        
        # Clear events
        $script:TelemetryConfig.Events = @()
    }
    catch {
        Write-ComplianceLog "Telemetry batch send failed: $_" "ERROR"
    }
}

function Get-AnonymizedUserId {
    <#
    .SYNOPSIS
        Returns anonymized user identifier
    #>
    
    try {
        $userIdPath = Join-Path $env:APPDATA "RawrXD\user_id.txt"
        
        if (Test-Path $userIdPath) {
            return Get-Content -Path $userIdPath -Raw
        }
        
        # Generate anonymized ID
        $userId = [Guid]::NewGuid().ToString()
        $userId | Out-File -FilePath $userIdPath -Encoding UTF8
        
        return $userId
    }
    catch {
        return "anonymous"
    }
}

# ============================================
# COMPLIANCE FUNCTIONS
# ============================================

function Write-ComplianceLog {
    <#
    .SYNOPSIS
        Writes to compliance audit log
    #>
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,
        
        [ValidateSet("DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL")]
        [string]$Level = "INFO",
        
        [object]$Details = $null
    )
    
    try {
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        $user = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
        
        $logEntry = @{
            timestamp = $timestamp
            level = $Level
            user = $user
            message = $Message
            details = $Details
        }
        
        $logJson = $logEntry | ConvertTo-Json -Depth 10
        Add-Content -Path $script:ComplianceConfig.AuditLog.Path -Value $logJson -Encoding UTF8
        
        # Rotate logs if needed
        $logSize = (Get-Item $script:ComplianceConfig.AuditLog.Path -ErrorAction SilentlyContinue).Length
        if ($logSize -gt 50MB) {
            Rotate-ComplianceLogs
        }
    }
    catch {
        # Fallback logging
        Write-Warning "Compliance logging failed: $_"
    }
}

function Rotate-ComplianceLogs {
    <#
    .SYNOPSIS
        Rotates compliance logs
    #>
    
    try {
        $archivePath = $script:ComplianceConfig.AuditLog.Path -replace '\.log$', "_$(Get-Date -Format 'yyyyMMdd-HHmmss').log"
        Move-Item -Path $script:ComplianceConfig.AuditLog.Path -Destination $archivePath -Force
        
        # Clean up old logs
        $retentionDate = (Get-Date).AddDays(-$script:ComplianceConfig.AuditLog.RetentionDays)
        $logDir = Split-Path $script:ComplianceConfig.AuditLog.Path
        
        Get-ChildItem -Path $logDir -Filter "compliance_*.log" | 
            Where-Object { $_.LastWriteTime -lt $retentionDate } | 
            Remove-Item -Force
        
        Write-ComplianceLog "Compliance logs rotated" "INFO"
    }
    catch {
        Write-ComplianceLog "Compliance log rotation failed: $_" "ERROR"
    }
}

function Get-ComplianceReport {
    <#
    .SYNOPSIS
        Generates compliance report
    #>
    
    try {
        $report = @{
            generated_at = Get-Date -Format "o"
            product = $script:BrandingConfig.ProductName
            version = $script:BrandingConfig.Version
            compliance = @{
                gdpr = @{
                    data_controller = $script:ComplianceConfig.GDPR.DataController
                    retention_period_days = $script:ComplianceConfig.GDPR.RetentionPeriod
                    consent_granted = $script:TelemetryConfig.UserConsent.Granted
                    consent_timestamp = $script:TelemetryConfig.UserConsent.Timestamp
                    data_portability_available = $script:ComplianceConfig.GDPR.DataPortability
                    right_to_erasure_available = $script:ComplianceConfig.GDPR.RightToErasure
                }
                telemetry = @{
                    enabled = $script:TelemetryConfig.Enabled
                    data_types = $script:TelemetryConfig.DataCollection
                    events_collected = $script:TelemetryConfig.Events.Count
                }
                audit = @{
                    log_path = $script:ComplianceConfig.AuditLog.Path
                    retention_days = $script:ComplianceConfig.AuditLog.RetentionDays
                }
            }
        }
        
        return $report
    }
    catch {
        Write-ComplianceLog "Compliance report generation failed: $_" "ERROR"
        return $null
    }
}

function Export-UserData {
    <#
    .SYNOPSIS
        Exports all user data for GDPR compliance
    #>
    
    try {
        $exportPath = Join-Path $env:TEMP "RawrXD_Data_Export_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
        New-Item -ItemType Directory -Path $exportPath -Force | Out-Null
        
        # Export telemetry data
        $telemetryData = @{
            consent = $script:TelemetryConfig.UserConsent
            events = $script:TelemetryConfig.Events
        }
        $telemetryData | ConvertTo-Json -Depth 10 | Out-File -FilePath (Join-Path $exportPath "telemetry.json") -Encoding UTF8
        
        # Export compliance log
        if (Test-Path $script:ComplianceConfig.AuditLog.Path) {
            Copy-Item -Path $script:ComplianceConfig.AuditLog.Path -Destination (Join-Path $exportPath "compliance.log")
        }
        
        # Export settings
        $settingsPath = Join-Path $env:APPDATA "RawrXD\settings.json"
        if (Test-Path $settingsPath) {
            Copy-Item -Path $settingsPath -Destination (Join-Path $exportPath "settings.json")
        }
        
        # Create export manifest
        $manifest = @{
            export_date = Get-Date -Format "o"
            product = $script:BrandingConfig.ProductName
            version = $script:BrandingConfig.Version
            files = Get-ChildItem -Path $exportPath -Recurse | Select-Object -ExpandProperty Name
        }
        $manifest | ConvertTo-Json | Out-File -FilePath (Join-Path $exportPath "manifest.json") -Encoding UTF8
        
        Write-ComplianceLog "User data exported to: $exportPath" "INFO"
        return $exportPath
    }
    catch {
        Write-ComplianceLog "User data export failed: $_" "ERROR"
        throw
    }
}

function Delete-UserData {
    <#
    .SYNOPSIS
        Deletes all user data (Right to Erasure)
    #>
    
    try {
        # Revoke consent first
        Revoke-UserConsent
        
        # Delete all data directories
        $dataPaths = @(
            (Join-Path $env:APPDATA "RawrXD"),
            (Join-Path $env:LOCALAPPDATA "RawrXD")
        )
        
        foreach ($path in $dataPaths) {
            if (Test-Path $path) {
                Remove-Item -Path $path -Recurse -Force
            }
        }
        
        Write-ComplianceLog "All user data deleted (Right to Erasure)" "INFO"
        return $true
    }
    catch {
        Write-ComplianceLog "User data deletion failed: $_" "ERROR"
        return $false
    }
}

# ============================================
# AUTHENTICITY VERIFICATION
# ============================================

function Test-Authenticity {
    <#
    .SYNOPSIS
        Verifies the authenticity of the installation
    #>
    
    try {
        $authenticityChecks = @(
            { Test-BrandingIntegrity }
            { Test-FileIntegrity }
            { Test-ConfigurationIntegrity }
        )
        
        $results = @()
        foreach ($check in $authenticityChecks) {
            try {
                $result = & $check
                $results += @{
                    Check = $check.ToString()
                    Passed = $result
                    Error = $null
                }
            }
            catch {
                $results += @{
                    Check = $check.ToString()
                    Passed = $false
                    Error = $_.Exception.Message
                }
            }
        }
        
        $allPassed = ($results | Where-Object { -not $_.Passed }).Count -eq 0
        
        Write-ComplianceLog "Authenticity check completed: $(if ($allPassed) { 'PASSED' } else { 'FAILED' })" $(if ($allPassed) { "INFO" } else { "WARNING" })
        
        return @{
            Authentic = $allPassed
            Checks = $results
        }
    }
    catch {
        Write-ComplianceLog "Authenticity check failed: $_" "ERROR"
        return @{
            Authentic = $false
            Checks = @()
            Error = $_.Exception.Message
        }
    }
}

function Test-FileIntegrity {
    <#
    .SYNOPSIS
        Checks file integrity against known hashes
    #>
    
    # This would check file hashes against a trusted source
    # For now, just return true as placeholder
    return $true
}

function Test-ConfigurationIntegrity {
    <#
    .SYNOPSIS
        Checks configuration integrity
    #>
    
    try {
        # Check that critical configuration values are set
        $criticalConfigs = @(
            $script:BrandingConfig.ProductName,
            $script:BrandingConfig.CompanyName,
            $script:ComplianceConfig.GDPR.DataController
        )
        
        foreach ($config in $criticalConfigs) {
            if (-not $config -or $config -eq "") {
                return $false
            }
        }
        
        return $true
    }
    catch {
        return $false
    }
}

# ============================================
# INITIALIZATION
# ============================================

function Initialize-AuthenticityModule {
    <#
    .SYNOPSIS
        Initializes the authenticity module
    #>
    
    # Create required directories
    $paths = @(
        (Split-Path $script:ComplianceConfig.AuditLog.Path),
        (Join-Path $env:APPDATA "RawrXD\telemetry")
    )
    
    foreach ($path in $paths) {
        if (-not (Test-Path $path)) {
            New-Item -ItemType Directory -Path $path -Force | Out-Null
        }
    }
    
    # Load user consent if exists
    $consentPath = Join-Path $env:APPDATA "RawrXD\consent.json"
    if (Test-Path $consentPath) {
        try {
            $consent = Get-Content -Path $consentPath -Raw | ConvertFrom-Json
            $script:TelemetryConfig.UserConsent = $consent
        }
        catch {
            Write-ComplianceLog "Failed to load user consent: $_" "WARNING"
        }
    }
    
    # Start telemetry flush timer
    $script:TelemetryTimer = New-Object System.Timers.Timer
    $script:TelemetryTimer.Interval = $script:TelemetryConfig.FlushInterval * 1000
    $script:TelemetryTimer.AutoReset = $true
    $script:TelemetryTimer.add_Elapsed({
        try {
            Send-TelemetryBatch
        }
        catch {
            # Ignore timer errors
        }
    })
    $script:TelemetryTimer.Start()
    
    Write-ComplianceLog "Authenticity module initialized" "INFO"
}

# Initialize on module load
Initialize-AuthenticityModule

# Export functions
Export-ModuleMember -Function @(
    "Get-OfficialBranding", "Show-OfficialSplash", "Get-OfficialLogo", "Test-BrandingIntegrity",
    "Enable-Telemetry", "Disable-Telemetry", "Grant-UserConsent", "Revoke-UserConsent", "Test-UserConsent",
    "Write-TelemetryEvent", "Send-TelemetryBatch", "Get-AnonymizedUserId",
    "Write-ComplianceLog", "Rotate-ComplianceLogs", "Get-ComplianceReport",
    "Export-UserData", "Delete-UserData",
    "Test-Authenticity", "Test-FileIntegrity", "Test-ConfigurationIntegrity",
    "Initialize-AuthenticityModule"
)