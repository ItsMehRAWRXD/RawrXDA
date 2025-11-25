#requires -Version 5.1
<#
.SYNOPSIS
    Comprehensive validation script for RawrXD.ps1 without running the GUI
    
.DESCRIPTION
    Performs deep analysis of the PowerShell code to identify:
    - Syntax errors and parsing issues
    - Undefined variables and function references
    - Missing dependencies and assemblies
    - Security vulnerabilities
    - Performance bottlenecks
    - Best practice violations
    
.PARAMETER Path
    Path to the RawrXD.ps1 file to validate
    
.PARAMETER ReportPath
    Optional path to save the detailed validation report
    
.EXAMPLE
    .\Validate-RawrXD.ps1 -Path .\RawrXD.ps1
    
.EXAMPLE
    .\Validate-RawrXD.ps1 -Path .\RawrXD.ps1 -ReportPath .\validation_report.html
#>

param(
  [Parameter(Mandatory = $false)]
  [string]$Path = ".\RawrXD.ps1",
    
  [Parameter(Mandatory = $false)]
  [string]$ReportPath = ""
)

# Initialize validation results
$ValidationResults = @{
  Timestamp              = Get-Date
  TotalIssues            = 0
  CriticalIssues         = 0
  WarningIssues          = 0
  InfoIssues             = 0
  SyntaxErrors           = @()
  UndefinedVariables     = @()
  UndefinedFunctions     = @()
  MissingAssemblies      = @()
  SecurityIssues         = @()
  PerformanceIssues      = @()
  BestPracticeViolations = @()
  FunctionAnalysis       = @{}
  VariableAnalysis       = @{}
  OverallScore           = 0
}

Write-Host "🔍 Starting RawrXD.ps1 Validation..." -ForegroundColor Cyan
Write-Host "=" * 60

# Check if file exists
if (-not (Test-Path $Path)) {
  Write-Host "❌ File not found: $Path" -ForegroundColor Red
  exit 1
}

$scriptContent = Get-Content $Path -Raw
$scriptLines = Get-Content $Path
$lineCount = $scriptLines.Count

Write-Host "📄 File: $Path"
Write-Host "📊 Total Lines: $lineCount"
Write-Host ""

# 1. SYNTAX VALIDATION
Write-Host "🔸 1. Syntax Validation..." -ForegroundColor Yellow
try {
  $errors = $null
  $tokens = $null
  $ast = [System.Management.Automation.Language.Parser]::ParseInput($scriptContent, [ref]$tokens, [ref]$errors)
    
  if ($errors) {
    foreach ($error in $errors) {
      $ValidationResults.SyntaxErrors += @{
        Line     = $error.Extent.StartLineNumber
        Column   = $error.Extent.StartColumnNumber
        Message  = $error.Message
        Severity = "CRITICAL"
      }
      Write-Host "   ❌ Line $($error.Extent.StartLineNumber): $($error.Message)" -ForegroundColor Red
      $ValidationResults.CriticalIssues++
    }
  }
  else {
    Write-Host "   ✅ No syntax errors found" -ForegroundColor Green
  }
}
catch {
  Write-Host "   ❌ Failed to parse script: $_" -ForegroundColor Red
  $ValidationResults.CriticalIssues++
}

# 2. VARIABLE ANALYSIS
Write-Host "🔸 2. Variable Analysis..." -ForegroundColor Yellow
$definedVariables = @()
$referencedVariables = @()

# Extract variable definitions (script: and global: scoped)
$variableDefPattern = '\$(?:script:|global:)?([a-zA-Z_][a-zA-Z0-9_]*)\s*='
$variableDefMatches = [regex]::Matches($scriptContent, $variableDefPattern, 'IgnoreCase')
foreach ($match in $variableDefMatches) {
  $varName = $match.Groups[1].Value
  if ($varName -notin $definedVariables) {
    $definedVariables += $varName
  }
}

# Extract variable references
$variableRefPattern = '\$(?:script:|global:)?([a-zA-Z_][a-zA-Z0-9_]*)'
$variableRefMatches = [regex]::Matches($scriptContent, $variableRefPattern, 'IgnoreCase')
foreach ($match in $variableRefMatches) {
  $varName = $match.Groups[1].Value
  if ($varName -notin $referencedVariables) {
    $referencedVariables += $varName
  }
}

# Built-in variables to ignore
$builtinVars = @('_', 'args', 'error', 'false', 'true', 'null', 'pwd', 'home', 'host', 'pid', 'profile', 'pshome', 'psversion')

# Find undefined variables
$undefinedVars = $referencedVariables | Where-Object { 
  $_ -notin $definedVariables -and 
  $_ -notin $builtinVars -and 
  $_.Length -gt 1
}

foreach ($undefinedVar in $undefinedVars) {
  # Find line numbers where undefined variable is used
  $lineNumbers = @()
  for ($i = 0; $i -lt $scriptLines.Count; $i++) {
    if ($scriptLines[$i] -match "\`$$undefinedVar\b") {
      $lineNumbers += ($i + 1)
    }
  }
    
  $ValidationResults.UndefinedVariables += @{
    Variable = $undefinedVar
    Lines    = $lineNumbers
    Severity = "WARNING"
  }
  Write-Host "   ⚠️ Undefined variable: `$$undefinedVar (Lines: $($lineNumbers -join ', '))" -ForegroundColor Yellow
  $ValidationResults.WarningIssues++
}

$ValidationResults.VariableAnalysis = @{
  DefinedCount    = $definedVariables.Count
  ReferencedCount = $referencedVariables.Count
  UndefinedCount  = $undefinedVars.Count
}

Write-Host "   📊 Variables: $($definedVariables.Count) defined, $($referencedVariables.Count) referenced, $($undefinedVars.Count) undefined"

# 3. FUNCTION ANALYSIS
Write-Host "🔸 3. Function Analysis..." -ForegroundColor Yellow
$definedFunctions = @()
$referencedFunctions = @()

# Extract function definitions
$functionDefPattern = 'function\s+([a-zA-Z_][a-zA-Z0-9_-]*)'
$functionDefMatches = [regex]::Matches($scriptContent, $functionDefPattern, 'IgnoreCase')
foreach ($match in $functionDefMatches) {
  $funcName = $match.Groups[1].Value
  if ($funcName -notin $definedFunctions) {
    $definedFunctions += $funcName
  }
}

# Extract function calls
$functionCallPattern = '([a-zA-Z_][a-zA-Z0-9_-]*)\s*\('
$functionCallMatches = [regex]::Matches($scriptContent, $functionCallPattern, 'IgnoreCase')
foreach ($match in $functionCallMatches) {
  $funcName = $match.Groups[1].Value
  if ($funcName -notin $referencedFunctions) {
    $referencedFunctions += $funcName
  }
}

# Built-in cmdlets and functions to ignore
$builtinFunctions = @('Write-Host', 'Write-Output', 'Get-Content', 'Set-Content', 'Test-Path', 'Get-Item', 'Get-ChildItem', 'New-Object', 'Add-Type', 'Start-Process', 'Stop-Process', 'Get-Date', 'Start-Sleep', 'Out-Null', 'Select-Object', 'Where-Object', 'ForEach-Object', 'Sort-Object', 'Group-Object', 'Measure-Object', 'ConvertTo-Json', 'ConvertFrom-Json', 'Join-Path', 'Split-Path', 'Get-Random', 'Get-EventLog')

# Find undefined functions
$undefinedFunctions = $referencedFunctions | Where-Object { 
  $_ -notin $definedFunctions -and 
  $_ -notin $builtinFunctions -and
  $_.Length -gt 1 -and
  $_ -notmatch '^\d'
}

foreach ($undefinedFunc in $undefinedFunctions) {
  # Find line numbers where undefined function is used
  $lineNumbers = @()
  for ($i = 0; $i -lt $scriptLines.Count; $i++) {
    if ($scriptLines[$i] -match "$undefinedFunc\s*\(") {
      $lineNumbers += ($i + 1)
    }
  }
    
  $ValidationResults.UndefinedFunctions += @{
    Function = $undefinedFunc
    Lines    = $lineNumbers
    Severity = "WARNING"
  }
  Write-Host "   ⚠️ Undefined function: $undefinedFunc (Lines: $($lineNumbers -join ', '))" -ForegroundColor Yellow
  $ValidationResults.WarningIssues++
}

$ValidationResults.FunctionAnalysis = @{
  DefinedCount    = $definedFunctions.Count
  ReferencedCount = $referencedFunctions.Count
  UndefinedCount  = $undefinedFunctions.Count
}

Write-Host "   📊 Functions: $($definedFunctions.Count) defined, $($referencedFunctions.Count) referenced, $($undefinedFunctions.Count) undefined"

# 4. ASSEMBLY AND TYPE CHECKING
Write-Host "🔸 4. Assembly & Type Checking..." -ForegroundColor Yellow
$requiredAssemblies = @()
$missingAssemblies = @()

# Extract Add-Type calls
$addTypeMatches = [regex]::Matches($scriptContent, 'Add-Type\s+-AssemblyName\s+([^\s]+)', 'IgnoreCase')
foreach ($match in $addTypeMatches) {
  $assemblyName = $match.Groups[1].Value
  $requiredAssemblies += $assemblyName
    
  try {
    [System.Reflection.Assembly]::LoadWithPartialName($assemblyName) | Out-Null
    Write-Host "   ✅ Assembly available: $assemblyName" -ForegroundColor Green
  }
  catch {
    $ValidationResults.MissingAssemblies += @{
      Assembly = $assemblyName
      Severity = "CRITICAL"
    }
    Write-Host "   ❌ Missing assembly: $assemblyName" -ForegroundColor Red
    $ValidationResults.CriticalIssues++
  }
}

# Check for .NET types used
$dotNetTypes = @(
  'System.Windows.Forms',
  'System.Drawing',
  'System.IO',
  'System.Net',
  'System.Security.Cryptography',
  'System.Text',
  'System.Threading',
  'System.Diagnostics'
)

foreach ($type in $dotNetTypes) {
  if ($scriptContent -match "\[$type\." -or $scriptContent -match "New-Object $type\.") {
    try {
      Add-Type -AssemblyName $type -ErrorAction SilentlyContinue
      Write-Host "   ✅ .NET Type available: $type" -ForegroundColor Green
    }
    catch {
      Write-Host "   ⚠️ .NET Type might be missing: $type" -ForegroundColor Yellow
      $ValidationResults.WarningIssues++
    }
  }
}

# 5. SECURITY ANALYSIS
Write-Host "🔸 5. Security Analysis..." -ForegroundColor Yellow
$securityPatterns = @{
  'Invoke-Expression'                      = 'Potentially dangerous: Invoke-Expression usage'
  'iex\s'                                  = 'Potentially dangerous: iex alias usage'
  'cmd\.exe|cmd\s/c'                       = 'Command execution detected'
  'powershell\.exe.*-e\s'                  = 'Encoded PowerShell execution'
  'DownloadString|DownloadFile'            = 'Network download functionality'
  '\$env:temp|\$env:tmp'                   = 'Temporary directory usage'
  'ConvertFrom-SecureString.*-AsPlainText' = 'Potential credential exposure'
  'Start-Process.*-Credential'             = 'Process started with credentials'
}

foreach ($pattern in $securityPatterns.Keys) {
  $matches = [regex]::Matches($scriptContent, $pattern, 'IgnoreCase')
  if ($matches.Count -gt 0) {
    foreach ($match in $matches) {
      $lineNumber = ($scriptContent.Substring(0, $match.Index) -split "`n").Count
      $ValidationResults.SecurityIssues += @{
        Pattern     = $pattern
        Description = $securityPatterns[$pattern]
        Line        = $lineNumber
        Severity    = "HIGH"
      }
      Write-Host "   🔒 Security concern (Line $lineNumber): $($securityPatterns[$pattern])" -ForegroundColor Magenta
    }
    $ValidationResults.CriticalIssues++
  }
}

# 6. PERFORMANCE ANALYSIS
Write-Host "🔸 6. Performance Analysis..." -ForegroundColor Yellow
$performancePatterns = @{
  'Get-ChildItem.*-Recurse'       = 'Potentially slow recursive directory scan'
  'foreach.*Get-ChildItem'        = 'Inefficient nested file operations'
  'Start-Sleep\s+\d{2,}'          = 'Long sleep duration detected'
  '\|\s*Out-Host|\|\s*Write-Host' = 'Inefficient output to console in pipeline'
  'Where-Object.*\{.*\$_\.'       = 'Complex Where-Object that could use simplified syntax'
  'ForEach-Object.*\{.*\$_\.'     = 'Complex ForEach-Object operations'
}

foreach ($pattern in $performancePatterns.Keys) {
  $matches = [regex]::Matches($scriptContent, $pattern, 'IgnoreCase')
  if ($matches.Count -gt 0) {
    foreach ($match in $matches) {
      $lineNumber = ($scriptContent.Substring(0, $match.Index) -split "`n").Count
      $ValidationResults.PerformanceIssues += @{
        Pattern     = $pattern
        Description = $performancePatterns[$pattern]
        Line        = $lineNumber
        Severity    = "INFO"
      }
      Write-Host "   ⚡ Performance suggestion (Line $lineNumber): $($performancePatterns[$pattern])" -ForegroundColor Cyan
    }
    $ValidationResults.InfoIssues++
  }
}

# 7. BEST PRACTICES
Write-Host "🔸 7. Best Practices Check..." -ForegroundColor Yellow
$bestPracticePatterns = @{
  '^(?!.*#requires).*$'               = 'Missing #requires statement at top of script'
  'Write-Host(?!\s+-ForegroundColor)' = 'Write-Host without color specification'
  '\$\w+\s*=\s*\$null'                = 'Variable explicitly set to null (consider Remove-Variable)'
  'catch\s*\{\s*\}'                   = 'Empty catch block detected'
  'function\s+\w+(?!\s*\{\s*param)'   = 'Function without param block'
}

foreach ($pattern in $bestPracticePatterns.Keys) {
  $matches = [regex]::Matches($scriptContent, $pattern, 'IgnoreCase,Multiline')
  if ($matches.Count -gt 0) {
    foreach ($match in $matches) {
      $lineNumber = ($scriptContent.Substring(0, $match.Index) -split "`n").Count
      $ValidationResults.BestPracticeViolations += @{
        Pattern     = $pattern
        Description = $bestPracticePatterns[$pattern]
        Line        = $lineNumber
        Severity    = "INFO"
      }
    }
    Write-Host "   📋 Best practice suggestion: $($bestPracticePatterns[$pattern]) ($($matches.Count) occurrences)" -ForegroundColor Blue
    $ValidationResults.InfoIssues += $matches.Count
  }
}

# 8. CALCULATE OVERALL SCORE
Write-Host ""
Write-Host "🔸 8. Calculating Overall Score..." -ForegroundColor Yellow

$maxScore = 100
$scoreDeductions = 0

# Deductions
$scoreDeductions += $ValidationResults.CriticalIssues * 10
$scoreDeductions += $ValidationResults.WarningIssues * 3
$scoreDeductions += $ValidationResults.InfoIssues * 1

$ValidationResults.OverallScore = [Math]::Max(0, $maxScore - $scoreDeductions)

# Generate Summary
Write-Host ""
Write-Host "=" * 60
Write-Host "📋 VALIDATION SUMMARY" -ForegroundColor Green
Write-Host "=" * 60

Write-Host "🎯 Overall Score: $($ValidationResults.OverallScore)/100" -ForegroundColor $(if ($ValidationResults.OverallScore -ge 80) { "Green" } elseif ($ValidationResults.OverallScore -ge 60) { "Yellow" } else { "Red" })
Write-Host ""

Write-Host "📊 Issue Breakdown:"
Write-Host "   🔴 Critical Issues: $($ValidationResults.CriticalIssues)"
Write-Host "   🟡 Warning Issues: $($ValidationResults.WarningIssues)"
Write-Host "   🔵 Info Issues: $($ValidationResults.InfoIssues)"
Write-Host "   📈 Total Issues: $($ValidationResults.CriticalIssues + $ValidationResults.WarningIssues + $ValidationResults.InfoIssues)"

Write-Host ""
Write-Host "🔧 Component Analysis:"
Write-Host "   ✅ Syntax Errors: $($ValidationResults.SyntaxErrors.Count)"
Write-Host "   📝 Undefined Variables: $($ValidationResults.UndefinedVariables.Count)"
Write-Host "   🔧 Undefined Functions: $($ValidationResults.UndefinedFunctions.Count)"
Write-Host "   📦 Missing Assemblies: $($ValidationResults.MissingAssemblies.Count)"
Write-Host "   🔒 Security Issues: $($ValidationResults.SecurityIssues.Count)"
Write-Host "   ⚡ Performance Issues: $($ValidationResults.PerformanceIssues.Count)"
Write-Host "   📋 Best Practice Violations: $($ValidationResults.BestPracticeViolations.Count)"

# Recommendations
Write-Host ""
Write-Host "💡 RECOMMENDATIONS" -ForegroundColor Cyan
Write-Host "=" * 60

if ($ValidationResults.SyntaxErrors.Count -eq 0) {
  Write-Host "✅ Syntax: Script syntax is valid and should run without parsing errors"
}
else {
  Write-Host "❌ Syntax: Fix syntax errors before running the script"
}

if ($ValidationResults.UndefinedVariables.Count -eq 0) {
  Write-Host "✅ Variables: All referenced variables appear to be defined"
}
else {
  Write-Host "⚠️ Variables: Review undefined variable references - may cause runtime errors"
}

if ($ValidationResults.UndefinedFunctions.Count -eq 0) {
  Write-Host "✅ Functions: All referenced functions appear to be defined"
}
else {
  Write-Host "⚠️ Functions: Review undefined function calls - may cause runtime errors"
}

if ($ValidationResults.MissingAssemblies.Count -eq 0) {
  Write-Host "✅ Dependencies: All required assemblies appear to be available"
}
else {
  Write-Host "❌ Dependencies: Install missing assemblies before running"
}

$statusColor = if ($ValidationResults.OverallScore -ge 90) { "Green" } 
elseif ($ValidationResults.OverallScore -ge 75) { "Yellow" } 
else { "Red" }

$statusEmoji = if ($ValidationResults.OverallScore -ge 90) { "🎉" } 
elseif ($ValidationResults.OverallScore -ge 75) { "⚠️" } 
else { "❌" }

Write-Host ""
Write-Host "$statusEmoji OVERALL STATUS: " -NoNewline
if ($ValidationResults.OverallScore -ge 90) {
  Write-Host "EXCELLENT - Script should run smoothly" -ForegroundColor Green
}
elseif ($ValidationResults.OverallScore -ge 75) {
  Write-Host "GOOD - Minor issues may occur during runtime" -ForegroundColor Yellow
}
elseif ($ValidationResults.OverallScore -ge 50) {
  Write-Host "FAIR - Several issues may cause runtime problems" -ForegroundColor Yellow
}
else {
  Write-Host "POOR - Major issues likely to cause runtime failures" -ForegroundColor Red
}

# Generate HTML Report if requested
if ($ReportPath) {
  Write-Host ""
  Write-Host "📄 Generating detailed HTML report..." -ForegroundColor Cyan
    
  $htmlReport = @"
<!DOCTYPE html>
<html>
<head>
    <title>RawrXD Validation Report</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .header { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 20px; border-radius: 10px; margin-bottom: 30px; }
        .score { font-size: 48px; font-weight: bold; text-align: center; margin: 20px 0; }
        .score.excellent { color: #28a745; }
        .score.good { color: #ffc107; }
        .score.poor { color: #dc3545; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .critical { background: #f8d7da; border-color: #f5c6cb; }
        .warning { background: #fff3cd; border-color: #ffeaa7; }
        .info { background: #d1ecf1; border-color: #bee5eb; }
        .success { background: #d4edda; border-color: #c3e6cb; }
        .issue { margin: 5px 0; padding: 8px; background: rgba(255,255,255,0.7); border-radius: 3px; }
        .stats { display: flex; justify-content: space-between; margin: 20px 0; }
        .stat-box { text-align: center; padding: 15px; background: #f8f9fa; border-radius: 5px; }
        table { width: 100%; border-collapse: collapse; margin: 15px 0; }
        th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background: #f8f9fa; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🔍 RawrXD Validation Report</h1>
            <p>Generated: $($ValidationResults.Timestamp)</p>
            <p>File: $Path</p>
        </div>
        
        <div class="score $(if ($ValidationResults.OverallScore -ge 80) { 'excellent' } elseif ($ValidationResults.OverallScore -ge 60) { 'good' } else { 'poor' })">
            $($ValidationResults.OverallScore)/100
        </div>
        
        <div class="stats">
            <div class="stat-box">
                <h3>$($ValidationResults.CriticalIssues)</h3>
                <p>Critical Issues</p>
            </div>
            <div class="stat-box">
                <h3>$($ValidationResults.WarningIssues)</h3>
                <p>Warnings</p>
            </div>
            <div class="stat-box">
                <h3>$($ValidationResults.InfoIssues)</h3>
                <p>Info Items</p>
            </div>
            <div class="stat-box">
                <h3>$lineCount</h3>
                <p>Total Lines</p>
            </div>
        </div>
"@

  # Add detailed sections for each issue type
  if ($ValidationResults.SyntaxErrors.Count -gt 0) {
    $htmlReport += "<div class='section critical'><h3>🚨 Syntax Errors</h3>"
    foreach ($error in $ValidationResults.SyntaxErrors) {
      $htmlReport += "<div class='issue'>Line $($error.Line): $($error.Message)</div>"
    }
    $htmlReport += "</div>"
  }
    
  if ($ValidationResults.UndefinedVariables.Count -gt 0) {
    $htmlReport += "<div class='section warning'><h3>⚠️ Undefined Variables</h3>"
    foreach ($var in $ValidationResults.UndefinedVariables) {
      $htmlReport += "<div class='issue'>Variable: `$$($var.Variable) (Lines: $($var.Lines -join ', '))</div>"
    }
    $htmlReport += "</div>"
  }
    
  if ($ValidationResults.UndefinedFunctions.Count -gt 0) {
    $htmlReport += "<div class='section warning'><h3>⚠️ Undefined Functions</h3>"
    foreach ($func in $ValidationResults.UndefinedFunctions) {
      $htmlReport += "<div class='issue'>Function: $($func.Function) (Lines: $($func.Lines -join ', '))</div>"
    }
    $htmlReport += "</div>"
  }
    
  $htmlReport += @"
        </div>
    </body>
    </html>
"@
    
  $htmlReport | Set-Content $ReportPath -Encoding UTF8
  Write-Host "✅ Report saved to: $ReportPath" -ForegroundColor Green
    
  if (Get-Command Start-Process -ErrorAction SilentlyContinue) {
    $openReport = Read-Host "Open report in browser? (Y/N)"
    if ($openReport -eq 'Y' -or $openReport -eq 'y') {
      Start-Process $ReportPath
    }
  }
}

Write-Host ""
Write-Host "🎯 Validation Complete!" -ForegroundColor Green
Write-Host "Run time: $((Get-Date) - $ValidationResults.Timestamp)" -ForegroundColor Gray

# Return validation results for programmatic use
return $ValidationResults