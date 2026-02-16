#!/usr/bin/env pwsh
param([string]$Action='enhanced',[string]$Mode='enhanced',[string]$Prompt="",[string[]]$Tasks=@("Analyze src/","Analyze scripts/"))
$act=switch($Action){'validate'{'validate'}'launch-assistant'{$Mode}'external-api'{'api'}'multi-agent'{'parallel'}'all'{& "$PSScriptRoot\RawrXD_Drive.ps1" -Action validate;'enhanced'}default{'enhanced'}}
$arg=@{Action=$act};if($Prompt){$arg['Prompt']=$Prompt};if($Tasks.Count -gt 0){$arg['Tasks']=$Tasks}
& "$PSScriptRoot\RawrXD_Drive.ps1" @arg
