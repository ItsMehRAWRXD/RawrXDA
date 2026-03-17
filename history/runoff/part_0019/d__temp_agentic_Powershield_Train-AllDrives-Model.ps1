<#
.SYNOPSIS
    Train a custom model from ALL drives (C, D, E, F, G) — no file limit.

.DESCRIPTION
    Scans C:\, D:\, E:\, F:\, G:\ for code, documentation, and project files,
    merges everything into one JSONL training set, and creates a comprehensive
    Ollama model that knows your entire machine.

.PARAMETER Drives
    Array of drive roots to scan (default: C:\, D:\, E:\, F:\, G:\)

.PARAMETER BaseModel
    Base model to fine-tune (default: bigdaddyg-fast:latest)

.PARAMETER OutputModelName
    Name for the new custom model (default: bigdaddyg-alldrive)

.PARAMETER OutputDir
    Where to write results (default: D:\drive-model-output)

.PARAMETER IncludeExtensions
    File extensions to include

.EXAMPLE
    .\Train-AllDrives-Model.ps1
    .\Train-AllDrives-Model.ps1 -OutputModelName "bigdaddyg-full-machine"
#>

[CmdletBinding()] param(
  [string[]]$Drives = @('C:\', 'D:\', 'E:\', 'F:\', 'G:\'),

  [string]$BaseModel = "bigdaddyg-fast:latest",

  [string]$OutputModelName = "bigdaddyg-alldrive",

  [string]$OutputDir = "D:\drive-model-output",

  # Model tier upgrade: Auto-pulls a larger base model before training
  # Options: 'current' (keep BaseModel), '3b', '7b', '8b', '13b', '27b'
  [ValidateSet('current','3b','7b','8b','13b','27b')]
  [string]$ModelTier = 'current',

  [string[]]$IncludeExtensions = @(
    '.ps1', '.py', '.js', '.ts', '.jsx', '.tsx',
    '.md', '.txt', '.rst',
    '.asm', '.s', '.nasm',
    '.c', '.cpp', '.h', '.hpp', '.cc', '.cxx',
    '.cs', '.java', '.rs', '.go', '.rb', '.php',
    '.json', '.yaml', '.yml', '.toml', '.ini', '.cfg',
    '.sql', '.sh', '.bat', '.cmd',
    '.html', '.css', '.scss', '.less',
    '.xml', '.svg', '.proto',
    '.dockerfile', '.makefile', '.cmake'
  ),

  [int]$MaxFileSizeKB = 50
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

# ─── Output paths ─────────────────────────────────────────────────────
$trainingDataFile = Join-Path $OutputDir 'all_drives_training_data.jsonl'
$modelfileDir     = Join-Path $OutputDir 'Modelfiles'
$manifestFile     = Join-Path $OutputDir 'TRAINING_MANIFEST.json'

foreach ($dir in @($OutputDir, $modelfileDir)) {
  if (-not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
  }
}

# ─── Banner ───────────────────────────────────────────────────────────
Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  🧠  ALL-DRIVE MODEL TRAINING SYSTEM                        ║" -ForegroundColor Cyan
Write-Host "║  Feeding C:\ D:\ E:\ F:\ G:\ into one comprehensive model  ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Drives       : $($Drives -join '  ')" -ForegroundColor Yellow
Write-Host "  File Limit   : UNLIMITED" -ForegroundColor Yellow
Write-Host "  Max File Size: $($MaxFileSizeKB)KB per file" -ForegroundColor Yellow
Write-Host "  Output       : $OutputDir" -ForegroundColor Yellow
Write-Host "  Base Model   : $BaseModel" -ForegroundColor Yellow
Write-Host "  Model Tier   : $ModelTier" -ForegroundColor Yellow
Write-Host "  New Model    : $OutputModelName" -ForegroundColor Yellow
Write-Host ""

# ─── Model Tier Upgrade ──────────────────────────────────────────────
$tierMap = @{
  '3b'  = 'llama3.2:3b'
  '7b'  = 'llama3.1:7b'
  '8b'  = 'llama3.1:8b'
  '13b' = 'codellama:13b'
  '27b' = 'gemma3:27b'
}
if ($ModelTier -ne 'current' -and $tierMap.ContainsKey($ModelTier)) {
  $upgradeTag = $tierMap[$ModelTier]
  Write-Host "⬆️  Upgrading base model to $upgradeTag ($ModelTier tier) ..." -ForegroundColor Magenta
  try {
    $existing = ollama list 2>&1 | Select-String -Pattern ([regex]::Escape($upgradeTag))
    if (-not $existing) {
      Write-Host "   📥 Pulling $upgradeTag (this may take a while) ..." -ForegroundColor Yellow
      & ollama pull $upgradeTag 2>&1 | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkGray }
    } else {
      Write-Host "   ✅ $upgradeTag already available locally" -ForegroundColor Green
    }
    $BaseModel = $upgradeTag
    Write-Host "   ✅ Base model set to: $BaseModel" -ForegroundColor Green
  } catch {
    Write-Host "   ⚠️  Failed to pull $upgradeTag, keeping $BaseModel" -ForegroundColor Yellow
  }
  Write-Host ""
}

# ─── Exclusion patterns ──────────────────────────────────────────────
$ExcludePatterns = @(
  '\\node_modules\\',
  '\\\.git\\',
  '\\__pycache__\\',
  '\\\.(vs|vscode)\\',
  '\\(bin|obj|target|build|dist|out|debug|release)\\',
  '\\(\.cache|\.npm|\.nuget|\.cargo|\.rustup)\\',
  '\\Windows\\(WinSxS|assembly|Installer|SoftwareDistribution|servicing)\\',
  '\\Windows\\System32\\(DriverStore|config|winevt)\\',
  '\\Program Files.*\\(Common Files|WindowsApps|Microsoft)\\',
  '\\ProgramData\\(Microsoft|Package Cache)\\',
  '\\Recovery\\',
  '\\AppData\\Local\\(Temp|Microsoft|NuGet|npm-cache)\\',
  '\\\.ollama\\models\\',
  '\\site-packages\\',
  '\\vendor\\',
  '\\\.next\\'
)
$excludeRegex = ($ExcludePatterns | ForEach-Object { "($_)" }) -join '|'

# ─── Helpers ──────────────────────────────────────────────────────────
function Read-FileContent {
  param([string]$Path, [int]$MaxKB)
  try {
    $info = Get-Item $Path -ErrorAction Stop
    if ($info.Length -gt ($MaxKB * 1KB)) { return $null }
    $raw = Get-Content -Path $Path -Raw -ErrorAction Stop
    if ([string]::IsNullOrWhiteSpace($raw)) { return $null }
    $raw = $raw -replace '\r\n', "`n" -replace '\0', ''
    return $raw
  } catch { return $null }
}

function New-TrainingPair {
  param([string]$FilePath, [string]$Content, [string]$Ext, [string]$DriveRoot)

  $fileName     = [System.IO.Path]::GetFileName($FilePath)
  $relativePath = $FilePath

  $prompt = switch ($Ext) {
    { $_ -in @('.ps1','.bat','.cmd','.sh') }          { "Explain this shell/PowerShell script and its functionality:`n`n$Content" }
    { $_ -in @('.py','.pyx') }                         { "Analyze this Python code and describe what it does:`n`n$Content" }
    { $_ -in @('.js','.ts','.jsx','.tsx') }             { "Review this JavaScript/TypeScript code:`n`n$Content" }
    { $_ -in @('.asm','.s','.nasm') }                   { "Explain this assembly language code:`n`n$Content" }
    { $_ -in @('.c','.cpp','.h','.hpp','.cc','.cxx') } { "Analyze this C/C++ code:`n`n$Content" }
    { $_ -in @('.cs') }                                 { "Review this C# code:`n`n$Content" }
    { $_ -in @('.rs') }                                 { "Analyze this Rust code:`n`n$Content" }
    { $_ -in @('.go') }                                 { "Review this Go code:`n`n$Content" }
    { $_ -in @('.java') }                               { "Analyze this Java code:`n`n$Content" }
    { $_ -in @('.md','.txt','.rst') }                   { "Summarize this documentation:`n`n$Content" }
    { $_ -in @('.json','.yaml','.yml','.toml','.ini','.cfg') } { "Explain this configuration:`n`n$Content" }
    { $_ -in @('.html','.css','.scss','.less') }        { "Review this web markup/style:`n`n$Content" }
    { $_ -in @('.sql') }                                { "Analyze this SQL:`n`n$Content" }
    { $_ -in @('.proto') }                              { "Explain this protobuf definition:`n`n$Content" }
    default                                             { "Analyze this $Ext file ($fileName):`n`n$Content" }
  }

  $completion = "This is $fileName located at $relativePath on drive $DriveRoot. "
  $completion += switch ($Ext) {
    { $_ -in @('.ps1','.bat','.cmd','.sh') }            { "It contains shell/PowerShell automation logic with parameter handling, error checking, and system integration." }
    { $_ -in @('.py','.pyx') }                           { "It implements Python functionality with classes, functions, proper error handling, and documentation." }
    { $_ -in @('.js','.ts','.jsx','.tsx') }               { "It provides JavaScript/TypeScript functionality with modern ES6+ features, async handling, and component architecture." }
    { $_ -in @('.asm','.s','.nasm') }                     { "It provides low-level assembly with direct hardware interaction, register management, and optimized performance." }
    { $_ -in @('.c','.cpp','.h','.hpp','.cc','.cxx') }   { "It implements system-level C/C++ with memory management, pointer operations, and efficient algorithms." }
    { $_ -in @('.cs') }                                   { "It implements C# functionality with .NET integration, OOP patterns, and managed code." }
    { $_ -in @('.rs') }                                   { "It implements Rust with ownership semantics, safe concurrency, and zero-cost abstractions." }
    { $_ -in @('.go') }                                   { "It implements Go with goroutines, channels, and idiomatic error handling." }
    { $_ -in @('.java') }                                 { "It implements Java with OOP patterns, exception handling, and framework integration." }
    { $_ -in @('.md','.txt','.rst') }                     { "It provides documentation including setup instructions, API references, and usage examples." }
    { $_ -in @('.json','.yaml','.yml','.toml','.ini') }  { "It contains structured configuration settings that are part of the larger system architecture." }
    { $_ -in @('.html','.css','.scss','.less') }          { "It provides web presentation with markup, styling, and responsive design." }
    { $_ -in @('.sql') }                                  { "It contains database queries, schema definitions, or migration scripts." }
    default                                               { "It contains structured data relevant to the project architecture." }
  }

  return @{
    prompt     = $prompt
    completion = $completion
    metadata   = @{
      file      = $fileName
      path      = $relativePath
      drive     = $DriveRoot
      extension = $Ext
      size      = $Content.Length
    }
  }
}

# ─── SCAN ALL DRIVES ─────────────────────────────────────────────────
$allTrainingExamples = [System.Collections.ArrayList]::new()
$driveStats = @{}
$totalProcessed = 0
$totalSkipped   = 0
$scanStart = Get-Date

foreach ($drive in $Drives) {
  if (-not (Test-Path $drive)) {
    Write-Host "  ⚠️  $drive not found — skipping" -ForegroundColor Yellow
    continue
  }

  $driveLetter = $drive.Substring(0,2)
  Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
  Write-Host "  📂 Scanning $driveLetter ..." -ForegroundColor Cyan
  $driveStart = Get-Date

  $driveFiles = Get-ChildItem -Path $drive -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object {
      $_.Extension.ToLower() -in $IncludeExtensions -and
      $_.FullName -notmatch $excludeRegex
    } |
    Sort-Object LastWriteTime -Descending

  $driveFileCount = ($driveFiles | Measure-Object).Count
  Write-Host "     Found $driveFileCount files on $driveLetter" -ForegroundColor Green

  $driveProcessed = 0
  $driveSkipped   = 0

  foreach ($file in $driveFiles) {
    $content = Read-FileContent -Path $file.FullName -MaxKB $MaxFileSizeKB
    if ($content) {
      $example = New-TrainingPair -FilePath $file.FullName -Content $content -Ext $file.Extension.ToLower() -DriveRoot $driveLetter
      [void]$allTrainingExamples.Add($example)
      $driveProcessed++
      $totalProcessed++

      if ($driveProcessed % 500 -eq 0) {
        Write-Host "     ... $driveLetter $driveProcessed processed" -ForegroundColor Gray
      }
    } else {
      $driveSkipped++
      $totalSkipped++
    }
  }

  $driveElapsed = (Get-Date) - $driveStart
  Write-Host "     ✅ $driveLetter done: $driveProcessed processed, $driveSkipped skipped ($([math]::Round($driveElapsed.TotalSeconds))s)" -ForegroundColor Green

  $driveStats[$driveLetter] = @{
    total     = $driveFileCount
    processed = $driveProcessed
    skipped   = $driveSkipped
    elapsed   = [math]::Round($driveElapsed.TotalSeconds)
  }
}

$scanElapsed = (Get-Date) - $scanStart

Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
Write-Host "  📊 SCAN COMPLETE" -ForegroundColor Green
Write-Host "     Total Processed : $totalProcessed files" -ForegroundColor Yellow
Write-Host "     Total Skipped   : $totalSkipped files" -ForegroundColor Yellow
Write-Host "     Training Examples: $($allTrainingExamples.Count)" -ForegroundColor Yellow
Write-Host "     Scan Time       : $([math]::Round($scanElapsed.TotalMinutes, 1)) minutes" -ForegroundColor Yellow
Write-Host ""

foreach ($dk in $driveStats.Keys | Sort-Object) {
  $ds = $driveStats[$dk]
  Write-Host "     $dk  →  $($ds.processed) examples ($($ds.elapsed)s)" -ForegroundColor Gray
}
Write-Host ""

# ─── SAVE TRAINING DATA ──────────────────────────────────────────────
Write-Host "💾 Saving training data to $trainingDataFile ..." -ForegroundColor Yellow

$sw = [System.IO.StreamWriter]::new($trainingDataFile, $false, [System.Text.Encoding]::UTF8)
foreach ($ex in $allTrainingExamples) {
  $obj = @{
    prompt     = $ex.prompt
    completion = $ex.completion
    metadata   = $ex.metadata
  }
  $sw.WriteLine(($obj | ConvertTo-Json -Compress -Depth 4))
}
$sw.Close()

$jsonlSize = [math]::Round((Get-Item $trainingDataFile).Length / 1MB, 1)
Write-Host "   ✅ Saved: $trainingDataFile ($($jsonlSize) MB)" -ForegroundColor Green
Write-Host ""

# ─── SAVE MANIFEST ───────────────────────────────────────────────────
$manifest = @{
  created         = (Get-Date).ToString('o')
  drives          = $Drives
  totalProcessed  = $totalProcessed
  totalSkipped    = $totalSkipped
  trainingExamples = $allTrainingExamples.Count
  scanTimeMinutes = [math]::Round($scanElapsed.TotalMinutes, 1)
  driveStats      = $driveStats
  jsonlFile       = $trainingDataFile
  jsonlSizeMB     = $jsonlSize
  baseModel       = $BaseModel
  outputModel     = $OutputModelName
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $manifestFile -Encoding UTF8
Write-Host "📋 Manifest: $manifestFile" -ForegroundColor Green
Write-Host ""

# ─── BUILD MODELFILE ──────────────────────────────────────────────────
$sysPrompt = @"
You are an AI assistant with comprehensive knowledge of the user's ENTIRE machine across all drives (C, D, E, F, G).

## DEEP KNOWLEDGE OF:
- RawrXD IDE: Full Win32 C++ IDE with MASM x64, syntax highlighting, build system
- RawrZ Security: Powershield, payload builder, polymorphic encryption, AES-256-GCM
- Carmilla Encryption: AES-256-GCM with PBKDF2 key derivation
- 800B Model System: 5-drive distributed GGUF loader, NanoSlice memory, multi-engine inference
- BigDaddyG Projects: Custom Ollama models, agentic systems, swarm orchestration
- PowerShell Automation: RawrXD2.ps1, Powershield toolkit, build scripts
- Assembly / MASM x64: Anti-debug, polymorphic stubs, tensor streaming
- C/C++ Systems: Win32 API, memory management, performance optimization
- Python Tools: Drive digestion, autonomous agents, security research
- JavaScript/TypeScript: Electron apps, Node.js servers, React components
- System Architecture: Multi-drive arrays, distributed inference, GPU/CPU coordination

## DRIVE LAYOUT:
- C:\ — Windows system, program files, development tools
- D:\ — Primary development drive: RawrXD IDE, BigDaddyG projects, Powershield, models, temp/agentic work
- E:\ — Security research, GitHub repos, Carmilla encryption system
- F:\ — Additional storage and project backups
- G:\ — Security research repos, additional project storage

## RESPONSE STYLE:
- Reference specific files, paths, and functions from the user's codebase
- Provide solutions that integrate with their existing toolchain
- Use their coding conventions and patterns
- Include practical, copy-paste-ready examples
- Be concise and technical — the user is an expert reverse engineer and kernel specialist
"@

$modelfileContent = @"
# All-Drive Comprehensive Model — $OutputModelName
# Trained on: C:\ D:\ E:\ F:\ G:\ ($totalProcessed files)
# Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
FROM $BaseModel

SYSTEM """
$sysPrompt
"""

PARAMETER temperature 0.3
PARAMETER top_p 0.9
PARAMETER top_k 40
PARAMETER repeat_penalty 1.1
PARAMETER num_ctx 8192
PARAMETER num_predict 4096

PARAMETER stop "<|endoftext|>"
PARAMETER stop "</s>"
PARAMETER stop "# END"
"@

$modelfilePath = Join-Path $modelfileDir "$OutputModelName.Modelfile"
$modelfileContent | Out-File -FilePath $modelfilePath -Encoding UTF8
Write-Host "📝 Modelfile: $modelfilePath" -ForegroundColor Green
Write-Host ""

# ─── CREATE OLLAMA MODEL ─────────────────────────────────────────────
Write-Host "🚀 Creating Ollama model '$OutputModelName' ..." -ForegroundColor Magenta

try {
  $result = & ollama create $OutputModelName -f $modelfilePath 2>&1
  if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ Model '$OutputModelName' created!" -ForegroundColor Green
    Write-Host ""

    # Quick test
    Write-Host "🧪 Testing model ..." -ForegroundColor Yellow
    try {
      $body = @{
        model   = $OutputModelName
        prompt  = "What projects do I have across my C, D, E, F, G drives and how do they connect?"
        stream  = $false
        options = @{ temperature = 0.3; num_predict = 400 }
      } | ConvertTo-Json -Depth 3

      $resp = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method POST -Body $body -ContentType "application/json" -TimeoutSec 60
      Write-Host "✅ Test passed!" -ForegroundColor Green
      Write-Host ""
      Write-Host "Sample:" -ForegroundColor Cyan
      Write-Host $resp.response -ForegroundColor White
    } catch {
      Write-Host "⚠️ Created but test failed: $($_.Exception.Message)" -ForegroundColor Yellow
    }
  } else {
    Write-Host "❌ Creation failed: $result" -ForegroundColor Red
  }
} catch {
  Write-Host "❌ Error: $($_.Exception.Message)" -ForegroundColor Red
}

# ─── FINAL REPORT ────────────────────────────────────────────────────
Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  🎯  ALL-DRIVE TRAINING COMPLETE                            ║" -ForegroundColor Cyan
Write-Host "╠═══════════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║  Drives Scanned : C:\ D:\ E:\ F:\ G:\                      ║" -ForegroundColor Gray
Write-Host "║  Files Processed: $($totalProcessed.ToString().PadRight(42))║" -ForegroundColor Gray
Write-Host "║  Training Data  : $($jsonlSize.ToString().PadRight(39)) MB ║" -ForegroundColor Gray
Write-Host "║  Scan Time      : $("$([math]::Round($scanElapsed.TotalMinutes,1)) min".PadRight(42))║" -ForegroundColor Gray
Write-Host "║  Model Name     : $($OutputModelName.PadRight(42))║" -ForegroundColor Gray
Write-Host "║  Output Dir     : $($OutputDir.PadRight(42))║" -ForegroundColor Gray
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Usage:" -ForegroundColor Green
Write-Host "    ollama run $OutputModelName 'your question'" -ForegroundColor White
Write-Host "    .\chat-bigDaddyG.ps1 'your question'" -ForegroundColor White
Write-Host ""
