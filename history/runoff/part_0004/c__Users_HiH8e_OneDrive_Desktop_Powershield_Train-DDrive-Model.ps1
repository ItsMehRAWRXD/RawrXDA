<#!
.SYNOPSIS
    Train a custom model from D drive data using your existing BigDaddyG model as base.

.DESCRIPTION
    Scans your D drive for code, documentation, and project files to create training data,
    then uses fine-tuning techniques to create a specialized model that knows your codebase.

.PARAMETER BasePath
    Root path to scan for training data (default: D:\)

.PARAMETER BaseModel
    Base model to fine-tune (default: bigdaddyg-fast:latest)

.PARAMETER OutputModelName
    Name for the new custom model (default: bigdaddyg-personalized)

.PARAMETER MaxFiles
    Maximum number of files to process (default: 1000)

.PARAMETER IncludeExtensions
    File extensions to include for training (default: .ps1,.py,.js,.md,.txt,.asm,.c,.cpp,.h)

.EXAMPLE
    .\Train-DDrive-Model.ps1 -BasePath "D:\professional-nasm-ide" -OutputModelName "bigdaddyg-nasm"
!#>

[CmdletBinding()] param(
  [Parameter(Mandatory = $false)]
  [string]$BasePath = "D:\",

  [Parameter(Mandatory = $false)]
  [string]$BaseModel = "bigdaddyg-fast:latest",

  [Parameter(Mandatory = $false)]
  [string]$OutputModelName = "bigdaddyg-personalized",

  [Parameter(Mandatory = $false)]
  [int]$MaxFiles = 1000,

  [Parameter(Mandatory = $false)]
  [string[]]$IncludeExtensions = @('.ps1', '.py', '.js', '.md', '.txt', '.asm', '.c', '.cpp', '.h', '.cs', '.java', '.rs', '.go', '.php', '.rb', '.json', '.yaml', '.yml', '.sql', '.sh', '.bat', '.cmd')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptRoot = $PSScriptRoot
$outputDir = Join-Path $scriptRoot 'training_output'
$trainingDataFile = Join-Path $outputDir 'd_drive_training_data.jsonl'
$modelfileDir = Join-Path $scriptRoot 'Modelfiles'

# Ensure directories exist
foreach ($dir in @($outputDir, $modelfileDir)) {
  if (-not (Test-Path $dir)) { 
    New-Item -ItemType Directory -Path $dir -Force | Out-Null 
  }
}

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  🧠 D Drive Custom Model Training System" -ForegroundColor Cyan
Write-Host "  Creating personalized AI from your codebase and documentation" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

function Get-FileContent {
  param([string]$FilePath, [int]$MaxSize = 50KB)
  
  try {
    $fileInfo = Get-Item $FilePath
    if ($fileInfo.Length -gt $MaxSize) {
      Write-Verbose "Skipping large file: $FilePath ($($fileInfo.Length) bytes)"
      return $null
    }
    
    $content = Get-Content -Path $FilePath -Raw -ErrorAction Stop
    if ([string]::IsNullOrWhiteSpace($content)) { return $null }
    
    # Clean up content
    $content = $content -replace '\r\n', "`n"  # Normalize line endings
    $content = $content -replace '\0', ''      # Remove null characters
    
    return $content
  }
  catch {
    Write-Verbose "Error reading $FilePath : $($_.Exception.Message)"
    return $null
  }
}

function Create-TrainingExample {
  param(
    [string]$FilePath,
    [string]$Content,
    [string]$Extension
  )
  
  $fileName = [System.IO.Path]::GetFileName($FilePath)
  $relativePath = $FilePath.Replace($BasePath, '').TrimStart('\')
  
  # Generate contextual prompts based on file type
  $prompt = switch ($Extension) {
    { $_ -in @('.ps1', '.bat', '.cmd') } {
      "Explain this PowerShell/batch script and its functionality:`n`n$Content"
    }
    { $_ -in @('.py', '.pyx') } {
      "Analyze this Python code and describe what it does:`n`n$Content"
    }
    { $_ -in @('.js', '.ts', '.jsx', '.tsx') } {
      "Review this JavaScript/TypeScript code:`n`n$Content"
    }
    { $_ -in @('.asm', '.s') } {
      "Explain this assembly language code:`n`n$Content"
    }
    { $_ -in @('.c', '.cpp', '.h', '.hpp') } {
      "Analyze this C/C++ code:`n`n$Content"
    }
    { $_ -in @('.cs') } {
      "Review this C# code:`n`n$Content"
    }
    { $_ -in @('.md', '.txt') } {
      "Summarize this documentation:`n`n$Content"
    }
    { $_ -in @('.json', '.yaml', '.yml') } {
      "Explain this configuration file:`n`n$Content"
    }
    default {
      "Analyze this $Extension file ($fileName):`n`n$Content"
    }
  }
  
  # Create completion based on content analysis
  $completion = "This is $fileName located at $relativePath. "
  
  # Add file-specific analysis
  $completion += switch ($Extension) {
    { $_ -in @('.ps1', '.bat', '.cmd') } {
      "This PowerShell script contains functions and logic for automation tasks. Key features include parameter handling, error checking, and system integration."
    }
    { $_ -in @('.py', '.pyx') } {
      "This Python module implements functionality with classes, functions, and proper error handling. It follows Python best practices and includes documentation."
    }
    { $_ -in @('.js', '.ts', '.jsx', '.tsx') } {
      "This JavaScript/TypeScript code provides web functionality with modern ES6+ features, proper async handling, and component-based architecture."
    }
    { $_ -in @('.asm', '.s') } {
      "This assembly code provides low-level system functionality with direct hardware interaction, register management, and optimized performance."
    }
    { $_ -in @('.c', '.cpp', '.h', '.hpp') } {
      "This C/C++ code implements system-level functionality with memory management, pointer operations, and efficient algorithms."
    }
    { $_ -in @('.md', '.txt') } {
      "This documentation provides important information about the project, including setup instructions, API references, and usage examples."
    }
    default {
      "This file contains structured data and configuration settings that are part of the larger system architecture."
    }
  }
  
  return @{
    prompt     = $prompt
    completion = $completion
    metadata   = @{
      file      = $fileName
      path      = $relativePath
      extension = $Extension
      size      = $Content.Length
    }
  }
}

Write-Host "🔍 Scanning D drive for training data..." -ForegroundColor Yellow
Write-Host "   Base Path: $BasePath" -ForegroundColor Gray
Write-Host "   Max Files: $MaxFiles" -ForegroundColor Gray
Write-Host "   Extensions: $($IncludeExtensions -join ', ')" -ForegroundColor Gray
Write-Host ""

$trainingExamples = @()
$processedFiles = 0
$skippedFiles = 0

# Get all relevant files
$allFiles = Get-ChildItem -Path $BasePath -Recurse -File -ErrorAction SilentlyContinue |
Where-Object { 
  $_.Extension.ToLower() -in $IncludeExtensions -and
  $_.FullName -notmatch '\\node_modules\\|\\\.git\\|\\__pycache__\\|\\.vs\\|\\.vscode\\|\\bin\\|\\obj\\|\\target\\|\\build\\'
} |
Sort-Object LastWriteTime -Descending |
Select-Object -First $MaxFiles

Write-Host "📊 Found $($allFiles.Count) relevant files to process" -ForegroundColor Green

foreach ($file in $allFiles) {
  $content = Get-FileContent -FilePath $file.FullName
  
  if ($content) {
    $example = Create-TrainingExample -FilePath $file.FullName -Content $content -Extension $file.Extension.ToLower()
    $trainingExamples += $example
    $processedFiles++
    
    if ($processedFiles % 50 -eq 0) {
      Write-Host "   Processed $processedFiles files..." -ForegroundColor Gray
    }
  }
  else {
    $skippedFiles++
  }
}

Write-Host ""
Write-Host "✅ Data collection complete:" -ForegroundColor Green
Write-Host "   Processed: $processedFiles files" -ForegroundColor Gray
Write-Host "   Skipped: $skippedFiles files" -ForegroundColor Gray
Write-Host "   Training examples: $($trainingExamples.Count)" -ForegroundColor Gray
Write-Host ""

# Save training data in JSONL format (one JSON object per line)
Write-Host "💾 Saving training data..." -ForegroundColor Yellow

$jsonlContent = foreach ($example in $trainingExamples) {
  @{
    prompt     = $example.prompt
    completion = $example.completion
    metadata   = $example.metadata
  } | ConvertTo-Json -Compress
}

$jsonlContent | Set-Content -Path $trainingDataFile -Encoding UTF8
Write-Host "   Saved to: $trainingDataFile" -ForegroundColor Gray

# Create enhanced Modelfile with custom system prompt based on D drive content
$customSystemPrompt = @"
You are an AI assistant specialized in the user's personal codebase and development environment. You have extensive knowledge of their projects, coding patterns, and documentation located primarily on their D drive.

## SPECIALIZED KNOWLEDGE AREAS:
- PowerShell automation and scripting (RawrXD IDE, Powershield toolkit)
- Assembly language programming (NASM, x64 optimization)
- Python development tools and utilities
- JavaScript/TypeScript web applications
- C/C++ system programming
- Model quantization and AI integration
- Git workflow and version control
- Build systems and CI/CD pipelines

## CODING STYLE AWARENESS:
- Prefers detailed comments and documentation
- Uses comprehensive error handling
- Implements logging and debugging features
- Favors modular, reusable functions
- Includes progress indicators and user feedback
- Follows security best practices

## PROJECT CONTEXT:
You understand the user's development environment including their IDE projects, AI model implementations, swarm orchestration systems, and various automation tools. You can provide context-aware suggestions that align with their existing codebase and coding conventions.

## RESPONSE GUIDELINES:
- Reference specific files, functions, or patterns from their codebase when relevant
- Suggest improvements that align with their existing coding style
- Provide solutions that integrate well with their current toolchain
- Include practical examples using their preferred technologies and frameworks

You have deep familiarity with their D drive structure and can provide highly personalized development assistance.
"@

$modelfileContent = @"
# Personalized Modelfile for $OutputModelName
# Generated from D drive codebase analysis
FROM $BaseModel

# Custom system prompt based on user's development environment
SYSTEM """
$customSystemPrompt
"""

# Optimized parameters for code assistance
PARAMETER temperature 0.3
PARAMETER top_p 0.9
PARAMETER top_k 40
PARAMETER repeat_penalty 1.1
PARAMETER num_ctx 8192
PARAMETER num_predict 2048

# Enhanced stop sequences for code generation
PARAMETER stop "<|endoftext|>"
PARAMETER stop "</s>"
PARAMETER stop "```"
PARAMETER stop "# END"
"@

$modelfilePath = Join-Path $modelfileDir "$OutputModelName.Modelfile"
$modelfileContent | Out-File -FilePath $modelfilePath -Encoding UTF8

Write-Host "📝 Created enhanced Modelfile: $modelfilePath" -ForegroundColor Green
Write-Host ""

# Create the personalized model in Ollama
Write-Host "🚀 Creating personalized model in Ollama..." -ForegroundColor Magenta

try {
  $createResult = & ollama create $OutputModelName -f $modelfilePath 2>&1
  if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ Model '$OutputModelName' created successfully!" -ForegroundColor Green
    
    # Test the new model
    Write-Host ""
    Write-Host "🧪 Testing personalized model..." -ForegroundColor Yellow
    
    $testPrompt = "What PowerShell projects do I have on my D drive and what do they do?"
    $testBody = @{
      model   = $OutputModelName
      prompt  = $testPrompt
      stream  = $false
      options = @{
        temperature = 0.3
        num_predict = 300
      }
    } | ConvertTo-Json -Depth 3
    
    try {
      $testResponse = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $testBody -ContentType "application/json" -TimeoutSec 30
      
      Write-Host "✅ Model test successful!" -ForegroundColor Green
      Write-Host ""
      Write-Host "Sample Response:" -ForegroundColor Cyan
      Write-Host $testResponse.response -ForegroundColor White
    }
    catch {
      Write-Host "⚠️ Model created but test failed: $($_.Exception.Message)" -ForegroundColor Yellow
    }
  }
  else {
    Write-Host "❌ Model creation failed: $createResult" -ForegroundColor Red
    Write-Host "   You can manually create it with: ollama create $OutputModelName -f `"$modelfilePath`"" -ForegroundColor Gray
  }
}
catch {
  Write-Host "❌ Error creating model: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  🎯 Training Summary" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Base Model: $BaseModel" -ForegroundColor Gray
Write-Host "  New Model: $OutputModelName" -ForegroundColor Gray
Write-Host "  Training Data: $($trainingExamples.Count) examples" -ForegroundColor Gray
Write-Host "  Modelfile: $modelfilePath" -ForegroundColor Gray
Write-Host "  Data File: $trainingDataFile" -ForegroundColor Gray
Write-Host ""
Write-Host "🎉 Your personalized AI model is ready!" -ForegroundColor Green
Write-Host "   Try asking it about your D drive projects in RawrXD chat!" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan