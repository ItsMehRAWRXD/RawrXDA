# 🔧 FIX AGENTIC TEST SCRIPT ERRORS
# Fixes the date calculation and color formatting issues in Full-Agentic-Test.ps1

Write-Host "🔧 FIXING AGENTIC TEST SCRIPT ERRORS..." -ForegroundColor Green

$content = Get-Content ".\Full-Agentic-Test.ps1" -Raw

# Fix 1: Date calculation syntax error
Write-Host "1. Fixing Get-Date subtraction syntax..." -ForegroundColor Yellow

# The issue is that Get-Date - $script:TestStartTime is being parsed incorrectly
# Need to wrap (Get-Date) in parentheses for proper calculation
$content = $content -replace 'Get-Date - \$script:TestStartTime', '(Get-Date) - $script:TestStartTime'

# Fix 2: Ensure color variables are properly initialized
Write-Host "2. Adding color validation in Write-TestResult..." -ForegroundColor Yellow

# Add color validation to prevent null color issues
$oldWriteTestResult = @'
  $color = $colorMap[$normalizedStatus]
  if (-not $color) { $color = "Gray" }
'@

$newWriteTestResult = @'
  $color = $colorMap[$normalizedStatus]
  if (-not $color -or [string]::IsNullOrEmpty($color)) { $color = "Gray" }
'@

$content = $content.Replace($oldWriteTestResult, $newWriteTestResult)

# Fix 3: Add error handling around agent simulation
Write-Host "3. Improving agent simulation error handling..." -ForegroundColor Yellow

# Find and improve the agent simulation try-catch block
$oldAgentSimulation = @'
  catch {
    Write-TestResult "Agent Command Simulation" "FAIL" "Error during simulation: $_" "AGENT_SIMULATION"
  }
'@

$newAgentSimulation = @'
  catch {
    $errorMsg = $_.Exception.Message -replace '"', "'"
    Write-TestResult "Agent Command Simulation" "FAIL" "Error during simulation: $errorMsg" "AGENT_SIMULATION"
  }
'@

$content = $content.Replace($oldAgentSimulation, $newAgentSimulation)

# Fix 4: Improve report generation section with better error handling
Write-Host "4. Adding error handling to report generation..." -ForegroundColor Yellow

# Find the problematic report generation section
$oldReportGen = @'
**Test Duration**: $([math]::Round((Get-Date - $script:TestStartTime).TotalSeconds, 2)) seconds
'@

$newReportGen = @'
**Test Duration**: $([math]::Round(((Get-Date) - $script:TestStartTime).TotalSeconds, 2)) seconds
'@

$content = $content.Replace($oldReportGen, $newReportGen)

# Fix 5: Fix the final duration calculation
$oldDurationCalc = @'
  Write-Host "Duration: $([math]::Round((Get-Date - $script:TestStartTime).TotalSeconds, 2)) seconds" -ForegroundColor Gray
'@

$newDurationCalc = @'
  Write-Host "Duration: $([math]::Round(((Get-Date) - $script:TestStartTime).TotalSeconds, 2)) seconds" -ForegroundColor Gray
'@

$content = $content.Replace($oldDurationCalc, $newDurationCalc)

# Fix 6: Add null checking for variables that might be null
Write-Host "5. Adding null checks for critical variables..." -ForegroundColor Yellow

# Add better initialization at the top
$initSection = @'
$script:TestStartTime = Get-Date
$script:TotalTests = 0
$script:PassedTests = 0
$script:FailedTests = 0
$script:WarningTests = 0
$script:InfoTests = 0
'@

# Find where variables are initialized and enhance them
$oldInit = '$script:TestStartTime = Get-Date'
$newInit = @'
$script:TestStartTime = Get-Date
$script:TotalTests = 0
$script:PassedTests = 0
$script:FailedTests = 0
$script:WarningTests = 0
$script:InfoTests = 0

# Function to safely format colors
function Get-SafeColor {
    param([string]$Color)
    $validColors = @("Black", "DarkBlue", "DarkGreen", "DarkCyan", "DarkRed", "DarkMagenta", "DarkYellow", "Gray", "DarkGray", "Blue", "Green", "Cyan", "Red", "Magenta", "Yellow", "White")
    if ($Color -and $Color -in $validColors) {
        return $Color
    }
    return "Gray"
}
'@

$content = $content.Replace($oldInit, $newInit)

# Fix 7: Update Write-TestResult to use safe color function
$oldColorUsage = @'
  Write-Host "$icon [$Category] $TestName" -ForegroundColor $color
  if ($Details) {
    Write-Host "   └─ $Details" -ForegroundColor Gray
  }
'@

$newColorUsage = @'
  $safeColor = Get-SafeColor $color
  Write-Host "$icon [$Category] $TestName" -ForegroundColor $safeColor
  if ($Details) {
    Write-Host "   └─ $Details" -ForegroundColor Gray
  }
'@

$content = $content.Replace($oldColorUsage, $newColorUsage)

# Backup and save the fixed version
Write-Host "📝 Creating backup and saving fixed version..." -ForegroundColor Cyan
Copy-Item ".\Full-Agentic-Test.ps1" ".\Full-Agentic-Test-backup.ps1"
Set-Content ".\Full-Agentic-Test.ps1" $content

Write-Host ""
Write-Host "✅ AGENTIC TEST SCRIPT FIXES COMPLETED!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "🔧 FIXES APPLIED:" -ForegroundColor Cyan
Write-Host "   ✅ Fixed Get-Date subtraction syntax with proper parentheses" -ForegroundColor White
Write-Host "   ✅ Added color validation to prevent null ForegroundColor errors" -ForegroundColor White
Write-Host "   ✅ Improved agent simulation error handling" -ForegroundColor White
Write-Host "   ✅ Enhanced report generation with better date handling" -ForegroundColor White
Write-Host "   ✅ Added safe color function for robust color handling" -ForegroundColor White
Write-Host "   ✅ Initialized all test counter variables" -ForegroundColor White
Write-Host ""
Write-Host "📝 Files Modified:" -ForegroundColor Yellow
Write-Host "   • Full-Agentic-Test.ps1 (updated with fixes)" -ForegroundColor Gray
Write-Host "   • Full-Agentic-Test-backup.ps1 (original backup)" -ForegroundColor Gray
Write-Host ""
Write-Host "🎯 NEXT STEPS:" -ForegroundColor Green
Write-Host "1. Run .\Full-Agentic-Test.ps1 to verify fixes work" -ForegroundColor White
Write-Host "2. Check that no more date calculation errors occur" -ForegroundColor White
Write-Host "3. Verify agent simulation runs without color errors" -ForegroundColor White
Write-Host "4. Confirm test report generates successfully" -ForegroundColor White
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "✅ Ready to run fixed agentic test!" -ForegroundColor Green