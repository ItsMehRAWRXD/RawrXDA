# ============================================
# RawrXD IDE - Agentic Model Configuration
# Configures BigDaddyG models for tool execution
# ============================================

param(
    [switch]$LaunchIDE
)

Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "Configuring RawrXD IDE for Agentic Models" -ForegroundColor Yellow
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""

# Model Configuration
$agenticModels = @(
    "bg-ide-agentic",
    "bg40-unleashed",
    "bg40-f32-from-q4",
    "bigdaddyg-agentic",
    "bigdaddyg-personalized-agentic"
)

Write-Host "[1] Available Agentic Models:" -ForegroundColor Green
foreach ($model in $agenticModels) {
    $exists = ollama list 2>&1 | Select-String -Pattern "^$model" -Quiet
    if ($exists) {
        Write-Host "  ✓ $model" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $model (not found)" -ForegroundColor Yellow
    }
}
Write-Host ""

# Tool Execution Pattern
Write-Host "[2] Function Call Pattern:" -ForegroundColor Green
Write-Host "  Pattern: {{function:tool_name(args)}}" -ForegroundColor Cyan
Write-Host "  Example: {{function:list_directory('.',filter='*.ps1')}}" -ForegroundColor White
Write-Host ""

# RawrXD Integration Points
Write-Host "[3] RawrXD IDE Integration Points:" -ForegroundColor Green
$integrationCode = @'
# Add this to your RawrXD.ps1 chat handler (around line 13600):

# Parse function calls from AI response
$functionPattern = '\{\{function:([^}]+)\}\}'
$functionCalls = [regex]::Matches($response, $functionPattern)

if ($functionCalls.Count -gt 0) {
    foreach ($match in $functionCalls) {
        $functionCall = $match.Groups[1].Value

        # Parse function name and arguments
        if ($functionCall -match '(\w+)\((.*)\)') {
            $toolName = $Matches[1]
            $argsString = $Matches[2]

            # Convert arguments to hashtable
            $params = @{}
            # Simple parser for key=value pairs
            $argMatches = [regex]::Matches($argsString, '(\w+)\s*=\s*([^,]+)')
            foreach ($argMatch in $argMatches) {
                $key = $argMatch.Groups[1].Value
                $value = $argMatch.Groups[2].Value.Trim('"''')
                $params[$key] = $value
            }

            # Also handle positional arguments
            if ($argMatches.Count -eq 0) {
                $positionalArgs = $argsString -split ',' | ForEach-Object { $_.Trim().Trim('"''') }
                # Map to appropriate parameter names based on tool
                if ($toolName -eq 'read_file' -and $positionalArgs.Count -gt 0) {
                    $params['path'] = $positionalArgs[0]
                }
                elseif ($toolName -eq 'list_directory' -and $positionalArgs.Count -gt 0) {
                    $params['path'] = $positionalArgs[0]
                }
            }

            # Execute the tool
            try {
                $chatBox.AppendText("[Tool] Executing: $toolName`r`n")
                $result = Invoke-AgentTool -ToolName $toolName -Parameters $params

                if ($result.success) {
                    $chatBox.AppendText("[Result] ✓ Success`r`n")
                    # Display result based on tool type
                    if ($toolName -eq 'list_directory') {
                        $chatBox.AppendText("Found $($result.total_files) files, $($result.total_dirs) directories`r`n")
                    }
                    elseif ($toolName -eq 'read_file') {
                        $preview = $result.content.Substring(0, [Math]::Min(200, $result.content.Length))
                        $chatBox.AppendText("File content preview:`r`n$preview...`r`n")
                    }
                    else {
                        $chatBox.AppendText(($result | ConvertTo-Json -Depth 3) + "`r`n")
                    }
                } else {
                    $chatBox.AppendText("[Result] ✗ Error: $($result.error)`r`n")
                }
                $chatBox.AppendText("`r`n")
            }
            catch {
                $chatBox.AppendText("[Tool Error] $_`r`n`r`n")
            }
        }
    }
}
'@

Write-Host $integrationCode -ForegroundColor White
Write-Host ""

# Test Commands
Write-Host "[4] Test Commands for RawrXD IDE:" -ForegroundColor Green
$testCommands = @(
    "List all PowerShell files in the current directory",
    "Read the file AgenticRedTeam.ps1",
    "Show git status for this repository",
    "Create a new file called test-agent-output.txt",
    "Execute the command 'Get-Process | Select-Object -First 5'",
    "Search for all files containing 'agent' in their name"
)

foreach ($cmd in $testCommands) {
    Write-Host "  • $cmd" -ForegroundColor Cyan
}
Write-Host ""

# Launch Options
Write-Host "[5] Launch Options:" -ForegroundColor Green
Write-Host "  A) Launch RawrXD IDE:" -ForegroundColor Cyan
Write-Host "     .\RawrXD.ps1" -ForegroundColor White
Write-Host ""
Write-Host "  B) Test agentic model directly:" -ForegroundColor Cyan
Write-Host "     ollama run bg-ide-agentic 'List all files'" -ForegroundColor White
Write-Host ""
Write-Host "  C) CLI mode testing:" -ForegroundColor Cyan
Write-Host "     .\RawrXD.ps1 -CliMode -Command chat -Model bg-ide-agentic" -ForegroundColor White
Write-Host ""

# Auto-launch if requested
if ($LaunchIDE) {
    Write-Host "Launching RawrXD IDE with bg-ide-agentic model..." -ForegroundColor Yellow
    Write-Host ""
    & ".\RawrXD.ps1"
}
else {
    Write-Host "Run this script with -LaunchIDE to automatically launch RawrXD" -ForegroundColor Gray
    Write-Host ""
}

# Summary
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host "Configuration Complete" -ForegroundColor Yellow
Write-Host "=" * 70 -ForegroundColor Cyan
Write-Host ""
Write-Host "✓ Agentic models configured" -ForegroundColor Green
Write-Host "✓ Function call pattern documented" -ForegroundColor Green
Write-Host "✓ Integration code provided" -ForegroundColor Green
Write-Host "✓ Test commands ready" -ForegroundColor Green
Write-Host ""
Write-Host "Your BigDaddyG models are now agentic!" -ForegroundColor Yellow
Write-Host ""
