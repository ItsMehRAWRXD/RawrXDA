# ============================================================================
# RawrXD TODO Resolver - PowerShell Bridge to Native Pattern Engine
# ============================================================================

#Requires -Version 7.0

param(
    [Parameter(Mandatory = $false)]
    [string]$SourcePath = "D:\lazy init ide\src",
    
    [Parameter(Mandatory = $false)]
    [string[]]$Extensions = @("*.ps1", "*.psm1", "*.cpp", "*.h", "*.asm", "*.cs", "*.py", "*.js", "*.ts"),
    
    [Parameter(Mandatory = $false)]
    [switch]$GenerateReport,
    
    [Parameter(Mandatory = $false)]
    [string]$ReportPath = "D:\lazy init ide\reports",
    
    [Parameter(Mandatory = $false)]
    [switch]$VerboseOutput,

    [Parameter(Mandatory = $false)]
    [switch]$CreateTodos,

    [Parameter(Mandatory = $false)]
    [string]$TodoStoragePath = "D:\lazy init ide\data\todos.json"
)

$ErrorActionPreference = 'Stop'

# ============================================================================
# Modules and Routing
# ============================================================================

$patternModulePath = "C:\\Users\\HiH8e\\Documents\\PowerShell\\Modules\\RawrXD_PatternBridge\\RawrXD_PatternBridge.psm1"
if (-not (Test-Path $patternModulePath)) {
    throw "Pattern bridge module not found at $patternModulePath"
}
Import-Module $patternModulePath -Force

$todoModulePath = Join-Path $PSScriptRoot "TodoManager.psm1"
$TodoList = $null
if ($CreateTodos) {
    if (-not (Test-Path $todoModulePath)) {
        throw "TodoManager.psm1 not found at $todoModulePath"
    }
    Import-Module $todoModulePath -Force
    $TodoList = New-TodoList -StoragePath $TodoStoragePath
}

# Map pattern engine types to actionable TODO categories
$ClassificationRouting = @{
    Template = @{ Name = "TODO"; Priority = 1; Color = "Yellow"; Action = "Schedule"; CreateTodo = $true; TodoPriority = "High" }
    Learned  = @{ Name = "TODO"; Priority = 1; Color = "Yellow"; Action = "Schedule"; CreateTodo = $true; TodoPriority = "Medium" }
    NonPattern = @{ Name = "IGNORE"; Priority = 0; Color = "Gray"; Action = "Ignore"; CreateTodo = $false; TodoPriority = "Low" }
    Unknown = @{ Name = "IGNORE"; Priority = 0; Color = "Gray"; Action = "Ignore"; CreateTodo = $false; TodoPriority = "Low" }
}

# ============================================================================
# Helper Functions
# ============================================================================

function Get-LineNumber {
    param([string]$Content, [int]$Position)
    
    $lines = 1
    for ($i = 0; $i -lt $Position -and $i -lt $Content.Length; $i++) {
        if ($Content[$i] -eq "`n") {
            $lines++
        }
    }
    return $lines
}

function Invoke-PatternClassification {
    param(
        [Parameter(Mandatory)]
        [string]$Text,
        
        [Parameter(Mandatory = $false)]
        [string]$Context = ""
    )
    
    $result = Invoke-RawrXDClassification -Code $Text -Context $Context
    $route = $ClassificationRouting[$result.TypeName]
    if (-not $route) {
        return $null
    }

    return @{
        Type = $result.Type
        TypeName = $route.Name
        Priority = $route.Priority
        Confidence = [math]::Round($result.Confidence, 2)
        Action = $route.Action
        CreateTodo = $route.CreateTodo
        TodoPriority = $route.TodoPriority
    }
}

function Scan-FileForPatterns {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath
    )
    
    $content = Get-Content -Path $FilePath -Raw -ErrorAction SilentlyContinue
    if (-not $content) {
        return @()
    }
    
    $patterns = @()
    $lines = $content -split "`n"
    
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        
        # Quick regex pre-filter to avoid calling classifier on every line
        if ($line -match '\\b(TODO|FIXME|XXX|HACK|BUG|NOTE|IDEA|REVIEW|implement|fix|add)\\b') {
            $result = Invoke-PatternClassification -Text $line -Context ([System.IO.Path]::GetExtension($FilePath))
            
            if ($result -and $result.TypeName -ne "IGNORE") {
                $patterns += [PSCustomObject]@{
                    File = $FilePath
                    Line = $i + 1
                    Type = $result.TypeName
                    Priority = $result.Priority
                    Confidence = $result.Confidence
                    Action = $result.Action
                    Content = $line.Trim()
                    CreateTodo = $result.CreateTodo
                    TodoPriority = $result.TodoPriority
                }
            }
        }
    }
    
    return $patterns
}

# ============================================================================
# Main Execution
# ============================================================================

Write-Host ""
Write-Host "=== RawrXD TODO Resolver ===" -ForegroundColor Cyan
Write-Host "Source: $SourcePath" -ForegroundColor Gray
Write-Host "Extensions: $($Extensions -join ', ')" -ForegroundColor Gray
Write-Host ""

# Initialize native engine
# Pattern engine ready via module import
Write-Host "[Init] Pattern engine (PowerShell backend) ready" -ForegroundColor Green

# Collect files
$files = @()
foreach ($ext in $Extensions) {
    $files += Get-ChildItem -Path $SourcePath -Filter $ext -Recurse -File -ErrorAction SilentlyContinue
}

Write-Host "[Scan] Found $($files.Count) files to scan" -ForegroundColor Yellow
Write-Host ""

# Scan all files
$allPatterns = @()
$fileCount = 0

foreach ($file in $files) {
    $fileCount++
    
    if ($VerboseOutput) {
        Write-Progress -Activity "Scanning files" -Status "$fileCount / $($files.Count)" -PercentComplete (($fileCount / $files.Count) * 100)
    }
    
    $patterns = Scan-FileForPatterns -FilePath $file.FullName
    $allPatterns += $patterns
    
    if ($patterns.Count -gt 0 -and $VerboseOutput) {
        Write-Host "[+] $($file.Name): $($patterns.Count) pattern(s)" -ForegroundColor Green
    }
}

if ($VerboseOutput) {
    Write-Progress -Activity "Scanning files" -Completed
}

# Display results
Write-Host "=== Scan Results ===" -ForegroundColor Cyan
Write-Host ""

if ($allPatterns.Count -eq 0) {
    Write-Host "No patterns found!" -ForegroundColor Green
}
else {
    # Group by priority
    $critical = $allPatterns | Where-Object { $_.Priority -eq 3 }
    $high = $allPatterns | Where-Object { $_.Priority -eq 2 }
    $medium = $allPatterns | Where-Object { $_.Priority -eq 1 }
    $low = $allPatterns | Where-Object { $_.Priority -eq 0 }
    
    # Display critical first
    if ($critical.Count -gt 0) {
        Write-Host "CRITICAL ($($critical.Count)):" -ForegroundColor Red
        foreach ($p in $critical) {
            $relPath = $p.File -replace [regex]::Escape($SourcePath), ""
            Write-Host "  [$($p.Type)] $relPath`:$($p.Line)" -ForegroundColor Red
            Write-Host "    $($p.Content)" -ForegroundColor Gray
            Write-Host "    Action: $($p.Action) | Confidence: $($p.Confidence)" -ForegroundColor DarkGray
        }
        Write-Host ""
    }
    
    # High priority
    if ($high.Count -gt 0) {
        Write-Host "HIGH ($($high.Count)):" -ForegroundColor DarkYellow
        foreach ($p in $high | Select-Object -First 5) {
            $relPath = $p.File -replace [regex]::Escape($SourcePath), ""
            Write-Host "  [$($p.Type)] $relPath`:$($p.Line)" -ForegroundColor DarkYellow
        }
        if ($high.Count -gt 5) {
            Write-Host "  ... and $($high.Count - 5) more" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    # Medium priority
    if ($medium.Count -gt 0) {
        Write-Host "MEDIUM ($($medium.Count)):" -ForegroundColor Yellow
        foreach ($p in $medium | Select-Object -First 3) {
            $relPath = $p.File -replace [regex]::Escape($SourcePath), ""
            Write-Host "  [$($p.Type)] $relPath`:$($p.Line)" -ForegroundColor Yellow
        }
        if ($medium.Count -gt 3) {
            Write-Host "  ... and $($medium.Count - 3) more" -ForegroundColor Gray
        }
        Write-Host ""
    }
    
    # Summary
    Write-Host "=== Summary ===" -ForegroundColor Cyan
    Write-Host "Total patterns: $($allPatterns.Count)" -ForegroundColor White
    
    $groupedByType = $allPatterns | Group-Object -Property Type
    foreach ($group in $groupedByType) {
        $color = switch ($group.Name) {
            "BUG" { "Red" }
            "FIXME" { "DarkYellow" }
            "TODO" { "Yellow" }
            default { "Gray" }
        }
        Write-Host "  $($group.Name): $($group.Count)" -ForegroundColor $color
    }
}

# Create todos when requested
if ($CreateTodos -and $TodoList -and $allPatterns.Count -gt 0) {
    Write-Host "`n[TODO] Creating todo items..." -ForegroundColor Yellow
    foreach ($p in $allPatterns) {
        if (-not $p.CreateTodo) { continue }
        if (-not (Test-CanAddTodo -TodoList $TodoList)) { break }

        $text = "[$($p.Type)] $($p.Content) ($([System.IO.Path]::GetFileName($p.File)):$($p.Line))"
        $priority = switch ($p.TodoPriority) {
            "High" { "High" }
            "Medium" { "Medium" }
            default { "Low" }
        }

        Add-Todo -TodoList $TodoList -Text $text -Priority $priority -Category "pattern" | Out-Null
    }
    Write-Host "[TODO] Todo list updated" -ForegroundColor Green
}

# Generate report
if ($GenerateReport) {
    Write-Host ""
    Write-Host "[Report] Generating JSON report..." -ForegroundColor Yellow
    
    if (-not (Test-Path $ReportPath)) {
        New-Item -ItemType Directory -Path $ReportPath -Force | Out-Null
    }
    
    $reportFile = Join-Path $ReportPath "todo-scan-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
    
    $report = @{
        Timestamp = (Get-Date).ToString("o")
        SourcePath = $SourcePath
        TotalFiles = $files.Count
        TotalPatterns = $allPatterns.Count
        Patterns = $allPatterns | ForEach-Object {
            @{
                File = $_.File
                Line = $_.Line
                Type = $_.Type
                Priority = $_.Priority
                Confidence = $_.Confidence
                Action = $_.Action
                Content = $_.Content
            }
        }
        Statistics = @{
            Critical = $critical.Count
            High = $high.Count
            Medium = $medium.Count
            Low = $low.Count
        }
    }
    
    $report | ConvertTo-Json -Depth 10 | Out-File -FilePath $reportFile -Encoding UTF8
    Write-Host "[Report] Saved: $reportFile" -ForegroundColor Green
}

Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Green
