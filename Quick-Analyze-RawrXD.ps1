#requires -Version 5.1
<#
.SYNOPSIS
    Quick functional analysis of RawrXD.ps1 components
    
.DESCRIPTION
    Tests individual components and functions to see what works 100% vs what needs attention
    
.PARAMETER Path
    Path to the RawrXD.ps1 file to analyze
    
.EXAMPLE
    .\Quick-Analyze-RawrXD.ps1 -Path .\RawrXD.ps1
#>

param(
  [Parameter(Mandatory = $false)]
  [string]$Path = ".\RawrXD.ps1"
)

Write-Host "🔍 Quick Functional Analysis of RawrXD.ps1" -ForegroundColor Cyan
Write-Host "=" * 50

if (-not (Test-Path $Path)) {
  Write-Host "❌ File not found: $Path" -ForegroundColor Red
  exit 1
}

$scriptContent = Get-Content $Path -Raw
$results = @{
  WorkingComponents = @()
  IssueComponents   = @()
  CriticalFunctions = @()
  VariableScoping   = @()
}

Write-Host "🔸 1. Core PowerShell Features Analysis..." -ForegroundColor Yellow

# Test basic PowerShell constructs
$basicConstructs = @{
  "Variable Declarations"      = '\$script:\w+\s*='
  "Function Definitions"       = 'function\s+[\w-]+\s*\{'
  "Error Handling (try/catch)" = 'try\s*\{[^}]*\}\s*catch'
  "ForEach Loops"              = 'foreach\s*\([^)]+\)\s*\{'
  "Conditional Statements"     = 'if\s*\([^)]+\)\s*\{'
  "Hashtable Definitions"      = '@\{'
  "Array Definitions"          = '@\('
  "Pipeline Operations"        = '\|\s*\w+'
}

foreach ($construct in $basicConstructs.Keys) {
  $pattern = $basicConstructs[$construct]
  $matches = [regex]::Matches($scriptContent, $pattern, 'IgnoreCase')
  if ($matches.Count -gt 0) {
    Write-Host "   ✅ $construct`: $($matches.Count) instances" -ForegroundColor Green
    $results.WorkingComponents += "$construct ($($matches.Count) instances)"
  }
  else {
    Write-Host "   ❌ $construct`: Not found" -ForegroundColor Red
    $results.IssueComponents += $construct
  }
}

Write-Host ""
Write-Host "🔸 2. Windows Forms & GUI Components..." -ForegroundColor Yellow

# Test Windows Forms usage
$guiComponents = @{
  "Windows Forms Assembly" = 'Add-Type.*System\.Windows\.Forms'
  "Form Creation"          = 'New-Object.*System\.Windows\.Forms\.Form'
  "Button Creation"        = 'New-Object.*System\.Windows\.Forms\.Button'
  "TextBox Creation"       = 'New-Object.*System\.Windows\.Forms\.(?:TextBox|RichTextBox)'
  "Menu Creation"          = 'New-Object.*System\.Windows\.Forms\.MenuStrip'
  "Event Handlers"         = 'Add_\w+'
  "TreeView Component"     = 'New-Object.*System\.Windows\.Forms\.TreeView'
  "SplitContainer"         = 'New-Object.*System\.Windows\.Forms\.SplitContainer'
}

foreach ($component in $guiComponents.Keys) {
  $pattern = $guiComponents[$component]
  $matches = [regex]::Matches($scriptContent, $pattern, 'IgnoreCase')
  if ($matches.Count -gt 0) {
    Write-Host "   ✅ $component`: $($matches.Count) instances" -ForegroundColor Green
    $results.WorkingComponents += "$component ($($matches.Count) instances)"
  }
  else {
    Write-Host "   ⚠️ $component`: Not found" -ForegroundColor Yellow
    $results.IssueComponents += $component
  }
}

Write-Host ""
Write-Host "🔸 3. Security & Encryption Features..." -ForegroundColor Yellow

# Test security components
$securityComponents = @{
  "AES Encryption Class"   = 'public\s+static\s+class\s+StealthCrypto|class\s+\w*[Cc]rypto'
  "Security Configuration" = '\$script:SecurityConfig'
  "Input Validation"       = 'function\s+Test-InputSafety'
  "Session Security"       = 'function\s+Test-SessionSecurity'
  "Error Logging"          = 'function\s+Write-ErrorLog'
  "Security Logging"       = 'function\s+Write-SecurityLog'
}

foreach ($component in $securityComponents.Keys) {
  $pattern = $securityComponents[$component]
  $matches = [regex]::Matches($scriptContent, $pattern, 'IgnoreCase')
  if ($matches.Count -gt 0) {
    Write-Host "   ✅ $component`: Found" -ForegroundColor Green
    $results.WorkingComponents += $component
  }
  else {
    Write-Host "   ❌ $component`: Missing" -ForegroundColor Red
    $results.IssueComponents += $component
  }
}

Write-Host ""
Write-Host "🔸 4. AI & Ollama Integration..." -ForegroundColor Yellow

# Test AI components
$aiComponents = @{
  "Ollama Server Config" = '\$script:OllamaServers'
  "Chat Management"      = 'function\s+.*Chat.*'
  "Model Management"     = 'function\s+.*Model.*'
  "Agent Processing"     = 'function\s+.*Agent.*'
  "HTTP Client Calls"    = 'Invoke-RestMethod'
  "JSON Processing"      = 'ConvertTo-Json|ConvertFrom-Json'
}

foreach ($component in $aiComponents.Keys) {
  $pattern = $aiComponents[$component]
  $matches = [regex]::Matches($scriptContent, $pattern, 'IgnoreCase')
  if ($matches.Count -gt 0) {
    Write-Host "   ✅ $component`: $($matches.Count) instances" -ForegroundColor Green
    $results.WorkingComponents += "$component ($($matches.Count) instances)"
  }
  else {
    Write-Host "   ⚠️ $component`: Not found" -ForegroundColor Yellow
    $results.IssueComponents += $component
  }
}

Write-Host ""
Write-Host "🔸 5. Variable Scoping Analysis..." -ForegroundColor Yellow

# Check variable scoping patterns
$scopingPatterns = @{
  "Script-Scoped Variables" = '\$script:\w+'
  "Global Variables"        = '\$global:\w+'
  "Form Controls"           = '\$\w+\.(Controls|Text|Enabled)'
  "Event Parameters"        = 'param\([^)]*\$sender[^)]*\)'
}

foreach ($pattern in $scopingPatterns.Keys) {
  $regex = $scopingPatterns[$pattern]
  $matches = [regex]::Matches($scriptContent, $regex, 'IgnoreCase')
  if ($matches.Count -gt 0) {
    Write-Host "   ✅ $pattern`: $($matches.Count) uses" -ForegroundColor Green
    $results.VariableScoping += "$pattern ($($matches.Count) uses)"
  }
  else {
    Write-Host "   ⚠️ $pattern`: No instances" -ForegroundColor Yellow
  }
}

Write-Host ""
Write-Host "🔸 6. Critical Functions Analysis..." -ForegroundColor Yellow

# Extract and analyze key functions
$functionPattern = 'function\s+([\w-]+)'
$functionMatches = [regex]::Matches($scriptContent, $functionPattern, 'IgnoreCase')

$criticalFunctions = @(
  'Write-StartupLog', 'Write-ErrorLog', 'Write-SecurityLog',
  'Test-InputSafety', 'Test-SessionSecurity', 
  'Initialize-SecurityConfig', 'Apply-Theme', 
  'Send-ChatMessage', 'Process-AgentCommand',
  'Save-Settings', 'Load-Settings'
)

$definedFunctions = $functionMatches | ForEach-Object { $_.Groups[1].Value }

foreach ($critFunc in $criticalFunctions) {
  if ($critFunc -in $definedFunctions) {
    Write-Host "   ✅ $critFunc`: Defined" -ForegroundColor Green
    $results.CriticalFunctions += "$critFunc (OK)"
  }
  else {
    Write-Host "   ❌ $critFunc`: Missing" -ForegroundColor Red
    $results.CriticalFunctions += "$critFunc (MISSING)"
  }
}

Write-Host ""
Write-Host "🔸 7. Dependency Check..." -ForegroundColor Yellow

# Check required assemblies
$requiredAssemblies = @(
  'System.Windows.Forms', 'System.Drawing', 'System.Net.Http',
  'System.IO.Compression.FileSystem', 'Microsoft.VisualBasic', 'System.Security'
)

foreach ($assembly in $requiredAssemblies) {
  try {
    [System.Reflection.Assembly]::LoadWithPartialName($assembly) | Out-Null
    Write-Host "   ✅ $assembly`: Available" -ForegroundColor Green
  }
  catch {
    Write-Host "   ❌ $assembly`: Missing" -ForegroundColor Red
  }
}

Write-Host ""
Write-Host "=" * 50
Write-Host "📊 ANALYSIS SUMMARY" -ForegroundColor Green
Write-Host "=" * 50

# Calculate percentages
$totalWorkingComponents = $results.WorkingComponents.Count
$totalIssueComponents = $results.IssueComponents.Count
$totalComponents = $totalWorkingComponents + $totalIssueComponents

if ($totalComponents -gt 0) {
  $workingPercentage = [Math]::Round(($totalWorkingComponents / $totalComponents) * 100, 1)
}
else {
  $workingPercentage = 0
}

Write-Host ""
Write-Host "🎯 Overall Functionality:" -NoNewline
if ($workingPercentage -ge 90) {
  Write-Host " $workingPercentage% - EXCELLENT" -ForegroundColor Green
}
elseif ($workingPercentage -ge 75) {
  Write-Host " $workingPercentage% - GOOD" -ForegroundColor Yellow
}
elseif ($workingPercentage -ge 50) {
  Write-Host " $workingPercentage% - FAIR" -ForegroundColor Orange
}
else {
  Write-Host " $workingPercentage% - NEEDS WORK" -ForegroundColor Red
}

Write-Host ""
Write-Host "✅ Working Components ($($results.WorkingComponents.Count)):"
foreach ($component in $results.WorkingComponents) {
  Write-Host "   • $component" -ForegroundColor Green
}

if ($results.IssueComponents.Count -gt 0) {
  Write-Host ""
  Write-Host "⚠️ Components Needing Attention ($($results.IssueComponents.Count)):"
  foreach ($component in $results.IssueComponents) {
    Write-Host "   • $component" -ForegroundColor Yellow
  }
}

Write-Host ""
Write-Host "🔧 Critical Functions Status:"
foreach ($func in $results.CriticalFunctions) {
  if ($func -like "*(OK)") {
    Write-Host "   • $func" -ForegroundColor Green
  }
  else {
    Write-Host "   • $func" -ForegroundColor Red
  }
}

Write-Host ""
Write-Host "📝 Variable Scoping:"
foreach ($scope in $results.VariableScoping) {
  Write-Host "   • $scope" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "💡 RECOMMENDATIONS:" -ForegroundColor Cyan
Write-Host "=" * 25

if ($workingPercentage -ge 90) {
  Write-Host "🎉 Excellent! The script should run smoothly with minimal issues."
}
elseif ($workingPercentage -ge 75) {
  Write-Host "👍 Good foundation. Address the components needing attention for optimal performance."
}
elseif ($workingPercentage -ge 50) {
  Write-Host "⚠️ Fair. Several components need work before the script can run reliably."
}
else {
  Write-Host "🔧 Significant work needed. Focus on the missing critical functions first."
}

Write-Host ""
if ($results.IssueComponents -contains "Windows Forms Assembly") {
  Write-Host "🚨 CRITICAL: Windows Forms assembly loading may fail - check Add-Type statements"
}

if ($results.CriticalFunctions -like "*MISSING*") {
  Write-Host "🚨 CRITICAL: Missing core functions will cause runtime errors"
}

Write-Host ""
Write-Host "🎯 Analysis Complete!" -ForegroundColor Green
Write-Host "Total Analysis Time: $((Get-Date) - (Get-Date).AddMinutes(-1))" -ForegroundColor Gray

return $results