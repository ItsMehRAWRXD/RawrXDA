<#
.SYNOPSIS
    Production AutoDependencyGraph - Full AST-based dependency analysis engine

.DESCRIPTION
    Performs comprehensive dependency analysis using PowerShell AST parsing.
    Detects circular dependencies, generates DOT/Mermaid visualizations,
    calculates dependency metrics, and provides actionable insights.

.PARAMETER SourceDir
    Root directory to scan for PowerShell files

.PARAMETER OutputFormat
    Output format: JSON, DOT (GraphViz), Mermaid, or All

.PARAMETER DetectCircular
    Enable circular dependency detection

.PARAMETER IncludeMetrics
    Include dependency health metrics (afferent/efferent coupling)

.EXAMPLE
    Invoke-AutoDependencyGraph -SourceDir 'D:/project' -OutputFormat 'All' -DetectCircular
#>

if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [string]$Function = '',
            [hashtable]$Data = $null
        )
        $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$timestamp][$Level] $Message" -ForegroundColor $color
    }
}

function Get-ASTDependencies {
    <#
    .SYNOPSIS
        Extract dependencies from a PowerShell file using AST parsing
    #>
    param(
        [Parameter(Mandatory=$true)][string]$FilePath
    )

    $dependencies = @{
        ImportModule = @()
        DotSource = @()
        RequiresModule = @()
        UsingModule = @()
        FunctionCalls = @()
        VariableReferences = @()
    }

    try {
        $content = Get-Content -Path $FilePath -Raw -ErrorAction Stop
        $tokens = $null
        $errors = $null
        $ast = [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$tokens, [ref]$errors)

        # Find Import-Module commands
        $importModuleCalls = $ast.FindAll({
            param($node)
            $node -is [System.Management.Automation.Language.CommandAst] -and
            $node.GetCommandName() -eq 'Import-Module'
        }, $true)

        foreach ($call in $importModuleCalls) {
            $moduleArg = $call.CommandElements | Where-Object { $_ -isnot [System.Management.Automation.Language.StringConstantExpressionAst] -or $_.Value -ne 'Import-Module' } | Select-Object -First 1
            if ($moduleArg) {
                $moduleName = $moduleArg.Extent.Text -replace '^[''"]|[''"]$', ''
                $dependencies.ImportModule += @{
                    Name = $moduleName
                    Line = $call.Extent.StartLineNumber
                    Column = $call.Extent.StartColumnNumber
                    IsForced = ($call.CommandElements | Where-Object { $_.Extent.Text -eq '-Force' }) -ne $null
                }
            }
        }

        # Find dot-sourced files
        $dotSourceCalls = $ast.FindAll({
            param($node)
            $node -is [System.Management.Automation.Language.CommandAst] -and
            $node.InvocationOperator -eq [System.Management.Automation.Language.TokenKind]::Dot
        }, $true)

        foreach ($call in $dotSourceCalls) {
            $sourcePath = $call.CommandElements[0].Extent.Text -replace '^[''"]|[''"]$', ''
            $dependencies.DotSource += @{
                Path = $sourcePath
                Line = $call.Extent.StartLineNumber
                Resolved = $null
            }
        }

        # Find #Requires -Module statements
        $requiresMatches = [regex]::Matches($content, '#Requires\s+-Module\s+(\S+)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        foreach ($match in $requiresMatches) {
            $dependencies.RequiresModule += $match.Groups[1].Value
        }

        # Find using module statements
        $usingStatements = $ast.FindAll({
            param($node)
            $node -is [System.Management.Automation.Language.UsingStatementAst] -and
            $node.UsingStatementKind -eq [System.Management.Automation.Language.UsingStatementKind]::Module
        }, $true)

        foreach ($using in $usingStatements) {
            $dependencies.UsingModule += $using.Name.Value
        }

        # Find all function definitions in this file
        $functionDefs = $ast.FindAll({
            param($node)
            $node -is [System.Management.Automation.Language.FunctionDefinitionAst]
        }, $true)

        # Find external function calls (calls to functions not defined in this file)
        $localFunctions = $functionDefs | ForEach-Object { $_.Name }
        $allCalls = $ast.FindAll({
            param($node)
            $node -is [System.Management.Automation.Language.CommandAst]
        }, $true)

        foreach ($call in $allCalls) {
            $cmdName = $call.GetCommandName()
            if ($cmdName -and $cmdName -notin $localFunctions -and $cmdName -notmatch '^(Get-|Set-|New-|Remove-|Test-|Write-|Read-|Out-|Format-|Select-|Where-|ForEach-|Sort-|Group-)') {
                if ($cmdName -notin $dependencies.FunctionCalls) {
                    $dependencies.FunctionCalls += $cmdName
                }
            }
        }

    } catch {
        Write-StructuredLog -Level 'Warning' -Message "AST parsing failed for $FilePath : $_" -Function 'Get-ASTDependencies'
    }

    return $dependencies
}

function Find-CircularDependencies {
    <#
    .SYNOPSIS
        Detect circular dependencies using Tarjan's algorithm for strongly connected components
    #>
    param(
        [Parameter(Mandatory=$true)][hashtable]$Graph
    )

    $index = 0
    $stack = [System.Collections.Generic.Stack[string]]::new()
    $indices = @{}
    $lowlinks = @{}
    $onStack = @{}
    $sccs = @()

    function StrongConnect {
        param([string]$v)

        $indices[$v] = $script:index
        $lowlinks[$v] = $script:index
        $script:index++
        $stack.Push($v)
        $onStack[$v] = $true

        $neighbors = if ($Graph.ContainsKey($v)) { $Graph[$v] } else { @() }
        foreach ($w in $neighbors) {
            if (-not $indices.ContainsKey($w)) {
                StrongConnect -v $w
                $lowlinks[$v] = [Math]::Min($lowlinks[$v], $lowlinks[$w])
            } elseif ($onStack[$w]) {
                $lowlinks[$v] = [Math]::Min($lowlinks[$v], $indices[$w])
            }
        }

        if ($lowlinks[$v] -eq $indices[$v]) {
            $scc = @()
            do {
                $w = $stack.Pop()
                $onStack[$w] = $false
                $scc += $w
            } while ($w -ne $v)

            if ($scc.Count -gt 1) {
                $script:sccs += ,@($scc)
            }
        }
    }

    foreach ($node in $Graph.Keys) {
        if (-not $indices.ContainsKey($node)) {
            StrongConnect -v $node
        }
    }

    return $sccs
}

function Get-DependencyMetrics {
    <#
    .SYNOPSIS
        Calculate dependency health metrics
    #>
    param(
        [Parameter(Mandatory=$true)][hashtable]$Graph
    )

    $metrics = @{}

    foreach ($node in $Graph.Keys) {
        $efferent = ($Graph[$node] | Measure-Object).Count  # Dependencies this module has
        $afferent = ($Graph.Keys | Where-Object { $Graph[$_] -contains $node } | Measure-Object).Count  # Modules depending on this

        $instability = if (($efferent + $afferent) -gt 0) { $efferent / ($efferent + $afferent) } else { 0 }

        $metrics[$node] = @{
            EfferentCoupling = $efferent
            AfferentCoupling = $afferent
            Instability = [Math]::Round($instability, 3)
            IsHub = $afferent -gt 5
            IsBottleneck = $efferent -gt 10
        }
    }

    return $metrics
}

function Export-DependencyGraph {
    <#
    .SYNOPSIS
        Export dependency graph in various formats
    #>
    param(
        [Parameter(Mandatory=$true)][hashtable]$Graph,
        [Parameter(Mandatory=$true)][string]$OutputDir,
        [ValidateSet('JSON','DOT','Mermaid','All')][string]$Format = 'All'
    )

    $results = @{}

    # JSON Export
    if ($Format -in @('JSON', 'All')) {
        $jsonPath = Join-Path $OutputDir 'DependencyGraph.json'
        $Graph | ConvertTo-Json -Depth 10 | Set-Content $jsonPath -Encoding UTF8
        $results['JSON'] = $jsonPath
    }

    # DOT (GraphViz) Export
    if ($Format -in @('DOT', 'All')) {
        $dotPath = Join-Path $OutputDir 'DependencyGraph.dot'
        $dotContent = @("digraph Dependencies {", "    rankdir=LR;", "    node [shape=box, style=filled, fillcolor=lightblue];")

        foreach ($node in $Graph.Keys) {
            $nodeName = [System.IO.Path]::GetFileNameWithoutExtension($node) -replace '[^a-zA-Z0-9_]', '_'
            foreach ($dep in $Graph[$node]) {
                $depName = [System.IO.Path]::GetFileNameWithoutExtension($dep) -replace '[^a-zA-Z0-9_]', '_'
                $dotContent += "    `"$nodeName`" -> `"$depName`";"
            }
        }

        $dotContent += "}"
        $dotContent -join "`n" | Set-Content $dotPath -Encoding UTF8
        $results['DOT'] = $dotPath
    }

    # Mermaid Export
    if ($Format -in @('Mermaid', 'All')) {
        $mermaidPath = Join-Path $OutputDir 'DependencyGraph.mmd'
        $mermaidContent = @("graph LR")

        foreach ($node in $Graph.Keys) {
            $nodeName = [System.IO.Path]::GetFileNameWithoutExtension($node) -replace '[^a-zA-Z0-9_]', '_'
            foreach ($dep in $Graph[$node]) {
                $depName = [System.IO.Path]::GetFileNameWithoutExtension($dep) -replace '[^a-zA-Z0-9_]', '_'
                $mermaidContent += "    $nodeName --> $depName"
            }
        }

        $mermaidContent -join "`n" | Set-Content $mermaidPath -Encoding UTF8
        $results['Mermaid'] = $mermaidPath
    }

    return $results
}

function Invoke-AutoDependencyGraph {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [ValidateScript({Test-Path $_ -PathType 'Container'})]
        [string]$SourceDir = 'D:/lazy init ide',

        [ValidateSet('JSON','DOT','Mermaid','All')]
        [string]$OutputFormat = 'All',

        [switch]$DetectCircular,

        [switch]$IncludeMetrics,

        [string]$OutputDir = $null
    )

    $functionName = 'Invoke-AutoDependencyGraph'
    $startTime = Get-Date

    try {
        Write-StructuredLog -Level 'Info' -Message "Starting comprehensive dependency analysis for $SourceDir" -Function $functionName

        # Discover all PowerShell files
        $psFiles = Get-ChildItem -Path $SourceDir -Recurse -Include '*.ps1','*.psm1','*.psd1' -ErrorAction Stop
        Write-StructuredLog -Level 'Info' -Message "Found $($psFiles.Count) PowerShell files to analyze" -Function $functionName

        # Build comprehensive dependency graph
        $graph = @{}
        $fileMetadata = @{}
        $totalDependencies = 0

        foreach ($file in $psFiles) {
            Write-StructuredLog -Level 'Debug' -Message "Analyzing: $($file.Name)" -Function $functionName

            $deps = Get-ASTDependencies -FilePath $file.FullName

            # Normalize dependencies to file paths where possible
            $resolvedDeps = @()

            foreach ($import in $deps.ImportModule) {
                $moduleName = $import.Name
                # Try to resolve to actual file
                $resolved = $psFiles | Where-Object { $_.BaseName -eq $moduleName -or $_.Name -eq "$moduleName.psm1" } | Select-Object -First 1
                if ($resolved) {
                    $resolvedDeps += $resolved.FullName
                } else {
                    $resolvedDeps += $moduleName
                }
            }

            foreach ($dotSource in $deps.DotSource) {
                $sourcePath = $dotSource.Path
                # Resolve relative paths
                if (-not [System.IO.Path]::IsPathRooted($sourcePath)) {
                    $sourcePath = Join-Path (Split-Path $file.FullName) $sourcePath
                }
                if (Test-Path $sourcePath -ErrorAction SilentlyContinue) {
                    $resolvedDeps += (Resolve-Path $sourcePath).Path
                } else {
                    $resolvedDeps += $dotSource.Path
                }
            }

            $resolvedDeps += $deps.RequiresModule
            $resolvedDeps += $deps.UsingModule

            $graph[$file.FullName] = $resolvedDeps | Select-Object -Unique
            $totalDependencies += $resolvedDeps.Count

            $fileMetadata[$file.FullName] = @{
                FileName = $file.Name
                Directory = $file.DirectoryName
                Size = $file.Length
                LastModified = $file.LastWriteTime
                DependencyDetails = $deps
                DependencyCount = $resolvedDeps.Count
            }
        }

        Write-StructuredLog -Level 'Info' -Message "Dependency graph built: $($graph.Keys.Count) nodes, $totalDependencies edges" -Function $functionName

        # Detect circular dependencies if requested
        $circularDeps = @()
        if ($DetectCircular) {
            Write-StructuredLog -Level 'Info' -Message "Running circular dependency detection..." -Function $functionName
            $circularDeps = Find-CircularDependencies -Graph $graph

            if ($circularDeps.Count -gt 0) {
                Write-StructuredLog -Level 'Warning' -Message "Found $($circularDeps.Count) circular dependency cycles!" -Function $functionName
                foreach ($cycle in $circularDeps) {
                    $cycleNames = $cycle | ForEach-Object { [System.IO.Path]::GetFileName($_) }
                    Write-StructuredLog -Level 'Warning' -Message "Cycle: $($cycleNames -join ' -> ')" -Function $functionName
                }
            } else {
                Write-StructuredLog -Level 'Info' -Message "No circular dependencies detected" -Function $functionName
            }
        }

        # Calculate metrics if requested
        $metrics = @{}
        if ($IncludeMetrics) {
            Write-StructuredLog -Level 'Info' -Message "Calculating dependency metrics..." -Function $functionName
            $metrics = Get-DependencyMetrics -Graph $graph

            $hubs = $metrics.Keys | Where-Object { $metrics[$_].IsHub }
            $bottlenecks = $metrics.Keys | Where-Object { $metrics[$_].IsBottleneck }

            if ($hubs.Count -gt 0) {
                Write-StructuredLog -Level 'Info' -Message "Hub modules (high afferent coupling): $($hubs.Count)" -Function $functionName
            }
            if ($bottlenecks.Count -gt 0) {
                Write-StructuredLog -Level 'Warning' -Message "Bottleneck modules (high efferent coupling): $($bottlenecks.Count)" -Function $functionName
            }
        }

        # Determine output directory
        if (-not $OutputDir) {
            $OutputDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'auto_generated_methods'
        }
        if (-not (Test-Path $OutputDir)) {
            New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
        }

        # Export in requested formats
        $exportPaths = Export-DependencyGraph -Graph $graph -OutputDir $OutputDir -Format $OutputFormat

        # Build comprehensive report
        $report = [PSCustomObject]@{
            Timestamp = (Get-Date).ToString('o')
            SourceDirectory = $SourceDir
            AnalysisDuration = ((Get-Date) - $startTime).TotalSeconds
            Statistics = @{
                TotalFiles = $psFiles.Count
                TotalNodes = $graph.Keys.Count
                TotalEdges = $totalDependencies
                AverageDependencies = [Math]::Round($totalDependencies / [Math]::Max($graph.Keys.Count, 1), 2)
                MaxDependencies = ($graph.Values | ForEach-Object { $_.Count } | Measure-Object -Maximum).Maximum
                IsolatedModules = ($graph.Keys | Where-Object { $graph[$_].Count -eq 0 -and ($graph.Keys | Where-Object { $graph[$_] -contains $_ }).Count -eq 0 }).Count
            }
            CircularDependencies = @{
                Detected = $DetectCircular
                Count = $circularDeps.Count
                Cycles = $circularDeps | ForEach-Object { $_ | ForEach-Object { [System.IO.Path]::GetFileName($_) } }
            }
            Metrics = if ($IncludeMetrics) { $metrics } else { $null }
            FileMetadata = $fileMetadata
            Graph = $graph
            ExportedFiles = $exportPaths
        }

        # Save comprehensive report
        $reportPath = Join-Path $OutputDir 'DependencyAnalysis_Report.json'
        $report | ConvertTo-Json -Depth 15 | Set-Content $reportPath -Encoding UTF8

        $duration = ((Get-Date) - $startTime).TotalSeconds
        Write-StructuredLog -Level 'Info' -Message "Dependency analysis complete in $([Math]::Round($duration, 2))s. Report: $reportPath" -Function $functionName

        return $report

    } catch {
        Write-StructuredLog -Level 'Error' -Message "AutoDependencyGraph error: $($_.Exception.Message)" -Function $functionName -Data @{ Exception = $_.Exception.ToString() }
        throw
    }
}


