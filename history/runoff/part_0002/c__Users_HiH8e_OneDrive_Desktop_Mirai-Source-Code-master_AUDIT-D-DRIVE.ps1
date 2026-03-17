#!/usr/bin/env pwsh
<#
.SYNOPSIS
Comprehensive D: Drive LLM Toolchain Audit
.DESCRIPTION
Scans and catalogs all AI/LLM components, models, and tools on D: drive
#>

Write-Host "🔍 Starting D: Drive Audit (LLM Toolchain Analysis)" -ForegroundColor Cyan
Write-Host "=" * 80

# Key directories to audit
$keyDirs = @(
    "D:\BIGDADDYG-RECOVERY",
    "D:\BigDaddyG-Standalone-40GB",
    "D:\BigDaddyG-40GB-Torrent",
    "D:\llm_toolchain",
    "D:\OllamaModels",
    "D:\models",
    "D:\01-AI-Models",
    "D:\agentic_framework",
    "D:\ai_copilot",
    "D:\offline_ai",
    "D:\TEMP-REUPLOAD-BigDaddyG-Part2-Compilers",
    "D:\TEMP-REUPLOAD-BigDaddyG-Part3-Web-AI-Automation"
)

$report = @{
    timestamp = Get-Date
    drive_total_size = 0
    model_files = @()
    toolchain_components = @()
    python_scripts = @()
    config_files = @()
    large_files = @()
}

# 1. Find all model weight files
Write-Host "`n📦 SEARCHING FOR MODEL FILES..." -ForegroundColor Yellow
$modelExtensions = @("*.gguf", "*.bin", "*.pth", "*.pt", "*.safetensors", "*.h5")
foreach ($ext in $modelExtensions) {
    try {
        $files = Get-ChildItem -Path "D:\" -Filter $ext -Recurse -ErrorAction SilentlyContinue -File | 
                 Select-Object Name, FullName, @{n='SizeGB';e={[math]::Round($_.Length/1GB,2)}}
        
        foreach ($file in $files) {
            Write-Host "  ✓ Found: $($file.Name) ($($file.SizeGB) GB)" -ForegroundColor Green
            $report.model_files += $file
        }
    } catch {
        # Skip on error
    }
}

# 2. Find Python scripts
Write-Host "`n🐍 SEARCHING FOR PYTHON SCRIPTS..." -ForegroundColor Yellow
try {
    $pyScripts = Get-ChildItem -Path "D:\" -Filter "*.py" -Recurse -ErrorAction SilentlyContinue | 
                 Where-Object { $_.DirectoryName -match "(llm|ai|model|gguf|bigdaddyg)" } |
                 Select-Object Name, FullName -First 30
    
    foreach ($script in $pyScripts) {
        Write-Host "  ✓ $($script.Name)" -ForegroundColor Green
        $report.python_scripts += $script
    }
} catch { }

# 3. Find key config files
Write-Host "`n⚙️  SEARCHING FOR CONFIG FILES..." -ForegroundColor Yellow
$configPatterns = @("*.json", "*.yaml", "*.yml", "*.toml", "*.cfg", "*.conf")
foreach ($pattern in $configPatterns) {
    try {
        $configs = Get-ChildItem -Path "D:\" -Filter $pattern -Recurse -ErrorAction SilentlyContinue |
                  Where-Object { $_.DirectoryName -match "(llm|ai|model|config)" } |
                  Select-Object Name, FullName -First 10
        
        foreach ($config in $configs) {
            Write-Host "  ✓ $($config.Name)" -ForegroundColor Green
            $report.config_files += $config
        }
    } catch { }
}

# 4. Find large files (potential models)
Write-Host "`n💾 SEARCHING FOR LARGE FILES (>500MB)..." -ForegroundColor Yellow
try {
    $largeFiles = Get-ChildItem -Path "D:\" -Recurse -ErrorAction SilentlyContinue -File |
                  Where-Object { $_.Length -gt 500MB } |
                  Select-Object Name, FullName, @{n='SizeGB';e={[math]::Round($_.Length/1GB,2)}} |
                  Sort-Object SizeGB -Descending |
                  Select-Object -First 20
    
    foreach ($file in $largeFiles) {
        Write-Host "  ✓ $($file.Name) - $($file.SizeGB) GB" -ForegroundColor Green
        $report.large_files += $file
    }
} catch { }

# 5. Check key directories existence
Write-Host "`n🗂️  KEY DIRECTORIES STATUS..." -ForegroundColor Yellow
foreach ($dir in $keyDirs) {
    if (Test-Path $dir) {
        $itemCount = (Get-ChildItem -Path $dir -Recurse -ErrorAction SilentlyContinue | Measure-Object).Count
        Write-Host "  ✓ $(Split-Path -Leaf $dir) - Contains $itemCount items" -ForegroundColor Green
    } else {
        Write-Host "  ✗ $(Split-Path -Leaf $dir) - NOT FOUND" -ForegroundColor Red
    }
}

# 6. Identify Python environment/requirements
Write-Host "`n📋 PYTHON DEPENDENCIES..." -ForegroundColor Yellow
$requirementsFiles = Get-ChildItem -Path "D:\" -Filter "requirements*.txt" -Recurse -ErrorAction SilentlyContinue |
                    Select-Object -First 10
foreach ($req in $requirementsFiles) {
    Write-Host "  ✓ Found: $($req.FullName)" -ForegroundColor Green
}

Write-Host "`n" + "=" * 80
Write-Host "✅ AUDIT COMPLETE" -ForegroundColor Green
Write-Host "=" * 80

# Summary
Write-Host "`n📊 SUMMARY:" -ForegroundColor Cyan
Write-Host "  Model Files Found: $($report.model_files.Count)"
Write-Host "  Python Scripts: $($report.python_scripts.Count)"
Write-Host "  Config Files: $($report.config_files.Count)"
Write-Host "  Large Files (>500MB): $($report.large_files.Count)"

# Export report
$reportPath = "c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\D-DRIVE-AUDIT-REPORT.json"
$report | ConvertTo-Json -Depth 10 | Out-File $reportPath
Write-Host "`n📄 Full report saved to: $reportPath" -ForegroundColor Cyan
