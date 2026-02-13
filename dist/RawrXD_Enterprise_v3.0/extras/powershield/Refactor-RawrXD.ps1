<#
.SYNOPSIS
    Enterprise PowerShell Source Code Refactoring Tool for RawrXD.ps1

.DESCRIPTION
    Analyzes a monolithic PowerShell script and refactors it into:
    - A single main entry point file
    - Modular plugin/extension structure organized by functionality
    - Proper dependency management
    - Enterprise-grade organization with manifest

.PARAMETER SourceFile
    Path to the source PowerShell script to refactor

.PARAMETER OutputDirectory
    Directory where refactored code will be written

.PARAMETER DryRun
    Analyze only, don't create files

.EXAMPLE
    .\Refactor-RawrXD.ps1 -SourceFile "RawrXD.ps1" -OutputDirectory "Refactored"

.EXAMPLE
    .\Refactor-RawrXD.ps1 -SourceFile "RawrXD.ps1" -DryRun
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$SourceFile = "RawrXD.ps1",

    [Parameter(Mandatory = $false)]
    [string]$OutputDirectory = "Modules",

    [Parameter(Mandatory = $false)]
    [string]$MainScriptName = "RawrXD-Main.ps1",

    [Parameter(Mandatory = $false)]
    [switch]$PreserveStructure = $true,

    [Parameter(Mandatory = $false)]
    [switch]$DryRun
)

#region Configuration
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# Module categories with priority ordering and specific patterns
# Categories are checked in order - more specific categories first
$script:ModuleCategories = @{
    # High priority - very specific patterns
    'Git' = @{
        'NamePatterns' = @('^.*-Git\w*$', '^.*-Repository\w*$', '^.*-Commit\w*$', '^.*-Branch\w*$', '^.*-Merge\w*$', '^.*-Pull\w*$', '^.*-Push\w*$')
        'Keywords' = @('Git', 'Repository', 'Commit', 'Branch', 'Merge', 'Pull', 'Push', 'Status', 'Stash', 'Diff', 'Blame')
        'ExcludePatterns' = @()
    }
    'Video' = @{
        'NamePatterns' = @('^.*-Video\w*$', '^.*-YouTube\w*$')
        'Keywords' = @('Video', 'YouTube', 'Download', 'Play', 'Search', 'Stream', 'Thumbnail')
        'ExcludePatterns' = @()
    }
    'Marketplace' = @{
        'NamePatterns' = @('^.*-Marketplace\w*$', '^.*-Extension\w*$', '^.*-VSCode\w*$')
        'Keywords' = @('Marketplace', 'Extension', 'VSCode', 'Install', 'Uninstall', 'Extension')
        'ExcludePatterns' = @()
    }
    'Terminal' = @{
        'NamePatterns' = @('^.*-CLI\w*$', '^.*-Console\w*$', '^.*-Terminal\w*$', '^Start-Console\w*$', '^Show-Console\w*$', '^Process-Console\w*$')
        'Keywords' = @('CLI', 'Console', 'Terminal', 'Command', 'Execute', 'Shell', 'Interactive')
        'ExcludePatterns' = @('Log', 'Error')
    }
    'Editor' = @{
        'NamePatterns' = @('^.*-Editor\w*$', '^.*-Syntax\w*$', '^.*-Highlight\w*$', '^.*-Text\w*$', '^.*-Cursor\w*$', '^.*-Selection\w*$')
        'Keywords' = @('Editor', 'Syntax', 'Highlight', 'RichText', 'Text', 'Cursor', 'Selection', 'TabBar', 'Tabs')
        'ExcludePatterns' = @('Log', 'Error', 'File')
    }
    'FileOperations' = @{
        'NamePatterns' = @('^Open-File\w*$', '^Save-\w*File\w*$', '^.*-File\w*$', '^.*-Directory\w*$', '^.*-Explorer\w*$', '^.*-Tree\w*$', '^Get-File\w*$', '^Update-File\w*$')
        'Keywords' = @('File', 'Directory', 'Explorer', 'Tree', 'Open', 'Save', 'Read', 'Write-File', 'Read-File')
        'ExcludePatterns' = @('Log', 'Error', 'Write-Error', 'Write-Log')
    }
    'Browser' = @{
        'NamePatterns' = @('^.*-Browser\w*$', '^.*-WebView\w*$', '^.*-Navigate\w*$', '^.*-Screenshot\w*$', '^.*-Click\w*$')
        'Keywords' = @('Browser', 'WebView', 'Navigate', 'Screenshot', 'Click', 'Web', 'HTML', 'DOM')
        'ExcludePatterns' = @()
    }
    'AI' = @{
        'NamePatterns' = @('^.*-Ollama\w*$', '^.*-AI\w*$', '^.*-Chat\w*$', '^.*-Model\w*$', '^.*-Prompt\w*$')
        'Keywords' = @('Ollama', 'AI', 'Chat', 'Model', 'Prompt', 'Message', 'Completion', 'Inference')
        'ExcludePatterns' = @()
    }
    'Agent' = @{
        'NamePatterns' = @('^.*-Agent\w*$', '^.*-Task\w*$', '^.*-Automation\w*$', '^.*-Orchestr\w*$')
        'Keywords' = @('Agent', 'Task', 'Automation', 'Schedule', 'Orchestr', 'Workflow')
        'ExcludePatterns' = @()
    }
    'Security' = @{
        'NamePatterns' = @('^.*-Security\w*$', '^.*-Encrypt\w*$', '^.*-Decrypt\w*$', '^.*-Protect\w*$', '^.*-Authenticate\w*$')
        'Keywords' = @('Security', 'Encrypt', 'Decrypt', 'Protect', 'Hash', 'Validate', 'Authenticate', 'Secure', 'Stealth')
        'ExcludePatterns' = @('Log')
    }
    'Settings' = @{
        'NamePatterns' = @('^.*-Setting\w*$', '^.*-Config\w*$', '^.*-Preference\w*$', '^.*-Theme\w*$', '^Load-Setting\w*$', '^Apply-\w*Setting\w*$')
        'Keywords' = @('Setting', 'Config', 'Preference', 'Theme', 'Color', 'Style', 'WindowSetting')
        'ExcludePatterns' = @()
    }
    'Performance' = @{
        'NamePatterns' = @('^.*-Performance\w*$', '^.*-Optimize\w*$', '^.*-Monitor\w*$', '^.*-Metrics\w*$', '^.*-Memory\w*$', '^.*-CPU\w*$')
        'Keywords' = @('Performance', 'Optimize', 'Monitor', 'Memory', 'CPU', 'Metrics', 'Insight', 'Threshold')
        'ExcludePatterns' = @()
    }
    'UI' = @{
        'NamePatterns' = @('^Show-\w*$', '^Display-\w*$', '^.*-Dialog\w*$', '^.*-Form\w*$', '^.*-Window\w*$', '^.*-Button\w*$', '^.*-Menu\w*$', '^.*-Panel\w*$', '^Initialize-WindowsForms\w*$')
        'Keywords' = @('Show', 'Display', 'Dialog', 'Form', 'Window', 'Button', 'Menu', 'Panel', 'Notification', 'Dashboard')
        'ExcludePatterns' = @('Log', 'Error', 'CLI', 'Console', 'Terminal')
    }
    'Logging' = @{
        'NamePatterns' = @('^Write-\w*Log\w*$', '^.*-Log\w*$', '^Get-Log\w*$', '^Initialize-Log\w*$', '^.*-ErrorLog\w*$', '^.*-EmergencyLog\w*$')
        'Keywords' = @('Log', 'ErrorLog', 'EmergencyLog', 'StartupLog', 'SecurityLog', 'StructuredErrorLog', 'LogFilePath', 'LogConfiguration')
        'ExcludePatterns' = @('File', 'CLI', 'Console', 'Terminal', 'Editor', 'Show', 'Display')
    }
    'Core' = @{
        'NamePatterns' = @('^Initialize-\w*$', '^Start-\w*$', '^Main\w*$', '^Entry\w*$')
        'Keywords' = @('Initialize', 'Start', 'Main', 'Entry', 'Setup')
        'ExcludePatterns' = @('Log', 'Console', 'CLI', 'Terminal')
    }
    'Utilities' = @{
        'NamePatterns' = @('^Get-\w*$', '^Set-\w*$', '^Convert-\w*$', '^Format-\w*$', '^Parse-\w*$', '^Normalize-\w*$')
        'Keywords' = @('Get', 'Set', 'Convert', 'Format', 'Parse', 'Normalize', 'Helper', 'Cache', 'Chunked')
        'ExcludePatterns' = @('Log', 'File', 'Setting', 'Config')
    }
}
#endregion

#region Helper Functions
function Write-RefactorLog {
    param(
        [string]$Message,
        [ValidateSet('Info', 'Success', 'Warning', 'Error', 'Debug')]
        [string]$Level = 'Info'
    )

    $color = switch ($Level) {
        'Success' { 'Green' }
        'Warning' { 'Yellow' }
        'Error' { 'Red' }
        'Debug' { 'Gray' }
        default { 'Cyan' }
    }

    $timestamp = Get-Date -Format 'HH:mm:ss'
    Write-Host "[$timestamp] [$Level] $Message" -ForegroundColor $color

    if ($script:LogFile) {
        Add-Content -Path $script:LogFile -Value "[$timestamp] [$Level] $Message" -ErrorAction SilentlyContinue
    }
}

function Get-AllFunctions {
    param([string]$Content)

    $functions = @()
    $lines = $Content -split "`r?`n"
    $inFunction = $false
    $functionStart = $null
    $functionName = $null
    $braceCount = 0

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]

        # Check for function definition
        if ($line -match '^\s*function\s+([\w-]+)') {
            # Save previous function if exists
            if ($inFunction) {
                $functions += @{
                    Name = $functionName
                    StartLine = $functionStart + 1
                    EndLine = $i
                    Content = ($lines[$functionStart..($i-1)] -join "`n")
                }
            }

            $functionName = $matches[1]
            $functionStart = $i
            $inFunction = $true
            $braceCount = ($line | Select-String -Pattern '\{' -AllMatches).Matches.Count
            $braceCount -= ($line | Select-String -Pattern '\}' -AllMatches).Matches.Count
        }
        elseif ($inFunction) {
            $openBraces = ($line | Select-String -Pattern '\{' -AllMatches).Matches.Count
            $closeBraces = ($line | Select-String -Pattern '\}' -AllMatches).Matches.Count
            $braceCount += $openBraces - $closeBraces

            if ($braceCount -eq 0) {
                $functions += @{
                    Name = $functionName
                    StartLine = $functionStart + 1
                    EndLine = $i + 1
                    Content = ($lines[$functionStart..$i] -join "`n")
                }
                $inFunction = $false
                $functionName = $null
                $functionStart = $null
            }
        }
    }

    # Handle last function if file ends without closing
    if ($inFunction) {
        $functions += @{
            Name = $functionName
            StartLine = $functionStart + 1
            EndLine = $lines.Count
            Content = ($lines[$functionStart..($lines.Count-1)] -join "`n")
        }
    }

    return $functions
}

function Get-FunctionDependencies {
    param(
        [string]$FunctionContent,
        [array]$AllFunctions
    )

    $dependencies = @{
        Functions = @()
        Variables = @()
        Modules = @()
    }

    # Find function calls
    $functionCalls = [regex]::Matches($FunctionContent, '(\w+-\w+)\s*\(') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -ne $null }

    foreach ($call in $functionCalls) {
        $found = $AllFunctions | Where-Object { $_.Name -eq $call }
        if ($found -and $dependencies.Functions -notcontains $call) {
            $dependencies.Functions += $call
        }
    }

    # Find script-scope variables
    $variableRefs = [regex]::Matches($FunctionContent, '\$script:(\w+)') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -ne $null }
    $dependencies.Variables += $variableRefs

    # Find module imports
    $moduleMatches = [regex]::Matches($FunctionContent, 'Import-Module\s+([\w-]+)')
    foreach ($match in $moduleMatches) {
        if ($dependencies.Modules -notcontains $match.Groups[1].Value) {
            $dependencies.Modules += $match.Groups[1].Value
        }
    }

    return $dependencies
}

function Categorize-Function {
    param(
        [string]$FunctionName,
        [string]$FunctionContent
    )

    # Check categories in priority order (most specific first)
    $categoryOrder = @('Git', 'Video', 'Marketplace', 'Terminal', 'Editor', 'FileOperations',
                       'Browser', 'AI', 'Agent', 'Security', 'Settings', 'Performance',
                       'UI', 'Logging', 'Core', 'Utilities')

    foreach ($category in $categoryOrder) {
        $categoryConfig = $script:ModuleCategories[$category]
        if (-not $categoryConfig) { continue }

        $matched = $false

        # First check name patterns (most specific)
        if ($categoryConfig.NamePatterns) {
            foreach ($pattern in $categoryConfig.NamePatterns) {
                if ($FunctionName -match $pattern) {
                    # Check exclusions
                    $excluded = $false
                    if ($categoryConfig.ExcludePatterns) {
                        foreach ($exclude in $categoryConfig.ExcludePatterns) {
                            if ($FunctionName -match $exclude) {
                                $excluded = $true
                                break
                            }
                        }
                    }
                    if (-not $excluded) {
                        return $category
                    }
                }
            }
        }

        # Then check keywords in function name (more specific than content)
        if ($categoryConfig.Keywords) {
            foreach ($keyword in $categoryConfig.Keywords) {
                # Check if keyword appears in function name (case-insensitive)
                if ($FunctionName -match [regex]::Escape($keyword)) {
                    # Check exclusions
                    $excluded = $false
                    if ($categoryConfig.ExcludePatterns) {
                        foreach ($exclude in $categoryConfig.ExcludePatterns) {
                            if ($FunctionName -match $exclude) {
                                $excluded = $true
                                break
                            }
                        }
                    }
                    if (-not $excluded) {
                        return $category
                    }
                }
            }
        }

        # Finally check keywords in content (least specific)
        if ($categoryConfig.Keywords) {
            foreach ($keyword in $categoryConfig.Keywords) {
                # Use word boundaries to avoid partial matches
                $pattern = '\b' + [regex]::Escape($keyword) + '\b'
                if ($FunctionContent -match $pattern) {
                    # Check exclusions
                    $excluded = $false
                    if ($categoryConfig.ExcludePatterns) {
                        foreach ($exclude in $categoryConfig.ExcludePatterns) {
                            if ($FunctionName -match $exclude -or $FunctionContent -match $exclude) {
                                $excluded = $true
                                break
                            }
                        }
                    }
                    if (-not $excluded) {
                        return $category
                    }
                }
            }
        }
    }

    # Default fallback
    return 'Utilities'
}

function Extract-MainContent {
    param(
        [string]$Content,
        [array]$Functions
    )

    $mainContent = @()
    $lines = $Content -split "`r?`n"
    $functionLineNumbers = @()

    foreach ($func in $Functions) {
        $functionLineNumbers += $func.StartLine..$func.EndLine
    }

    # Extract non-function code (script-level code)
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $lineNum = $i + 1
        if ($lineNum -notin $functionLineNumbers) {
            $mainContent += $lines[$i]
        }
    }

    return ($mainContent -join "`n")
}
#endregion

#region Main Refactoring Logic
function Start-Refactoring {
    # Resolve source file path - check current directory first, then script directory
    $resolvedSourceFile = $SourceFile
    if (-not [System.IO.Path]::IsPathRooted($SourceFile)) {
        # Try current directory first
        $currentPath = Join-Path (Get-Location).Path $SourceFile
        if (Test-Path $currentPath) {
            $resolvedSourceFile = Resolve-Path $currentPath
        }
        # Try script directory if not found
        elseif ($PSScriptRoot -and (Test-Path (Join-Path $PSScriptRoot $SourceFile))) {
            $resolvedSourceFile = Resolve-Path (Join-Path $PSScriptRoot $SourceFile)
        }
    }
    else {
        $resolvedSourceFile = Resolve-Path $SourceFile -ErrorAction SilentlyContinue
    }

    Write-RefactorLog "Starting enterprise refactoring process..." "Info"
    Write-RefactorLog "Source: $resolvedSourceFile" "Info"
    Write-RefactorLog "Output: $OutputDirectory" "Info"

    # Validate source file
    if (-not $resolvedSourceFile -or -not (Test-Path $resolvedSourceFile)) {
        Write-RefactorLog "Error: Source file not found: $($script:SourceFile ?? $SourceFile)" "Error"
        Write-RefactorLog "Current directory: $(Get-Location)" "Error"
        Write-RefactorLog "Please specify the correct path to RawrXD.ps1" "Error"
        return
    }

    # Use resolved path for rest of function
    $script:SourceFile = $resolvedSourceFile

    # Create output directory structure
    $script:LogFile = Join-Path $OutputDirectory "refactoring.log"
    if (-not $DryRun) {
        if (Test-Path $OutputDirectory) {
            Write-RefactorLog "Output directory exists. Cleaning..." "Warning"
            Remove-Item -Path $OutputDirectory -Recurse -Force -ErrorAction SilentlyContinue
        }
        New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $OutputDirectory "Modules") -Force | Out-Null
        Write-RefactorLog "Created output directory structure" "Success"
    }

    # Read source file
    Write-RefactorLog "Reading source file..." "Info"
    try {
        $sourceContent = Get-Content -Path $script:SourceFile -Raw -Encoding UTF8
        $sourceLines = (Get-Content -Path $script:SourceFile).Count
        Write-RefactorLog "Source file: $sourceLines lines" "Info"
    }
    catch {
        Write-RefactorLog "Error reading source file: $($_.Exception.Message)" "Error"
        return
    }

    # Extract all functions
    Write-RefactorLog "Extracting functions..." "Info"
    $allFunctions = Get-AllFunctions -Content $sourceContent
    Write-RefactorLog "Found $($allFunctions.Count) functions" "Success"

    # Analyze function dependencies
    Write-RefactorLog "Analyzing dependencies..." "Info"
    $functionDependencies = @{}
    foreach ($func in $allFunctions) {
        $deps = Get-FunctionDependencies -FunctionContent $func.Content -AllFunctions $allFunctions
        $functionDependencies[$func.Name] = $deps
    }

    # Categorize functions
    Write-RefactorLog "Categorizing functions..." "Info"
    $categorizedFunctions = @{}
    foreach ($category in $script:ModuleCategories.Keys) {
        $categorizedFunctions[$category] = @()
    }

    foreach ($func in $allFunctions) {
        $category = Categorize-Function -FunctionName $func.Name -FunctionContent $func.Content
        $categorizedFunctions[$category] += $func
    }

    # Generate module files
    Write-RefactorLog "Generating module files..." "Info"
    $moduleManifest = @{}

    foreach ($category in ($categorizedFunctions.Keys | Sort-Object)) {
        $functions = $categorizedFunctions[$category]
        if ($functions.Count -eq 0) { continue }

        $moduleName = "RawrXD.$category"
        $modulePath = Join-Path $OutputDirectory "Modules" "$moduleName.psm1"
        $moduleManifest[$category] = @{
            Name = $moduleName
            Path = $modulePath
            FunctionCount = $functions.Count
            Functions = $functions | ForEach-Object { $_.Name }
        }

        if (-not $DryRun) {
            # Create module content
            $moduleContent = @"
<#
.SYNOPSIS
    RawrXD $category Module

.DESCRIPTION
    Contains $category-related functions extracted from RawrXD.ps1

.NOTES
    Generated by Refactor-RawrXD.ps1
    Original functions: $($functions.Count)
    Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
#>

# Module variables
`$script:ModuleName = '$moduleName'
`$script:ModuleVersion = '1.0.0'

# Function definitions
"@

            # Add function content
            foreach ($func in $functions) {
                $moduleContent += "`n`n# ============================================"
                $moduleContent += "`n# Function: $($func.Name)"
                $moduleContent += "`n# ============================================"
                $moduleContent += "`n$($func.Content)"
                $moduleContent += "`n"
            }

            # Add export statement
            $exportList = $functions | ForEach-Object { "    '$($_.Name)'" }
            $moduleContent += @"

# Export functions
`$ExportedFunctions = @(
$($exportList -join ",`n")
)

Export-ModuleMember -Function `$ExportedFunctions
"@

            Set-Content -Path $modulePath -Value $moduleContent -Encoding UTF8
            Write-RefactorLog "Created module: $moduleName ($($functions.Count) functions)" "Success"
        }
    }

    # Extract main script content
    Write-RefactorLog "Extracting main script content..." "Info"
    $mainContent = Extract-MainContent -Content $sourceContent -Functions $allFunctions

    # Create main.ps1
    if (-not $DryRun) {
        $moduleList = $moduleManifest.Keys | ForEach-Object { "    'RawrXD.$_'" } | Sort-Object
        $moduleListString = $moduleList -join ",`n"

        $mainScriptContent = @"
<#
.SYNOPSIS
    RawrXD - Main Entry Point

.DESCRIPTION
    Main entry point for RawrXD application.
    This file orchestrates module loading and application initialization.

.NOTES
    Generated by Refactor-RawrXD.ps1
    Original source: $SourceFile
    Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = `$false)]
    [switch]`$CliMode,

    [Parameter(Mandatory = `$false)]
    [string]`$Command,

    [Parameter(Mandatory = `$false)]
    [string]`$FilePath,

    [Parameter(Mandatory = `$false)]
    [string]`$Model = "llama2",

    [Parameter(Mandatory = `$false)]
    [string]`$Prompt,

    [Parameter(Mandatory = `$false)]
    [string]`$AgentName,

    [Parameter(Mandatory = `$false)]
    [string]`$SettingName,

    [Parameter(Mandatory = `$false)]
    [string]`$SettingValue,

    [Parameter(Mandatory = `$false)]
    [string]`$URL,

    [Parameter(Mandatory = `$false)]
    [string]`$Selector,

    [Parameter(Mandatory = `$false)]
    [string]`$OutputPath
)

# ============================================
# MODULE LOADING
# ============================================
`$ModuleRoot = if (`$PSScriptRoot) { `$PSScriptRoot } else { Split-Path -Parent `$MyInvocation.MyCommand.Path }
`$ModulesPath = Join-Path `$ModuleRoot "Modules"

Write-Host "Loading RawrXD modules..." -ForegroundColor Cyan
`$Modules = @(
$moduleListString
)

foreach (`$module in `$Modules) {
    `$moduleFile = Join-Path `$ModulesPath "$module.psm1"
    if (Test-Path `$moduleFile) {
        try {
            Import-Module `$moduleFile -Force -ErrorAction Stop
            Write-Host "  ✓ Loaded: `$module" -ForegroundColor Green
        }
        catch {
            Write-Host "  ✗ Failed to load: `$module - `$(`$_.Exception.Message)" -ForegroundColor Red
        }
    }
    else {
        Write-Host "  ✗ Missing: `$module" -ForegroundColor Yellow
    }
}
Write-Host "Module loading complete.`n" -ForegroundColor Cyan

# ============================================
# MAIN SCRIPT LOGIC
# ============================================
$mainContent
"@

        $mainPath = Join-Path $OutputDirectory "main.ps1"
        Set-Content -Path $mainPath -Value $mainScriptContent -Encoding UTF8
        Write-RefactorLog "Created main.ps1" "Success"
    }

    # Create module manifest file
    if (-not $DryRun) {
        $manifestContent = @{
            OriginalFile = $SourceFile
            RefactoredDate = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
            TotalFunctions = $allFunctions.Count
            Modules = $moduleManifest
            Statistics = @{
                TotalLines = $sourceLines
                FunctionsExtracted = $allFunctions.Count
                ModulesCreated = ($moduleManifest.Keys | Measure-Object).Count
            }
        }

        $manifestPath = Join-Path $OutputDirectory "refactoring-manifest.json"
        $manifestContent | ConvertTo-Json -Depth 10 | Set-Content -Path $manifestPath -Encoding UTF8
        Write-RefactorLog "Created refactoring manifest" "Success"
    }

    # Generate summary report
    Write-RefactorLog "`n=== REFACTORING SUMMARY ===" "Info"
    Write-RefactorLog "Total Functions: $($allFunctions.Count)" "Info"
    Write-RefactorLog "Modules Created: $(($moduleManifest.Keys | Measure-Object).Count)" "Info"
    Write-RefactorLog "`nModule Breakdown:" "Info"

    foreach ($category in ($moduleManifest.Keys | Sort-Object)) {
        $funcCount = $moduleManifest[$category].FunctionCount
        Write-RefactorLog "  $category : $funcCount functions" "Info"
    }

    Write-RefactorLog "`nRefactoring complete!" "Success"
    Write-RefactorLog "Output directory: $OutputDirectory" "Info"

    if ($DryRun) {
        Write-RefactorLog "DRY RUN MODE - No files were created" "Warning"
    }
}
#endregion

#region Execution
try {
    Start-Refactoring
}
catch {
    Write-RefactorLog "Fatal error: $($_.Exception.Message)" "Error"
    Write-RefactorLog $_.ScriptStackTrace "Error"
    exit 1
}
#endregion
