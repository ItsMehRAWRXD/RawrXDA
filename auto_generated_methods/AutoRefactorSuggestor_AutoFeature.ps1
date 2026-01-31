<#
.SYNOPSIS
    Production AutoRefactorSuggestor - Comprehensive code quality analysis engine

.DESCRIPTION
    Performs deep code analysis using PowerShell AST to detect code smells,
    calculate complexity metrics, identify anti-patterns, and provide
    actionable refactoring suggestions with automated fixes where possible.

.PARAMETER SourceDir
    Directory to analyze for refactoring opportunities

.PARAMETER Severity
    Minimum severity to report: Critical, High, Medium, Low

.PARAMETER EnableAutoFix
    Generate auto-fix scripts for applicable issues

.PARAMETER MaxCyclomaticComplexity
    Threshold for cyclomatic complexity warnings (default: 10)

.EXAMPLE
    Invoke-AutoRefactorSuggestor -SourceDir 'D:/project' -EnableAutoFix -MaxCyclomaticComplexity 15
#>

if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [hashtable]$Data = $null
        )
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$Level] $Message" -ForegroundColor $color
    }
}

# Code smell detection patterns
$script:CodeSmellPatterns = @(
    @{
        Name = 'Write-Host in Production Code'
        Pattern = 'Write-Host\s+'
        Severity = 'Medium'
        Category = 'Logging'
        Description = 'Write-Host bypasses the pipeline and cannot be captured or redirected.'
        Suggestion = 'Replace with Write-Output, Write-Verbose, or structured logging.'
        AutoFixable = $true
        FixPattern = 'Write-Host\s+([''"].*?[''"])'
        FixReplacement = 'Write-Output $1'
    },
    @{
        Name = 'Magic Numbers'
        Pattern = '(?<![''"\w])(?<![_\-])\b(?!0\b|1\b|2\b)(\d{2,})\b(?![''"\w])'
        Severity = 'Low'
        Category = 'Maintainability'
        Description = 'Magic numbers make code harder to understand and maintain.'
        Suggestion = 'Extract numeric literals to named constants or configuration.'
        AutoFixable = $false
    },
    @{
        Name = 'Deeply Nested Code'
        Pattern = '(?:if|foreach|while|for|switch)\s*\([^)]*\)\s*\{[^{}]*(?:if|foreach|while|for|switch)\s*\([^)]*\)\s*\{[^{}]*(?:if|foreach|while|for|switch)'
        Severity = 'High'
        Category = 'Complexity'
        Description = 'Deeply nested code is hard to read, test, and maintain.'
        Suggestion = 'Extract nested logic to separate functions or use early returns.'
        AutoFixable = $false
    },
    @{
        Name = 'Cmdlet Alias Usage'
        Pattern = '\b(cd|dir|ls|cat|echo|copy|move|del|rm|cls|ps|kill|sort|select|where|foreach|%|?)\b(?=\s+[^=]|\s*$|\s*\|)'
        Severity = 'Low'
        Category = 'Readability'
        Description = 'Aliases reduce readability and may not exist in all environments.'
        Suggestion = 'Use full cmdlet names for better readability and compatibility.'
        AutoFixable = $true
        AliasMap = @{
            'cd' = 'Set-Location'
            'dir' = 'Get-ChildItem'
            'ls' = 'Get-ChildItem'
            'cat' = 'Get-Content'
            'echo' = 'Write-Output'
            'copy' = 'Copy-Item'
            'move' = 'Move-Item'
            'del' = 'Remove-Item'
            'rm' = 'Remove-Item'
            'cls' = 'Clear-Host'
            'ps' = 'Get-Process'
            'kill' = 'Stop-Process'
            '%' = 'ForEach-Object'
            '?' = 'Where-Object'
        }
    },
    @{
        Name = 'Backtick Line Continuation'
        Pattern = '`\s*$'
        Severity = 'Low'
        Category = 'Readability'
        Description = 'Backtick continuation is fragile and easily broken by trailing whitespace.'
        Suggestion = 'Use splatting, pipeline breaks, or natural line breaks after operators.'
        AutoFixable = $false
    },
    @{
        Name = 'Empty Catch Block'
        Pattern = 'catch\s*(?:\[[\w\.]+\])?\s*\{\s*\}'
        Severity = 'High'
        Category = 'Error Handling'
        Description = 'Empty catch blocks swallow exceptions silently.'
        Suggestion = 'Log exceptions or rethrow with context. Never silently catch.'
        AutoFixable = $false
    },
    @{
        Name = 'Catch-All Without Logging'
        Pattern = 'catch\s*\{(?![^}]*(?:Write-|Log-|\$_))[^}]*\}'
        Severity = 'Medium'
        Category = 'Error Handling'
        Description = 'Catching all exceptions without logging loses diagnostic information.'
        Suggestion = 'Always log caught exceptions with full context.'
        AutoFixable = $false
    },
    @{
        Name = 'Multiple Return Statements'
        Pattern = '(?s)function\s+\w+[^{]*\{(?:[^{}]|\{[^{}]*\})*return[^{}]*return'
        Severity = 'Low'
        Category = 'Complexity'
        Description = 'Multiple return statements can make function flow hard to follow.'
        Suggestion = 'Consider single exit point pattern or early return pattern consistently.'
        AutoFixable = $false
    },
    @{
        Name = 'Global Variable Usage'
        Pattern = '\$global:'
        Severity = 'Medium'
        Category = 'Design'
        Description = 'Global variables create hidden dependencies and make testing difficult.'
        Suggestion = 'Use script scope, parameters, or return values instead.'
        AutoFixable = $false
    },
    @{
        Name = 'Large Function'
        Type = 'AST'
        Severity = 'Medium'
        Category = 'Maintainability'
        Description = 'Functions longer than 50 lines are harder to understand and test.'
        Suggestion = 'Break down into smaller, focused functions with single responsibility.'
        Threshold = 50
    },
    @{
        Name = 'Too Many Parameters'
        Type = 'AST'
        Severity = 'Medium'
        Category = 'Design'
        Description = 'Functions with many parameters are hard to use and test.'
        Suggestion = 'Consider using parameter objects, splatting, or breaking up the function.'
        Threshold = 7
    },
    @{
        Name = 'Duplicate Code Blocks'
        Type = 'Analysis'
        Severity = 'High'
        Category = 'DRY Violation'
        Description = 'Duplicate code increases maintenance burden and bug risk.'
        Suggestion = 'Extract common code to reusable functions.'
    }
)

function Get-CyclomaticComplexity {
    <#
    .SYNOPSIS
        Calculate cyclomatic complexity of a function using AST
    #>
    param(
        [System.Management.Automation.Language.FunctionDefinitionAst]$FunctionAst
    )

    $complexity = 1  # Base complexity

    $decisionPoints = $FunctionAst.Body.FindAll({
        param($node)
        $node -is [System.Management.Automation.Language.IfStatementAst] -or
        $node -is [System.Management.Automation.Language.SwitchStatementAst] -or
        $node -is [System.Management.Automation.Language.ForStatementAst] -or
        $node -is [System.Management.Automation.Language.ForEachStatementAst] -or
        $node -is [System.Management.Automation.Language.WhileStatementAst] -or
        $node -is [System.Management.Automation.Language.DoWhileStatementAst] -or
        $node -is [System.Management.Automation.Language.DoUntilStatementAst] -or
        $node -is [System.Management.Automation.Language.TrapStatementAst] -or
        $node -is [System.Management.Automation.Language.CatchClauseAst]
    }, $true)

    $complexity += $decisionPoints.Count

    # Count logical operators (AND/OR add paths)
    $binaryExpressions = $FunctionAst.Body.FindAll({
        param($node)
        $node -is [System.Management.Automation.Language.BinaryExpressionAst] -and
        $node.Operator -in @([System.Management.Automation.Language.TokenKind]::And,
                             [System.Management.Automation.Language.TokenKind]::Or)
    }, $true)

    $complexity += $binaryExpressions.Count

    return $complexity
}

function Get-HalsteadMetrics {
    <#
    .SYNOPSIS
        Calculate Halstead complexity metrics
    #>
    param([string]$Content)

    $operators = @{}
    $operands = @{}

    # Extract tokens
    $tokens = $null
    $errors = $null
    [void][System.Management.Automation.Language.Parser]::ParseInput($Content, [ref]$tokens, [ref]$errors)

    foreach ($token in $tokens) {
        switch ($token.Kind) {
            { $_ -in @([System.Management.Automation.Language.TokenKind]::Plus,
                       [System.Management.Automation.Language.TokenKind]::Minus,
                       [System.Management.Automation.Language.TokenKind]::Multiply,
                       [System.Management.Automation.Language.TokenKind]::Divide,
                       [System.Management.Automation.Language.TokenKind]::Equals,
                       [System.Management.Automation.Language.TokenKind]::Pipe) } {
                $operators[$token.Text] = ($operators[$token.Text] ?? 0) + 1
            }
            { $_ -in @([System.Management.Automation.Language.TokenKind]::Variable,
                       [System.Management.Automation.Language.TokenKind]::Identifier,
                       [System.Management.Automation.Language.TokenKind]::Number,
                       [System.Management.Automation.Language.TokenKind]::StringExpandable,
                       [System.Management.Automation.Language.TokenKind]::StringLiteral) } {
                $operands[$token.Text] = ($operands[$token.Text] ?? 0) + 1
            }
        }
    }

    $n1 = $operators.Keys.Count      # Distinct operators
    $n2 = $operands.Keys.Count       # Distinct operands
    $N1 = ($operators.Values | Measure-Object -Sum).Sum  # Total operators
    $N2 = ($operands.Values | Measure-Object -Sum).Sum   # Total operands

    $vocabulary = $n1 + $n2
    $length = $N1 + $N2
    $volume = if ($vocabulary -gt 0) { $length * [Math]::Log2($vocabulary) } else { 0 }
    $difficulty = if ($n2 -gt 0) { ($n1 / 2) * ($N2 / $n2) } else { 0 }
    $effort = $difficulty * $volume

    return @{
        Vocabulary = $vocabulary
        Length = $length
        Volume = [Math]::Round($volume, 2)
        Difficulty = [Math]::Round($difficulty, 2)
        Effort = [Math]::Round($effort, 2)
        EstimatedBugs = [Math]::Round($volume / 3000, 3)
    }
}

function Find-DuplicateCodeBlocks {
    <#
    .SYNOPSIS
        Detect duplicate or similar code blocks across files
    #>
    param(
        [hashtable]$FileContents,
        [int]$MinLines = 5
    )

    $duplicates = @()
    $codeBlocks = @{}

    foreach ($file in $FileContents.Keys) {
        $lines = $FileContents[$file] -split "`n"
        for ($i = 0; $i -lt ($lines.Count - $MinLines); $i++) {
            $block = ($lines[$i..($i + $MinLines - 1)] | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }) -join '|'
            if ($block.Length -gt 50) {  # Only consider substantial blocks
                $hash = [System.Security.Cryptography.SHA256]::Create().ComputeHash([System.Text.Encoding]::UTF8.GetBytes($block))
                $hashString = [BitConverter]::ToString($hash) -replace '-', ''

                if (-not $codeBlocks.ContainsKey($hashString)) {
                    $codeBlocks[$hashString] = @()
                }
                $codeBlocks[$hashString] += @{
                    File = $file
                    StartLine = $i + 1
                    EndLine = $i + $MinLines
                    Preview = $lines[$i].Trim().Substring(0, [Math]::Min(60, $lines[$i].Trim().Length))
                }
            }
        }
    }

    # Find actual duplicates (blocks appearing more than once)
    foreach ($hash in $codeBlocks.Keys) {
        if ($codeBlocks[$hash].Count -gt 1) {
            $duplicates += @{
                BlockHash = $hash.Substring(0, 16)
                Occurrences = $codeBlocks[$hash].Count
                Locations = $codeBlocks[$hash]
            }
        }
    }

    return $duplicates
}

function Invoke-AutoRefactorSuggestor {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [ValidateScript({Test-Path $_ -PathType 'Container'})]
        [string]$SourceDir = "D:/lazy init ide",

        [ValidateSet('Critical','High','Medium','Low')]
        [string]$MinSeverity = 'Low',

        [switch]$EnableAutoFix,

        [int]$MaxCyclomaticComplexity = 10,

        [int]$MaxFunctionLines = 50,

        [int]$MaxParameters = 7,

        [string]$OutputPath = "D:/lazy init ide/auto_generated_methods/RefactorSuggestions.json"
    )

    $functionName = 'Invoke-AutoRefactorSuggestor'
    $startTime = Get-Date

    try {
        Write-StructuredLog -Message "Starting comprehensive code analysis for $SourceDir" -Level Info

        $severityRank = @{ 'Critical' = 4; 'High' = 3; 'Medium' = 2; 'Low' = 1 }
        $minRank = $severityRank[$MinSeverity]

        $psFiles = Get-ChildItem -Path $SourceDir -Recurse -Include '*.ps1','*.psm1' -ErrorAction Stop
        Write-StructuredLog -Message "Analyzing $($psFiles.Count) PowerShell files" -Level Info

        $suggestions = @()
        $fileMetrics = @{}
        $fileContents = @{}
        $autoFixes = @()

        foreach ($file in $psFiles) {
            Write-StructuredLog -Message "Analyzing: $($file.Name)" -Level Debug

            try {
                $content = Get-Content $file.FullName -Raw -ErrorAction Stop
                $lines = Get-Content $file.FullName -ErrorAction Stop
                $fileContents[$file.FullName] = $content

                # Parse AST
                $tokens = $null
                $errors = $null
                $ast = [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$tokens, [ref]$errors)

                $fileSuggestions = @()
                $metrics = @{
                    FileName = $file.Name
                    FullPath = $file.FullName
                    LineCount = $lines.Count
                    Functions = @()
                    OverallComplexity = 0
                    Halstead = $null
                }

                # Analyze functions
                $functions = $ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $true)

                foreach ($func in $functions) {
                    $funcLines = ($func.Extent.EndLineNumber - $func.Extent.StartLineNumber) + 1
                    $complexity = Get-CyclomaticComplexity -FunctionAst $func
                    $paramCount = if ($func.Parameters) { $func.Parameters.Count } else { 0 }
                    if ($func.Body.ParamBlock) { $paramCount = $func.Body.ParamBlock.Parameters.Count }

                    $metrics.Functions += @{
                        Name = $func.Name
                        StartLine = $func.Extent.StartLineNumber
                        EndLine = $func.Extent.EndLineNumber
                        Lines = $funcLines
                        CyclomaticComplexity = $complexity
                        ParameterCount = $paramCount
                    }

                    # Check cyclomatic complexity
                    if ($complexity -gt $MaxCyclomaticComplexity) {
                        $fileSuggestions += [PSCustomObject]@{
                            File = $file.Name
                            FullPath = $file.FullName
                            Line = $func.Extent.StartLineNumber
                            Function = $func.Name
                            Issue = 'High Cyclomatic Complexity'
                            Severity = if ($complexity -gt ($MaxCyclomaticComplexity * 2)) { 'Critical' } else { 'High' }
                            Category = 'Complexity'
                            Description = "Function has cyclomatic complexity of $complexity (threshold: $MaxCyclomaticComplexity)."
                            Suggestion = 'Break down into smaller functions, reduce conditional logic, or use polymorphism.'
                            Metric = $complexity
                            AutoFixable = $false
                        }
                    }

                    # Check function size
                    if ($funcLines -gt $MaxFunctionLines) {
                        $fileSuggestions += [PSCustomObject]@{
                            File = $file.Name
                            FullPath = $file.FullName
                            Line = $func.Extent.StartLineNumber
                            Function = $func.Name
                            Issue = 'Large Function'
                            Severity = 'Medium'
                            Category = 'Maintainability'
                            Description = "Function has $funcLines lines (threshold: $MaxFunctionLines)."
                            Suggestion = 'Extract logical blocks into helper functions with single responsibilities.'
                            Metric = $funcLines
                            AutoFixable = $false
                        }
                    }

                    # Check parameter count
                    if ($paramCount -gt $MaxParameters) {
                        $fileSuggestions += [PSCustomObject]@{
                            File = $file.Name
                            FullPath = $file.FullName
                            Line = $func.Extent.StartLineNumber
                            Function = $func.Name
                            Issue = 'Too Many Parameters'
                            Severity = 'Medium'
                            Category = 'Design'
                            Description = "Function has $paramCount parameters (threshold: $MaxParameters)."
                            Suggestion = 'Use parameter objects, configuration objects, or split into multiple functions.'
                            Metric = $paramCount
                            AutoFixable = $false
                        }
                    }
                }

                $metrics.OverallComplexity = ($metrics.Functions | ForEach-Object { $_.CyclomaticComplexity } | Measure-Object -Sum).Sum
                $metrics.Halstead = Get-HalsteadMetrics -Content $content

                # Pattern-based code smell detection
                foreach ($smell in ($script:CodeSmellPatterns | Where-Object { $_.Type -ne 'AST' -and $_.Type -ne 'Analysis' })) {
                    if ($severityRank[$smell.Severity] -lt $minRank) { continue }

                    $matches = [regex]::Matches($content, $smell.Pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)

                    foreach ($match in $matches) {
                        $lineNumber = ($content.Substring(0, $match.Index) -split "`n").Count

                        $suggestion = [PSCustomObject]@{
                            File = $file.Name
                            FullPath = $file.FullName
                            Line = $lineNumber
                            Issue = $smell.Name
                            Severity = $smell.Severity
                            Category = $smell.Category
                            Description = $smell.Description
                            Suggestion = $smell.Suggestion
                            MatchedText = $match.Value.Substring(0, [Math]::Min($match.Value.Length, 80))
                            AutoFixable = $smell.AutoFixable
                        }

                        $fileSuggestions += $suggestion

                        # Generate auto-fix if applicable
                        if ($EnableAutoFix -and $smell.AutoFixable -and $smell.FixPattern) {
                            $autoFixes += @{
                                File = $file.FullName
                                Line = $lineNumber
                                Original = $match.Value
                                Fixed = $match.Value -replace $smell.FixPattern, $smell.FixReplacement
                                Issue = $smell.Name
                            }
                        }
                    }
                }

                $suggestions += $fileSuggestions
                $fileMetrics[$file.FullName] = $metrics

            } catch {
                Write-StructuredLog -Message "Error analyzing $($file.Name): $_" -Level Warning
            }
        }

        # Find duplicate code blocks
        Write-StructuredLog -Message "Checking for duplicate code blocks..." -Level Info
        $duplicates = Find-DuplicateCodeBlocks -FileContents $fileContents -MinLines 5

        foreach ($dup in $duplicates) {
            foreach ($loc in $dup.Locations) {
                $suggestions += [PSCustomObject]@{
                    File = [System.IO.Path]::GetFileName($loc.File)
                    FullPath = $loc.File
                    Line = $loc.StartLine
                    Issue = 'Duplicate Code Block'
                    Severity = 'High'
                    Category = 'DRY Violation'
                    Description = "Duplicate code block found in $($dup.Occurrences) locations."
                    Suggestion = 'Extract common code to a reusable function.'
                    MatchedText = $loc.Preview
                    AutoFixable = $false
                    RelatedLocations = $dup.Locations | Where-Object { $_.File -ne $loc.File -or $_.StartLine -ne $loc.StartLine }
                }
            }
        }

        # Calculate summary metrics
        $severityCounts = @{ 'Critical' = 0; 'High' = 0; 'Medium' = 0; 'Low' = 0 }
        foreach ($s in $suggestions) { $severityCounts[$s.Severity]++ }

        $categoryCounts = @{}
        foreach ($s in $suggestions) {
            if (-not $categoryCounts.ContainsKey($s.Category)) { $categoryCounts[$s.Category] = 0 }
            $categoryCounts[$s.Category]++
        }

        $techDebtScore = ($severityCounts['Critical'] * 8) + ($severityCounts['High'] * 4) + ($severityCounts['Medium'] * 2) + $severityCounts['Low']
        $avgComplexity = ($fileMetrics.Values.OverallComplexity | Measure-Object -Average).Average

        # Build comprehensive report
        $report = [PSCustomObject]@{
            Timestamp = (Get-Date).ToString('o')
            AnalysisDuration = ((Get-Date) - $startTime).TotalSeconds
            SourceDirectory = $SourceDir
            Configuration = @{
                MinSeverity = $MinSeverity
                MaxCyclomaticComplexity = $MaxCyclomaticComplexity
                MaxFunctionLines = $MaxFunctionLines
                MaxParameters = $MaxParameters
                AutoFixEnabled = $EnableAutoFix.IsPresent
            }
            Summary = @{
                TotalFilesAnalyzed = $psFiles.Count
                TotalSuggestions = $suggestions.Count
                TechnicalDebtScore = $techDebtScore
                AverageComplexity = [Math]::Round($avgComplexity, 2)
                TotalFunctions = ($fileMetrics.Values.Functions | Measure-Object).Count
                AutoFixesAvailable = $autoFixes.Count
            }
            SeverityBreakdown = $severityCounts
            CategoryBreakdown = $categoryCounts
            TopIssueFiles = $fileMetrics.Values | Sort-Object { $_.OverallComplexity } -Descending | Select-Object -First 10 | ForEach-Object {
                @{ FileName = $_.FileName; Complexity = $_.OverallComplexity; Issues = ($suggestions | Where-Object { $_.FullPath -eq $_.FullPath }).Count }
            }
            FileMetrics = $fileMetrics
            Suggestions = $suggestions
            AutoFixes = if ($EnableAutoFix) { $autoFixes } else { $null }
            DuplicateBlocks = $duplicates
        }

        # Ensure output directory exists
        $outputDir = Split-Path $OutputPath -Parent
        if (-not (Test-Path $outputDir)) {
            New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
        }

        $report | ConvertTo-Json -Depth 15 | Set-Content $OutputPath -Encoding UTF8
        Write-StructuredLog -Message "Analysis report saved to $OutputPath" -Level Info

        # Output summary
        $duration = ((Get-Date) - $startTime).TotalSeconds
        Write-StructuredLog -Message "Code analysis complete in $([Math]::Round($duration, 2))s" -Level Info
        Write-StructuredLog -Message "Technical Debt Score: $techDebtScore | Issues: Critical=$($severityCounts['Critical']), High=$($severityCounts['High']), Medium=$($severityCounts['Medium']), Low=$($severityCounts['Low'])" -Level $(if ($techDebtScore -gt 50) { 'Warning' } else { 'Info' })

        return $report

    } catch {
        Write-StructuredLog -Message "AutoRefactorSuggestor error: $_" -Level Error
        throw
    }
}

