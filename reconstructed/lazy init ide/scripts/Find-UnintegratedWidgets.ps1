<#
.SYNOPSIS
    Finds widgets/panels that exist but aren't integrated into MainWindow

.DESCRIPTION
    Scans for QWidget-derived classes in include/ and src/ directories,
    then checks if they're instantiated in mainwindow.cpp as dockable widgets or tabs.
    Reports widgets that exist but aren't wired into the main UI.

.EXAMPLE
    .\Find-UnintegratedWidgets.ps1
    .\Find-UnintegratedWidgets.ps1 -Verbose
#>

[CmdletBinding()]
param(
    [string]$IDERoot = "d:\lazy init ide",
    [switch]$ShowIntegrated
)

$ErrorActionPreference = "Continue"

Write-Host "=== Unintegrated Widget Scanner ===" -ForegroundColor Cyan
Write-Host "Scanning: $IDERoot`n" -ForegroundColor Gray

# Step 1: Find all widget/panel classes
Write-Verbose "Step 1: Discovering widget classes..."
$widgetClasses = @()

# Scan header files for QWidget-derived classes
Get-ChildItem -Path "$IDERoot\include" -Filter "*.h" -File -ErrorAction SilentlyContinue | ForEach-Object {
    $content = Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue
    if ($content -match 'class\s+(\w+)\s*:\s*public\s+QWidget') {
        $className = $Matches[1]
        $widgetClasses += [PSCustomObject]@{
            ClassName = $className
            HeaderFile = $_.Name
            FullPath = $_.FullName
        }
        Write-Verbose "  Found widget: $className in $($_.Name)"
    }
}

Write-Host "Found $($widgetClasses.Count) widget classes`n" -ForegroundColor Yellow

# Step 2: Check MainWindow integration
Write-Verbose "Step 2: Checking MainWindow integration..."
$mainWindowCpp = "$IDERoot\src\mainwindow.cpp"
$mainWindowH = "$IDERoot\include\mainwindow.h"

if (-not (Test-Path $mainWindowCpp)) {
    Write-Warning "mainwindow.cpp not found at: $mainWindowCpp"
    Write-Host "`nSearching for mainwindow.cpp..." -ForegroundColor Yellow
    $found = Get-ChildItem -Path $IDERoot -Filter "mainwindow.cpp" -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $mainWindowCpp = $found.FullName
        Write-Host "Found: $mainWindowCpp" -ForegroundColor Green
    } else {
        Write-Error "Cannot locate mainwindow.cpp"
        exit 1
    }
}

$mainWindowContent = Get-Content $mainWindowCpp -Raw -ErrorAction SilentlyContinue
$mainWindowHeader = if (Test-Path $mainWindowH) { Get-Content $mainWindowH -Raw } else { "" }

# Integration patterns to check
$integrationPatterns = @(
    # Dockable widget patterns
    'new\s+{0}\s*\(',
    'addDockWidget\s*\([^)]*{0}',
    '{0}\s*=\s*new\s+{0}',
    'QDockWidget.*{0}',
    
    # Tab widget patterns  
    'addTab\s*\([^)]*{0}',
    'insertTab\s*\([^)]*{0}',
    'tabWidget.*add.*{0}',
    
    # Central widget patterns
    'setCentralWidget\s*\([^)]*{0}',
    
    # Member variable patterns
    '{0}\s*\*\s*\w+\s*;',
    '{0}\s+\w+\s*;'
)

# Step 3: Analyze integration status
$integratedWidgets = @()
$unintegratedWidgets = @()

foreach ($widget in $widgetClasses) {
    $className = $widget.ClassName
    $isIntegrated = $false
    $integrationMethods = @()
    
    foreach ($pattern in $integrationPatterns) {
        $searchPattern = $pattern -f [regex]::Escape($className)
        
        if ($mainWindowContent -match $searchPattern) {
            $isIntegrated = $true
            $integrationMethods += $Matches[0]
            Write-Verbose "  $className: Matched pattern '$searchPattern'"
        }
    }
    
    # Also check header for member declarations
    if ($mainWindowHeader -match "$className\s*\*") {
        $isIntegrated = $true
        $integrationMethods += "Member pointer declared in header"
    }
    
    $result = [PSCustomObject]@{
        ClassName = $className
        HeaderFile = $widget.HeaderFile
        Integrated = $isIntegrated
        IntegrationMethods = $integrationMethods -join "; "
    }
    
    if ($isIntegrated) {
        $integratedWidgets += $result
    } else {
        $unintegratedWidgets += $result
    }
}

# Step 4: Report results
Write-Host "`n=== UNINTEGRATED WIDGETS ===" -ForegroundColor Red
if ($unintegratedWidgets.Count -eq 0) {
    Write-Host "✓ All widgets are integrated into MainWindow!" -ForegroundColor Green
} else {
    Write-Host "Found $($unintegratedWidgets.Count) widgets NOT integrated into MainWindow:`n" -ForegroundColor Yellow
    
    foreach ($widget in $unintegratedWidgets) {
        Write-Host "  ❌ $($widget.ClassName)" -ForegroundColor Red
        Write-Host "     File: $($widget.HeaderFile)" -ForegroundColor Gray
        Write-Host ""
    }
    
    Write-Host "`nTo integrate these widgets into MainWindow:" -ForegroundColor Cyan
    Write-Host "  1. Add member pointer in mainwindow.h:" -ForegroundColor White
    Write-Host "     ClassName* widgetName_;" -ForegroundColor Gray
    Write-Host "  2. Instantiate in mainwindow.cpp constructor:" -ForegroundColor White
    Write-Host "     widgetName_ = new ClassName(this);" -ForegroundColor Gray
    Write-Host "  3. Add as dock widget:" -ForegroundColor White
    Write-Host "     auto* dock = new QDockWidget(tr(`"Title`"), this);" -ForegroundColor Gray
    Write-Host "     dock->setWidget(widgetName_);" -ForegroundColor Gray
    Write-Host "     addDockWidget(Qt::RightDockWidgetArea, dock);" -ForegroundColor Gray
    Write-Host "  OR add as tab:" -ForegroundColor White
    Write-Host "     tabWidget->addTab(widgetName_, tr(`"Title`"));" -ForegroundColor Gray
}

if ($ShowIntegrated -and $integratedWidgets.Count -gt 0) {
    Write-Host "`n=== INTEGRATED WIDGETS ===" -ForegroundColor Green
    Write-Host "Found $($integratedWidgets.Count) widgets already integrated:`n" -ForegroundColor Yellow
    
    foreach ($widget in $integratedWidgets) {
        Write-Host "  ✓ $($widget.ClassName)" -ForegroundColor Green
        Write-Host "     File: $($widget.HeaderFile)" -ForegroundColor Gray
        Write-Host "     Integration: $($widget.IntegrationMethods)" -ForegroundColor DarkGray
        Write-Host ""
    }
}

# Summary
Write-Host "`n=== SUMMARY ===" -ForegroundColor Cyan
Write-Host "Total Widgets: $($widgetClasses.Count)" -ForegroundColor White
Write-Host "Integrated:    $($integratedWidgets.Count) ✓" -ForegroundColor Green
Write-Host "Unintegrated:  $($unintegratedWidgets.Count) ❌" -ForegroundColor Red

# Return objects for scripting
return [PSCustomObject]@{
    TotalWidgets = $widgetClasses.Count
    Integrated = $integratedWidgets
    Unintegrated = $unintegratedWidgets
}
