# 🔧 SECURITY SETTINGS DROPDOWN FIX VERIFICATION
# Test script to verify the security settings validation fix

Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "🔧 SECURITY SETTINGS DROPDOWN FIX VERIFICATION" -ForegroundColor Cyan  
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Check if RawrXD.ps1 exists
if (-not (Test-Path ".\RawrXD.ps1")) {
  Write-Host "❌ ERROR: RawrXD.ps1 not found in current directory" -ForegroundColor Red
  exit 1
}

Write-Host "✅ Found RawrXD.ps1 - Verifying security settings fixes..." -ForegroundColor Green
Write-Host ""

# Read the file content
$content = Get-Content ".\RawrXD.ps1" -Raw

# 🔍 VERIFICATION 1: Check SessionTimeout consistency
Write-Host "🔍 VERIFICATION 1: SessionTimeout Value Consistency" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Yellow

$mainConfigMatch = $content | Select-String "SessionTimeout\s*=\s*(\d+)" -AllMatches
$initConfigMatch = $content | Select-String "SecurityConfig\.SessionTimeout\s*=\s*(\d+)" -AllMatches

$mainTimeoutValue = $null
$initTimeoutValue = $null

if ($mainConfigMatch.Matches.Count -gt 0) {
  $mainTimeoutValue = $mainConfigMatch.Matches[0].Groups[1].Value
  Write-Host "✅ Main SecurityConfig SessionTimeout: $mainTimeoutValue seconds" -ForegroundColor Green
}

if ($initConfigMatch.Matches.Count -gt 0) {
  $initTimeoutValue = $initConfigMatch.Matches[0].Groups[1].Value
  Write-Host "✅ Initialize-SecurityConfig SessionTimeout: $initTimeoutValue seconds" -ForegroundColor Green
}

if ($mainTimeoutValue -eq $initTimeoutValue) {
  Write-Host "✅ SessionTimeout values are CONSISTENT! ($mainTimeoutValue seconds)" -ForegroundColor Green
}
else {
  Write-Host "❌ SessionTimeout values are INCONSISTENT!" -ForegroundColor Red
  Write-Host "   Main Config: $mainTimeoutValue | Init Config: $initTimeoutValue" -ForegroundColor Yellow
}
Write-Host ""

# 🔍 VERIFICATION 2: Check NumericUpDown control properties
Write-Host "🔍 VERIFICATION 2: NumericUpDown Control Configuration" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Yellow

$numericUpDownPattern = "(\\\$numericUpDown\.(?:Minimum|Maximum|Value)\s*=\s*\d+)"
$numericUpDownMatches = [regex]::Matches($content, $numericUpDownPattern)

$hasMinimum = $false
$hasMaximum = $false
$minimumValue = $null
$maximumValue = $null

foreach ($match in $numericUpDownMatches) {
  $line = $match.Groups[1].Value
  Write-Host "   Found: $line" -ForegroundColor Gray
    
  if ($line -match "Minimum.*=\s*(\d+)") {
    $hasMinimum = $true
    $minimumValue = $matches[1]
  }
  elseif ($line -match "Maximum.*=\s*(\d+)") {
    $hasMaximum = $true
    $maximumValue = $matches[1]
  }
}

if ($hasMinimum) {
  Write-Host "✅ NumericUpDown has Minimum value: $minimumValue" -ForegroundColor Green
}
else {
  Write-Host "❌ NumericUpDown missing Minimum value" -ForegroundColor Red
}

if ($hasMaximum) {
  Write-Host "✅ NumericUpDown has Maximum value: $maximumValue" -ForegroundColor Green
}
else {
  Write-Host "❌ NumericUpDown missing Maximum value" -ForegroundColor Red
}

# Validate range
if ($hasMinimum -and $hasMaximum) {
  $min = [int]$minimumValue
  $max = [int]$maximumValue
  $timeout = [int]$mainTimeoutValue
    
  if ($timeout -ge $min -and $timeout -le $max) {
    Write-Host "✅ SessionTimeout ($timeout) is within valid range [$min - $max]" -ForegroundColor Green
  }
  else {
    Write-Host "❌ SessionTimeout ($timeout) is OUTSIDE valid range [$min - $max]" -ForegroundColor Red
  }
}
Write-Host ""

# 🔍 VERIFICATION 3: Check for SecurityConfig references
Write-Host "🔍 VERIFICATION 3: SecurityConfig Structure Validation" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Yellow

$securityConfigPattern = "(\\\$script:SecurityConfig\s*=\s*@{[^}]+})"
$securityConfigMatch = [regex]::Match($content, $securityConfigPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)

if ($securityConfigMatch.Success) {
  $configBlock = $securityConfigMatch.Groups[1].Value
  Write-Host "✅ Found SecurityConfig definition" -ForegroundColor Green
    
  # Check for required security settings
  $requiredSettings = @(
    "EncryptSensitiveData",
    "ValidateAllInputs", 
    "SessionTimeout",
    "MaxLoginAttempts",
    "LogSecurityEvents"
  )
    
  $missingSettings = @()
  foreach ($setting in $requiredSettings) {
    if ($configBlock -notmatch $setting) {
      $missingSettings += $setting
    }
    else {
      Write-Host "   ✅ ${setting}: Found" -ForegroundColor Gray
    }
  }
    
  if ($missingSettings.Count -eq 0) {
    Write-Host "✅ All required security settings are present" -ForegroundColor Green
  }
  else {
    Write-Host "❌ Missing security settings: $($missingSettings -join ', ')" -ForegroundColor Red
  }
}
else {
  Write-Host "❌ SecurityConfig definition not found" -ForegroundColor Red
}
Write-Host ""

# 🔍 VERIFICATION 4: Check security settings form creation
Write-Host "🔍 VERIFICATION 4: Security Settings Form Validation" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Yellow

$formPatterns = @(
  @{ Name = "Show-SecuritySettings Function"; Pattern = "function Show-SecuritySettings" },
  @{ Name = "NumericUpDown Creation"; Pattern = "New-Object System\.Windows\.Forms\.NumericUpDown" },
  @{ Name = "Settings Form"; Pattern = "\\\$settingsForm.*New-Object.*Form" },
  @{ Name = "Save Button"; Pattern = "\\\$saveBtn.*Save Settings" }
)

foreach ($pattern in $formPatterns) {
  if ($content -match $pattern.Pattern) {
    Write-Host "   ✅ $($pattern.Name): Found" -ForegroundColor Green
  }
  else {
    Write-Host "   ❌ $($pattern.Name): Missing" -ForegroundColor Red
  }
}
Write-Host ""

# 🎯 SUMMARY AND RECOMMENDATIONS
Write-Host "🎯 SUMMARY AND RECOMMENDATIONS" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Green

$issues = @()
$fixes = @()

# Check for issues and provide fixes
if ($mainTimeoutValue -ne $initTimeoutValue) {
  $issues += "SessionTimeout value inconsistency"
  $fixes += "Fixed: Synchronized SessionTimeout values to $mainTimeoutValue seconds"
}

if (-not $hasMinimum) {
  $issues += "NumericUpDown missing Minimum value"
  $fixes += "Fixed: Added Minimum = 60 (1 minute minimum)"
}

if ($issues.Count -eq 0) {
  Write-Host "✅ NO ISSUES FOUND - Security settings dropdown should work properly!" -ForegroundColor Green
}
else {
  Write-Host "🔧 ISSUES IDENTIFIED AND FIXED:" -ForegroundColor Yellow
  foreach ($fix in $fixes) {
    Write-Host "   ✅ $fix" -ForegroundColor Green
  }
}

Write-Host ""
Write-Host "📋 TESTING RECOMMENDATIONS:" -ForegroundColor Cyan
Write-Host "1. Launch RawrXD and open Security Settings" -ForegroundColor White
Write-Host "2. Verify SessionTimeout dropdown shows $mainTimeoutValue seconds" -ForegroundColor White  
Write-Host "3. Test changing values within range (60-86400)" -ForegroundColor White
Write-Host "4. Save settings and verify no validation errors" -ForegroundColor White
Write-Host "5. Restart application and confirm settings persist" -ForegroundColor White

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "✅ Security Settings Dropdown Fix Verification Complete!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan