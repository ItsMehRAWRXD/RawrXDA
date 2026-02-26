#Requires -Version 7.0
<#
.SYNOPSIS
    Comprehensive Cursor IDE Features Extraction Script
.DESCRIPTION
    Uses Playwright to extract ALL features from Cursor's website including:
    - Main features page
    - Changelog (latest updates)
    - Pricing/enterprise features
    - Dynamically loaded content
    - Sub-features and capabilities
#>

param(
    [string]$OutputFile = "",
    [switch]$InstallPlaywright,
    [switch]$ShowProgress,
    [switch]$IncludeChangelog,
    [switch]$IncludePricing,
    [switch]$IncludeEnterprise
)

# Color codes
$Colors = @{
    Info = "Cyan"
    Success = "Green"
    Warning = "Yellow"
    Error = "Red"
    Header = "Magenta"
    Detail = "White"
    Feature = "Green"
    Category = "Yellow"
}

function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Type = "Info"
    )
    Write-Host $Message -ForegroundColor $Colors[$Type]
}

function Install-PlaywrightIfNeeded {
    Write-ColorOutput "=== CHECKING PLAYWRIGHT INSTALLATION ===" "Header"
    
    try {
        # Check if Playwright module is installed
        $playwrightModule = Get-Module -ListAvailable -Name "Microsoft.Playwright.PowerShell" -ErrorAction SilentlyContinue
        
        if (-not $playwrightModule) {
            Write-ColorOutput "⚠ Playwright PowerShell module not found" "Warning"
            Write-ColorOutput "Installing Playwright..." "Info"
            
            # Install the module
            Install-Module -Name "Microsoft.Playwright.PowerShell" -Force -Scope CurrentUser
            
            Write-ColorOutput "✓ Playwright PowerShell module installed" "Success"
        }
        else {
            Write-ColorOutput "✓ Playwright PowerShell module found" "Success"
        }
        
        # Install browsers
        Write-ColorOutput "Installing Playwright browsers..." "Info"
        & playwright install chromium
        
        Write-ColorOutput "✓ Playwright browsers installed" "Success"
        return $true
    }
    catch {
        Write-ColorOutput "✗ Error installing Playwright: $_" "Error"
        Write-ColorOutput "  Please install manually: Install-Module Microsoft.Playwright.PowerShell" "Detail"
        return $false
    }
}

function Extract-FeaturesFromUrl {
    param(
        [string]$Url,
        [string]$PageName,
        [ref]$FeaturesCollection
    )
    
    Write-ColorOutput "=== EXTRACTING FROM $PageName ===" "Header"
    Write-ColorOutput "URL: $Url" "Detail"
    
    try {
        # Launch browser
        $browser = Start-PlaywrightBrowser -BrowserType Chromium -Headless
        $page = $browser.NewPageAsync().Result
        
        # Navigate to URL
        Write-ColorOutput "  Navigating to page..." "Info"
        $page.GotoAsync($Url, @{ WaitUntil = "networkidle" }).Wait()
        
        # Wait a bit for any dynamic content
        Start-Sleep -Seconds 2
        
        # Extract features using multiple selectors
        $selectors = @(
            # Headers
            @{ selector = "h1, h2, h3"; name = "Headers" },
            # Feature titles
            @{ selector = ".feature-title, .feature-heading, [class*='feature'] h2, [class*='feature'] h3"; name = "Feature Titles" },
            # Strong/bold text that might be features
            @{ selector = "strong, b, .font-bold, .font-semibold"; name = "Bold Text" },
            # List items
            @{ selector = "li, .list-item, .feature-item"; name = "List Items" },
            # Cards
            @{ selector = ".card, .feature-card, .product-card"; name = "Cards" },
            # Buttons with feature-like text
            @{ selector = "button, .btn, .button"; name = "Buttons" },
            # Pricing features
            @{ selector = ".pricing-feature, .plan-feature, .enterprise-feature"; name = "Pricing Features" },
            # Changelog items
            @{ selector = ".changelog-item, .release-note, .update-item"; name = "Changelog Items" },
            # Any element with feature-related classes
            @{ selector = "[class*='feature'], [class*='capability'], [class*='functionality']"; name = "Feature Classes" },
            # Any element with feature-related IDs
            @{ selector = "[id*='feature'], [id*='capability'], [id*='functionality']"; name = "Feature IDs" }
        )
        
        $extractedCount = 0
        
        foreach ($selectorInfo in $selectors) {
            $selector = $selectorInfo.selector
            $selectorName = $selectorInfo.name
            
            Write-ColorOutput "  Extracting from $selectorName..." "Info"
            
            try {
                # Get elements
                $elements = $page.QuerySelectorAllAsync($selector).Result
                
                foreach ($element in $elements) {
                    # Get text content
                    $text = $element.InnerTextAsync().Result
                    
                    if ($text) {
                        # Clean the text
                        $cleanText = $text.Trim()
                        $cleanText = [regex]::Replace($cleanText, "\s+", " ")
                        $cleanText = [regex]::Replace($cleanText, "^\s+|\s+$", "")
                        
                        # Filter out short or irrelevant text
                        if ($cleanText.Length -ge 5 -and $cleanText.Length -le 200) {
                            # Skip common irrelevant text
                            $skipPatterns = @(
                                "^\d+$",  # Just numbers
                                "^[^a-zA-Z]+$",  # No letters
                                "^\s*$",  # Empty or whitespace
                                "^(Sign|Log|Get|Try|Learn|Contact|About|Privacy|Terms|Cookie|Copyright|Home|Back|Next|Previous)",
                                "^(pricing|subscribe|payment|billing|account|login|logout|register|signup)",
                                "^\s*[-+*=><!?&%$#@^*()\[\]{};:\"',./\\\\]+\s*$"
                            )
                            
                            $shouldSkip = $false
                            foreach ($pattern in $skipPatterns) {
                                if ($cleanText -match $pattern) {
                                    $shouldSkip = $true
                                    break
                                }
                            }
                            
                            if (-not $shouldSkip) {
                                # Add to collection
                                $FeaturesCollection.Value[$cleanText] = @{
                                    Text = $cleanText
                                    Source = $PageName
                                    Selector = $selectorName
                                    Url = $Url
                                    Length = $cleanText.Length
                                }
                                $extractedCount++
                            }
                        }
                    }
                }
            }
            catch {
                # Continue with next selector if this one fails
                continue
            }
        }
        
        # Close browser
        $browser.CloseAsync().Wait()
        
        Write-ColorOutput "✓ Extracted $extractedCount features from $PageName" "Success"
        return $true
    }
    catch {
        Write-ColorOutput "✗ Error extracting from $PageName`: $_" "Error"
        return $false
    }
}

function Get-CursorUrls {
    $urls = @{
        Features = "https://www.cursor.com/features"
        Changelog = "https://www.cursor.com/changelog"
        Pricing = "https://www.cursor.com/pricing"
        Enterprise = "https://www.cursor.com/enterprise"
    }
    
    # Filter based on parameters
    $selectedUrls = @()
    
    $selectedUrls += @{ Url = $urls.Features; Name = "Features" }
    
    if ($IncludeChangelog) {
        $selectedUrls += @{ Url = $urls.Changelog; Name = "Changelog" }
    }
    
    if ($IncludePricing) {
        $selectedUrls += @{ Url = $urls.Pricing; Name = "Pricing" }
    }
    
    if ($IncludeEnterprise) {
        $selectedUrls += @{ Url = $urls.Enterprise; Name = "Enterprise" }
    }
    
    return $selectedUrls
}

function Categorize-Features {
    param([hashtable]$Features)
    
    Write-ColorOutput "=== CATEGORIZING FEATURES ===" "Header"
    
    $categories = @{
        "AI/ML" = @('AI', 'artificial intelligence', 'machine learning', 'ML', 'neural', 'deep learning', 'natural language', 'NLP', 'GPT', 'LLM', 'model', 'prediction', 'suggestion', 'completion', 'generation', 'refactoring', 'optimization', 'analysis', 'understanding', 'intelligent', 'smart', 'context-aware', 'codebase-wide')
        "Code_Editor" = @('code', 'programming', 'development', 'IDE', 'editor', 'syntax', 'highlighting', 'completion', 'navigation', 'search', 'find', 'replace', 'snippet', 'template', 'formatting', 'style', 'linting', 'static analysis', 'multi-cursor', 'editing', 'refactoring', 'multi-line', 'inline')
        "Communication" = @('chat', 'conversation', 'assistant', 'collaboration', 'team', 'share', 'review', 'comment', 'communication', 'real-time', 'live', 'session', 'walkthrough', 'together', 'simultaneous', 'multi-user')
        "Debugging" = @('debug', 'error', 'bug', 'breakpoint', 'stack trace', 'variable', 'watch', 'inspector', 'exception', 'handling', 'recovery', 'call stack', 'performance', 'profiler', 'bottleneck', 'memory', 'leak', 'detection')
        "Testing" = @('test', 'validation', 'verification', 'assertion', 'coverage', 'mock', 'stub', 'benchmark', 'performance', 'integration', 'continuous', 'automated', 'generation', 'edge case', 'isolated', 'reporting', 'gap analysis')
        "User_Interface" = @('UI', 'interface', 'design', 'layout', 'visual', 'theme', 'customizable', 'personalization', 'font', 'color', 'scheme', 'keyboard', 'shortcut', 'touch bar', 'high DPI', 'scaling', 'monitor', 'window', 'management')
        "Performance" = @('performance', 'speed', 'optimization', 'efficient', 'fast', 'cache', 'memory', 'startup', 'loading', 'indexing', 'background', 'lazy loading', 'garbage collection', 'network', 'parallel', 'multi-core', 'CPU', 'utilization')
        "Security" = @('security', 'auth', 'encryption', 'protection', 'safe', 'vulnerability', 'credential', 'storage', 'management', 'access control', 'permissions', 'audit', 'logging', 'compliance', 'SOC 2', 'local mode', 'ghost mode', 'privacy')
        "Integration" = @('integration', 'API', 'service', 'plugin', 'extension', 'webhook', 'database', 'cloud', 'deployment', 'container', 'Docker', 'VS Code', 'compatibility', 'migration', 'external', 'tool', 'service')
        "Documentation" = @('documentation', 'explain', 'comment', 'guide', 'help', 'README', 'changelog', 'API', 'interactive', 'example', 'generation', 'project structure', 'analysis', 'inline', 'tooltip', 'explanation')
        "Advanced" = @('custom', 'rules', 'preferences', 'indexing', 'refactoring', 'impact analysis', 'advanced', 'sophisticated', 'enterprise', 'team', 'workflow', 'automation', 'agentic', 'autonomous', 'sub-agent', 'skill', 'MCP', 'protocol')
    }
    
    $categorized = @{}
    $uncategorized = @()
    
    foreach ($category in $categories.Keys) {
        $categorized[$category] = @()
    }
    
    # Categorize each feature
    foreach ($featureText in $Features.Keys) {
        $categorizedFeature = $false
        
        foreach ($category in $categories.Keys) {
            foreach ($keyword in $categories[$category]) {
                if ($featureText -match "\b$([regex]::Escape($keyword))\b" -or 
                    $featureText -match $keyword -or
                    $featureText.ToLower() -match $keyword.ToLower()) {
                    $categorized[$category] += $Features[$featureText]
                    $categorizedFeature = $true
                    break
                }
            }
            if ($categorizedFeature) { break }
        }
        
        if (-not $categorizedFeature) {
            $uncategorized += $Features[$featureText]
        }
    }
    
    Write-ColorOutput "✓ Categorized $($Features.Count) features" "Success"
    
    return @{
        Categorized = $categorized
        Uncategorized = $uncategorized
    }
}

function Generate-ComprehensiveReport {
    param(
        [hashtable]$CategorizedFeatures,
        [string]$OutputPath
    )
    
    Write-ColorOutput "=== GENERATING COMPREHENSIVE REPORT ===" "Header"
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $totalFeatures = $CategorizedFeatures.Categorized.Values | ForEach-Object { $_.Count } | Measure-Object -Sum | Select-Object -ExpandProperty Sum
    $totalFeatures += $CategorizedFeatures.Uncategorized.Count
    
    $report = @()
    $report += "=" * 80
    $report += "CURSOR IDE COMPLETE FEATURES ANALYSIS"
    $report += "Generated: $timestamp"
    $report += "Total Features Extracted: $totalFeatures"
    $report += "=" * 80
    $report += ""
    
    # Executive Summary
    $report += "EXECUTIVE SUMMARY"
    $report += "-" * 40
    $report += "This report contains a comprehensive analysis of all Cursor IDE features"
    $report += "extracted from multiple sources including the main features page,"
    $report += "changelog, pricing, and enterprise sections."
    $report += ""
    $report += "Key Findings:"
    $report += "• Total Features: $totalFeatures"
    $report += "• Categories Identified: $($CategorizedFeatures.Categorized.Keys.Count)"
    $report += "• Uncategorized Features: $($CategorizedFeatures.Uncategorized.Count)"
    $report += ""
    
    # Features by Category
    $report += "FEATURES BY CATEGORY"
    $report += "-" * 40
    $report += ""
    
    $featureNumber = 1
    foreach ($category in $CategorizedFeatures.Categorized.Keys) {
        $features = $CategorizedFeatures.Categorized[$category]
        $count = $features.Count
        
        if ($count -gt 0) {
            $percentage = [math]::Round(($count / $totalFeatures) * 100, 1)
            $report += "$category`: $count features ($percentage%)"
            $report += ""
            
            foreach ($feature in $features) {
                $report += "  $featureNumber. $($feature.Text)"
                $report += "     Source: $($feature.Source) | $($feature.Selector)"
                $featureNumber++
            }
            $report += ""
        }
    }
    
    # Uncategorized Features
    if ($CategorizedFeatures.Uncategorized.Count -gt 0) {
        $report += "UNCATEGORIZED FEATURES"
        $report += "-" * 40
        $report += ""
        
        foreach ($feature in $CategorizedFeatures.Uncategorized) {
            $report += "  $featureNumber. $($feature.Text)"
            $report += "     Source: $($feature.Source) | $($feature.Selector)"
            $featureNumber++
        }
        $report += ""
    }
    
    # Implementation Recommendations
    $report += "IMPLEMENTATION RECOMMENDATIONS"
    $report += "-" * 40
    $report += ""
    $report += "Based on the feature analysis, here are the recommended implementation phases:"
    $report += ""
    
    $phases = @(
        @{
            Name = "Phase 1: Core Editor Foundation"
            Categories = @("Code_Editor", "User_Interface")
            Priority = "Critical"
            EstimatedEffort = "2-3 weeks"
        },
        @{
            Name = "Phase 2: AI Integration"
            Categories = @("AI/ML", "Documentation")
            Priority = "High"
            EstimatedEffort = "3-4 weeks"
        },
        @{
            Name = "Phase 3: Developer Tools"
            Categories = @("Debugging", "Testing")
            Priority = "High"
            EstimatedEffort = "2-3 weeks"
        },
        @{
            Name = "Phase 4: Collaboration & Integration"
            Categories = @("Communication", "Integration")
            Priority = "Medium"
            EstimatedEffort = "2-3 weeks"
        },
        @{
            Name = "Phase 5: Performance & Security"
            Categories = @("Performance", "Security")
            Priority = "Medium"
            EstimatedEffort = "2-3 weeks"
        },
        @{
            Name = "Phase 6: Advanced Features"
            Categories = @("Advanced")
            Priority = "Low"
            EstimatedEffort = "3-4 weeks"
        }
    )
    
    foreach ($phase in $phases) {
        $report += $phase.Name
        $report += "Priority: $($phase.Priority)"
        $report += "Estimated Effort: $($phase.EstimatedEffort)"
        $report += "Categories: $($phase.Categories -join ', ')"
        $report += ""
        
        $phaseFeatures = 0
        foreach ($category in $phase.Categories) {
            if ($CategorizedFeatures.Categorized.ContainsKey($category)) {
                $phaseFeatures += $CategorizedFeatures.Categorized[$category].Count
            }
        }
        
        $report += "Features in this phase: $phaseFeatures"
        $report += ""
    }
    
    # Technical Recommendations
    $report += "TECHNICAL RECOMMENDATIONS"
    $report += "-" * 40
    $report += ""
    $report += "Architecture:"
    $report += "• Use modular architecture for each category"
    $report += "• Implement plugin system for extensions"
    $report += "• Use dependency injection for testability"
    $report += ""
    $report += "AI/ML Integration:"
    $aiFeatureCount = $CategorizedFeatures.Categorized["AI/ML"].Count
    $report += "• Features requiring AI: $aiFeatureCount"
    $report += "• Recommended models: GPT-4, Claude, or local LLMs"
    $report += "• Implement model abstraction layer for flexibility"
    $report += ""
    $report += "Hotpatching Support:"
    $report += "• Design components to be hotpatchable"
    $report += "• Use interface-based design"
    $report += "• Implement version control for components"
    $report += ""
    $report += "Continuous Integration:"
    $report += "• Automated testing for each feature"
    $report += "• Performance benchmarking"
    $report += "• Security scanning"
    $report += ""
    
    # Save report
    $report | Out-File -FilePath $OutputPath -Encoding UTF8
    
    Write-ColorOutput "✓ Comprehensive report saved to: $OutputPath" "Success"
    Write-ColorOutput "  Report size: $((Get-Item $OutputPath).Length) bytes" "Detail"
}

# Main execution
Write-ColorOutput "=" * 80
Write-ColorOutput "CURSOR IDE COMPLETE FEATURES EXTRACTION"
Write-ColorOutput "Starting at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-ColorOutput "=" * 80
Write-ColorOutput ""

# Check if Playwright needs to be installed
if ($InstallPlaywright) {
    $playwrightReady = Install-PlaywrightIfNeeded
    if (-not $playwrightReady) {
        Write-ColorOutput "✗ Playwright installation failed. Exiting." "Error"
        exit 1
    }
}

# Import Playwright module
try {
    Import-Module Microsoft.Playwright.PowerShell -ErrorAction Stop
    Write-ColorOutput "✓ Playwright module imported" "Success"
}
catch {
    Write-ColorOutput "✗ Failed to import Playwright module: $_" "Error"
    Write-ColorOutput "  Run with -InstallPlaywright flag to install" "Detail"
    exit 1
}

# Get URLs to extract from
$urls = Get-CursorUrls

Write-ColorOutput "URLs to extract from: $($urls.Count)" "Info"
foreach ($urlInfo in $urls) {
    Write-ColorOutput "  - $($urlInfo.Name): $($urlInfo.Url)" "Detail"
}
Write-ColorOutput ""

# Extract features from each URL
$allFeatures = @{}
$successCount = 0

foreach ($urlInfo in $urls) {
    $success = Extract-FeaturesFromUrl -Url $urlInfo.Url -PageName $urlInfo.Name -FeaturesCollection ([ref]$allFeatures)
    
    if ($success) {
        $successCount++
    }
    
    Write-ColorOutput ""
}

# Show extraction summary
Write-ColorOutput "=" * 80
Write-ColorOutput "EXTRACTION SUMMARY"
Write-ColorOutput "-" * 40
Write-ColorOutput "Total URLs processed: $($urls.Count)"
Write-ColorOutput "Successful extractions: $successCount"
Write-ColorOutput "Total features extracted: $($allFeatures.Count)"
Write-ColorOutput "=" * 80
Write-ColorOutput ""

if ($allFeatures.Count -eq 0) {
    Write-ColorOutput "✗ No features extracted. Exiting." "Error"
    exit 1
}

# Categorize features
Write-ColorOutput "Categorizing features..." "Info"
$categorizedResult = Categorize-Features -Features $allFeatures

Write-ColorOutput "✓ Features categorized" "Success"
Write-ColorOutput "  Categories: $($categorizedResult.Categorized.Keys.Count)"
Write-ColorOutput "  Uncategorized: $($categorizedResult.Uncategorized.Count)"
Write-ColorOutput ""

# Show category breakdown
Write-ColorOutput "FEATURES BY CATEGORY"
Write-ColorOutput "-" * 40
foreach ($category in $categorizedResult.Categorized.Keys) {
    $count = $categorizedResult.Categorized[$category].Count
    if ($count -gt 0) {
        $percentage = [math]::Round(($count / $allFeatures.Count) * 100, 1)
        Write-ColorOutput "$($category.PadRight(20)) $count features ($percentage%)" "Category"
    }
}
if ($categorizedResult.Uncategorized.Count -gt 0) {
    $percentage = [math]::Round(($categorizedResult.Uncategorized.Count / $allFeatures.Count) * 100, 1)
    Write-ColorOutput "$("Uncategorized".PadRight(20)) $($categorizedResult.Uncategorized.Count) features ($percentage%)" "Category"
}
Write-ColorOutput ""

# Generate output file
if (-not $OutputFile) {
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputFile = "cursor_features_complete_$timestamp.txt"
}

# Generate comprehensive report
Generate-ComprehensiveReport -CategorizedFeatures $categorizedResult -OutputPath $OutputFile

Write-ColorOutput ""
Write-ColorOutput "=" * 80
Write-ColorOutput "EXTRACTION COMPLETE"
Write-ColorOutput "Completed at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-ColorOutput "=" * 80
Write-ColorOutput ""
Write-ColorOutput "Next steps:" "Info"
Write-ColorOutput "1. Review the extracted features in: $OutputFile" "Detail"
Write-ColorOutput "2. Use the categorized features for implementation planning" "Detail"
Write-ColorOutput "3. Feed the features to the autonomous agentic engine" "Detail"
Write-ColorOutput "4. Prioritize features based on categories and requirements" "Detail"
Write-ColorOutput ""
Write-ColorOutput "Happy building! 🚀" "Success"
