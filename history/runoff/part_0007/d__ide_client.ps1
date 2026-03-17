# RawrXD IDE CLI Client - Full IPC Communication
# Connects to the IDE command server via local socket and sends JSON commands

param(
    [Parameter(Mandatory=$false)]
    [string]$Command = "help",
    
    [Parameter(Mandatory=$false)]
    [string]$ModelPath = "",
    
    [Parameter(Mandatory=$false)]
    [string]$FilePath = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Message = "",
    
    [Parameter(Mandatory=$false)]
    [string]$DockName = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Directory = "D:\RawrXD-production-lazy-init\src",
    
    [Parameter(Mandatory=$false)]
    [string]$ServerName = "RawrXD_IDE_Server",
    
    [Parameter(Mandatory=$false)]
    [int]$Timeout = 30000  # 30 seconds
)

# Add .NET types for named pipes
Add-Type -AssemblyName System.Core

function Connect-IDEServer {
    param([string]$serverName, [int]$timeout)
    
    try {
        $pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", $serverName, [System.IO.Pipes.PipeDirection]::InOut)
        $pipe.Connect($timeout)
        
        if ($pipe.IsConnected) {
            return $pipe
        }
    }
    catch {
        Write-Host "❌ Failed to connect to IDE server: $_" -ForegroundColor Red
        return $null
    }
    
    return $null
}

function Send-IDECommand {
    param(
        [System.IO.Pipes.NamedPipeClientStream]$pipe,
        [hashtable]$commandData
    )
    
    try {
        # Convert command to JSON
        $json = $commandData | ConvertTo-Json -Compress
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($json + "`n")
        
        # Send command
        $pipe.Write($bytes, 0, $bytes.Length)
        $pipe.Flush()
        
        # Read response
        $reader = New-Object System.IO.StreamReader($pipe, [System.Text.Encoding]::UTF8)
        $responseLine = $reader.ReadLine()
        
        if ($responseLine) {
            return $responseLine | ConvertFrom-Json
        }
        
        return $null
    }
    catch {
        Write-Host "❌ Error communicating with server: $_" -ForegroundColor Red
        return $null
    }
}

function Format-Response {
    param($response)
    
    if (-not $response) {
        Write-Host "❌ No response received" -ForegroundColor Red
        return
    }
    
    Write-Host ""
    if ($response.status -eq "success") {
        Write-Host "✅ SUCCESS" -ForegroundColor Green
        Write-Host "Message: $($response.message)" -ForegroundColor Cyan
        
        if ($response.data) {
            Write-Host ""
            Write-Host "Data:" -ForegroundColor Yellow
            $response.data | ConvertTo-Json -Depth 10 | Write-Host
        }
    }
    elseif ($response.status -eq "error") {
        Write-Host "❌ ERROR" -ForegroundColor Red
        Write-Host "Error: $($response.error)" -ForegroundColor Red
        
        if ($response.details) {
            Write-Host "Details: $($response.details)" -ForegroundColor Yellow
        }
    }
    else {
        Write-Host "Response:" -ForegroundColor Cyan
        $response | ConvertTo-Json -Depth 10 | Write-Host
    }
    Write-Host ""
}

# Main execution
Write-Host ""
Write-Host "🎯 RawrXD IDE CLI Client" -ForegroundColor Cyan
Write-Host "━" * 60 -ForegroundColor Blue
Write-Host ""

# Connect to server
Write-Host "📡 Connecting to IDE server: $ServerName..." -ForegroundColor Yellow
$pipe = Connect-IDEServer -serverName $ServerName -timeout $Timeout

if (-not $pipe) {
    Write-Host ""
    Write-Host "💡 Make sure the IDE is running!" -ForegroundColor Yellow
    Write-Host "   Start with: Start-Process -FilePath 'D:\RawrXD-production-lazy-init\build\bin\Release\RawrXD-AgenticIDE.exe'" -ForegroundColor Gray
    Write-Host ""
    exit 1
}

Write-Host "✅ Connected to IDE!" -ForegroundColor Green
Write-Host ""

# Wait for welcome message
$reader = New-Object System.IO.StreamReader($pipe, [System.Text.Encoding]::UTF8)
$welcome = $reader.ReadLine()
if ($welcome) {
    $welcomeObj = $welcome | ConvertFrom-Json
    if ($welcomeObj.type -eq "welcome") {
        Write-Host "📩 $($welcomeObj.message)" -ForegroundColor Cyan
        Write-Host "   Version: $($welcomeObj.version)" -ForegroundColor Gray
        Write-Host "   Client ID: $($welcomeObj.clientId)" -ForegroundColor Gray
        Write-Host ""
    }
}

# Execute command
$commandData = @{
    command = $Command.ToLower()
    params = @{}
}

switch ($Command.ToLower()) {
    "load-model" {
        if ([string]::IsNullOrEmpty($ModelPath)) {
            Write-Host "❌ Error: -ModelPath parameter is required" -ForegroundColor Red
            Write-Host "   Usage: .\ide_client.ps1 -Command load-model -ModelPath 'path/to/model.gguf'" -ForegroundColor Yellow
            $pipe.Close()
            exit 1
        }
        
        Write-Host "📦 Loading model: $ModelPath" -ForegroundColor Yellow
        $commandData.params.path = $ModelPath
    }
    
    "open-file" {
        if ([string]::IsNullOrEmpty($FilePath)) {
            Write-Host "❌ Error: -FilePath parameter is required" -ForegroundColor Red
            Write-Host "   Usage: .\ide_client.ps1 -Command open-file -FilePath 'path/to/file.cpp'" -ForegroundColor Yellow
            $pipe.Close()
            exit 1
        }
        
        Write-Host "📂 Opening file: $FilePath" -ForegroundColor Yellow
        $commandData.params.path = $FilePath
    }
    
    "toggle-dock" {
        if ([string]::IsNullOrEmpty($DockName)) {
            Write-Host "❌ Error: -DockName parameter is required" -ForegroundColor Red
            Write-Host "   Usage: .\ide_client.ps1 -Command toggle-dock -DockName 'output'" -ForegroundColor Yellow
            Write-Host "   Valid docks: suggestions, security, optimizations, file-explorer, output, metrics" -ForegroundColor Gray
            $pipe.Close()
            exit 1
        }
        
        Write-Host "🔄 Toggling dock: $DockName" -ForegroundColor Yellow
        $commandData.params.name = $DockName
    }
    
    "send-chat" {
        if ([string]::IsNullOrEmpty($Message)) {
            Write-Host "❌ Error: -Message parameter is required" -ForegroundColor Red
            Write-Host "   Usage: .\ide_client.ps1 -Command send-chat -Message 'Analyze this code'" -ForegroundColor Yellow
            $pipe.Close()
            exit 1
        }
        
        Write-Host "💬 Sending chat message..." -ForegroundColor Yellow
        $commandData.params.message = $Message
    }
    
    "get-status" {
        Write-Host "📊 Getting IDE status..." -ForegroundColor Yellow
    }
    
    "list-files" {
        Write-Host "📁 Listing files in: $Directory" -ForegroundColor Yellow
        $commandData.params.directory = $Directory
    }
    
    "ping" {
        Write-Host "🏓 Pinging IDE server..." -ForegroundColor Yellow
    }
    
    "help" {
        Write-Host "📖 RawrXD IDE CLI Commands" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "Available Commands:" -ForegroundColor Yellow
        Write-Host "  load-model       Load a GGUF model into the IDE" -ForegroundColor White
        Write-Host "                   Usage: -Command load-model -ModelPath 'path/to/model.gguf'" -ForegroundColor Gray
        Write-Host ""
        Write-Host "  open-file        Open a file in the editor" -ForegroundColor White
        Write-Host "                   Usage: -Command open-file -FilePath 'path/to/file.cpp'" -ForegroundColor Gray
        Write-Host ""
        Write-Host "  toggle-dock      Show/hide a dock panel" -ForegroundColor White
        Write-Host "                   Usage: -Command toggle-dock -DockName 'output'" -ForegroundColor Gray
        Write-Host "                   Docks: suggestions, security, optimizations, file-explorer, output, metrics" -ForegroundColor DarkGray
        Write-Host ""
        Write-Host "  send-chat        Send a message to the AI chat" -ForegroundColor White
        Write-Host "                   Usage: -Command send-chat -Message 'Analyze this code'" -ForegroundColor Gray
        Write-Host ""
        Write-Host "  get-status       Get current IDE status" -ForegroundColor White
        Write-Host "                   Usage: -Command get-status" -ForegroundColor Gray
        Write-Host ""
        Write-Host "  list-files       List files in a directory" -ForegroundColor White
        Write-Host "                   Usage: -Command list-files -Directory 'path/to/dir'" -ForegroundColor Gray
        Write-Host ""
        Write-Host "  ping             Test server connection" -ForegroundColor White
        Write-Host "                   Usage: -Command ping" -ForegroundColor Gray
        Write-Host ""
        Write-Host "Examples:" -ForegroundColor Cyan
        Write-Host "  .\ide_client.ps1 -Command load-model -ModelPath 'D:\Models\model.gguf'" -ForegroundColor DarkGray
        Write-Host "  .\ide_client.ps1 -Command open-file -FilePath 'D:\project\main.cpp'" -ForegroundColor DarkGray
        Write-Host "  .\ide_client.ps1 -Command toggle-dock -DockName output" -ForegroundColor DarkGray
        Write-Host "  .\ide_client.ps1 -Command get-status" -ForegroundColor DarkGray
        Write-Host ""
        $pipe.Close()
        exit 0
    }
    
    default {
        Write-Host "❌ Unknown command: $Command" -ForegroundColor Red
        Write-Host "   Use -Command help for available commands" -ForegroundColor Yellow
        $pipe.Close()
        exit 1
    }
}

# Send command and get response
$response = Send-IDECommand -pipe $pipe -commandData $commandData

# Close connection
$pipe.Close()

# Display response
Format-Response -response $response

Write-Host "━" * 60 -ForegroundColor Blue
Write-Host ""
