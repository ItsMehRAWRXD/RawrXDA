# =============================================================================
# MANIFEST TRACER PIPELINE - End-to-End Manifest Discovery and Tracking
# =============================================================================
# This module provides self-locating manifest discovery, parsing, tracing,
# and validation to eliminate all gaps in the orchestration pipeline.

using namespace System.Collections.Generic
using namespace System.IO

# Global manifest registry
$script:ManifestRegistry = @{
    DiscoveredManifests = [List[hashtable]]::new()
    ParsedManifests = [List[hashtable]]::new()
    DependencyGraph = $null
    ValidationResults = [List[hashtable]]::new()
    TraceLog = [List[hashtable]]::new()
}

# Manifest format definitions
$script:ManifestFormats = @{
    JSON = @{
        Extensions = @('.json')
        Parser = 'Parse-JsonManifest'
        Validator = 'Validate-JsonManifest'
    }
    XML = @{
        Extensions = @('.xml', '.config')
        Parser = 'Parse-XmlManifest'
        Validator = 'Validate-XmlManifest'
    }
    YAML = @{
        Extensions = @('.yaml', '.yml')
        Parser = 'Parse-YamlManifest'
        Validator = 'Validate-YamlManifest'
    }
    INI = @{
        Extensions = @('.ini', '.cfg')
        Parser = 'Parse-IniManifest'
        Validator = 'Validate-IniManifest'
    }
    TOML = @{
        Extensions = @('.toml')
        Parser = 'Parse-TomlManifest'
        Validator = 'Validate-TomlManifest'
    }
    PowerShell = @{
        Extensions = @('.ps1', '.psm1')
        Parser = 'Parse-PowerShellManifest'
        Validator = 'Validate-PowerShellManifest'
    }
}

# =============================================================================
# MANIFEST DISCOVERY ENGINE - Self-Locating Manifest Finder
# =============================================================================

function Find-Manifests {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SearchPath,
        
        [Parameter(Mandatory = $false)]
        [string[]]$IncludePatterns = @('*manifest*', '*config*', '*settings*', '*orchestrator*'),
        
        [Parameter(Mandatory = $false)]
        [string[]]$ExcludePatterns = @('node_modules', '.git', 'bin', 'obj', 'packages'),
        
        [Parameter(Mandatory = $false)]
        [switch]$Recurse,
        
        [Parameter(Mandatory = $false)]
        [int]$MaxDepth = 10,
        
        [Parameter(Mandatory = $false)]
        [switch]$IncludeHidden
    )
    
    Write-Log "INFO" "Starting manifest discovery in $SearchPath"
    
    $discoveredManifests = [List[hashtable]]::new()
    $searchOptions = @{
        Path = $SearchPath
        Recurse = $Recurse
        File = $true
    }
    
    if ($MaxDepth -gt 0) {
        $searchOptions['Depth'] = $MaxDepth
    }
    
    # Build search patterns from manifest formats
    $allExtensions = $script:ManifestFormats.Values.Extensions | ForEach-Object { $_ } | Select-Object -Unique
    $patternList = [List[string]]::new()
    
    foreach ($pattern in $IncludePatterns) {
        foreach ($ext in $allExtensions) {
            $patternList.Add("$pattern$ext")
        }
    }
    
    if ($patternList.Count -gt 0) {
        $searchOptions['Include'] = $patternList.ToArray()
    }
    
    try {
        $files = Get-ChildItem @searchOptions -ErrorAction SilentlyContinue | Where-Object {
            $file = $_
            $shouldInclude = $true
            
            # Check exclude patterns
            foreach ($exclude in $ExcludePatterns) {
                if ($file.FullName -like "*$exclude*") {
                    $shouldInclude = $false
                    break
                }
            }
            
            # Check hidden files
            if (!$IncludeHidden -and $file.Attributes -band [System.IO.FileAttributes]::Hidden) {
                $shouldInclude = $false
            }
            
            $shouldInclude
        }
        
        foreach ($file in $files) {
            $manifestInfo = @{
                FullPath = $file.FullName
                Name = $file.Name
                Directory = $file.DirectoryName
                Extension = $file.Extension
                Size = $file.Length
                LastModified = $file.LastWriteTime
                Format = Identify-ManifestFormat -FilePath $file.FullName
                Confidence = Calculate-Confidence -File $file
                DiscoverySource = "FileSystem"
                DiscoveryTimestamp = Get-Date
            }
            
            $discoveredManifests.Add($manifestInfo)
            
            # Log discovery
            Write-Log "DEBUG" "Discovered manifest: $($file.FullName) (Format: $($manifestInfo.Format))"
        }
        
        Write-Log "INFO" "Manifest discovery completed. Found $($discoveredManifests.Count) manifests"
        
        # Update registry
        $script:ManifestRegistry.DiscoveredManifests = $discoveredManifests
        
        return $discoveredManifests
    }
    catch {
        Write-Log "ERROR" "Manifest discovery failed: $_"
        return $null
    }
}

function Identify-ManifestFormat {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath
    )
    
    $extension = [System.IO.Path]::GetExtension($FilePath).ToLower()
    $fileName = [System.IO.Path]::GetFileName($FilePath).ToLower()
    
    # Check by extension
    foreach ($format in $script:ManifestFormats.Keys) {
        if ($script:ManifestFormats[$format].Extensions -contains $extension) {
            return $format
        }
    }
    
    # Check by filename patterns
    if ($fileName -match 'orchestrator|digestion|engine') {
        return 'PowerShell'
    }
    
    if ($fileName -match 'manifest|config|settings') {
        return 'JSON'  # Default assumption
    }
    
    return 'Unknown'
}

function Calculate-Confidence {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.FileInfo]$File
    )
    
    $confidence = 50  # Base confidence
    
    # Check filename patterns
    $fileName = $File.Name.ToLower()
    if ($fileName -match 'manifest|orchestrator|config|settings') {
        $confidence += 30
    }
    
    # Check file size (reasonable manifest size)
    if ($File.Length -gt 100 -and $File.Length -lt 100000) {
        $confidence += 10
    }
    
    # Check directory context
    $directory = $File.DirectoryName.ToLower()
    if ($directory -match 'config|settings|orchestrator|engine') {
        $confidence += 10
    }
    
    return [Math]::Min($confidence, 100)
}

# =============================================================================
# UNIVERSAL MANIFEST PARSER - Multi-Format Parser
# =============================================================================

function Parse-Manifest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        
        [Parameter(Mandatory = $false)]
        [string]$Format = "Auto",
        
        [Parameter(Mandatory = $false)]
        [switch]$Validate
    )
    
    Write-Log "INFO" "Parsing manifest: $FilePath"
    
    if (!(Test-Path $FilePath)) {
        Write-Log "ERROR" "Manifest file not found: $FilePath"
        return $null
    }
    
    # Auto-detect format if not specified
    if ($Format -eq "Auto") {
        $Format = Identify-ManifestFormat -FilePath $FilePath
    }
    
    $parserFunction = $script:ManifestFormats[$Format].Parser
    
    if (!$parserFunction) {
        Write-Log "ERROR" "No parser available for format: $Format"
        return $null
    }
    
    try {
        $parsedManifest = & $parserFunction -FilePath $FilePath
        
        if ($Validate) {
            $validationResult = Validate-Manifest -Manifest $parsedManifest -Format $Format
            $parsedManifest.Validation = $validationResult
        }
        
        $parsedManifest.FileInfo = @{
            Path = $FilePath
            Format = $Format
            ParsedTimestamp = Get-Date
        }
        
        # Add to registry
        $script:ManifestRegistry.ParsedManifests.Add($parsedManifest)
        
        Write-Log "INFO" "Manifest parsed successfully (Format: $Format)"
        
        return $parsedManifest
    }
    catch {
        Write-Log "ERROR" "Failed to parse manifest: $_"
        return $null
    }
}

function Parse-JsonManifest {
    param([string]$FilePath)
    
    $content = Get-Content $FilePath -Raw -ErrorAction Stop
    $json = $content | ConvertFrom-Json -ErrorAction Stop
    
    return @{
        Format = "JSON"
        Data = $json
        RawContent = $content
    }
}

function Parse-XmlManifest {
    param([string]$FilePath)
    
    $xml = [xml](Get-Content $FilePath -Raw -ErrorAction Stop)
    
    return @{
        Format = "XML"
        Data = $xml
        RawContent = $xml.OuterXml
    }
}

function Parse-YamlManifest {
    param([string]$FilePath)
    
    # Use PowerShell-YAML module if available, otherwise fallback to regex parsing
    try {
        if (Get-Module -ListAvailable -Name "powershell-yaml") {
            Import-Module powershell-yaml -ErrorAction Stop
            $content = Get-Content $FilePath -Raw -ErrorAction Stop
            $yaml = ConvertFrom-Yaml $content -ErrorAction Stop
            
            return @{
                Format = "YAML"
                Data = $yaml
                RawContent = $content
            }
        }
    }
    catch {
        Write-Log "WARNING" "PowerShell-YAML module not available, using fallback parser"
    }
    
    # Fallback to simple key-value parsing
    return Parse-KeyValueManifest -FilePath $FilePath -Format "YAML"
}

function Parse-IniManifest {
    param([string]$FilePath)
    
    $content = Get-Content $FilePath -ErrorAction Stop
    $ini = @{} 
    $section = ""
    
    foreach ($line in $content) {
        $line = $line.Trim()
        
        if ($line -match '^\[(.+)\]$') {
            $section = $matches[1]
            $ini[$section] = @{} 
        }
        elseif ($line -match '^(.+?)\s*=\s*(.+)$') {
            $key = $matches[1].Trim()
            $value = $matches[2].Trim()
            
            if ($section) {
                $ini[$section][$key] = $value
            }
            else {
                $ini[$key] = $value
            }
        }
    }
    
    return @{
        Format = "INI"
        Data = $ini
        RawContent = ($content | Out-String)
    }
}

function Parse-TomlManifest {
    param([string]$FilePath)
    
    # TOML is similar to INI but with more complex data types
    # For now, use the INI parser as a fallback
    return Parse-IniManifest -FilePath $FilePath
}

function Parse-PowerShellManifest {
    param([string]$FilePath)
    
    # For PowerShell files, extract configuration variables and parameters
    $content = Get-Content $FilePath -Raw -ErrorAction Stop
    $ast = [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$null, [ref]$null)
    
    $config = @{}
    
    # Extract variables that look like configuration
    $variableAsts = $ast.FindAll({ $args[0] -is [System.Management.Automation.Language.VariableExpressionAst] }, $true)
    foreach ($varAst in $variableAsts) {
        $varName = $varAst.VariablePath.UserPath
        if ($varName -match 'config|setting|option|parameter') {
            $config[$varName] = $varAst.Extent.Text
        }
    }
    
    # Extract parameters
    $paramAsts = $ast.FindAll({ $args[0] -is [System.Management.Automation.Language.ParameterAst] }, $true)
    $parameters = @{}
    foreach ($paramAst in $paramAsts) {
        $paramName = $paramAst.Name.VariablePath.UserPath
        $parameters[$paramName] = @{
            Type = $paramAst.StaticType.Name
            DefaultValue = if ($paramAst.DefaultValue) { $paramAst.DefaultValue.Value } else { $null }
            Attributes = $paramAst.Attributes | ForEach-Object { $_.TypeName.Name }
        }
    }
    
    return @{
        Format = "PowerShell"
        Data = @{
            Configuration = $config
            Parameters = $parameters
            AST = $ast
        }
        RawContent = $content
    }
}

function Parse-KeyValueManifest {
    param(
        [string]$FilePath,
        [string]$Format = "KeyValue"
    )
    
    $content = Get-Content $FilePath -ErrorAction Stop
    $data = @{}
    
    foreach ($line in $content) {
        $line = $line.Trim()
        
        if ($line -and !$line.StartsWith('#') -and !$line.StartsWith(';')) {
            if ($line -match '^(.+?)\s*[:=]\s*(.+)$') {
                $key = $matches[1].Trim()
                $value = $matches[2].Trim()
                $data[$key] = $value
            }
        }
    }
    
    return @{
        Format = $Format
        Data = $data
        RawContent = ($content | Out-String)
    }
}

# =============================================================================
# MANIFEST VALIDATION ENGINE - Schema and Integrity Validation
# =============================================================================

function Validate-Manifest {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Manifest,
        
        [Parameter(Mandatory = $false)]
        [string]$Format = "Auto"
    )
    
    if ($Format -eq "Auto") {
        $Format = $Manifest.Format
    }
    
    $validatorFunction = $script:ManifestFormats[$Format].Validator
    
    if (!$validatorFunction) {
        Write-Log "WARNING" "No validator available for format: $Format, using generic validation"
        return Validate-GenericManifest -Manifest $Manifest
    }
    
    try {
        return & $validatorFunction -Manifest $Manifest
    }
    catch {
        Write-Log "ERROR" "Manifest validation failed: $_"
        return @{
            IsValid = $false
            Errors = @($_)
            Warnings = @()
            SchemaVersion = "Unknown"
        }
    }
}

function Validate-GenericManifest {
    param([hashtable]$Manifest)
    
    $errors = [List[string]]::new()
    $warnings = [List[string]]::new()
    
    # Check for required fields
    if (!$Manifest.Data) {
        $errors.Add("Manifest has no data")
    }
    
    # Check for circular references (basic)
    if (Test-CircularReferences -Manifest $Manifest) {
        $errors.Add("Circular references detected in manifest")
    }
    
    # Check for security issues
    if (Test-SecurityIssues -Manifest $Manifest) {
        $warnings.Add("Potential security issues detected in manifest")
    }
    
    return @{
        IsValid = $errors.Count -eq 0
        Errors = $errors
        Warnings = $warnings
        SchemaVersion = "Generic"
        ValidationTimestamp = Get-Date
    }
}

function Validate-JsonManifest {
    param([hashtable]$Manifest)
    
    $errors = [List[string]]::new()
    $warnings = [List[string]]::new()
    
    try {
        $json = $Manifest.Data
        
        # Check for required JSON structure
        if ($json -is [System.Management.Automation.PSCustomObject]) {
            $properties = $json.PSObject.Properties.Name
            
            # Look for common manifest patterns
            $hasVersion = $properties -contains 'version' -or $properties -contains 'Version'
            $hasName = $properties -contains 'name' -or $properties -contains 'Name'
            $hasConfig = $properties -contains 'config' -or $properties -contains 'configuration'
            
            if (!$hasVersion) {
                $warnings.Add("Manifest missing version field")
            }
            
            if (!$hasName -and !$hasConfig) {
                $warnings.Add("Manifest missing name or configuration section")
            }
        }
        
        # Validate JSON schema if possible
        if ($Manifest.RawContent) {
            try {
                $parsedJson = $Manifest.RawContent | ConvertFrom-Json -ErrorAction Stop
            }
            catch {
                $errors.Add("Invalid JSON syntax: $_")
            }
        }
    }
    catch {
        $errors.Add("JSON validation failed: $_")
    }
    
    return @{
        IsValid = $errors.Count -eq 0
        Errors = $errors
        Warnings = $warnings
        SchemaVersion = "JSON"
        ValidationTimestamp = Get-Date
    }
}

function Validate-XmlManifest {
    param([hashtable]$Manifest)
    
    $errors = [List[string]]::new()
    $warnings = [List[string]]::new()
    
    try {
        $xml = $Manifest.Data
        
        # Check XML structure
        if ($xml.DocumentElement) {
            $rootName = $xml.DocumentElement.Name
            
            # Look for common XML manifest patterns
            if ($rootName -notin @('configuration', 'manifest', 'settings', 'orchestrator')) {
                $warnings.Add("Unusual root element: $rootName")
            }
        }
        else {
            $errors.Add("XML has no root element")
        }
        
        # Check for XML schema validation
        if ($xml.Schemas.Count -eq 0) {
            $warnings.Add("No XML schema defined")
        }
    }
    catch {
        $errors.Add("XML validation failed: $_")
    }
    
    return @{
        IsValid = $errors.Count -eq 0
        Errors = $errors
        Warnings = $warnings
        SchemaVersion = "XML"
        ValidationTimestamp = Get-Date
    }
}

function Test-CircularReferences {
    param([hashtable]$Manifest)
    
    # This is a simplified circular reference detector
    # In a full implementation, this would traverse the manifest structure
    
    $data = $Manifest.Data
    $visited = [HashSet[string]]::new()
    
    function Test-Node {
        param($node, $path = "")
        
        if ($node -is [string] -and $path) {
            if ($visited.Contains($path)) {
                return $true  # Circular reference found
            }
            $visited.Add($path) | Out-Null
        }
        
        return $false
    }
    
    # Recursively check the data structure
    # This is a placeholder for a more sophisticated algorithm
    
    return $false
}

function Test-SecurityIssues {
    param([hashtable]$Manifest)
    
    $content = $Manifest.RawContent
    
    # Check for potentially dangerous content
    $dangerousPatterns = @(
        'Invoke-Expression',
        'iex\s+',
        'Start-Process.*-ArgumentList',
        'System\.Diagnostics\.Process',
        'Add-Type.*-MemberDefinition'
    )
    
    foreach ($pattern in $dangerousPatterns) {
        if ($content -match $pattern) {
            return $true
        }
    }
    
    return $false
}

# =============================================================================
# DEPENDENCY GRAPH BUILDER - Build dependency relationships
# =============================================================================

function Build-DependencyGraph {
    param(
        [Parameter(Mandatory = $true)]
        [List[hashtable]]$Manifests
    )
    
    Write-Log "INFO" "Building dependency graph from $($Manifests.Count) manifests"
    
    $graph = @{
        Nodes = [List[hashtable]]::new()
        Edges = [List[hashtable]]::new()
        RootNodes = [List[string]]::new()
        LeafNodes = [List[string]]::new()
    }
    
    # Create nodes
    foreach ($manifest in $Manifests) {
        $node = @{
            Id = $manifest.FileInfo.Path
            Name = if ($manifest.Data.name) { $manifest.Data.name } else { [System.IO.Path]::GetFileNameWithoutExtension($manifest.FileInfo.Path) }
            Type = $manifest.Format
            Dependencies = [List[string]]::new()
            Dependents = [List[string]]::new()
            Metadata = $manifest
        }
        
        # Extract dependencies from manifest
        $deps = Extract-Dependencies -Manifest $manifest
        foreach ($dep in $deps) {
            $node.Dependencies.Add($dep)
        }
        
        $graph.Nodes.Add($node)
    }
    
    # Create edges based on dependencies
    foreach ($node in $graph.Nodes) {
        foreach ($dep in $node.Dependencies) {
            # Find the dependency node
            $depNode = $graph.Nodes | Where-Object { $_.Id -eq $dep -or $_.Name -eq $dep }
            
            if ($depNode) {
                $edge = @{
                    From = $node.Id
                    To = $depNode.Id
                    Type = "Dependency"
                    Weight = 1
                }
                $graph.Edges.Add($edge)
                
                # Add reverse dependency
                $depNode.Dependents.Add($node.Id)
            }
            else {
                Write-Log "WARNING" "Unresolved dependency: $dep in $($node.Name)"
            }
        }
    }
    
    # Identify root and leaf nodes
    foreach ($node in $graph.Nodes) {
        if ($node.Dependencies.Count -eq 0) {
            $graph.RootNodes.Add($node.Id)
        }
        
        if ($node.Dependents.Count -eq 0) {
            $graph.LeafNodes.Add($node.Id)
        }
    }
    
    # Update registry
    $script:ManifestRegistry.DependencyGraph = $graph
    
    Write-Log "INFO" "Dependency graph built: $($graph.Nodes.Count) nodes, $($graph.Edges.Count) edges"
    
    return $graph
}

function Extract-Dependencies {
    param([hashtable]$Manifest)
    
    $dependencies = [List[string]]::new()
    $data = $Manifest.Data
    
    # Look for common dependency patterns
    $dependencyPatterns = @(
        'dependencies',
        'requires',
        'imports',
        'includes',
        'references',
        'uses',
        'extends'
    )
    
    foreach ($pattern in $dependencyPatterns) {
        if ($data.$pattern) {
            if ($data.$pattern -is [array]) {
                $dependencies.AddRange($data.$pattern)
            }
            else {
                $dependencies.Add($data.$pattern)
            }
        }
    }
    
    # Search recursively in the data structure
    $deps = Search-DependenciesRecursive -Data $data
    foreach ($dep in $deps) {
        if (!$dependencies.Contains($dep)) {
            $dependencies.Add($dep)
        }
    }
    
    return $dependencies
}

function Search-DependenciesRecursive {
    param($Data)
    
    $dependencies = [List[string]]::new()
    
    if ($Data -is [string]) {
        # Check if string looks like a file path or reference
        if ($Data -match '\.(ps1|json|xml|yaml|yml|dll|exe)$') {
            $dependencies.Add($Data)
        }
    }
    elseif ($Data -is [hashtable] -or $Data -is [System.Management.Automation.PSCustomObject]) {
        foreach ($value in $Data.PSObject.Properties.Value) {
            $deps = Search-DependenciesRecursive -Data $value
            foreach ($dep in $deps) {
                if (!$dependencies.Contains($dep)) {
                    $dependencies.Add($dep)
                }
            }
        }
    }
    elseif ($Data -is [array]) {
        foreach ($item in $Data) {
            $deps = Search-DependenciesRecursive -Data $item
            foreach ($dep in $deps) {
                if (!$dependencies.Contains($dep)) {
                    $dependencies.Add($dep)
                }
            }
        }
    }
    
    return $dependencies
}

# =============================================================================
# MANIFEST TRACER PIPELINE - End-to-End Tracking
# =============================================================================

function Start-ManifestTracerPipeline {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SearchPath,
        
        [Parameter(Mandatory = $false)]
        [string]$OutputPath,
        
        [Parameter(Mandatory = $false)]
        [switch]$BuildDependencyGraph,
        
        [Parameter(Mandatory = $false)]
        [switch]$ValidateManifests,
        
        [Parameter(Mandatory = $false)]
        [switch]$GenerateReport,

        [Parameter(Mandatory = $false)]
        [string]$ReportFormat = "HTML"
    )
    
    $pipelineStartTime = Get-Date
    
    Write-Log "INFO" "Starting Manifest Tracer Pipeline"
    Write-Log "INFO" "Search Path: $SearchPath"
    Write-Log "INFO" "Output Path: $OutputPath"
    Write-Log "INFO" "Report Format: $ReportFormat"
    
    # Step 1: Discover manifests
    Write-Log "INFO" "=== Step 1: Manifest Discovery ==="
    $discoveredManifests = Find-Manifests -SearchPath $SearchPath -Recurse -IncludeHidden
    
    if (!$discoveredManifests -or $discoveredManifests.Count -eq 0) {
        Write-Log "WARNING" "No manifests discovered"
        return $false
    }
    
    # Step 2: Parse manifests
    Write-Log "INFO" "=== Step 2: Manifest Parsing ==="
    $parsedManifests = [List[hashtable]]::new()
    
    foreach ($manifestInfo in $discoveredManifests) {
        Write-Log "DEBUG" "Parsing: $($manifestInfo.FullPath)"
        
        $parsedManifest = Parse-Manifest -FilePath $manifestInfo.FullPath -Validate:$ValidateManifests
        
        if ($parsedManifest) {
            $parsedManifests.Add($parsedManifest)
            
            # Log trace
            $script:ManifestRegistry.TraceLog.Add(@{
                Timestamp = Get-Date
                Step = "Parse"
                ManifestPath = $manifestInfo.FullPath
                Success = $true
                Duration = 0
            })
        }
        else {
            Write-Log "WARNING" "Failed to parse: $($manifestInfo.FullPath)"
        }
    }
    
    Write-Log "INFO" "Parsed $($parsedManifests.Count) manifests successfully"
    
    # Step 3: Build dependency graph
    if ($BuildDependencyGraph) {
        Write-Log "INFO" "=== Step 3: Dependency Graph Construction ==="
        $dependencyGraph = Build-DependencyGraph -Manifests $parsedManifests
        
        if ($dependencyGraph) {
            Write-Log "INFO" "Dependency graph built with $($dependencyGraph.Nodes.Count) nodes"
        }
    }
    
    # Step 4: Validate manifests
    if ($ValidateManifests) {
        Write-Log "INFO" "=== Step 4: Manifest Validation ==="
        
        foreach ($manifest in $parsedManifests) {
            $validationResult = Validate-Manifest -Manifest $manifest
            $script:ManifestRegistry.ValidationResults.Add($validationResult)
            
            if (!$validationResult.IsValid) {
                Write-Log "WARNING" "Manifest validation failed: $($manifest.FileInfo.Path)"
                foreach ($error in $validationResult.Errors) {
                    Write-Log "ERROR" "  - $error"
                }
            }
        }
    }
    
    # Step 5: Generate report
    if ($GenerateReport -and $OutputPath) {
        Write-Log "INFO" "=== Step 5: Report Generation ==="
        
        if (!(Test-Path $OutputPath)) {
            New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
        }
        
        $reportPath = Generate-ManifestReport -OutputPath $OutputPath -Format $ReportFormat
        Write-Log "INFO" "Manifest report generated: $reportPath"
    }
    
    $pipelineEndTime = Get-Date
    $pipelineDuration = ($pipelineEndTime - $pipelineStartTime).TotalSeconds
    
    Write-Log "INFO" "=== Pipeline Completed ==="
    Write-Log "INFO" "Duration: $pipelineDuration seconds"
    Write-Log "INFO" "Manifests Discovered: $($discoveredManifests.Count)"
    Write-Log "INFO" "Manifests Parsed: $($parsedManifests.Count)"
    Write-Log "INFO" "Validation Results: $($script:ManifestRegistry.ValidationResults.Count)"
    
    return $true
}

# =============================================================================
# REPORTING - Generate comprehensive manifest analysis reports
# =============================================================================

function Generate-ManifestReport {
    param(
        [Parameter(Mandatory = $true)]
        [string]$OutputPath,
        
        [Parameter(Mandatory = $false)]
        [string]$Format = "HTML"
    )
    
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $reportPath = Join-Path $OutputPath "ManifestTracer_Report_$timestamp.$Format"
    
    Write-Log "INFO" "Generating $Format report with $($script:ManifestRegistry.ParsedManifests.Count) manifests..."
    
    # Use streaming file writes for large reports to avoid memory issues
    switch ($Format.ToUpper()) {
        "HTML" {
            Generate-HtmlReport -OutputPath $reportPath
        }
        "JSON" {
            Generate-JsonReport -OutputPath $reportPath
        }
        "MARKDOWN" {
            Generate-MarkdownReport -OutputPath $reportPath
        }
        default {
            Generate-HtmlReport -OutputPath $reportPath
        }
    }
    
    Write-Log "INFO" "Report generated: $reportPath"
    return $reportPath
}

function Generate-HtmlReport {
    param([string]$OutputPath)
    
    Write-Log "INFO" "Generating HTML report to $OutputPath"
    
    # Use streaming file write to avoid memory issues with large reports
    $stream = [System.IO.StreamWriter]::new($OutputPath, $false, [System.Text.Encoding]::UTF8)
    
    try {
        # Write header
        $stream.WriteLine(@"
<!DOCTYPE html>
<html>
<head>
    <title>Manifest Tracer Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 10px; margin-bottom: 20px; }
        .section { margin-bottom: 30px; }
        .manifest-item { border: 1px solid #ddd; padding: 10px; margin-bottom: 10px; }
        .valid { color: green; }
        .invalid { color: red; }
        .warning { color: orange; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        .progress { position: fixed; top: 10px; right: 10px; background: #fff; padding: 10px; border: 1px solid #ddd; }
    </style>
</head>
<body>
    <div class="progress" id="progress">Generating report...</div>
    <div class="header">
        <h1>Manifest Tracer Report</h1>
        <p>Generated: $(Get-Date)</p>
        <p>Total Manifests: $($script:ManifestRegistry.ParsedManifests.Count)</p>
    </div>
"@)
        
        # Write discovered manifests section (limit to first 100 to avoid huge HTML)
        $stream.WriteLine("    <div class='section'>")
        $stream.WriteLine("        <h2>Discovered Manifests</h2>")
        $stream.WriteLine("        <p>Showing first 100 of $($script:ManifestRegistry.DiscoveredManifests.Count) manifests</p>")
        $stream.WriteLine("        <table>")
        $stream.WriteLine("            <tr><th>Path</th><th>Format</th><th>Size</th><th>Last Modified</th></tr>")
        
        $displayCount = [Math]::Min(100, $script:ManifestRegistry.DiscoveredManifests.Count)
        for ($i = 0; $i -lt $displayCount; $i++) {
            $manifest = $script:ManifestRegistry.DiscoveredManifests[$i]
            $stream.WriteLine("            <tr>")
            $stream.WriteLine("                <td>$($manifest.FullPath)</td>")
            $stream.WriteLine("                <td>$($manifest.Format)</td>")
            $stream.WriteLine("                <td>$($manifest.Size)</td>")
            $stream.WriteLine("                <td>$($manifest.LastModified)</td>")
            $stream.WriteLine("            </tr>")
            
            # Update progress every 10 items
            if ($i % 10 -eq 0) {
                Write-Log "DEBUG" "Processed $i manifests for HTML report"
            }
        }
        
        if ($script:ManifestRegistry.DiscoveredManifests.Count -gt 100) {
            $stream.WriteLine("            <tr><td colspan='4'>... and $($script:ManifestRegistry.DiscoveredManifests.Count - 100) more manifests</td></tr>")
        }
        
        $stream.WriteLine("        </table>")
        $stream.WriteLine("    </div>")
        
        # Write validation results section
        $stream.WriteLine("    <div class='section'>")
        $stream.WriteLine("        <h2>Validation Results</h2>")
        $stream.WriteLine("        <table>")
        $stream.WriteLine("            <tr><th>Manifest</th><th>Valid</th><th>Error Count</th></tr>")
        
        $validationCount = [Math]::Min(100, $script:ManifestRegistry.ValidationResults.Count)
        for ($i = 0; $i -lt $validationCount; $i++) {
            $result = $script:ManifestRegistry.ValidationResults[$i]
            $validClass = if ($result.IsValid) { "valid" } else { "invalid" }
            $stream.WriteLine("            <tr>")
            $stream.WriteLine("                <td>$($result.ManifestPath)</td>")
            $stream.WriteLine("                <td class='$validClass'>$($result.IsValid)</td>")
            $stream.WriteLine("                <td>$($result.Errors.Count)</td>")
            $stream.WriteLine("            </tr>")
        }
        
        $stream.WriteLine("        </table>")
        $stream.WriteLine("    </div>")
        
        # Write footer
        $stream.WriteLine(@"
    <script>
        document.getElementById('progress').style.display = 'none';
    </script>
</body>
</html>
"@)
        
        Write-Log "SUCCESS" "HTML report generated successfully"
        return $true
    }
    catch {
        Write-Log "ERROR" "Failed to generate HTML report: $_"
        return $false
    }
    finally {
        $stream.Close()
    }
}
function Generate-JsonReport {
    param([string]$OutputPath)
    
    Write-Log "INFO" "Generating JSON report to $OutputPath"
    
    $summaryReport = @{
        Metadata = @{
            Generated = Get-Date
            Version = "1.0"
            Tool = "ManifestTracer"
        }
        Summary = @{
            TotalDiscovered = $script:ManifestRegistry.DiscoveredManifests.Count
            TotalParsed = $script:ManifestRegistry.ParsedManifests.Count
            TotalValidated = $script:ManifestRegistry.ValidationResults.Count
        }
        DiscoveredManifests = $script:ManifestRegistry.DiscoveredManifests | Select-Object -First 100 | ForEach-Object {
            @{
                Name = $_.Name
                Path = $_.FullPath
                Format = $_.Format
                Size = $_.Size
                LastModified = $_.LastModified
            }
        }
        ValidationSummary = $script:ManifestRegistry.ValidationResults | Select-Object -First 200 | ForEach-Object {
            @{
                ManifestPath = $_.ManifestPath
                IsValid = $_.IsValid
                ErrorCount = $_.Errors.Count
                WarningCount = if ($_.Warnings) { $_.Warnings.Count } else { 0 }
            }
        }
    }
    
    $summaryReport | ConvertTo-Json -Depth 5 | Out-File $OutputPath -Encoding UTF8
    Write-Log "SUCCESS" "JSON report generated successfully"
    return $true
}

function Generate-MarkdownReport {
    param([string]$OutputPath)
    
    Write-Log "INFO" "Generating Markdown report to $OutputPath"
    
    $stream = [System.IO.StreamWriter]::new($OutputPath, $false, [System.Text.Encoding]::UTF8)
    
    try {
        $stream.WriteLine("# Manifest Tracer Report")
        $stream.WriteLine("")
        $stream.WriteLine("**Generated:** $(Get-Date)")
        $stream.WriteLine("")
        $stream.WriteLine("## Summary")
        $stream.WriteLine("")
        $stream.WriteLine("- **Total Discovered:** $($script:ManifestRegistry.DiscoveredManifests.Count)")
        $stream.WriteLine("- **Total Parsed:** $($script:ManifestRegistry.ParsedManifests.Count)")
        $stream.WriteLine("- **Total Validated:** $($script:ManifestRegistry.ValidationResults.Count)")
        $stream.WriteLine("")
        
        $stream.WriteLine("## Discovered Manifests (First 50)")
        $stream.WriteLine("")
        $stream.WriteLine("| Path | Format | Confidence | Size |")
        $stream.WriteLine("|------|--------|------------|------|")
        
        $displayCount = [Math]::Min(50, $script:ManifestRegistry.DiscoveredManifests.Count)
        for ($i = 0; $i -lt $displayCount; $i++) {
            $manifest = $script:ManifestRegistry.DiscoveredManifests[$i]
            $sizeKB = [Math]::Round($manifest.Size / 1024, 2)
            $stream.WriteLine("| $($manifest.FullPath) | $($manifest.Format) | $($manifest.Confidence)% | ${sizeKB} KB |")
        }
        
        if ($script:ManifestRegistry.DiscoveredManifests.Count -gt 50) {
            $stream.WriteLine("")
            $stream.WriteLine("*... and $($script:ManifestRegistry.DiscoveredManifests.Count - 50) more manifests*")
        }
        
        if ($script:ManifestRegistry.ValidationResults.Count -gt 0) {
            $stream.WriteLine("")
            $stream.WriteLine("## Validation Results (First 50)")
            $stream.WriteLine("")
            $stream.WriteLine("| Manifest | Valid | Errors | Warnings |")
            $stream.WriteLine("|----------|-------|--------|----------|")
            
            $validationCount = [Math]::Min(50, $script:ManifestRegistry.ValidationResults.Count)
            for ($i = 0; $i -lt $validationCount; $i++) {
                $result = $script:ManifestRegistry.ValidationResults[$i]
                $valid = if ($result.IsValid) { "✅" } else { "❌" }
                $warningCount = if ($result.Warnings) { $result.Warnings.Count } else { 0 }
                $stream.WriteLine("| $($result.ManifestPath) | $valid | $($result.Errors.Count) | $warningCount |")
            }
        }
        
        Write-Log "SUCCESS" "Markdown report generated successfully"
        return $true
    }
    catch {
        Write-Log "ERROR" "Failed to generate Markdown report: $_"
        return $false
    }
    finally {
        $stream.Close()
    }
}

# =============================================================================
# INTEGRATION HELPER - Integrate with existing orchestrator
# =============================================================================

function Initialize-ManifestTracer {
    param(
        [Parameter(Mandatory = $true)]
        [string]$OrchestratorPath
    )
    
    Write-Log "INFO" "Initializing Manifest Tracer integration"
    
    # Check if orchestrator exists
    if (!(Test-Path $OrchestratorPath)) {
        Write-Log "ERROR" "Orchestrator not found: $OrchestratorPath"
        return $false
    }
    
    # Load orchestrator content
    $orchestratorContent = Get-Content $OrchestratorPath -Raw
    
    # Check if already integrated
    if ($orchestratorContent -match 'ManifestTracer|Start-ManifestTracerPipeline') {
        Write-Log "INFO" "Manifest Tracer already integrated"
        return $true
    }
    
    Write-Log "INFO" "Manifest Tracer integration ready"
    return $true
}

function Write-Log {
    param(
        [string]$Level = "INFO",
        [string]$Message,
        [string]$Component = "ManifestTracer"
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    $logEntry = "[$timestamp] [$Level] [$Component] $Message"
    
    # Console output
    $color = switch ($Level) {
        "ERROR" { "Red" }
        "WARNING" { "Yellow" }
        "SUCCESS" { "Green" }
        "INFO" { "Cyan" }
        "DEBUG" { "Gray" }
        default { "White" }
    }
    
    Write-Host $logEntry -ForegroundColor $color
    
    # File logging if path is available
    if ($global:LogFile) {
        Add-Content -Path $global:LogFile -Value $logEntry -Encoding UTF8
    }
}

# Export public functions
Export-ModuleMember -Function @(
    'Find-Manifests',
    'Parse-Manifest',
    'Validate-Manifest',
    'Build-DependencyGraph',
    'Start-ManifestTracerPipeline',
    'Generate-ManifestReport',
    'Initialize-ManifestTracer'
)
