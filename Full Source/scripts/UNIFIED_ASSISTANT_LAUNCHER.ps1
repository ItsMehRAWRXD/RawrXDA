#!/usr/bin/env pwsh
param([ValidateSet('enhanced','voice','digest-only')][string]$Mode='enhanced',[switch]$EnableVoice,[switch]$EnableExternalAPI,[switch]$DigestFirst)
$a=if($Mode -eq 'digest-only'){'digest'}else{$Mode}
& "$PSScriptRoot\RawrXD_Drive.ps1" -Action $a -EnableVoice:$EnableVoice.IsPresent -EnableExternalAPI:$EnableExternalAPI.IsPresent -DigestFirst:$DigestFirst.IsPresent
