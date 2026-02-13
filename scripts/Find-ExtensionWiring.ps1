<#
.SYNOPSIS
    Finds extension enablement wiring (or lack thereof) in IDE features

.DESCRIPTION
    Scans the codebase to identify:
    1. Extensions that exist in the extensions/ directory
    2. Where extension enablement is checked/used in IDE code
    3. IDE features that DON'T check extension status before use
    4. Missing integration points for extension system

.EXAMPLE
    .\Find-ExtensionWiring.ps1
    .\Find-ExtensionWiring.ps1 -ShowWired -Verbose
#>

[CmdletBinding()]
param(
    [string]$IDERoot = "d:\lazy init ide",
    [switch]$ShowWired,
    [switch]$DeepScan
)

$ErrorActionPreference = "Continue"

Write-Host "=== Extension Wiring Scanner ===" -ForegroundColor Cyan
Write-Host "Scanning: $IDERoot`n" -ForegroundColor Gray

# Step 1: Discover extensions
Write-Verbose "Step 1: Discovering extensions..."
$extensions = @()
$extensionsDir = "$IDERoot\extensions"
$scriptsDir = "$IDERoot\scripts"

# Check extension registry
$registryPath = "$extensionsDir\registry.json"
if (Test-Path $registryPath) {
    try {
        $registry = Get-Content $registryPath -Raw | ConvertFrom-Json
        foreach ($ext in $registry.extensions) {
            $extensions += [PSCustomObject]@{
                Name = $ext.name
                Type = $ext.type
                Path = $ext.path
                Enabled = $ext.enabled
                Source = "Registry"
            }
            Write-Verbose "  Found extension in registry: $($ext.name) (enabled: $($ext.enabled))"
        }
    } catch {
        Write-Warning "Failed to parse extension registry: $_"
    }
}

# Scan extensions directory
if (Test-Path $extensionsDir) {
    Get-ChildItem -Path $extensionsDir -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        $extName = $_.Name
        if ($extensions.Name -notcontains $extName) {
            $extensions += [PSCustomObject]@{
                Name = $extName
                Type = "Unknown"
                Path = $_.FullName
                Enabled = $false
                Source = "Directory"
            }
            Write-Verbose "  Found extension directory: $extName"
        }
    }
}

# Scan scripts for .psm1 modules
Get-ChildItem -Path $scriptsDir -Filter "*.psm1" -File -ErrorAction SilentlyContinue | ForEach-Object {
    $moduleName = $_.BaseName
    if ($extensions.Name -notcontains $moduleName) {
        $extensions += [PSCustomObject]@{
            Name = $moduleName
            Type = "Module"
            Path = $_.FullName
            Enabled = $null  # Unknown
            Source = "Scripts"
        }
        Write-Verbose "  Found module: $moduleName"
    }
}

Write-Host "Found $($extensions.Count) extensions/modules`n" -ForegroundColor Yellow

# Step 2: Find extension enablement patterns
Write-Verbose "Step 2: Scanning for extension enablement checks..."

$wiringPatterns = @{
    # Extension Manager usage
    ExtensionManager = @(
        'ExtensionManager',
        'GetExtensionManager',
        'isExtensionEnabled',
        'getExtension\s*\(',
        'enableExtension',
        'disableExtension'
    )
    
    # PowerShell module checks
    ModuleChecks = @(
        'Get-Module.*-Name',
        'Import-Module',
        'Test-ModuleMember',
        'Get-Command.*-Module'
    )
    
    # Direct extension invocation
    DirectInvocation = @(
        'invokePowerShellExtension',
        'Invoke-Expression.*extension',
        'extension.*->.*\(',
        '\$extensions\['
    )
    
    # Conditional feature gating
    FeatureGating = @(
        'if.*extension.*enabled',
        'unless.*extension.*disabled',
        'when.*extension.*available',
        '#ifdef.*EXTENSION'
    )
}

# Scan C++ and PowerShell files
$sourceFiles = @()
$sourceFiles += Get-ChildItem -Path "$IDERoot\src" -Filter "*.cpp" -File -ErrorAction SilentlyContinue
$sourceFiles += Get-ChildItem -Path "$IDERoot\include" -Filter "*.h" -File -ErrorAction SilentlyContinue
$sourceFiles += Get-ChildItem -Path "$IDERoot\scripts" -Filter "*.ps1" -File -ErrorAction SilentlyContinue
$sourceFiles += Get-ChildItem -Path "$IDERoot\scripts" -Filter "*.psm1" -File -ErrorAction SilentlyContinue

$wiringResults = @()

foreach ($file in $sourceFiles) {
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    $fileResult = [PSCustomObject]@{
        File = $file.Name
        RelativePath = $file.FullName.Replace($IDERoot, '').TrimStart('\')
        HasExtensionWiring = $false
        WiringTypes = @()
        Matches = @()
    }
    
    foreach ($category in $wiringPatterns.Keys) {
        foreach ($pattern in $wiringPatterns[$category]) {
            if ($content -match $pattern) {
                $fileResult.HasExtensionWiring = $true
                if ($fileResult.WiringTypes -notcontains $category) {
                    $fileResult.WiringTypes += $category
                }
                
                # Extract match context
                $matches = [regex]::Matches($content, $pattern)
                foreach ($match in $matches) {
                    $startPos = [Math]::Max(0, $match.Index - 50)
                    $length = [Math]::Min(150, $content.Length - $startPos)
                    $context = $content.Substring($startPos, $length).Replace("`n", " ").Replace("`r", "")
                    
                    $fileResult.Matches += [PSCustomObject]@{
                        Pattern = $pattern
                        Context = $context.Trim()
                    }
                }
            }
        }
    }
    
    if ($fileResult.HasExtensionWiring -or $DeepScan) {
        $wiringResults += $fileResult
    }
}

# Step 3: Find features that DON'T check extensions
Write-Verbose "Step 3: Identifying unwired features..."

$featurePatterns = @(
    # AI/Model features that might need extension checks
    'model.*translate',
    'ai.*generate',
    'completion.*request',
    'language.*analyze',
    
    # Plugin/module features
    'plugin.*load',
    'module.*initialize',
    'addon.*activate',
    
    # Tool/utility features
    'tool.*invoke',
    'utility.*execute',
    'processor.*run'
)

$potentiallyUnwired = @()

foreach ($file in $sourceFiles) {
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    
    # Skip if file already has extension wiring
    if (($wiringResults | Where-Object { $_.File -eq $file.Name }).HasExtensionWiring) {
        continue
    }
    
    # Check if file has features that might need extension checks
    $hasFeatures = $false
    $foundFeatures = @()
    
    foreach ($pattern in $featurePatterns) {
        if ($content -match $pattern) {
            $hasFeatures = $true
            $foundFeatures += $pattern
        }
    }
    
    if ($hasFeatures) {
        $potentiallyUnwired += [PSCustomObject]@{
            File = $file.Name
            RelativePath = $file.FullName.Replace($IDERoot, '').TrimStart('\')
            SuspectedFeatures = $foundFeatures -join ", "
        }
    }
}

# Step 4: Check extension-specific integration
Write-Verbose "Step 4: Checking extension-specific integration..."

$extensionIntegration = @()

foreach ($ext in $extensions) {
    $extName = $ext.Name
    $isWired = $false
    $usageLocations = @()
    
    foreach ($result in $wiringResults) {
        $file = Get-Item (Join-Path $IDERoot $result.RelativePath) -ErrorAction SilentlyContinue
        if (-not $file) { continue }
        
        $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($content -match [regex]::Escape($extName)) {
            $isWired = $true
            $usageLocations += $result.File
        }
    }
    
    $extensionIntegration += [PSCustomObject]@{
        Extension = $extName
        Type = $ext.Type
        Enabled = $ext.Enabled
        IsWired = $isWired
        UsageLocations = $usageLocations -join ", "
    }
}

# Step 5: Report results
Write-Host "`n=== EXTENSION WIRING STATUS ===" -ForegroundColor Cyan

Write-Host "`n--- Files WITH Extension Wiring ---" -ForegroundColor Green
if ($wiringResults.Count -eq 0) {
    Write-Host "  ⚠ No files found with extension enablement checks!" -ForegroundColor Yellow
} else {
    foreach ($result in $wiringResults) {
        Write-Host "  ✓ $($result.File)" -ForegroundColor Green
        Write-Host "     Path: $($result.RelativePath)" -ForegroundColor Gray
        Write-Host "     Wiring: $($result.WiringTypes -join ', ')" -ForegroundColor DarkGreen
        
        if ($ShowWired) {
            foreach ($match in ($result.Matches | Select-Object -First 3)) {
                Write-Host "       - $($match.Context)" -ForegroundColor DarkGray
            }
        }
        Write-Host ""
    }
}

Write-Host "`n--- Features POTENTIALLY MISSING Extension Checks ---" -ForegroundColor Red
if ($potentiallyUnwired.Count -eq 0) {
    Write-Host "  ✓ No obvious unwired features detected" -ForegroundColor Green
} else {
    foreach ($unwired in $potentiallyUnwired) {
        Write-Host "  ❌ $($unwired.File)" -ForegroundColor Red
        Write-Host "     Path: $($unwired.RelativePath)" -ForegroundColor Gray
        Write-Host "     Features: $($unwired.SuspectedFeatures)" -ForegroundColor Yellow
        Write-Host ""
    }
}

Write-Host "`n--- Extension-Specific Integration ---" -ForegroundColor Cyan
$wiredExtensions = $extensionIntegration | Where-Object { $_.IsWired }
$unwiredExtensions = $extensionIntegration | Where-Object { -not $_.IsWired }

Write-Host "Wired Extensions: $($wiredExtensions.Count)" -ForegroundColor Green
foreach ($ext in $wiredExtensions) {
    Write-Host "  ✓ $($ext.Extension) [$($ext.Type)]" -ForegroundColor Green
    if ($ext.UsageLocations) {
        Write-Host "     Used in: $($ext.UsageLocations)" -ForegroundColor Gray
    }
}

Write-Host "`nUnwired Extensions: $($unwiredExtensions.Count)" -ForegroundColor Red
foreach ($ext in $unwiredExtensions) {
    Write-Host "  ❌ $($ext.Extension) [$($ext.Type)]" -ForegroundColor Red
    Write-Host "     Status: $(if ($ext.Enabled) { 'Enabled' } elseif ($null -eq $ext.Enabled) { 'Unknown' } else { 'Disabled' })" -ForegroundColor Yellow
}

# Integration recommendations
Write-Host "`n=== INTEGRATION RECOMMENDATIONS ===" -ForegroundColor Cyan

if ($unwiredExtensions.Count -gt 0) {
    Write-Host "`nTo wire extensions into IDE features:" -ForegroundColor Yellow
    Write-Host "  1. In C++ code, check extension before using:" -ForegroundColor White
    Write-Host "     auto& extMgr = GetExtensionManager();" -ForegroundColor Gray
    Write-Host "     if (extMgr.isExtensionEnabled(`"ExtensionName`")) {" -ForegroundColor Gray
    Write-Host "         extMgr.invokePowerShellExtension(`"ExtensionName`", `"FunctionName`", args);" -ForegroundColor Gray
    Write-Host "     }" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  2. In PowerShell code, check module availability:" -ForegroundColor White
    Write-Host "     if (Get-Module -Name ExtensionName -ListAvailable) {" -ForegroundColor Gray
    Write-Host "         Import-Module ExtensionName" -ForegroundColor Gray
    Write-Host "         Invoke-ExtensionFunction" -ForegroundColor Gray
    Write-Host "     }" -ForegroundColor Gray
}

if ($potentiallyUnwired.Count -gt 0) {
    Write-Host "`nFiles with features that may need extension checks:" -ForegroundColor Yellow
    foreach ($file in $potentiallyUnwired | Select-Object -First 5) {
        Write-Host "  - $($file.RelativePath)" -ForegroundColor Gray
    }
}

# Summary
Write-Host "`n=== SUMMARY ===" -ForegroundColor Cyan
Write-Host "Total Extensions:        $($extensions.Count)" -ForegroundColor White
Write-Host "Wired into IDE:          $($wiredExtensions.Count) ✓" -ForegroundColor Green
Write-Host "Not Wired:               $($unwiredExtensions.Count) ❌" -ForegroundColor Red
Write-Host "`nFiles with Wiring:      $($wiringResults.Count) ✓" -ForegroundColor Green
Write-Host "Potentially Unwired:     $($potentiallyUnwired.Count) ⚠" -ForegroundColor Yellow

# Return objects for scripting
return [PSCustomObject]@{
    TotalExtensions = $extensions.Count
    WiredExtensions = $wiredExtensions
    UnwiredExtensions = $unwiredExtensions
    FilesWithWiring = $wiringResults
    PotentiallyUnwired = $potentiallyUnwired
    AllExtensions = $extensions
}
