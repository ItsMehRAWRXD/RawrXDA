<#
.SYNOPSIS
    RawrXD SafeMode CLI - Full IDE functionality via PowerShell
    
.DESCRIPTION
    Provides complete IDE functionality without Qt/GUI dependencies.
    Enables agentic/autonomous Win32 access when GUI or IDE fails.
    
    Features:
    - Poly Model Loading (GGUF, Q4_0, Q8_0, etc.)
    - Agentic Tool Execution (file ops, git, shell)
    - LLM Inference (streaming/sync)
    - Tier hopping with compression
    - Autonomous chat with tool use
    - Telemetry and system monitoring
    - API server mode
    
.NOTES
    Author: RawrXD Team
    Version: 1.0.0
#>

param(
    [Parameter(Position=0)]
    [string]$Command,
    
    [Parameter(Position=1, ValueFromRemainingArguments)]
    [string[]]$Arguments,
    
    [string]$Model,
    [string]$Workspace = ".",
    [int]$Port = 11434,
    [switch]$Api,
    [switch]$Governor,
    [switch]$Verbose,
    [switch]$Unsafe,
    [switch]$Help
)

# ============================================================================
# Configuration
# ============================================================================
$script:Config = @{
    SafeModeBinary = "$PSScriptRoot\..\build\bin-msvc\RawrXD-SafeMode.exe"
    FallbackBinary = "$PSScriptRoot\..\build-debug\RawrXD-SafeMode.exe"
    OllamaEndpoint = "http://localhost:11434"
    ModelsDir = @(
        ".\models",
        "..\models",
        "C:\models",
        "D:\models",
        "$env:USERPROFILE\.ollama\models"
    )
    DefaultModel = "llama3.2"
    MaxTokens = 512
    Temperature = 0.7
}

$script:State = @{
    CurrentModel = $null
    CurrentTier = "auto"
    WorkspaceRoot = (Resolve-Path $Workspace -ErrorAction SilentlyContinue)?.Path ?? $PWD.Path
    ChatHistory = @()
    ToolExecutor = $null
    Verbose = $Verbose.IsPresent
}

# ============================================================================
# ANSI Colors
# ============================================================================
$script:Colors = @{
    Reset   = "`e[0m"
    Red     = "`e[31m"
    Green   = "`e[32m"
    Yellow  = "`e[33m"
    Blue    = "`e[34m"
    Magenta = "`e[35m"
    Cyan    = "`e[36m"
    White   = "`e[37m"
    Bold    = "`e[1m"
    Dim     = "`e[2m"
}

function Write-ColorOutput {
    param([string]$Color, [string]$Message, [switch]$NoNewline)
    $c = $script:Colors[$Color] ?? ""
    if ($NoNewline) {
        Write-Host "$c$Message$($script:Colors.Reset)" -NoNewline
    } else {
        Write-Host "$c$Message$($script:Colors.Reset)"
    }
}

function Write-Success { param([string]$Msg) Write-ColorOutput Green "✓ $Msg" }
function Write-Error2 { param([string]$Msg) Write-ColorOutput Red "✗ $Msg" }
function Write-Info { param([string]$Msg) Write-ColorOutput Cyan "ℹ $Msg" }
function Write-Warn { param([string]$Msg) Write-ColorOutput Yellow "⚠ $Msg" }

# ============================================================================
# Banner and Help
# ============================================================================
function Show-Banner {
    Write-ColorOutput Cyan @"

 ____                     __  ______  
|  _ \ __ ___      ___ __|  \/  |  _ \ 
| |_) / _`` \ \ /\ / / '__| |\/| | | | |
|  _ < (_| |\ V  V /| |  | |  | | |_| |
|_| \_\__,_| \_/\_/ |_|  |_|  |_|____/ 
                                       
"@
    Write-ColorOutput Yellow "SafeMode CLI v1.0.0 - Full IDE via PowerShell"
    Write-Host ""
    Write-ColorOutput Dim "Type 'rawr help' for commands"
    Write-Host ""
}

function Show-Help {
    Write-ColorOutput Cyan "`n=== RawrXD SafeMode CLI Commands ===`n"
    
    Write-ColorOutput Bold "AGENTIC ENGINE & PLANNING:"
    Write-Host "  $(Write-ColorOutput Green 'rawr mission <objective>' -NoNewline)    Start high-level agentic mission (Zero-Day Engine)"
    Write-Host "  $(Write-ColorOutput Green 'rawr plan <objective>' -NoNewline)       Generate execution plan for a task"
    Write-Host "  $(Write-ColorOutput Green 'rawr execute_plan' -NoNewline)           Execute the last generated plan"
    Write-Host "  $(Write-ColorOutput Green 'rawr registry' -NoNewline)               List all 44+ production tools"
    Write-Host "  $(Write-ColorOutput Green 'rawr agent <task>' -NoNewline)           Run agentic task with tool use"
    
    Write-ColorOutput Bold "`nMODEL MANAGEMENT (Ollama-style):"
    Write-Host "  $(Write-ColorOutput Green 'rawr run <model>' -NoNewline)            Load and interact with a model"
    Write-Host "  $(Write-ColorOutput Green 'rawr pull <model>' -NoNewline)           Download model from registry"
    Write-Host "  $(Write-ColorOutput Green 'rawr list' -NoNewline)                   List available models"
    Write-Host "  $(Write-ColorOutput Green 'rawr show <model>' -NoNewline)           Show model information"
    Write-Host "  $(Write-ColorOutput Green 'rawr tier [name]' -NoNewline)            Show/switch compression tier"
    Write-Host "  $(Write-ColorOutput Green 'rawr tiers' -NoNewline)                  List all available tiers"
    
    Write-ColorOutput Bold "`nINFERENCE:"
    Write-Host "  $(Write-ColorOutput Green 'rawr gen <prompt>' -NoNewline)           Generate text (single shot)"
    Write-Host "  $(Write-ColorOutput Green 'rawr chat' -NoNewline)                   Enter interactive chat mode"
    Write-Host "  $(Write-ColorOutput Green 'rawr stream <prompt>' -NoNewline)        Stream generation token-by-token"
    
    Write-ColorOutput Bold "`nHOTPATCH & DIAGNOSTICS:"
    Write-Host "  $(Write-ColorOutput Green 'rawr hotpatch list' -NoNewline)          List available and applied hotpatches"
    Write-Host "  $(Write-ColorOutput Green 'rawr hotpatch apply <id>' -NoNewline)    Apply a hotpatch by ID"
    Write-Host "  $(Write-ColorOutput Green 'rawr hotpatch revert <id>' -NoNewline)   Revert a hotpatch by ID"
    Write-Host "  $(Write-ColorOutput Green 'rawr selftest' -NoNewline)               Run system self-test diagnostics"
    
    Write-ColorOutput Bold "`nAGENTIC TOOLS:"
    Write-Host "  $(Write-ColorOutput Green 'rawr tools' -NoNewline)                  List available agentic tools"
    Write-Host "  $(Write-ColorOutput Green 'rawr tool <name> <params>' -NoNewline)   Execute a specific tool"
    Write-Host "  $(Write-ColorOutput Green 'rawr file read <path>' -NoNewline)       Read file contents"
    Write-Host "  $(Write-ColorOutput Green 'rawr file write <p> <c>' -NoNewline)     Write content to file"
    Write-Host "  $(Write-ColorOutput Green 'rawr file list <dir>' -NoNewline)        List directory contents"
    Write-Host "  $(Write-ColorOutput Green 'rawr git status' -NoNewline)             Git status"
    Write-Host "  $(Write-ColorOutput Green 'rawr git add <files>' -NoNewline)        Git add files"
    Write-Host "  $(Write-ColorOutput Green 'rawr git commit <msg>' -NoNewline)       Git commit"
    Write-Host "  $(Write-ColorOutput Green 'rawr exec <cmd>' -NoNewline)             Execute shell command"
    
    Write-ColorOutput Bold "`nSYSTEM:"
    Write-Host "  $(Write-ColorOutput Green 'rawr status' -NoNewline)                 System status & health"
    Write-Host "  $(Write-ColorOutput Green 'rawr telemetry' -NoNewline)              Show telemetry data"
    Write-Host "  $(Write-ColorOutput Green 'rawr api start [port]' -NoNewline)       Start API server"
    Write-Host "  $(Write-ColorOutput Green 'rawr api stop' -NoNewline)               Stop API server"
    Write-Host "  $(Write-ColorOutput Green 'rawr governor start' -NoNewline)         Start overclock governor"
    Write-Host "  $(Write-ColorOutput Green 'rawr governor stop' -NoNewline)          Stop overclock governor"
    Write-Host "  $(Write-ColorOutput Green 'rawr workspace <path>' -NoNewline)       Set workspace root"
    Write-Host "  $(Write-ColorOutput Green 'rawr settings' -NoNewline)               Show current settings"
    Write-Host "  $(Write-ColorOutput Green 'rawr verbose [on|off]' -NoNewline)       Toggle verbose output"
    Write-Host "  $(Write-ColorOutput Green 'rawr repl' -NoNewline)                   Enter interactive REPL mode"
    Write-Host ""
}

# ============================================================================
# Model Management
# ============================================================================
function Find-Model {
    param([string]$Name)
    
    # Check if it's already a full path
    if (Test-Path $Name) {
        return (Resolve-Path $Name).Path
    }
    
    # Search in common locations
    foreach ($dir in $script:Config.ModelsDir) {
        $candidate = Join-Path $dir $Name
        if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
        
        $candidate = Join-Path $dir "$Name.gguf"
        if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
    }
    
    return $null
}

function Invoke-ModelRun {
    param([string]$ModelName)
    
    if (-not $ModelName) {
        Write-Error2 "Usage: rawr run <model_name_or_path>"
        return
    }
    
    $modelPath = Find-Model $ModelName
    
    if (-not $modelPath) {
        Write-Error2 "Model not found: $ModelName"
        Write-Info "Try: rawr list - to see available models"
        return
    }
    
    Write-Info "Loading model: $modelPath"
    
    # Use native binary if available
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary run $modelPath
    } else {
        # Fallback to Ollama API
        try {
            $response = Invoke-RestMethod -Uri "$($script:Config.OllamaEndpoint)/api/tags" -Method Get -ErrorAction Stop
            
            # Check if model exists in Ollama
            $ollamaModel = $response.models | Where-Object { $_.name -like "*$ModelName*" }
            
            if ($ollamaModel) {
                $script:State.CurrentModel = $ollamaModel.name
                Write-Success "Model loaded: $($ollamaModel.name)"
            } else {
                Write-Warn "Model not in Ollama. Attempting local GGUF load..."
                $script:State.CurrentModel = $ModelName
            }
        } catch {
            Write-Warn "Ollama not available. Running in offline mode."
            $script:State.CurrentModel = $ModelName
        }
    }
}

function Invoke-ModelList {
    Write-ColorOutput Cyan "`n=== Available Models ===`n"
    
    $count = 0
    
    # List local GGUF files
    foreach ($dir in $script:Config.ModelsDir) {
        if (Test-Path $dir) {
            Get-ChildItem $dir -Filter "*.gguf" -ErrorAction SilentlyContinue | ForEach-Object {
                $sizeGB = [math]::Round($_.Length / 1GB, 2)
                Write-Host "  $($script:Colors.Green)$($_.Name)$($script:Colors.Reset) $($script:Colors.Dim)($sizeGB GB)$($script:Colors.Reset)"
                $count++
            }
        }
    }
    
    # Also check Ollama
    try {
        $response = Invoke-RestMethod -Uri "$($script:Config.OllamaEndpoint)/api/tags" -Method Get -ErrorAction Stop -TimeoutSec 2
        
        if ($response.models) {
            Write-Host "`n  $($script:Colors.Bold)Ollama Models:$($script:Colors.Reset)"
            $response.models | ForEach-Object {
                $sizeGB = [math]::Round($_.size / 1GB, 2)
                Write-Host "  $($script:Colors.Green)$($_.name)$($script:Colors.Reset) $($script:Colors.Dim)($sizeGB GB)$($script:Colors.Reset)"
                $count++
            }
        }
    } catch {
        # Ollama not available, ignore
    }
    
    if ($count -eq 0) {
        Write-Warn "No models found. Place .gguf files in ./models/"
    }
    
    Write-Host ""
}

function Invoke-ModelShow {
    param([string]$ModelName)
    
    if (-not $script:State.CurrentModel -and -not $ModelName) {
        Write-Warn "No model loaded. Use 'rawr run <model>' first."
        return
    }
    
    $model = $ModelName ?? $script:State.CurrentModel
    
    Write-ColorOutput Cyan "`n=== Model Information ===`n"
    Write-Host "  Model: $model"
    Write-Host "  Tier: $($script:State.CurrentTier)"
    
    # Try to get more info from Ollama
    try {
        $response = Invoke-RestMethod -Uri "$($script:Config.OllamaEndpoint)/api/show" -Method Post -Body (@{name=$model} | ConvertTo-Json) -ContentType "application/json" -TimeoutSec 5
        
        if ($response.modelfile) {
            Write-Host "`n  $($script:Colors.Bold)Parameters:$($script:Colors.Reset)"
            Write-Host "    $($response.parameters -replace "`n", "`n    ")"
        }
    } catch {
        # Ollama not available
    }
    
    Write-Host ""
}

# ============================================================================
# Inference
# ============================================================================
function Invoke-Generate {
    param([string]$Prompt)
    
    if (-not $Prompt) {
        Write-Error2 "Usage: rawr gen <prompt>"
        return
    }
    
    $model = $script:State.CurrentModel ?? $script:Config.DefaultModel
    
    Write-Info "Generating with $model...`n"
    
    # Use native binary if available
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary generate $Prompt
        return
    }
    
    # Fallback to Ollama API
    try {
        $body = @{
            model = $model
            prompt = $Prompt
            stream = $false
            options = @{
                temperature = $script:Config.Temperature
                num_predict = $script:Config.MaxTokens
            }
        } | ConvertTo-Json -Depth 5
        
        $response = Invoke-RestMethod -Uri "$($script:Config.OllamaEndpoint)/api/generate" -Method Post -Body $body -ContentType "application/json"
        
        Write-Host $response.response
        Write-Host ""
        Write-ColorOutput Dim "[$($response.eval_count) tokens | $([math]::Round($response.eval_count / ($response.eval_duration / 1e9), 1)) tok/s]"
    } catch {
        Write-Error2 "Generation failed: $_"
    }
}

function Invoke-Stream {
    param([string]$Prompt)
    
    if (-not $Prompt) {
        Write-Error2 "Usage: rawr stream <prompt>"
        return
    }
    
    $model = $script:State.CurrentModel ?? $script:Config.DefaultModel
    
    Write-Info "Streaming with $model...`n"
    
    try {
        $body = @{
            model = $model
            prompt = $Prompt
            stream = $true
            options = @{
                temperature = $script:Config.Temperature
                num_predict = $script:Config.MaxTokens
            }
        } | ConvertTo-Json -Depth 5
        
        $request = [System.Net.HttpWebRequest]::Create("$($script:Config.OllamaEndpoint)/api/generate")
        $request.Method = "POST"
        $request.ContentType = "application/json"
        
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($body)
        $request.ContentLength = $bytes.Length
        
        $requestStream = $request.GetRequestStream()
        $requestStream.Write($bytes, 0, $bytes.Length)
        $requestStream.Close()
        
        $response = $request.GetResponse()
        $reader = New-Object System.IO.StreamReader($response.GetResponseStream())
        
        $tokenCount = 0
        while (-not $reader.EndOfStream) {
            $line = $reader.ReadLine()
            if ($line) {
                $json = $line | ConvertFrom-Json
                if ($json.response) {
                    Write-Host $json.response -NoNewline
                    $tokenCount++
                }
            }
        }
        
        $reader.Close()
        $response.Close()
        
        Write-Host "`n"
        Write-ColorOutput Dim "[$tokenCount tokens]"
    } catch {
        Write-Error2 "Streaming failed: $_"
    }
}

function Invoke-Chat {
    Write-ColorOutput Cyan "`n=== Interactive Chat ==="
    Write-ColorOutput Dim "Type 'exit' to leave, '/clear' to reset history`n"
    
    $model = $script:State.CurrentModel ?? $script:Config.DefaultModel
    $history = @()
    
    while ($true) {
        Write-ColorOutput Green "You: " -NoNewline
        $input = Read-Host
        
        if ($input -eq "exit" -or $input -eq "/exit") { break }
        if ($input -eq "/clear") {
            $history = @()
            Write-Info "Chat history cleared"
            continue
        }
        if ([string]::IsNullOrWhiteSpace($input)) { continue }
        
        # Add user message to history
        $history += @{ role = "user"; content = $input }
        
        try {
            $body = @{
                model = $model
                messages = $history
                stream = $false
                options = @{
                    temperature = $script:Config.Temperature
                }
            } | ConvertTo-Json -Depth 5
            
            $response = Invoke-RestMethod -Uri "$($script:Config.OllamaEndpoint)/api/chat" -Method Post -Body $body -ContentType "application/json"
            
            $assistantMsg = $response.message.content
            $history += @{ role = "assistant"; content = $assistantMsg }
            
            Write-ColorOutput Cyan "`nAssistant: " -NoNewline
            Write-Host "$assistantMsg`n"
        } catch {
            Write-Error2 "Chat failed: $_"
        }
    }
    
    Write-Host ""
}

# ============================================================================
# Agentic Tools
# ============================================================================
$script:ToolSchemas = @{
    file_read = @{
        description = "Read text content of a file"
        params = @{ path = "File path to read" }
        required = @("path")
    }
    file_write = @{
        description = "Write text content to a file"
        params = @{ path = "File path"; content = "Text to write" }
        required = @("path", "content")
    }
    file_list = @{
        description = "List directory contents"
        params = @{ path = "Directory path"; recursive = "true/false" }
        required = @("path")
    }
    file_exists = @{
        description = "Check if path exists"
        params = @{ path = "Path to check" }
        required = @("path")
    }
    file_delete = @{
        description = "Delete a file"
        params = @{ path = "File path to remove" }
        required = @("path")
    }
    git_status = @{
        description = "Get git status"
        params = @{ short = "Use short format" }
        required = @()
    }
    git_add = @{
        description = "Stage files for commit"
        params = @{ paths = "Comma-separated file paths" }
        required = @("paths")
    }
    git_commit = @{
        description = "Commit staged changes"
        params = @{ message = "Commit message" }
        required = @("message")
    }
    git_push = @{
        description = "Push current branch"
        params = @{ remote = "Remote name"; branch = "Branch name" }
        required = @()
    }
    git_pull = @{
        description = "Pull current branch"
        params = @{ remote = "Remote name"; branch = "Branch name" }
        required = @()
    }
    git_diff = @{
        description = "Show diff"
        params = @{ spec = "Diff spec" }
        required = @()
    }
    exec = @{
        description = "Execute shell command"
        params = @{ command = "Shell command to run" }
        required = @("command")
    }
}

function Invoke-ToolList {
    Write-ColorOutput Cyan "`n=== Available Agentic Tools ===`n"
    
    foreach ($tool in $script:ToolSchemas.Keys | Sort-Object) {
        $schema = $script:ToolSchemas[$tool]
        Write-Host "  $($script:Colors.Green)$tool$($script:Colors.Reset) - $($schema.description)"
        if ($schema.required.Count -gt 0) {
            Write-ColorOutput Dim "    Required: $($schema.required -join ', ')"
        }
    }
    
    Write-Host ""
}

function Invoke-Tool {
    param(
        [string]$ToolName,
        [hashtable]$Params = @{}
    )
    
    $result = @{ success = $false; data = $null; error = $null }
    
    switch ($ToolName) {
        "file_read" {
            $path = $Params.path
            if (-not $path) { 
                $result.error = "Missing path parameter"
                return $result 
            }
            if (Test-Path $path) {
                $result.success = $true
                $result.data = Get-Content $path -Raw
            } else {
                $result.error = "File not found: $path"
            }
        }
        "file_write" {
            $path = $Params.path
            $content = $Params.content
            if (-not $path -or -not $content) { 
                $result.error = "Missing path or content parameter"
                return $result 
            }
            try {
                $dir = Split-Path $path -Parent
                if ($dir -and -not (Test-Path $dir)) {
                    New-Item -ItemType Directory -Path $dir -Force | Out-Null
                }
                Set-Content -Path $path -Value $content
                $result.success = $true
                $result.data = @{ path = $path; bytes = $content.Length }
            } catch {
                $result.error = $_.Exception.Message
            }
        }
        "file_list" {
            $path = $Params.path ?? "."
            $recursive = $Params.recursive -eq "true"
            if (Test-Path $path) {
                $result.success = $true
                $items = if ($recursive) {
                    Get-ChildItem $path -Recurse | ForEach-Object { $_.FullName }
                } else {
                    Get-ChildItem $path | ForEach-Object { $_.Name }
                }
                $result.data = @{ path = $path; files = $items; count = $items.Count }
            } else {
                $result.error = "Directory not found: $path"
            }
        }
        "file_exists" {
            $path = $Params.path
            $result.success = $true
            $result.data = @{ path = $path; exists = (Test-Path $path) }
        }
        "file_delete" {
            $path = $Params.path
            if (Test-Path $path) {
                Remove-Item $path -Force
                $result.success = $true
                $result.data = @{ path = $path; deleted = $true }
            } else {
                $result.error = "File not found: $path"
            }
        }
        "git_status" {
            try {
                $output = git status $(if ($Params.short -eq "true") { "-s" })
                $result.success = $true
                $result.data = $output
            } catch {
                $result.error = $_.Exception.Message
            }
        }
        "git_add" {
            try {
                $paths = $Params.paths -split ","
                git add $paths
                $result.success = $true
                $result.data = @{ added = $paths }
            } catch {
                $result.error = $_.Exception.Message
            }
        }
        "git_commit" {
            try {
                git commit -m $Params.message
                $result.success = $true
                $result.data = @{ message = $Params.message }
            } catch {
                $result.error = $_.Exception.Message
            }
        }
        "git_push" {
            try {
                $remote = $Params.remote ?? "origin"
                $branch = $Params.branch
                if ($branch) {
                    git push $remote $branch
                } else {
                    git push $remote
                }
                $result.success = $true
            } catch {
                $result.error = $_.Exception.Message
            }
        }
        "git_pull" {
            try {
                $remote = $Params.remote ?? "origin"
                $branch = $Params.branch
                if ($branch) {
                    git pull $remote $branch
                } else {
                    git pull $remote
                }
                $result.success = $true
            } catch {
                $result.error = $_.Exception.Message
            }
        }
        "git_diff" {
            try {
                $output = git diff $Params.spec
                $result.success = $true
                $result.data = $output
            } catch {
                $result.error = $_.Exception.Message
            }
        }
        "exec" {
            try {
                $output = Invoke-Expression $Params.command 2>&1
                $result.success = $true
                $result.data = $output
            } catch {
                $result.error = $_.Exception.Message
            }
        }
        default {
            $result.error = "Unknown tool: $ToolName"
        }
    }
    
    return $result
}

function Invoke-ToolCommand {
    param(
        [string]$ToolName,
        [string[]]$ToolArgs
    )
    
    if (-not $ToolName) {
        Write-Error2 "Usage: rawr tool <name> <param1=value1> <param2=value2>"
        return
    }
    
    # Parse args into hashtable
    $params = @{}
    foreach ($arg in $ToolArgs) {
        if ($arg -match "^(\w+)=(.*)$") {
            $params[$Matches[1]] = $Matches[2]
        } elseif ($arg -match "^\{.*\}$") {
            # JSON format
            try {
                $json = $arg | ConvertFrom-Json -AsHashtable
                $params = $json
            } catch {
                Write-Error2 "Invalid JSON: $arg"
                return
            }
        }
    }
    
    $result = Invoke-Tool -ToolName $ToolName -Params $params
    
    if ($result.success) {
        Write-Success "Tool executed successfully"
        if ($result.data) {
            if ($result.data -is [string]) {
                Write-Host $result.data
            } else {
                $result.data | ConvertTo-Json -Depth 5
            }
        }
    } else {
        Write-Error2 "Tool failed: $($result.error)"
    }
}

function Invoke-FileCommand {
    param([string[]]$Args)
    
    if ($Args.Count -lt 1) {
        Write-Error2 "Usage: rawr file <read|write|list|exists|delete> <path> [content]"
        return
    }
    
    $subcmd = $Args[0]
    
    switch ($subcmd) {
        "read" {
            if ($Args.Count -lt 2) { Write-Error2 "Usage: rawr file read <path>"; return }
            $result = Invoke-Tool -ToolName "file_read" -Params @{ path = $Args[1] }
            if ($result.success) { Write-Host $result.data }
            else { Write-Error2 $result.error }
        }
        "write" {
            if ($Args.Count -lt 3) { Write-Error2 "Usage: rawr file write <path> <content>"; return }
            $content = $Args[2..($Args.Count-1)] -join " "
            $result = Invoke-Tool -ToolName "file_write" -Params @{ path = $Args[1]; content = $content }
            if ($result.success) { Write-Success "File written" }
            else { Write-Error2 $result.error }
        }
        "list" {
            $path = if ($Args.Count -ge 2) { $Args[1] } else { "." }
            $result = Invoke-Tool -ToolName "file_list" -Params @{ path = $path }
            if ($result.success) { $result.data.files | ForEach-Object { Write-Host "  $_" } }
            else { Write-Error2 $result.error }
        }
        "exists" {
            if ($Args.Count -lt 2) { Write-Error2 "Usage: rawr file exists <path>"; return }
            $result = Invoke-Tool -ToolName "file_exists" -Params @{ path = $Args[1] }
            if ($result.success) { Write-Host "Exists: $($result.data.exists)" }
        }
        "delete" {
            if ($Args.Count -lt 2) { Write-Error2 "Usage: rawr file delete <path>"; return }
            $result = Invoke-Tool -ToolName "file_delete" -Params @{ path = $Args[1] }
            if ($result.success) { Write-Success "File deleted" }
            else { Write-Error2 $result.error }
        }
        default {
            Write-Error2 "Unknown file command: $subcmd"
        }
    }
}

function Invoke-GitCommand {
    param([string[]]$Args)
    
    if ($Args.Count -lt 1) {
        Write-Error2 "Usage: rawr git <status|add|commit|push|pull|diff>"
        return
    }
    
    $subcmd = $Args[0]
    
    switch ($subcmd) {
        "status" {
            $result = Invoke-Tool -ToolName "git_status" -Params @{}
            if ($result.success) { Write-Host $result.data }
            else { Write-Error2 $result.error }
        }
        "add" {
            if ($Args.Count -lt 2) { Write-Error2 "Usage: rawr git add <files>"; return }
            $paths = $Args[1..($Args.Count-1)] -join ","
            $result = Invoke-Tool -ToolName "git_add" -Params @{ paths = $paths }
            if ($result.success) { Write-Success "Files staged" }
            else { Write-Error2 $result.error }
        }
        "commit" {
            if ($Args.Count -lt 2) { Write-Error2 "Usage: rawr git commit <message>"; return }
            $msg = $Args[1..($Args.Count-1)] -join " "
            $result = Invoke-Tool -ToolName "git_commit" -Params @{ message = $msg }
            if ($result.success) { Write-Success "Committed" }
            else { Write-Error2 $result.error }
        }
        "push" {
            $result = Invoke-Tool -ToolName "git_push" -Params @{}
            if ($result.success) { Write-Success "Pushed" }
            else { Write-Error2 $result.error }
        }
        "pull" {
            $result = Invoke-Tool -ToolName "git_pull" -Params @{}
            if ($result.success) { Write-Success "Pulled" }
            else { Write-Error2 $result.error }
        }
        "diff" {
            $spec = if ($Args.Count -ge 2) { $Args[1..($Args.Count-1)] -join " " } else { "" }
            $result = Invoke-Tool -ToolName "git_diff" -Params @{ spec = $spec }
            if ($result.success) { Write-Host $result.data }
            else { Write-Error2 $result.error }
        }
        default {
            Write-Error2 "Unknown git command: $subcmd"
        }
    }
}

function Invoke-ExecCommand {
    param([string[]]$Args)
    
    if ($Args.Count -lt 1) {
        Write-Error2 "Usage: rawr exec <command>"
        return
    }
    
    $cmd = $Args -join " "
    Write-Info "Executing: $cmd"
    
    $result = Invoke-Tool -ToolName "exec" -Params @{ command = $cmd }
    if ($result.success) {
        Write-Host $result.data
    } else {
        Write-Error2 $result.error
    }
}

# ============================================================================
# Agentic AI
# ============================================================================
function Invoke-Agent {
    param([string]$Task)
    
    if (-not $Task) {
        Write-Error2 "Usage: rawr agent <task_description>"
        return
    }
    
    $model = $script:State.CurrentModel ?? $script:Config.DefaultModel
    
    Write-Info "Running agentic task: $Task`n"
    
    # Generate tool prompt
    $toolPrompt = "You can call tools using the format: TOOL:<name>:<json parameters>`nAvailable tools:`n"
    foreach ($tool in $script:ToolSchemas.Keys | Sort-Object) {
        $schema = $script:ToolSchemas[$tool]
        $toolPrompt += " - $tool: $($schema.description)`n"
    }
    
    $systemPrompt = @"
$toolPrompt

When you need to use a tool, respond with TOOL:<name>:<json>.
Otherwise, provide your final answer.
"@
    
    $history = @(
        @{ role = "system"; content = $systemPrompt }
        @{ role = "user"; content = $Task }
    )
    
    $maxIterations = 10
    $iteration = 0
    
    while ($iteration -lt $maxIterations) {
        try {
            $body = @{
                model = $model
                messages = $history
                stream = $false
                options = @{
                    temperature = $script:Config.Temperature
                }
            } | ConvertTo-Json -Depth 10
            
            $response = Invoke-RestMethod -Uri "$($script:Config.OllamaEndpoint)/api/chat" -Method Post -Body $body -ContentType "application/json"
            
            $aiResponse = $response.message.content
            
            # Check for tool call
            if ($aiResponse -match "TOOL:(\w+):(\{[^}]+\})") {
                $toolName = $Matches[1]
                $toolParams = $Matches[2] | ConvertFrom-Json -AsHashtable
                
                Write-ColorOutput Dim "  AI calling tool: $toolName"
                
                $result = Invoke-Tool -ToolName $toolName -Params $toolParams
                
                if ($result.success) {
                    Write-ColorOutput Green "  ✓ Tool $toolName succeeded"
                    $toolResult = $result.data | ConvertTo-Json -Depth 5 -Compress
                } else {
                    Write-ColorOutput Red "  ✗ Tool $toolName failed: $($result.error)"
                    $toolResult = "Error: $($result.error)"
                }
                
                # Add to history
                $history += @{ role = "assistant"; content = $aiResponse }
                $history += @{ role = "user"; content = "Tool result: $toolResult" }
                
                $iteration++
                continue
            } else {
                # Final answer
                Write-ColorOutput Cyan "`nResult: " -NoNewline
                Write-Host "$aiResponse`n"
                return
            }
        } catch {
            Write-Error2 "Agent failed: $_"
            return
        }
        
        $iteration++
    }
    
    Write-Warn "Maximum iterations reached"
}

# ============================================================================
# Advanced Agentic Commands (Zero-Day Engine Integration)
# ============================================================================

function Invoke-Mission {
    param([string]$Objective)
    
    if (-not $Objective) {
        Write-Error2 "Usage: rawr mission <objective>"
        return
    }
    
    Write-Info "Starting agentic mission: $Objective"
    Write-ColorOutput Dim "Zero-Day Agentic Engine taking control...`n"
    
    # Use native binary for mission execution
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary mission $Objective
    } elseif (Test-Path $script:Config.FallbackBinary) {
        & $script:Config.FallbackBinary mission $Objective
    } else {
        Write-Error2 "SafeMode binary not found. Build RawrXD-SafeMode first."
        Write-Info "Binary expected at: $($script:Config.SafeModeBinary)"
    }
}

function Invoke-Plan {
    param([string]$Objective)
    
    if (-not $Objective) {
        Write-Error2 "Usage: rawr plan <objective>"
        return
    }
    
    Write-Info "Generating execution plan for: $Objective"
    Write-ColorOutput Dim "Plan Orchestrator analyzing task...`n"
    
    # Use native binary for plan generation
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary plan $Objective
    } elseif (Test-Path $script:Config.FallbackBinary) {
        & $script:Config.FallbackBinary plan $Objective
    } else {
        Write-Error2 "SafeMode binary not found. Build RawrXD-SafeMode first."
    }
}

function Invoke-ExecutePlan {
    Write-Info "Executing last generated plan..."
    Write-ColorOutput Dim "Plan Orchestrator executing...`n"
    
    # Use native binary for plan execution
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary execute_plan
    } elseif (Test-Path $script:Config.FallbackBinary) {
        & $script:Config.FallbackBinary execute_plan
    } else {
        Write-Error2 "SafeMode binary not found. Build RawrXD-SafeMode first."
    }
}

function Invoke-Registry {
    Write-ColorOutput Cyan "`n=== Production Tool Registry (44+ Tools) ===`n"
    
    # Use native binary for registry listing
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary registry
    } elseif (Test-Path $script:Config.FallbackBinary) {
        & $script:Config.FallbackBinary registry
    } else {
        # Fallback: list local tool schemas
        Write-ColorOutput Bold "Local PowerShell Tools:"
        foreach ($tool in $script:ToolSchemas.Keys | Sort-Object) {
            $schema = $script:ToolSchemas[$tool]
            Write-Host "  $($script:Colors.Green)$tool$($script:Colors.Reset) - $($schema.description)"
        }
        Write-Host ""
        Write-Warn "For full 44+ tool registry, build RawrXD-SafeMode"
    }
}

function Invoke-Hotpatch {
    param([string[]]$Args)
    
    if ($Args.Count -lt 1) {
        Write-Error2 "Usage: rawr hotpatch <list|apply|revert> [id]"
        return
    }
    
    $subcmd = $Args[0].ToLower()
    
    switch ($subcmd) {
        "list" {
            Write-ColorOutput Cyan "`n=== Available Hotpatches ===`n"
            if (Test-Path $script:Config.SafeModeBinary) {
                & $script:Config.SafeModeBinary hotpatch list
            } elseif (Test-Path $script:Config.FallbackBinary) {
                & $script:Config.FallbackBinary hotpatch list
            } else {
                Write-Error2 "SafeMode binary not found"
            }
        }
        "apply" {
            if ($Args.Count -lt 2) {
                Write-Error2 "Usage: rawr hotpatch apply <id>"
                return
            }
            $patchId = $Args[1]
            Write-Info "Applying hotpatch: $patchId"
            if (Test-Path $script:Config.SafeModeBinary) {
                & $script:Config.SafeModeBinary hotpatch apply $patchId
            } elseif (Test-Path $script:Config.FallbackBinary) {
                & $script:Config.FallbackBinary hotpatch apply $patchId
            } else {
                Write-Error2 "SafeMode binary not found"
            }
        }
        "revert" {
            if ($Args.Count -lt 2) {
                Write-Error2 "Usage: rawr hotpatch revert <id>"
                return
            }
            $patchId = $Args[1]
            Write-Info "Reverting hotpatch: $patchId"
            if (Test-Path $script:Config.SafeModeBinary) {
                & $script:Config.SafeModeBinary hotpatch revert $patchId
            } elseif (Test-Path $script:Config.FallbackBinary) {
                & $script:Config.FallbackBinary hotpatch revert $patchId
            } else {
                Write-Error2 "SafeMode binary not found"
            }
        }
        default {
            Write-Error2 "Unknown hotpatch command: $subcmd"
            Write-Host "Usage: rawr hotpatch <list|apply|revert> [id]"
        }
    }
}

function Invoke-SelfTest {
    Write-ColorOutput Cyan "`n=== Self-Test Diagnostics ===`n"
    Write-Info "Running system self-test..."
    
    # Use native binary for self-test
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary selftest
    } elseif (Test-Path $script:Config.FallbackBinary) {
        & $script:Config.FallbackBinary selftest
    } else {
        # Fallback: basic PowerShell diagnostics
        Write-ColorOutput Bold "PowerShell Fallback Diagnostics:"
        
        # Check Ollama
        Write-Host "`n  Checking Ollama..."
        try {
            Invoke-RestMethod -Uri "$($script:Config.OllamaEndpoint)/api/tags" -Method Get -TimeoutSec 2 | Out-Null
            Write-Success "  Ollama: Running"
        } catch {
            Write-Error2 "  Ollama: Not available"
        }
        
        # Check models
        Write-Host "`n  Checking models..."
        $modelCount = 0
        foreach ($dir in $script:Config.ModelsDir) {
            if (Test-Path $dir) {
                $models = Get-ChildItem $dir -Filter "*.gguf" -ErrorAction SilentlyContinue
                $modelCount += $models.Count
            }
        }
        if ($modelCount -gt 0) {
            Write-Success "  Models: $modelCount GGUF file(s) found"
        } else {
            Write-Warn "  Models: No GGUF files found"
        }
        
        # Check workspace
        Write-Host "`n  Checking workspace..."
        if (Test-Path $script:State.WorkspaceRoot) {
            Write-Success "  Workspace: $($script:State.WorkspaceRoot)"
        } else {
            Write-Error2 "  Workspace: Invalid"
        }
        
        # System resources
        Write-Host "`n  System Resources:"
        $mem = Get-CimInstance Win32_OperatingSystem
        $usedGB = [math]::Round(($mem.TotalVisibleMemorySize - $mem.FreePhysicalMemory) / 1MB, 1)
        $totalGB = [math]::Round($mem.TotalVisibleMemorySize / 1MB, 1)
        $freePercent = [math]::Round(($mem.FreePhysicalMemory / $mem.TotalVisibleMemorySize) * 100, 1)
        Write-Host "    RAM: $usedGB / $totalGB GB ($freePercent% free)"
        
        Write-Host ""
        Write-Warn "For full diagnostics, build RawrXD-SafeMode"
    }
}

function Invoke-Telemetry {
    Write-ColorOutput Cyan "`n=== Telemetry Data ===`n"
    
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary telemetry
    } elseif (Test-Path $script:Config.FallbackBinary) {
        & $script:Config.FallbackBinary telemetry
    } else {
        Write-Error2 "SafeMode binary not found for full telemetry"
    }
}

function Invoke-Governor {
    param([string[]]$Args)
    
    if ($Args.Count -lt 1) {
        Write-Error2 "Usage: rawr governor <start|stop>"
        return
    }
    
    $subcmd = $Args[0].ToLower()
    
    switch ($subcmd) {
        "start" {
            Write-Info "Starting overclock governor..."
            if (Test-Path $script:Config.SafeModeBinary) {
                & $script:Config.SafeModeBinary governor start
            } else {
                Write-Error2 "SafeMode binary not found"
            }
        }
        "stop" {
            Write-Info "Stopping overclock governor..."
            if (Test-Path $script:Config.SafeModeBinary) {
                & $script:Config.SafeModeBinary governor stop
            } else {
                Write-Error2 "SafeMode binary not found"
            }
        }
        default {
            Write-Error2 "Unknown governor command: $subcmd"
        }
    }
}

function Invoke-Settings {
    Write-ColorOutput Cyan "`n=== Current Settings ===`n"
    
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary settings
    } else {
        # Show PowerShell config
        Write-ColorOutput Bold "SafeMode Binary:"
        Write-Host "  Path: $($script:Config.SafeModeBinary)"
        Write-Host "  Available: $(Test-Path $script:Config.SafeModeBinary)"
        
        Write-ColorOutput Bold "`nOllama Endpoint:"
        Write-Host "  $($script:Config.OllamaEndpoint)"
        
        Write-ColorOutput Bold "`nModel Directories:"
        foreach ($dir in $script:Config.ModelsDir) {
            $exists = Test-Path $dir
            Write-Host "  $dir ($($exists ? 'exists' : 'not found'))"
        }
        
        Write-ColorOutput Bold "`nCurrent State:"
        Write-Host "  Model: $($script:State.CurrentModel ?? 'None')"
        Write-Host "  Tier: $($script:State.CurrentTier)"
        Write-Host "  Workspace: $($script:State.WorkspaceRoot)"
        Write-Host "  Verbose: $($script:State.Verbose)"
        Write-Host ""
    }
}

function Set-Verbose {
    param([string]$Toggle)
    
    if ($Toggle -eq "on" -or $Toggle -eq "true" -or $Toggle -eq "1") {
        $script:State.Verbose = $true
        Write-Success "Verbose output enabled"
    } elseif ($Toggle -eq "off" -or $Toggle -eq "false" -or $Toggle -eq "0") {
        $script:State.Verbose = $false
        Write-Success "Verbose output disabled"
    } else {
        Write-Info "Verbose: $($script:State.Verbose)"
    }
}

function Invoke-Tiers {
    Write-ColorOutput Cyan "`n=== Available Compression Tiers ===`n"
    
    if (Test-Path $script:Config.SafeModeBinary) {
        & $script:Config.SafeModeBinary tiers
    } else {
        # List known tiers
        Write-ColorOutput Bold "Standard Tiers:"
        Write-Host "  Q2_K   - 2-bit quantization (smallest, fastest)"
        Write-Host "  Q4_0   - 4-bit quantization (good balance)"
        Write-Host "  Q4_K_M - 4-bit k-quants medium"
        Write-Host "  Q5_0   - 5-bit quantization"
        Write-Host "  Q5_K_M - 5-bit k-quants medium"
        Write-Host "  Q8_0   - 8-bit quantization (highest quality)"
        Write-Host "  F16    - Half precision (16-bit float)"
        Write-Host "  F32    - Full precision (32-bit float)"
        Write-Host ""
        Write-ColorOutput Bold "Brutal Compression Tiers:"
        Write-Host "  ULTRA  - Maximum compression (tier hopping)"
        Write-Host "  AUTO   - Automatic tier selection"
        Write-Host ""
    }
}

# ============================================================================
# System
# ============================================================================
function Invoke-Status {
    Write-ColorOutput Cyan "`n=== RawrXD SafeMode Status ===`n"
    
    Write-ColorOutput Bold "Model:"
    if ($script:State.CurrentModel) {
        Write-Host "  $($script:Colors.Green)● $($script:Colors.Reset)Loaded: $($script:State.CurrentModel)"
        Write-Host "  Active tier: $($script:State.CurrentTier)"
    } else {
        Write-ColorOutput Dim "  ○ No model loaded"
    }
    
    Write-ColorOutput Bold "`nServices:"
    
    # Check Ollama
    try {
        Invoke-RestMethod -Uri "$($script:Config.OllamaEndpoint)/api/tags" -Method Get -TimeoutSec 2 | Out-Null
        Write-Host "  $($script:Colors.Green)● $($script:Colors.Reset)Ollama: Running"
    } catch {
        Write-ColorOutput Dim "  ○ Ollama: Not available"
    }
    
    Write-ColorOutput Bold "`nWorkspace:"
    Write-Host "  Root: $($script:State.WorkspaceRoot)"
    
    # System info
    Write-ColorOutput Bold "`nSystem:"
    $mem = Get-CimInstance Win32_OperatingSystem
    $usedGB = [math]::Round(($mem.TotalVisibleMemorySize - $mem.FreePhysicalMemory) / 1MB, 1)
    $totalGB = [math]::Round($mem.TotalVisibleMemorySize / 1MB, 1)
    Write-Host "  RAM: $usedGB / $totalGB GB"
    
    # GPU if available
    try {
        $gpu = Get-CimInstance Win32_VideoController | Select-Object -First 1
        Write-Host "  GPU: $($gpu.Name)"
    } catch {}
    
    Write-Host ""
}

function Invoke-ApiControl {
    param([string[]]$Args)
    
    if ($Args.Count -lt 1) {
        Write-Error2 "Usage: rawr api <start|stop> [port]"
        return
    }
    
    $subcmd = $Args[0]
    
    switch ($subcmd) {
        "start" {
            $port = if ($Args.Count -ge 2) { [int]$Args[1] } else { $Port }
            
            # Use native binary if available
            if (Test-Path $script:Config.SafeModeBinary) {
                Write-Info "Starting API server on port $port..."
                Start-Process -FilePath $script:Config.SafeModeBinary -ArgumentList "-a", $port -NoNewWindow
            } else {
                Write-Warn "Native binary not available. Ollama API at $($script:Config.OllamaEndpoint)"
            }
        }
        "stop" {
            Write-Info "API server stop requested"
            # Would need process management here
        }
        default {
            Write-Error2 "Unknown api command: $subcmd"
        }
    }
}

function Set-Workspace {
    param([string]$Path)
    
    if (-not $Path) {
        Write-Info "Current workspace: $($script:State.WorkspaceRoot)"
        return
    }
    
    if (-not (Test-Path $Path)) {
        Write-Error2 "Path does not exist: $Path"
        return
    }
    
    $script:State.WorkspaceRoot = (Resolve-Path $Path).Path
    Set-Location $script:State.WorkspaceRoot
    Write-Success "Workspace set to: $($script:State.WorkspaceRoot)"
}

# ============================================================================
# REPL Mode
# ============================================================================
function Invoke-Repl {
    Show-Banner
    
    while ($true) {
        Write-ColorOutput Cyan "rawr> " -NoNewline
        $input = Read-Host
        
        if ([string]::IsNullOrWhiteSpace($input)) { continue }
        if ($input -eq "quit" -or $input -eq "exit" -or $input -eq "q") { break }
        
        $tokens = $input -split '\s+', 2
        $cmd = $tokens[0].ToLower()
        $args = if ($tokens.Count -gt 1) { $tokens[1] -split '\s+' } else { @() }
        
        switch ($cmd) {
            "help" { Show-Help }
            { $_ -in "run", "load" } { Invoke-ModelRun $args[0] }
            "list" { Invoke-ModelList }
            "show" { Invoke-ModelShow $args[0] }
            "tier" { Write-Info "Current tier: $($script:State.CurrentTier)" }
            { $_ -in "gen", "generate" } { Invoke-Generate ($args -join " ") }
            "stream" { Invoke-Stream ($args -join " ") }
            "chat" { Invoke-Chat }
            "agent" { Invoke-Agent ($args -join " ") }
            "tools" { Invoke-ToolList }
            "tool" { Invoke-ToolCommand $args[0] $args[1..($args.Count-1)] }
            "file" { Invoke-FileCommand $args }
            "git" { Invoke-GitCommand $args }
            "exec" { Invoke-ExecCommand $args }
            "status" { Invoke-Status }
            "api" { Invoke-ApiControl $args }
            { $_ -in "workspace", "ws" } { Set-Workspace $args[0] }
            "clear" { Clear-Host }
            default { Write-Error2 "Unknown command: $cmd. Type 'help' for available commands." }
        }
    }
    
    Write-ColorOutput Dim "Goodbye!"
}

# ============================================================================
# Main Entry Point
# ============================================================================
if ($Help) {
    Show-Help
    return
}

# Handle command-line invocation
if ($Command) {
    $cmd = $Command.ToLower()
    
    switch ($cmd) {
        "help" { Show-Help }
        { $_ -in "run", "load" } { Invoke-ModelRun $Arguments[0] }
        "list" { Invoke-ModelList }
        "show" { Invoke-ModelShow $Arguments[0] }
        "tier" { Write-Info "Current tier: $($script:State.CurrentTier)" }
        { $_ -in "gen", "generate" } { Invoke-Generate ($Arguments -join " ") }
        "stream" { Invoke-Stream ($Arguments -join " ") }
        "chat" { Invoke-Chat }
        "agent" { Invoke-Agent ($Arguments -join " ") }
        "tools" { Invoke-ToolList }
        "tool" { Invoke-ToolCommand $Arguments[0] $Arguments[1..($Arguments.Count-1)] }
        "file" { Invoke-FileCommand $Arguments }
        "git" { Invoke-GitCommand $Arguments }
        "exec" { Invoke-ExecCommand $Arguments }
        "status" { Invoke-Status }
        "api" { Invoke-ApiControl $Arguments }
        { $_ -in "workspace", "ws" } { Set-Workspace $Arguments[0] }
        "repl" { Invoke-Repl }
        default { Write-Error2 "Unknown command: $cmd. Run 'rawr help' for available commands." }
    }
} else {
    # No command - enter REPL mode
    Invoke-Repl
}
