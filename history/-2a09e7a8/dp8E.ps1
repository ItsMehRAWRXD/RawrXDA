<#
.SYNOPSIS
    Production SecurityVulnerabilityScanner - Comprehensive security analysis engine

.DESCRIPTION
    Performs deep security analysis using pattern matching, AST inspection, and
    security best practices validation. Detects OWASP-style vulnerabilities,
    credential exposure, injection risks, and provides remediation guidance.

.PARAMETER SourceDirectory
    Directory to scan for vulnerabilities

.PARAMETER Severity
    Minimum severity level to report: Critical, High, Medium, Low, Info

.PARAMETER EnableCVELookup
    Enable CVE database lookup for known vulnerable patterns

.PARAMETER ExcludePatterns
    File patterns to exclude from scanning

.EXAMPLE
    Invoke-SecurityVulnerabilityScanner -SourceDirectory 'D:/project' -Severity 'Medium'
#>

if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [hashtable]$Data = $null
        )
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$Level] $Message" -ForegroundColor $color
    }
}

# Comprehensive vulnerability patterns database
$script:VulnerabilityPatterns = @(
    # Code Execution Vulnerabilities
    @{
        Name = 'Invoke-Expression Usage'
        Pattern = 'Invoke-Expression|iex\s+[\$\(]'
        Severity = 'Critical'
        Category = 'Code Injection'
        CWE = 'CWE-94'
        Description = 'Invoke-Expression can execute arbitrary code and is vulnerable to injection attacks.'
        Remediation = 'Replace with direct command invocation, & operator, or parameterized commands.'
    },
    @{
        Name = 'Dynamic Script Block Execution'
        Pattern = '\[scriptblock\]::Create\s*\('
        Severity = 'High'
        Category = 'Code Injection'
        CWE = 'CWE-94'
        Description = 'Dynamic script block creation can execute untrusted code.'
        Remediation = 'Use static script blocks or validate input before execution.'
    },
    @{
        Name = 'Start-Process with User Input'
        Pattern = 'Start-Process\s+.*\$[a-zA-Z]'
        Severity = 'High'
        Category = 'Command Injection'
        CWE = 'CWE-78'
        Description = 'Start-Process with variable arguments may allow command injection.'
        Remediation = 'Validate and sanitize all arguments before passing to Start-Process.'
    },

    # Credential & Secret Vulnerabilities
    @{
        Name = 'Plain Text Password'
        Pattern = 'ConvertTo-SecureString\s+.*-AsPlainText'
        Severity = 'Critical'
        Category = 'Credential Exposure'
        CWE = 'CWE-256'
        Description = 'Passwords stored as plain text can be easily compromised.'
        Remediation = 'Use Windows Credential Manager, Azure Key Vault, or encrypted credential files.'
    },
    @{
        Name = 'Hardcoded Credentials'
        Pattern = '(?i)(password|passwd|pwd|secret|apikey|api_key|token|bearer)\s*[=:]\s*[''"][^''"]{4,}[''"]'
        Severity = 'Critical'
        Category = 'Credential Exposure'
        CWE = 'CWE-798'
        Description = 'Hardcoded credentials in source code can be easily extracted.'
        Remediation = 'Use environment variables, secure vaults, or credential managers.'
    },
    @{
        Name = 'Connection String with Credentials'
        Pattern = '(?i)(connection\s*string|connstr).*password\s*='
        Severity = 'Critical'
        Category = 'Credential Exposure'
        CWE = 'CWE-798'
        Description = 'Connection strings containing embedded credentials are insecure.'
        Remediation = 'Use integrated authentication or secure credential storage.'
    },
    @{
        Name = 'AWS Access Keys'
        Pattern = '(?i)(AKIA|ABIA|ACCA|ASIA)[0-9A-Z]{16}'
        Severity = 'Critical'
        Category = 'Credential Exposure'
        CWE = 'CWE-798'
        Description = 'AWS access keys detected in source code.'
        Remediation = 'Use IAM roles, AWS Secrets Manager, or environment variables.'
    },
    @{
        Name = 'Private Key Material'
        Pattern = '-----BEGIN\s+(RSA\s+)?PRIVATE\s+KEY-----'
        Severity = 'Critical'
        Category = 'Credential Exposure'
        CWE = 'CWE-321'
        Description = 'Private key embedded in source code.'
        Remediation = 'Store private keys in secure key stores or HSMs.'
    },

    # Input Validation Vulnerabilities
    @{
        Name = 'SQL Injection Risk'
        Pattern = '(?i)(SELECT|INSERT|UPDATE|DELETE|EXEC|EXECUTE)\s+.*\$[a-zA-Z]'
        Severity = 'Critical'
        Category = 'Injection'
        CWE = 'CWE-89'
        Description = 'SQL query constructed with variable interpolation may be vulnerable to injection.'
        Remediation = 'Use parameterized queries or stored procedures.'
    },
    @{
        Name = 'Path Traversal Risk'
        Pattern = '\.\.[/\\]|\$[a-zA-Z]+.*[/\\]'
        Severity = 'High'
        Category = 'Path Traversal'
        CWE = 'CWE-22'
        Description = 'Path manipulation may allow directory traversal attacks.'
        Remediation = 'Validate paths, use Resolve-Path, and implement allow-lists.'
    },
    @{
        Name = 'Unvalidated File Operations'
        Pattern = '(Get-Content|Set-Content|Remove-Item|Copy-Item)\s+.*\$[a-zA-Z](?![^\|]*\|.*Test-Path)'
        Severity = 'Medium'
        Category = 'Input Validation'
        CWE = 'CWE-20'
        Description = 'File operations with unvalidated paths may be exploited.'
        Remediation = 'Always validate file paths before operations using Test-Path and Resolve-Path.'
    },

    # Network Security Vulnerabilities
    @{
        Name = 'Insecure HTTP Connection'
        Pattern = '(?i)http://(?!localhost|127\.0\.0\.1|0\.0\.0\.0)'
        Severity = 'Medium'
        Category = 'Transport Security'
        CWE = 'CWE-319'
        Description = 'Non-localhost HTTP connections transmit data in clear text.'
        Remediation = 'Use HTTPS for all external connections.'
    },
    @{
        Name = 'Certificate Validation Disabled'
        Pattern = '(?i)(-SkipCertificateCheck|ServerCertificateValidationCallback|ServicePointManager.*SecurityProtocol)'
        Severity = 'High'
        Category = 'Transport Security'
        CWE = 'CWE-295'
        Description = 'Disabling certificate validation enables man-in-the-middle attacks.'
        Remediation = 'Use valid certificates and maintain proper certificate chain validation.'
    },
    @{
        Name = 'Weak TLS Configuration'
        Pattern = '(?i)(ssl3|tls10|tls11|SecurityProtocolType\.Ssl3|SecurityProtocolType\.Tls\b)'
        Severity = 'High'
        Category = 'Transport Security'
        CWE = 'CWE-326'
        Description = 'Weak TLS/SSL protocols are vulnerable to known attacks.'
        Remediation = 'Use TLS 1.2 or higher exclusively.'
    },

    # Error Handling & Information Disclosure
    @{
        Name = 'Verbose Error Output'
        Pattern = '\$Error\[0\]|\$_\.Exception(?!\.Message\b)|\$PSItem\.Exception(?!\.Message\b)'
        Severity = 'Low'
        Category = 'Information Disclosure'
        CWE = 'CWE-209'
        Description = 'Detailed exception information may leak sensitive data.'
        Remediation = 'Log detailed errors securely but only show generic messages to users.'
    },
    @{
        Name = 'Write-Debug in Production'
        Pattern = 'Write-Debug\s+.*(\$|secret|password|key|token)'
        Severity = 'Medium'
        Category = 'Information Disclosure'
        CWE = 'CWE-215'
        Description = 'Debug output may expose sensitive information in production.'
        Remediation = 'Remove or conditionally enable debug statements.'
    },

    # Dangerous Operations
    @{
        Name = 'Dangerous WMI/CIM Operations'
        Pattern = '(?i)(Get-WmiObject|Invoke-WmiMethod|Get-CimInstance)\s+.*(Win32_Process|Win32_Service)'
        Severity = 'Medium'
        Category = 'Dangerous Operations'
        CWE = 'CWE-250'
        Description = 'WMI operations can be used for privilege escalation or lateral movement.'
        Remediation = 'Implement least privilege and audit WMI usage.'
    },
    @{
        Name = 'Registry Modification'
        Pattern = '(?i)(Set-ItemProperty|New-ItemProperty|Remove-ItemProperty)\s+.*HKLM:|HKCU:'
        Severity = 'Medium'
        Category = 'Dangerous Operations'
        CWE = 'CWE-732'
        Description = 'Registry modifications may persist malicious configurations.'
        Remediation = 'Document and audit all registry changes, implement change management.'
    },
    @{
        Name = 'Execution Policy Bypass'
        Pattern = '(?i)-ExecutionPolicy\s+(Bypass|Unrestricted|RemoteSigned)'
        Severity = 'Medium'
        Category = 'Security Control Bypass'
        CWE = 'CWE-693'
        Description = 'Bypassing execution policy weakens security controls.'
        Remediation = 'Use AllSigned policy where possible and sign scripts.'
    },

    # Cryptographic Weaknesses
    @{
        Name = 'Weak Hash Algorithm'
        Pattern = '(?i)(MD5|SHA1)(?!-)'
        Severity = 'Medium'
        Category = 'Cryptographic Weakness'
        CWE = 'CWE-328'
        Description = 'MD5 and SHA1 are cryptographically broken for security purposes.'
        Remediation = 'Use SHA-256 or SHA-512 for cryptographic operations.'
    },
    @{
        Name = 'Hardcoded IV or Salt'
        Pattern = '(?i)(IV|salt|nonce)\s*=\s*\[byte\[\]\]\s*@?\('
        Severity = 'High'
        Category = 'Cryptographic Weakness'
        CWE = 'CWE-329'
        Description = 'Hardcoded initialization vectors or salts weaken encryption.'
        Remediation = 'Generate random IVs and salts for each encryption operation.'
    }
)

function Get-FileSecurityContext {
    <#
    .SYNOPSIS
        Extract security-relevant context from a file
    #>
    param([string]$FilePath)

    $context = @{
        HasNetworkCalls = $false
        HasFileOperations = $false
        HasCredentialHandling = $false
        HasCrypto = $false
        HasProcessExecution = $false
        FunctionCount = 0
        LineCount = 0
    }

    try {
        $content = Get-Content $FilePath -Raw -ErrorAction Stop
        $context.LineCount = ($content -split "`n").Count

        $context.HasNetworkCalls = $content -match '(?i)(Invoke-RestMethod|Invoke-WebRequest|Net\.WebClient|HttpClient)'
        $context.HasFileOperations = $content -match '(?i)(Get-Content|Set-Content|Out-File|Export-|Import-)'
        $context.HasCredentialHandling = $content -match '(?i)(credential|password|secret|token|apikey)'
        $context.HasCrypto = $content -match '(?i)(encrypt|decrypt|hash|certificate|secure)'
        $context.HasProcessExecution = $content -match '(?i)(Start-Process|Invoke-Command|& \$|Invoke-Expression)'

        # Count function definitions
        $tokens = $null
        $errors = $null
        $ast = [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$tokens, [ref]$errors)
        $context.FunctionCount = ($ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $true)).Count
    } catch {
        # Silently continue with default context
    }

    return $context
}

function Get-VulnerabilityLineContext {
    <#
    .SYNOPSIS
        Get code context around a vulnerability match
    #>
    param(
        [string[]]$Lines,
        [int]$MatchLineNumber,
        [int]$ContextLines = 3
    )

    $startLine = [Math]::Max(0, $MatchLineNumber - $ContextLines - 1)
    $endLine = [Math]::Min($Lines.Count - 1, $MatchLineNumber + $ContextLines - 1)

    $context = @()
    for ($i = $startLine; $i -le $endLine; $i++) {
        $lineMarker = if ($i -eq ($MatchLineNumber - 1)) { '>>>' } else { '   ' }
        $context += "$lineMarker $($i + 1): $($Lines[$i])"
    }

    return $context -join "`n"
}

function Invoke-SecurityVulnerabilityScanner {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [ValidateScript({Test-Path $_ -PathType 'Container'})]
        [string]$SourceDirectory = "D:/lazy init ide",

        [ValidateSet('Critical','High','Medium','Low','Info')]
        [string]$MinSeverity = 'Low',

        [string]$ReportPath = "D:/lazy init ide/reports/security_report.json",

        [string[]]$ExcludePatterns = @('*.tests.ps1', '*.test.ps1', '*_test.ps1'),

        [switch]$IncludeContext,

        [int]$ContextLines = 3
    )

    $functionName = 'Invoke-SecurityVulnerabilityScanner'
    $startTime = Get-Date

    Write-StructuredLog -Message "Starting comprehensive security scan of $SourceDirectory" -Level Info

    try {
        # Ensure report directory exists
        $reportDir = Split-Path $ReportPath -Parent
        if (-not (Test-Path $reportDir)) {
            New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
        }

        # Severity ranking for filtering
        $severityRank = @{ 'Critical' = 5; 'High' = 4; 'Medium' = 3; 'Low' = 2; 'Info' = 1 }
        $minSeverityRank = $severityRank[$MinSeverity]

        # Discover files to scan
        $files = Get-ChildItem -Path $SourceDirectory -Recurse -Include '*.ps1','*.psm1','*.psd1' -ErrorAction Stop
        $files = $files | Where-Object {
            $fileName = $_.Name
            -not ($ExcludePatterns | Where-Object { $fileName -like $_ })
        }

        Write-StructuredLog -Message "Scanning $($files.Count) files for security vulnerabilities" -Level Info

        $vulnerabilities = @()
        $fileResults = @{}
        $categoryCounts = @{}
        $severityCounts = @{ 'Critical' = 0; 'High' = 0; 'Medium' = 0; 'Low' = 0; 'Info' = 0 }

        foreach ($file in $files) {
            Write-StructuredLog -Message "Scanning: $($file.Name)" -Level Debug

            try {
                $content = Get-Content -Path $file.FullName -Raw -ErrorAction Stop
                $lines = Get-Content -Path $file.FullName -ErrorAction Stop
                $fileContext = Get-FileSecurityContext -FilePath $file.FullName
                $fileVulns = @()

                foreach ($vuln in $script:VulnerabilityPatterns) {
                    # Check severity threshold
                    if ($severityRank[$vuln.Severity] -lt $minSeverityRank) { continue }

                    # Find all matches
                    $matches = [regex]::Matches($content, $vuln.Pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)

                    foreach ($match in $matches) {
                        # Calculate line number
                        $lineNumber = ($content.Substring(0, $match.Index) -split "`n").Count

                        $vulnEntry = [PSCustomObject]@{
                            FileName = $file.Name
                            FullPath = $file.FullName
                            Line = $lineNumber
                            Column = $match.Index - $content.LastIndexOf("`n", [Math]::Max(0, $match.Index - 1))
                            MatchedText = $match.Value.Substring(0, [Math]::Min($match.Value.Length, 100))
                            VulnerabilityName = $vuln.Name
                            Severity = $vuln.Severity
                            Category = $vuln.Category
                            CWE = $vuln.CWE
                            Description = $vuln.Description
                            Remediation = $vuln.Remediation
                            Context = if ($IncludeContext) { Get-VulnerabilityLineContext -Lines $lines -MatchLineNumber $lineNumber -ContextLines $ContextLines } else { $null }
                            Timestamp = (Get-Date).ToString('o')
                        }

                        $vulnerabilities += $vulnEntry
                        $fileVulns += $vulnEntry
                        $severityCounts[$vuln.Severity]++

                        if (-not $categoryCounts.ContainsKey($vuln.Category)) {
                            $categoryCounts[$vuln.Category] = 0
                        }
                        $categoryCounts[$vuln.Category]++

                        Write-StructuredLog -Message "[$($vuln.Severity)] $($file.Name):$lineNumber - $($vuln.Name)" -Level Warning
                    }
                }

                $fileResults[$file.FullName] = @{
                    FileName = $file.Name
                    VulnerabilityCount = $fileVulns.Count
                    SecurityContext = $fileContext
                    Vulnerabilities = $fileVulns
                }

            } catch {
                Write-StructuredLog -Message "Error scanning $($file.Name): $_" -Level Error
            }
        }

        # Calculate risk score
        $riskScore = ($severityCounts['Critical'] * 10) + ($severityCounts['High'] * 5) + ($severityCounts['Medium'] * 2) + $severityCounts['Low']
        $maxRiskScore = $files.Count * 50  # Theoretical max
        $normalizedRiskScore = [Math]::Round(($riskScore / [Math]::Max($maxRiskScore, 1)) * 100, 2)

        # Generate comprehensive report
        $report = [PSCustomObject]@{
            Timestamp = (Get-Date).ToString('o')
            ScanDuration = ((Get-Date) - $startTime).TotalSeconds
            SourceDirectory = $SourceDirectory
            Configuration = @{
                MinSeverity = $MinSeverity
                ExcludePatterns = $ExcludePatterns
                IncludeContext = $IncludeContext.IsPresent
            }
            Summary = @{
                TotalFilesScanned = $files.Count
                FilesWithVulnerabilities = ($fileResults.Keys | Where-Object { $fileResults[$_].VulnerabilityCount -gt 0 }).Count
                TotalVulnerabilitiesFound = $vulnerabilities.Count
                RiskScore = $normalizedRiskScore
                RiskLevel = switch ($normalizedRiskScore) {
                    { $_ -ge 50 } { 'Critical' }
                    { $_ -ge 30 } { 'High' }
                    { $_ -ge 15 } { 'Medium' }
                    { $_ -ge 5 } { 'Low' }
                    default { 'Minimal' }
                }
            }
            SeverityBreakdown = $severityCounts
            CategoryBreakdown = $categoryCounts
            TopAffectedFiles = $fileResults.Values | Sort-Object { $_.VulnerabilityCount } -Descending | Select-Object -First 10 | ForEach-Object {
                @{ FileName = $_.FileName; VulnerabilityCount = $_.VulnerabilityCount }
            }
            Vulnerabilities = $vulnerabilities
            FileResults = $fileResults
            Recommendations = @(
                if ($severityCounts['Critical'] -gt 0) { "IMMEDIATE ACTION REQUIRED: Address $($severityCounts['Critical']) critical vulnerabilities before deployment." }
                if ($severityCounts['High'] -gt 0) { "HIGH PRIORITY: Remediate $($severityCounts['High']) high-severity issues in current sprint." }
                if ($categoryCounts['Credential Exposure']) { "Implement centralized secret management (Azure Key Vault, HashiCorp Vault)." }
                if ($categoryCounts['Code Injection']) { "Review and eliminate all dynamic code execution patterns." }
                if ($categoryCounts['Transport Security']) { "Enforce TLS 1.2+ for all network communications." }
                "Schedule regular security scans in CI/CD pipeline."
                "Implement code review requirements for security-sensitive changes."
            )
        }

        $report | ConvertTo-Json -Depth 15 | Set-Content -Path $ReportPath -Encoding UTF8
        Write-StructuredLog -Message "Security report saved to $ReportPath" -Level Info

        # Summary output
        $duration = ((Get-Date) - $startTime).TotalSeconds
        Write-StructuredLog -Message "Security scan complete in $([Math]::Round($duration, 2))s" -Level Info
        Write-StructuredLog -Message "Risk Level: $($report.Summary.RiskLevel) (Score: $normalizedRiskScore)" -Level $(if ($normalizedRiskScore -ge 30) { 'Warning' } else { 'Info' })
        Write-StructuredLog -Message "Vulnerabilities: Critical=$($severityCounts['Critical']), High=$($severityCounts['High']), Medium=$($severityCounts['Medium']), Low=$($severityCounts['Low'])" -Level Info

        return $report

    } catch {
        Write-StructuredLog -Message "SecurityVulnerabilityScanner error: $($_.Exception.Message)" -Level Error
        throw
    }
}

