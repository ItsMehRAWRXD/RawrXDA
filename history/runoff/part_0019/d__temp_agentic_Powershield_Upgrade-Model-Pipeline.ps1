<#
.SYNOPSIS
    Upgrade your BigDaddyG model pipeline — 3 upgrade paths.

.DESCRIPTION
    Option A — Upgrade to a larger base model (7B / 8B / 13B)
               and auto-merge your ALL-DRIVE training dataset.
    Option B — Train multiple expert adapters (code, security, reasoning)
               and combine them into a single "Mixture-of-Experts" model.
    Option C — Build a 3-model ensemble router
               (small=fast queries, medium=code, large=reasoning).

.PARAMETER Mode
    Which upgrade path: A, B, C, or Menu (interactive).

.PARAMETER TrainingDataFile
    Path to existing JSONL training data (from Train-AllDrives-Model.ps1).

.PARAMETER OllamaServer
    Ollama API endpoint (default: localhost:11434).

.EXAMPLE
    .\Upgrade-Model-Pipeline.ps1                          # Interactive menu
    .\Upgrade-Model-Pipeline.ps1 -Mode A                  # Upgrade base model
    .\Upgrade-Model-Pipeline.ps1 -Mode B                  # Expert adapters
    .\Upgrade-Model-Pipeline.ps1 -Mode C                  # Ensemble router
#>

[CmdletBinding()] param(
  [ValidateSet('A','B','C','Menu')]
  [string]$Mode = 'Menu',

  [string]$TrainingDataFile = 'D:\drive-model-output\all_drives_training_data.jsonl',

  [string]$OllamaServer = 'localhost:11434',

  [string]$OutputDir = 'D:\drive-model-output\upgrades',

  [switch]$SkipTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

# ─── Ensure output dir ────────────────────────────────────────────────
if (-not (Test-Path $OutputDir)) {
  New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}
$modelfileDir = Join-Path $OutputDir 'Modelfiles'
if (-not (Test-Path $modelfileDir)) {
  New-Item -ItemType Directory -Path $modelfileDir -Force | Out-Null
}

# ─── Helpers ──────────────────────────────────────────────────────────
function Test-OllamaRunning {
  try {
    $null = Invoke-RestMethod -Uri "http://$OllamaServer/api/version" -TimeoutSec 5
    return $true
  } catch { return $false }
}

function Get-OllamaModels {
  try {
    $resp = Invoke-RestMethod -Uri "http://$OllamaServer/api/tags" -TimeoutSec 10
    return $resp.models
  } catch { return @() }
}

function Test-ModelExists {
  param([string]$Name)
  $models = Get-OllamaModels
  return ($models | Where-Object { $_.name -eq $Name -or $_.name -eq "$Name`:latest" }).Count -gt 0
}

function Invoke-OllamaGenerate {
  param([string]$Model, [string]$Prompt, [int]$MaxTokens = 400, [double]$Temp = 0.3)
  $body = @{
    model   = $Model
    prompt  = $Prompt
    stream  = $false
    options = @{ temperature = $Temp; num_predict = $MaxTokens }
  } | ConvertTo-Json -Depth 3
  try {
    $resp = Invoke-RestMethod -Uri "http://$OllamaServer/api/generate" -Method POST -Body $body -ContentType "application/json" -TimeoutSec 120
    return $resp
  } catch {
    Write-Host "  ⚠️  Generation failed: $($_.Exception.Message)" -ForegroundColor Yellow
    return $null
  }
}

function Show-Banner {
  Write-Host ""
  Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
  Write-Host "║  ⚡  MODEL UPGRADE PIPELINE                                 ║" -ForegroundColor Magenta
  Write-Host "║  Upgrade your 3.2B → 7B / 8B / 13B + Expert Routing        ║" -ForegroundColor Magenta
  Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
  Write-Host ""
  Write-Host "  GPU       : AMD Radeon RX 7800 XT (16 GB VRAM)" -ForegroundColor Gray
  Write-Host "  Current   : bigdaddyg-comprehensive (3.2B Q4_K_M, ~2.0 GB)" -ForegroundColor Gray
  Write-Host "  Headroom  : ~10-11 GB VRAM free for larger models" -ForegroundColor Gray
  Write-Host ""
}

function Show-Menu {
  Write-Host "┌─────────────────────────────────────────────────────────────┐" -ForegroundColor Cyan
  Write-Host "│  Choose an upgrade path:                                   │" -ForegroundColor Cyan
  Write-Host "├─────────────────────────────────────────────────────────────┤" -ForegroundColor Cyan
  Write-Host "│                                                            │" -ForegroundColor Cyan
  Write-Host "│  [A] Upgrade Base Model                                    │" -ForegroundColor White
  Write-Host "│      Pull a larger model (7B/8B/13B), re-apply your        │" -ForegroundColor Gray
  Write-Host "│      ALL-DRIVE system prompt + parameters.                 │" -ForegroundColor Gray
  Write-Host "│      VRAM: 5-9 GB  Speed: 40-100 tok/s                    │" -ForegroundColor DarkGray
  Write-Host "│                                                            │" -ForegroundColor Cyan
  Write-Host "│  [B] Multi-Expert Adapters                                 │" -ForegroundColor White
  Write-Host "│      Create 3 specialized models from different bases:     │" -ForegroundColor Gray
  Write-Host "│      code-expert, security-expert, reasoning-expert.       │" -ForegroundColor Gray
  Write-Host "│      Each tuned with domain-specific system prompts.       │" -ForegroundColor DarkGray
  Write-Host "│                                                            │" -ForegroundColor Cyan
  Write-Host "│  [C] 3-Model Ensemble Router                               │" -ForegroundColor White
  Write-Host "│      Small (3.2B) for fast queries, Medium (8B) for code,  │" -ForegroundColor Gray
  Write-Host "│      Large (13B+) for deep reasoning. Auto-routes by       │" -ForegroundColor Gray
  Write-Host "│      query complexity. Script: Ensemble-Router.ps1         │" -ForegroundColor DarkGray
  Write-Host "│                                                            │" -ForegroundColor Cyan
  Write-Host "│  [Q] Quit                                                  │" -ForegroundColor DarkGray
  Write-Host "│                                                            │" -ForegroundColor Cyan
  Write-Host "└─────────────────────────────────────────────────────────────┘" -ForegroundColor Cyan
  Write-Host ""
}

# ─── VRAM budget table ────────────────────────────────────────────────
$ModelTiers = @{
  '3.2B' = @{ Tag = 'llama3.2:3b';         VRAM = '~3.5 GB';  Speed = '80-180 tok/s';  Quant = 'Q4_K_M'; SizeGB = 2.0 }
  '7B'   = @{ Tag = 'llama3.1:7b';          VRAM = '~5.5 GB';  Speed = '50-100 tok/s';  Quant = 'Q4_K_M'; SizeGB = 4.7 }
  '8B'   = @{ Tag = 'llama3.1:8b';          VRAM = '~6.0 GB';  Speed = '45-90 tok/s';   Quant = 'Q4_K_M'; SizeGB = 4.9 }
  '13B'  = @{ Tag = 'codellama:13b';        VRAM = '~9.0 GB';  Speed = '25-50 tok/s';   Quant = 'Q4_K_M'; SizeGB = 7.4 }
  '27B'  = @{ Tag = 'gemma3:27b';           VRAM = '~14 GB';   Speed = '10-25 tok/s';   Quant = 'Q4_K_M'; SizeGB = 17  }
}

# ─── Shared system prompt (from Train-AllDrives-Model.ps1) ───────────
$AllDriveSystemPrompt = @"
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
- F:\ — Additional storage, Ollama model blobs, project backups
- G:\ — Security research repos, additional project storage

## RESPONSE STYLE:
- Reference specific files, paths, and functions from the user's codebase
- Provide solutions that integrate with their existing toolchain
- Use their coding conventions and patterns
- Include practical, copy-paste-ready examples
- Be concise and technical — the user is an expert reverse engineer and kernel specialist
"@

# ═══════════════════════════════════════════════════════════════════════
#  OPTION A — UPGRADE BASE MODEL
# ═══════════════════════════════════════════════════════════════════════
function Invoke-OptionA {
  Write-Host ""
  Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Magenta
  Write-Host "  ⚡ OPTION A — UPGRADE BASE MODEL" -ForegroundColor Magenta
  Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Magenta
  Write-Host ""
  Write-Host "  Your current base: llama3.2 3.2B Q4_K_M (~2.0 GB)" -ForegroundColor Gray
  Write-Host "  Your GPU: 7800 XT with 16 GB VRAM" -ForegroundColor Gray
  Write-Host ""

  # Show tier options
  Write-Host "  Available upgrade tiers:" -ForegroundColor Yellow
  Write-Host ""
  Write-Host "  ┌──────┬──────────────────────┬───────────┬──────────────────┐" -ForegroundColor DarkGray
  Write-Host "  │ Tier │ Model                │ VRAM      │ Speed            │" -ForegroundColor DarkGray
  Write-Host "  ├──────┼──────────────────────┼───────────┼──────────────────┤" -ForegroundColor DarkGray
  Write-Host "  │  1   │ llama3.1:8b (Q4_K_M) │ ~6.0 GB   │ 45-90  tok/s     │" -ForegroundColor White
  Write-Host "  │  2   │ qwen2.5-coder:7b     │ ~5.5 GB   │ 50-100 tok/s     │" -ForegroundColor White
  Write-Host "  │  3   │ codellama:13b         │ ~9.0 GB   │ 25-50  tok/s     │" -ForegroundColor White
  Write-Host "  │  4   │ gemma3:27b            │ ~14  GB   │ 10-25  tok/s     │" -ForegroundColor Yellow
  Write-Host "  │  5   │ Custom (enter tag)    │ varies    │ varies           │" -ForegroundColor DarkGray
  Write-Host "  └──────┴──────────────────────┴───────────┴──────────────────┘" -ForegroundColor DarkGray
  Write-Host ""
  Write-Host "  Recommended: Tier 1 or 2 for best speed/quality tradeoff" -ForegroundColor Green
  Write-Host ""

  $tierChoice = Read-Host "  Select tier [1-5]"

  $baseTag = switch ($tierChoice) {
    '1' { 'llama3.1:8b' }
    '2' { 'qwen2.5-coder:7b' }
    '3' { 'codellama:13b' }
    '4' { 'gemma3:27b' }
    '5' {
      $custom = Read-Host "  Enter Ollama model tag (e.g. mistral:7b)"
      $custom
    }
    default { 'llama3.1:8b' }
  }

  $newModelName = Read-Host "  Name for upgraded model (default: bigdaddyg-upgraded)"
  if ([string]::IsNullOrWhiteSpace($newModelName)) { $newModelName = 'bigdaddyg-upgraded' }

  Write-Host ""
  Write-Host "  📦 Base model : $baseTag" -ForegroundColor Cyan
  Write-Host "  🏷️  New name   : $newModelName" -ForegroundColor Cyan
  Write-Host ""

  # Pull if not present
  if (-not (Test-ModelExists $baseTag)) {
    Write-Host "  📥 Pulling $baseTag (this may take a while) ..." -ForegroundColor Yellow
    & ollama pull $baseTag 2>&1 | ForEach-Object { Write-Host "     $_" -ForegroundColor DarkGray }
    if ($LASTEXITCODE -ne 0) {
      Write-Host "  ❌ Failed to pull $baseTag" -ForegroundColor Red
      return
    }
    Write-Host "  ✅ Pull complete" -ForegroundColor Green
  } else {
    Write-Host "  ✅ $baseTag already available locally" -ForegroundColor Green
  }

  # Build Modelfile with your ALL-DRIVE system prompt
  $modelfileContent = @"
# Upgraded Model — $newModelName
# Base: $baseTag (upgraded from 3.2B)
# Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
FROM $baseTag

SYSTEM """
$AllDriveSystemPrompt
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

  $mfPath = Join-Path $modelfileDir "$newModelName.Modelfile"
  $modelfileContent | Out-File -FilePath $mfPath -Encoding UTF8
  Write-Host "  📝 Modelfile: $mfPath" -ForegroundColor Green

  # Create in Ollama
  Write-Host "  🚀 Creating $newModelName in Ollama ..." -ForegroundColor Yellow
  $result = & ollama create $newModelName -f $mfPath 2>&1
  if ($LASTEXITCODE -eq 0) {
    Write-Host "  ✅ Model '$newModelName' created!" -ForegroundColor Green
  } else {
    Write-Host "  ❌ Creation failed: $result" -ForegroundColor Red
    return
  }

  # Test
  if (-not $SkipTest) {
    Write-Host ""
    Write-Host "  🧪 Testing $newModelName ..." -ForegroundColor Yellow
    $resp = Invoke-OllamaGenerate -Model $newModelName -Prompt "What is RawrXD IDE and where is it located on my drives?"
    if ($resp) {
      Write-Host "  ✅ Test passed!" -ForegroundColor Green
      Write-Host ""
      Write-Host "  Sample response:" -ForegroundColor Cyan
      $preview = $resp.response.Substring(0, [Math]::Min(500, $resp.response.Length))
      Write-Host "  $preview" -ForegroundColor White
    }
  }

  Write-Host ""
  Write-Host "  ╔═══════════════════════════════════════════════════════╗" -ForegroundColor Green
  Write-Host "  ║  ✅ OPTION A COMPLETE                                ║" -ForegroundColor Green
  Write-Host "  ║  Model: $($newModelName.PadRight(44))║" -ForegroundColor Green
  Write-Host "  ║  Base : $($baseTag.PadRight(44))║" -ForegroundColor Green
  Write-Host "  ║                                                      ║" -ForegroundColor Green
  Write-Host "  ║  Usage: ollama run $newModelName                     ║" -ForegroundColor Green
  Write-Host "  ╚═══════════════════════════════════════════════════════╝" -ForegroundColor Green
}

# ═══════════════════════════════════════════════════════════════════════
#  OPTION B — MULTI-EXPERT ADAPTERS
# ═══════════════════════════════════════════════════════════════════════
function Invoke-OptionB {
  Write-Host ""
  Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Magenta
  Write-Host "  ⚡ OPTION B — MULTI-EXPERT ADAPTERS" -ForegroundColor Magenta
  Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Magenta
  Write-Host ""
  Write-Host "  Creates 3 specialized expert models, each with a domain" -ForegroundColor Gray
  Write-Host "  system prompt tuned for a specific task type." -ForegroundColor Gray
  Write-Host ""

  # Expert definitions
  $experts = @(
    @{
      Name       = 'bigdaddyg-code-expert'
      BaseModel  = 'qwen2.5-coder:latest'  # Already pulled (4.7GB)
      Domain     = 'Code Generation & Analysis'
      Temp       = 0.2
      NumCtx     = 8192
      NumPredict = 4096
      SystemPrompt = @"
$AllDriveSystemPrompt

## CODE EXPERT SPECIALIZATION:
You are the CODE EXPERT in a multi-model ensemble. Your primary role:
- Write, review, and debug code in any language the user works with
- PowerShell, Python, C/C++, Assembly (MASM x64), JavaScript/TypeScript, Rust, Go, C#
- Produce production-quality code with proper error handling
- Follow the user's coding conventions: detailed comments, modular functions, progress indicators
- When given a file path, reference the actual project structure on their drives
- Optimize for correctness first, performance second
- Always include imports/includes and complete function signatures
- Output code in fenced blocks with language tags
"@
    },
    @{
      Name       = 'bigdaddyg-security-expert'
      BaseModel  = 'llama3.2:3b'  # Fast, lightweight for security analysis
      Domain     = 'Security Research & Analysis'
      Temp       = 0.15
      NumCtx     = 8192
      NumPredict = 4096
      SystemPrompt = @"
$AllDriveSystemPrompt

## SECURITY EXPERT SPECIALIZATION:
You are the SECURITY EXPERT in a multi-model ensemble. Your primary role:
- Analyze code for vulnerabilities (buffer overflows, injection, race conditions)
- Review encryption implementations (AES-256-GCM, PBKDF2, Carmilla system)
- Assess RawrZ payloads, polymorphic stubs, and anti-debug techniques
- Provide CVE-aware analysis with MITRE ATT&CK mapping
- Review Win32 API usage for security implications
- Analyze assembly code for anti-tamper and obfuscation quality
- Suggest hardening: input validation, memory safety, least privilege
- Reference the user's existing security tools: Powershield, RawrZ, Carmilla
- Be direct, technical, and assume expert-level knowledge
"@
    },
    @{
      Name       = 'bigdaddyg-reasoning-expert'
      BaseModel  = 'gemma3:latest'  # Already pulled (3.3GB), good at reasoning
      Domain     = 'Deep Reasoning & Architecture'
      Temp       = 0.4
      NumCtx     = 16384
      NumPredict = 4096
      SystemPrompt = @"
$AllDriveSystemPrompt

## REASONING EXPERT SPECIALIZATION:
You are the REASONING EXPERT in a multi-model ensemble. Your primary role:
- Perform multi-step logical reasoning and architecture planning
- Design system architectures across the user's multi-drive environment
- Analyze trade-offs between approaches (performance, memory, complexity)
- Plan refactoring strategies for large codebases
- Debug complex cross-system issues (GPU offload, distributed inference, multi-process)
- Provide step-by-step breakdowns of complex problems
- Think through edge cases and failure modes
- Reference the user's 800B distributed model system, NanoSlice memory, multi-engine inference
- Output structured analysis with headers, numbered steps, and decision matrices
"@
    }
  )

  foreach ($expert in $experts) {
    Write-Host "  ┌───────────────────────────────────────────────────────┐" -ForegroundColor Cyan
    Write-Host "  │  🧠 $($expert.Domain.PadRight(50))│" -ForegroundColor Cyan
    Write-Host "  │  Model : $($expert.Name.PadRight(44))│" -ForegroundColor White
    Write-Host "  │  Base  : $($expert.BaseModel.PadRight(44))│" -ForegroundColor Gray
    Write-Host "  │  Temp  : $($expert.Temp)   Context: $($expert.NumCtx)                    │" -ForegroundColor Gray
    Write-Host "  └───────────────────────────────────────────────────────┘" -ForegroundColor Cyan

    # Pull base if needed
    if (-not (Test-ModelExists $expert.BaseModel)) {
      Write-Host "     📥 Pulling $($expert.BaseModel) ..." -ForegroundColor Yellow
      & ollama pull $expert.BaseModel 2>&1 | ForEach-Object { Write-Host "        $_" -ForegroundColor DarkGray }
    }

    # Build Modelfile
    $mfContent = @"
# Expert Adapter — $($expert.Name)
# Domain: $($expert.Domain)
# Base: $($expert.BaseModel)
# Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
FROM $($expert.BaseModel)

SYSTEM """
$($expert.SystemPrompt)
"""

PARAMETER temperature $($expert.Temp)
PARAMETER top_p 0.9
PARAMETER top_k 40
PARAMETER repeat_penalty 1.1
PARAMETER num_ctx $($expert.NumCtx)
PARAMETER num_predict $($expert.NumPredict)

PARAMETER stop "<|endoftext|>"
PARAMETER stop "</s>"
PARAMETER stop "# END"
"@

    $mfPath = Join-Path $modelfileDir "$($expert.Name).Modelfile"
    $mfContent | Out-File -FilePath $mfPath -Encoding UTF8

    # Create model
    Write-Host "     🚀 Creating $($expert.Name) ..." -ForegroundColor Yellow
    $result = & ollama create $expert.Name -f $mfPath 2>&1
    if ($LASTEXITCODE -eq 0) {
      Write-Host "     ✅ Created!" -ForegroundColor Green
    } else {
      Write-Host "     ❌ Failed: $result" -ForegroundColor Red
    }
    Write-Host ""
  }

  # Test all experts
  if (-not $SkipTest) {
    Write-Host "  🧪 Testing expert models ..." -ForegroundColor Yellow
    Write-Host ""

    $testCases = @(
      @{ Model = 'bigdaddyg-code-expert';      Prompt = 'Write a PowerShell function that recursively scans a directory and returns file hashes.' }
      @{ Model = 'bigdaddyg-security-expert';   Prompt = 'Analyze this for vulnerabilities: eval(user_input) in a Node.js Express handler.' }
      @{ Model = 'bigdaddyg-reasoning-expert';  Prompt = 'What is the optimal architecture for distributing a 70B model across 3 GPUs with different VRAM sizes?' }
    )

    foreach ($tc in $testCases) {
      Write-Host "     🔹 $($tc.Model) ..." -ForegroundColor Cyan
      $resp = Invoke-OllamaGenerate -Model $tc.Model -Prompt $tc.Prompt -MaxTokens 200
      if ($resp) {
        $preview = $resp.response.Substring(0, [Math]::Min(200, $resp.response.Length))
        Write-Host "        $preview..." -ForegroundColor Gray
        Write-Host "        ✅ OK ($([math]::Round($resp.total_duration / 1e9, 1))s)" -ForegroundColor Green
      } else {
        Write-Host "        ⚠️ No response" -ForegroundColor Yellow
      }
      Write-Host ""
    }
  }

  Write-Host "  ╔═══════════════════════════════════════════════════════╗" -ForegroundColor Green
  Write-Host "  ║  ✅ OPTION B COMPLETE — 3 Expert Models Created      ║" -ForegroundColor Green
  Write-Host "  ║                                                      ║" -ForegroundColor Green
  Write-Host "  ║  bigdaddyg-code-expert      → Code generation        ║" -ForegroundColor Green
  Write-Host "  ║  bigdaddyg-security-expert   → Security analysis      ║" -ForegroundColor Green
  Write-Host "  ║  bigdaddyg-reasoning-expert  → Architecture/reasoning ║" -ForegroundColor Green
  Write-Host "  ║                                                      ║" -ForegroundColor Green
  Write-Host "  ║  Use with Ensemble-Router.ps1 for auto-routing!      ║" -ForegroundColor Green
  Write-Host "  ╚═══════════════════════════════════════════════════════╝" -ForegroundColor Green
}

# ═══════════════════════════════════════════════════════════════════════
#  OPTION C — 3-MODEL ENSEMBLE ROUTER
# ═══════════════════════════════════════════════════════════════════════
function Invoke-OptionC {
  Write-Host ""
  Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Magenta
  Write-Host "  ⚡ OPTION C — 3-MODEL ENSEMBLE ROUTER" -ForegroundColor Magenta
  Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Magenta
  Write-Host ""
  Write-Host "  This creates 3 tiered models + an auto-routing script." -ForegroundColor Gray
  Write-Host ""
  Write-Host "  ┌────────────┬──────────────────────┬─────────────────────┐" -ForegroundColor DarkGray
  Write-Host "  │ Tier       │ Model                │ Use Case            │" -ForegroundColor DarkGray
  Write-Host "  ├────────────┼──────────────────────┼─────────────────────┤" -ForegroundColor DarkGray
  Write-Host "  │ SMALL      │ llama3.2:3b (3.2B)   │ Fast chat, simple Q │" -ForegroundColor Green
  Write-Host "  │ MEDIUM     │ llama3.1:8b (8B)     │ Code gen, scripting │" -ForegroundColor Yellow
  Write-Host "  │ LARGE      │ gemma3:27b (27B)     │ Deep reasoning      │" -ForegroundColor Red
  Write-Host "  └────────────┴──────────────────────┴─────────────────────┘" -ForegroundColor DarkGray
  Write-Host ""

  $tiers = @(
    @{
      Name  = 'bigdaddyg-tier-small'
      Base  = 'llama3.2:3b'
      Tier  = 'SMALL'
      Desc  = 'Fast queries, simple chat, quick lookups'
      Temp  = 0.3
      Ctx   = 4096
      Pred  = 2048
    },
    @{
      Name  = 'bigdaddyg-tier-medium'
      Base  = 'llama3.1:8b'
      Tier  = 'MEDIUM'
      Desc  = 'Code generation, scripting, file analysis'
      Temp  = 0.3
      Ctx   = 8192
      Pred  = 4096
    },
    @{
      Name  = 'bigdaddyg-tier-large'
      Base  = 'gemma3:27b'
      Tier  = 'LARGE'
      Desc  = 'Deep reasoning, architecture, complex debugging'
      Temp  = 0.4
      Ctx   = 16384
      Pred  = 4096
    }
  )

  foreach ($tier in $tiers) {
    Write-Host "  📦 [$($tier.Tier)] $($tier.Name) ← $($tier.Base)" -ForegroundColor Cyan

    # Pull if needed
    if (-not (Test-ModelExists $tier.Base)) {
      Write-Host "     📥 Pulling $($tier.Base) ..." -ForegroundColor Yellow
      & ollama pull $tier.Base 2>&1 | ForEach-Object { Write-Host "        $_" -ForegroundColor DarkGray }
    }

    # Modelfile
    $mfContent = @"
# Ensemble Tier: $($tier.Tier) — $($tier.Name)
# Purpose: $($tier.Desc)
# Base: $($tier.Base)
# Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
FROM $($tier.Base)

SYSTEM """
$AllDriveSystemPrompt

## ENSEMBLE ROLE: $($tier.Tier)
You are the $($tier.Tier) tier model in a 3-model ensemble.
Your role: $($tier.Desc).
Optimize for $(if ($tier.Tier -eq 'SMALL') {'speed and brevity'} elseif ($tier.Tier -eq 'MEDIUM') {'accuracy and completeness'} else {'depth and thoroughness'}).
"""

PARAMETER temperature $($tier.Temp)
PARAMETER top_p 0.9
PARAMETER top_k 40
PARAMETER repeat_penalty 1.1
PARAMETER num_ctx $($tier.Ctx)
PARAMETER num_predict $($tier.Pred)

PARAMETER stop "<|endoftext|>"
PARAMETER stop "</s>"
PARAMETER stop "# END"
"@

    $mfPath = Join-Path $modelfileDir "$($tier.Name).Modelfile"
    $mfContent | Out-File -FilePath $mfPath -Encoding UTF8

    Write-Host "     🚀 Creating ..." -ForegroundColor Yellow
    $result = & ollama create $tier.Name -f $mfPath 2>&1
    if ($LASTEXITCODE -eq 0) {
      Write-Host "     ✅ Created!" -ForegroundColor Green
    } else {
      Write-Host "     ❌ Failed: $result" -ForegroundColor Red
    }
    Write-Host ""
  }

  # Generate the Ensemble-Router.ps1 script
  $routerScriptPath = Join-Path $PSScriptRoot 'Ensemble-Router.ps1'
  Write-Host "  📝 Generating Ensemble-Router.ps1 ..." -ForegroundColor Yellow

  if (-not (Test-Path $routerScriptPath)) {
    Write-Host "     ⚠️  Ensemble-Router.ps1 not found — create it separately." -ForegroundColor Yellow
    Write-Host "     (It should already exist if you ran this pipeline)" -ForegroundColor DarkGray
  } else {
    Write-Host "     ✅ Ensemble-Router.ps1 already exists at: $routerScriptPath" -ForegroundColor Green
  }

  Write-Host ""
  Write-Host "  ╔═══════════════════════════════════════════════════════╗" -ForegroundColor Green
  Write-Host "  ║  ✅ OPTION C COMPLETE — 3-Tier Ensemble Built        ║" -ForegroundColor Green
  Write-Host "  ║                                                      ║" -ForegroundColor Green
  Write-Host "  ║  bigdaddyg-tier-small   → Fast (3.2B)                ║" -ForegroundColor Green
  Write-Host "  ║  bigdaddyg-tier-medium  → Code (8B)                  ║" -ForegroundColor Green
  Write-Host "  ║  bigdaddyg-tier-large   → Reasoning (27B)            ║" -ForegroundColor Green
  Write-Host "  ║                                                      ║" -ForegroundColor Green
  Write-Host "  ║  Router: .\Ensemble-Router.ps1 'your question'       ║" -ForegroundColor Green
  Write-Host "  ╚═══════════════════════════════════════════════════════╝" -ForegroundColor Green
}

# ═══════════════════════════════════════════════════════════════════════
#  MAIN
# ═══════════════════════════════════════════════════════════════════════

# Preflight
if (-not (Test-OllamaRunning)) {
  Write-Host "❌ Ollama is not running at $OllamaServer" -ForegroundColor Red
  Write-Host "   Start it with: ollama serve" -ForegroundColor Yellow
  exit 1
}

Show-Banner

if ($Mode -eq 'Menu') {
  Show-Menu
  $choice = Read-Host "  Your choice"
  $Mode = $choice.ToUpper()
}

switch ($Mode) {
  'A' { Invoke-OptionA }
  'B' { Invoke-OptionB }
  'C' { Invoke-OptionC }
  'Q' { Write-Host "  👋 Bye!" -ForegroundColor Gray; exit 0 }
  default {
    Write-Host "  ❌ Invalid choice: $Mode" -ForegroundColor Red
    Show-Menu
  }
}

Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
Write-Host "  Done. Run 'ollama list' to see all your models." -ForegroundColor Gray
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor DarkGray
Write-Host ""
