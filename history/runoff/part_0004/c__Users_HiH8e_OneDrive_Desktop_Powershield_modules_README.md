# Powershield Modules: PoshLLM & AgenticFramework

## Overview
This directory contains two lightweight, from-scratch PowerShell modules designed for local experimentation and integration inside your IDE:

1. **PoshLLM** (`PoshLLM.psm1`) – Upgraded: a tiny GPT-style decoder (tokenizer, learned positional embeddings, multi-head causal self-attention, FFN, layer-norm, temperature top-k sampling). Trains on small corpora in seconds.
2. **AgenticFramework** (`AgenticFramework.psm1`) – A minimal agent loop scaffold (ReAct style) that wraps external model calls (e.g. Ollama) and provides tool registration / invocation.
3. **PoshVision** (`PoshVision.psm1`) – Local screenshot-to-text using Phi-3-vision ONNX via ONNX Runtime (CPU-only). Helps the IDE “see” screenshots and explain issues.

Both modules avoid external dependencies, so you can import them directly and start tinkering.

---
## PoshLLM Module (mini GPT)
### Key Functions
- `Initialize-PoshLLM -Name <string> -Corpus <string[]> [-Force]` – Trains a bigram model from the provided corpus.
- `Invoke-PoshLLM -Name <string> -Prompt <string> [-MaxTokens <int>]` – Generates text continuation.
- `Save-PoshLLM -Name <string> -Path <file>` – Persists the model counts as JSON.
- `Load-PoshLLM -Name <string> -Path <file>` – Reloads a saved model.

### Quick Start
```powershell
Import-Module "$PSScriptRoot/PoshLLM/PoshLLM.psd1"
$corpus = @(
  'hello world this is a test',
  'hello there world of powershell',
  'powershell makes scripting enjoyable'
)
Initialize-PoshLLM -Name demo -Corpus $corpus -Epochs 10
Invoke-PoshLLM -Name demo -Prompt 'hello' -MaxTokens 25 -Temperature 0.8 -TopK 10
```

### Extending
Add smoothing strategies, trigrams, or simple scoring heuristics by editing the class `PoshLLMModel`.

---
## AgenticFramework Module
### Key Functions
- `Set-AgentModel -Name <string>` – Sets the external model identifier (e.g. `llama3.2:1b` or HF reference).
- `Register-AgentTool -Name <string> -ScriptBlock { param($arg) ... }` – Adds a callable tool.
- `Get-AgentTools` – Lists registered tools.
- `Invoke-AgentTool -Name <string> -Arg <string>` – Manually invokes a tool.
- `Invoke-ExternalModel -Model <string> -Prompt <string>` – Raw CLI invocation wrapper.
- `Start-AgentLoop -Prompt <string> [-MaxTurns <int>]` – Runs a ReAct-style loop expecting `TOOL:` or `ANSWER:` responses.

### Quick Start
```powershell
Import-Module "$PSScriptRoot/AgenticFramework/AgenticFramework.psd1"
Set-AgentModel -Name 'hf.co/bartowski/Llama-3.2-1B-Instruct-GGUF:Q4_K_M'
# Register an extra custom tool
Register-AgentTool -Name time -ScriptBlock { (Get-Date).ToString('o') }
Start-AgentLoop -Prompt 'Add 2+3 and answer JSON' -MaxTurns 6 | Format-List
```
If the model outputs `TOOL:<name>:<arg>` the framework executes the tool and feeds the observation back. When it outputs `ANSWER:` the loop returns.

### Notes
- CLI is executed via `ollama run` – ensure Ollama CLI is installed and the model is available locally.
- Output parsing is naive (last line). You can improve by capturing full output and applying regex patterns or by shifting to an API client.

---
## Using Both Modules Together
You can use PoshLLM for synthetic corpus generation or prototyping prompt expansion fed into AgenticFramework:
```powershell
Import-Module "$PSScriptRoot/PoshLLM/PoshLLM.psd1"
Import-Module "$PSScriptRoot/AgenticFramework/AgenticFramework.psd1"

Initialize-PoshLLM -Name scratch -Corpus ('agent tools enhance capability','tools provide power','capability emerges from loops') -Epochs 5
$seed = (Invoke-PoshLLM -Name scratch -Prompt 'agent' -MaxTokens 10 -Temperature 0.9).Output
Start-AgentLoop -Prompt "Use reasoning: $seed" -MaxTurns 5

---
## PoshVision Module (Phi-3-vision ONNX)
### Setup
Place these next to `PoshVision.psm1`:
- `phi3v-384.onnx` (≈ 900 MB)
- `phi3v-tokenizer.json` (≈ 2 MB) and optional `phi3v-tokenizer.exe`

First run auto-downloads ONNX Runtime to `modules/PoshVision/onnx/...`.

### Quick Start
```powershell
Import-Module "$PSScriptRoot/PoshVision/PoshVision.psd1" -Force
Get-VisionAnswer -Prompt "Why is there a red underline?" | Format-List
```
Optionally pass `-ImagePath` to analyse a specific file; otherwise it captures the primary screen.
```

---
## IDE Tips
- Add this `modules` directory to your PowerShell module path during development:
```powershell
$env:PSModulePath = "$PSScriptRoot;$env:PSModulePath"
```
- For fast reloads while editing: `Remove-Module PoshLLM, AgenticFramework; Import-Module ./PoshLLM/PoshLLM.psd1, ./AgenticFramework/AgenticFramework.psd1`
- Use `Get-Command -Module PoshLLM` and `Get-Command -Module AgenticFramework` for discovery.

---
## 30-second smoke test (PowerShell 7+)
```powershell
# 1) Import modules
Import-Module "$PSScriptRoot/PoshLLM/PoshLLM.psd1" -Force
Import-Module "$PSScriptRoot/PoshVision/PoshVision.psd1" -Force  # optional; requires ONNX files

# 2) Tiny in-memory training
$corpus = @('hello world','power shell gpt','hello again')
Initialize-PoshLLM -Name mini -Corpus $corpus -VocabSize 65 -EmbedDim 64 -Layers 2 -Epochs 1 | Out-Null

# 3) Generate
Invoke-PoshLLM -Name mini -Prompt 'hello' -MaxTokens 20 -Temperature 0.8 | Format-List
```
You should see ~20 tokens. Loss will be high but trending down across epochs.

---
## Optional dot-method sugar (parser-safe)
Add this snippet to your `$PROFILE` or dot-source once after importing `PoshLLM`:
```powershell
Update-TypeData -TypeName MiniGPT -MemberName Generate -MemberType ScriptMethod -Value {
  param($Prompt, $MaxTokens=20, $Temperature=1.0, $TopK=10)
  Generate $this $Prompt $MaxTokens $Temperature $TopK
}
Update-TypeData -TypeName MiniGPT -MemberName Train -MemberType ScriptMethod -Value {
  param($Text, $Steps=50, $LearningRate=1e-3)
  Train-OneEpoch $this @($Text) 1 $LearningRate
}
```
Then you can call:
```powershell
$g = [MiniGPT]::new(64,64,4,2,128)
$g.Train('more text', 50, 1e-3)
$g.Generate('hello', 15, 0.7)
```
No class methods are defined in the module, so the PowerShell parser stays happy.

---
## Vision quick-start (once ONNX files are present)
```powershell
Import-Module "$PSScriptRoot/PoshVision/PoshVision.psd1" -Force
# Screen → text
Get-VisionAnswer -Prompt "What’s the error shown?"
# File → text
Get-VisionAnswer -Prompt "Describe" -ImagePath "$env:USERPROFILE\Pictures\demo.png"
```
First run auto-downloads ONNX Runtime CPU to `modules/PoshVision/onnx/...`.

---
## If anything still barks
Capture the exact error and share:
```powershell
$PSVersionTable.PSVersion
Get-Module PoshLLM,PoshVision -ErrorAction SilentlyContinue | Format-Table Name,Version,ExportedCommands
```
We’ll zero-in on the tensor op or tokenizer path that needs tightening.

## Roadmap Ideas
- Switch external model invocation to JSON streaming & structured tool calls.
- Add probability inspection & perplexity approximation for PoshLLM.
- Introduce memory vector store for AgenticFramework.
- Provide a test harness & Pester tests.

---
## Disclaimer
These modules are experimental and intentionally minimal. PoshLLM is not a substitute for production LLMs. AgenticFramework does not sandbox tools—review any tool ScriptBlocks for safety.

Enjoy exploring! 🎯
