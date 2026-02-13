# Test script for PoshLLM and AgenticFramework modules
Import-Module "$PSScriptRoot/PoshLLM/PoshLLM.psd1" -Force
Import-Module "$PSScriptRoot/AgenticFramework/AgenticFramework.psd1" -Force

Write-Host '--- PoshLLM Demo ---'
$corpus = @(
  'hello world this is a test',
  'hello there world of powershell',
  'powershell makes scripting enjoyable',
  'scripting in powershell feels powerful'
)
Initialize-PoshLLM -Name demo -Corpus $corpus | Out-Null
Invoke-PoshLLM -Name demo -Prompt 'hello' -MaxTokens 15 | Format-List

Write-Host '--- AgenticFramework Demo (simple) ---'
Set-AgentModel -Name 'hf.co/bartowski/Llama-3.2-1B-Instruct-GGUF:Q4_K_M'
Register-AgentTool -Name upper -ScriptBlock { param($t) $t.ToUpper() } | Out-Null
Start-AgentLoop -Prompt 'Transform the string hello via TOOL upper then ANSWER the result.' -MaxTurns 6 | Format-List
