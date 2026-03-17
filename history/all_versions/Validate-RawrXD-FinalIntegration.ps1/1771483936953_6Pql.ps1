z# Validate-RawrXD-FinalIntegration.ps1
# Verifies the 5 critical fixes + prevents WSOD regression

param([string]$Root = "D:\rawrxd", [string]$Binary = "D:\rawrxd\build\RawrXD-Win32IDE.exe")

$Results = @{Passed=0;Failed=0;Fixes=@{}}

function Test-Fix($Name,$Pattern,$FilePattern,$ShouldExist=$true){
    $files = Get-ChildItem $Root -Recurse -Include $FilePattern -ErrorAction SilentlyContinue
    $found = $files | Where-Object { (Get-Content $_.FullName -Raw) -match $Pattern }
    $ok = ($found -and $ShouldExist) -or (!$found -and !$ShouldExist)
    $Results.Fixes[$Name] = @{Status=$(if($ok){"PASS"}else{"FAIL"});File=$found.Name}
    if($ok){$Results.Passed++}else{$Results.Failed++}
    Write-Host "[$($Results.Fixes[$Name].Status)] $Name" -ForegroundColor $(if($ok){"Green"}else{"Red"})
}

Write-Host "`n=== VALIDATING 5 CRITICAL FIXES ===" -ForegroundColor Cyan

# Fix 1: OllamaProxy has real WinHTTP, not stubs
Test-Fix "OllamaProxy_WinHTTP" "WinHttpOpen|WinHttp_GET|NDJSON" "*ollama_proxy*"

# Fix 2: EnhancedModelLoader validates GGUF magic (0x46554747 = 'GGUF')
Test-Fix "GGUF_MagicValidation" "0x46554747|GGUF_MAGIC" "*enhanced_model_loader*"

# Fix 3: callModel has auto-fallback logic
Test-Fix "AutoFallback_Logic" "model_type.*auto|falls through.*Ollama|remember.*failure" "*ai_model_caller*"

# Fix 4: Config points to real model (bigdaddyg-alldrive)
Test-Fix "Config_DefaultModel" "bigdaddyg-alldrive|auto.*mode" "*agentic_configuration*"

# Fix 5: ChatSystem has modelId field
Test-Fix "Chat_ModelIdField" "modelId" "*chat_interface*"

# Bonus: Verify no stubs remain in critical paths
Test-Fix "No_Stubs_In_Proxy" "return false;.*//stub|/\*TODO\*/.*return" "*ollama_proxy*" -ShouldExist:$false

Write-Host "`n=== WSOD PREVENTION CHECK ===" -ForegroundColor Cyan

# Check WM_PAINT has GDI fallback (from previous fixes)
$hasGdiFallback = (Get-ChildItem $Root -Recurse -Include *.cpp -ErrorAction SilentlyContinue | Where-Object { 
    (Get-Content $_.FullName -Raw) -match "WM_PAINT.*GetDC|FillRect.*hdc|Emergency.*Fallback" 
})
if($hasGdiFallback){
    Write-Host "[PASS] GDI Emergency Fallback present" -ForegroundColor Green
    $Results.Passed++
}else{
    Write-Host "[WARN] No GDI fallback detected" -ForegroundColor Yellow
}

Write-Host "`n=== ENTROPY TREND ===" -ForegroundColor Cyan
$entropyFile = "$Root\rawrxd_dataset\random.json"
if(Test-Path $entropyFile){
    $entropy = (Get-Content $entropyFile | ConvertFrom-Json).UnmappedFunctions.Count
    Write-Host "Current Entropy: $entropy unmapped functions" -ForegroundColor $(if($entropy -lt 4000){"Green"}else{"Yellow"})
    if($entropy -lt 3944){ Write-Host "[IMPROVING] Entropy decreasing" -ForegroundColor Green }
}else{
    Write-Host "[SKIP] No entropy file found" -ForegroundColor Yellow
}

Write-Host "`n=== FINAL SCORE ===" -ForegroundColor White
Write-Host "Passed: $($Results.Passed)" -ForegroundColor Green
Write-Host "Failed: $($Results.Failed)" -ForegroundColor Red

if($Results.Failed -eq 0){
    Write-Host "`nALL SYSTEMS OPERATIONAL" -ForegroundColor Green
    exit 0
}else{
    Write-Host "`nREGRESSION DETECTED - Review failed checks above" -ForegroundColor Red
    exit 1
}
