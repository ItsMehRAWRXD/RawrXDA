
# Cache for function results
$script:FunctionCache = @{}

function Get-FromCache {
    param([string]$Key)
    if ($script:FunctionCache.ContainsKey($Key)) {
        return $script:FunctionCache[$Key]
    }
    return $null
}

function Set-Cache {
    param([string]$Key, $Value)
    $script:FunctionCache[$Key] = $Value
}<#
.SYNOPSIS
    RawrXD Auto-Dependency Graph Module
    
.DESCRIPTION
    Production-ready module for generating comprehensive dependency graphs of PowerShell codebases.
    Analyzes Import-Module statements, function dependencies, and generates JSON reports.
    
.AUTHOR
    RawrXD Auto-Generation System
    
.VERSION
    1.0.0
#>

# Import required helpers
$scriptRoot = Split-Path -Parent $PSScriptRoot
$loggingModule = Join-Path $scriptRoot 'RawrXD.Logging.psm1'
$configModule = Join-Path $scriptRoot 'RawrXD.Config.psm1'

if (Test-Path $loggingModule) { 
    try { Import-Module $loggingModule -Force -ErrorAction SilentlyContinue } catch { } 
}
if (Test-Path $configModule) { 
    try { Import-Module $configModule -Force -ErrorAction SilentlyContinue } catch { } 
}

function Invoke-AutoDependencyGraph {
    <#
    .SYNOPSIS
        Generates a comprehensive dependency graph for a PowerShell codebase
        
    .DESCRIPTION
        Scans all PowerShell files in a directory tree and analyzes:
        - Import-Module statements
        - Function dependencies
        - Cross-file references
        Outputs detailed JSON report with visualization-ready data
        
    .PARAMETER SourceDir
        Root directory to analyze (defaults to parent of script root)
        
    .PARAMETER OutputPath
        Path for the dependency graph JSON file
        
    .PARAMETER IncludePatterns
        File patterns to include in analysis
        
    .PARAMETER ExcludePatterns  
        File patterns to exclude from analysis
        
    .PARAMETER MaxDepth
        Maximum recursion depth for directory traversal
        
    .EXAMPLE
        Invoke-AutoDependencyGraph -SourceDir "C:\MyProject" -OutputPath "deps.json"
        
    .OUTPUTS
        System.Collections.Hashtable
        Returns hashtable with dependency graph data
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [ValidateScript({Test-Path $_ -PathType Container})]
        [string]$SourceDir = $(if (Get-Command Get-RawrXDRootPath -ErrorAction SilentlyContinue) { 
            Get-RawrXDRootPath 
        } else { 
            Split-Path -Parent $PSScriptRoot 
        }),
        
        [Parameter(Mandatory=$false)]
        [string]$OutputPath = $null,
        
        [Parameter(Mandatory=$false)]
        [string[]]$IncludePatterns = @('*.ps1', '*.psm1'),
        
        [Parameter(Mandatory=$false)]
        [string[]]$ExcludePatterns = @('*.backup.*', '*_Broken.*'),
        
        [Parameter(Mandatory=$false)]
        [ValidateRange(1, 50)]
        [int]$MaxDepth = 10
    )
    
    if (Get-Command Measure-FunctionLatency -ErrorAction SilentlyContinue) {
        return Measure-FunctionLatency -FunctionName 'Invoke-AutoDependencyGraph' -Script {
            Invoke-DependencyAnalysis @PSBoundParameters
        }
    } else {
        return Invoke-DependencyAnalysis @PSBoundParameters
    }
}

function Invoke-DependencyAnalysis {
    [CmdletBinding()]
    param(
        [string]$SourceDir,
        [string]$OutputPath,
        [string[]]$IncludePatterns,
        [string[]]$ExcludePatterns,
        [int]$MaxDepth
    )
    
    try {
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'INFO' -Message "Starting comprehensive dependency analysis for $SourceDir" -Function 'Invoke-AutoDependencyGraph'
        } else {
            Write-Host "Starting dependency analysis for $SourceDir"
        }
        
        # Get PowerShell files with filtering
        $allFiles = @()
        foreach ($pattern in $IncludePatterns) {
            $files = Get-ChildItem -Path $SourceDir -Filter $pattern -Recurse -ErrorAction SilentlyContinue
            $allFiles += $files
        }
        
        # Apply exclusion patterns
        foreach ($excludePattern in $ExcludePatterns) {
            $allFiles = $allFiles | Where-Object { $_.Name -notlike $excludePattern }
        }
        
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'INFO' -Message "Found $($allFiles.Count) PowerShell files to analyze" -Function 'Invoke-AutoDependencyGraph'
        } else {
            Write-Host "Found $($allFiles.Count) PowerShell files to analyze"
        }
        
        $dependencyGraph = @{
            Metadata = @{
                GeneratedAt = (Get-Date).ToString('o')
                SourceDir = $SourceDir
                FilesAnalyzed = $allFiles.Count
                IncludePatterns = $IncludePatterns
                ExcludePatterns = $ExcludePatterns
                Version = '1.0.0'
            }
            Nodes = @{}
            Edges = @()
            Summary = @{
                TotalNodes = 0
                TotalEdges = 0
                OrphanedFiles = 0
                CircularDependencies = @()
            }
        }
        
        # Analyze each file
        $edgeCount = 0
        foreach ($file in $allFiles) {
            try {
                if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
                    Write-StructuredLog -Level 'DEBUG' -Message "Analyzing: $($file.Name)" -Function 'Invoke-AutoDependencyGraph'
                } else {
                    Write-Verbose "Analyzing: $($file.Name)"
                }
                
                $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
                if (-not $content) { continue }
                
                # Parse imports
                $imports = @()
                $importMatches = [regex]::Matches($content, 'Import-Module\s+[''"]?([^''";\s\)]+)[''"]?', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
                foreach ($match in $importMatches) {
                    $importPath = $match.Groups[1].Value
                    $imports += $importPath
                    
                    # Add edge to graph
                    $dependencyGraph.Edges += @{
                        Source = $file.FullName
                        Target = $importPath
                        Type = 'Import'
                        LineNumber = ($content.Substring(0, $match.Index) -split "`n").Count
                    }
                    $edgeCount++
                }
                
                # Parse function definitions
                $functions = @()
                $functionMatches = [regex]::Matches($content, 'function\s+([A-Za-z][A-Za-z0-9_-]*)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
                foreach ($match in $functionMatches) {
                    $functions += $match.Groups[1].Value
                }
                
                # Add node to graph
                $relativePath = $file.FullName.Replace($SourceDir, '').TrimStart('\', '/')
                $dependencyGraph.Nodes[$file.FullName] = @{
                    FilePath = $file.FullName
                    RelativePath = $relativePath
                    FileName = $file.Name
                    FileSize = $file.Length
                    LastModified = $file.LastWriteTime.ToString('o')
                    Imports = $imports
                    Functions = $functions
                    ImportCount = $imports.Count
                    FunctionCount = $functions.Count
                    FileType = $file.Extension.ToLower()
                }
                
            } catch {
                if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
                    Write-StructuredLog -Level 'WARNING' -Message "AST parsing failed for $($file.FullName) : $($_.Exception.Message)" -Function 'Invoke-AutoDependencyGraph'
                } else {
                    Write-Warning "Failed to parse $($file.FullName): $($_.Exception.Message)"
                }
            }
        }
        
        # Update summary
        $dependencyGraph.Summary.TotalNodes = $dependencyGraph.Nodes.Count
        $dependencyGraph.Summary.TotalEdges = $edgeCount
        $dependencyGraph.Summary.OrphanedFiles = ($dependencyGraph.Nodes.Values | Where-Object { $_.ImportCount -eq 0 }).Count
        
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'INFO' -Message "Dependency graph built: $($dependencyGraph.Summary.TotalNodes) nodes, $($dependencyGraph.Summary.TotalEdges) edges" -Function 'Invoke-AutoDependencyGraph'
        } else {
            Write-Host "Dependency graph built: $($dependencyGraph.Summary.TotalNodes) nodes, $($dependencyGraph.Summary.TotalEdges) edges"
        }
        
        # Save to file
        if (-not $OutputPath) {
            if (Get-Command Get-RawrXDRootPath -ErrorAction SilentlyContinue) {
                $OutputPath = Join-Path (Get-RawrXDRootPath) 'auto_generated_methods/DependencyAnalysis_Report.json'
            } else {
                $OutputPath = Join-Path $SourceDir 'DependencyAnalysis_Report.json'
            }
        }
        
        $dependencyGraph | ConvertTo-Json -Depth 10 | Set-Content $OutputPath -Encoding UTF8
        
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'INFO' -Message "Dependency analysis complete in $([math]::Round($stopwatch.Elapsed.TotalSeconds,2))s. Report: $OutputPath" -Function 'Invoke-AutoDependencyGraph'
        } else {
            Write-Host "Dependency analysis complete. Report: $OutputPath"
        }
        
        return $dependencyGraph
        
    } catch {
        if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
            Write-StructuredLog -Level 'ERROR' -Message $_.Exception.Message -Function 'Invoke-AutoDependencyGraph'
        } else {
            Write-Error $_.Exception.Message
        }
        throw
    }
}

# Export public functions
Export-ModuleMember -Function @(
    'Invoke-AutoDependencyGraph'
)

