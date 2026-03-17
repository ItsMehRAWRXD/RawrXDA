# ============================================================================
# RawrXD IDE Bridge - PowerShell Pattern/TODO Integration + Named Pipe Server
# Default backend: PowerShell/C# (no external dependencies)
# ============================================================================

#Requires -Version 7.0

param(
    [Parameter(Mandatory=$false)]
    [string]$ScanPath = "D:\\lazy init ide\\src",

    [Parameter(Mandatory=$false)]
    [string[]]$Extensions = @("*.ps1", "*.psm1", "*.cpp", "*.h", "*.asm", "*.cs", "*.py", "*.js", "*.ts"),

    [Parameter(Mandatory=$false)]
    [switch]$CreateTickets,

    [Parameter(Mandatory=$false)]
    [switch]$AutoAssign,

    [Parameter(Mandatory=$false)]
    [string]$TodoStoragePath = "D:\\lazy init ide\\data\\todos.json",

    [Parameter(Mandatory=$false)]
    [switch]$StartServer,

    [Parameter(Mandatory=$false)]
    [string]$PipeName = "RawrXD_PatternBridge",

    [Parameter(Mandatory=$false)]
    [int]$PipeTimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'

# ============================================================================
# Module Imports
# ============================================================================

# Pattern engine (PowerShell backend)
$patternModulePath = "C:\\Users\\HiH8e\\Documents\\PowerShell\\Modules\\RawrXD_PatternBridge\\RawrXD_PatternBridge.psm1"
if (-not (Test-Path $patternModulePath)) {
    throw "Pattern bridge module not found at $patternModulePath"
}
Import-Module $patternModulePath -Force

# Todo manager
$todoModulePath = Join-Path $PSScriptRoot "TodoManager.psm1"
if (-not (Test-Path $todoModulePath)) {
    throw "TodoManager.psm1 not found at $todoModulePath"
}
Import-Module $todoModulePath -Force

# ============================================================================
# Classification Mapping
# ============================================================================

$ClassificationRouting = @{
    Template = @{ TodoPriority = "High"; Action = "Schedule"; CreateTicket = $true }
    Learned  = @{ TodoPriority = "Medium"; Action = "Schedule"; CreateTicket = $true }
    NonPattern = @{ TodoPriority = "Low"; Action = "Ignore"; CreateTicket = $false }
    Unknown = @{ TodoPriority = "Low"; Action = "Ignore"; CreateTicket = $false }
}

$TodoList = $null
if ($CreateTickets) {
    $TodoList = New-TodoList -StoragePath $TodoStoragePath
}

# ============================================================================
# Helpers
# ============================================================================

function Invoke-BridgeClassification {
    param(
        [Parameter(Mandatory)] [string]$Text,
        [Parameter(Mandatory)] [string]$Context
    )

    $result = Invoke-RawrXDClassification -Code $Text -Context $Context
    $route = $ClassificationRouting[$result.TypeName]
    if (-not $route) { return $null }

    return [PSCustomObject]@{
        TypeName = $result.TypeName
        Confidence = [math]::Round($result.Confidence, 2)
        TodoPriority = $route.TodoPriority
        Action = $route.Action
        CreateTicket = $route.CreateTicket
    }
}

function Scan-File {
    param(
        [Parameter(Mandatory)] [string]$FilePath
    )

    $content = Get-Content -Path $FilePath -Raw -ErrorAction SilentlyContinue
    if (-not $content) { return @() }

    $lines = $content -split "`n"
    $hits = @()
    $ext = [System.IO.Path]::GetExtension($FilePath)

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line.Length -eq 0) { continue }

        # Fast pre-filter on common todo markers to reduce calls
        if ($line -notmatch '\\b(TODO|FIXME|XXX|HACK|BUG|NOTE|IDEA|REVIEW|implement|fix|add)\\b') {
            continue
        }

        $classification = Invoke-BridgeClassification -Text $line -Context $ext
        if (-not $classification) { continue }
        if (-not $classification.CreateTicket) { continue }

        $hits += [PSCustomObject]@{
            File = $FilePath
            Line = $i + 1
            Type = $classification.TypeName
            Priority = $classification.TodoPriority
            Confidence = $classification.Confidence
            Action = $classification.Action
            Content = $line.Trim()
        }
    }

    return $hits
}

function Scan-Path {
    param(
        [Parameter(Mandatory)] [string]$Path
    )

    $results = @()
    $files = @()
    foreach ($ext in $Extensions) {
        $files += Get-ChildItem -Path $Path -Filter $ext -File -Recurse -ErrorAction SilentlyContinue
    }

    foreach ($file in $files) {
        $results += Scan-File -FilePath $file.FullName
    }

    return $results
}

function Create-TodoTickets {
    param(
        [Parameter(Mandatory)] [PSCustomObject[]]$Items
    )

    if (-not $CreateTickets -or -not $TodoList) { return }

    foreach ($item in $Items) {
        $text = "[$($item.Type)] $($item.Content) ($([System.IO.Path]::GetFileName($item.File)):$($item.Line))"
        $priority = switch ($item.Priority) {
            "High" { "High" }
            "Medium" { "Medium" }
            default { "Low" }
        }

        if (-not (Test-CanAddTodo -TodoList $TodoList)) { break }
        Add-Todo -TodoList $TodoList -Text $text -Priority $priority -Category "pattern" | Out-Null

        if ($AutoAssign) {
            Update-Todo -TodoList $TodoList -Id $TodoList.Items[-1].Id -Updates @{ Status = "pending" }
        }
    }
}

function Start-PipeServer {
    Write-Host "[Pipe] Starting named pipe server: $PipeName" -ForegroundColor Cyan
    function Dispose-Quietly {
        param([IDisposable]$Disposable)
        if (-not $Disposable) { return }
        try { $Disposable.Dispose() } catch { Write-Verbose "[Pipe] Dispose skipped: $_" }
    }

    while ($true) {
        $server = $null
        try {
            $server = [System.IO.Pipes.NamedPipeServerStream]::new($PipeName, [System.IO.Pipes.PipeDirection]::InOut, 1, [System.IO.Pipes.PipeTransmissionMode]::Byte, [System.IO.Pipes.PipeOptions]::Asynchronous)
            $await = $server.WaitForConnectionAsync()
            $completed = $await.Wait([TimeSpan]::FromSeconds($PipeTimeoutSeconds))
            if (-not $completed) { Dispose-Quietly $server; continue }

            $reader = [System.IO.StreamReader]::new($server)
            $writer = [System.IO.StreamWriter]::new($server)
            $writer.AutoFlush = $true

            try {
                $request = $reader.ReadLine()
                if (-not $request) { continue }

                if ($request -like "CLASSIFY|*") {
                    $targetPath = $request.Substring(9)
                    if (Test-Path $targetPath) {
                        $items = Scan-File -FilePath $targetPath
                        $json = $items | ConvertTo-Json -Depth 6
                        $writer.WriteLine($json)
                    } else {
                        $writer.WriteLine('{"error":"File not found"}')
                    }
                }
                elseif ($request -like "SCANPATH|*") {
                    $targetPath = $request.Substring(9)
                    if (Test-Path $targetPath) {
                        $items = Scan-Path -Path $targetPath
                        $json = $items | ConvertTo-Json -Depth 6
                        $writer.WriteLine($json)
                    } else {
                        $writer.WriteLine('{"error":"Path not found"}')
                    }
                }
                else {
                    $writer.WriteLine('{"error":"Unknown command"}')
                }
            }
            catch {
                Write-Warning "[Pipe] Request handling failed: $_"
            }
            finally {
                Dispose-Quietly $writer
                Dispose-Quietly $reader
                Dispose-Quietly $server
            }
        }
        catch {
            Write-Warning "[Pipe] Server loop error: $_"
            Dispose-Quietly $server
            Start-Sleep -Seconds 1
        }
    }
}

# ============================================================================
# Main Flow
# ============================================================================

if ($StartServer) {
    Start-PipeServer
    return
}

Write-Host "=== RawrXD IDE Bridge ===" -ForegroundColor Cyan
Write-Host "Scan Path: $ScanPath" -ForegroundColor Gray
Write-Host "Create Tickets: $CreateTickets" -ForegroundColor Gray
Write-Host "Pipe Name: $PipeName" -ForegroundColor Gray
Write-Host ""

$results = Scan-Path -Path $ScanPath
if ($CreateTickets -and $results.Count -gt 0) {
    Create-TodoTickets -Items $results
}

Write-Host "Found $($results.Count) actionable patterns" -ForegroundColor Yellow
if ($results.Count -gt 0) {
    $results | Select-Object File, Line, Type, Priority, Confidence, Content | Format-Table -AutoSize
}

Write-Host "Done." -ForegroundColor Green
