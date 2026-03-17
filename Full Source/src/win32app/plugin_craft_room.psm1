#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Plugin Craft Room - On-the-Fly Custom Extension Creator
    Create fully custom plugins for anything and everything on demand

.DESCRIPTION
    Complete system for dynamically creating:
    - Custom plugins from templates
    - Extensions for any language
    - Specialized modules for unique tasks
    - Dynamic feature generators
    - One-off custom solutions

.EXAMPLE
    Import-Module .\plugin_craft_room.psm1
    New-CraftRoomPlugin -Name "MyTranslator" -Type "Translator" -Languages @("Spanish", "French")
    Start-PluginCraftRoom
#>

param()

# ============================================================================
# CONFIGURATION
# ============================================================================

$script:CraftRoomRoot = "D:\lazy init ide\craft_room"
$script:PluginTemplatesDir = Join-Path $script:CraftRoomRoot "templates"
$script:PluginOutputDir = Join-Path $script:CraftRoomRoot "plugins"
$script:PluginRegistry = @{}

# Ensure directories exist
@($script:CraftRoomRoot, $script:PluginTemplatesDir, $script:PluginOutputDir) | ForEach-Object {
    if (-not (Test-Path $_)) { New-Item -Path $_ -ItemType Directory -Force | Out-Null }
}

# ============================================================================
# PLUGIN TEMPLATES
# ============================================================================

$script:PluginTemplates = @{
    "Translator" = @{
        Description = "Language translation plugin"
        Extension = ".psm1"
        RequiredParams = @("Languages")
        Template = 'translator_template.ps1'
    }
    
    "Analyzer" = @{
        Description = "Data analysis and processing plugin"
        Extension = ".psm1"
        RequiredParams = @("AnalysisType")
        Template = 'analyzer_template.ps1'
    }
    
    "Connector" = @{
        Description = "API/Service connection plugin"
        Extension = ".psm1"
        RequiredParams = @("ServiceName", "Endpoint")
        Template = 'connector_template.ps1'
    }
    
    "Generator" = @{
        Description = "Code/Content generation plugin"
        Extension = ".psm1"
        RequiredParams = @("OutputType")
        Template = 'generator_template.ps1'
    }
    
    "Processor" = @{
        Description = "Data processing pipeline"
        Extension = ".psm1"
        RequiredParams = @("ProcessType")
        Template = 'processor_template.ps1'
    }
    
    "Custom" = @{
        Description = "Fully custom plugin"
        Extension = ".ps1"
        RequiredParams = @()
        Template = 'custom_template.ps1'
    }
}

# ============================================================================
# PLUGIN TEMPLATE GENERATORS
# ============================================================================

function Get-TranslatorTemplate {
    <#
    .SYNOPSIS
        Generate translator plugin template
    #>
    param(
        [string[]]$Languages,
        [string]$PluginName
    )
    
    $langList = ($Languages | ForEach-Object { "        '$_'" }) -join ",`n"
    
    return @"
#!/usr/bin/env pwsh
<#
.SYNOPSIS
    $PluginName - Auto-generated Translator Plugin
    
.DESCRIPTION
    Translates between the following languages:
$(($Languages | ForEach-Object { "    - $_" }) -join "`n")
#>

param()

# Configuration
\$script:SupportedLanguages = @(
$langList
)

\$script:TranslationCache = @{}

function Translate-Text {
    param(
        [Parameter(Mandatory=\$true)]
        [string]\$Text,
        
        [Parameter(Mandatory=\$true)]
        [string]\$SourceLanguage,
        
        [Parameter(Mandatory=\$true)]
        [string]\$TargetLanguage
    )
    
    if (-not (\$script:SupportedLanguages -contains \$SourceLanguage)) {
        throw "Source language '\$SourceLanguage' not supported"
    }
    
    if (-not (\$script:SupportedLanguages -contains \$TargetLanguage)) {
        throw "Target language '\$TargetLanguage' not supported"
    }
    
    # Translation logic here
    Write-Host "📝 Translating: \$SourceLanguage → \$TargetLanguage" -ForegroundColor Cyan
    Write-Host "   Text: \$Text" -ForegroundColor Gray
    
    return "Translated: \$Text"
}

function Get-SupportedLanguages {
    return \$script:SupportedLanguages
}

Export-ModuleMember -Function @(
    'Translate-Text',
    'Get-SupportedLanguages'
)
"@
}

function Get-AnalyzerTemplate {
    <#
    .SYNOPSIS
        Generate analyzer plugin template
    #>
    param(
        [string]$AnalysisType,
        [string]$PluginName
    )
    
    return @"
#!/usr/bin/env pwsh
<#
.SYNOPSIS
    $PluginName - Auto-generated Analyzer Plugin
    
.DESCRIPTION
    Performs $AnalysisType analysis on input data
#>

param()

# Configuration
\$script:AnalysisType = "$AnalysisType"
\$script:Results = @()

function Invoke-Analysis {
    param(
        [Parameter(Mandatory=\$true)]
        [object]\$InputData,
        
        [hashtable]\$Options = @{}
    )
    
    Write-Host "🔍 Running \$(\$script:AnalysisType) analysis..." -ForegroundColor Cyan
    
    \$result = @{
        Type = \$script:AnalysisType
        Input = \$InputData
        Timestamp = Get-Date
        Status = "Complete"
    }
    
    \$script:Results += \$result
    
    return \$result
}

function Get-AnalysisResults {
    return \$script:Results
}

Export-ModuleMember -Function @(
    'Invoke-Analysis',
    'Get-AnalysisResults'
)
"@
}

function Get-ConnectorTemplate {
    <#
    .SYNOPSIS
        Generate connector plugin template
    #>
    param(
        [string]$ServiceName,
        [string]$Endpoint,
        [string]$PluginName
    )
    
    return @"
#!/usr/bin/env pwsh
<#
.SYNOPSIS
    $PluginName - Auto-generated Connector Plugin
    
.DESCRIPTION
    Connects to $ServiceName service at $Endpoint
#>

param()

# Configuration
\$script:ServiceName = "$ServiceName"
\$script:Endpoint = "$Endpoint"
\$script:IsConnected = \$false
\$script:ConnectionTimeout = 30

function Connect-Service {
    param(
        [PSCredential]\$Credential = \$null,
        [int]\$Timeout = \$script:ConnectionTimeout
    )
    
    Write-Host "🔗 Connecting to \$(\$script:ServiceName)..." -ForegroundColor Cyan
    Write-Host "   Endpoint: \$(\$script:Endpoint)" -ForegroundColor Gray
    
    \$script:IsConnected = \$true
    
    return @{
        Service = \$script:ServiceName
        Status = "Connected"
        Timestamp = Get-Date
    }
}

function Disconnect-Service {
    Write-Host "🔌 Disconnecting from \$(\$script:ServiceName)..." -ForegroundColor Cyan
    \$script:IsConnected = \$false
}

function Invoke-ServiceCall {
    param(
        [Parameter(Mandatory=\$true)]
        [string]\$Method,
        
        [hashtable]\$Parameters = @{}
    )
    
    if (-not \$script:IsConnected) {
        throw "Not connected to service"
    }
    
    Write-Host "📤 Calling: \$Method" -ForegroundColor Cyan
    
    return @{
        Method = \$Method
        Status = "Success"
        Response = "Service response"
    }
}

Export-ModuleMember -Function @(
    'Connect-Service',
    'Disconnect-Service',
    'Invoke-ServiceCall'
)
"@
}

function Get-GeneratorTemplate {
    <#
    .SYNOPSIS
        Generate code generation plugin template
    #>
    param(
        [string]$OutputType,
        [string]$PluginName
    )
    
    return @"
#!/usr/bin/env pwsh
<#
.SYNOPSIS
    $PluginName - Auto-generated Generator Plugin
    
.DESCRIPTION
    Generates $OutputType output from templates and rules
#>

param()

# Configuration
\$script:OutputType = "$OutputType"
\$script:GenerationRules = @{}

function New-GeneratedContent {
    param(
        [Parameter(Mandatory=\$true)]
        [hashtable]\$Parameters,
        
        [string]\$Template = "default"
    )
    
    Write-Host "🎨 Generating \$(\$script:OutputType)..." -ForegroundColor Cyan
    
    \$generatedContent = @{
        Type = \$script:OutputType
        Template = \$Template
        Parameters = \$Parameters
        Content = "Generated content here"
        Timestamp = Get-Date
    }
    
    return \$generatedContent
}

function Add-GenerationRule {
    param(
        [Parameter(Mandatory=\$true)]
        [string]\$RuleName,
        
        [Parameter(Mandatory=\$true)]
        [scriptblock]\$Rule
    )
    
    \$script:GenerationRules[\$RuleName] = \$Rule
}

Export-ModuleMember -Function @(
    'New-GeneratedContent',
    'Add-GenerationRule'
)
"@
}

function Get-ProcessorTemplate {
    <#
    .SYNOPSIS
        Generate data processor plugin template
    #>
    param(
        [string]$ProcessType,
        [string]$PluginName
    )
    
    return @"
#!/usr/bin/env pwsh
<#
.SYNOPSIS
    $PluginName - Auto-generated Processor Plugin
    
.DESCRIPTION
    Processes data using $ProcessType pipeline
#>

param()

# Configuration
\$script:ProcessType = "$ProcessType"
\$script:Pipeline = @()

function Add-PipelineStage {
    param(
        [Parameter(Mandatory=\$true)]
        [string]\$StageName,
        
        [Parameter(Mandatory=\$true)]
        [scriptblock]\$StageLogic
    )
    
    \$script:Pipeline += @{
        Name = \$StageName
        Logic = \$StageLogic
    }
}

function Invoke-Pipeline {
    param(
        [Parameter(Mandatory=\$true)]
        [object]\$InputData,
        
        [hashtable]\$Options = @{}
    )
    
    Write-Host "⚙️  Processing data through \$(\$script:ProcessType) pipeline..." -ForegroundColor Cyan
    
    \$output = \$InputData
    
    foreach (\$stage in \$script:Pipeline) {
        Write-Host "   → \$(\$stage.Name)" -ForegroundColor Gray
        \$output = & \$stage.Logic -InputData \$output -Options \$Options
    }
    
    return \$output
}

Export-ModuleMember -Function @(
    'Add-PipelineStage',
    'Invoke-Pipeline'
)
"@
}

function Get-CustomTemplate {
    <#
    .SYNOPSIS
        Generate custom plugin template
    #>
    param([string]$PluginName)
    
    return @"
#!/usr/bin/env pwsh
<#
.SYNOPSIS
    $PluginName - Custom Plugin
    
.DESCRIPTION
    Fully customizable plugin for specialized tasks
#>

param()

function Initialize-Plugin {
    Write-Host "🔧 Initializing $PluginName..." -ForegroundColor Cyan
}

function Execute-CustomLogic {
    param([hashtable]\$Parameters)
    
    Write-Host "⚡ Executing custom logic..." -ForegroundColor Cyan
    return \$Parameters
}

Export-ModuleMember -Function @(
    'Initialize-Plugin',
    'Execute-CustomLogic'
)
"@
}

# ============================================================================
# PLUGIN CREATION
# ============================================================================

function New-CraftRoomPlugin {
    <#
    .SYNOPSIS
        Create a new plugin from template
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name,
        
        [Parameter(Mandatory=$true)]
        [ValidateSet("Translator", "Analyzer", "Connector", "Generator", "Processor", "Custom")]
        [string]$Type,
        
        [hashtable]$Parameters = @{}
    )
    
    if (-not $script:PluginTemplates.ContainsKey($Type)) {
        throw "Plugin type '$Type' not found"
    }
    
    $template = $script:PluginTemplates[$Type]
    $fileName = "$Name$($template.Extension)"
    $outputPath = Join-Path $script:PluginOutputDir $fileName
    
    Write-Host ""
    Write-Host "🎨 CREATING CRAFT ROOM PLUGIN" -ForegroundColor Green
    Write-Host "═" * 70
    Write-Host "  Name: $Name" -ForegroundColor Cyan
    Write-Host "  Type: $Type" -ForegroundColor Cyan
    Write-Host "  Output: $outputPath" -ForegroundColor Cyan
    Write-Host ""
    
    # Get template content
    $content = switch ($Type) {
        "Translator" {
            if (-not $Parameters.ContainsKey("Languages")) {
                throw "Translator plugin requires 'Languages' parameter"
            }
            Get-TranslatorTemplate -Languages $Parameters.Languages -PluginName $Name
        }
        "Analyzer" {
            if (-not $Parameters.ContainsKey("AnalysisType")) {
                $Parameters.AnalysisType = "General"
            }
            Get-AnalyzerTemplate -AnalysisType $Parameters.AnalysisType -PluginName $Name
        }
        "Connector" {
            if (-not $Parameters.ContainsKey("ServiceName") -or -not $Parameters.ContainsKey("Endpoint")) {
                throw "Connector requires 'ServiceName' and 'Endpoint' parameters"
            }
            Get-ConnectorTemplate -ServiceName $Parameters.ServiceName -Endpoint $Parameters.Endpoint -PluginName $Name
        }
        "Generator" {
            if (-not $Parameters.ContainsKey("OutputType")) {
                $Parameters.OutputType = "Custom"
            }
            Get-GeneratorTemplate -OutputType $Parameters.OutputType -PluginName $Name
        }
        "Processor" {
            if (-not $Parameters.ContainsKey("ProcessType")) {
                $Parameters.ProcessType = "Custom"
            }
            Get-ProcessorTemplate -ProcessType $Parameters.ProcessType -PluginName $Name
        }
        "Custom" {
            Get-CustomTemplate -PluginName $Name
        }
    }
    
    $content | Out-File -FilePath $outputPath -Encoding UTF8
    
    # Register plugin
    $script:PluginRegistry[$Name] = @{
        Name = $Name
        Type = $Type
        Path = $outputPath
        CreatedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        Parameters = $Parameters
    }
    
    Write-Host "  ✅ Plugin created successfully" -ForegroundColor Green
    Write-Host ""
    
    return $outputPath
}

# ============================================================================
# PLUGIN MANAGEMENT
# ============================================================================

function Show-PluginTemplates {
    <#
    .SYNOPSIS
        Display all available plugin templates
    #>
    
    Write-Host ""
    Write-Host "📋 AVAILABLE PLUGIN TEMPLATES" -ForegroundColor Green
    Write-Host "═" * 70
    Write-Host ""
    
    foreach ($type in $script:PluginTemplates.Keys | Sort-Object) {
        $template = $script:PluginTemplates[$type]
        Write-Host "  [$type]" -ForegroundColor Cyan
        Write-Host "    Description: $($template.Description)" -ForegroundColor Gray
        Write-Host "    Extension: $($template.Extension)" -ForegroundColor Gray
        if ($template.RequiredParams.Count -gt 0) {
            Write-Host "    Required: $($template.RequiredParams -join ', ')" -ForegroundColor Gray
        }
        Write-Host ""
    }
}

function Show-CreatedPlugins {
    <#
    .SYNOPSIS
        Display all created plugins
    #>
    
    Write-Host ""
    Write-Host "📦 CREATED PLUGINS" -ForegroundColor Green
    Write-Host "═" * 70
    Write-Host ""
    
    if ($script:PluginRegistry.Count -eq 0) {
        Write-Host "  No plugins created yet" -ForegroundColor Yellow
        return
    }
    
    foreach ($plugin in $script:PluginRegistry.Values | Sort-Object CreatedAt -Descending) {
        Write-Host "  ✓ $($plugin.Name)" -ForegroundColor Green
        Write-Host "    Type: $($plugin.Type)" -ForegroundColor Gray
        Write-Host "    Path: $($plugin.Path)" -ForegroundColor Gray
        Write-Host "    Created: $($plugin.CreatedAt)" -ForegroundColor Gray
        Write-Host ""
    }
}

function Import-CraftPlugin {
    <#
    .SYNOPSIS
        Import and load a created plugin
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$PluginName
    )
    
    if (-not $script:PluginRegistry.ContainsKey($PluginName)) {
        throw "Plugin '$PluginName' not found in registry"
    }
    
    $plugin = $script:PluginRegistry[$PluginName]
    
    Write-Host ""
    Write-Host "🚀 LOADING PLUGIN: $PluginName" -ForegroundColor Green
    Write-Host "  Type: $($plugin.Type)" -ForegroundColor Cyan
    Write-Host "  Path: $($plugin.Path)" -ForegroundColor Gray

    if (-not (Test-Path $plugin.Path)) {
        throw "Plugin file not found: $($plugin.Path)"
    }

    $resolvedPath = (Resolve-Path $plugin.Path).Path
    $extension = [System.IO.Path]::GetExtension($resolvedPath).ToLowerInvariant()

    if ($extension -notin @('.psm1', '.ps1')) {
        throw "Unsupported plugin type: $extension. Only .psm1 and .ps1 are allowed."
    }
    
    if ($extension -eq '.psm1') {
        Import-Module $resolvedPath -Force -DisableNameChecking -ErrorAction Stop
        Write-Host "  ✅ Module loaded" -ForegroundColor Green
    } else {
        . $resolvedPath
        Write-Host "  ✅ Script loaded" -ForegroundColor Green
    }
    
    Write-Host ""
}

function Remove-CraftPlugin {
    <#
    .SYNOPSIS
        Remove a created plugin
    #>
    param(
        [Parameter(Mandatory=$true)]
        [string]$PluginName,
        
        [switch]$Force = $false
    )
    
    if (-not $script:PluginRegistry.ContainsKey($PluginName)) {
        throw "Plugin '$PluginName' not found"
    }
    
    $plugin = $script:PluginRegistry[$PluginName]
    
    if (-not $Force) {
        Write-Host ""
        Write-Host "⚠️  About to delete: $PluginName" -ForegroundColor Yellow
        Write-Host "  Path: $($plugin.Path)" -ForegroundColor Gray
        Write-Host ""
        $confirm = Read-Host "Type YES to confirm deletion"
        if ($confirm -ne "YES") {
            Write-Host "❌ Deletion cancelled" -ForegroundColor Red
            return
        }
    }
    
    if (Test-Path $plugin.Path) {
        Remove-Item $plugin.Path -Force
        Write-Host "✅ Plugin file deleted" -ForegroundColor Green
    }
    
    $script:PluginRegistry.Remove($PluginName)
    Write-Host "✅ Plugin removed from registry" -ForegroundColor Green
}

# ============================================================================
# INTERACTIVE CRAFT ROOM
# ============================================================================

function Start-PluginCraftRoom {
    <#
    .SYNOPSIS
        Launch interactive plugin craft room
    #>
    
    while ($true) {
        Write-Host ""
        Write-Host "╔════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║  🎨 PLUGIN CRAFT ROOM - Create Custom Plugins On-the-Fly ║" -ForegroundColor Cyan
        Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
        Write-Host ""
        
        Write-Host "Craft Room Options:" -ForegroundColor Yellow
        Write-Host "  [1] Create New Plugin" -ForegroundColor White
        Write-Host "  [2] View Available Templates" -ForegroundColor White
        Write-Host "  [3] List Created Plugins" -ForegroundColor White
        Write-Host "  [4] Load Plugin" -ForegroundColor White
        Write-Host "  [5] Delete Plugin" -ForegroundColor White
        Write-Host "  [6] Create Translator" -ForegroundColor White
        Write-Host "  [7] Create Analyzer" -ForegroundColor White
        Write-Host "  [8] Create Connector" -ForegroundColor White
        Write-Host "  [9] Create Generator" -ForegroundColor White
        Write-Host "  [10] Create Processor" -ForegroundColor White
        Write-Host "  [0] Exit Craft Room" -ForegroundColor Gray
        Write-Host ""
        
        Write-Host "Choice: " -NoNewline -ForegroundColor Cyan
        $choice = Read-Host
        
        switch ($choice) {
            '1' {
                $name = Read-Host "Plugin name"
                Write-Host ""
                Write-Host "Types: Translator, Analyzer, Connector, Generator, Processor, Custom" -ForegroundColor Yellow
                $type = Read-Host "Plugin type"
                New-CraftRoomPlugin -Name $name -Type $type
            }
            '2' { Show-PluginTemplates }
            '3' { Show-CreatedPlugins }
            '4' {
                Show-CreatedPlugins
                $name = Read-Host "Plugin name to load"
                Import-CraftPlugin -PluginName $name
            }
            '5' {
                Show-CreatedPlugins
                $name = Read-Host "Plugin name to delete"
                Remove-CraftPlugin -PluginName $name
            }
            '6' {
                $name = Read-Host "Plugin name"
                $langs = @(Read-Host "Languages (comma-separated)")
                New-CraftRoomPlugin -Name $name -Type "Translator" -Parameters @{ Languages = $langs }
            }
            '7' {
                $name = Read-Host "Plugin name"
                $type = Read-Host "Analysis type"
                New-CraftRoomPlugin -Name $name -Type "Analyzer" -Parameters @{ AnalysisType = $type }
            }
            '8' {
                $name = Read-Host "Plugin name"
                $service = Read-Host "Service name"
                $endpoint = Read-Host "Endpoint URL"
                New-CraftRoomPlugin -Name $name -Type "Connector" -Parameters @{ ServiceName = $service; Endpoint = $endpoint }
            }
            '9' {
                $name = Read-Host "Plugin name"
                $type = Read-Host "Output type"
                New-CraftRoomPlugin -Name $name -Type "Generator" -Parameters @{ OutputType = $type }
            }
            '10' {
                $name = Read-Host "Plugin name"
                $type = Read-Host "Process type"
                New-CraftRoomPlugin -Name $name -Type "Processor" -Parameters @{ ProcessType = $type }
            }
            '0' { break }
        }
        
        if ($choice -ne '0') {
            Read-Host "Press Enter to continue"
        }
    }
}

# ============================================================================
# EXPORTS
# ============================================================================

Export-ModuleMember -Function @(
    'New-CraftRoomPlugin',
    'Show-PluginTemplates',
    'Show-CreatedPlugins',
    'Import-CraftPlugin',
    'Remove-CraftPlugin',
    'Start-PluginCraftRoom'
)
