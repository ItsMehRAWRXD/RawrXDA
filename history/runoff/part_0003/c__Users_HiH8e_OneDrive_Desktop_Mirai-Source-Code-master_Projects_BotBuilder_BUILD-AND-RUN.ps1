#!/usr/bin/env powershell

# BotBuilder GUI - Quick Build & Run Script
# Bypasses VS Code issues, uses pure CLI

Write-Host "🔷 BotBuilder GUI - Build & Run Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Navigate to project
$projectPath = "c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder"
$projectFile = Join-Path $projectPath "BotBuilder.csproj"

Write-Host "Project Path: $projectPath" -ForegroundColor Gray
Write-Host "Project File: $projectFile" -ForegroundColor Gray
Write-Host ""

# Check if project exists
if (-not (Test-Path $projectFile)) {
    Write-Host "❌ Project file not found!" -ForegroundColor Red
    exit 1
}

# Step 1: Clean
Write-Host "Step 1: Cleaning previous builds..." -ForegroundColor Yellow
dotnet clean $projectFile -v minimal -nologo
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Clean failed" -ForegroundColor Red
    exit 1
}
Write-Host "✅ Clean complete" -ForegroundColor Green
Write-Host ""

# Step 2: Restore
Write-Host "Step 2: Restoring NuGet packages..." -ForegroundColor Yellow
dotnet restore $projectFile -v minimal -nologo
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Restore failed" -ForegroundColor Red
    exit 1
}
Write-Host "✅ Packages restored" -ForegroundColor Green
Write-Host ""

# Step 3: Build
Write-Host "Step 3: Building solution..." -ForegroundColor Yellow
dotnet build $projectFile -c Release -v minimal -nologo
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Build failed" -ForegroundColor Red
    exit 1
}
Write-Host "✅ Build successful" -ForegroundColor Green
Write-Host ""

# Step 4: Run
Write-Host "Step 4: Starting application..." -ForegroundColor Yellow
Write-Host "Application window opening..." -ForegroundColor Gray
dotnet run --project $projectFile --no-build -c Release

Write-Host ""
Write-Host "✅ Application closed" -ForegroundColor Green
