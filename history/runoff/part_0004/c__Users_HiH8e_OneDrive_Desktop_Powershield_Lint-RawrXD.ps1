<#
.SYNOPSIS
    PowerShell Linter & Error Analyzer for RawrXD.ps1

.DESCRIPTION
    Performs comprehensive syntax checking, undefined reference detection,
    and runtime error analysis to identify all issues preventing GUI launch.
#>

param(
    [Parameter(Mandatory = $false)]
    [string]$ScriptPath = "$PSScriptRoot\RawrXD.ps1",
    
    [Parameter(Mandatory = $false)]
    [switch]$Verbose,
    
    [Parameter(Mandatory = $false)]
    [switch]$FixCommon
)

$ErrorActionPreference = 'Continue'
$script:ErrorCount = 0
$script:WarningCount = 0
$script:FixCount = 0

function Write-LintError {
    param([string]$Message, [int]$Line = 0, [string]$Code = "")
    $script:ErrorCount++
    $lineInfo = if ($Line -gt 0) { " [Line $Line]" } else { "" }
    Write-Host "❌ ERROR$lineInfo`: $Message" -ForegroundColor Red
    if ($Code) {
        Write-Host "   Code: $Code" -ForegroundColor DarkGray
    }
}

function Write-LintWarning {
    param([string]$Message, [int]$Line = 0)
    $script:WarningCount++
    $lineInfo = if ($Line -gt 0) { " [Line $Line]" } else { "" }
    Write-Host "⚠️  WARNING$lineInfo`: $Message" -ForegroundColor Yellow
}

function Write-LintInfo {
    param([string]$Message)
    Write-Host "ℹ️  INFO: $Message" -ForegroundColor Cyan
}

function Write-LintSuccess {
    param([string]$Message)
    Write-Host "✅ $Message" -ForegroundColor Green
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  PowerShell Linter for RawrXD.ps1" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Check if file exists
if (-not (Test-Path $ScriptPath)) {
    Write-LintError "Script file not found: $ScriptPath"
    exit 1
}

Write-LintInfo "Analyzing: $ScriptPath"
Write-LintInfo "File size: $([math]::Round((Get-Item $ScriptPath).Length / 1KB, 2)) KB"

# Read the script content
$scriptContent = Get-Content $ScriptPath -Raw
$scriptLines = Get-Content $ScriptPath

Write-Host "`n--- PHASE 1: Syntax Analysis ---`n" -ForegroundColor Magenta

# Test 1: PowerShell Syntax Check
Write-LintInfo "Running PowerShell AST parser..."
try {
    $errors = $null
    $tokens = $null
    $ast = [System.Management.Automation.Language.Parser]::ParseInput($scriptContent, [ref]$tokens, [ref]$errors)
    
    if ($errors.Count -gt 0) {
        Write-LintError "Found $($errors.Count) syntax errors:"
        foreach ($err in $errors) {
            Write-LintError $err.Message $err.Extent.StartLineNumber $err.Extent.Text
        }
    } else {
        Write-LintSuccess "No syntax errors found in AST parse"
    }
} catch {
    Write-LintError "AST parsing failed: $($_.Exception.Message)"
}

Write-Host "`n--- PHASE 2: Function & Variable Analysis ---`n" -ForegroundColor Magenta

# Test 2: Find all function definitions
Write-LintInfo "Extracting function definitions..."
$functionPattern = '(?m)^\s*function\s+([A-Za-z0-9_-]+)'
$functionMatches = [regex]::Matches($scriptContent, $functionPattern)
$definedFunctions = @{}
foreach ($match in $functionMatches) {
    $funcName = $match.Groups[1].Value
    $lineNumber = ($scriptContent.Substring(0, $match.Index) -split "`n").Count
    $definedFunctions[$funcName] = $lineNumber
}
Write-LintSuccess "Found $($definedFunctions.Count) function definitions"

# Test 3: Find all function calls
Write-LintInfo "Extracting function calls..."
$callPattern = '(?m)(?:^|\s)([A-Za-z][A-Za-z0-9_-]*)\s*(?:-|\(|\s+\$)'
$callMatches = [regex]::Matches($scriptContent, $callPattern)
$functionCalls = @{}
foreach ($match in $callMatches) {
    $funcName = $match.Groups[1].Value
    # Exclude keywords and common cmdlets
    if ($funcName -match '^(if|else|elseif|foreach|while|for|switch|try|catch|finally|function|param|return|break|continue|throw|exit|New-Object|Add-Type|Import-Module|Get-|Set-|Write-|Out-|ConvertTo-|ConvertFrom-|Test-|Join-|Split-)') {
        continue
    }
    if (-not $functionCalls.ContainsKey($funcName)) {
        $functionCalls[$funcName] = @()
    }
    $lineNumber = ($scriptContent.Substring(0, $match.Index) -split "`n").Count
    $functionCalls[$funcName] += $lineNumber
}

# Test 4: Check for undefined functions
Write-LintInfo "Checking for undefined function calls..."
$undefinedFunctions = @{}
foreach ($call in $functionCalls.Keys) {
    if (-not $definedFunctions.ContainsKey($call)) {
        # Check if it's a built-in cmdlet
        if (-not (Get-Command $call -ErrorAction SilentlyContinue)) {
            $undefinedFunctions[$call] = $functionCalls[$call]
        }
    }
}

if ($undefinedFunctions.Count -gt 0) {
    Write-LintWarning "Found $($undefinedFunctions.Count) potentially undefined functions:"
    foreach ($func in ($undefinedFunctions.Keys | Sort-Object)) {
        $lines = $undefinedFunctions[$func] -join ", "
        Write-Host "   • $func (called at lines: $lines)" -ForegroundColor Yellow
    }
} else {
    Write-LintSuccess "All function calls appear to be defined"
}

Write-Host "`n--- PHASE 3: Common Error Patterns ---`n" -ForegroundColor Magenta

# Test 5: Check for mismatched braces/brackets
Write-LintInfo "Checking brace/bracket balance..."
$openBraces = ([regex]::Matches($scriptContent, '\{')).Count
$closeBraces = ([regex]::Matches($scriptContent, '\}')).Count
$openParens = ([regex]::Matches($scriptContent, '\(')).Count
$closeParens = ([regex]::Matches($scriptContent, '\)')).Count
$openBrackets = ([regex]::Matches($scriptContent, '\[')).Count
$closeBrackets = ([regex]::Matches($scriptContent, '\]')).Count

if ($openBraces -ne $closeBraces) {
    Write-LintError "Mismatched braces: $openBraces opening vs $closeBraces closing"
}
if ($openParens -ne $closeParens) {
    Write-LintError "Mismatched parentheses: $openParens opening vs $closeParens closing"
}
if ($openBrackets -ne $closeBrackets) {
    Write-LintError "Mismatched brackets: $openBrackets opening vs $closeBrackets closing"
}
if ($openBraces -eq $closeBraces -and $openParens -eq $closeParens -and $openBrackets -eq $closeBrackets) {
    Write-LintSuccess "All braces, brackets, and parentheses are balanced"
}

# Test 6: Check for null-coalescing operator (??) - PowerShell 5.1 incompatible
Write-LintInfo "Checking for PowerShell 7+ only features..."
$nullCoalescing = [regex]::Matches($scriptContent, '\?\?')
if ($nullCoalescing.Count -gt 0) {
    Write-LintError "Found $($nullCoalescing.Count) uses of ?? operator (PowerShell 7+ only, incompatible with 5.1)"
    foreach ($match in $nullCoalescing) {
        $lineNumber = ($scriptContent.Substring(0, $match.Index) -split "`n").Count
        $lineContent = $scriptLines[$lineNumber - 1].Trim()
        Write-Host "   Line $lineNumber`: $lineContent" -ForegroundColor DarkGray
    }
}

# Test 7: Check for missing try-catch blocks
Write-LintInfo "Checking try-catch-finally structure..."
$tryBlocks = [regex]::Matches($scriptContent, '(?m)^\s*try\s*\{')
$catchBlocks = [regex]::Matches($scriptContent, '(?m)^\s*catch')
$finallyBlocks = [regex]::Matches($scriptContent, '(?m)^\s*finally')

Write-Host "   Try blocks: $($tryBlocks.Count)" -ForegroundColor Gray
Write-Host "   Catch blocks: $($catchBlocks.Count)" -ForegroundColor Gray
Write-Host "   Finally blocks: $($finallyBlocks.Count)" -ForegroundColor Gray

if ($tryBlocks.Count -gt ($catchBlocks.Count + $finallyBlocks.Count)) {
    Write-LintWarning "Some try blocks may be missing catch or finally blocks"
}

# Test 8: Check for orphaned code blocks
Write-LintInfo "Checking for orphaned code patterns..."

# Check for lines that look like they should be inside a block
$orphanedPatterns = @(
    @{Pattern = '(?m)^\s*else\s*$'; Name = 'orphaned else'},
    @{Pattern = '(?m)^\s*elseif.*\s*$'; Name = 'orphaned elseif'},
    @{Pattern = '(?m)^\s*catch\s*\{?\s*$\s*^\s*[^{]'; Name = 'catch without try'},
    @{Pattern = '(?m)^\s*finally\s*\{?\s*$'; Name = 'orphaned finally'}
)

foreach ($pattern in $orphanedPatterns) {
    $matches = [regex]::Matches($scriptContent, $pattern.Pattern)
    if ($matches.Count -gt 0) {
        Write-LintWarning "Found $($matches.Count) potential $($pattern.Name) statements"
    }
}

# Test 9: Check for $script:variable usage without initialization
Write-LintInfo "Checking script-scope variables..."
$scriptVarPattern = '\$script:([A-Za-z0-9_]+)'
$scriptVarMatches = [regex]::Matches($scriptContent, $scriptVarPattern)
$scriptVars = @{}
foreach ($match in $scriptVarMatches) {
    $varName = $match.Groups[1].Value
    if (-not $scriptVars.ContainsKey($varName)) {
        $scriptVars[$varName] = 0
    }
    $scriptVars[$varName]++
}
Write-Host "   Found $($scriptVars.Count) unique script-scope variables" -ForegroundColor Gray

# Test 10: Check for Windows Forms control creation patterns
Write-LintInfo "Checking Windows Forms controls..."
$controlPattern = 'New-Object\s+System\.Windows\.Forms\.(\w+)'
$controlMatches = [regex]::Matches($scriptContent, $controlPattern)
$controls = @{}
foreach ($match in $controlMatches) {
    $controlType = $match.Groups[1].Value
    if (-not $controls.ContainsKey($controlType)) {
        $controls[$controlType] = 0
    }
    $controls[$controlType]++
}
Write-Host "   Found $($controls.Count) different control types" -ForegroundColor Gray
if ($Verbose) {
    foreach ($ctrl in ($controls.Keys | Sort-Object)) {
        Write-Host "      • $ctrl`: $($controls[$ctrl]) instances" -ForegroundColor DarkGray
    }
}

Write-Host "`n--- PHASE 4: Extension System Analysis ---`n" -ForegroundColor Magenta

# Test 11: Check for editor extension files
Write-LintInfo "Checking editor extension modules..."
$extensionsPath = Join-Path $PSScriptRoot "extensions"
if (Test-Path $extensionsPath) {
    $editorModules = Get-ChildItem -Path $extensionsPath -Filter "*Editor*.psm1" -ErrorAction SilentlyContinue
    if ($editorModules.Count -gt 0) {
        Write-LintSuccess "Found $($editorModules.Count) editor extension modules:"
        foreach ($module in $editorModules) {
            Write-Host "   • $($module.Name)" -ForegroundColor Green
            
            # Check if module has syntax errors
            try {
                $modContent = Get-Content $module.FullName -Raw
                $modErrors = $null
                $modTokens = $null
                $modAst = [System.Management.Automation.Language.Parser]::ParseInput($modContent, [ref]$modTokens, [ref]$modErrors)
                
                if ($modErrors.Count -gt 0) {
                    Write-LintError "Module $($module.Name) has $($modErrors.Count) syntax errors"
                    foreach ($err in $modErrors) {
                        Write-Host "      Line $($err.Extent.StartLineNumber): $($err.Message)" -ForegroundColor Red
                    }
                }
            } catch {
                Write-LintError "Failed to parse $($module.Name): $_"
            }
        }
    } else {
        Write-LintWarning "No editor extension modules found in $extensionsPath"
    }
} else {
    Write-LintWarning "Extensions directory not found: $extensionsPath"
}

# Test 12: Check for New-*Editor function definitions
Write-LintInfo "Checking for editor factory functions..."
$editorFactories = @('New-BasicEditor', 'New-AdvancedEditor', 'New-RichTextEditor')
foreach ($factory in $editorFactories) {
    if ($definedFunctions.ContainsKey($factory)) {
        Write-LintSuccess "$factory is defined at line $($definedFunctions[$factory])"
    } else {
        Write-LintWarning "$factory is NOT defined in main script (should be in extension module)"
    }
}

Write-Host "`n--- PHASE 5: Runtime Simulation ---`n" -ForegroundColor Magenta

# Test 13: Try to actually load and validate the script
Write-LintInfo "Attempting dry-run script load..."
try {
    # Create a new runspace to isolate the test
    $rs = [runspacefactory]::CreateRunspace()
    $rs.Open()
    $ps = [powershell]::Create()
    $ps.Runspace = $rs
    
    # Add the script
    $ps.AddScript($scriptContent) | Out-Null
    
    # Try to parse (but don't run)
    $result = $ps.BeginInvoke()
    Start-Sleep -Milliseconds 100
    
    if ($ps.HadErrors) {
        Write-LintError "Script has runtime errors:"
        foreach ($err in $ps.Streams.Error) {
            Write-Host "   $($err.Exception.Message)" -ForegroundColor Red
        }
    }
    
    $ps.Stop()
    $rs.Close()
    $rs.Dispose()
    
} catch {
    Write-LintWarning "Could not complete runtime simulation: $_"
}

Write-Host "`n--- PHASE 6: Specific Issue Detection ---`n" -ForegroundColor Magenta

# Test 14: Check for the specific issues we know about
Write-LintInfo "Checking for known problematic patterns..."

# Check for HandleCreated event with SelectionColor
$handleCreatedPattern = 'Add_HandleCreated.*SelectionColor'
if ($scriptContent -match $handleCreatedPattern) {
    Write-LintWarning "Found SelectionColor usage in HandleCreated event (can cause RTF errors)"
}

# Check for Rtf property assignment without try-catch
$rtfAssignPattern = '(?m)^\s*\$\w+\.Rtf\s*='
$rtfMatches = [regex]::Matches($scriptContent, $rtfAssignPattern)
foreach ($match in $rtfMatches) {
    $lineNumber = ($scriptContent.Substring(0, $match.Index) -split "`n").Count
    $context = $scriptContent.Substring([Math]::Max(0, $match.Index - 200), 200)
    if ($context -notmatch 'try\s*\{') {
        Write-LintWarning "Rtf assignment at line $lineNumber may not be wrapped in try-catch"
    }
}

# Check for Initialize-DefaultEditor call
if ($scriptContent -match 'Initialize-DefaultEditor') {
    Write-LintSuccess "Initialize-DefaultEditor function is called"
} else {
    Write-LintWarning "Initialize-DefaultEditor may not be called during startup"
}

# Check for editor panel initialization
if ($scriptContent -match '\$script:editorPanel\s*=') {
    Write-LintSuccess "editorPanel is initialized"
} else {
    Write-LintWarning "editorPanel may not be properly initialized"
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  Linting Complete" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

Write-Host "Summary:" -ForegroundColor White
Write-Host "  ❌ Errors: $script:ErrorCount" -ForegroundColor $(if ($script:ErrorCount -gt 0) { 'Red' } else { 'Green' })
Write-Host "  ⚠️  Warnings: $script:WarningCount" -ForegroundColor $(if ($script:WarningCount -gt 0) { 'Yellow' } else { 'Green' })

if ($script:ErrorCount -eq 0 -and $script:WarningCount -eq 0) {
    Write-Host "`n✅ No critical issues found!" -ForegroundColor Green
} else {
    Write-Host "`n⚠️  Please review the issues above" -ForegroundColor Yellow
}

Write-Host ""

# Return exit code based on errors
exit $(if ($script:ErrorCount -gt 0) { 1 } else { 0 })
