# RawrXD.ModelLoader.psm1
# PowerShell scaffold for model loading and routing across backends

$script:ModelLoaderConfigPath = Join-Path (Get-RawrXDRootPath) 'model_config.json'

function Get-RawrXDModelConfig {
    if (-not (Test-Path $script:ModelLoaderConfigPath)) {
        return @{ models = @{}; defaults = @{} }
    }
    $json = Get-Content $script:ModelLoaderConfigPath -Raw | ConvertFrom-Json
    return $json
}

function Resolve-RawrXDSecrets {
    param([string]$Value)
    if (-not $Value) { return $Value }
    return ($Value -replace '\$\{([^}]+)\}', {
        param($m)
        $name = $m.Groups[1].Value
        $envValue = [Environment]::GetEnvironmentVariable($name)
        if ($envValue) { return $envValue }
        return "${$name}"
    })
}

function Get-RawrXDModelDefinition {
    param([string]$ModelName)
    $config = Get-RawrXDModelConfig
    if (-not $config.models.$ModelName) { return $null }
    $model = $config.models.$ModelName
    $model.api_key = Resolve-RawrXDSecrets $model.api_key
    $model.endpoint = Resolve-RawrXDSecrets $model.endpoint
    return $model
}

function Test-RawrXDModelBackend {
    param([string]$ModelName)
    $model = Get-RawrXDModelDefinition -ModelName $ModelName
    if (-not $model) { return $false }

    switch ($model.backend) {
        'LOCAL_GGUF' {
            return (Test-Path $model.model_id)
        }
        'OLLAMA_LOCAL' {
            try {
                $tags = Invoke-RestMethod -Uri "$($model.endpoint)/api/tags" -Method Get -TimeoutSec 5
                return $true
            } catch { return $false }
        }
        default {
            return $true
        }
    }
}

function Invoke-RawrXDModelRequest {
    param(
        [Parameter(Mandatory = $true)][string]$ModelName,
        [string]$Prompt,
        [object[]]$Messages = $null,
        [double]$Temperature = 0.7,
        [int]$MaxTokens = 2048
    )

    $model = Get-RawrXDModelDefinition -ModelName $ModelName
    if (-not $model) {
        return @{ Success = $false; Error = "Model not found: $ModelName" }
    }

    $context = @{ Model = $ModelName; Backend = $model.backend }
    return Invoke-RawrXDSafeOperation -Name 'Invoke-RawrXDModelRequest' -Context $context -Script {
        $headers = @{ 'Content-Type' = 'application/json' }
        if ($model.api_key) { $headers['Authorization'] = "Bearer $($model.api_key)" }

        switch ($model.backend) {
            'OLLAMA_LOCAL' {
                $payload = @{ 
                    model = $model.model_id
                    prompt = $Prompt
                    stream = $false
                    options = @{ temperature = $Temperature }
                }
                $response = Invoke-RestMethod -Uri "$($model.endpoint)/api/generate" -Method Post -Body ($payload | ConvertTo-Json) -ContentType 'application/json'
                return @{ Success = $true; Output = $response.response; RawResponse = $response }
            }
            
            'OPENAI_COMPATIBLE' {
                $payload = @{
                    model = $model.model_id
                    messages = if ($Messages) { $Messages } else { @(@{ role = 'user'; content = $Prompt }) }
                    temperature = $Temperature
                    max_tokens = $MaxTokens
                }
                $response = Invoke-RestMethod -Uri "$($model.endpoint)/v1/chat/completions" -Method Post -Headers $headers -Body ($payload | ConvertTo-Json -Depth 10)
                return @{ Success = $true; Output = $response.choices[0].message.content; RawResponse = $response }
            }
            
            'ANTHROPIC_CLAUDE' {
                $headers['x-api-key'] = $model.api_key
                $headers['anthropic-version'] = '2023-06-01'
                $payload = @{
                    model = $model.model_id
                    messages = if ($Messages) { $Messages } else { @(@{ role = 'user'; content = $Prompt }) }
                    temperature = $Temperature
                    max_tokens = $MaxTokens
                }
                $response = Invoke-RestMethod -Uri "$($model.endpoint)/v1/messages" -Method Post -Headers $headers -Body ($payload | ConvertTo-Json -Depth 10)
                return @{ Success = $true; Output = $response.content[0].text; RawResponse = $response }
            }
            
            'GOOGLE_GEMINI' {
                $uri = "$($model.endpoint)/v1beta/models/$($model.model_id):generateContent?key=$($model.api_key)"
                $payload = @{
                    contents = @(@{ parts = @(@{ text = $Prompt }) })
                    generationConfig = @{ temperature = $Temperature; maxOutputTokens = $MaxTokens }
                }
                $response = Invoke-RestMethod -Uri $uri -Method Post -Body ($payload | ConvertTo-Json -Depth 10) -ContentType 'application/json'
                return @{ Success = $true; Output = $response.candidates[0].content.parts[0].text; RawResponse = $response }
            }
            
            'AZURE_OPENAI' {
                $headers['api-key'] = $model.api_key
                $payload = @{
                    messages = if ($Messages) { $Messages } else { @(@{ role = 'user'; content = $Prompt }) }
                    temperature = $Temperature
                    max_tokens = $MaxTokens
                }
                $response = Invoke-RestMethod -Uri "$($model.endpoint)/openai/deployments/$($model.model_id)/chat/completions?api-version=2024-02-01" -Method Post -Headers $headers -Body ($payload | ConvertTo-Json -Depth 10)
                return @{ Success = $true; Output = $response.choices[0].message.content; RawResponse = $response }
            }
            
            'AWS_BEDROCK' {
                try {
                    # Pure PowerShell AWS Bedrock integration
                    $region = $model.endpoint -replace 'https://', '' -replace '\.amazonaws\.com.*', ''
                    $service = 'bedrock-runtime'
                    $hostName = "$region.$service.amazonaws.com"
                    
                    # Get AWS credentials from environment or IAM role
                    $accessKey = [Environment]::GetEnvironmentVariable('AWS_ACCESS_KEY_ID')
                    $secretKey = [Environment]::GetEnvironmentVariable('AWS_SECRET_ACCESS_KEY')
                    $sessionToken = [Environment]::GetEnvironmentVariable('AWS_SESSION_TOKEN')
                    
                    if (-not $accessKey -or -not $secretKey) {
                        # Try to get credentials from AWS CLI config
                        $awsConfigPath = Join-Path $env:USERPROFILE '.aws\credentials'
                        if (Test-Path $awsConfigPath) {
                            $config = Get-Content $awsConfigPath -Raw
                            if ($config -match 'aws_access_key_id\s*=\s*(\w+)') {
                                $accessKey = $matches[1]
                            }
                            if ($config -match 'aws_secret_access_key\s*=\s*([\w/+=]+)') {
                                $secretKey = $matches[1]
                            }
                        }
                    }
                    
                    if (-not $accessKey -or -not $secretKey) {
                        return @{ Success = $false; Error = 'AWS credentials not found. Set AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY environment variables.' }
                    }
                    
                    # Prepare request
                    $payload = @{
                        prompt = $Prompt
                        max_tokens = $MaxTokens
                        temperature = $Temperature
                    } | ConvertTo-Json -Compress
                    
                    $payloadBytes = [System.Text.Encoding]::UTF8.GetBytes($payload)
                    $payloadHash = [System.Security.Cryptography.SHA256]::Create().ComputeHash($payloadBytes)
                    $payloadHashHex = [BitConverter]::ToString($payloadHash).Replace('-', '').ToLower()
                    
                    # Generate AWS Signature Version 4
                    $now = [DateTime]::UtcNow
                    $amzDate = $now.ToString('yyyyMMddTHHmmssZ')
                    $dateStamp = $now.ToString('yyyyMMdd')
                    
                    # Create canonical request
                    $canonicalUri = "/model/$($model.model_id)/invoke"
                    $canonicalQueryString = ""
                    $canonicalHeaders = "host:$hostName`nx-amz-date:$amzDate`n"
                    if ($sessionToken) {
                        $canonicalHeaders += "x-amz-security-token:$sessionToken`n"
                    }
                    $signedHeaders = "host;x-amz-date"
                    if ($sessionToken) {
                        $signedHeaders += ";x-amz-security-token"
                    }
                    
                    $canonicalRequest = @(
                        "POST",
                        $canonicalUri,
                        $canonicalQueryString,
                        $canonicalHeaders,
                        $signedHeaders,
                        $payloadHashHex
                    ) -join "`n"
                    
                    # Create string to sign
                    $algorithm = "AWS4-HMAC-SHA256"
                    $credentialScope = "$dateStamp/$region/$service/aws4_request"
                    $canonicalRequestHash = [System.Security.Cryptography.SHA256]::Create().ComputeHash([System.Text.Encoding]::UTF8.GetBytes($canonicalRequest))
                    $canonicalRequestHashHex = [BitConverter]::ToString($canonicalRequestHash).Replace('-', '').ToLower()
                    
                    $stringToSign = @(
                        $algorithm,
                        $amzDate,
                        $credentialScope,
                        $canonicalRequestHashHex
                    ) -join "`n"
                    
                    # Calculate signature
                    $kSecret = [System.Text.Encoding]::UTF8.GetBytes(("AWS4" + $secretKey)))
                    $kDate = New-Object System.Security.Cryptography.HMACSHA256($kSecret)
                    $kDate.Key = $kSecret
                    $kRegion = New-Object System.Security.Cryptography.HMACSHA256($kDate.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($dateStamp)))
                    $kService = New-Object System.Security.Cryptography.HMACSHA256($kRegion.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($region)))
                    $kSigning = New-Object System.Security.Cryptography.HMACSHA256($kService.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($service)))
                    $signature = [BitConverter]::ToString($kSigning.ComputeHash([System.Text.Encoding]::UTF8.GetBytes("aws4_request"))).Replace('-', '').ToLower()
                    $signature = [BitConverter]::ToString($kSigning.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($stringToSign))).Replace('-', '').ToLower()
                    
                    # Create authorization header
                    $authorizationHeader = "$algorithm Credential=$accessKey/$credentialScope, SignedHeaders=$signedHeaders, Signature=$signature"
                    
                    # Make request
                    $headers = @{
                        'Authorization' = $authorizationHeader
                        'x-amz-date' = $amzDate
                        'Content-Type' = 'application/json'
                    }
                    if ($sessionToken) {
                        $headers['x-amz-security-token'] = $sessionToken
                    }
                    
                    $uri = "https://$hostName$canonicalUri"
                    $response = Invoke-RestMethod -Uri $uri -Method Post -Headers $headers -Body $payload -ContentType 'application/json'
                    
                    return @{ Success = $true; Output = $response.completion; RawResponse = $response }
                } catch {
                    return @{ Success = $false; Error = "AWS Bedrock request failed: $_" }
                }
            }
            
            'MOONSHOT_AI' {
                $payload = @{
                    model = $model.model_id
                    messages = if ($Messages) { $Messages } else { @(@{ role = 'user'; content = $Prompt }) }
                    temperature = $Temperature
                    max_tokens = $MaxTokens
                }
                $response = Invoke-RestMethod -Uri "$($model.endpoint)/v1/chat/completions" -Method Post -Headers $headers -Body ($payload | ConvertTo-Json -Depth 10)
                return @{ Success = $true; Output = $response.choices[0].message.content; RawResponse = $response }
            }
            
            'LOCAL_GGUF' {
                $exe = Join-Path (Get-RawrXDRootPath) 'RawrXD-ModelLoader.exe'
                if (-not (Test-Path $exe)) {
                    return @{ Success = $false; Error = 'RawrXD-ModelLoader.exe not found. Build with: cmake --build . --target RawrXD-ModelLoader' }
                }
                
                # Execute GGUF loader with CLI arguments
                $args = @(
                    '--model', $model.model_id,
                    '--prompt', $Prompt,
                    '--temp', $Temperature,
                    '--n-predict', $MaxTokens
                )
                
                try {
                    $output = & $exe $args 2>&1
                    $exitCode = $LASTEXITCODE
                    if ($exitCode -eq 0) {
                        return @{ Success = $true; Output = ($output -join "`n"); RawResponse = $output }
                    } else {
                        return @{ Success = $false; Error = "GGUF loader failed with exit code $exitCode. Output: $output" }
                    }
                } catch {
                    return @{ Success = $false; Error = "Failed to execute GGUF loader: $_" }
                }
            }
            
            default {
                return @{ Success = $false; Error = "Unknown backend: $($model.backend)" }
            }
        }
    }
}

Export-ModuleMember -Function Get-RawrXDModelConfig, Get-RawrXDModelDefinition, Test-RawrXDModelBackend, Invoke-RawrXDModelRequest
