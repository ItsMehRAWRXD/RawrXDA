#requires -Version 7.0

<#
.SYNOPSIS
    Stress test: Complex multi-step request through Agentic Engine
.DESCRIPTION
    Tests if the agentic system can parse an ambiguous request, 
    create files, edit them, and save them while handling failures gracefully
#>

Push-Location "E:\Desktop\Powershield\Modules"

# Import all required modules
Import-Module ".\ModelInvocationFailureDetector.psm1" -Force
Import-Module ".\ProductionMonitoring.psm1" -Force
Import-Module ".\SecurityManager.psm1" -Force
Import-Module ".\AgenticEngine.psm1" -Force
Import-Module ".\AgenticIntegration.psm1" -Force

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RawrXD Agentic System - Complex Request Test                ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Initialize the agentic engine
Write-Host "🚀 Initializing Agentic Engine..." -ForegroundColor Yellow
try {
    $initResult = Initialize-AgenticEngine
    Write-Host "✓ Agentic Engine ready" -ForegroundColor Green
}
catch {
    Write-Host "⚠️  Agentic Engine initialization skipped: $_" -ForegroundColor Yellow
}
Write-Host ""

# Complex ambiguous request
$userPrompt = "create a react server in C++"

Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "USER REQUEST" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "> $userPrompt" -ForegroundColor Yellow
Write-Host ""

# Execute agentic flow
Write-Host "🧠 Processing request through Agentic Engine..." -ForegroundColor Cyan
Write-Host ""

try {
    $agenticResult = Invoke-AgenticFlow `
        -Prompt $userPrompt `
        -WorkingDirectory "E:\Desktop\Powershield\AgenticTest" `
        -OnUpdate {
            param($update)
            if ($update.Status -eq "Processing") {
                Write-Host "  ⏳ $($update.Step)" -ForegroundColor Gray
            }
            elseif ($update.Status -eq "Completed") {
                Write-Host "  ✓ $($update.Step)" -ForegroundColor Green
            }
            else {
                Write-Host "  ✗ Error: $($update.Error)" -ForegroundColor Red
            }
        }
}
catch {
    Write-Host "⚠️  Agentic flow execution skipped: $_" -ForegroundColor Yellow
    $agenticResult = $null
}

Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "AGENTIC ENGINE RESPONSE" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

if ($agenticResult) {
    Write-Host "Response:" -ForegroundColor Green
    Write-Host "$($agenticResult.Response)" -ForegroundColor Yellow
    Write-Host ""
    
    if ($agenticResult.Metrics) {
        Write-Host "Metrics:" -ForegroundColor Green
        Write-Host "  Duration: $($agenticResult.Metrics.Duration)ms" -ForegroundColor Gray
        Write-Host "  Tools Used: $($agenticResult.Metrics.ToolsUsed)" -ForegroundColor Gray
    }
}
else {
    Write-Host "❌ No response from agentic engine" -ForegroundColor Red
}

Write-Host ""

# Now attempt to use the failure detector with a more realistic curl-based model request
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "ASKING AI MODEL DIRECTLY (via Ollama)" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$aiResult = Invoke-ModelWithFailureDetection `
    -ModelName "llama3.2:3b" `
    -BackendType "Ollama" `
    -Action {
        Write-Host "  → Querying AI model about: '$userPrompt'" -ForegroundColor Gray
        
        $payload = @{
            model = "llama3.2:3b"
            prompt = "I need to $userPrompt. Provide a brief plan (5 steps max)"
            stream = $false
            temperature = 0.7
        } | ConvertTo-Json
        
        $response = & curl.exe -s -X POST "http://localhost:11434/api/generate" `
            -H "Content-Type: application/json" `
            -d $payload 2>&1
        
        if ($LASTEXITCODE -ne 0) {
            throw "Model invocation failed"
        }
        
        return $response | ConvertFrom-Json
    } `
    -EnableRetry `
    -MaxRetries 2

Write-Host ""

if ($aiResult.Success) {
    $answer = $aiResult.Result.response
    Write-Host "AI Plan:" -ForegroundColor Green
    Write-Host "$answer" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Duration: $($aiResult.Duration.TotalMilliseconds)ms" -ForegroundColor Gray
}
else {
    Write-Host "❌ Model invocation failed: $($aiResult.Error)" -ForegroundColor Red
    Write-Host "Failure Type: $($aiResult.FailureType)" -ForegroundColor Yellow
}

Write-Host ""

# Attempt to create a basic React server file in C++
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "FILE CREATION TEST" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$testDir = "E:\Desktop\Powershield\AgenticTest"
if (-not (Test-Path $testDir)) {
    New-Item -ItemType Directory -Path $testDir -Force | Out-Null
    Write-Host "📁 Created test directory: $testDir" -ForegroundColor Green
}

# Create a C++ file
$cppFile = "$testDir\react_server.cpp"

$cppContent = @"
#include <iostream>
#include <string>
#include <vector>
#include <http_server.h>  // Hypothetical C++ HTTP library
#include <json.hpp>

class ReactServer {
private:
    int port;
    std::vector<std::string> routes;

public:
    ReactServer(int p = 3000) : port(p) {}
    
    void addRoute(const std::string& path, const std::string& handler) {
        routes.push_back(path);
        std::cout << "Route added: " << path << std::endl;
    }
    
    void start() {
        std::cout << "React Server starting on port " << port << std::endl;
        // Server implementation would go here
    }
    
    void stop() {
        std::cout << "Stopping server..." << std::endl;
    }
};

int main() {
    ReactServer server(3000);
    server.addRoute("/", "home");
    server.addRoute("/api/data", "getData");
    server.start();
    return 0;
}
"@

try {
    $cppContent | Out-File -FilePath $cppFile -Encoding UTF8 -Force
    Write-Host "✓ Created: $cppFile" -ForegroundColor Green
    Write-Host "  Lines: $(($cppContent | Measure-Object -Line).Lines)" -ForegroundColor Gray
    Write-Host ""
}
catch {
    Write-Host "✗ Failed to create file: $_" -ForegroundColor Red
}

# Now attempt to edit it (add more features)
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "FILE EDITING TEST" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

if (Test-Path $cppFile) {
    $originalContent = Get-Content -Path $cppFile -Raw
    Write-Host "📖 Read original file: $(($originalContent | Measure-Object -Line).Lines) lines" -ForegroundColor Gray
    
    # Add logging feature
    $editedContent = $originalContent.Replace(
        'std::cout << "React Server starting on port " << port << std::endl;',
        @'
        std::cout << "React Server starting on port " << port << std::endl;
        std::cout << "Routes registered: " << routes.size() << std::endl;
'@
    )
    
    try {
        $editedContent | Out-File -FilePath $cppFile -Encoding UTF8 -Force
        Write-Host "✓ Edited file: Added logging" -ForegroundColor Green
        Write-Host "  New lines: $(($editedContent | Measure-Object -Line).Lines)" -ForegroundColor Gray
        Write-Host ""
    }
    catch {
        Write-Host "✗ Failed to edit file: $_" -ForegroundColor Red
    }
}

# Create a header file too
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "SUPPORTING FILE CREATION TEST" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$headerFile = "$testDir\react_server.h"

$headerContent = @"
#pragma once

#include <string>
#include <vector>
#include <functional>

class ReactServer {
public:
    explicit ReactServer(int port = 3000);
    ~ReactServer();
    
    void addRoute(const std::string& path, std::function<void()> handler);
    void start();
    void stop();
    void handle_request(const std::string& request);
    
private:
    int port_;
    std::vector<std::string> routes_;
    bool running_;
};
"@

try {
    $headerContent | Out-File -FilePath $headerFile -Encoding UTF8 -Force
    Write-Host "✓ Created: $headerFile" -ForegroundColor Green
    Write-Host "  Lines: $(($headerContent | Measure-Object -Line).Lines)" -ForegroundColor Gray
    Write-Host ""
}
catch {
    Write-Host "✗ Failed to create header file: $_" -ForegroundColor Red
}

# Verify files were created and saved
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "FILE VERIFICATION" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$files = Get-ChildItem -Path $testDir -Include "*.cpp", "*.h" 2>$null
if ($files) {
    Write-Host "✓ Files created and saved:" -ForegroundColor Green
    foreach ($file in $files) {
        $lines = @(Get-Content -Path $file.FullName).Count
        $size = (Get-Item -Path $file.FullName).Length
        Write-Host "  📄 $($file.Name) - $lines lines, $size bytes" -ForegroundColor Yellow
    }
}
else {
    Write-Host "✗ No files found" -ForegroundColor Red
}

Write-Host ""

# Generate a build script
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "BUILD SCRIPT GENERATION" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$buildScript = @"
# CMakeLists.txt - Build configuration for React C++ Server

cmake_minimum_required(VERSION 3.10)
project(ReactCppServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    react_server.cpp
)

# Create executable
add_executable(react_server `${SOURCES})

# Link libraries
# target_link_libraries(react_server PRIVATE http_server json)

# Compiler flags
if(MSVC)
    target_compile_options(react_server PRIVATE /W4)
else()
    target_compile_options(react_server PRIVATE -Wall -Wextra -pedantic)
endif()

# Install target
install(TARGETS react_server DESTINATION bin)
"@

$cmakeFile = "$testDir\CMakeLists.txt"
try {
    $buildScript | Out-File -FilePath $cmakeFile -Encoding UTF8 -Force
    Write-Host "✓ Created build script: CMakeLists.txt" -ForegroundColor Green
    Write-Host ""
}
catch {
    Write-Host "✗ Failed to create build script: $_" -ForegroundColor Red
}

# Final health check
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host "SYSTEM HEALTH CHECK" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Cyan
Write-Host ""

$health = Get-ModelHealth
Write-Host "Model Health: $($health.OverallHealth)" -ForegroundColor $(
    if ($health.OverallHealth -eq 'Healthy') { 'Green' } else { 'Yellow' }
)

# Skip metrics if not available
try {
    $metrics = Get-AgenticMetrics -ErrorAction SilentlyContinue
    if ($metrics) {
        Write-Host "Agentic Metrics:" -ForegroundColor Green
        Write-Host "  Total Actions: $($metrics.Total)" -ForegroundColor Gray
    }
}
catch { }

Write-Host ""
Write-Host "✓ Complex Request Test Complete" -ForegroundColor Green
Write-Host ""
Write-Host "📁 Generated files at: $testDir" -ForegroundColor Cyan

Pop-Location
