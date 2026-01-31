# RawrXD Source Digestion & Reverse Engineering Automation Engine
# Version: 1.0.0 - Production Ready
# Purpose: Complete source analysis, feature extraction, and deployment audit generation

param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath,
    
    [Parameter(Mandatory=$false)]
    [Alias('OutputPath')]
    [string]$OutputDir = ".",

    [Parameter(Mandatory=$false)]
    [string]$ConfigPath = $null,
    
    [Parameter(Mandatory=$false)]
    [switch]$GenerateRainbowTable,
    
    [Parameter(Mandatory=$false)]
    [switch]$CreateDeploymentAudit,
    
    [Parameter(Mandatory=$false)]
    [switch]$ExtractDependencies,
    
    [Parameter(Mandatory=$false)]
    [switch]$IdentifyMinimalViableProduct

    ,

    [Parameter(Mandatory=$false)]
    [switch]$GenerateReport,

    [Parameter(Mandatory=$false)]
    [string]$ReportFormat = "JSON",

    [Parameter(Mandatory=$false)]
    [switch]$EnableReverseEngineering,

    [Parameter(Mandatory=$false)]
    [switch]$EnableDeploymentAudit,

    [Parameter(Mandatory=$false)]
    [int]$MaxFiles = 200,

    [Parameter(Mandatory=$false)]
    [int]$MaxFileSizeKB = 1024
)

# Initialize global analysis structures
$global:SourceAnalysis = @{
    TotalFiles = 0
    TotalLines = 0
    FunctionCount = 0
    ClassCount = 0
    VariableCount = 0
    DependencyCount = 0
    ComplexityScore = 0
    FeatureMatrix = @()
    DependencyTree = @()
    ExecutionFlow = @()
    SecuritySurface = @()
    PerformanceBottlenecks = @()
}

$script:CurrentSourceFile = $null

function Get-SourceDigestionTargetFiles {
    param(
        [Parameter(Mandatory=$true)][string]$Path,
        [int]$MaxFiles,
        [int]$MaxFileSizeKB
    )

    if (-not (Test-Path $Path)) {
        throw "Source path not found: $Path"
    }

    $item = Get-Item -LiteralPath $Path
    if (-not $item.PSIsContainer) {
        return @($item.FullName)
    }

    $include = @('*.ps1','*.psm1','*.psd1','*.js','*.ts','*.json','*.md','*.cpp','*.c','*.h','*.hpp','*.cs','*.py','*.bat','*.cmd')
    $excludeDirs = @('node_modules','.git','bin','obj','packages','dist','build','out','.vs','.vscode','agentic_build','build-arm64','build_universal','build_masm_tests')

    $files = Get-ChildItem -Path $item.FullName -Recurse -File -Include $include -ErrorAction SilentlyContinue |
        Where-Object {
            $full = $_.FullName
            foreach ($d in $excludeDirs) {
                if ($full -match [regex]::Escape("\\$d\\")) { return $false }
            }
            return $true
        } |
        Where-Object { $_.Length -le ($MaxFileSizeKB * 1KB) } |
        Select-Object -First $MaxFiles

    return @($files | ForEach-Object { $_.FullName })
}

$global:FeatureRainbowTable = @{
    CoreFeatures = @()
    OptionalFeatures = @()
    SecurityFeatures = @()
    PerformanceFeatures = @()
    UI_Features = @()
    IntegrationFeatures = @()
}

$global:DeploymentAudit = @{
    RequiredFiles = @()
    RequiredModules = @()
    EnvironmentVariables = @()
    RegistryKeys = @()
    NetworkEndpoints = @()
    FilePermissions = @()
    ServiceDependencies = @()
}

# Feature extraction patterns
$global:FeaturePatterns = @{
    FileOperations = @('Get-Content', 'Set-Content', 'Add-Content', 'Out-File', 'Export-Csv', 'Import-Csv')
    NetworkOperations = @('Invoke-WebRequest', 'Invoke-RestMethod', 'Test-NetConnection', 'New-Object System.Net.WebClient')
    RegistryOperations = @('Get-ItemProperty', 'Set-ItemProperty', 'New-Item', 'Remove-Item', 'Get-ChildItem')
    ProcessOperations = @('Start-Process', 'Stop-Process', 'Get-Process', 'Wait-Process')
    CryptoOperations = @('ConvertTo-SecureString', 'ConvertFrom-SecureString', 'Protect-CmsMessage', 'Unprotect-CmsMessage')
    UI_Operations = @('Show-Command', 'Out-GridView', 'Read-Host', 'Write-Host', 'Write-Progress')
    WMI_Operations = @('Get-WmiObject', 'Invoke-WmiMethod', 'Register-WmiEvent')
    COM_Operations = @('New-Object -ComObject', '[System.Runtime.InteropServices.Marshal]')
}

function Start-SourceDigestion {
    Write-Host "🔍 Starting RawrXD Source Digestion Engine..." -ForegroundColor Cyan
    Write-Host "📁 Source: $SourcePath" -ForegroundColor Yellow
    
    if (-not (Test-Path $SourcePath)) {
        throw "Source file not found: $SourcePath"
    }

    $targetFiles = Get-SourceDigestionTargetFiles -Path $SourcePath -MaxFiles $MaxFiles -MaxFileSizeKB $MaxFileSizeKB
    $global:SourceAnalysis.TotalFiles = $targetFiles.Count

    Write-Host "📊 Analyzing $($global:SourceAnalysis.TotalFiles) file(s)..." -ForegroundColor Green

    foreach ($filePath in $targetFiles) {
        try {
            $script:CurrentSourceFile = $filePath
            $sourceContent = Get-Content $filePath -Raw -ErrorAction Stop
            $lineCount = (Get-Content $filePath -ErrorAction Stop).Count
            $global:SourceAnalysis.TotalLines += $lineCount

            Write-Host "   • $filePath ($lineCount lines)" -ForegroundColor DarkGray

            # Start parallel analysis tasks (kept as-is; functions currently run synchronously)
            $analysisTasks = @(
                (Extract-Functions $sourceContent),
                (Extract-Classes $sourceContent),
                (Extract-Variables $sourceContent),
                (Extract-Dependencies $sourceContent),
                (Analyze-ExecutionFlow $sourceContent),
                (Analyze-SecuritySurface $sourceContent),
                (Analyze-PerformanceProfile $sourceContent)
            )

            foreach ($task in $analysisTasks) {
                if ($task -is [System.Management.Automation.Job]) {
                    Wait-Job $task | Out-Null
                    Receive-Job $task
                }
            }
        } catch {
            Write-Host "⚠️  Skipping unreadable file: $filePath ($_)" -ForegroundColor Yellow
        } finally {
            $script:CurrentSourceFile = $null
        }
    }
    
    # Generate outputs based on flags
    if ($GenerateRainbowTable) {
        Generate-FeatureRainbowTable
    }
    
    if ($CreateDeploymentAudit) {
        Create-DeploymentAuditReport
    }
    
    if ($IdentifyMinimalViableProduct) {
        Identify-MVP_Features
    }
    
    # Generate comprehensive manifest
    Generate-ComprehensiveManifest
    
    Write-Host "✅ Source digestion complete!" -ForegroundColor Green
    Write-Host "📋 Generated manifests in: $OutputDir" -ForegroundColor Yellow
}

function Extract-Functions {
    param($content)
    
    Write-Host "🔍 Extracting functions..." -ForegroundColor Gray
    
    $functionMatches = [regex]::Matches($content, '^function\s+(\w+)\s*{', 'Multiline')
    $global:SourceAnalysis.FunctionCount = $functionMatches.Count
    
    foreach ($match in $functionMatches) {
        $funcName = $match.Groups[1].Value
        $funcStart = $match.Index
        $funcEnd = Find-BlockEnd $content $funcStart
        $funcBody = $content.Substring($funcStart, $funcEnd - $funcStart)
        
        $funcAnalysis = @{
            Name = $funcName
            File = $script:CurrentSourceFile
            StartLine = ($content.Substring(0, $funcStart) -split "`n").Count
            EndLine = ($content.Substring(0, $funcEnd) -split "`n").Count
            Parameters = Extract-Parameters $funcBody
            CalledFunctions = Extract-CalledFunctions $funcBody
            Complexity = Calculate-Complexity $funcBody
            SecurityRisk = Assess-SecurityRisk $funcBody
            PerformanceImpact = Assess-PerformanceImpact $funcBody
            Category = Categorize-Feature $funcName $funcBody
        }
        
        $global:SourceAnalysis.FeatureMatrix += $funcAnalysis
        
        # Add to rainbow table
        Add-ToRainbowTable $funcAnalysis
    }
    
    Write-Host "📊 Found $global:SourceAnalysis.FunctionCount functions" -ForegroundColor Green
}

function Extract-Classes {
    param($content)
    
    Write-Host "🔍 Extracting classes..." -ForegroundColor Gray
    
    $classMatches = [regex]::Matches($content, 'class\s+(\w+)', 'Multiline')
    $global:SourceAnalysis.ClassCount = $classMatches.Count
    
    foreach ($match in $classMatches) {
        $className = $match.Groups[1].Value
        $classStart = $match.Index
        $classEnd = Find-BlockEnd $content $classStart
        $classBody = $content.Substring($classStart, $classEnd - $classStart)
        
        $classAnalysis = @{
            Name = $className
            File = $script:CurrentSourceFile
            Methods = Extract-ClassMethods $classBody
            Properties = Extract-ClassProperties $classBody
            Inheritance = Extract-Inheritance $classBody
            Interfaces = Extract-Interfaces $classBody
        }
        
        $global:SourceAnalysis.FeatureMatrix += $classAnalysis
    }
    
    Write-Host "📊 Found $global:SourceAnalysis.ClassCount classes" -ForegroundColor Green
}

function Extract-Variables {
    param($content)
    
    Write-Host "🔍 Extracting variables..." -ForegroundColor Gray
    
    $varMatches = [regex]::Matches($content, '\$([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;\n]+)', 'Multiline')
    $global:SourceAnalysis.VariableCount = $varMatches.Count
    
    foreach ($match in $varMatches) {
        $varName = $match.Groups[1].Value
        $varValue = $match.Groups[2].Value.Trim()
        
        $varAnalysis = @{
            Name = $varName
            Value = $varValue
            File = $script:CurrentSourceFile
            Type = Infer-VariableType $varValue
            Scope = Determine-Scope $content $match.Index
            IsConstant = Test-IsConstant $varValue
            IsSecure = Test-IsSecureString $varValue
        }
        
        $global:SourceAnalysis.FeatureMatrix += $varAnalysis
    }
    
    Write-Host "📊 Found $global:SourceAnalysis.VariableCount variables" -ForegroundColor Green
}

function Extract-Dependencies {
    param($content)
    
    Write-Host "🔍 Extracting dependencies..." -ForegroundColor Gray
    
    $dependencies = @()
    
    # Extract module imports
    $moduleMatches = [regex]::Matches($content, 'Import-Module\s+([^;\n]+)', 'Multiline')
    foreach ($match in $moduleMatches) {
        $dependencies += @{
            Type = 'Module'
            Name = $match.Groups[1].Value.Trim()
            File = $script:CurrentSourceFile
            LineNumber = ($content.Substring(0, $match.Index) -split "`n").Count
        }
    }
    
    # Extract assembly references
    $assemblyMatches = [regex]::Matches($content, 'Add-Type\s+-AssemblyName\s+([^;\n]+)', 'Multiline')
    foreach ($match in $assemblyMatches) {
        $dependencies += @{
            Type = 'Assembly'
            Name = $match.Groups[1].Value.Trim()
            File = $script:CurrentSourceFile
            LineNumber = ($content.Substring(0, $match.Index) -split "`n").Count
        }
    }
    
    # Extract external commands
    $commandMatches = [regex]::Matches($content, '&\s*"([^"]+)"', 'Multiline')
    foreach ($match in $commandMatches) {
        $dependencies += @{
            Type = 'ExternalCommand'
            Name = $match.Groups[1].Value.Trim()
            File = $script:CurrentSourceFile
            LineNumber = ($content.Substring(0, $match.Index) -split "`n").Count
        }
    }
    
    $global:SourceAnalysis.DependencyCount = $dependencies.Count
    $global:SourceAnalysis.DependencyTree = $dependencies
    
    Write-Host "📊 Found $global:SourceAnalysis.DependencyCount dependencies" -ForegroundColor Green
}

function Analyze-ExecutionFlow {
    param($content)
    
    Write-Host "🔍 Analyzing execution flow..." -ForegroundColor Gray
    
    $flowAnalysis = @()
    
    # Extract main entry points
    $mainMatches = [regex]::Matches($content, 'if\s*\(\s*\$args\.Count\s*-eq\s*0\s*\)', 'Multiline')
    if ($mainMatches.Count -gt 0) {
        $flowAnalysis += @{
            Type = 'EntryPoint'
            Name = 'MainMenu'
            Condition = 'No CLI arguments'
            Actions = @('Show-MainMenu')
        }
    }
    
    # Extract CLI handlers
    $cliMatches = [regex]::Matches($content, 'switch\s*\(\s*\$args\[0\]\s*\)', 'Multiline')
    if ($cliMatches.Count -gt 0) {
        $flowAnalysis += @{
            Type = 'CLI_Dispatcher'
            Name = 'CLIMode'
            Modes = Extract-CLIModes $content
        }
    }
    
    $global:SourceAnalysis.ExecutionFlow = $flowAnalysis
    Write-Host "📊 Analyzed execution flow patterns" -ForegroundColor Green
}

function Analyze-SecuritySurface {
    param($content)
    
    Write-Host "🔍 Analyzing security surface..." -ForegroundColor Gray
    
    $securityIssues = @()
    
    # Check for hardcoded credentials
    $credPattern = '(password|passwd|pwd|credential)\s*=\s*["''][^"'']+["'']'
    $credMatches = [regex]::Matches($content, $credPattern, 'IgnoreCase')
    foreach ($match in $credMatches) {
        $securityIssues += @{
            Type = 'HardcodedCredential'
            Severity = 'Critical'
            Location = ($content.Substring(0, $match.Index) -split "`n").Count
            Recommendation = 'Move to secure vault or environment variable'
        }
    }
    
    # Check for insecure network calls
    $networkPattern = 'http://[^\s\'']+'
    $insecureNetwork = [regex]::Matches($content, $networkPattern, 'IgnoreCase')
    foreach ($match in $insecureNetwork) {
        $securityIssues += @{
            Type = 'InsecureNetwork'
            Severity = 'High'
            Location = ($content.Substring(0, $match.Index) -split "`n").Count
            Recommendation = 'Use HTTPS/TLS'
        }
    }
    
    # Check for privilege escalation
    $privPattern = 'Start-Process.*-Verb\s+RunAs'
    $privEscMatches = [regex]::Matches($content, $privPattern, 'IgnoreCase')
    foreach ($match in $privEscMatches) {
        $securityIssues += @{
            Type = 'PrivilegeEscalation'
            Severity = 'Medium'
            Location = ($content.Substring(0, $match.Index) -split "`n").Count
            Recommendation = 'Document and audit UAC requirements'
        }
    }
    
    $global:SourceAnalysis.SecuritySurface = $securityIssues
    Write-Host "📊 Found $($securityIssues.Count) security issues" -ForegroundColor Yellow
}

function Analyze-PerformanceProfile {
    param($content)
    
    Write-Host "🔍 Analyzing performance profile..." -ForegroundColor Gray
    
    $bottlenecks = @()
    
    # Check for nested loops
    $loopMatches = [regex]::Matches($content, 'foreach\s*\([^}]+\)\s*{[^}]*foreach', 'Multiline')
    foreach ($match in $loopMatches) {
        $bottlenecks += @{
            Type = 'NestedLoop'
            Severity = 'High'
            Location = ($content.Substring(0, $match.Index) -split "`n").Count
            Recommendation = 'Consider parallel processing or data structure optimization'
        }
    }
    
    # Check for repeated operations
    $repeatedOps = [regex]::Matches($content, '(Get-Content|Import-Csv)\s+[^;\n]+\s*;\s*\1', 'Multiline')
    foreach ($match in $repeatedOps) {
        $bottlenecks += @{
            Type = 'RepeatedIO'
            Severity = 'Medium'
            Location = ($content.Substring(0, $match.Index) -split "`n").Count
            Recommendation = 'Cache results in memory'
        }
    }
    
    $global:SourceAnalysis.PerformanceBottlenecks = $bottlenecks
    Write-Host "📊 Found $($bottlenecks.Count) performance bottlenecks" -ForegroundColor Yellow
}

function Generate-FeatureRainbowTable {
    Write-Host "🌈 Generating Feature Rainbow Table..." -ForegroundColor Magenta
    
    $rainbowTablePath = Join-Path $OutputDir "FeatureRainbowTable.json"
    
    $rainbowTable = @{
        Metadata = @{
            GeneratedDate = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            SourceFile = $SourcePath
            TotalFeatures = $global:SourceAnalysis.FeatureMatrix.Count
        }
        CoreFeatures = $global:FeatureRainbowTable.CoreFeatures | Sort-Object Priority
        OptionalFeatures = $global:FeatureRainbowTable.OptionalFeatures | Sort-Object Priority
        SecurityFeatures = $global:FeatureRainbowTable.SecurityFeatures | Sort-Object Priority
        PerformanceFeatures = $global:FeatureRainbowTable.PerformanceFeatures | Sort-Object Priority
        UI_Features = $global:FeatureRainbowTable.UI_Features | Sort-Object Priority
        IntegrationFeatures = $global:FeatureRainbowTable.IntegrationFeatures | Sort-Object Priority
    }
    
    $rainbowTable | ConvertTo-Json -Depth 10 | Out-File $rainbowTablePath -Encoding UTF8
    Write-Host "📊 Rainbow table saved to: $rainbowTablePath" -ForegroundColor Green
}

function Create-DeploymentAuditReport {
    Write-Host "📋 Creating Deployment Audit Report..." -ForegroundColor Cyan
    
    $auditPath = Join-Path $OutputDir "DeploymentAudit.md"
    $auditContent = @()
    $auditContent += "# RawrXD Deployment Audit Report"
    $auditContent += "Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")"
    $auditContent += "Source: $SourcePath"
    $auditContent += ""
    $auditContent += "## Executive Summary"
    $auditContent += "- Total Functions: $($global:SourceAnalysis.FunctionCount)"
    $auditContent += "- Total Classes: $($global:SourceAnalysis.ClassCount)"
    $auditContent += "- Total Dependencies: $($global:SourceAnalysis.DependencyCount)"
    $auditContent += "- Complexity Score: $($global:SourceAnalysis.ComplexityScore)"
    $auditContent += "- Security Issues: $($global:SourceAnalysis.SecuritySurface.Count)"
    $auditContent += "- Performance Bottlenecks: $($global:SourceAnalysis.PerformanceBottlenecks.Count)"
    $auditContent += ""
    $auditContent += "## Required Files"
    $auditContent += ($global:DeploymentAudit.RequiredFiles | ForEach-Object { "- $_" })
    $auditContent += ""
    $auditContent += "## Required Modules"
    $auditContent += ($global:DeploymentAudit.RequiredModules | ForEach-Object { "- $_" })
    $auditContent += ""
    $auditContent += "## Environment Variables"
    $auditContent += ($global:DeploymentAudit.EnvironmentVariables | ForEach-Object { "- $_" })
    $auditContent += ""
    $auditContent += "## Registry Keys"
    $auditContent += ($global:DeploymentAudit.RegistryKeys | ForEach-Object { "- $_" })
    $auditContent += ""
    $auditContent += "## Network Endpoints"
    $auditContent += ($global:DeploymentAudit.NetworkEndpoints | ForEach-Object { "- $_" })
    $auditContent += ""
    $auditContent += "## Security Recommendations"
    foreach ($issue in $global:SourceAnalysis.SecuritySurface) {
        $auditContent += "### $($issue.Type) [Severity: $($issue.Severity)]"
        $auditContent += "- Location: Line $($issue.Location)"
        $auditContent += "- Recommendation: $($issue.Recommendation)"
        $auditContent += ""
    }
    $auditContent += ""
    $auditContent += "## Performance Recommendations"
    foreach ($bottleneck in $global:SourceAnalysis.PerformanceBottlenecks) {
        $auditContent += "### $($bottleneck.Type) [Severity: $($bottleneck.Severity)]"
        $auditContent += "- Location: Line $($bottleneck.Location)"
        $auditContent += "- Recommendation: $($bottleneck.Recommendation)"
        $auditContent += ""
    }
    $auditContent += ""
    $auditContent += "## Deployment Checklist"
    $auditContent += "- [ ] All required modules installed"
    $auditContent += "- [ ] Environment variables configured"
    $auditContent += "- [ ] Registry keys created (if needed)"
    $auditContent += "- [ ] Network endpoints accessible"
    $auditContent += "- [ ] File permissions set correctly"
    $auditContent += "- [ ] Security audit completed"
    $auditContent += "- [ ] Performance testing completed"
    $auditContent += "- [ ] Backup procedures established"
    
    $auditContent | Out-File $auditPath -Encoding UTF8
    Write-Host "📋 Deployment audit saved to: $auditPath" -ForegroundColor Green
}

function Identify-MVP_Features {
    Write-Host "🎯 Identifying Minimal Viable Product Features..." -ForegroundColor Yellow
    
    $mvpFeatures = @()
    
    # Core functionality (must-have)
    $coreFuncs = $global:FeatureRainbowTable.CoreFeatures | Where-Object { $_.Priority -eq 'Critical' }
    foreach ($feature in $coreFuncs) {
        $mvpFeatures += @{
            Feature = $feature.Name
            Category = 'Core'
            ImplementationEffort = $feature.Complexity
            BusinessValue = 'High'
            Risk = $feature.SecurityRisk
        }
    }
    
    # Security essentials
    $securityFuncs = $global:FeatureRainbowTable.SecurityFeatures | Where-Object { $_.Priority -eq 'High' }
    foreach ($feature in $securityFuncs) {
        $mvpFeatures += @{
            Feature = $feature.Name
            Category = 'Security'
            ImplementationEffort = $feature.Complexity
            BusinessValue = 'Critical'
            Risk = $feature.SecurityRisk
        }
    }
    
    $mvpPath = Join-Path $OutputDir "MVP_Features.json"
    $mvpFeatures | ConvertTo-Json -Depth 5 | Out-File $mvpPath -Encoding UTF8
    Write-Host "🎯 MVP features saved to: $mvpPath" -ForegroundColor Green
}

function Generate-ComprehensiveManifest {
    Write-Host "📦 Generating Comprehensive Manifest..." -ForegroundColor Cyan
    
    $manifest = @{
        SourceAnalysis = $global:SourceAnalysis
        FeatureRainbowTable = $global:FeatureRainbowTable
        DeploymentAudit = $global:DeploymentAudit
        Metadata = @{
            GeneratedDate = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            ToolVersion = "1.0.0"
            SourceFile = $SourcePath
            AnalysisDuration = "TBD"
        }
    }
    
    $manifestPath = Join-Path $OutputDir "RawrXD_ComprehensiveManifest.json"
    $manifest | ConvertTo-Json -Depth 15 | Out-File $manifestPath -Encoding UTF8
    Write-Host "📦 Comprehensive manifest saved to: $manifestPath" -ForegroundColor Green

    # Orchestrator compatibility alias
    $resultsPath = Join-Path $OutputDir "analysis_results.json"
    $manifest | ConvertTo-Json -Depth 15 | Out-File $resultsPath -Encoding UTF8
}

# Helper functions
function Find-BlockEnd {
    param($content, $startIndex)
    
    $braceCount = 0
    $inString = $false
    $stringChar = ''
    
    for ($i = $startIndex; $i -lt $content.Length; $i++) {
        $char = $content[$i]
        
        # Handle strings
        if ($char -eq '"' -or $char -eq "'") {
            if (-not $inString) {
                $inString = $true
                $stringChar = $char
            } elseif ($stringChar -eq $char) {
                $inString = $false
            }
            continue
        }
        
        # Skip content inside strings
        if ($inString) { continue }
        
        # Count braces
        if ($char -eq '{') {
            $braceCount++
        } elseif ($char -eq '}') {
            $braceCount--
            if ($braceCount -eq 0) {
                return $i + 1
            }
        }
    }
    
    return $content.Length
}

function Extract-Parameters {
    param($funcBody)
    
    $paramMatches = [regex]::Matches($funcBody, 'param\s*\(\s*([^)]+)\s*\)', 'Multiline')
    if ($paramMatches.Count -gt 0) {
        return $paramMatches[0].Groups[1].Value -split ',' | ForEach-Object { $_.Trim() }
    }
    return @()
}

function Extract-CalledFunctions {
    param($funcBody)
    
    $callMatches = [regex]::Matches($funcBody, '\b(\w+)\s+', 'Multiline')
    return $callMatches | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique
}

function Calculate-Complexity {
    param($funcBody)
    
    $complexity = 0
    
    # Count conditional statements
    $complexity += ([regex]::Matches($funcBody, '\b(if|switch|where)\b', 'IgnoreCase')).Count * 2
    
    # Count loops
    $complexity += ([regex]::Matches($funcBody, '\b(foreach|for|while|do)\b', 'IgnoreCase')).Count * 3
    
    # Count error handling
    $complexity += ([regex]::Matches($funcBody, '\b(try|catch|finally)\b', 'IgnoreCase')).Count * 2
    
    # Count nested calls
    $complexity += ([regex]::Matches($funcBody, '\w+\s+\w+\s+\w+', 'Multiline')).Count
    
    return $complexity
}

function Assess-SecurityRisk {
    param($funcBody)
    
    $riskScore = 0
    
    # Check for credential handling
    if ($funcBody -match 'password|credential|securestring') { $riskScore += 3 }
    
    # Check for network operations
    if ($funcBody -match 'webrequest|webclient|http') { $riskScore += 2 }
    
    # Check for registry operations
    if ($funcBody -match 'registry|regkey') { $riskScore += 2 }
    
    # Check for process execution
    if ($funcBody -match 'start-process|&\s*"') { $riskScore += 2 }
    
    # Check for privilege escalation
    if ($funcBody -match 'runas|elevated|admin') { $riskScore += 3 }
    
    return $riskScore
}

function Assess-PerformanceImpact {
    param($funcBody)
    
    $impactScore = 0
    
    # Check for file I/O
    if ($funcBody -match 'get-content|out-file|import-csv') { $impactScore += 2 }
    
    # Check for network I/O
    if ($funcBody -match 'invoke-webrequest|invoke-restmethod') { $impactScore += 3 }
    
    # Check for nested loops
    if ($funcBody -match 'foreach.*foreach|for.*for') { $impactScore += 4 }
    
    # Check for WMI queries
    if ($funcBody -match 'get-wmiobject|invoke-wmimethod') { $impactScore += 2 }
    
    return $impactScore
}

function Categorize-Feature {
    param($funcName, $funcBody)
    
    $category = @{
        Name = $funcName
        Category = 'Unknown'
        Priority = 'Low'
        Complexity = Calculate-Complexity $funcBody
        SecurityRisk = Assess-SecurityRisk $funcBody
        PerformanceImpact = Assess-PerformanceImpact $funcBody
    }
    
    # Categorize based on name patterns
    if ($funcName -match 'show|display|write|out') {
        $category.Category = 'UI'
        $category.Priority = 'Medium'
    }
    elseif ($funcName -match 'get|set|new|remove|test') {
        $category.Category = 'Core'
        $category.Priority = 'High'
    }
    elseif ($funcName -match 'security|encrypt|decrypt|hash') {
        $category.Category = 'Security'
        $category.Priority = 'Critical'
    }
    elseif ($funcName -match 'performance|optimize|cache|batch') {
        $category.Category = 'Performance'
        $category.Priority = 'Medium'
    }
    elseif ($funcName -match 'connect|import|export|sync') {
        $category.Category = 'Integration'
        $category.Priority = 'Medium'
    }
    
    return $category
}

function Add-ToRainbowTable {
    param($feature)
    
    switch ($feature.Category) {
        'Core' { $global:FeatureRainbowTable.CoreFeatures += $feature }
        'UI' { $global:FeatureRainbowTable.UI_Features += $feature }
        'Security' { $global:FeatureRainbowTable.SecurityFeatures += $feature }
        'Performance' { $global:FeatureRainbowTable.PerformanceFeatures += $feature }
        'Integration' { $global:FeatureRainbowTable.IntegrationFeatures += $feature }
        Default { $global:FeatureRainbowTable.OptionalFeatures += $feature }
    }
}

function Extract-CLIModes {
    param($content)
    
    $modes = @()
    $switchMatches = [regex]::Matches($content, 'switch\s*\([^)]+\)\s*{([^}]+)}', 'Multiline')
    
    foreach ($match in $switchMatches) {
        $switchBody = $match.Groups[1].Value
        $caseMatches = [regex]::Matches($switchBody, '"([^"]+)"', 'Multiline')
        
        foreach ($caseMatch in $caseMatches) {
            $modes += $caseMatch.Groups[1].Value
        }
    }
    
    return $modes
}

function Infer-VariableType {
    param($value)
    
    if ($value -match '^\d+$') { return 'Int' }
    elseif ($value -match '^\d+\.\d+$') { return 'Double' }
    elseif ($value -match '^["''].*["'']$') { return 'String' }
    elseif ($value -match '^@\(') { return 'Array' }
    elseif ($value -match '^\$') { return 'VariableReference' }
    else { return 'Unknown' }
}

function Determine-Scope {
    param($content, $position)
    
    $beforeContent = $content.Substring(0, $position)
    $lines = $beforeContent -split "`n"
    
    # Check if inside a function
    for ($i = $lines.Count - 1; $i -ge 0; $i--) {
        if ($lines[$i] -match '^function\s+\w+') {
            return "Function:$($matches[1])"
        }
    }
    
    return 'Global'
}

function Test-IsConstant {
    param($value)
    
    return $value -match '^["''].*["'']$' -or $value -match '^\d+$'
}

function Test-IsSecureString {
    param($value)
    
    return $value -match 'ConvertTo-SecureString|AsSecureString'
}

function Invoke-Ollama {
    param (
        [string]$Prompt,
        [string]$Model = "llama3"
    )

    Write-Host "🤖 Sending prompt to Ollama AI..." -ForegroundColor Cyan

    # Create a new runspace for asynchronous execution
    $runspace = [runspacefactory]::CreateRunspace()
    $runspace.ApartmentState = "STA"
    $runspace.ThreadOptions = "ReuseThread"
    $runspace.Open()

    # Define the script block to execute in the runspace
    $scriptBlock = {
        param ($Prompt, $Model)

        try {
            # Simulate AI call (replace with actual API call)
            Start-Sleep -Seconds 5  # Simulate delay
            $response = "AI Response for prompt: $Prompt using model: $Model"

            # Return the response
            return $response
        } catch {
            Write-Host "❌ Error in AI call: $_" -ForegroundColor Red
            return $null
        }
    }

    # Pass parameters to the runspace
    $runspace.SessionStateProxy.SetVariable("Prompt", $Prompt)
    $runspace.SessionStateProxy.SetVariable("Model", $Model)

    # Create a PowerShell instance and assign the script block
    $powershell = [powershell]::Create()
    $powershell.Runspace = $runspace
    $powershell.AddScript($scriptBlock).AddArgument($Prompt).AddArgument($Model)

    # Run the script asynchronously
    $asyncResult = $powershell.BeginInvoke()

    # Display a loading message while waiting for the response
    while (-not $asyncResult.IsCompleted) {
        Write-Host "⏳ Waiting for AI response..." -NoNewline
        Start-Sleep -Seconds 1
        Write-Host "." -NoNewline
    }

    Write-Host ""  # Clear the loading line

    # Retrieve the result
    $result = $powershell.EndInvoke($asyncResult)

    # Clean up
    $powershell.Dispose()
    $runspace.Close()

    # Display the result
    if ($result) {
        Write-Host "🤖 AI Response: $result" -ForegroundColor Green
    } else {
        Write-Host "❌ Failed to get a response from AI." -ForegroundColor Red
    }
}

function Extract-ClassMethods {
    param($classBody)

    Write-Host "🔍 Extracting class methods..." -ForegroundColor Gray

    $methodMatches = [regex]::Matches($classBody, 'function\s+(\w+)\s*\(', 'Multiline')
    $methods = @()

    foreach ($match in $methodMatches) {
        $methods += @{
            Name = $match.Groups[1].Value
            StartLine = ($classBody.Substring(0, $match.Index) -split "`n").Count
        }
    }

    Write-Host "📊 Found $($methods.Count) methods" -ForegroundColor Green
    return $methods
}

# Ensure output directory exists
if (-not (Test-Path $OutputDir)) {
    Write-Host "📂 Creating output directory: $OutputDir" -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

# Execute the digestion engine
Start-SourceDigestion