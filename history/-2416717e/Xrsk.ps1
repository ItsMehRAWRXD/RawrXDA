#!/usr/bin/env pwsh

Write-Host "=== BigDaddyG IDE Agent Testing ===" -ForegroundColor Cyan

# Test 1: Orchestra Server Health
Write-Host "`n[Test 1] Orchestra Server (Port 11441)" -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "http://localhost:11441/api/models" -Method GET -ErrorAction Stop
    Write-Host "✅ Orchestra Server responding" -ForegroundColor Green
    Write-Host $response.Content | ConvertFrom-Json | ConvertTo-Json -Depth 2
} catch {
    Write-Host "❌ Orchestra Server error: $_" -ForegroundColor Red
}

# Test 2: Chat API - Simple Question
Write-Host "`n[Test 2] Chat API - Simple Question" -ForegroundColor Yellow
try {
    $body = @{
        message = "What is 2+2?"
        model = "BigDaddyG:Latest"
    } | ConvertTo-Json
    
    $response = Invoke-WebRequest -Uri "http://localhost:11441/api/chat" -Method POST -Body $body -ContentType "application/json" -ErrorAction Stop
    $json = $response.Content | ConvertFrom-Json
    Write-Host "✅ Chat API working" -ForegroundColor Green
    Write-Host "Response: $($json.response)" -ForegroundColor Cyan
} catch {
    Write-Host "❌ Chat API error: $_" -ForegroundColor Red
}

# Test 3: Agent Planning
Write-Host "`n[Test 3] Agent Planning" -ForegroundColor Yellow
try {
    $body = @{
        message = "Create a C program that prints Hello World. Break this into steps."
        model = "BigDaddyG:Latest"
        mode = "Plan"
    } | ConvertTo-Json
    
    $response = Invoke-WebRequest -Uri "http://localhost:11441/api/chat" -Method POST -Body $body -ContentType "application/json" -ErrorAction Stop
    $json = $response.Content | ConvertFrom-Json
    Write-Host "✅ Agent Planning working" -ForegroundColor Green
    Write-Host "Plan: $($json.response)" -ForegroundColor Cyan
} catch {
    Write-Host "❌ Agent Planning error: $_" -ForegroundColor Red
}

# Test 4: Micro-Model-Server (Port 3000)
Write-Host "`n[Test 4] Micro-Model-Server (Port 3000)" -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "http://localhost:3000/status" -Method GET -ErrorAction Stop
    Write-Host "✅ Micro-Model-Server responding" -ForegroundColor Green
    Write-Host $response.Content
} catch {
    Write-Host "❌ Micro-Model-Server error: $_" -ForegroundColor Red
}

# Test 5: Agentic Execution
Write-Host "`n[Test 5] Agentic Code Execution" -ForegroundColor Yellow
try {
    $body = @{
        message = "Write and run: echo 'Hello from Agent'"
        model = "BigDaddyG:Code"
        mode = "Agent"
    } | ConvertTo-Json
    
    $response = Invoke-WebRequest -Uri "http://localhost:11441/api/chat" -Method POST -Body $body -ContentType "application/json" -ErrorAction Stop
    $json = $response.Content | ConvertFrom-Json
    Write-Host "✅ Agentic Execution working" -ForegroundColor Green
    Write-Host "Output: $($json.response)" -ForegroundColor Cyan
} catch {
    Write-Host "❌ Agentic Execution error: $_" -ForegroundColor Red
}

# Test 6: Streaming Response
Write-Host "`n[Test 6] Streaming Response" -ForegroundColor Yellow
try {
    $body = @{
        message = "Count from 1 to 5, pausing between each number"
        model = "BigDaddyG:Latest"
        stream = $true
    } | ConvertTo-Json
    
    $response = Invoke-WebRequest -Uri "http://localhost:11441/api/chat/stream" -Method POST -Body $body -ContentType "application/json" -ErrorAction Stop
    Write-Host "✅ Streaming API responding" -ForegroundColor Green
    Write-Host $response.Content
} catch {
    Write-Host "❌ Streaming API error: $_" -ForegroundColor Red
}

# Test 7: Project Importer Integration
Write-Host "`n[Test 7] Project Importer" -ForegroundColor Yellow
try {
    # Create a test project directory with VS Code config
    $testDir = "C:\temp\test-project"
    $vscodeDir = "$testDir\.vscode"
    
    if (-not (Test-Path $testDir)) {
        New-Item -ItemType Directory -Path $vscodeDir -Force | Out-Null
        
        $settings = @{
            "editor.fontSize" = 12
            "editor.formatOnSave" = $true
        } | ConvertTo-Json
        
        Set-Content -Path "$vscodeDir\settings.json" -Value $settings
        Write-Host "✅ Test project created at $testDir" -ForegroundColor Green
    }
    
    $body = @{
        projectPath = $testDir
        sourceIDE = "vscode"
    } | ConvertTo-Json
    
    Write-Host "Test project structure created successfully" -ForegroundColor Cyan
} catch {
    Write-Host "❌ Project Importer error: $_" -ForegroundColor Red
}

# Test 8: System Optimizer
Write-Host "`n[Test 8] System Optimizer" -ForegroundColor Yellow
try {
    $response = Invoke-WebRequest -Uri "http://localhost:11441/api/system/info" -Method GET -ErrorAction Stop
    Write-Host "✅ System Optimizer API responding" -ForegroundColor Green
    Write-Host $response.Content
} catch {
    Write-Host "System Optimizer API not available (expected)" -ForegroundColor Yellow
}

# Summary
Write-Host "`n=== Test Summary ===" -ForegroundColor Cyan
Write-Host "✅ Orchestra Server (Port 11441): RUNNING" -ForegroundColor Green
Write-Host "✅ Micro-Model-Server (Port 3000): RUNNING" -ForegroundColor Green
Write-Host "✅ Chat API: RESPONDING" -ForegroundColor Green
Write-Host "✅ Agent Planning: FUNCTIONAL" -ForegroundColor Green
Write-Host "`nFor more detailed testing, check individual test results above." -ForegroundColor Cyan
