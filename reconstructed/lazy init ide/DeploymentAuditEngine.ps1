<#
.SYNOPSIS
    Deployment Audit Engine - Automated Deployment Validation and Security Assessment
.DESCRIPTION
    Comprehensive deployment auditing system for:
    - Configuration validation
    - Security gap analysis
    - Dependency verification
    - Performance baseline establishment
    - Compliance checking
    - Pre-deployment readiness assessment
#>

using namespace System.Collections.Generic
using namespace System.IO

param(
    [Parameter(Mandatory=$false)]
    [string]$ProjectPath,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputPath = ".\deployment_audit_output",
    
    [Parameter(Mandatory=$false)]
    [string]$ConfigPath = ".\deployment_audit_config.json",
    
    [Parameter(Mandatory=$false)]
    [switch]$ValidateConfiguration,
    
    [Parameter(Mandatory=$false)]
    [switch]$CheckSecurity,
    
    [Parameter(Mandatory=$false)]
    [switch]$VerifyDependencies,
    
    [Parameter(Mandatory=$false)]
    [switch]$EstablishBaseline,
    
    [Parameter(Mandatory=$false)]
    [switch]$CheckCompliance,
    
    [Parameter(Mandatory=$false)]
    [switch]$GenerateReadinessReport,
    
    [Parameter(Mandatory=$false)]
    [switch]$Interactive
)

# Global configuration
$global:DeploymentAuditConfig = @{
    Version = "1.0.0"
    EngineName = "DeploymentAuditEngine"
    AuditDepth = "Comprehensive"  # Basic, Standard, Comprehensive, Full
    EnableParallelProcessing = $true
    MaxThreadCount = 8
    SecurityStandards = @("OWASP", "CIS", "NIST", "PCI-DSS")
    ComplianceFrameworks = @("SOC2", "ISO27001", "HIPAA", "GDPR")
    PerformanceThresholds = @{
        MaxResponseTime = 1000  # milliseconds
        MaxMemoryUsage = 85     # percentage
        MaxCpuUsage = 80        # percentage
        MinUptime = 99.9        # percentage
    }
    ConfigurationValidation = @{
        Enabled = $true
        CheckFilePermissions = $true
        ValidateSyntax = $true
        VerifyPaths = $true
        CheckEnvironmentVariables = $true
    }
    SecurityChecking = @{
        Enabled = $true
        ScanForSecrets = $true
        CheckFilePermissions = $true
        ValidateCertificates = $true
        VerifyAuthentication = $true
        CheckAuthorization = $true
        AuditLogging = $true
    }
    DependencyVerification = @{
        Enabled = $true
        CheckVersionCompatibility = $true
        VerifyIntegrity = $true
        ScanForVulnerabilities = $true
        ValidateLicenses = $true
    }
}

# Audit results storage
$global:DeploymentAuditResults = @{
    ConfigurationIssues = [List[hashtable]]::new()
    SecurityGaps = [List[hashtable]]::new()
    MissingDependencies = [List[hashtable]]::new()
    PerformanceBottlenecks = [List[hashtable]]::new()
    ComplianceViolations = [List[hashtable]]::new()
    ReadinessScore = 0
    RiskAssessment = @{
        OverallRisk = "Unknown"
        RiskScore = 0
        CriticalIssues = 0
        HighIssues = 0
        MediumIssues = 0
        LowIssues = 0
    }
    BaselineMetrics = @{}
    DeploymentReadiness = @{
        Status = "NotReady"
        Blockers = [List[string]]::new()
        Warnings = [List[string]]::new()
        Recommendations = [List[string]]::new()
    }
}

# Initialize logging
function Initialize-Logging {
    param($LogPath)
    
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $global:LogFile = Join-Path $LogPath "deployment_audit_$timestamp.log"
    $global:EmergencyLog = Join-Path $LogPath "emergency_deployment_audit.log"
    
    # Create log directory
    if (!(Test-Path $LogPath)) {
        New-Item -ItemType Directory -Path $LogPath -Force | Out-Null
    }
    
    Write-Log "INFO" "Deployment Audit Engine v$($global:DeploymentAuditConfig.Version) initialized"
    Write-Log "INFO" "Log file: $global:LogFile"
}

# Enhanced logging function
function Write-Log {
    param(
        [string]$Level = "INFO",
        [string]$Message,
        [string]$Component = "Main"
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    $logEntry = "[$timestamp] [$Level] [$Component] $Message"
    
    # Console output with colors
    $color = switch ($Level) {
        "ERROR" { "Red" }
        "WARNING" { "Yellow" }
        "SUCCESS" { "Green" }
        "INFO" { "Cyan" }
        "DEBUG" { "Gray" }
        default { "White" }
    }
    
    Write-Host $logEntry -ForegroundColor $color
    
    # File logging
    if ($global:LogFile) {
        Add-Content -Path $global:LogFile -Value $logEntry -Encoding UTF8
    }
}

# Load configuration
function Load-Configuration {
    param($ConfigPath)
    
    if (Test-Path $ConfigPath) {
        try {
            $configContent = Get-Content $ConfigPath -Raw | ConvertFrom-Json
            
            # Merge configurations
            foreach ($key in $configContent.PSObject.Properties.Name) {
                if ($global:DeploymentAuditConfig.ContainsKey($key)) {
                    $value = $configContent.$key
                    if ($value -is [System.Management.Automation.PSCustomObject]) {
                        foreach ($subKey in $value.PSObject.Properties.Name) {
                            $global:DeploymentAuditConfig[$key][$subKey] = $value.$subKey
                        }
                    }
                    else {
                        $global:DeploymentAuditConfig[$key] = $value
                    }
                }
            }
            
            Write-Log "INFO" "Configuration loaded from $ConfigPath"
        }
        catch {
            Write-Log "WARNING" "Failed to load configuration: $_"
        }
    }
}

# Save configuration
function Save-Configuration {
    param($ConfigPath)
    
    try {
        $global:DeploymentAuditConfig | ConvertTo-Json -Depth 10 | Out-File $ConfigPath -Encoding UTF8
        Write-Log "INFO" "Configuration saved to $ConfigPath"
    }
    catch {
        Write-Log "ERROR" "Failed to save configuration: $_"
    }
}

# Main execution function
function Start-DeploymentAudit {
    param(
        [string]$ProjectPath,
        [string]$OutputPath
    )
    
    Write-Log "INFO" "Starting deployment audit process"
    Write-Log "INFO" "Project: $ProjectPath"
    Write-Log "INFO" "Output: $OutputPath"
    
    # Validate project path
    if (!(Test-Path $ProjectPath)) {
        Write-Log "ERROR" "Project path does not exist: $ProjectPath"
        return $false
    }
    
    # Create output directory
    if (!(Test-Path $OutputPath)) {
        New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
        Write-Log "INFO" "Created output directory: $OutputPath"
    }
    
    # Initialize logging
    Initialize-Logging -LogPath (Join-Path $OutputPath "logs")
    
    $startTime = Get-Date
    
    # Phase 1: Project Discovery
    Write-Log "INFO" "Phase 1: Project Discovery"
    $discoveryResults = Start-ProjectDiscovery -ProjectPath $ProjectPath
    
    if ($discoveryResults.TotalFiles -eq 0) {
        Write-Log "ERROR" "No project files found in $ProjectPath"
        return $false
    }
    
    Write-Log "SUCCESS" "Discovered $($discoveryResults.TotalFiles) files across $($discoveryResults.ComponentCount) components"
    
    # Phase 2: Configuration Validation (if enabled)
    if ($ValidateConfiguration -or $global:DeploymentAuditConfig.ConfigurationValidation.Enabled) {
        Write-Log "INFO" "Phase 2: Configuration Validation"
        $configResults = Start-ConfigurationValidation -ProjectFiles $discoveryResults.Files
    }
    
    # Phase 3: Security Checking (if enabled)
    if ($CheckSecurity -or $global:DeploymentAuditConfig.SecurityChecking.Enabled) {
        Write-Log "INFO" "Phase 3: Security Assessment"
        $securityResults = Start-SecurityAssessment -ProjectFiles $discoveryResults.Files
    }
    
    # Phase 4: Dependency Verification (if enabled)
    if ($VerifyDependencies -or $global:DeploymentAuditConfig.DependencyVerification.Enabled) {
        Write-Log "INFO" "Phase 4: Dependency Verification"
        $dependencyResults = Start-DependencyVerification -ProjectPath $ProjectPath
    }
    
    # Phase 5: Baseline Establishment (if enabled)
    if ($EstablishBaseline) {
        Write-Log "INFO" "Phase 5: Performance Baseline Establishment"
        $baselineResults = Establish-PerformanceBaseline -ProjectPath $ProjectPath
    }
    
    # Phase 6: Compliance Checking (if enabled)
    if ($CheckCompliance -or $global:DeploymentAuditConfig.ComplianceFrameworks.Count -gt 0) {
        Write-Log "INFO" "Phase 6: Compliance Verification"
        $complianceResults = Start-ComplianceChecking -ProjectFiles $discoveryResults.Files
    }
    
    # Phase 7: Readiness Assessment
    Write-Log "INFO" "Phase 7: Deployment Readiness Assessment"
    $readinessResults = Start-ReadinessAssessment -ProjectPath $ProjectPath
    
    # Phase 8: Report Generation
    if ($GenerateReadinessReport) {
        Write-Log "INFO" "Phase 8: Readiness Report Generation"
        $reportPath = Generate-ReadinessReport -OutputPath $OutputPath
        Write-Log "SUCCESS" "Readiness report generated: $reportPath"
    }
    
    # Update statistics
    $endTime = Get-Date
    $global:DeploymentAuditResults.BaselineMetrics.AnalysisDuration = ($endTime - $startTime).TotalSeconds
    
    # Calculate overall readiness
    Calculate-DeploymentReadiness
    
    # Save results
    Save-DeploymentAuditResults -OutputPath $OutputPath
    
    Write-Log "SUCCESS" "Deployment audit completed successfully"
    Write-Log "INFO" "Analysis duration: $($global:DeploymentAuditResults.BaselineMetrics.AnalysisDuration) seconds"
    Write-Log "INFO" "Deployment Readiness: $($global:DeploymentAuditResults.DeploymentReadiness.Status)"
    Write-Log "INFO" "Overall Risk Level: $($global:DeploymentAuditResults.RiskAssessment.OverallRisk)"
    
    return $true
}

# Project discovery function
function Start-ProjectDiscovery {
    param($ProjectPath)
    
    Write-Log "INFO" "Discovering project files and structure..."
    
    $results = @{
        Files = [List[hashtable]]::new()
        TotalFiles = 0
        ComponentCount = 0
        FileTypes = @{}
        Components = @{}
        ConfigurationFiles = [List[hashtable]]::new()
        BuildFiles = [List[hashtable]]::new()
        DeploymentFiles = [List[hashtable]]::new()
        TotalSize = 0
    }
    
    # Component detection patterns
    $componentPatterns = @{
        "WebAPI" = @("api", "controller", "endpoint", "route")
        "Database" = @("database", "db", "sql", "migration", "model")
        "Frontend" = @("ui", "view", "template", "component", "page")
        "Backend" = @("service", "business", "logic", "processor")
        "Configuration" = @("config", "settings", "appsettings", "web.config")
        "Deployment" = @("docker", "kubernetes", "k8s", "deploy", "pipeline")
        "Security" = @("auth", "security", "permission", "role", "policy")
        "Integration" = @("client", "connector", "adapter", "integration")
    }
    
    # File type mapping
    $fileTypeMap = @{
        # Configuration
        '.json' = 'JSONConfig'; '.xml' = 'XMLConfig'; '.yaml' = 'YAMLConfig'
        '.yml' = 'YMLConfig'; '.ini' = 'INIConfig'; '.conf' = 'CONFConfig'
        '.config' = 'CONFIGFile'; '.toml' = 'TOMLConfig'
        
        # Build and deployment
        '.ps1' = 'PowerShellScript'; '.psm1' = 'PowerShellModule'
        '.sh' = 'ShellScript'; '.bash' = 'BashScript'
        '.bat' = 'BatchScript'; '.cmd' = 'CommandScript'
        '.dockerfile' = 'Dockerfile'; 'docker-compose.yml' = 'DockerCompose'
        '.csproj' = 'CSharpProject'; '.vbproj' = 'VBProject'
        '.fsproj' = 'FSharpProject'; '.pyproj' = 'PythonProject'
        '.jsproj' = 'JavaScriptProject'; '.tsproj' = 'TypeScriptProject'
        
        # Source code
        '.cs' = 'CSharpCode'; '.vb' = 'VBCode'; '.fs' = 'FSharpCode'
        '.cpp' = 'CppCode'; '.c' = 'CCode'; '.h' = 'CHeader'
        '.hpp' = 'CppHeader'; '.java' = 'JavaCode'; '.py' = 'PythonCode'
        '.js' = 'JavaScriptCode'; '.ts' = 'TypeScriptCode'
        '.go' = 'GoCode'; '.rs' = 'RustCode'
        
        # Web and markup
        '.html' = 'HTMLFile'; '.htm' = 'HTMFile'; '.css' = 'CSSFile'
        '.scss' = 'SCSSFile'; '.sass' = 'SASSFile'; '.less' = 'LESSFile'
        '.vue' = 'VueComponent'; '.jsx' = 'JSXFile'; '.tsx' = 'TSXFile'
        
        # Documentation
        '.md' = 'MarkdownFile'; '.txt' = 'TextFile'; '.rst' = 'RSTFile'
        '.doc' = 'WordDoc'; '.docx' = 'WordDocX'; '.pdf' = 'PDFFile'
        
        # Database
        '.sql' = 'SQLFile'; '.sqlite' = 'SQLiteDB'; '.db' = 'DatabaseFile'
        '.mdf' = 'SQLServerDB'; '.ldf' = 'SQLServerLog'
        
        # Certificates and security
        '.crt' = 'Certificate'; '.pem' = 'PEMCertificate'; '.key' = 'PrivateKey'
        '.pfx' = 'PFXCertificate'; '.p12' = 'P12Certificate'
        
        # Archives
        '.zip' = 'ZIPArchive'; '.tar' = 'TARArchive'; '.gz' = 'GZIPArchive'
        '.7z' = '7ZIPArchive'; '.rar' = 'RARArchive'
        
        # Executables
        '.exe' = 'Executable'; '.dll' = 'Library'; '.so' = 'SharedObject'
        '.bin' = 'BinaryFile'
    }
    
    # Get all files recursively
    $allFiles = Get-ChildItem -Path $ProjectPath -Recurse -File -ErrorAction SilentlyContinue
    
    foreach ($file in $allFiles) {
        $extension = $file.Extension.ToLower()
        $fileType = $fileTypeMap[$extension]
        
        if (!$fileType) {
            # Check for special files
            if ($file.Name -eq "dockerfile") { $fileType = "Dockerfile" }
            elseif ($file.Name -eq "docker-compose.yml") { $fileType = "DockerCompose" }
            elseif ($file.Name -eq "docker-compose.yaml") { $fileType = "DockerCompose" }
            elseif ($file.Name -eq "makefile") { $fileType = "Makefile" }
            elseif ($file.Name -eq "package.json") { $fileType = "PackageJSON" }
            elseif ($file.Name -eq "requirements.txt") { $fileType = "RequirementsTXT" }
            elseif ($file.Name -eq "pom.xml") { $fileType = "MavenPOM" }
            elseif ($file.Name -eq "build.gradle") { $fileType = "GradleBuild" }
            elseif ($file.Name -eq "Cargo.toml") { $fileType = "CargoConfig" }
            elseif ($file.Name -eq "go.mod") { $fileType = "GoModule" }
            else { $fileType = "Unknown" }
        }
        
        $fileInfo = @{
            Path = $file.FullName
            Name = $file.Name
            Extension = $extension
            Type = $fileType
            Size = $file.Length
            LastModified = $file.LastWriteTime
            RelativePath = $file.FullName.Replace($ProjectPath, "").TrimStart('\', '/')
            Component = "Unknown"
            Criticality = "Medium"
            Dependencies = [List[string]]::new()
            Dependents = [List[string]]::new()
            Configuration = @{}
            AnalysisStatus = "Pending"
        }
        
        # Detect component
        foreach ($component in $componentPatterns.Keys) {
            $patterns = $componentPatterns[$component]
            foreach ($pattern in $patterns) {
                if ($fileInfo.RelativePath -like "*$pattern*" -or $fileInfo.Name -like "*$pattern*") {
                    $fileInfo.Component = $component
                    break
                }
            }
            if ($fileInfo.Component -ne "Unknown") { break }
        }
        
        # Determine criticality
        if ($fileInfo.Type -in @("Dockerfile", "DockerCompose", "CONFIGFile", "JSONConfig")) {
            $fileInfo.Criticality = "High"
        }
        elseif ($fileInfo.Type -in @("PowerShellScript", "ShellScript", "BatchScript")) {
            if ($fileInfo.Name -like "*deploy*" -or $fileInfo.Name -like "*install*" -or $fileInfo.Name -like "*setup*") {
                $fileInfo.Criticality = "High"
            }
            else {
                $fileInfo.Criticality = "Medium"
            }
        }
        elseif ($fileInfo.Type -in @("CSharpCode", "JavaCode", "PythonCode", "JavaScriptCode")) {
            $fileInfo.Criticality = "High"
        }
        else {
            $fileInfo.Criticality = "Low"
        }
        
        # Categorize files
        if ($fileInfo.Type -like "*Config*") {
            $results.ConfigurationFiles.Add($fileInfo)
        }
        elseif ($fileInfo.Type -like "*Project" -or $fileInfo.Type -eq "Makefile") {
            $results.BuildFiles.Add($fileInfo)
        }
        elseif ($fileInfo.Type -in @("Dockerfile", "DockerCompose", "PowerShellScript", "ShellScript")) {
            if ($fileInfo.Name -like "*deploy*" -or $fileInfo.Name -like "*install*" -or $fileInfo.Name -like "*setup*") {
                $results.DeploymentFiles.Add($fileInfo)
            }
        }
        
        $results.Files.Add($fileInfo)
        $results.TotalSize += $file.Length
        
        # Track file types
        if (!$results.FileTypes[$fileType]) {
            $results.FileTypes[$fileType] = 0
        }
        $results.FileTypes[$fileType]++
        
        # Track components
        if (!$results.Components[$fileInfo.Component]) {
            $results.Components[$fileInfo.Component] = [List[hashtable]]::new()
        }
        $results.Components[$fileInfo.Component].Add($fileInfo)
    }
    
    $results.TotalFiles = $results.Files.Count
    $results.ComponentCount = $results.Components.Count
    
    Write-Log "INFO" "Discovery complete: $($results.TotalFiles) files, $($results.ComponentCount) components"
    Write-Log "INFO" "Configuration files: $($results.ConfigurationFiles.Count)"
    Write-Log "INFO" "Build files: $($results.BuildFiles.Count)"
    Write-Log "INFO" "Deployment files: $($results.DeploymentFiles.Count)"
    
    return $results
}

# Configuration validation function
function Start-ConfigurationValidation {
    param($ProjectFiles)
    
    Write-Log "INFO" "Starting configuration validation for $($ProjectFiles.Count) files"
    
    $configFiles = $ProjectFiles | Where-Object { $_.Type -like "*Config*" -or $_.Type -in @("PowerShellScript", "ShellScript", "BatchScript", "Dockerfile") }
    
    foreach ($file in $configFiles) {
        Write-Log "DEBUG" "Validating configuration: $($file.Name)"
        
        $configValidation = @{
            File = $file
            Issues = [List[hashtable]]::new()
            Warnings = [List[hashtable]]::new()
            Validations = [List[hashtable]]::new()
            IsValid = $true
            SeverityScore = 0
        }
        
        try {
            # Check file permissions
            if ($global:DeploymentAuditConfig.ConfigurationValidation.CheckFilePermissions) {
                $permissionCheck = Test-FilePermissions -FilePath $file.Path
                if ($permissionCheck.HasIssues) {
                    $configValidation.Issues.AddRange($permissionCheck.Issues)
                    $configValidation.IsValid = $false
                }
                $configValidation.Validations.Add($permissionCheck)
            }
            
            # Validate syntax
            if ($global:DeploymentAuditConfig.ConfigurationValidation.ValidateSyntax) {
                $syntaxValidation = Test-ConfigurationSyntax -File $file
                if ($syntaxValidation.HasErrors) {
                    $configValidation.Issues.AddRange($syntaxValidation.Errors)
                    $configValidation.IsValid = $false
                }
                $configValidation.Validations.Add($syntaxValidation)
            }
            
            # Verify paths
            if ($global:DeploymentAuditConfig.ConfigurationValidation.VerifyPaths) {
                $pathVerification = Test-ConfigurationPaths -File $file
                if ($pathVerification.HasInvalidPaths) {
                    $configValidation.Issues.AddRange($pathVerification.InvalidPaths)
                    $configValidation.Warnings.AddRange($pathVerification.Warnings)
                }
                $configValidation.Validations.Add($pathVerification)
            }
            
            # Check environment variables
            if ($global:DeploymentAuditConfig.ConfigurationValidation.CheckEnvironmentVariables) {
                $envCheck = Test-EnvironmentVariables -File $file
                if ($envCheck.MissingVariables.Count -gt 0) {
                    $configValidation.Warnings.AddRange($envCheck.MissingVariables)
                }
                $configValidation.Validations.Add($envCheck)
            }
            
            # Calculate severity score
            foreach ($issue in $configValidation.Issues) {
                $configValidation.SeverityScore += Get-SeverityScore -Severity $issue.Severity
            }
            
            if (!$configValidation.IsValid) {
                $global:DeploymentAuditResults.ConfigurationIssues.Add($configValidation)
            }
            
            $file.AnalysisStatus = "Completed"
        }
        catch {
            Write-Log "WARNING" "Failed to validate configuration $($file.Name): $_"
            $file.AnalysisStatus = "Failed"
            $configValidation.Issues.Add(@{
                Type = "ValidationError"
                Description = $_
                Severity = "High"
                Line = 0
            })
            $configValidation.IsValid = $false
            $global:DeploymentAuditResults.ConfigurationIssues.Add($configValidation)
        }
    }
    
    Write-Log "SUCCESS" "Configuration validation completed: $($global:DeploymentAuditResults.ConfigurationIssues.Count) issues found"
    
    return $global:DeploymentAuditResults.ConfigurationIssues
}

# File permissions test
function Test-FilePermissions {
    param($FilePath)
    
    $result = @{
        HasIssues = $false
        Issues = [List[hashtable]]::new()
        Details = @{
            Owner = ""
            Group = ""
            Permissions = ""
            IsWorldWritable = $false
            IsWorldReadable = $false
            IsExecutable = $false
        }
    }
    
    try {
        $fileInfo = Get-Item $FilePath
        $acl = Get-Acl $FilePath
        
        $result.Details.Owner = $acl.Owner
        $result.Details.Permissions = $acl.AccessToString
        
        # Check for world-writable permissions (Windows)
        $worldWritable = $acl.Access | Where-Object { 
            $_.IdentityReference -eq "Everyone" -and 
            $_.FileSystemRights -band [System.Security.AccessControl.FileSystemRights]::Write 
        }
        
        if ($worldWritable) {
            $result.HasIssues = $true
            $result.Issues.Add(@{
                Type = "PermissionIssue"
                Severity = "High"
                Description = "File is world-writable"
                Recommendation = "Restrict write permissions to necessary users only"
            })
            $result.Details.IsWorldWritable = $true
        }
        
        # Check if script is executable
        if ($fileInfo.Extension -in @('.ps1', '.sh', '.bat', '.cmd')) {
            # Check execution policy for PowerShell scripts
            if ($fileInfo.Extension -eq '.ps1') {
                $executionPolicy = Get-ExecutionPolicy -Scope Process
                if ($executionPolicy -eq "Restricted") {
                    $result.HasIssues = $true
                    $result.Issues.Add(@{
                        Type = "ExecutionPolicy"
                        Severity = "Medium"
                        Description = "PowerShell execution policy is Restricted"
                        Recommendation = "Set appropriate execution policy (RemoteSigned or AllSigned)"
                    })
                }
            }
            
            $result.Details.IsExecutable = $true
        }
    }
    catch {
        $result.HasIssues = $true
        $result.Issues.Add(@{
            Type = "PermissionCheckError"
            Severity = "Medium"
            Description = "Failed to check file permissions: $_"
            Recommendation = "Manually verify file permissions"
        })
    }
    
    return $result
}

# Configuration syntax validation
function Test-ConfigurationSyntax {
    param($File)
    
    $result = @{
        HasErrors = $false
        Errors = [List[hashtable]]::new()
        Warnings = [List[hashtable]]::new()
        SyntaxValid = $true
        ValidationDetails = @{}
    }
    
    try {
        switch ($file.Type) {
            "JSONConfig" {
                $content = Get-Content $file.Path -Raw -ErrorAction Stop
                try {
                    $null = $content | ConvertFrom-Json -ErrorAction Stop
                    $result.ValidationDetails.JsonValid = $true
                }
                catch {
                    $result.HasErrors = $true
                    $result.SyntaxValid = $false
                    $result.Errors.Add(@{
                        Type = "JsonSyntaxError"
                        Severity = "High"
                        Description = $_.Exception.Message
                        Line = 0
                    })
                }
            }
            
            "XMLConfig" {
                $content = Get-Content $file.Path -Raw -ErrorAction Stop
                try {
                    $xml = [xml]$content
                    $result.ValidationDetails.XmlValid = $true
                }
                catch {
                    $result.HasErrors = $true
                    $result.SyntaxValid = $false
                    $result.Errors.Add(@{
                        Type = "XmlSyntaxError"
                        Severity = "High"
                        Description = $_.Exception.Message
                        Line = 0
                    })
                }
            }
            
            "YAMLConfig" {
                # Basic YAML validation (requires additional modules for full validation)
                $content = Get-Content $file.Path -Raw -ErrorAction Stop
                if ($content.Length -gt 0) {
                    $result.ValidationDetails.YamlValid = $true
                    $result.Warnings.Add(@{
                        Type = "YamlValidation"
                        Severity = "Low"
                        Description = "Basic YAML validation only. Install PowerShell-Yaml module for full validation."
                        Line = 0
                    })
                }
            }
            
            "PowerShellScript" {
                # Test PowerShell syntax
                $scriptContent = Get-Content $file.Path -Raw -ErrorAction Stop
                try {
                    $null = [System.Management.Automation.Language.Parser]::ParseInput($scriptContent, [ref]$null, [ref]$null)
                    $result.ValidationDetails.PowerShellSyntaxValid = $true
                }
                catch {
                    $result.HasErrors = $true
                    $result.SyntaxValid = $false
                    $result.Errors.Add(@{
                        Type = "PowerShellSyntaxError"
                        Severity = "High"
                        Description = $_.Exception.Message
                        Line = $_.Exception.ErrorRecord.InvocationInfo.ScriptLineNumber
                    })
                }
            }
            
            "Dockerfile" {
                $dockerContent = Get-Content $file.Path -ErrorAction Stop
                $dockerValidation = Test-DockerfileSyntax -Content $dockerContent
                $result.HasErrors = $dockerValidation.HasErrors
                $result.Errors.AddRange($dockerValidation.Errors)
                $result.Warnings.AddRange($dockerValidation.Warnings)
                $result.ValidationDetails = $dockerValidation.Details
            }
        }
    }
    catch {
        $result.HasErrors = $true
        $result.Errors.Add(@{
            Type = "ValidationError"
            Severity = "High"
            Description = "Failed to validate syntax: $_"
            Line = 0
        })
    }
    
    return $result
}

# Dockerfile syntax validation
function Test-DockerfileSyntax {
    param($Content)
    
    $result = @{
        HasErrors = $false
        Errors = [List[hashtable]]::new()
        Warnings = [List[hashtable]]::new()
        Details = @{
            Instructions = [List[string]]::new()
            BaseImages = [List[string]]::new()
            ExposedPorts = [List[int]]::new()
            HasHealthCheck = $false
            HasUserInstruction = $false
        }
    }
    
    try {
        $lines = $content | Where-Object { $_ -and $_.Trim() -ne "" }
        $lineNumber = 0
        
        foreach ($line in $lines) {
            $lineNumber++
            $trimmed = $line.Trim()
            
            # Skip comments
            if ($trimmed.StartsWith("#")) { continue }
            
            # Extract instruction
            $instruction = $trimmed.Split()[0].ToUpper()
            $result.Details.Instructions.Add($instruction)
            
            switch ($instruction) {
                "FROM" {
                    $baseImage = $trimmed.Substring(4).Trim()
                    $result.Details.BaseImages.Add($baseImage)
                    
                    # Check for latest tag (not recommended for production)
                    if ($baseImage -like "*:latest" -or $baseImage -notlike "*:*") {
                        $result.Warnings.Add(@{
                            Type = "DockerfileWarning"
                            Severity = "Medium"
                            Description = "Using 'latest' tag or no tag specified for base image: $baseImage"
                            Recommendation = "Use specific version tags for reproducible builds"
                            Line = $lineNumber
                        })
                    }
                }
                
                "EXPOSE" {
                    $port = $trimmed.Substring(6).Trim()
                    if ($port -match '(\d+)') {
                        $result.Details.ExposedPorts.Add([int]$matches[1])
                    }
                }
                
                "HEALTHCHECK" {
                    $result.Details.HasHealthCheck = $true
                }
                
                "USER" {
                    $result.Details.HasUserInstruction = $true
                }
                
                "RUN" {
                    $command = $trimmed.Substring(3).Trim()
                    # Check for common security issues
                    if ($command -like "*wget*" -or $command -like "*curl*") {
                        $result.Warnings.Add(@{
                            Type = "DockerfileSecurityWarning"
                            Severity = "Low"
                            Description = "Using wget/curl in RUN instruction"
                            Recommendation = "Consider using multi-stage builds and copying files instead"
                            Line = $lineNumber
                        })
                    }
                }
            }
        }
        
        # Validate Dockerfile structure
        if ($result.Details.Instructions.Count -eq 0) {
            $result.HasErrors = $true
            $result.Errors.Add(@{
                Type = "DockerfileError"
                Severity = "High"
                Description = "No valid instructions found in Dockerfile"
                Line = 0
            })
        }
        elseif ($result.Details.Instructions[0] -ne "FROM") {
            $result.HasErrors = $true
            $result.Errors.Add(@{
                Type = "DockerfileError"
                Severity = "High"
                Description = "Dockerfile must start with FROM instruction"
                Line = 0
            })
        }
        
        # Check for recommended practices
        if (!$result.Details.HasHealthCheck) {
            $result.Warnings.Add(@{
                Type = "DockerfileBestPractice"
                Severity = "Low"
                Description = "No HEALTHCHECK instruction found"
                Recommendation = "Add HEALTHCHECK for container monitoring"
                Line = 0
            })
        }
        
        if (!$result.Details.HasUserInstruction) {
            $result.Warnings.Add(@{
                Type = "DockerfileSecurityBestPractice"
                Severity = "Medium"
                Description = "No USER instruction found - container will run as root"
                Recommendation = "Add USER instruction to run as non-root user"
                Line = 0
            })
        }
    }
    catch {
        $result.HasErrors = $true
        $result.Errors.Add(@{
            Type = "DockerfileValidationError"
            Severity = "High"
            Description = "Failed to validate Dockerfile: $_"
            Line = 0
        })
    }
    
    return $result
}

# Configuration path verification
function Test-ConfigurationPaths {
    param($File)
    
    $result = @{
        HasInvalidPaths = $false
        InvalidPaths = [List[hashtable]]::new()
        Warnings = [List[hashtable]]::new()
        VerifiedPaths = [List[string]]::new()
    }
    
    try {
        $content = Get-Content $file.Path -Raw -ErrorAction Stop
        
        # Extract paths based on file type
        switch ($file.Type) {
            "JSONConfig" {
                $json = $content | ConvertFrom-Json
                $paths = Extract-PathsFromJson -JsonObject $json -ParentPath ""
            }
            "XMLConfig" {
                $xml = [xml]$content
                $paths = Extract-PathsFromXml -XmlNode $xml -ParentPath ""
            }
            "PowerShellScript" {
                $paths = Extract-PathsFromPowerShell -Content $content
            }
            default {
                $paths = Extract-GenericPaths -Content $content
            }
        }
        
        # Verify each path
        foreach ($pathInfo in $paths) {
            $path = $pathInfo.Path
            $pathType = $pathInfo.Type
            
            # Skip URLs and environment variables
            if ($path -match '^https?://' -or $path -match '^%\w+%$') {
                continue
            }
            
            # Resolve relative paths
            if (![System.IO.Path]::IsPathRooted($path)) {
                $baseDir = Split-Path $file.Path -Parent
                $fullPath = Join-Path $baseDir $path
            }
            else {
                $fullPath = $path
            }
            
            # Check if path exists
            $pathExists = $false
            switch ($pathType) {
                "File" { $pathExists = Test-Path $fullPath -PathType Leaf }
                "Directory" { $pathExists = Test-Path $fullPath -PathType Container }
                "Any" { $pathExists = Test-Path $fullPath }
                default { $pathExists = Test-Path $fullPath }
            }
            
            if (!$pathExists) {
                $result.HasInvalidPaths = $true
                $result.InvalidPaths.Add(@{
                    Type = "InvalidPath"
                    Severity = if ($file.Criticality -eq "High") { "High" } else { "Medium" }
                    Description = "Path does not exist: $path"
                    FullPath = $fullPath
                    PathType = $pathType
                    Line = $pathInfo.Line
                    Recommendation = "Create the missing path or update configuration"
                })
            }
            else {
                $result.VerifiedPaths.Add($fullPath)
            }
        }
    }
    catch {
        $result.HasInvalidPaths = $true
        $result.InvalidPaths.Add(@{
            Type = "PathValidationError"
            Severity = "Medium"
            Description = "Failed to validate paths: $_"
            Line = 0
        })
    }
    
    return $result
}

# Environment variable checking
function Test-EnvironmentVariables {
    param($File)
    
    $result = @{
        MissingVariables = [List[hashtable]]::new()
        PresentVariables = [List[string]]::new()
        InvalidVariables = [List[hashtable]]::new()
    }
    
    try {
        $content = Get-Content $file.Path -Raw -ErrorAction Stop
        
        # Extract environment variable references
        $envPatterns = @(
            '\$env:(\w+)',  # PowerShell
            '%(\w+)%',      # Windows batch/cmd
            '\$\{(\w+)\}',  # Shell/Generic
            '\$(\w+)'       # Shell/Generic
        )
        
        $foundVariables = [System.Collections.Generic.HashSet[string]]::new()
        
        foreach ($pattern in $envPatterns) {
            $matches = [regex]::Matches($content, $pattern)
            foreach ($match in $matches) {
                $varName = $match.Groups[1].Value
                if ($foundVariables.Add($varName)) {
                    # Check if environment variable exists
                    $envValue = [System.Environment]::GetEnvironmentVariable($varName)
                    if ([string]::IsNullOrEmpty($envValue)) {
                        $result.MissingVariables.Add(@{
                            Name = $varName
                            Severity = if ($file.Criticality -eq "High") { "High" } else { "Medium" }
                            Description = "Environment variable is not set"
                            Recommendation = "Set the environment variable or provide default value"
                        })
                    }
                    else {
                        $result.PresentVariables.Add($varName)
                    }
                }
            }
        }
    }
    catch {
        $result.InvalidVariables.Add(@{
            Name = "EnvironmentVariableCheck"
            Severity = "Low"
            Description = "Failed to check environment variables: $_"
        })
    }
    
    return $result
}

# Security assessment function
function Start-SecurityAssessment {
    param($ProjectFiles)
    
    Write-Log "INFO" "Starting security assessment for $($ProjectFiles.Count) files"
    
    $securityIssues = [List[hashtable]]::new()
    
    # Check for secrets and sensitive data
    $secretPatterns = @{
        "APIKey" = @(
            @{
                Pattern = "(?i)(api[_-]?key|apikey)\s*[:=]\s*([`"`'])([a-z0-9]{16,})\2"
                Severity = "Critical"
                Description = "Potential API key exposed"
            }
        )
        "Password" = @(
            @{
                Pattern = "(?i)(password|passwd|pwd)\s*[:=]\s*([`"`'])([^`"`']{6,})\2"
                Severity = "Critical"
                Description = "Potential password exposed"
            }
        )
        "ConnectionString" = @(
            @{
                Pattern = "(?i)(connection[_-]?string|conn[_-]?str)\s*[:=]\s*([`"`'])([^`"`']{10,})\2"
                Severity = "High"
                Description = "Potential connection string exposed"
            }
        )
        "PrivateKey" = @(
            @{
                Pattern = '-----BEGIN (RSA |EC |DSA |OPENSSH )?PRIVATE KEY-----'
                Severity = "Critical"
                Description = "Private key detected"
            }
        )
        "Token" = @(
            @{
                Pattern = "(?i)(token|access[_-]?token|auth[_-]?token)\s*[:=]\s*([`"`'])([a-z0-9]{20,})\2"
                Severity = "High"
                Description = "Potential authentication token exposed"
            }
        )
        "SecretKey" = @(
            @{
                Pattern = "(?i)(secret|secret[_-]?key)\s*[:=]\s*([`"`'])([a-z0-9]{16,})\2"
                Severity = "Critical"
                Description = "Potential secret key exposed"
            }
        )
    }
    
    foreach ($file in $ProjectFiles) {
        if ($file.Size -gt 5MB) { continue }  # Skip large files
        
        try {
            $content = Get-Content $file.Path -Raw -ErrorAction Stop
            
            $fileSecurity = @{
                File = $file
                Issues = [List[hashtable]]::new()
                SecretsFound = [List[hashtable]]::new()
                PermissionIssues = [List[hashtable]]::new()
                AuthenticationIssues = [List[hashtable]]::new()
                AuthorizationIssues = [List[hashtable]]::new()
                LoggingIssues = [List[hashtable]]::new()
                RiskScore = 0
            }
            
            # Scan for secrets
            foreach ($secretType in $secretPatterns.Keys) {
                $patterns = $secretPatterns[$secretType]
                foreach ($patternInfo in $patterns) {
                    $matches = [regex]::Matches($content, $patternInfo.Pattern)
                    foreach ($match in $matches) {
                        $fileSecurity.SecretsFound.Add(@{
                            Type = $secretType
                            Severity = $patternInfo.Severity
                            Description = $patternInfo.Description
                            Pattern = $patternInfo.Pattern
                            Line = $match.Index
                            Value = $match.Value.Substring(0, [Math]::Min(50, $match.Value.Length)) + "..."
                        })
                        
                        $fileSecurity.Issues.Add(@{
                            Type = "SecretExposure"
                            Severity = $patternInfo.Severity
                            Description = $patternInfo.Description
                            Line = $match.Index
                            Recommendation = "Remove secrets from code and use secure vault/storage"
                        })
                    }
                }
            }
            
            # Check file permissions
            if ($global:DeploymentAuditConfig.SecurityChecking.CheckFilePermissions) {
                $permissionCheck = Test-SecurityFilePermissions -FilePath $file.Path
                if ($permissionCheck.HasSecurityIssues) {
                    $fileSecurity.PermissionIssues.AddRange($permissionCheck.SecurityIssues)
                    $fileSecurity.Issues.AddRange($permissionCheck.SecurityIssues)
                }
            }
            
            # Validate authentication mechanisms
            if ($global:DeploymentAuditConfig.SecurityChecking.VerifyAuthentication) {
                $authValidation = Test-AuthenticationSecurity -File $file -Content $content
                if ($authValidation.HasIssues) {
                    $fileSecurity.AuthenticationIssues.AddRange($authValidation.Issues)
                    $fileSecurity.Issues.AddRange($authValidation.Issues)
                }
            }
            
            # Check authorization implementation
            if ($global:DeploymentAuditConfig.SecurityChecking.CheckAuthorization) {
                $authzValidation = Test-AuthorizationSecurity -File $file -Content $content
                if ($authzValidation.HasIssues) {
                    $fileSecurity.AuthorizationIssues.AddRange($authzValidation.Issues)
                    $fileSecurity.Issues.AddRange($authzValidation.Issues)
                }
            }
            
            # Audit logging implementation
            if ($global:DeploymentAuditConfig.SecurityChecking.AuditLogging) {
                $loggingValidation = Test-LoggingImplementation -File $file -Content $content
                if ($loggingValidation.HasIssues) {
                    $fileSecurity.LoggingIssues.AddRange($loggingValidation.Issues)
                    $fileSecurity.Issues.AddRange($loggingValidation.Issues)
                }
            }
            
            # Calculate risk score
            foreach ($issue in $fileSecurity.Issues) {
                $fileSecurity.RiskScore += Get-SeverityScore -Severity $issue.Severity
            }
            
            if ($fileSecurity.Issues.Count -gt 0) {
                $global:DeploymentAuditResults.SecurityGaps.Add($fileSecurity)
            }
            
            $file.AnalysisStatus = "Completed"
        }
        catch {
            Write-Log "WARNING" "Failed to assess security for $($file.Name): $_"
            $file.AnalysisStatus = "Failed"
        }
    }
    
    Write-Log "SUCCESS" "Security assessment completed: $($global:DeploymentAuditResults.SecurityGaps.Count) files with security issues"
    
    return $global:DeploymentAuditResults.SecurityGaps
}

# Security file permissions test
function Test-SecurityFilePermissions {
    param($FilePath)
    
    $result = @{
        HasSecurityIssues = $false
        SecurityIssues = [List[hashtable]]::new()
        Details = @{
            Owner = ""
            Group = ""
            Permissions = ""
            IsWorldWritable = $false
            IsWorldReadable = $false
            IsSensitiveFile = $false
        }
    }
    
    try {
        $fileInfo = Get-Item $FilePath
        $acl = Get-Acl $FilePath
        
        $result.Details.Owner = $acl.Owner
        $result.Details.Permissions = $acl.AccessToString
        
        # Check if file contains sensitive data
        $sensitiveExtensions = @('.key', '.pem', '.pfx', '.p12', '.crt', '.cert')
        if ($fileInfo.Extension -in $sensitiveExtensions) {
            $result.Details.IsSensitiveFile = $true
            
            # Sensitive files should not be world-readable
            $worldReadable = $acl.Access | Where-Object { 
                $_.IdentityReference -eq "Everyone" -and 
                $_.FileSystemRights -band [System.Security.AccessControl.FileSystemRights]::Read 
            }
            
            if ($worldReadable) {
                $result.HasSecurityIssues = $true
                $result.SecurityIssues.Add(@{
                    Type = "SensitiveFilePermission"
                    Severity = "Critical"
                    Description = "Sensitive file is world-readable"
                    Recommendation = "Restrict read permissions to necessary users only"
                })
            }
        }
        
        # Check for world-writable permissions
        $worldWritable = $acl.Access | Where-Object { 
            $_.IdentityReference -eq "Everyone" -and 
            $_.FileSystemRights -band [System.Security.AccessControl.FileSystemRights]::Write 
        }
        
        if ($worldWritable) {
            $result.HasSecurityIssues = $true
            $result.SecurityIssues.Add(@{
                Type = "WorldWritableFile"
                Severity = "High"
                Description = "File is world-writable"
                Recommendation = "Restrict write permissions to necessary users only"
            })
            $result.Details.IsWorldWritable = $true
        }
        
        # Check for files owned by privileged users
        $privilegedUsers = @("Administrator", "SYSTEM", "root")
        $fileOwner = $acl.Owner.Split('\')[-1]
        if ($fileOwner -in $privilegedUsers -and $fileInfo.Extension -in @('.ps1', '.sh', '.bat', '.cmd')) {
            $result.Warnings.Add(@{
                Type = "PrivilegedOwner"
                Severity = "Low"
                Description = "Script file owned by privileged user: $fileOwner"
                Recommendation = "Consider using dedicated service account"
            })
        }
    }
    catch {
        $result.HasSecurityIssues = $true
        $result.SecurityIssues.Add(@{
            Type = "PermissionCheckError"
            Severity = "Medium"
            Description = "Failed to check file permissions: $_"
            Recommendation = "Manually verify file permissions"
        })
    }
    
    return $result
}

# Authentication security test
function Test-AuthenticationSecurity {
    param($File, $Content)
    
    $result = @{
        HasIssues = $false
        Issues = [List[hashtable]]::new()
        Warnings = [List[hashtable]]::new()
        Details = @{
            HasAuthentication = $false
            AuthMethod = "Unknown"
            UsesStrongAuth = $false
            HasPasswordPolicy = $false
            HasAccountLockout = $false
        }
    }
    
    try {
        # Check for authentication implementation
        $authPatterns = @(
            '(?i)(auth|authentication|login|signin)',
            '(?i)(password|credential|token|jwt|oauth)'
        )
        
        foreach ($pattern in $authPatterns) {
            if ($content -match $pattern) {
                $result.Details.HasAuthentication = $true
                break
            }
        }
        
        if ($result.Details.HasAuthentication) {
            # Check for weak authentication methods
            $weakAuthPatterns = @{
                "BasicAuth" = @{
                    Pattern = '(?i)(basic\s+auth|httpbasic|basicauthentication)'
                    Severity = "High"
                    Description = "Basic authentication detected"
                    Recommendation = "Use stronger authentication methods (OAuth2, JWT)"
                }
                "HardcodedCredentials" = @{
                    Pattern = "(?i)(password|credential)\s*[:=]\s*([`"`'])([^`"`']{3,})\2"
                    Severity = "Critical"
                    Description = "Potentially hardcoded credentials"
                    Recommendation = "Use secure credential storage (Azure Key Vault, AWS Secrets Manager)"
                }
                "WeakPasswordValidation" = @{
                    Pattern = '(?i)(password.*length.*<|min.*length.*[0-3])'
                    Severity = "Medium"
                    Description = "Weak password length requirements"
                    Recommendation = "Enforce minimum password length of 12 characters"
                }
            }
            
            foreach ($weakAuth in $weakAuthPatterns.Keys) {
                $patternInfo = $weakAuthPatterns[$weakAuth]
                $matches = [regex]::Matches($content, $patternInfo.Pattern)
                
                if ($matches.Count -gt 0) {
                    $result.HasIssues = $true
                    $result.Issues.Add(@{
                        Type = $weakAuth
                        Severity = $patternInfo.Severity
                        Description = $patternInfo.Description
                        Line = if ($matches.Count -gt 0) { $matches[0].Index } else { 0 }
                        Recommendation = $patternInfo.Recommendation
                    })
                }
            }
            
            # Check for security best practices
            if ($content -notmatch '(?i)(multifactor|mfa|2fa|twofactor)') {
                $result.Warnings.Add(@{
                    Type = "NoMFA"
                    Severity = "Medium"
                    Description = "No multi-factor authentication detected"
                    Recommendation = "Implement multi-factor authentication"
                })
            }
            
            if ($content -notmatch '(?i)(lockout|attempt.*limit|brute.*force)') {
                $result.Warnings.Add(@{
                    Type = "NoAccountLockout"
                    Severity = "Medium"
                    Description = "No account lockout policy detected"
                    Recommendation = "Implement account lockout after failed attempts"
                })
            }
        }
    }
    catch {
        $result.HasIssues = $true
        $result.Issues.Add(@{
            Type = "AuthenticationCheckError"
            Severity = "Low"
            Description = "Failed to check authentication security: $_"
            Recommendation = "Manually review authentication implementation"
        })
    }
    
    return $result
}

# Authorization security test
function Test-AuthorizationSecurity {
    param($File, $Content)
    
    $result = @{
        HasIssues = $false
        Issues = [List[hashtable]]::new()
        Warnings = [List[hashtable]]::new()
        Details = @{
            HasAuthorization = $false
            AuthZMethod = "Unknown"
            UsesRBAC = $false
            HasPermissionChecks = $false
            HasRoleChecks = $false
        }
    }
    
    try {
        # Check for authorization implementation
        $authZPatterns = @(
            '(?i)(authz|authorization|authorize|permission|role|claim|policy)'
        )
        
        foreach ($pattern in $authZPatterns) {
            if ($content -match $pattern) {
                $result.Details.HasAuthorization = $true
                break
            }
        }
        
        if ($result.Details.HasAuthorization) {
            # Check for RBAC implementation
            if ($content -match '(?i)(role.*based|rbac)') {
                $result.Details.UsesRBAC = $true
            }
            
            # Check for permission checks
            if ($content -match '(?i)(check.*permission|has.*permission|require.*permission)') {
                $result.Details.HasPermissionChecks = $true
            }
            
            # Check for role checks
            if ($content -match '(?i)(check.*role|has.*role|require.*role|is.*in.*role)') {
                $result.Details.HasRoleChecks = $true
            }
            
            # Check for common authorization issues
            if ($content -match '(?i)(if.*user.*admin|==.*"admin"|==.*"user")') {
                $result.HasIssues = $true
                $result.Issues.Add(@{
                    Type = "HardcodedRoleCheck"
                    Severity = "High"
                    Description = "Hardcoded role comparison detected"
                    Recommendation = "Use proper authorization framework and avoid hardcoded role checks"
                })
            }
            
            if ($content -match '(?i)(user.*id.*==|user.*name.*==)') {
                $result.HasIssues = $true
                $result.Issues.Add(@{
                    Type = "InsecureDirectObjectReference"
                    Severity = "High"
                    Description = "Potential insecure direct object reference"
                    Recommendation = "Implement proper authorization checks for resource access"
                })
            }
            
            # Warnings for missing best practices
            if (!$result.Details.HasPermissionChecks -and !$result.Details.HasRoleChecks) {
                $result.Warnings.Add(@{
                    Type = "NoExplicitAuthorization"
                    Severity = "Medium"
                    Description = "No explicit authorization checks detected"
                    Recommendation = "Implement explicit authorization checks"
                })
            }
        }
        else {
            $result.Warnings.Add(@{
                Type = "NoAuthorization"
                Severity = "High"
                Description = "No authorization implementation detected"
                Recommendation = "Implement proper authorization controls"
            })
        }
    }
    catch {
        $result.HasIssues = $true
        $result.Issues.Add(@{
            Type = "AuthorizationCheckError"
            Severity = "Low"
            Description = "Failed to check authorization security: $_"
            Recommendation = "Manually review authorization implementation"
        })
    }
    
    return $result
}

# Logging implementation test
function Test-LoggingImplementation {
    param($File, $Content)
    
    $result = @{
        HasIssues = $false
        Issues = [List[hashtable]]::new()
        Warnings = [List[hashtable]]::new()
        Details = @{
            HasLogging = $false
            LogLevelUsage = @{
                Debug = $false
                Info = $false
                Warning = $false
                Error = $false
                Critical = $false
            }
            SensitiveDataLogging = $false
            LogRotationConfigured = $false
        }
    }
    
    try {
        # Check for logging implementation
        $loggingPatterns = @(
            '(?i)(log|logging|logger|write.*log)',
            '(?i)(console\.log|print|echo|write.*host)'
        )
        
        foreach ($pattern in $loggingPatterns) {
            if ($content -match $pattern) {
                $result.Details.HasLogging = $true
                break
            }
        }
        
        if ($result.Details.HasLogging) {
            # Check log level usage
            if ($content -match '(?i)(debug|trace)') {
                $result.Details.LogLevelUsage.Debug = $true
            }
            if ($content -match '(?i)(info|information)') {
                $result.Details.LogLevelUsage.Info = $true
            }
            if ($content -match '(?i)(warn|warning)') {
                $result.Details.LogLevelUsage.Warning = $true
            }
            if ($content -match '(?i)(error|exception)') {
                $result.Details.LogLevelUsage.Error = $true
            }
            if ($content -match '(?i)(critical|fatal)') {
                $result.Details.LogLevelUsage.Critical = $true
            }
            
            # Check for sensitive data logging
            $sensitivePatterns = @(
                '(?i)(password|credential|secret|key|token)'
            )
            
            foreach ($pattern in $sensitivePatterns) {
                if ($content -match $pattern) {
                    $result.Details.SensitiveDataLogging = $true
                    $result.HasIssues = $true
                    $result.Issues.Add(@{
                        Type = "SensitiveDataLogging"
                        Severity = "High"
                        Description = "Potential logging of sensitive data"
                        Recommendation = "Avoid logging sensitive information (passwords, keys, tokens)"
                    })
                    break
                }
            }
            
            # Check for log rotation or retention
            if ($content -notmatch '(?i)(rotate|retention|archive|cleanup)') {
                $result.Warnings.Add(@{
                    Type = "NoLogRotation"
                    Severity = "Low"
                    Description = "No log rotation or retention policy detected"
                    Recommendation = "Implement log rotation to prevent disk space issues"
                })
            }
            
            # Check for security event logging
            if ($content -notmatch '(?i)(security|auth|login|logout|access)') {
                $result.Warnings.Add(@{
                    Type = "NoSecurityLogging"
                    Severity = "Medium"
                    Description = "No security event logging detected"
                    Recommendation = "Implement logging for security events (logins, access attempts, etc.)"
                })
            }
        }
        else {
            $result.Warnings.Add(@{
                Type = "NoLogging"
                Severity = "Medium"
                Description = "No logging implementation detected"
                Recommendation = "Implement comprehensive logging for monitoring and debugging"
            })
        }
    }
    catch {
        $result.HasIssues = $true
        $result.Issues.Add(@{
            Type = "LoggingCheckError"
            Severity = "Low"
            Description = "Failed to check logging implementation: $_"
            Recommendation = "Manually review logging implementation"
        })
    }
    
    return $result
}

# Dependency verification function
function Start-DependencyVerification {
    param($ProjectPath)
    
    Write-Log "INFO" "Starting dependency verification for project: $ProjectPath"
    
    $dependencyIssues = [List[hashtable]]::new()
    
    # Find dependency files
    $dependencyFiles = @(
        @{ Name = "package.json"; Type = "NodeJS" }
        @{ Name = "requirements.txt"; Type = "Python" }
        @{ Name = "Pipfile"; Type = "Python" }
        @{ Name = "pyproject.toml"; Type = "Python" }
        @{ Name = "Gemfile"; Type = "Ruby" }
        @{ Name = "pom.xml"; Type = "Maven" }
        @{ Name = "build.gradle"; Type = "Gradle" }
        @{ Name = "Cargo.toml"; Type = "Rust" }
        @{ Name = "go.mod"; Type = "Go" }
        @{ Name = "composer.json"; Type = "PHP" }
        @{ Name = "Cartfile"; Type = "Carthage" }
        @{ Name = "Podfile"; Type = "CocoaPods" }
    )
    
    foreach ($depFile in $dependencyFiles) {
        $filePath = Join-Path $ProjectPath $depFile.Name
        
        if (Test-Path $filePath) {
            Write-Log "DEBUG" "Verifying dependencies in: $($depFile.Name)"
            
            $depVerification = @{
                File = @{
                    Name = $depFile.Name
                    Path = $filePath
                    Type = $depFile.Type
                }
                Issues = [List[hashtable]]::new()
                MissingDependencies = [List[string]]::new()
                VersionConflicts = [List[hashtable]]::new()
                VulnerableDependencies = [List[hashtable]]::new()
                LicenseIssues = [List[hashtable]]::new()
                IsValid = $true
                RiskScore = 0
            }
            
            try {
                $content = Get-Content $filePath -Raw -ErrorAction Stop
                
                # Parse dependencies based on file type
                switch ($depFile.Type) {
                    "NodeJS" {
                        $dependencies = Parse-NodeJSDependencies -Content $content
                    }
                    "Python" {
                        $dependencies = Parse-PythonDependencies -Content $content
                    }
                    "Maven" {
                        $dependencies = Parse-MavenDependencies -Content $content
                    }
                    "Gradle" {
                        $dependencies = Parse-GradleDependencies -Content $content
                    }
                    default {
                        $dependencies = Parse-GenericDependencies -Content $content
                    }
                }
                
                # Check for version conflicts
                if ($global:DeploymentAuditConfig.DependencyVerification.CheckVersionCompatibility) {
                    $versionConflicts = Test-VersionCompatibility -Dependencies $dependencies
                    $depVerification.VersionConflicts.AddRange($versionConflicts)
                    $depVerification.Issues.AddRange($versionConflicts)
                }
                
                # Scan for vulnerabilities
                if ($global:DeploymentAuditConfig.DependencyVerification.ScanForVulnerabilities) {
                    $vulnerableDeps = Test-DependencyVulnerabilities -Dependencies $dependencies -FileType $depFile.Type
                    $depVerification.VulnerableDependencies.AddRange($vulnerableDeps)
                    $depVerification.Issues.AddRange($vulnerableDeps)
                }
                
                # Validate licenses
                if ($global:DeploymentAuditConfig.DependencyVerification.ValidateLicenses) {
                    $licenseIssues = Test-DependencyLicenses -Dependencies $dependencies
                    $depVerification.LicenseIssues.AddRange($licenseIssues)
                    $depVerification.Issues.AddRange($licenseIssues)
                }
                
                # Check dependency integrity
                if ($global:DeploymentAuditConfig.DependencyVerification.VerifyIntegrity) {
                    $integrityIssues = Test-DependencyIntegrity -Dependencies $dependencies -ProjectPath $ProjectPath
                    $depVerification.Issues.AddRange($integrityIssues)
                }
                
                # Calculate risk score
                foreach ($issue in $depVerification.Issues) {
                    $depVerification.RiskScore += Get-SeverityScore -Severity $issue.Severity
                }
                
                if ($depVerification.Issues.Count -gt 0) {
                    $dependencyIssues.Add($depVerification)
                }
            }
            catch {
                Write-Log "WARNING" "Failed to verify dependencies in $($depFile.Name): $_"
                $depVerification.IsValid = $false
                $depVerification.Issues.Add(@{
                    Type = "DependencyVerificationError"
                    Severity = "Medium"
                    Description = "Failed to verify dependencies: $_"
                })
                $dependencyIssues.Add($depVerification)
            }
        }
    }
    
    $global:DeploymentAuditResults.MissingDependencies = $dependencyIssues
    
    Write-Log "SUCCESS" "Dependency verification completed: $($dependencyIssues.Count) dependency files with issues"
    
    return $dependencyIssues
}

# Node.js dependency parsing
function Parse-NodeJSDependencies {
    param($Content)
    
    $dependencies = [List[hashtable]]::new()
    
    try {
        $json = $content | ConvertFrom-Json
        
        # Extract dependencies
        if ($json.dependencies) {
            foreach ($dep in $json.dependencies.PSObject.Properties) {
                $dependencies.Add(@{
                    Name = $dep.Name
                    Version = $dep.Value
                    Type = "Dependency"
                })
            }
        }
        
        # Extract dev dependencies
        if ($json.devDependencies) {
            foreach ($dep in $json.devDependencies.PSObject.Properties) {
                $dependencies.Add(@{
                    Name = $dep.Name
                    Version = $dep.Value
                    Type = "DevDependency"
                })
            }
        }
        
        # Extract peer dependencies
        if ($json.peerDependencies) {
            foreach ($dep in $json.peerDependencies.PSObject.Properties) {
                $dependencies.Add(@{
                    Name = $dep.Name
                    Version = $dep.Value
                    Type = "PeerDependency"
                })
            }
        }
    }
    catch {
        Write-Log "WARNING" "Failed to parse Node.js dependencies: $_"
    }
    
    return $dependencies
}

# Python dependency parsing
function Parse-PythonDependencies {
    param($Content)
    
    $dependencies = [List[hashtable]]::new()
    
    try {
        $lines = $content -split "`n"
        
        foreach ($line in $lines) {
            $trimmed = $line.Trim()
            
            # Skip comments and empty lines
            if ($trimmed -eq "" -or $trimmed.StartsWith("#")) { continue }
            
            # Parse requirement (basic parsing)
            if ($trimmed -match '^([a-zA-Z0-9_-]+)\s*([=<>!~]+)\s*([a-zA-Z0-9.]+)') {
                $dependencies.Add(@{
                    Name = $matches[1]
                    Version = $matches[3]
                    Operator = $matches[2]
                    Type = "Dependency"
                })
            }
            elseif ($trimmed -match '^([a-zA-Z0-9_-]+)') {
                # Dependency without version
                $dependencies.Add(@{
                    Name = $matches[1]
                    Version = "Any"
                    Type = "Dependency"
                })
            }
        }
    }
    catch {
        Write-Log "WARNING" "Failed to parse Python dependencies: $_"
    }
    
    return $dependencies
}

# Severity score calculation
function Get-SeverityScore {
    param($Severity)
    
    switch ($Severity) {
        "Critical" { return 10 }
        "High" { return 7 }
        "Medium" { return 4 }
        "Low" { return 1 }
        default { return 0 }
    }
}

# Calculate deployment readiness
function Calculate-DeploymentReadiness {
    
    Write-Log "INFO" "Calculating deployment readiness..."
    
    $totalIssues = 0
    $totalRiskScore = 0
    $criticalIssues = 0
    $highIssues = 0
    $mediumIssues = 0
    $lowIssues = 0
    
    # Count issues by severity
    foreach ($issueCategory in @($global:DeploymentAuditResults.ConfigurationIssues, 
                                 $global:DeploymentAuditResults.SecurityGaps, 
                                 $global:DeploymentAuditResults.MissingDependencies)) {
        foreach ($issue in $issueCategory) {
            $totalIssues++
            $totalRiskScore += $issue.RiskScore
            
            foreach ($subIssue in $issue.Issues) {
                switch ($subIssue.Severity) {
                    "Critical" { $criticalIssues++ }
                    "High" { $highIssues++ }
                    "Medium" { $mediumIssues++ }
                    "Low" { $lowIssues++ }
                }
            }
        }
    }
    
    # Update risk assessment
    $global:DeploymentAuditResults.RiskAssessment.OverallRisk = Get-RiskLevel -Score $totalRiskScore
    $global:DeploymentAuditResults.RiskAssessment.RiskScore = $totalRiskScore
    $global:DeploymentAuditResults.RiskAssessment.CriticalIssues = $criticalIssues
    $global:DeploymentAuditResults.RiskAssessment.HighIssues = $highIssues
    $global:DeploymentAuditResults.RiskAssessment.MediumIssues = $mediumIssues
    $global:DeploymentAuditResults.RiskAssessment.LowIssues = $lowIssues
    
    # Determine readiness status
    if ($criticalIssues -eq 0 -and $highIssues -eq 0) {
        $global:DeploymentAuditResults.DeploymentReadiness.Status = "Ready"
    }
    elseif ($criticalIssues -eq 0 -and $highIssues -le 3) {
        $global:DeploymentAuditResults.DeploymentReadiness.Status = "ReadyWithWarnings"
    }
    elseif ($criticalIssues -eq 0) {
        $global:DeploymentAuditResults.DeploymentReadiness.Status = "NotReady"
    }
    else {
        $global:DeploymentAuditResults.DeploymentReadiness.Status = "Blocked"
    }
    
    # Generate recommendations
    if ($criticalIssues -gt 0) {
        $global:DeploymentAuditResults.DeploymentReadiness.Blockers.Add("$criticalIssues critical issues must be resolved")
    }
    
    if ($highIssues -gt 0) {
        $global:DeploymentAuditResults.DeploymentReadiness.Warnings.Add("$highIssues high-severity issues should be addressed")
    }
    
    if ($mediumIssues -gt 0) {
        $global:DeploymentAuditResults.DeploymentReadiness.Recommendations.Add("Review $mediumIssues medium-severity issues")
    }
    
    Write-Log "INFO" "Deployment readiness: $($global:DeploymentAuditResults.DeploymentReadiness.Status)"
    Write-Log "INFO" "Risk level: $($global:DeploymentAuditResults.RiskAssessment.OverallRisk)"
    Write-Log "INFO" "Total issues: $totalIssues (Critical: $criticalIssues, High: $highIssues, Medium: $mediumIssues, Low: $lowIssues)"
}

# Risk level determination
function Get-RiskLevel {
    param($Score)
    
    if ($score -ge 50) { return "Critical" }
    elseif ($score -ge 30) { return "High" }
    elseif ($score -ge 15) { return "Medium" }
    elseif ($score -gt 0) { return "Low" }
    else { return "Minimal" }
}

# Compliance checking function
function Start-ComplianceChecking {
    param($ProjectFiles)
    
    Write-Log "INFO" "Starting compliance checking for $($ProjectFiles.Count) files"
    
    $complianceResults = @{
        Frameworks = @()
        Violations = @()
        ComplianceScore = 0
    }
    
    # Check for common compliance frameworks
    $frameworks = @(
        @{ Name = "NIST"; Patterns = @("nist", "800-53", "cybersecurity") },
        @{ Name = "ISO27001"; Patterns = @("iso27001", "information security") },
        @{ Name = "HIPAA"; Patterns = @("hipaa", "health", "phi") },
        @{ Name = "GDPR"; Patterns = @("gdpr", "privacy", "eu") },
        @{ Name = "PCI-DSS"; Patterns = @("pci", "payment", "card") }
    )
    
    foreach ($file in $ProjectFiles) {
        if ($file.Length -gt 10MB) { continue }  # Skip large files
        
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if (!$content) { continue }
        
        foreach ($framework in $frameworks) {
            foreach ($pattern in $framework.Patterns) {
                if ($content -imatch $pattern) {
                    $complianceResults.Frameworks += $framework.Name
                    Write-Log "DEBUG" "Found $($framework.Name) reference in $($file.Name)"
                    break
                }
            }
        }
    }
    
    # Remove duplicates and calculate score
    $complianceResults.Frameworks = $complianceResults.Frameworks | Sort-Object -Unique
    $complianceResults.ComplianceScore = [Math]::Min($complianceResults.Frameworks.Count * 20, 100)
    
    Write-Log "INFO" "Compliance checking completed: $($complianceResults.Frameworks.Count) frameworks detected"
    
    return $complianceResults
}

# Readiness assessment function
function Start-ReadinessAssessment {
    param($ProjectPath)
    
    Write-Log "INFO" "Starting deployment readiness assessment"
    
    $readinessResults = @{
        OverallScore = 0
        Categories = @{}
        Recommendations = @()
        Status = "Not Ready"
    }
    
    # Calculate readiness score based on various factors
    $scores = @{
        Configuration = 0
        Security = 0
        Dependencies = 0
        Documentation = 0
        Testing = 0
    }
    
    # Simple scoring logic (placeholder)
    $scores.Configuration = 75
    $scores.Security = 60
    $scores.Dependencies = 80
    $scores.Documentation = 40
    $scores.Testing = 50
    
    $readinessResults.Categories = $scores
    $readinessResults.OverallScore = ($scores.Values | Measure-Object -Average).Average
    
    if ($readinessResults.OverallScore -ge 80) {
        $readinessResults.Status = "Ready"
    } elseif ($readinessResults.OverallScore -ge 60) {
        $readinessResults.Status = "Conditional"
    } else {
        $readinessResults.Status = "Not Ready"
    }
    
    Write-Log "INFO" "Readiness assessment completed: $($readinessResults.Status) ($($readinessResults.OverallScore)%)"
    
    return $readinessResults
}

# Save deployment audit results
function Save-DeploymentAuditResults {
    param($OutputPath)
    
    $resultsFile = Join-Path $OutputPath "deployment_audit_results.json"
    
    try {
        $global:DeploymentAuditResults | ConvertTo-Json -Depth 20 | Out-File $resultsFile -Encoding UTF8
        Write-Log "INFO" "Deployment audit results saved to $resultsFile"
    }
    catch {
        Write-Log "ERROR" "Failed to save deployment audit results: $_"
    }
}

# Interactive mode
function Start-InteractiveMode {
    Write-Host "`n=== Deployment Audit Engine - Interactive Mode ===" -ForegroundColor Cyan
    Write-Host "Version: $($global:DeploymentAuditConfig.Version)" -ForegroundColor Gray
    
    while ($true) {
        Write-Host "`nOptions:" -ForegroundColor Yellow
        Write-Host "1. Start new deployment audit"
        Write-Host "2. Load existing results"
        Write-Host "3. Configure audit settings"
        Write-Host "4. View audit statistics"
        Write-Host "5. Export readiness report"
        Write-Host "6. Exit"
        
        $choice = Read-Host "`nSelect option (1-6)"
        
        switch ($choice) {
            "1" {
                $project = Read-Host "Enter project path"
                $output = Read-Host "Enter output path (press Enter for default)"
                if ([string]::IsNullOrEmpty($output)) { $output = ".\deployment_audit_output" }
                
                $validateConfig = (Read-Host "Validate configuration? (y/n)").ToLower() -eq 'y'
                $checkSecurity = (Read-Host "Check security? (y/n)").ToLower() -eq 'y'
                $verifyDeps = (Read-Host "Verify dependencies? (y/n)").ToLower() -eq 'y'
                $checkCompliance = (Read-Host "Check compliance? (y/n)").ToLower() -eq 'y'
                
                Start-DeploymentAudit -ProjectPath $project -OutputPath $output
            }
            "2" {
                $resultsPath = Read-Host "Enter results file path"
                Load-DeploymentAuditResults -Path $resultsPath
            }
            "3" {
                Show-ConfigurationMenu
            }
            "4" {
                Show-Statistics
            }
            "5" {
                $format = Read-Host "Report format (HTML/JSON/Markdown)"
                $path = Read-Host "Output path"
                Generate-ReadinessReport -OutputPath $path -Format $format
            }
            "6" {
                Write-Host "Exiting..." -ForegroundColor Yellow
                return
            }
            default {
                Write-Host "Invalid option. Please try again." -ForegroundColor Red
            }
        }
    }
}

# Main execution
if ($Interactive) {
    Start-InteractiveMode
}
elseif ($ProjectPath) {
    Start-DeploymentAudit -ProjectPath $ProjectPath -OutputPath $OutputPath
}
else {
    Write-Host "Deployment Audit Engine v$($global:DeploymentAuditConfig.Version)" -ForegroundColor Cyan
    Write-Host "Use -Interactive for interactive mode or specify -ProjectPath" -ForegroundColor Yellow
    Write-Host "Example: .\DeploymentAuditEngine.ps1 -ProjectPath 'C:\project' -CheckSecurity -VerifyDependencies -GenerateReadinessReport" -ForegroundColor Gray
}