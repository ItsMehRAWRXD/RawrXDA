# Build Recovery Script
# Restores the build environment and fixes common issues

param(
    [Parameter()]
    [ValidateSet('Full', 'Dependencies', 'Config', 'Clean')]
    [string]$RecoveryType = 'Full',
    
    [Parameter()]
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$ProjectRoot = "D:\MyCopilot-IDE"

function Write-RecoveryHeader {
    param([string]$Message)
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  $Message" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
}

function Write-RecoveryStep {
    param([string]$Message, [string]$Status = "Info")
    $color = switch ($Status) {
        "Success" { "Green" }
        "Warning" { "Yellow" }
        "Error" { "Red" }
        default { "White" }
    }
    Write-Host "  $Message" -ForegroundColor $color
}

function Test-Prerequisites {
    Write-RecoveryHeader "Checking Prerequisites"
    
    $checks = @{
        "Project Directory" = Test-Path $ProjectRoot
        "package.json" = Test-Path "$ProjectRoot\package.json"
        "Node.js" = $null -ne (Get-Command node -ErrorAction SilentlyContinue)
        "npm" = $null -ne (Get-Command npm -ErrorAction SilentlyContinue)
        "PowerShell 5.1+" = $PSVersionTable.PSVersion.Major -ge 5
    }
    
    $allPassed = $true
    foreach ($check in $checks.GetEnumerator()) {
        if ($check.Value) {
            Write-RecoveryStep "✓ $($check.Key)" "Success"
        }
        else {
            Write-RecoveryStep "✗ $($check.Key)" "Error"
            $allPassed = $false
        }
    }
    
    return $allPassed
}

function Repair-Dependencies {
    Write-RecoveryHeader "Repairing npm Dependencies"
    
    Set-Location $ProjectRoot
    
    # Remove problematic directories
    if (Test-Path "node_modules") {
        if ($Force) {
            Write-RecoveryStep "Removing node_modules..." "Warning"
            Remove-Item "node_modules" -Recurse -Force -ErrorAction SilentlyContinue
        }
        else {
            Write-RecoveryStep "node_modules exists (use -Force to remove)" "Warning"
        }
    }
    
    if (Test-Path "package-lock.json") {
        if ($Force) {
            Write-RecoveryStep "Removing package-lock.json..." "Warning"
            Remove-Item "package-lock.json" -Force
        }
    }
    
    # Clear npm cache
    Write-RecoveryStep "Clearing npm cache..."
    npm cache clean --force 2>&1 | Out-Null
    
    # Install dependencies
    Write-RecoveryStep "Installing dependencies (this may take several minutes)..."
    $installOutput = npm install 2>&1
    
    if ($LASTEXITCODE -eq 0) {
        Write-RecoveryStep "✓ Dependencies installed successfully" "Success"
        
        # Check for vulnerabilities
        Write-RecoveryStep "Running security audit..."
        $auditOutput = npm audit 2>&1
        $auditOutput | Out-String | Write-Host -ForegroundColor Gray
    }
    else {
        Write-RecoveryStep "✗ Dependency installation failed" "Error"
        $installOutput | Write-Host -ForegroundColor Red
        return $false
    }
    
    return $true
}

function Repair-Configuration {
    Write-RecoveryHeader "Repairing Configuration Files"
    
    Set-Location $ProjectRoot
    
    # Verify package.json
    if (Test-Path "package.json") {
        try {
            $packageJson = Get-Content "package.json" -Raw | ConvertFrom-Json
            
            # Check main entry point
            if ($packageJson.main -ne "electron-main.js") {
                Write-RecoveryStep "⚠ package.json main entry point is '$($packageJson.main)', expected 'electron-main.js'" "Warning"
                
                if ($Force) {
                    $packageJson.main = "electron-main.js"
                    $packageJson | ConvertTo-Json -Depth 100 | Set-Content "package.json"
                    Write-RecoveryStep "✓ Fixed main entry point" "Success"
                }
            }
            else {
                Write-RecoveryStep "✓ package.json main entry point correct" "Success"
            }
            
            # Check build configuration
            if ($packageJson.build) {
                Write-RecoveryStep "✓ Build configuration present" "Success"
            }
            else {
                Write-RecoveryStep "⚠ No build configuration in package.json" "Warning"
            }
        }
        catch {
            Write-RecoveryStep "✗ Error parsing package.json: $($_.Exception.Message)" "Error"
            return $false
        }
    }
    
    # Verify critical files
    $criticalFiles = @(
        "electron-main.js",
        "index.html",
        "Setup-UnifiedAgent.ps1",
        "UnifiedAgentProcessor-Fixed.psm1"
    )
    
    foreach ($file in $criticalFiles) {
        if (Test-Path $file) {
            Write-RecoveryStep "✓ $file exists" "Success"
        }
        else {
            Write-RecoveryStep "✗ $file missing" "Error"
        }
    }
    
    return $true
}

function Clean-BuildArtifacts {
    Write-RecoveryHeader "Cleaning Build Artifacts"
    
    Set-Location $ProjectRoot
    
    $buildDirs = Get-ChildItem -Directory -Filter "build-*" -ErrorAction SilentlyContinue
    
    if ($buildDirs.Count -gt 0) {
        Write-RecoveryStep "Found $($buildDirs.Count) build director$(if($buildDirs.Count -eq 1){'y'}else{'ies'})"
        
        if ($Force) {
            foreach ($dir in $buildDirs) {
                Write-RecoveryStep "Removing $($dir.Name)..." "Warning"
                Remove-Item $dir.FullName -Recurse -Force -ErrorAction SilentlyContinue
            }
            Write-RecoveryStep "✓ Build artifacts cleaned" "Success"
        }
        else {
            Write-RecoveryStep "Use -Force to remove build directories" "Warning"
            $buildDirs | ForEach-Object { Write-RecoveryStep "  - $($_.Name)" }
        }
    }
    else {
        Write-RecoveryStep "No build artifacts to clean" "Success"
    }
    
    # Clean electron cache
    $electronCache = "$env:LOCALAPPDATA\electron\Cache"
    if (Test-Path $electronCache) {
        if ($Force) {
            Write-RecoveryStep "Cleaning Electron cache..."
            Remove-Item $electronCache -Recurse -Force -ErrorAction SilentlyContinue
            Write-RecoveryStep "✓ Electron cache cleaned" "Success"
        }
    }
    
    return $true
}

function Test-BuildReadiness {
    Write-RecoveryHeader "Testing Build Readiness"
    
    Set-Location $ProjectRoot
    
    # Test PowerShell module
    Write-RecoveryStep "Testing UnifiedAgentProcessor module..."
    try {
        $testScript = @"
Import-Module '$ProjectRoot\UnifiedAgentProcessor-Fixed.psm1' -Force -ErrorAction Stop
`$processor = [UnifiedAgentProcessor]::new()
Write-Host "Module test: OK"
"@
        $testResult = powershell -NoProfile -Command $testScript 2>&1
        
        if ($testResult -match "Module test: OK") {
            Write-RecoveryStep "✓ PowerShell module loads correctly" "Success"
        }
        else {
            Write-RecoveryStep "✗ PowerShell module test failed" "Error"
            $testResult | Write-Host -ForegroundColor Red
            return $false
        }
    }
    catch {
        Write-RecoveryStep "✗ Error testing module: $($_.Exception.Message)" "Error"
        return $false
    }
    
    # Test Electron can start
    Write-RecoveryStep "Checking Electron installation..."
    if (Test-Path "node_modules\electron") {
        Write-RecoveryStep "✓ Electron installed" "Success"
    }
    else {
        Write-RecoveryStep "✗ Electron not installed" "Error"
        return $false
    }
    
    # Test electron-builder
    Write-RecoveryStep "Checking electron-builder..."
    if (Test-Path "node_modules\electron-builder") {
        Write-RecoveryStep "✓ electron-builder installed" "Success"
    }
    else {
        Write-RecoveryStep "✗ electron-builder not installed" "Error"
        return $false
    }
    
    return $true
}

function Show-RecoverySummary {
    param([bool]$Success, [string]$RecoveryType)
    
    Write-RecoveryHeader "Recovery Summary"
    
    if ($Success) {
        Write-Host "  ✓ Recovery completed successfully!" -ForegroundColor Green
        Write-Host ""
        Write-Host "  Next steps:" -ForegroundColor Cyan
        Write-Host "    1. Test the application:" -ForegroundColor White
        Write-Host "       .\Test-Agent-Fixed.ps1" -ForegroundColor Gray
        Write-Host ""
        Write-Host "    2. Build the application:" -ForegroundColor White
        Write-Host "       npx electron-builder --win --x64" -ForegroundColor Gray
        Write-Host ""
        Write-Host "    3. Run the application:" -ForegroundColor White
        Write-Host "       .\Launch-MyCopilot.ps1" -ForegroundColor Gray
    }
    else {
        Write-Host "  ✗ Recovery encountered errors" -ForegroundColor Red
        Write-Host ""
        Write-Host "  Troubleshooting:" -ForegroundColor Yellow
        Write-Host "    - Review error messages above" -ForegroundColor Gray
        Write-Host "    - Try running with -Force to reset everything" -ForegroundColor Gray
        Write-Host "    - Check docs\USER-GUIDE.md for detailed help" -ForegroundColor Gray
        Write-Host "    - Run .\Build-Recovery.ps1 -RecoveryType Full -Force" -ForegroundColor Gray
    }
    
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
}

# Main execution
try {
    Write-RecoveryHeader "MyCopilot IDE - Build Recovery"
    Write-Host "  Recovery Type: $RecoveryType" -ForegroundColor White
    Write-Host "  Force Mode: $Force" -ForegroundColor White
    
    # Check prerequisites
    if (-not (Test-Prerequisites)) {
        Write-Host "`n✗ Prerequisites check failed. Cannot continue." -ForegroundColor Red
        exit 1
    }
    
    $success = $true
    
    # Perform recovery based on type
    switch ($RecoveryType) {
        'Full' {
            $success = $success -and (Clean-BuildArtifacts)
            $success = $success -and (Repair-Configuration)
            $success = $success -and (Repair-Dependencies)
            $success = $success -and (Test-BuildReadiness)
        }
        'Dependencies' {
            $success = Repair-Dependencies
        }
        'Config' {
            $success = Repair-Configuration
        }
        'Clean' {
            $success = Clean-BuildArtifacts
        }
    }
    
    Show-RecoverySummary -Success $success -RecoveryType $RecoveryType
    
    if ($success) {
        exit 0
    }
    else {
        exit 1
    }
}
catch {
    Write-Host "`n✗ Fatal error during recovery:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Gray
    exit 1
}
