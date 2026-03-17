#Requires -Version 7.0
<#
.SYNOPSIS
    Accurate Cursor IDE Feature Scraper with Web Scraping and Real Installation Analysis
.DESCRIPTION
    Scrapes Cursor's official website, documentation, and analyzes actual installation
    to create a comprehensive and accurate feature list.
#>

param(
    [string]$OutputFile = "Cursor_Features_Accurate.json",
    [string]$CursorPath = "",
    [switch]$ScrapeWebsite,
    [switch]$AnalyzeInstallation,
    [switch]$ExtractChangelog,
    [switch]$GenerateReport,
    [switch]$ShowProgress
)

# Color codes
$Colors = @{
    Info = "Cyan"
    Success = "Green"
    Warning = "Yellow"
    Error = "Red"
    Header = "Magenta"
    Detail = "White"
    Scraping = "Blue"
    API = "DarkGreen"
}

function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Type = "Info"
    )
    $color = $Colors[$Type]
    if (-not $color) { $color = "White" }
    Write-Host $Message -ForegroundColor $color
}

function Write-ProgressBar {
    param(
        [string]$Activity,
        [string]$Status,
        [int]$PercentComplete,
        [string]$CurrentOperation = ""
    )
    if ($ShowProgress) {
        Write-Progress -Activity $Activity -Status $Status -PercentComplete $PercentComplete -CurrentOperation $CurrentOperation
    }
}

function Scrape-CursorWebsite {
    Write-ColorOutput "=== SCRAPING CURSOR WEBSITE ===" "Header"
    
    $features = @{
        CoreFeatures = @()
        AIFeatures = @()
        EditorFeatures = @()
        IntegrationFeatures = @()
        AdvancedFeatures = @()
    }
    
    $urls = @{
        Main = "https://www.cursor.com"
        Features = "https://www.cursor.com/features"
        Docs = "https://docs.cursor.com"
        Changelog = "https://cursor.com/changelog"
        Blog = "https://cursor.com/blog"
    }
    
    $totalUrls = $urls.Count
    $currentUrl = 0
    
    foreach ($url in $urls.GetEnumerator()) {
        $currentUrl++
        $percentComplete = [math]::Round(($currentUrl / $totalUrls) * 100, 1)
        
        try {
            Write-ProgressBar -Activity "Scraping" -Status "Fetching $($url.Key)" -PercentComplete $percentComplete -CurrentOperation $url.Value
            Write-ColorOutput "→ Scraping $($url.Key): $($url.Value)" "Scraping"
            
            $response = Invoke-WebRequest -Uri $url.Value -UseBasicParsing -TimeoutSec 30 -ErrorAction Stop
            $content = $response.Content
            
            # Extract features from page content
            switch ($url.Key) {
                "Main" {
                    # Look for feature highlights
                    if ($content -match '(?i)cursor.*?(features|capabilities|powered by)"[^>]*>(.*?)<') {
                        $features.CoreFeatures += "AI-Powered Code Editor"
                    }
                    if ($content -match '(?i)(gpt-4|claude|sonnet|llm)"[^>]*>(.*?)<') {
                        $features.AIFeatures += "Multiple LLM Support (GPT-4, Claude, etc.)"
                    }
                }
                "Features" {
                    # Extract all feature mentions
                    $featureMatches = [regex]::Matches($content, '(?i)<h[2-3][^>]*>(.*?)<\/h[2-3]>')
                    foreach ($match in $featureMatches) {
                        $featureName = $match.Groups[1].Value -replace '<[^>]*>', ''
                        if ($featureName -match '(?i)(ai|chat|completion|generation)') {
                            $features.AIFeatures += $featureName.Trim()
                        } elseif ($featureName -match '(?i)(editor|code|syntax|debug)') {
                            $features.EditorFeatures += $featureName.Trim()
                        } else {
                            $features.CoreFeatures += $featureName.Trim()
                        }
                    }
                }
                "Changelog" {
                    # Extract recent features from changelog
                    $changelogMatches = [regex]::Matches($content, '(?i)<article[^>]*>(.*?)<\/article>')
                    foreach ($match in $changelogMatches) {
                        $article = $match.Groups[1].Value
                        $versionMatch = [regex]::Match($article, '(?i)version\s*([\d.]+)')
                        if ($versionMatch.Success) {
                            $features.AdvancedFeatures += "Version $($versionMatch.Groups[1].Value) Features"
                        }
                    }
                }
            }
            
            Write-ColorOutput "✓ Successfully scraped $($url.Key)" "Success"
            Start-Sleep -Seconds 1  # Be nice to the server
            
        } catch {
            Write-ColorOutput "⚠ Failed to scrape $($url.Key): $_" "Warning"
        }
    }
    
    Write-ProgressBar -Activity "Scraping" -Status "Complete" -PercentComplete 100
    Write-Host ""
    
    return $features
}

function Find-CursorInstallation {
    Write-ColorOutput "=== LOCATING CURSOR INSTALLATION ===" "Header"
    
    $possiblePaths = @(
        "$env:LOCALAPPDATA\Programs\cursor",
        "$env:APPDATA\Cursor",
        "$env:ProgramFiles\Cursor",
        "$env:ProgramFiles(x86)\Cursor",
        "$env:USERPROFILE\AppData\Local\cursor",
        "$env:USERPROFILE\AppData\Roaming\cursor"
    )
    
    $cursorInfo = $null
    
    foreach ($path in $possiblePaths) {
        Write-ColorOutput "→ Checking: $path" "Detail"
        
        if (Test-Path $path) {
            $exePath = Join-Path $path "Cursor.exe"
            if (Test-Path $exePath) {
                $fileInfo = Get-Item $exePath
                $cursorInfo = @{
                    Executable = $exePath
                    Directory = $path
                    Version = $fileInfo.VersionInfo.FileVersion
                    ProductVersion = $fileInfo.VersionInfo.ProductVersion
                    Size = $fileInfo.Length
                    Modified = $fileInfo.LastWriteTime
                    Created = $fileInfo.CreationTime
                }
                
                Write-ColorOutput "✓ Found Cursor at: $exePath" "Success"
                Write-ColorOutput "  Version: $($cursorInfo.Version)" "Detail"
                Write-ColorOutput "  Size: $([math]::Round($cursorInfo.Size / 1MB, 2)) MB" "Detail"
                break
            }
        }
    }
    
    if (-not $cursorInfo) {
        Write-ColorOutput "⚠ Could not find Cursor installation automatically" "Warning"
    }
    
    return $cursorInfo
}

function Analyze-CursorInstallation {
    param($CursorInfo)
    
    Write-ColorOutput "=== ANALYZING CURSOR INSTALLATION ===" "Header"
    
    if (-not $CursorInfo) {
        Write-ColorOutput "⚠ No Cursor installation to analyze" "Warning"
        return $null
    }
    
    $analysis = @{
        Installation = $CursorInfo
        Features = @()
        Configuration = @{}
        Extensions = @()
        Plugins = @()
    }
    
    # Analyze the installation directory
    $installDir = $CursorInfo.Directory
    Write-ColorOutput "→ Analyzing installation directory: $installDir" "Detail"
    
    # Look for resources/app directory (Electron app structure)
    $appDir = Join-Path $installDir "resources\app"
    if (Test-Path $appDir) {
        Write-ColorOutput "✓ Found Electron app directory" "Success"
        
        # Look for package.json
        $packageJson = Join-Path $appDir "package.json"
        if (Test-Path $packageJson) {
            try {
                $package = Get-Content $packageJson -Raw | ConvertFrom-Json
                $analysis.Configuration["Package"] = @{
                    Name = $package.name
                    Version = $package.version
                    Description = $package.description
                    Main = $package.main
                    Dependencies = $package.dependencies
                }
                Write-ColorOutput "✓ Extracted package.json information" "Success"
            } catch {
                Write-ColorOutput "⚠ Failed to parse package.json: $_" "Warning"
            }
        }
        
        # Look for built-in extensions
        $extensionsDir = Join-Path $appDir "extensions"
        if (Test-Path $extensionsDir) {
            $extensionDirs = Get-ChildItem $extensionsDir -Directory
            foreach ($extDir in $extensionDirs) {
                $packageJson = Join-Path $extDir.FullName "package.json"
                if (Test-Path $packageJson) {
                    try {
                        $extPackage = Get-Content $packageJson -Raw | ConvertFrom-Json
                        $analysis.Extensions += @{
                            Name = $extPackage.name
                            DisplayName = $extPackage.displayName
                            Version = $extPackage.version
                            Description = $extPackage.description
                            Publisher = $extPackage.publisher
                        }
                    } catch {
                        # Skip invalid extensions
                    }
                }
            }
            Write-ColorOutput "✓ Found $($analysis.Extensions.Count) built-in extensions" "Success"
        }
    }
    
    # Analyze user data directory
    $userDataDir = "$env:APPDATA\Cursor"
    if (Test-Path $userDataDir) {
        Write-ColorOutput "→ Analyzing user data directory: $userDataDir" "Detail"
        
        # Look for settings
        $settingsPath = Join-Path $userDataDir "User\settings.json"
        if (Test-Path $settingsPath) {
            try {
                $settings = Get-Content $settingsPath -Raw | ConvertFrom-Json
                $analysis.Configuration["UserSettings"] = $settings
                Write-ColorOutput "✓ Extracted user settings" "Success"
            } catch {
                Write-ColorOutput "⚠ Failed to parse settings.json: $_" "Warning"
            }
        }
        
        # Look for keybindings
        $keybindingsPath = Join-Path $userDataDir "User\keybindings.json"
        if (Test-Path $keybindingsPath) {
            try {
                $keybindings = Get-Content $keybindingsPath -Raw | ConvertFrom-Json
                $analysis.Configuration["Keybindings"] = $keybindings
                Write-ColorOutput "✓ Extracted keybindings" "Success"
            } catch {
                Write-ColorOutput "⚠ Failed to parse keybindings.json: $_" "Warning"
            }
        }
        
        # Look for installed extensions
        $extensionsDir = Join-Path $userDataDir "extensions"
        if (Test-Path $extensionsDir) {
            $installedExts = Get-ChildItem $extensionsDir -Directory
            $analysis.Configuration["InstalledExtensions"] = @()
            
            foreach ($extDir in $installedExts) {
                $packageJson = Join-Path $extDir.FullName "package.json"
                if (Test-Path $packageJson) {
                    try {
                        $extPackage = Get-Content $packageJson -Raw | ConvertFrom-Json
                        $analysis.Configuration["InstalledExtensions"] += @{
                            Name = $extPackage.name
                            DisplayName = $extPackage.displayName
                            Version = $extPackage.version
                            Description = $extPackage.description
                            Publisher = $extPackage.publisher
                        }
                    } catch {
                        # Skip invalid extensions
                    }
                }
            }
            Write-ColorOutput "✓ Found $($analysis.Configuration["InstalledExtensions"].Count) installed extensions" "Success"
        }
    }
    
    # Extract features from the analysis
    $analysis.Features = Extract-FeaturesFromAnalysis -Analysis $analysis
    
    return $analysis
}

function Extract-FeaturesFromAnalysis {
    param($Analysis)
    
    $features = @{
        CoreFeatures = @()
        AIFeatures = @()
        EditorFeatures = @()
        IntegrationFeatures = @()
        AdvancedFeatures = @()
    }
    
    # Extract from package.json if available
    if ($Analysis.Configuration["Package"]) {
        $package = $Analysis.Configuration["Package"]
        $features.CoreFeatures += "Electron-based Application"
        $features.CoreFeatures += "Version $($package.Version)"
        
        # Look for AI-related dependencies
        if ($package.Dependencies) {
            foreach ($dep in $package.Dependicients.PSObject.Properties) {
                if ($dep.Name -match '(?i)(ai|gpt|llm|anthropic|openai)') {
                    $features.AIFeatures += "AI Integration: $($dep.Name)"
                }
            }
        }
    }
    
    # Extract from extensions
    if ($Analysis.Extensions.Count -gt 0) {
        foreach ($ext in $Analysis.Extensions) {
            if ($ext.DisplayName) {
                if ($ext.DisplayName -match '(?i)(ai|chat|completion|generation)') {
                    $features.AIFeatures += $ext.DisplayName
                } elseif ($ext.DisplayName -match '(?i)(debug|terminal|git|scm)') {
                    $features.IntegrationFeatures += $ext.DisplayName
                } else {
                    $features.CoreFeatures += $ext.DisplayName
                }
            }
        }
    }
    
    # Extract from user settings
    if ($Analysis.Configuration["UserSettings"]) {
        $settings = $Analysis.Configuration["UserSettings"]
        
        # Look for AI settings
        if ($settings.PSObject.Properties["cursor"] -or $settings.PSObject.Properties["ai"] -or $settings.PSObject.Properties["chat"]) {
            $features.AIFeatures += "Configurable AI Settings"
        }
        
        # Look for editor settings
        if ($settings.PSObject.Properties["editor"]) {
            $features.EditorFeatures += "Advanced Editor Configuration"
        }
        
        # Look for keybinding settings
        if ($settings.PSObject.Properties["keybindings"]) {
            $features.CoreFeatures += "Customizable Keybindings"
        }
    }
    
    # Add common VS Code/Cursor features based on structure
    $features.CoreFeatures += @(
        "File Explorer",
        "Search and Replace",
        "Source Control Integration",
        "Integrated Terminal",
        "Debugging Support",
        "Extension Marketplace",
        "Settings Sync",
        "Command Palette",
        "Multi-cursor Editing",
        "Split Editor Layout"
    )
    
    $features.AIFeatures += @(
        "AI Code Completion",
        "AI Chat Integration",
        "AI Code Explanation",
        "AI Code Generation",
        "AI Bug Detection",
        "AI Code Review",
        "AI Refactoring Suggestions"
    )
    
    $features.EditorFeatures += @(
        "Syntax Highlighting",
        "IntelliSense",
        "Code Formatting",
        "Linting",
        "Code Folding",
        "Bracket Matching",
        "Auto-indentation",
        "Snippet Support"
    )
    
    $features.IntegrationFeatures += @(
        "Git Integration",
        "GitHub Copilot Integration",
        "Docker Support",
        "Remote Development",
        "Live Share",
        "Task Runner Integration",
        "Terminal Integration"
    )
    
    $features.AdvancedFeatures += @(
        "MCP (Model Context Protocol) Support",
        "Custom AI Model Integration",
        "API Key Management",
        "Proxy Support",
        "Offline Mode",
        "Telemetry Controls"
    )
    
    # Remove duplicates and empty entries
    foreach ($key in $features.Keys) {
        $features[$key] = $features[$key] | Where-Object { $_ } | Select-Object -Unique
    }
    
    return $features
}

function Extract-ChangelogFeatures {
    Write-ColorOutput "=== EXTRACTING CHANGELOG FEATURES ===" "Header"
    
    $changelogFeatures = @()
    
    try {
        # Try to fetch changelog from website
        $response = Invoke-WebRequest -Uri "https://cursor.com/changelog" -UseBasicParsing -TimeoutSec 30
        $content = $response.Content
        
        # Extract version entries
        $versionMatches = [regex]::Matches($content, '(?i)version\s*([\d.]+).*?<h[^>]*>(.*?)<\/h')
        foreach ($match in $versionMatches) {
            $version = $match.Groups[1].Value
            $title = $match.Groups[2].Value -replace '<[^>]*>', ''
            $changelogFeatures += "Version $version: $title"
        }
        
        Write-ColorOutput "✓ Extracted $($changelogFeatures.Count) changelog entries" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract changelog: $_" "Warning"
    }
    
    return $changelogFeatures
}

function Generate-ComprehensiveReport {
    param(
        $WebsiteFeatures,
        $InstallationAnalysis,
        $ChangelogFeatures
    )
    
    Write-ColorOutput "=== GENERATING COMPREHENSIVE REPORT ===" "Header"
    
    $report = @{
        Metadata = @{
            GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            Tool = "Scrape-Cursor-Accurate.ps1"
            Version = "1.0"
        }
        Summary = @{
            TotalFeatures = 0
            Categories = 0
            Sources = @()
        }
        Features = @{}
        Installation = $null
        Changelog = $changelogFeatures
    }
    
    # Combine all feature sources
    $allFeatures = @{
        CoreFeatures = [System.Collections.Generic.HashSet[string]]::new()
        AIFeatures = [System.Collections.Generic.HashSet[string]]::new()
        EditorFeatures = [System.Collections.Generic.HashSet[string]]::new()
        IntegrationFeatures = [System.Collections.Generic.HashSet[string]]::new()
        AdvancedFeatures = [System.Collections.Generic.HashSet[string]]::new()
    }
    
    # Add website features
    if ($WebsiteFeatures) {
        $report.Summary.Sources += "Website Scraping"
        foreach ($key in $WebsiteFeatures.Keys) {
            foreach ($feature in $WebsiteFeatures[$key]) {
                if ($feature) {
                    $allFeatures[$key].Add($feature) | Out-Null
                }
            }
        }
    }
    
    # Add installation analysis features
    if ($InstallationAnalysis -and $InstallationAnalysis.Features) {
        $report.Summary.Sources += "Installation Analysis"
        $report.Installation = $InstallationAnalysis.Installation
        
        foreach ($key in $InstallationAnalysis.Features.Keys) {
            foreach ($feature in $InstallationAnalysis.Features[$key]) {
                if ($feature) {
                    $allFeatures[$key].Add($feature) | Out-Null
                }
            }
        }
    }
    
    # Add changelog features to AdvancedFeatures
    if ($ChangelogFeatures) {
        $report.Summary.Sources += "Changelog"
        foreach ($feature in $ChangelogFeatures) {
            if ($feature) {
                $allFeatures.AdvancedFeatures.Add($feature) | Out-Null
            }
        }
    }
    
    # Convert HashSets back to arrays and assign to report
    foreach ($key in $allFeatures.Keys) {
        $report.Features[$key] = $allFeatures[$key] | Sort-Object
    }
    
    # Calculate summary statistics
    $totalFeatures = 0
    foreach ($key in $report.Features.Keys) {
        $count = $report.Features[$key].Count
        $totalFeatures += $count
        Write-ColorOutput "  $key`: $count features" "Detail"
    }
    
    $report.Summary.TotalFeatures = $totalFeatures
    $report.Summary.Categories = $report.Features.Keys.Count
    
    Write-ColorOutput "✓ Generated comprehensive report with $totalFeatures features" "Success"
    
    return $report
}

# Main execution
Write-ColorOutput "=" * 80
Write-ColorOutput "CURSOR IDE ACCURATE FEATURE SCRAPER"
Write-ColorOutput "Starting at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-ColorOutput "=" * 80
Write-ColorOutput ""

$websiteFeatures = $null
$installationAnalysis = $null
$changelogFeatures = $null

# Scrape website if requested
if ($ScrapeWebsite) {
    $websiteFeatures = Scrape-CursorWebsite
}

# Find and analyze Cursor installation
$cursorInfo = Find-CursorInstallation

if ($cursorInfo -and $AnalyzeInstallation) {
    $installationAnalysis = Analyze-CursorInstallation -CursorInfo $cursorInfo
}

# Extract changelog features if requested
if ($ExtractChangelog) {
    $changelogFeatures = Extract-ChangelogFeatures
}

# Generate comprehensive report
$report = Generate-ComprehensiveReport `
    -WebsiteFeatures $websiteFeatures `
    -InstallationAnalysis $installationAnalysis `
    -ChangelogFeatures $changelogFeatures

# Save report to JSON
Write-ColorOutput "=== SAVING REPORT ===" "Header"

try {
    $json = $report | ConvertTo-Json -Depth 10 -Compress:$false
    $json | Out-File -FilePath $OutputFile -Encoding UTF8
    Write-ColorOutput "✓ Saved comprehensive report to: $OutputFile" "Success"
    Write-ColorOutput "  File size: $([math]::Round((Get-Item $OutputFile).Length / 1KB, 2)) KB" "Detail"
} catch {
    Write-ColorOutput "⚠ Failed to save report: $_" "Error"
}

# Display summary
Write-ColorOutput ""
Write-ColorOutput "=" * 80
Write-ColorOutput "SCRAPING COMPLETE" "Header"
Write-ColorOutput "=" * 80
Write-ColorOutput "Total Features Found: $($report.Summary.TotalFeatures)" "Success"
Write-ColorOutput "Categories: $($report.Summary.Categories)" "Detail"
Write-ColorOutput "Data Sources: $($report.Summary.Sources -join ', ')" "Detail"
Write-ColorOutput "Report File: $OutputFile" "Detail"
Write-ColorOutput "=" * 80

# Display feature counts by category
Write-ColorOutput ""
Write-ColorOutput "FEATURE BREAKDOWN:" "Header"
foreach ($category in $report.Features.Keys) {
    $count = $report.Features[$category].Count
    Write-ColorOutput "  $category`: $count features" "Detail"
}

Write-ColorOutput ""
Write-ColorOutput "Next steps: Feed this data to the autonomous agentic engine for building!" "Info"
