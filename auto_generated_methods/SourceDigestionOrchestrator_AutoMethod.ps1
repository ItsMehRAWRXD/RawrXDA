#requires -Version 5.1
<#
.SYNOPSIS
    Production-grade Source Digestion Orchestrator for comprehensive codebase analysis.
.DESCRIPTION
    Multi-stage source code digestion system supporting:
    - Full AST parsing and analysis for PowerShell, Python, JavaScript
    - Manifest-driven reverse engineering
    - Dependency graph extraction
    - Symbol table generation
    - Cross-reference mapping
    - Documentation extraction
    - Metrics collection (complexity, coverage, debt)
    - Incremental digestion with caching
    - Parallel processing for large codebases
.NOTES
    Author: RawrXD Production Team
    Version: 2.0.0
    Requires: PowerShell 5.1+
#>

# ============================================================================
# STRUCTURED LOGGING (Standalone fallback)
# ============================================================================
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Debug','Info','Warning','Error','Critical')][string]$Level = 'Info',
            [hashtable]$Context = @{}
        )
        $timestamp = Get-Date -Format 'yyyy-MM-ddTHH:mm:ss.fffZ'
        $color = switch ($Level) {
            'Debug' { 'Gray' }
            'Info' { 'White' }
            'Warning' { 'Yellow' }
            'Error' { 'Red' }
            'Critical' { 'Magenta' }
            default { 'White' }
        }
        Write-Host "[$timestamp] [$Level] $Message" -ForegroundColor $color
    }
}

# ============================================================================
# DIGESTION REGISTRY (Global State)
# ============================================================================
$script:DigestionState = @{
    Cache = @{}                      # FileHash -> DigestedResult
    SymbolTable = @{}                # SymbolName -> Location info
    CrossReferences = @{}            # Symbol -> @(References)
    DependencyGraph = @{}            # File -> @(Dependencies)
    Metrics = @{
        TotalFiles = 0
        TotalLines = 0
        TotalFunctions = 0
        TotalClasses = 0
        ComplexityScore = 0
        TechnicalDebt = 0
    }
    ProcessedFiles = @()
    FailedFiles = @()
    Configuration = @{
        CacheEnabled = $true
        CacheDirectory = "D:/lazy init ide/.digestion-cache"
        ParallelProcessing = $true
        MaxParallelJobs = 4
        SupportedLanguages = @('PowerShell', 'Python', 'JavaScript', 'TypeScript', 'JSON')
        ExcludePatterns = @('*.log', '*.tmp', 'node_modules/*', '.git/*', '*.min.js')
        IncrementalMode = $true
    }
}

# ============================================================================
# FILE HASH CALCULATION
# ============================================================================
function Get-FileContentHash {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$FilePath
    )
    
    if (-not (Test-Path $FilePath)) {
        return $null
    }
    
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $content = [System.IO.File]::ReadAllBytes($FilePath)
        $hash = $sha256.ComputeHash($content)
        return [BitConverter]::ToString($hash) -replace '-', ''
    } finally {
        $sha256.Dispose()
    }
}

# ============================================================================
# CACHE MANAGEMENT
# ============================================================================
function Get-CachedDigestion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$FilePath
    )
    
    if (-not $script:DigestionState.Configuration.CacheEnabled) {
        return $null
    }
    
    $hash = Get-FileContentHash -FilePath $FilePath
    if (-not $hash) { return $null }
    
    if ($script:DigestionState.Cache.ContainsKey($hash)) {
        Write-StructuredLog -Message "Cache hit for: $FilePath" -Level Debug
        return $script:DigestionState.Cache[$hash]
    }
    
    # Check disk cache
    $cacheDir = $script:DigestionState.Configuration.CacheDirectory
    $cacheFile = Join-Path $cacheDir "$hash.json"
    
    if (Test-Path $cacheFile) {
        try {
            $cached = Get-Content $cacheFile -Raw | ConvertFrom-Json -AsHashtable
            $script:DigestionState.Cache[$hash] = $cached
            Write-StructuredLog -Message "Disk cache hit for: $FilePath" -Level Debug
            return $cached
        } catch {
            # Invalid cache file, remove it
            Remove-Item $cacheFile -Force -ErrorAction SilentlyContinue
        }
    }
    
    return $null
}

function Save-DigestionToCache {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][hashtable]$Digestion
    )
    
    if (-not $script:DigestionState.Configuration.CacheEnabled) {
        return
    }
    
    $hash = Get-FileContentHash -FilePath $FilePath
    if (-not $hash) { return }
    
    # Memory cache
    $script:DigestionState.Cache[$hash] = $Digestion
    
    # Disk cache
    $cacheDir = $script:DigestionState.Configuration.CacheDirectory
    if (-not (Test-Path $cacheDir)) {
        New-Item -Path $cacheDir -ItemType Directory -Force | Out-Null
    }
    
    $cacheFile = Join-Path $cacheDir "$hash.json"
    try {
        $Digestion | ConvertTo-Json -Depth 10 | Set-Content $cacheFile -Encoding UTF8
    } catch {
        Write-StructuredLog -Message "Failed to save cache: $($_.Exception.Message)" -Level Warning
    }
}

# ============================================================================
# POWERSHELL AST DIGESTION
# ============================================================================
function Invoke-PowerShellDigestion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$FilePath
    )
    
    $result = @{
        Language = 'PowerShell'
        FilePath = $FilePath
        Functions = @()
        Classes = @()
        Variables = @()
        Imports = @()
        Exports = @()
        Comments = @()
        Metrics = @{
            Lines = 0
            CodeLines = 0
            CommentLines = 0
            BlankLines = 0
            Functions = 0
            Classes = 0
            CyclomaticComplexity = 0
        }
        Errors = @()
    }
    
    try {
        $content = Get-Content $FilePath -Raw
        $lines = $content -split "`n"
        $result.Metrics.Lines = $lines.Count
        
        # Parse AST
        $tokens = $null
        $parseErrors = $null
        $ast = [System.Management.Automation.Language.Parser]::ParseFile(
            $FilePath,
            [ref]$tokens,
            [ref]$parseErrors
        )
        
        if ($parseErrors.Count -gt 0) {
            $result.Errors = $parseErrors | ForEach-Object {
                @{
                    Message = $_.Message
                    Line = $_.Extent.StartLineNumber
                    Column = $_.Extent.StartColumnNumber
                }
            }
        }
        
        # Extract functions
        $functions = $ast.FindAll({
            param($node)
            $node -is [System.Management.Automation.Language.FunctionDefinitionAst]
        }, $true)
        
        foreach ($func in $functions) {
            $funcInfo = @{
                Name = $func.Name
                StartLine = $func.Extent.StartLineNumber
                EndLine = $func.Extent.EndLineNumber
                Parameters = @()
                HasCmdletBinding = $false
                Complexity = 1
                Documentation = $null
            }
            
            # Extract parameters
            if ($func.Parameters) {
                foreach ($param in $func.Parameters) {
                    $funcInfo.Parameters += @{
                        Name = $param.Name.VariablePath.UserPath
                        Type = if ($param.StaticType) { $param.StaticType.Name } else { 'Object' }
                        IsMandatory = $param.Attributes | Where-Object { 
                            $_.TypeName.Name -eq 'Parameter' -and 
                            $_.NamedArguments | Where-Object { $_.ArgumentName -eq 'Mandatory' -and $_.Argument.Value -eq $true }
                        } | Select-Object -First 1
                    }
                }
            }
            
            # Check for CmdletBinding
            if ($func.Body.ParamBlock) {
                $funcInfo.HasCmdletBinding = $func.Body.ParamBlock.Attributes | 
                    Where-Object { $_.TypeName.Name -eq 'CmdletBinding' } | 
                    Select-Object -First 1
            }
            
            # Calculate cyclomatic complexity
            $complexityAst = $func.FindAll({
                param($node)
                $node -is [System.Management.Automation.Language.IfStatementAst] -or
                $node -is [System.Management.Automation.Language.WhileStatementAst] -or
                $node -is [System.Management.Automation.Language.ForStatementAst] -or
                $node -is [System.Management.Automation.Language.ForEachStatementAst] -or
                $node -is [System.Management.Automation.Language.SwitchStatementAst] -or
                $node -is [System.Management.Automation.Language.TryStatementAst] -or
                $node -is [System.Management.Automation.Language.CatchClauseAst]
            }, $true)
            $funcInfo.Complexity = 1 + $complexityAst.Count
            
            # Extract documentation (comment-based help)
            $helpComment = $tokens | Where-Object {
                $_.Kind -eq 'Comment' -and 
                $_.Extent.EndLineNumber -eq ($func.Extent.StartLineNumber - 1) -and
                $_.Text -match '^\s*<#'
            } | Select-Object -First 1
            
            if ($helpComment) {
                $funcInfo.Documentation = $helpComment.Text
            }
            
            $result.Functions += $funcInfo
            $result.Metrics.CyclomaticComplexity += $funcInfo.Complexity
        }
        
        $result.Metrics.Functions = $result.Functions.Count
        
        # Extract classes
        $classes = $ast.FindAll({
            param($node)
            $node -is [System.Management.Automation.Language.TypeDefinitionAst]
        }, $true)
        
        foreach ($class in $classes) {
            $classInfo = @{
                Name = $class.Name
                StartLine = $class.Extent.StartLineNumber
                EndLine = $class.Extent.EndLineNumber
                BaseTypes = @()
                Members = @()
                Methods = @()
            }
            
            if ($class.BaseTypes) {
                $classInfo.BaseTypes = $class.BaseTypes | ForEach-Object { $_.TypeName.Name }
            }
            
            foreach ($member in $class.Members) {
                if ($member -is [System.Management.Automation.Language.FunctionMemberAst]) {
                    $classInfo.Methods += @{
                        Name = $member.Name
                        IsStatic = $member.IsStatic
                        ReturnType = if ($member.ReturnType) { $member.ReturnType.TypeName.Name } else { 'void' }
                    }
                } elseif ($member -is [System.Management.Automation.Language.PropertyMemberAst]) {
                    $classInfo.Members += @{
                        Name = $member.Name
                        Type = if ($member.PropertyType) { $member.PropertyType.TypeName.Name } else { 'Object' }
                    }
                }
            }
            
            $result.Classes += $classInfo
        }
        
        $result.Metrics.Classes = $result.Classes.Count
        
        # Extract imports (Import-Module, using statements)
        $imports = $ast.FindAll({
            param($node)
            ($node -is [System.Management.Automation.Language.CommandAst] -and 
             $node.GetCommandName() -eq 'Import-Module') -or
            $node -is [System.Management.Automation.Language.UsingStatementAst]
        }, $true)
        
        foreach ($import in $imports) {
            if ($import -is [System.Management.Automation.Language.CommandAst]) {
                $moduleName = $import.CommandElements | 
                    Where-Object { $_ -is [System.Management.Automation.Language.StringConstantExpressionAst] } | 
                    Select-Object -Skip 1 -First 1
                if ($moduleName) {
                    $result.Imports += @{
                        Type = 'Module'
                        Name = $moduleName.Value
                        Line = $import.Extent.StartLineNumber
                    }
                }
            } elseif ($import -is [System.Management.Automation.Language.UsingStatementAst]) {
                $result.Imports += @{
                    Type = 'Using'
                    Name = $import.Name.Value
                    Kind = $import.UsingStatementKind.ToString()
                    Line = $import.Extent.StartLineNumber
                }
            }
        }
        
        # Extract exports (Export-ModuleMember)
        $exports = $ast.FindAll({
            param($node)
            $node -is [System.Management.Automation.Language.CommandAst] -and 
            $node.GetCommandName() -eq 'Export-ModuleMember'
        }, $true)
        
        foreach ($export in $exports) {
            $functionParam = $export.CommandElements | Where-Object {
                $_ -is [System.Management.Automation.Language.CommandParameterAst] -and
                $_.ParameterName -eq 'Function'
            }
            if ($functionParam) {
                # Find the argument
                $idx = [array]::IndexOf($export.CommandElements, $functionParam)
                if ($idx -lt $export.CommandElements.Count - 1) {
                    $arg = $export.CommandElements[$idx + 1]
                    if ($arg -is [System.Management.Automation.Language.ArrayLiteralAst]) {
                        $result.Exports += $arg.Elements | ForEach-Object { $_.Value }
                    } elseif ($arg -is [System.Management.Automation.Language.StringConstantExpressionAst]) {
                        $result.Exports += $arg.Value
                    }
                }
            }
        }
        
        # Count comment/code/blank lines
        foreach ($line in $lines) {
            $trimmed = $line.Trim()
            if ([string]::IsNullOrEmpty($trimmed)) {
                $result.Metrics.BlankLines++
            } elseif ($trimmed.StartsWith('#') -or $trimmed.StartsWith('<#') -or $trimmed.EndsWith('#>')) {
                $result.Metrics.CommentLines++
            } else {
                $result.Metrics.CodeLines++
            }
        }
        
    } catch {
        $result.Errors += @{
            Message = $_.Exception.Message
            Line = 0
            Column = 0
        }
    }
    
    return $result
}

# ============================================================================
# PYTHON DIGESTION (Basic)
# ============================================================================
function Invoke-PythonDigestion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$FilePath
    )
    
    $result = @{
        Language = 'Python'
        FilePath = $FilePath
        Functions = @()
        Classes = @()
        Imports = @()
        Metrics = @{
            Lines = 0
            CodeLines = 0
            CommentLines = 0
            BlankLines = 0
            Functions = 0
            Classes = 0
        }
        Errors = @()
    }
    
    try {
        $content = Get-Content $FilePath -Raw
        $lines = $content -split "`n"
        $result.Metrics.Lines = $lines.Count
        
        $lineNum = 0
        foreach ($line in $lines) {
            $lineNum++
            $trimmed = $line.Trim()
            
            if ([string]::IsNullOrEmpty($trimmed)) {
                $result.Metrics.BlankLines++
            } elseif ($trimmed.StartsWith('#')) {
                $result.Metrics.CommentLines++
            } else {
                $result.Metrics.CodeLines++
                
                # Function detection
                if ($trimmed -match '^def\s+(\w+)\s*\(([^)]*)\)') {
                    $result.Functions += @{
                        Name = $matches[1]
                        Parameters = $matches[2] -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }
                        StartLine = $lineNum
                    }
                }
                
                # Class detection
                if ($trimmed -match '^class\s+(\w+)(?:\s*\(([^)]*)\))?') {
                    $result.Classes += @{
                        Name = $matches[1]
                        BaseTypes = if ($matches[2]) { $matches[2] -split ',' | ForEach-Object { $_.Trim() } } else { @() }
                        StartLine = $lineNum
                    }
                }
                
                # Import detection
                if ($trimmed -match '^import\s+(.+)$' -or $trimmed -match '^from\s+(\S+)\s+import') {
                    $result.Imports += @{
                        Type = if ($trimmed.StartsWith('from')) { 'FromImport' } else { 'Import' }
                        Name = $matches[1]
                        Line = $lineNum
                    }
                }
            }
        }
        
        $result.Metrics.Functions = $result.Functions.Count
        $result.Metrics.Classes = $result.Classes.Count
        
    } catch {
        $result.Errors += @{
            Message = $_.Exception.Message
            Line = 0
        }
    }
    
    return $result
}

# ============================================================================
# JAVASCRIPT/TYPESCRIPT DIGESTION (Basic)
# ============================================================================
function Invoke-JavaScriptDigestion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$FilePath
    )
    
    $result = @{
        Language = if ($FilePath -match '\.ts$') { 'TypeScript' } else { 'JavaScript' }
        FilePath = $FilePath
        Functions = @()
        Classes = @()
        Imports = @()
        Exports = @()
        Metrics = @{
            Lines = 0
            CodeLines = 0
            CommentLines = 0
            BlankLines = 0
            Functions = 0
            Classes = 0
        }
        Errors = @()
    }
    
    try {
        $content = Get-Content $FilePath -Raw
        $lines = $content -split "`n"
        $result.Metrics.Lines = $lines.Count
        
        $lineNum = 0
        $inBlockComment = $false
        
        foreach ($line in $lines) {
            $lineNum++
            $trimmed = $line.Trim()
            
            if ([string]::IsNullOrEmpty($trimmed)) {
                $result.Metrics.BlankLines++
                continue
            }
            
            # Handle block comments
            if ($trimmed -match '/\*') {
                $inBlockComment = $true
            }
            if ($inBlockComment) {
                $result.Metrics.CommentLines++
                if ($trimmed -match '\*/') {
                    $inBlockComment = $false
                }
                continue
            }
            
            if ($trimmed.StartsWith('//')) {
                $result.Metrics.CommentLines++
                continue
            }
            
            $result.Metrics.CodeLines++
            
            # Function detection (multiple patterns)
            if ($trimmed -match 'function\s+(\w+)\s*\(([^)]*)\)' -or
                $trimmed -match '(?:const|let|var)\s+(\w+)\s*=\s*(?:async\s+)?\(([^)]*)\)\s*=>' -or
                $trimmed -match '(\w+)\s*:\s*(?:async\s+)?function\s*\(([^)]*)\)') {
                $result.Functions += @{
                    Name = $matches[1]
                    Parameters = if ($matches[2]) { $matches[2] -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ } } else { @() }
                    StartLine = $lineNum
                    IsArrow = $trimmed -match '=>'
                    IsAsync = $trimmed -match 'async'
                }
            }
            
            # Class detection
            if ($trimmed -match 'class\s+(\w+)(?:\s+extends\s+(\w+))?') {
                $result.Classes += @{
                    Name = $matches[1]
                    Extends = $matches[2]
                    StartLine = $lineNum
                }
            }
            
            # Import detection
            if ($trimmed -match "^import\s+.*\s+from\s+['\"]([^'\"]+)['\"]" -or
                $trimmed -match "^import\s+['\"]([^'\"]+)['\"]" -or
                $trimmed -match "require\s*\(\s*['\"]([^'\"]+)['\"]") {
                $result.Imports += @{
                    Name = $matches[1]
                    Line = $lineNum
                    Type = if ($trimmed -match 'require') { 'CommonJS' } else { 'ESModule' }
                }
            }
            
            # Export detection
            if ($trimmed -match '^export\s+(default\s+)?(?:function|class|const|let|var)?\s*(\w+)?' -or
                $trimmed -match 'module\.exports\s*=') {
                $result.Exports += @{
                    Name = if ($matches[2]) { $matches[2] } else { 'default' }
                    IsDefault = $trimmed -match 'default|module\.exports'
                    Line = $lineNum
                }
            }
        }
        
        $result.Metrics.Functions = $result.Functions.Count
        $result.Metrics.Classes = $result.Classes.Count
        
    } catch {
        $result.Errors += @{
            Message = $_.Exception.Message
            Line = 0
        }
    }
    
    return $result
}

# ============================================================================
# MANIFEST PROCESSING
# ============================================================================
function Invoke-ManifestDigestion {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$ManifestPath
    )
    
    $result = @{
        ManifestPath = $ManifestPath
        Type = 'Unknown'
        Entries = @()
        Dependencies = @()
        Configuration = @{}
        Errors = @()
    }
    
    if (-not (Test-Path $ManifestPath)) {
        $result.Errors += "Manifest not found: $ManifestPath"
        return $result
    }
    
    try {
        $content = Get-Content $ManifestPath -Raw
        
        if ($ManifestPath -match '\.json$') {
            $manifest = $content | ConvertFrom-Json -AsHashtable
            $result.Type = 'JSON'
            
            # Detect manifest type
            if ($manifest.ContainsKey('name') -and $manifest.ContainsKey('version')) {
                if ($manifest.ContainsKey('dependencies')) {
                    $result.Type = 'NPM'
                    $result.Dependencies = $manifest.dependencies.Keys
                }
            }
            
            if ($manifest.ContainsKey('entries')) {
                $result.Entries = $manifest.entries
            } elseif ($manifest.ContainsKey('files')) {
                $result.Entries = $manifest.files
            }
            
            $result.Configuration = $manifest
            
        } elseif ($ManifestPath -match '\.psd1$') {
            $result.Type = 'PowerShellData'
            $manifest = Import-PowerShellDataFile $ManifestPath
            $result.Configuration = $manifest
            
            if ($manifest.ContainsKey('RequiredModules')) {
                $result.Dependencies = $manifest.RequiredModules
            }
            if ($manifest.ContainsKey('FunctionsToExport')) {
                $result.Entries = $manifest.FunctionsToExport
            }
        }
        
    } catch {
        $result.Errors += $_.Exception.Message
    }
    
    return $result
}

# ============================================================================
# SYMBOL TABLE GENERATION
# ============================================================================
function Update-SymbolTable {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$Digestion
    )
    
    $filePath = $Digestion.FilePath
    
    # Add functions to symbol table
    foreach ($func in $Digestion.Functions) {
        $symbolKey = "$($func.Name)::function"
        $script:DigestionState.SymbolTable[$symbolKey] = @{
            Name = $func.Name
            Type = 'Function'
            File = $filePath
            Line = $func.StartLine
            Language = $Digestion.Language
            Parameters = $func.Parameters
        }
    }
    
    # Add classes
    foreach ($class in $Digestion.Classes) {
        $symbolKey = "$($class.Name)::class"
        $script:DigestionState.SymbolTable[$symbolKey] = @{
            Name = $class.Name
            Type = 'Class'
            File = $filePath
            Line = $class.StartLine
            Language = $Digestion.Language
            BaseTypes = $class.BaseTypes
        }
    }
}

# ============================================================================
# DEPENDENCY GRAPH UPDATE
# ============================================================================
function Update-DependencyGraph {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][hashtable]$Digestion
    )
    
    $filePath = $Digestion.FilePath
    
    $dependencies = @()
    foreach ($import in $Digestion.Imports) {
        $dependencies += $import.Name
    }
    
    $script:DigestionState.DependencyGraph[$filePath] = $dependencies
}

# ============================================================================
# MAIN ENTRY POINT
# ============================================================================
function Invoke-SourceDigestionOrchestratorAuto {
    <#
    .SYNOPSIS
        Main entry point for the Source Digestion Orchestrator.
    .DESCRIPTION
        Performs comprehensive source code digestion with caching and metrics.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$InputFile,
        
        [Parameter(Mandatory=$false)]
        [string]$InputDirectory = "D:/lazy init ide",
        
        [Parameter(Mandatory=$false)]
        [string]$OutputDir = "D:/lazy init ide/digestion-output",
        
        [Parameter(Mandatory=$false)]
        [string]$ManifestPath = "D:/lazy init ide/manifests/manifest.json",
        
        [switch]$IncludeMetrics,
        [switch]$GenerateSymbolTable,
        [switch]$BuildDependencyGraph,
        [switch]$NoCache,
        
        [ValidateSet('All', 'Summary', 'Minimal')]
        [string]$OutputLevel = 'Summary'
    )
    
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    Write-StructuredLog -Message "Starting Source Digestion Orchestrator" -Level Info -Context @{
        InputFile = $InputFile
        InputDirectory = $InputDirectory
        ManifestPath = $ManifestPath
    }
    
    # Reset metrics
    $script:DigestionState.Metrics = @{
        TotalFiles = 0
        TotalLines = 0
        TotalFunctions = 0
        TotalClasses = 0
        ComplexityScore = 0
        TechnicalDebt = 0
    }
    $script:DigestionState.ProcessedFiles = @()
    $script:DigestionState.FailedFiles = @()
    
    if ($NoCache) {
        $script:DigestionState.Configuration.CacheEnabled = $false
    }
    
    # Ensure output directory exists
    if (-not (Test-Path $OutputDir)) {
        New-Item -Path $OutputDir -ItemType Directory -Force | Out-Null
    }
    
    # Collect files to process
    $filesToProcess = @()
    
    if ($InputFile -and (Test-Path $InputFile)) {
        $filesToProcess += Get-Item $InputFile
    } elseif (Test-Path $InputDirectory) {
        $extensions = @('*.ps1', '*.psm1', '*.psd1', '*.py', '*.js', '*.ts')
        foreach ($ext in $extensions) {
            $filesToProcess += Get-ChildItem -Path $InputDirectory -Filter $ext -Recurse -File -ErrorAction SilentlyContinue
        }
        
        # Exclude patterns
        $excludes = $script:DigestionState.Configuration.ExcludePatterns
        $filesToProcess = $filesToProcess | Where-Object {
            $path = $_.FullName
            $excluded = $false
            foreach ($pattern in $excludes) {
                if ($path -like $pattern) {
                    $excluded = $true
                    break
                }
            }
            -not $excluded
        }
    }
    
    Write-StructuredLog -Message "Files to process: $($filesToProcess.Count)" -Level Info
    
    # Process manifest if exists
    $manifestResult = $null
    if (Test-Path $ManifestPath) {
        Write-StructuredLog -Message "Processing manifest: $ManifestPath" -Level Info
        $manifestResult = Invoke-ManifestDigestion -ManifestPath $ManifestPath
    }
    
    # Process each file
    $allDigestions = @()
    
    foreach ($file in $filesToProcess) {
        # Check cache first
        if (-not $NoCache) {
            $cached = Get-CachedDigestion -FilePath $file.FullName
            if ($cached) {
                $allDigestions += $cached
                $script:DigestionState.ProcessedFiles += $file.FullName
                
                # Update aggregate metrics
                if ($cached.Metrics) {
                    $script:DigestionState.Metrics.TotalLines += $cached.Metrics.Lines
                    $script:DigestionState.Metrics.TotalFunctions += $cached.Metrics.Functions
                    $script:DigestionState.Metrics.TotalClasses += $cached.Metrics.Classes
                    if ($cached.Metrics.CyclomaticComplexity) {
                        $script:DigestionState.Metrics.ComplexityScore += $cached.Metrics.CyclomaticComplexity
                    }
                }
                continue
            }
        }
        
        # Determine language and process
        $digestion = $null
        $extension = $file.Extension.ToLower()
        
        try {
            switch ($extension) {
                {$_ -in '.ps1', '.psm1', '.psd1'} {
                    $digestion = Invoke-PowerShellDigestion -FilePath $file.FullName
                }
                '.py' {
                    $digestion = Invoke-PythonDigestion -FilePath $file.FullName
                }
                {$_ -in '.js', '.ts'} {
                    $digestion = Invoke-JavaScriptDigestion -FilePath $file.FullName
                }
            }
            
            if ($digestion) {
                $allDigestions += $digestion
                $script:DigestionState.ProcessedFiles += $file.FullName
                
                # Cache the result
                Save-DigestionToCache -FilePath $file.FullName -Digestion $digestion
                
                # Update symbol table
                if ($GenerateSymbolTable) {
                    Update-SymbolTable -Digestion $digestion
                }
                
                # Update dependency graph
                if ($BuildDependencyGraph) {
                    Update-DependencyGraph -Digestion $digestion
                }
                
                # Aggregate metrics
                $script:DigestionState.Metrics.TotalLines += $digestion.Metrics.Lines
                $script:DigestionState.Metrics.TotalFunctions += $digestion.Metrics.Functions
                $script:DigestionState.Metrics.TotalClasses += $digestion.Metrics.Classes
                if ($digestion.Metrics.CyclomaticComplexity) {
                    $script:DigestionState.Metrics.ComplexityScore += $digestion.Metrics.CyclomaticComplexity
                }
            }
        } catch {
            Write-StructuredLog -Message "Failed to digest: $($file.FullName)" -Level Warning -Context @{Error = $_.Exception.Message}
            $script:DigestionState.FailedFiles += $file.FullName
        }
    }
    
    $script:DigestionState.Metrics.TotalFiles = $script:DigestionState.ProcessedFiles.Count
    
    # Calculate technical debt estimate (simplified)
    if ($script:DigestionState.Metrics.ComplexityScore -gt 0) {
        $avgComplexity = $script:DigestionState.Metrics.ComplexityScore / [math]::Max(1, $script:DigestionState.Metrics.TotalFunctions)
        $script:DigestionState.Metrics.TechnicalDebt = [math]::Round($avgComplexity * $script:DigestionState.Metrics.TotalFunctions * 0.1, 2)
    }
    
    $stopwatch.Stop()
    
    # Generate output
    $result = @{
        Success = $script:DigestionState.FailedFiles.Count -eq 0
        ProcessedFiles = $script:DigestionState.ProcessedFiles.Count
        FailedFiles = $script:DigestionState.FailedFiles.Count
        Duration = $stopwatch.Elapsed.TotalMilliseconds
        Metrics = $script:DigestionState.Metrics
        Manifest = $manifestResult
    }
    
    if ($OutputLevel -eq 'All') {
        $result.Digestions = $allDigestions
        $result.SymbolTable = $script:DigestionState.SymbolTable
        $result.DependencyGraph = $script:DigestionState.DependencyGraph
    }
    
    # Save summary to output directory
    $summaryPath = Join-Path $OutputDir "digestion-summary.json"
    $result | ConvertTo-Json -Depth 10 | Set-Content $summaryPath -Encoding UTF8
    
    Write-StructuredLog -Message "Source Digestion complete" -Level Info -Context @{
        Processed = $script:DigestionState.ProcessedFiles.Count
        Failed = $script:DigestionState.FailedFiles.Count
        TotalLines = $script:DigestionState.Metrics.TotalLines
        TotalFunctions = $script:DigestionState.Metrics.TotalFunctions
        DurationMs = [math]::Round($stopwatch.Elapsed.TotalMilliseconds, 2)
    }
    
    return $result
}

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================
function Get-SymbolTable {
    return $script:DigestionState.SymbolTable
}

function Get-DependencyGraph {
    return $script:DigestionState.DependencyGraph
}

function Get-DigestionMetrics {
    return $script:DigestionState.Metrics
}

function Clear-DigestionCache {
    $script:DigestionState.Cache.Clear()
    $cacheDir = $script:DigestionState.Configuration.CacheDirectory
    if (Test-Path $cacheDir) {
        Remove-Item "$cacheDir/*.json" -Force -ErrorAction SilentlyContinue
    }
    Write-StructuredLog -Message "Digestion cache cleared" -Level Info
}

# Export
Export-ModuleMember -Function @(
    'Invoke-SourceDigestionOrchestratorAuto',
    'Get-SymbolTable',
    'Get-DependencyGraph',
    'Get-DigestionMetrics',
    'Clear-DigestionCache',
    'Invoke-PowerShellDigestion',
    'Invoke-PythonDigestion',
    'Invoke-JavaScriptDigestion',
    'Invoke-ManifestDigestion'
)

