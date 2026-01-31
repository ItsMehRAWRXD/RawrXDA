
# Cache for function results
$script:FunctionCache = @{}

function Get-FromCache {
    param([string]$Key)
    if ($script:FunctionCache.ContainsKey($Key)) {
        return $script:FunctionCache[$Key]
    }
    return $null
}

function Set-Cache {
    param([string]$Key, $Value)
    $script:FunctionCache[$Key] = $Value
}<#
.SYNOPSIS
    RawrXD Security Vulnerability Scanner Module
    
.DESCRIPTION
    Production-ready module for comprehensive security vulnerability scanning of PowerShell codebases.
    Detects common security issues, insecure patterns, and provides remediation recommendations.
    
.AUTHOR
    RawrXD Auto-Generation System
    
.VERSION
    1.0.0
#>

# Import required helpers
$scriptRoot = Split-Path -Parent $PSScriptRoot
$loggingModule = Join-Path $scriptRoot 'RawrXD.Logging.psm1'
$configModule = Join-Path $scriptRoot 'RawrXD.Config.psm1'

if (Test-Path $loggingModule) { 
    try { Import-Module $loggingModule -Force -ErrorAction SilentlyContinue } catch { } 
}
if (Test-Path $configModule) { 
    try { Import-Module $configModule -Force -ErrorAction SilentlyContinue } catch { } 
}

# Security vulnerability patterns
$script:VulnerabilityPatterns = @{
    'High' = @{
        'Command Injection' = @(
            'Invoke-Expression\s*\$',
            'cmd\s*/c\s*\$',
            'powershell\s.*\$',
            'Start-Process.*\$.*-ArgumentList'
        )
        'Path Traversal' = @(
            '\.\.[/\\]',
            'Join-Path.*\.\.',
            'Resolve-Path.*\.\.'
        )
        'Credential Exposure' = @(
            'password\s*=\s*[''"][^''"]+[''"]',
            'ConvertTo-SecureString.*-AsPlainText',
            '\$cred.*=.*Get-Credential.*-UserName.*-Password'
        )
    }
    'Medium' = @{
        'Insecure Downloads' = @(
            'Invoke-WebRequest.*http://',
            'wget.*http://',
            'curl.*http://',
            'DownloadFile.*http://'
        )
        'Registry Manipulation' = @(
            'Set-ItemProperty.*HKLM:',
            'New-Item.*HKEY_',
            'Remove-Item.*HKLM:'
        )
        'File System Access' = @(
            'Remove-Item.*-Recurse.*-Force',
            'Get-Content.*-Raw.*\$',
            'Set-Content.*-Value.*\$'
        )
    }
    'Low' = @{
        'Information Disclosure' = @(
            'Write-Host.*\$env:',
            'Write-Output.*\$PSScriptRoot',
            'Write-Verbose.*password'
        )
        'Weak Randomization' = @(
            'Get-Random\s*\)',
            '\$random\s*=\s*new-object',
            'System\.Random\(\)'
        )
    }
}

function Invoke-SecurityVulnerabilityScanner {
    <#
    .SYNOPSIS
        Performs comprehensive security vulnerability scanning of PowerShell files
        
    .DESCRIPTION
        Scans PowerShell scripts and modules for common security vulnerabilities including:
        - Command injection patterns
        - Path traversal vulnerabilities  
        - Credential exposure
        - Insecure network operations
        - Registry manipulation issues
        - Information disclosure
        
    .PARAMETER SourceDir
        Root directory to scan for PowerShell files
        
    .PARAMETER OutputPath
        Path for the vulnerability report JSON file
        
    .PARAMETER ScanLevel
        Security scan depth: Basic, Standard, or Comprehensive
        
    .PARAMETER IncludePatterns
        File patterns to include in the scan
        
    .PARAMETER ExcludePatterns
        File patterns to exclude from the scan
        
    .PARAMETER GenerateReport
        Generate detailed JSON report
        
    .EXAMPLE
        Invoke-SecurityVulnerabilityScanner -SourceDir "C:\Scripts" -ScanLevel Comprehensive
        
    .EXAMPLE
        Invoke-SecurityVulnerabilityScanner -SourceDir $pwd -GenerateReport -OutputPath "security_report.json"
        
    .OUTPUTS
        System.Collections.Hashtable
        Returns hashtable with vulnerability scan results
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [ValidateScript({Test-Path $_ -PathType Container})]
        [string]$SourceDir = $(Split-Path -Parent $PSScriptRoot),
        
        [Parameter(Mandatory=$false)]
        [string]$OutputPath = $null,
        
        [Parameter(Mandatory=$false)]
        [ValidateSet('Basic', 'Standard', 'Comprehensive')]
        [string]$ScanLevel = 'Standard',
        
        [Parameter(Mandatory=$false)]
        [string[]]$IncludePatterns = @('*.ps1', '*.psm1'),
        
        [Parameter(Mandatory=$false)]
        [string[]]$ExcludePatterns = @('*.backup.*', '*_test.*'),
        
        [Parameter(Mandatory=$false)]
        [switch]$GenerateReport
    )
    
    if (Get-Command Measure-FunctionLatency -ErrorAction SilentlyContinue) {
        return Measure-FunctionLatency -FunctionName 'Invoke-SecurityVulnerabilityScanner' -Script {
            Invoke-SecurityScan @PSBoundParameters
        }
    } else {
        return Invoke-SecurityScan @PSBoundParameters
    }
}

function Invoke-SecurityScan {
    [CmdletBinding()]
    param(
        [string]$SourceDir,
        [string]$OutputPath,
        [string]$ScanLevel,
        [string[]]$IncludePatterns,
        [string[]]$ExcludePatterns,
        [switch]$GenerateReport
    )
    
    try {
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'INFO' -Message "Starting security vulnerability scan for $SourceDir (Level: $ScanLevel)" -Function 'Invoke-SecurityVulnerabilityScanner'
        } else {
            Write-Host "Starting security scan for $SourceDir"
        }
        
        # Get files to scan
        $allFiles = @()
        foreach ($pattern in $IncludePatterns) {
            $files = Get-ChildItem -Path $SourceDir -Filter $pattern -Recurse -ErrorAction SilentlyContinue
            $allFiles += $files
        }
        
        # Apply exclusions
        foreach ($excludePattern in $ExcludePatterns) {
            $allFiles = $allFiles | Where-Object { $_.Name -notlike $excludePattern }
        }
        
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'INFO' -Message "Scanning $($allFiles.Count) files for security vulnerabilities" -Function 'Invoke-SecurityVulnerabilityScanner'
        } else {
            Write-Host "Scanning $($allFiles.Count) files"
        }
        
        # Initialize results
        $scanResults = @{
            Metadata = @{
                ScanTimestamp = (Get-Date).ToString('o')
                SourceDirectory = $SourceDir
                ScanLevel = $ScanLevel
                FilesScanned = $allFiles.Count
                ScanDurationSeconds = 0
                ToolVersion = '1.0.0'
            }
            Summary = @{
                TotalVulnerabilities = 0
                HighSeverity = 0
                MediumSeverity = 0
                LowSeverity = 0
                CleanFiles = 0
                RiskScore = 0
            }
            Vulnerabilities = @()
            FileResults = @()
        }
        
        $scanStart = Get-Date
        
        # Scan each file
        foreach ($file in $allFiles) {
            $fileStart = Get-Date
            $fileVulns = @()
            
            try {
                if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
                    Write-StructuredLog -Level 'DEBUG' -Message "Scanning: $($file.Name)" -Function 'Invoke-SecurityVulnerabilityScanner'
                } else {
                    Write-Verbose "Scanning: $($file.Name)"
                }
                
                $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
                if (-not $content) { continue }
                
                # Check patterns by severity level
                $severityLevels = @('High')
                if ($ScanLevel -in @('Standard', 'Comprehensive')) { $severityLevels += 'Medium' }
                if ($ScanLevel -eq 'Comprehensive') { $severityLevels += 'Low' }
                
                foreach ($severity in $severityLevels) {
                    foreach ($category in $script:VulnerabilityPatterns[$severity].Keys) {
                        foreach ($pattern in $script:VulnerabilityPatterns[$severity][$category]) {
                            $matches = [regex]::Matches($content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
                            
                            foreach ($match in $matches) {
                                $lineNumber = ($content.Substring(0, $match.Index) -split "`n").Count
                                $line = ($content -split "`n")[$lineNumber - 1].Trim()
                                
                                $vulnerability = @{
                                    Severity = $severity
                                    Category = $category
                                    Pattern = $pattern
                                    File = $file.FullName
                                    RelativePath = $file.FullName.Replace($SourceDir, '').TrimStart('\', '/')
                                    LineNumber = $lineNumber
                                    LineContent = $line
                                    MatchText = $match.Value
                                    RiskDescription = Get-VulnerabilityDescription -Category $category -Severity $severity
                                    Recommendation = Get-VulnerabilityRecommendation -Category $category
                                    CVSS_Score = Get-CVSSScore -Category $category -Severity $severity
                                }
                                
                                $fileVulns += $vulnerability
                                $scanResults.Vulnerabilities += $vulnerability
                                $scanResults.Summary.TotalVulnerabilities++
                                
                                switch ($severity) {
                                    'High' { $scanResults.Summary.HighSeverity++ }
                                    'Medium' { $scanResults.Summary.MediumSeverity++ }
                                    'Low' { $scanResults.Summary.LowSeverity++ }
                                }
                            }
                        }
                    }
                }
                
            } catch {
                if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
                    Write-StructuredLog -Level 'WARNING' -Message "Failed to scan $($file.FullName): $($_.Exception.Message)" -Function 'Invoke-SecurityVulnerabilityScanner'
                } else {
                    Write-Warning "Failed to scan $($file.FullName): $($_.Exception.Message)"
                }
            }
            
            # File result summary
            $fileEnd = Get-Date
            $fileResult = @{
                FilePath = $file.FullName
                RelativePath = $file.FullName.Replace($SourceDir, '').TrimStart('\', '/')
                VulnerabilityCount = $fileVulns.Count
                HighestSeverity = if ($fileVulns.Count -gt 0) { 
                    ($fileVulns | Sort-Object @{Expression={switch($_.Severity){'High'{1}'Medium'{2}'Low'{3}default{4}}}} | Select-Object -First 1).Severity 
                } else { 'None' }
                ScanDurationMs = ($fileEnd - $fileStart).TotalMilliseconds
                Status = if ($fileVulns.Count -eq 0) { 'Clean' } else { 'Issues Found' }
            }
            $scanResults.FileResults += $fileResult
            
            if ($fileVulns.Count -eq 0) { $scanResults.Summary.CleanFiles++ }
        }
        
        $scanEnd = Get-Date
        $scanResults.Metadata.ScanDurationSeconds = ($scanEnd - $scanStart).TotalSeconds
        
        # Calculate risk score (0-100)
        $riskScore = 0
        $riskScore += $scanResults.Summary.HighSeverity * 10
        $riskScore += $scanResults.Summary.MediumSeverity * 5
        $riskScore += $scanResults.Summary.LowSeverity * 1
        $riskScore = [Math]::Min(100, $riskScore)
        $scanResults.Summary.RiskScore = $riskScore
        
        # Log summary
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'INFO' -Message "Security scan complete. Found $($scanResults.Summary.TotalVulnerabilities) vulnerabilities (High: $($scanResults.Summary.HighSeverity), Medium: $($scanResults.Summary.MediumSeverity), Low: $($scanResults.Summary.LowSeverity))" -Function 'Invoke-SecurityVulnerabilityScanner'
        } else {
            Write-Host "Security scan complete. Found $($scanResults.Summary.TotalVulnerabilities) vulnerabilities"
        }
        
        # Generate report if requested
        if ($GenerateReport) {
            if (-not $OutputPath) {
                if (Get-Command Get-RawrXDRootPath -ErrorAction SilentlyContinue) {
                    $OutputPath = Join-Path (Get-RawrXDRootPath) "auto_generated_methods/SecurityScan_Report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
                } else {
                    $OutputPath = Join-Path $SourceDir "SecurityScan_Report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
                }
            }
            
            $scanResults | ConvertTo-Json -Depth 15 | Set-Content $OutputPath -Encoding UTF8
            
            if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
                Write-StructuredLog -Level 'INFO' -Message "Security scan report saved: $OutputPath" -Function 'Invoke-SecurityVulnerabilityScanner'
            } else {
                Write-Host "Security scan report saved: $OutputPath"
            }
        }
        
        return $scanResults
        
    } catch {
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'ERROR' -Message $_.Exception.Message -Function 'Invoke-SecurityVulnerabilityScanner'
        } else {
            Write-Error $_.Exception.Message
        }
        throw
    }
}

function Get-VulnerabilityDescription {
    param([string]$Category, [string]$Severity)
    
    $descriptions = @{
        'Command Injection' = "Potential command injection vulnerability where user input may be executed as system commands"
        'Path Traversal' = "Path traversal vulnerability that could allow access to files outside the intended directory"
        'Credential Exposure' = "Credentials or sensitive data may be exposed in plaintext"
        'Insecure Downloads' = "Network operations using insecure HTTP protocol instead of HTTPS"
        'Registry Manipulation' = "Direct registry modifications that could affect system security"
        'File System Access' = "Potentially dangerous file system operations"
        'Information Disclosure' = "Sensitive information may be disclosed in logs or output"
        'Weak Randomization' = "Use of weak random number generation for security-sensitive operations"
    }
    
    return $descriptions[$Category] + " (Severity: $Severity)"
}

function Get-VulnerabilityRecommendation {
    param([string]$Category)
    
    $recommendations = @{
        'Command Injection' = "Validate and sanitize all input. Use parameterized commands. Avoid Invoke-Expression with user input."
        'Path Traversal' = "Validate file paths. Use Resolve-Path with -ErrorAction Stop. Implement path allow-lists."
        'Credential Exposure' = "Use SecureString for passwords. Store credentials in secure credential stores."
        'Insecure Downloads' = "Use HTTPS instead of HTTP. Validate SSL certificates."
        'Registry Manipulation' = "Minimize registry changes. Use proper error handling and validation."
        'File System Access' = "Implement proper access controls. Validate file paths and permissions."
        'Information Disclosure' = "Avoid logging sensitive information. Use Write-Debug instead of Write-Host for diagnostic data."
        'Weak Randomization' = "Use cryptographically secure random number generation for security purposes."
    }
    
    return $recommendations[$Category]
}

function Get-CVSSScore {
    param([string]$Category, [string]$Severity)
    
    $scores = @{
        'High' = @{
            'Command Injection' = 9.0
            'Path Traversal' = 8.5
            'Credential Exposure' = 8.0
        }
        'Medium' = @{
            'Insecure Downloads' = 6.0
            'Registry Manipulation' = 5.5
            'File System Access' = 5.0
        }
        'Low' = @{
            'Information Disclosure' = 3.0
            'Weak Randomization' = 2.5
        }
    }
    
    return $scores[$Severity][$Category] ?? 1.0
}

# Export public functions
Export-ModuleMember -Function @(
    'Invoke-SecurityVulnerabilityScanner'
)

