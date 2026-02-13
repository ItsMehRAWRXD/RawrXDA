#Requires -Version 7.0
<#
.SYNOPSIS
    Extract Chat Pane, Agentic Features, and GitHub Copilot Integration Code
.DESCRIPTION
    Extracts specific features from Cursor source code:
    - Chat pane implementation
    - Agentic features (cursor-agent extension)
    - GitHub Copilot integration
    - AI services and APIs
    - Extension system
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$SourceDirectory,
    
    [string]$OutputDirectory = "Extracted_Features",
    
    [switch]$ExtractChatPane,
    [switch]$ExtractAgentFeatures,
    [switch]$ExtractGitHubCopilot,
    [switch]$ExtractAIServices,
    [switch]$ExtractExtensionSystem,
    [switch]$ExtractAll,
    [switch]$ShowProgress,
    [switch]$GenerateReport
)

# Color codes
$Colors = @{
    Info = "Cyan"
    Success = "Green"
    Warning = "Yellow"
    Error = "Red"
    Header = "Magenta"
    Detail = "White"
    Chat = "Blue"
    Agent = "DarkGreen"
    GitHub = "DarkCyan"
    AI = "DarkYellow"
    Extension = "DarkMagenta"
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
        Write-Progress -Activity $Activity -Status $Status -PercentComplete $percentComplete -CurrentOperation $currentOperation
    }
}

function Extract-ChatPane {
    param([string]$SourceDir, [string]$OutputDir)
    
    Write-ColorOutput "=== EXTRACTING CHAT PANE ===" "Header"
    
    $chatOutputDir = Join-Path $OutputDir "chat_pane"
    New-Item -Path $chatOutputDir -ItemType Directory -Force | Out-Null
    
    $chatFiles = @()
    
    # Look for chat-related files
    $chatPatterns = @(
        "*chat*",
        "*conversation*",
        "*message*",
        "*panel*",
        "*sidebar*",
        "*webview*"
    )
    
    $searchDirs = @(
        "out/vs/workbench/contrib",
        "out/vs/workbench/services",
        "out/vs/workbench/browser",
        "out/vs/workbench/parts",
        "extensions"
    )
    
    foreach ($searchDir in $searchDirs) {
        $fullSearchDir = Join-Path $SourceDir $searchDir
        if (Test-Path $fullSearchDir) {
            foreach ($pattern in $chatPatterns) {
                $files = Get-ChildItem -Path $fullSearchDir -Recurse -Filter $pattern -File -ErrorAction SilentlyContinue
                foreach ($file in $files) {
                    if ($file.Extension -in @(".js", ".ts", ".jsx", ".tsx", ".css", ".scss", ".html")) {
                        $relativePath = $file.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
                        $outputPath = Join-Path $chatOutputDir $relativePath
                        $outputParent = Split-Path $outputPath -Parent
                        
                        if (-not (Test-Path $outputParent)) {
                            New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
                        }
                        
                        Copy-Item -Path $file.FullName -Destination $outputPath -Force
                        $chatFiles += $relativePath
                    }
                }
            }
        }
    }
    
    Write-ColorOutput "✓ Extracted $($chatFiles.Count) chat-related files" "Success"
    return $chatFiles
}

function Extract-AgentFeatures {
    param([string]$SourceDir, [string]$OutputDir)
    
    Write-ColorOutput "=== EXTRACTING AGENTIC FEATURES ===" "Header"
    
    $agentOutputDir = Join-Path $OutputDir "agent_features"
    New-Item -Path $agentOutputDir -ItemType Directory -Force | Out-Null
    
    $agentFiles = @()
    
    # Look for cursor-agent extension
    $agentExtensionDir = Join-Path $SourceDir "extensions/cursor-agent"
    if (Test-Path $agentExtensionDir) {
        Write-ColorOutput "→ Found cursor-agent extension" "Agent"
        
        # Copy entire extension
        $extensionOutputDir = Join-Path $agentOutputDir "cursor-agent"
        Copy-Item -Path $agentExtensionDir -Destination $extensionOutputDir -Recurse -Force
        
        # Get all files in the extension
        $files = Get-ChildItem -Path $agentExtensionDir -Recurse -File
        $agentFiles += $files | ForEach-Object { $_.FullName.Substring($SourceDir.Length).TrimStart('\', '/') }
        
        Write-ColorOutput "✓ Extracted cursor-agent extension ($($files.Count) files)" "Success"
    }
    
    # Look for cursor-agent-exec extension
    $agentExecDir = Join-Path $SourceDir "extensions/cursor-agent-exec"
    if (Test-Path $agentExecDir) {
        Write-ColorOutput "→ Found cursor-agent-exec extension" "Agent"
        
        $execOutputDir = Join-Path $agentOutputDir "cursor-agent-exec"
        Copy-Item -Path $agentExecDir -Destination $execOutputDir -Recurse -Force
        
        $files = Get-ChildItem -Path $agentExecDir -Recurse -File
        $agentFiles += $files | ForEach-Object { $_.FullName.Substring($SourceDir.Length).TrimStart('\', '/') }
        
        Write-ColorOutput "✓ Extracted cursor-agent-exec extension ($($files.Count) files)" "Success"
    }
    
    # Look for agent-related files in out directory
    $agentPatterns = @(
        "*agent*",
        "*autonomous*",
        "*automation*",
        "*task*",
        "*workflow*"
    )
    
    $outDir = Join-Path $SourceDir "out"
    if (Test-Path $outDir) {
        foreach ($pattern in $agentPatterns) {
            $files = Get-ChildItem -Path $outDir -Recurse -Filter $pattern -File -ErrorAction SilentlyContinue
            foreach ($file in $files) {
                if ($file.Extension -in @(".js", ".ts")) {
                    $relativePath = $file.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
                    $outputPath = Join-Path $agentOutputDir $relativePath
                    $outputParent = Split-Path $outputPath -Parent
                    
                    if (-not (Test-Path $outputParent)) {
                        New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
                    }
                    
                    Copy-Item -Path $file.FullName -Destination $outputPath -Force
                    $agentFiles += $relativePath
                }
            }
        }
    }
    
    Write-ColorOutput "✓ Extracted $($agentFiles.Count) agent-related files" "Success"
    return $agentFiles
}

function Extract-GitHubCopilot {
    param([string]$SourceDir, [string]$OutputDir)
    
    Write-ColorOutput "=== EXTRACTING GITHUB COPILOT INTEGRATION ===" "Header"
    
    $githubOutputDir = Join-Path $OutputDir "github_copilot"
    New-Item -Path $githubOutputDir -ItemType Directory -Force | Out-Null
    
    $githubFiles = @()
    
    # Look for GitHub extensions
    $githubExtensionDir = Join-Path $SourceDir "extensions/github"
    if (Test-Path $githubExtensionDir) {
        Write-ColorOutput "→ Found GitHub extension" "GitHub"
        
        $extensionOutputDir = Join-Path $githubOutputDir "github"
        Copy-Item -Path $githubExtensionDir -Destination $extensionOutputDir -Recurse -Force
        
        $files = Get-ChildItem -Path $githubExtensionDir -Recurse -File
        $githubFiles += $files | ForEach-Object { $_.FullName.Substring($SourceDir.Length).TrimStart('\', '/') }
        
        Write-ColorOutput "✓ Extracted GitHub extension ($($files.Count) files)" "Success"
    }
    
    # Look for GitHub authentication extension
    $githubAuthDir = Join-Path $SourceDir "extensions/github-authentication"
    if (Test-Path $githubAuthDir) {
        Write-ColorOutput "→ Found GitHub authentication extension" "GitHub"
        
        $authOutputDir = Join-Path $githubOutputDir "github-authentication"
        Copy-Item -Path $githubAuthDir -Destination $authOutputDir -Recurse -Force
        
        $files = Get-ChildItem -Path $githubAuthDir -Recurse -File
        $githubFiles += $files | ForEach-Object { $_.FullName.Substring($SourceDir.Length).TrimStart('\', '/') }
        
        Write-ColorOutput "✓ Extracted GitHub authentication extension ($($files.Count) files)" "Success"
    }
    
    # Look for copilot-related files
    $copilotPatterns = @(
        "*copilot*",
        "*completion*",
        "*suggestion*",
        "*inline*",
        "*ghost*"
    )
    
    $outDir = Join-Path $SourceDir "out"
    if (Test-Path $outDir) {
        foreach ($pattern in $copilotPatterns) {
            $files = Get-ChildItem -Path $outDir -Recurse -Filter $pattern -File -ErrorAction SilentlyContinue
            foreach ($file in $files) {
                if ($file.Extension -in @(".js", ".ts")) {
                    $relativePath = $file.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
                    $outputPath = Join-Path $githubOutputDir $relativePath
                    $outputParent = Split-Path $outputPath -Parent
                    
                    if (-not (Test-Path $outputParent)) {
                        New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
                    }
                    
                    Copy-Item -Path $file.FullName -Destination $outputPath -Force
                    $githubFiles += $relativePath
                }
            }
        }
    }
    
    Write-ColorOutput "✓ Extracted $($githubFiles.Count) GitHub/Copilot-related files" "Success"
    return $githubFiles
}

function Extract-AIServices {
    param([string]$SourceDir, [string]$OutputDir)
    
    Write-ColorOutput "=== EXTRACTING AI SERVICES ===" "Header"
    
    $aiOutputDir = Join-Path $OutputDir "ai_services"
    New-Item -Path $aiOutputDir -ItemType Directory -Force | Out-Null
    
    $aiFiles = @()
    
    # Look for AI services directory
    $aiServicesDir = Join-Path $SourceDir "out/vs/workbench/services/ai"
    if (Test-Path $aiServicesDir) {
        Write-ColorOutput "→ Found AI services directory" "AI"
        
        $servicesOutputDir = Join-Path $aiOutputDir "services"
        Copy-Item -Path $aiServicesDir -Destination $servicesOutputDir -Recurse -Force
        
        $files = Get-ChildItem -Path $aiServicesDir -Recurse -File
        $aiFiles += $files | ForEach-Object { $_.FullName.Substring($SourceDir.Length).TrimStart('\', '/') }
        
        Write-ColorOutput "✓ Extracted AI services ($($files.Count) files)" "Success"
    }
    
    # Look for AI-related node_modules
    $aiModules = @(
        "*openai*",
        "*anthropic*",
        "*claude*",
        "*gpt*",
        "*llm*",
        "*ai*",
        "*langchain*",
        "*cohere*",
        "*huggingface*"
    )
    
    $nodeModulesDir = Join-Path $SourceDir "node_modules"
    if (Test-Path $nodeModulesDir) {
        foreach ($pattern in $aiModules) {
            $dirs = Get-ChildItem -Path $nodeModulesDir -Recurse -Directory -Filter $pattern -ErrorAction SilentlyContinue
            foreach ($dir in $dirs) {
                $relativePath = $dir.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
                $outputPath = Join-Path $aiOutputDir $relativePath
                
                Copy-Item -Path $dir.FullName -Destination $outputPath -Recurse -Force
                
                $files = Get-ChildItem -Path $dir.FullName -Recurse -File
                $aiFiles += $files | ForEach-Object { $_.FullName.Substring($SourceDir.Length).TrimStart('\', '/') }
                
                Write-ColorOutput "→ Extracted AI module: $($dir.Name)" "AI"
            }
        }
    }
    
    # Look for AI-related files in out directory
    $aiPatterns = @(
        "*ai*",
        "*llm*",
        "*model*",
        "*completion*",
        "*generation*",
        "*embedding*",
        "*vector*",
        "*semantic*"
    )
    
    $outDir = Join-Path $SourceDir "out"
    if (Test-Path $outDir) {
        foreach ($pattern in $aiPatterns) {
            $files = Get-ChildItem -Path $outDir -Recurse -Filter $pattern -File -ErrorAction SilentlyContinue
            foreach ($file in $files) {
                if ($file.Extension -in @(".js", ".ts")) {
                    $relativePath = $file.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
                    $outputPath = Join-Path $aiOutputDir $relativePath
                    $outputParent = Split-Path $outputPath -Parent
                    
                    if (-not (Test-Path $outputParent)) {
                        New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
                    }
                    
                    Copy-Item -Path $file.FullName -Destination $outputPath -Force
                    $aiFiles += $relativePath
                }
            }
        }
    }
    
    Write-ColorOutput "✓ Extracted $($aiFiles.Count) AI-related files" "Success"
    return $aiFiles
}

function Extract-ExtensionSystem {
    param([string]$SourceDir, [string]$OutputDir)
    
    Write-ColorOutput "=== EXTRACTING EXTENSION SYSTEM ===" "Header"
    
    $extensionOutputDir = Join-Path $OutputDir "extension_system"
    New-Item -Path $extensionOutputDir -ItemType Directory -Force | Out-Null
    
    $extensionFiles = @()
    
    # Copy all extensions
    $extensionsDir = Join-Path $SourceDir "extensions"
    if (Test-Path $extensionsDir) {
        Write-ColorOutput "→ Found extensions directory" "Extension"
        
        $allExtensionsDir = Join-Path $extensionOutputDir "all_extensions"
        Copy-Item -Path $extensionsDir -Destination $allExtensionsDir -Recurse -Force
        
        $files = Get-ChildItem -Path $extensionsDir -Recurse -File
        $extensionFiles += $files | ForEach-Object { $_.FullName.Substring($SourceDir.Length).TrimStart('\', '/') }
        
        Write-ColorOutput "✓ Extracted all extensions ($($files.Count) files)" "Success"
    }
    
    # Look for extension management code
    $extensionMgmtPatterns = @(
        "*extension*",
        "*plugin*",
        "*addon*"
    )
    
    $outDir = Join-Path $SourceDir "out"
    if (Test-Path $outDir) {
        foreach ($pattern in $extensionMgmtPatterns) {
            $files = Get-ChildItem -Path $outDir -Recurse -Filter $pattern -File -ErrorAction SilentlyContinue
            foreach ($file in $files) {
                if ($file.Extension -in @(".js", ".ts")) {
                    $relativePath = $file.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
                    $outputPath = Join-Path $extensionOutputDir $relativePath
                    $outputParent = Split-Path $outputPath -Parent
                    
                    if (-not (Test-Path $outputParent)) {
                        New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
                    }
                    
                    Copy-Item -Path $file.FullName -Destination $outputPath -Force
                    $extensionFiles += $relativePath
                }
            }
        }
    }
    
    Write-ColorOutput "✓ Extracted $($extensionFiles.Count) extension-related files" "Success"
    return $extensionFiles
}

function Generate-FeatureReport {
    param(
        $ChatFiles,
        $AgentFiles,
        $GitHubFiles,
        $AIFiles,
        $ExtensionFiles
    )
    
    Write-ColorOutput "=== GENERATING FEATURE EXTRACTION REPORT ===" "Header"
    
    $report = @{
        Metadata = @{
            Tool = "Extract-Chat-Agent-Features.ps1"
            Version = "1.0"
            GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            SourceDirectory = $SourceDirectory
        }
        Summary = @{
            ChatFiles = $ChatFiles.Count
            AgentFiles = $AgentFiles.Count
            GitHubFiles = $GitHubFiles.Count
            AIFiles = $AIFiles.Count
            ExtensionFiles = $ExtensionFiles.Count
            TotalFiles = $ChatFiles.Count + $AgentFiles.Count + $GitHubFiles.Count + $AIFiles.Count + $ExtensionFiles.Count
        }
        Features = @{
            ChatPane = $ChatFiles
            AgentFeatures = $AgentFiles
            GitHubCopilot = $GitHubFiles
            AIServices = $AIFiles
            ExtensionSystem = $ExtensionFiles
        }
    }
    
    # Save report
    $reportPath = Join-Path $OutputDirectory "feature_extraction_report.json"
    $reportJson = $report | ConvertTo-Json -Depth 10 -Compress:$false
    $reportJson | Out-File -FilePath $reportPath -Encoding UTF8
    
    Write-ColorOutput "✓ Generated feature extraction report" "Success"
    Write-ColorOutput "  Report saved to: $reportPath" "Detail"
    
    return $report
}

# Main execution
Write-ColorOutput "=" * 80
Write-ColorOutput "CHAT PANE & AGENTIC FEATURES EXTRACTION"
Write-ColorOutput "Starting at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-ColorOutput "Source Directory: $SourceDirectory"
Write-ColorOutput "=" * 80
Write-ColorOutput ""

# Check if source directory exists
if (-not (Test-Path $SourceDirectory)) {
    Write-ColorOutput "✗ Source directory not found: $SourceDirectory" "Error"
    exit 1
}

# Create output directory
if (Test-Path $OutputDirectory) {
    Remove-Item -Path $OutputDirectory -Recurse -Force
}
New-Item -Path $OutputDirectory -ItemType Directory -Force | Out-Null
Write-ColorOutput "✓ Created output directory: $OutputDirectory" "Success"

# If ExtractAll is specified, enable all extraction options
if ($ExtractAll) {
    $ExtractChatPane = $true
    $ExtractAgentFeatures = $true
    $ExtractGitHubCopilot = $true
    $ExtractAIServices = $true
    $ExtractExtensionSystem = $true
}

# Initialize file collections
$chatFiles = @()
$agentFiles = @()
$githubFiles = @()
$aiFiles = @()
$extensionFiles = @()

# Extract features based on parameters
if ($ExtractChatPane) {
    $chatFiles = Extract-ChatPane -SourceDir $SourceDirectory -OutputDir $OutputDirectory
}

if ($ExtractAgentFeatures) {
    $agentFiles = Extract-AgentFeatures -SourceDir $SourceDirectory -OutputDir $OutputDirectory
}

if ($ExtractGitHubCopilot) {
    $githubFiles = Extract-GitHubCopilot -SourceDir $SourceDirectory -OutputDir $OutputDirectory
}

if ($ExtractAIServices) {
    $aiFiles = Extract-AIServices -SourceDir $SourceDirectory -OutputDir $OutputDirectory
}

if ($ExtractExtensionSystem) {
    $extensionFiles = Extract-ExtensionSystem -SourceDir $SourceDirectory -OutputDir $OutputDirectory
}

# Generate report if requested
if ($GenerateReport) {
    $report = Generate-FeatureReport `
        -ChatFiles $chatFiles `
        -AgentFiles $agentFiles `
        -GitHubFiles $githubFiles `
        -AIFiles $aiFiles `
        -ExtensionFiles $extensionFiles
    
    # Display summary
    Write-ColorOutput ""
    Write-ColorOutput "=" * 80
    Write-ColorOutput "FEATURE EXTRACTION COMPLETE" "Header"
    Write-ColorOutput "=" * 80
    Write-ColorOutput "Chat Pane Files: $($chatFiles.Count)" "Chat"
    Write-ColorOutput "Agent Features Files: $($agentFiles.Count)" "Agent"
    Write-ColorOutput "GitHub Copilot Files: $($githubFiles.Count)" "GitHub"
    Write-ColorOutput "AI Services Files: $($aiFiles.Count)" "AI"
    Write-ColorOutput "Extension System Files: $($extensionFiles.Count)" "Extension"
    Write-ColorOutput "Total Files Extracted: $($report.Summary.TotalFiles)" "Success"
    Write-ColorOutput "=" * 80
    Write-ColorOutput "Output Directory: $OutputDirectory" "Detail"
    Write-ColorOutput "=" * 80
} else {
    Write-ColorOutput ""
    Write-ColorOutput "=" * 80
    Write-ColorOutput "EXTRACTION COMPLETE" "Header"
    Write-ColorOutput "=" * 80
    Write-ColorOutput "Total files extracted: $($chatFiles.Count + $agentFiles.Count + $githubFiles.Count + $aiFiles.Count + $extensionFiles.Count)" "Success"
    Write-ColorOutput "Output Directory: $OutputDirectory" "Detail"
    Write-ColorOutput "=" * 80
}

Write-ColorOutput ""
Write-ColorOutput "Next steps: Analyze the extracted features in $OutputDirectory" "Info"
