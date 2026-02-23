# CrashRecoveryLauncher.ps1 - RawrXD IDE Recovery Mode
param([Parameter(Mandatory=$true)][string]$LogPath,[string]$DumpPath="",[string]$ExePath="",[string]$OllamaUrl="http://localhost:11434",[string]$Model="llama3.2",[switch]$ApplySafeConfig,[switch]$RestartSafe)
$ErrorActionPreference="Stop"
function Write-RecoveryLog{param([string]$Msg,[string]$Lvl="INFO");$line="["+(Get-Date -Format "yyyy-MM-dd HH:mm:ss")+"] [$Lvl] $Msg";New-Item -ItemType Directory -Force -Path "crash_dumps" -EA 0|Out-Null;Add-Content -Path "crash_dumps\recovery_launcher.log" -Value $line -EA 0;Write-Host $line}
function Invoke-OllamaAnalysis{param([string]$CrashContent);try{$body=@{model=$Model;prompt="Analyze crash, reply JSON: {"likely_cause":"brief","suggested_config":{"features.extensionSystem":false}}
---
$CrashContent";stream=$false}|ConvertTo-Json;$r=Invoke-RestMethod -Uri "$OllamaUrl/api/generate" -Method Post -ContentType "application/json" -Body $body -TimeoutSec 30;if($r.response -match '\{[\s\S]*\}'){return $Matches[0]|ConvertFrom-Json}}catch{Write-RecoveryLog "Ollama failed: " "ERROR"};return $null}
Write-RecoveryLog "Recovery launcher started. Log=$LogPath"
$crashContent=if(Test-Path $LogPath){Get-Content $LogPath -Raw}else{""}
$analysis=Invoke-OllamaAnalysis -CrashContent $crashContent
$cfg=@{"features.extensionSystem"=$false;"features.vulkanCompute"=$false;"performance.vulkanRenderer"=$false;"performance.gpuTextRendering"=$false}
if($analysis -and $analysis.suggested_config){$analysis.suggested_config.PSObject.Properties|%{$cfg[$_.Name]=$_.Value};Write-RecoveryLog "Agent: $($analysis.likely_cause)"}
$cfgPath=if($ExePath){Join-Path(Split-Path $ExePath -Parent)"rawrxd.config.json"}else{"rawrxd.config.json"}
if(Test-Path $cfgPath){$existing=Get-Content $cfgPath -Raw|ConvertFrom-Json;$existing.PSObject.Properties|%{if(-not $cfg.ContainsKey($_.Name)){$cfg[$_.Name]=$_.Value}}}
New-Item -ItemType Directory -Force -Path "crash_dumps"|Out-Null
@{timestamp=(Get-Date -Format "o");logPath=$LogPath;suggested_config=$cfg}|ConvertTo-Json -Depth 5|Set-Content "crash_dumps\recovery_suggestions.json"
if($ApplySafeConfig -or $RestartSafe){$nested=@{};foreach($k in $cfg.Keys){$p=$k -split '\.';$cur=$nested;for($i=0;$i -lt $p.Count-1;$i++){if(-not $cur[$p[$i]]){$cur[$p[$i]]=@{}};$cur=$cur[$p[$i]]};$cur[$p[-1]]=$cfg[$k]};$nested|ConvertTo-Json -Depth 10|Set-Content $cfgPath -Encoding UTF8;Write-RecoveryLog "Safe config applied"}
if($RestartSafe -and $ExePath -and (Test-Path $ExePath)){Start-Process $ExePath -ArgumentList "--safe-mode";Write-RecoveryLog "Launched safe mode"}
Write-RecoveryLog "Finished"
