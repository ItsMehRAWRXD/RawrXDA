#Requires -Version 7.0
<#
.SYNOPSIS
    Builds Cursor IDE features using the autonomous agentic engine via PowerShell CLI
.DESCRIPTION
    Fetches Cursor's complete feature list and feeds it to the autonomous agentic engine
to build all features while we sit back and watch.
#>

param(
    [string]$FeaturesFile = "cursor_features.txt",
    [string]$OutputDirectory = "Cursor_Features_Build",
    [switch]$SkipFetch,
    [switch]$ShowProgress
)

# Set strict mode for better error handling
Set-StrictMode -Version Latest

# Color codes for output
$Colors = @{
    Info = "Cyan"
    Success = "Green"
    Warning = "Yellow"
    Error = "Red"
    Header = "Magenta"
    Detail = "White"
}

function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Type = "Info"
    )
    Write-Host $Message -ForegroundColor $Colors[$Type]
}

function Fetch-CursorFeatures {
    Write-ColorOutput "=== FETCHING CURSOR FEATURES ===" "Header"
    Write-ColorOutput "Connecting to Cursor website..." "Info"
    
    try {
        # Use Invoke-WebRequest to fetch Cursor's features page
        $cursorUrl = "https://www.cursor.com/features"
        $response = Invoke-WebRequest -Uri $cursorUrl -UseBasicParsing -TimeoutSec 30
        
        if ($response.StatusCode -eq 200) {
            Write-ColorOutput "✓ Successfully connected to Cursor website" "Success"
            
            # Extract features from the page content
            $content = $response.Content
            
            # Look for feature sections, headings, and descriptions
            $features = @()
            
            # Pattern to find feature headings and descriptions
            $patterns = @(
                '<h[1-3][^>]*>(?<feature>[^<]+)</h[1-3]>',
                '<div[^>]*class="[^"]*feature[^"]*"[^>]*>(?<feature>.*?)</div>',
                '<p[^>]*>(?<description>[^<]+)</p>'
            )
            
            foreach ($pattern in $patterns) {
                $matches = [regex]::Matches($content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
                foreach ($match in $matches) {
                    if ($match.Groups['feature'].Success) {
                        $features += $match.Groups['feature'].Value.Trim()
                    }
                    if ($match.Groups['description'].Success) {
                        $features += $match.Groups['description'].Value.Trim()
                    }
                }
            }
            
            # Remove duplicates and empty entries
            $uniqueFeatures = $features | Where-Object { $_ -and $_.Length -gt 10 } | Select-Object -Unique
            
            Write-ColorOutput "✓ Extracted $($uniqueFeatures.Count) unique features" "Success"
            
            # Save features to file
            $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
            $featuresFileName = "cursor_features_$timestamp.txt"
            
            $output = @()
            $output += "=== CURSOR IDE FEATURES ==="
            $output += "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
            $output += "Source: $cursorUrl"
            $output += ""
            $output += "=== FEATURE LIST ==="
            $output += ""
            
            $featureNumber = 1
            foreach ($feature in $uniqueFeatures) {
                $output += "$featureNumber. $feature"
                $featureNumber++
            }
            
            $output | Out-File -FilePath $featuresFileName -Encoding UTF8
            
            Write-ColorOutput "✓ Features saved to: $featuresFileName" "Success"
            Write-ColorOutput "  Total features: $($uniqueFeatures.Count)" "Detail"
            
            return $featuresFileName
        }
        else {
            Write-ColorOutput "✗ Failed to connect to Cursor website (Status: $($response.StatusCode))" "Error"
            return $null
        }
    }
    catch {
        Write-ColorOutput "✗ Error fetching Cursor features: $_" "Error"
        Write-ColorOutput "  Using fallback feature list..." "Warning"
        
        # Create fallback features file
        return Create-FallbackFeaturesFile
    }
}

function Create-FallbackFeaturesFile {
    Write-ColorOutput "Creating fallback feature list based on known Cursor capabilities..." "Info"
    
    $fallbackFeatures = @(
        "AI-Powered Code Completion with context-aware suggestions",
        "Natural Language Code Editing - edit code using plain English descriptions",
        "Multi-line code generation and refactoring",
        "Integrated AI Chat for code explanations and debugging help",
        "Inline AI Assistant for real-time code suggestions",
        "Codebase-wide AI understanding and navigation",
        "Smart code explanation and documentation generation",
        "AI-powered bug detection and fixing suggestions",
        "Natural language to code conversion",
        "Context-aware code refactoring recommendations",
        "AI-assisted test generation",
        "Code optimization suggestions powered by AI",
        "Integrated terminal with AI assistance",
        "Git integration with AI-powered commit message generation",
        "Multi-language support with AI understanding",
        "Real-time collaboration features",
        "Customizable AI model selection",
        "Privacy-focused local AI processing options",
        "Seamless VS Code extension compatibility",
        "Intelligent code navigation and search",
        "AI-powered code review assistance",
        "Automated code documentation generation",
        "Smart error explanation and resolution suggestions",
        "Codebase indexing for instant AI understanding",
        "Custom AI rules and preferences",
        "Integration with popular development tools and services",
        "AI-powered code snippet generation and management",
        "Intelligent import and dependency management",
        "Context-aware variable and function naming suggestions",
        "AI-assisted debugging with breakpoint suggestions"
    )
    
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $featuresFileName = "cursor_features_fallback_$timestamp.txt"
    
    $output = @()
    $output += "=== CURSOR IDE FEATURES (FALLBACK) ==="
    $output += "Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    $output += "Source: Fallback list based on known Cursor capabilities"
    $output += ""
    $output += "=== FEATURE LIST ==="
    $output += ""
    
    $featureNumber = 1
    foreach ($feature in $fallbackFeatures) {
        $output += "$featureNumber. $feature"
        $featureNumber++
    }
    
    $output | Out-File -FilePath $featuresFileName -Encoding UTF8
    
    Write-ColorOutput "✓ Fallback features saved to: $featuresFileName" "Success"
    Write-ColorOutput "  Total features: $($fallbackFeatures.Count)" "Detail"
    
    return $featuresFileName
}

function Invoke-AutonomousAgenticEngine {
    param(
        [string]$FeaturesFile,
        [string]$OutputDir
    )
    
    Write-ColorOutput "=== INVOKING AUTONOMOUS AGENTIC ENGINE ===" "Header"
    Write-ColorOutput "Features file: $FeaturesFile" "Info"
    Write-ColorOutput "Output directory: $OutputDir" "Info"
    Write-ColorOutput ""
    
    # Check if the autonomous agent script exists
    $agentScript = "RawrXD.AutonomousAgent.ps1"
    if (-not (Test-Path $agentScript)) {
        Write-ColorOutput "✗ Autonomous agent script not found: $agentScript" "Error"
        Write-ColorOutput "  Looking for alternative..." "Warning"
        
        # Try the consolidated version
        $agentScript = "Consolidated_Modules_$(Get-Date -Format 'yyyyMMdd')\02_Agentic\RawrXD.AutonomousAgent.psm1"
        if (-not (Test-Path $agentScript)) {
            Write-ColorOutput "✗ Alternative not found either" "Error"
            return $false
        }
    }
    
    Write-ColorOutput "✓ Found autonomous agent: $agentScript" "Success"
    Write-ColorOutput ""
    
    # Read features from file
    try {
        $features = Get-Content -Path $FeaturesFile -Raw
        $featureLines = Get-Content -Path $FeaturesFile
        $featureCount = ($featureLines | Where-Object { $_ -match '^\d+\.' }).Count
        
        Write-ColorOutput "✓ Loaded $featureCount features from file" "Success"
        Write-ColorOutput ""
    }
    catch {
        Write-ColorOutput "✗ Error reading features file: $_" "Error"
        return $false
    }
    
    # Create output directory
    if (-not (Test-Path $OutputDir)) {
        New-Item -Path $OutputDir -ItemType Directory -Force | Out-Null
        Write-ColorOutput "✓ Created output directory: $OutputDir" "Success"
    }
    
    # Build the command for the autonomous agent
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $logFile = Join-Path $OutputDir "build_log_$timestamp.txt"
    $progressFile = Join-Path $OutputDir "build_progress_$timestamp.json"
    
    $agentParams = @{
        Mode = "agentic"
        Task = "Build Cursor IDE Features"
        InputFile = $FeaturesFile
        OutputDirectory = $OutputDir
        LogFile = $logFile
        ProgressFile = $progressFile
        ShowProgress = $ShowProgress
    }
    
    Write-ColorOutput "=== STARTING BUILD PROCESS ===" "Header"
    Write-ColorOutput "This will take a while. Sit back and watch the magic happen!" "Info"
    Write-ColorOutput ""
    Write-ColorOutput "Build log: $logFile" "Detail"
    Write-ColorOutput "Progress tracking: $progressFile" "Detail"
    Write-ColorOutput ""
    
    # Execute the autonomous agent
    try {
        if ($agentScript -like "*.psm1") {
            # Import the module and invoke
            Import-Module $agentScript -Force
            $result = Invoke-AutonomousAgent @agentParams
        }
        else {
            # Execute the script directly
            $result = & $agentScript @agentParams
        }
        
        if ($result) {
            Write-ColorOutput "✓ Autonomous agent completed successfully" "Success"
            Write-ColorOutput "  Check $logFile for detailed build log" "Detail"
            Write-ColorOutput "  Check $progressFile for build progress" "Detail"
            return $true
        }
        else {
            Write-ColorOutput "✗ Autonomous agent failed to complete" "Error"
            return $false
        }
    }
    catch {
        Write-ColorOutput "✗ Error executing autonomous agent: $_" "Error"
        return $false
    }
}

function Show-BuildSummary {
    param(
        [string]$OutputDir,
        [string]$FeaturesFile
    )
    
    Write-ColorOutput "=== BUILD SUMMARY ===" "Header"
    Write-ColorOutput "Features file: $FeaturesFile" "Detail"
    Write-ColorOutput "Output directory: $OutputDir" "Detail"
    Write-ColorOutput ""
    
    # Count files in output directory
    if (Test-Path $OutputDir) {
        $files = Get-ChildItem -Path $OutputDir -Recurse -File
        $directories = Get-ChildItem -Path $OutputDir -Recurse -Directory
        
        Write-ColorOutput "✓ Build completed" "Success"
        Write-ColorOutput "  Files created: $($files.Count)" "Detail"
        Write-ColorOutput "  Directories created: $($directories.Count)" "Detail"
        Write-ColorOutput "  Total size: $([math]::Round((($files | Measure-Object -Property Length -Sum).Sum / 1MB), 2)) MB" "Detail"
        Write-ColorOutput ""
        
        # Show top-level directory structure
        Write-ColorOutput "=== OUTPUT STRUCTURE ===" "Header"
        Get-ChildItem -Path $OutputDir | Format-Table Name, @{Label="Type";Expression={if ($_.PSIsContainer) { "Directory" } else { "File" }}}, @{Label="Size";Expression={if ($_.PSIsContainer) { "" } else { "$([math]::Round($_.Length / 1KB, 1)) KB" }}}
    }
    else {
        Write-ColorOutput "✗ Output directory not found" "Error"
    }
}

# Main execution
Write-ColorOutput "=== CURSOR FEATURES BUILD ORCHESTRATOR ===" "Header"
Write-ColorOutput "Starting build process at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "Info"
Write-ColorOutput ""

# Step 1: Fetch Cursor features (or use provided file)
if ($SkipFetch -and (Test-Path $FeaturesFile)) {
    Write-ColorOutput "✓ Using existing features file: $FeaturesFile" "Success"
    $featuresFilePath = $FeaturesFile
}
else {
    $featuresFilePath = Fetch-CursorFeatures
    
    if (-not $featuresFilePath) {
        Write-ColorOutput "✗ Failed to fetch features. Exiting." "Error"
        exit 1
    }
}

Write-ColorOutput ""

# Step 2: Invoke autonomous agentic engine
$buildSuccess = Invoke-AutonomousAgenticEngine -FeaturesFile $featuresFilePath -OutputDir $OutputDirectory

if (-not $buildSuccess) {
    Write-ColorOutput "✗ Build failed. Check logs for details." "Error"
    exit 1
}

Write-ColorOutput ""

# Step 3: Show build summary
Show-BuildSummary -OutputDir $OutputDirectory -FeaturesFile $featuresFilePath

Write-ColorOutput ""
Write-ColorOutput "=== BUILD PROCESS COMPLETE ===" "Header"
Write-ColorOutput "Completed at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "Info"
Write-ColorOutput ""
Write-ColorOutput "Next steps:" "Info"
Write-ColorOutput "1. Review the built features in: $OutputDirectory" "Detail"
Write-ColorOutput "2. Test the features thoroughly" "Detail"
Write-ColorOutput "3. Check build logs for any warnings or issues" "Detail"
Write-ColorOutput "4. Integrate with your existing IDE if needed" "Detail"
Write-ColorOutput ""
Write-ColorOutput "Happy coding! 🚀" "Success"
