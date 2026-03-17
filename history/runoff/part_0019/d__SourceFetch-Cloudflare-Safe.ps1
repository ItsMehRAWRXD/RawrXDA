#!/usr/bin/env pwsh
# ================================================
# RawrXD SourceFetch v4.0 — Cloudflare-Safe, Production-Hardened
# Zero-error extraction with full validation and logging
# ================================================

function global:Fetch-Source {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Url,
        
        [string]$Name = '',
        [string]$Out = './captured_sources',
        [string]$Cat = 'auto',
        [hashtable]$Headers = @{},
        [string]$Cookies = '',
        [switch]$Force
    )
    
    $ErrorActionPreference = 'Stop'
    
    try {
        # ==========================================
        # 1. Initialize output directory
        # ==========================================
        $OutPath = [System.IO.Path]::GetFullPath($Out)
        $null = New-Item -ItemType Directory -Path $OutPath -Force -ErrorAction SilentlyContinue
        
        $manifestPath = Join-Path $OutPath 'manifest.json'
        $hashTable = @{}
        
        # Load existing manifest safely
        if (Test-Path $manifestPath) {
            try {
                $json = Get-Content $manifestPath -Raw -ErrorAction Stop | ConvertFrom-Json -AsHashtable -ErrorAction Stop
                if ($json -and $json.hashes) {
                    $hashTable = $json.hashes
                }
            } catch {
                Write-Warning "[MANIFEST] Corrupted manifest, creating new: $($_.Exception.Message)"
                $hashTable = @{}
            }
        }
        
        # ==========================================
        # 2. Fetch content with protection detection
        # ==========================================
        Write-Host "[FETCH] Downloading: $Url" -ForegroundColor Cyan
        
        $requestParams = @{
            Uri = $Url
            Method = 'GET'
            UseBasicParsing = $true
            TimeoutSec = 30
            MaximumRedirection = 5
            UserAgent = 'RawrXD-SourceFetch/4.0 (Atomic-Loader; +https://rawrxd.dev)'
        }
        
        if ($Headers.Count -gt 0) {
            $requestParams.Headers = $Headers
        }
        
        if ($Cookies) {
            $requestParams.Headers['Cookie'] = $Cookies
        }
        
        try {
            $response = Invoke-WebRequest @requestParams -ErrorAction Stop
            $content = $response.Content
            $statusCode = $response.StatusCode
        } catch {
            Write-Host "[ERROR] HTTP request failed: $($_.Exception.Message)" -ForegroundColor Red
            
            if ($_.Exception.Response.StatusCode -eq 403) {
                Write-Host "[TIP] This site has anti-bot protection (Cloudflare/similar)" -ForegroundColor Yellow
                Write-Host "[TIP] Try: -Headers @{'Cookie'='session_from_browser'}" -ForegroundColor Yellow
            }
            
            return $null
        }
        
        # ==========================================
        # 3. Validate content (detect Cloudflare)
        # ==========================================
        if (-not $content -or $content.Length -eq 0) {
            Write-Host "[ERROR] Empty response from $Url" -ForegroundColor Red
            return $null
        }
        
        # Detect Cloudflare challenge/protection
        $cloudflareSignatures = @(
            'Just a moment...',
            'Enable JavaScript and cookies to continue',
            'window._cf_chl_opt',
            'challenge-platform',
            'cf-browser-verification'
        )
        
        $isCloudflare = $false
        foreach ($sig in $cloudflareSignatures) {
            if ($content -match [regex]::Escape($sig)) {
                $isCloudflare = $true
                break
            }
        }
        
        if ($isCloudflare) {
            Write-Host "[CLOUDFLARE] Protection detected - cannot bypass without browser session" -ForegroundColor Red
            Write-Host "[SOLUTION] Options:" -ForegroundColor Yellow
            Write-Host "  1. Export cookies from your browser (DevTools > Application > Cookies)" -ForegroundColor Yellow
            Write-Host "  2. Use the site's API instead of web scraping" -ForegroundColor Yellow
            Write-Host "  3. Use Selenium/Puppeteer to handle JavaScript challenges" -ForegroundColor Yellow
            
            if (-not $Force) {
                Write-Host "[ABORT] Use -Force to save Cloudflare challenge page anyway" -ForegroundColor Red
                return $null
            }
            
            Write-Host "[FORCE] Saving Cloudflare page as requested..." -ForegroundColor DarkYellow
        }
        
        # ==========================================
        # 4. Convert to bytes for hashing
        # ==========================================
        if ($content -is [string]) {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($content)
        } elseif ($content -is [byte[]]) {
            $bytes = $content
        } else {
            # Fallback: convert to string first
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($content.ToString())
        }
        
        # ==========================================
        # 5. Compute SHA256 hash properly
        # ==========================================
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        $hashBytes = $sha256.ComputeHash($bytes)
        $sha256.Dispose()
        
        $contentHash = [System.BitConverter]::ToString($hashBytes) -replace '-', '' | ForEach-Object { $_.ToLower() }
        
        # ==========================================
        # 6. Check for duplicates
        # ==========================================
        if ($hashTable.ContainsKey($contentHash)) {
            Write-Host "[DUPLICATE] Already captured as: $($hashTable[$contentHash])" -ForegroundColor Yellow
            Write-Host "[HASH] $contentHash" -ForegroundColor DarkGray
            return $hashTable[$contentHash]
        }
        
        # ==========================================
        # 7. Detect category from URL and content
        # ==========================================
        $category = if ($Cat -ne 'auto') {
            $Cat
        } elseif ($Url -match '\.(asm|inc)$') {
            'masm'
        } elseif ($Url -match '\.(c|cpp|cc|cxx|h|hpp)$') {
            'cpp'
        } elseif ($Url -match '\.py$') {
            'python'
        } elseif ($Url -match '\.(js|jsx|ts|tsx)$') {
            'javascript'
        } elseif ($Url -match '\.rs$') {
            'rust'
        } elseif ($Url -match '\.(ps1|psm1|psd1)$') {
            'powershell'
        } elseif ($Url -match '\.(go)$') {
            'golang'
        } elseif ($Url -match '\.(java)$') {
            'java'
        } elseif ($Url -match '\.(html|htm)$') {
            'html'
        } else {
            'misc'
        }
        
        # ==========================================
        # 8. Determine base name
        # ==========================================
        $baseName = if ($Name) {
            $Name
        } else {
            try {
                $uri = [System.Uri]$Url
                $segments = $uri.Segments
                $lastSegment = $segments[-1] -replace '/', ''
                
                if ($lastSegment -and $lastSegment -ne '' -and $lastSegment -ne '/') {
                    [System.IO.Path]::GetFileNameWithoutExtension($lastSegment)
                } else {
                    'index'
                }
            } catch {
                'unknown'
            }
        }
        
        # ==========================================
        # 9. Sanitize filename (Windows-safe)
        # ==========================================
        # Remove all invalid characters
        $baseName = $baseName -replace '[<>:\"/\\|?*\x00-\x1f]', '_'
        
        # Remove reserved Windows names
        $baseName = $baseName -replace '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$', '_$1'
        
        # Trim whitespace and dots
        $baseName = $baseName.Trim('. ')
        
        # Ensure not empty
        if ([string]::IsNullOrWhiteSpace($baseName)) {
            $baseName = 'unnamed'
        }
        
        # Limit length (leave room for category, hash, sequence)
        if ($baseName.Length -gt 100) {
            $baseName = $baseName.Substring(0, 100)
        }
        
        # ==========================================
        # 10. Determine file extension
        # ==========================================
        $extension = if ($baseName -match '\.(\w+)$') {
            ".$($Matches[1])"
        } else {
            switch ($category) {
                'masm'       { '.asm' }
                'cpp'        { '.cpp' }
                'python'     { '.py' }
                'javascript' { '.js' }
                'rust'       { '.rs' }
                'powershell' { '.ps1' }
                'golang'     { '.go' }
                'java'       { '.java' }
                'html'       { '.html' }
                default      { '.txt' }
            }
        }
        
        # Remove extension from base name if present
        $baseName = $baseName -replace '\.\w+$', ''
        
        # ==========================================
        # 11. Create category directory
        # ==========================================
        $categoryDir = Join-Path $OutPath $category
        $null = New-Item -ItemType Directory -Path $categoryDir -Force -ErrorAction SilentlyContinue
        
        # ==========================================
        # 12. Generate collision-free filename
        # ==========================================
        $sequence = 0
        $finalPath = $null
        $finalName = $null
        
        do {
            $suffix = if ($sequence -gt 0) {
                "_{0:D3}" -f $sequence
            } else {
                ''
            }
            
            $finalName = "{0}_{1}_{2}{3}{4}" -f $category, $baseName, $contentHash.Substring(0, 8), $suffix, $extension
            $finalPath = Join-Path $categoryDir $finalName
            $sequence++
            
            # Safety limit
            if ($sequence -gt 999) {
                Write-Host "[ERROR] Collision limit exceeded (999 attempts)" -ForegroundColor Red
                return $null
            }
            
        } while (Test-Path $finalPath)
        
        # ==========================================
        # 13. Write file atomically
        # ==========================================
        Write-Host "[WRITE] $finalPath" -ForegroundColor Green
        [System.IO.File]::WriteAllBytes($finalPath, $bytes)
        
        # ==========================================
        # 14. Update hash table
        # ==========================================
        $hashTable[$contentHash] = $finalPath
        
        # ==========================================
        # 15. Generate metadata JSON
        # ==========================================
        $metadata = @{
            url = $Url
            hash = $contentHash
            category = $category
            name = $finalName
            size = $bytes.Length
            collision_count = ($sequence - 1)
            timestamp = (Get-Date -Format 'o')
            cloudflare_detected = $isCloudflare
            http_status = $statusCode
            content_preview = if ($content.Length -gt 200) {
                $content.Substring(0, 200).Trim() + '...'
            } else {
                $content.Trim()
            }
        }
        
        $metaPath = "$finalPath.meta.json"
        $metadata | ConvertTo-Json -Depth 5 | Set-Content $metaPath -Force
        
        # ==========================================
        # 16. Generate MASM feature stub
        # ==========================================
        $stubPath = "$finalPath.features.inc"
        
        $masmStub = @"
; ================================================
; RawrXD Auto-Generated Feature Stub
; Source: $Url
; Hash: $contentHash
; Captured: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
; Category: $category
; Size: $($bytes.Length) bytes
; ================================================

FEATURE_$(($category).ToUpper())_CAPTURED EQU 1
CONTENT_HASH_HIGH EQU 0$($contentHash.Substring(0, 16))h
CONTENT_HASH_LOW  EQU 0$($contentHash.Substring(16, 16))h
CONTENT_SIZE      EQU $($bytes.Length)

; Metadata strings
EXTRACTED_URL BYTE "$($Url -replace '"', '\"')", 0
EXTRACTED_HASH BYTE "$contentHash", 0
EXTRACTED_CATEGORY BYTE "$category", 0

; Status flags
CLOUDFLARE_DETECTED EQU $(if ($isCloudflare) { '1' } else { '0' })

END
"@
        
        $masmStub | Set-Content $stubPath -Force
        
        # ==========================================
        # 17. Update master manifest
        # ==========================================
        $manifest = @{
            hashes = $hashTable
            stats = @{
                total = $hashTable.Count
                last_update = (Get-Date -Format 'o')
                last_url = $Url
            }
        }
        
        $manifest | ConvertTo-Json -Depth 10 | Set-Content $manifestPath -Force
        
        # ==========================================
        # 18. Summary output
        # ==========================================
        Write-Host ""
        Write-Host "[SUCCESS] Extraction complete" -ForegroundColor Green
        Write-Host "  Path: $finalPath" -ForegroundColor Cyan
        Write-Host "  Hash: $contentHash" -ForegroundColor DarkGray
        Write-Host "  Size: $($bytes.Length) bytes" -ForegroundColor DarkGray
        Write-Host "  Meta: $metaPath" -ForegroundColor DarkGray
        Write-Host "  Stub: $stubPath" -ForegroundColor DarkGray
        
        if ($isCloudflare) {
            Write-Host "  [!] Cloudflare challenge page saved" -ForegroundColor Yellow
        }
        
        return $finalPath
        
    } catch {
        Write-Host ""
        Write-Host "[CRITICAL ERROR] $($_.Exception.Message)" -ForegroundColor Red
        Write-Host $_.ScriptStackTrace -ForegroundColor DarkRed
        return $null
    }
}

# ==========================================
# Batch fetcher
# ==========================================
function global:Fetch-Batch {
    param(
        [Parameter(Mandatory)]
        [string[]]$Urls,
        
        [hashtable]$NameMap = @{},
        [string]$Out = './captured_sources',
        [int]$DelayMs = 1000
    )
    
    $results = @()
    $total = $Urls.Count
    $current = 0
    
    foreach ($url in $Urls) {
        $current++
        Write-Host ""
        Write-Host "[$current/$total] Processing: $url" -ForegroundColor Magenta
        
        $customName = if ($NameMap.ContainsKey($url)) { $NameMap[$url] } else { '' }
        
        try {
            $result = Fetch-Source -Url $url -Name $customName -Out $Out
            $results += @{
                url = $url
                path = $result
                status = if ($result) { 'success' } else { 'failed' }
            }
        } catch {
            $results += @{
                url = $url
                path = $null
                status = 'error'
                error = $_.Exception.Message
            }
        }
        
        # Rate limiting
        if ($current -lt $total) {
            Start-Sleep -Milliseconds $DelayMs
        }
    }
    
    Write-Host ""
    Write-Host "[BATCH] Complete: $($results | Where-Object { $_.status -eq 'success' }).Count/$total succeeded" -ForegroundColor Green
    
    return $results
}

# ==========================================
# Atomic loader (single-call utility)
# ==========================================
function global:Bew {
    param(
        [Parameter(Mandatory)]
        [string]$u,
        [string]$o = '.\atomic.dat',
        [string]$h = $null
    )

    try {
        $invokeParams = @{
            Uri         = $u
            Method      = 'GET'
            TimeoutSec  = 30
            ErrorAction = 'Stop'
        }

        if ((Get-Command Invoke-WebRequest).Parameters.ContainsKey('UseBasicParsing')) {
            $invokeParams.UseBasicParsing = $true
        }

        $resp = Invoke-WebRequest @invokeParams
        $raw = $resp.Content

        if ($null -eq $raw) {
            return 'FAIL:Empty response'
        }

        $c = if ($raw -is [string]) {
            [System.Text.Encoding]::UTF8.GetBytes($raw)
        } elseif ($raw -is [byte[]]) {
            $raw
        } else {
            [byte[]]$raw
        }

        if ($null -eq $c -or $c.Length -eq 0) {
            return 'FAIL:Empty payload'
        }

        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            $s = ([System.BitConverter]::ToString($sha.ComputeHash($c))).Replace('-', '').ToLowerInvariant()
        } finally {
            $sha.Dispose()
        }

        if ($h -and ($s -ne $h.ToLowerInvariant())) {
            return "FAIL:HASH $s!=$($h.ToLowerInvariant())"
        }

        $dest = [System.IO.Path]::GetFullPath($o)
        $dir = [System.IO.Path]::GetDirectoryName($dest)
        if ($dir -and -not (Test-Path $dir)) {
            $null = New-Item -ItemType Directory -Path $dir -Force
        }

        [System.IO.File]::WriteAllBytes($dest, $c)
        return "OK:$dest ($($c.Length)b) SHA:$s"
    }
    catch {
        return "FAIL:$($_.Exception.Message)"
    }
}

function global:B {
    param([string]$u, [string]$o)
    Bew $u $o
}

# ==========================================
# Aliases
# ==========================================
Set-Alias -Name fs -Value Fetch-Source -Scope Global -Force
Set-Alias -Name fb -Value Fetch-Batch -Scope Global -Force

# ==========================================
# Export
# ==========================================
if ($ExecutionContext.SessionState.Module) {
    Export-ModuleMember -Function Fetch-Source, Fetch-Batch, Bew, B -Alias fs, fb
}

Write-Host ""
Write-Host "✓ RawrXD SourceFetch v4.0 loaded" -ForegroundColor Green
Write-Host "  Commands: Fetch-Source (fs), Fetch-Batch (fb), Bew, B" -ForegroundColor Cyan
Write-Host ""
