# RawrXD.UI.psm1
# UI logic for RawrXD (WinForms dialogs, error dialogs, browser, etc.)

# Exported functions will be added here during modularization.

# Asynchronous, streaming AI chat integration for Ollama

function Start-OllamaChatAsync {
	param(
		[Parameter(Mandatory)]
		[string]$Prompt,
		[Parameter(Mandatory)]
		[System.Windows.Forms.Form]$Form,
		[Parameter(Mandatory)]
		[System.Windows.Forms.TextBox]$ChatBox,
		[string]$OllamaModel = "llama3",
		[string]$OllamaHost = "http://localhost:11434",
		[bool]$StreamUI = $true  # If $true, stream to UI; if $false, update only when full response is received
	)

	$sync = [hashtable]::Synchronized(@{})
	$sync.Streaming = $true

    $rawrXDRootPath = if ($env:LAZY_INIT_IDE_ROOT -and (Test-Path $env:LAZY_INIT_IDE_ROOT)) {
        $env:LAZY_INIT_IDE_ROOT
    } else {
        $PSScriptRoot
    }

	$runspace = [runspacefactory]::CreateRunspace()
	$runspace.ApartmentState = "STA"
	$runspace.ThreadOptions = "ReuseThread"
	$runspace.Open()

	$ps = [PowerShell]::Create()
	$ps.Runspace = $runspace
    Import-Module (Join-Path $rawrXDRootPath "RawrXD.Logging.psm1") -Force
    Import-Module (Join-Path $rawrXDRootPath "RawrXD.Config.psm1") -Force

    $ps.AddScript({
        param($Prompt, $OllamaModel, $OllamaHost, $sync, $StreamUI, $RawrXDRootPath)
        Import-Module (Join-Path $RawrXDRootPath "RawrXD.Logging.psm1") -Force
        Import-Module (Join-Path $RawrXDRootPath "RawrXD.Config.psm1") -Force
		try {
			$url = "$OllamaHost/api/chat"
			$headers = @{ 'Content-Type' = 'application/json' }
			$body = @{ model = $OllamaModel; messages = @(@{ role = 'user'; content = $Prompt }) } | ConvertTo-Json -Depth 4

			$request = [System.Net.HttpWebRequest]::Create($url)
			$request.Method = "POST"
			$request.ContentType = "application/json"
			$bytes = [System.Text.Encoding]::UTF8.GetBytes($body)
			$request.ContentLength = $bytes.Length
			$requestStream = $request.GetRequestStream()
			$requestStream.Write($bytes, 0, $bytes.Length)
			$requestStream.Close()

			$response = $request.GetResponse()
			$stream = $response.GetResponseStream()
			$reader = New-Object System.IO.StreamReader($stream)

			$buffer = New-Object char[] 256
			$sb = New-Object System.Text.StringBuilder
			while (($read = $reader.Read($buffer, 0, $buffer.Length)) -gt 0 -and $sync.Streaming) {
				$chunk = -join $buffer[0..($read-1)]
				$sb.Append($chunk) | Out-Null
				if ($StreamUI) {
					# Stream to UI as chunks arrive
					[System.Windows.Forms.Application]::OpenForms[0].BeginInvoke({
						param($text)
						$ChatBox.AppendText($text)
					}, $chunk)
				}
			}
			$reader.Close()
			$stream.Close()
			$response.Close()

			if (-not $StreamUI) {
				# Update UI with the full response at once
				$fullText = $sb.ToString()
				[System.Windows.Forms.Application]::OpenForms[0].BeginInvoke({
					param($text)
					$ChatBox.AppendText($text)
				}, $fullText)
			}
		} catch {
			[System.Windows.Forms.Application]::OpenForms[0].BeginInvoke({
				param($err)
				$ChatBox.AppendText("`n[AI Error]: $err`n")
			}, $_.Exception.Message)
		}
    }).AddArgument($Prompt).AddArgument($OllamaModel).AddArgument($OllamaHost).AddArgument($sync).AddArgument($StreamUI).AddArgument($rawrXDRootPath)

	$ps.BeginInvoke()

	return $sync
}

# ============================================
# THREAD-SAFE UI UPDATES (Requirement A)
# ============================================

function Invoke-UIUpdate {
    param(
        [Parameter(Mandatory=$true)][System.Windows.Forms.Control]$Control,
        [Parameter(Mandatory=$true)][scriptblock]$Action
    )
    
    if ($Control.InvokeRequired) {
        $Control.Invoke($Action)
    } else {
        &$Action
    }
}

# ============================================
# WEBVIEW2 STABILIZATION (Requirement B)
# ============================================

function Get-WebView2RuntimePath {
    $registryPaths = @(
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8EBB-23585059158E}",
        "HKCU:\Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8EBB-23585059158E}"
    )
    
    foreach ($path in $registryPaths) {
        if (Test-Path $path) {
            $pv = Get-ItemProperty -Path $path -Name "pv" -ErrorAction SilentlyContinue
            if ($pv) { return $pv.pv }
        }
    }
    return $null
}

function Initialize-WebView2Safe {
    param(
        [Parameter(Mandatory=$true)][System.Windows.Forms.Control]$Container
    )
    
    $runtimeVersion = Get-WebView2RuntimePath
    
    if ($null -ne $runtimeVersion) {
        Write-StartupLog "WebView2 Runtime detected: $runtimeVersion" "SUCCESS"
        try {
            $wv2 = New-Object Microsoft.Web.WebView2.WinForms.WebView2
            $wv2.Dock = [System.Windows.Forms.DockStyle]::Fill
            $Container.Controls.Add($wv2)
            $wv2.EnsureCoreWebView2Async() | Out-Null
            return $wv2
        } catch {
            Write-StartupLog "WebView2 Initialization failed: $($_.Exception.Message)" "WARNING"
        }
    }
    
    Write-StartupLog "WebView2 not found. Falling back to IE Engine." "WARNING"
    $ie = New-Object System.Windows.Forms.WebBrowser
    $ie.Dock = [System.Windows.Forms.DockStyle]::Fill
    $Container.Controls.Add($ie)
    return $ie
}

Export-ModuleMember -Function Start-OllamaChatAsync, Invoke-UIUpdate, Initialize-WebView2Safe

# ============================================
# THREAD-SAFE UI UPDATES WITH .Invoke (Requirement A)
# ============================================

function Update-ChatBoxThreadSafe {
    param(
        [Parameter(Mandatory=$true)][System.Windows.Forms.TextBox]$ChatBox,
        [Parameter(Mandatory=$true)][string]$Text,
        [System.Windows.Forms.Form]$Form
    )
    
    if ($Form.InvokeRequired) {
        $Form.Invoke([action]{
            $ChatBox.AppendText($Text)
        })
    } else {
        $ChatBox.AppendText($Text)
    }
}

function Clear-ChatBoxThreadSafe {
    param(
        [Parameter(Mandatory=$true)][System.Windows.Forms.TextBox]$ChatBox,
        [System.Windows.Forms.Form]$Form
    )
    
    if ($Form.InvokeRequired) {
        $Form.Invoke([action]{
            $ChatBox.Clear()
        })
    } else {
        $ChatBox.Clear()
    }
}

# ============================================
# WEBVIEW2 HARDENED INITIALIZATION (Requirement B)
# ============================================

function Initialize-BrowserControl {
    param(
        [Parameter(Mandatory=$true)][System.Windows.Forms.Panel]$Container
    )
    
    $webView2Runtime = Get-WebView2RuntimePath
    
    if ($webView2Runtime) {
        try {
            Write-Host "🌐 Using WebView2 Runtime: $webView2Runtime" -ForegroundColor Green
            $wv2 = New-Object Microsoft.Web.WebView2.WinForms.WebView2
            $wv2.Dock = [System.Windows.Forms.DockStyle]::Fill
            $Container.Controls.Add($wv2)
            return $wv2
        } catch {
            Write-Host "⚠️ WebView2 initialization failed, falling back to IE" -ForegroundColor Yellow
        }
    }
    
    # Fallback to IE engine
    Write-Host "🌐 Using System.Windows.Forms.WebBrowser (IE Engine)" -ForegroundColor Yellow
    $browser = New-Object System.Windows.Forms.WebBrowser
    $browser.Dock = [System.Windows.Forms.DockStyle]::Fill
    $Container.Controls.Add($browser)
    return $browser
}

# ============================================
# DIALOG HELPERS FOR CONFIGURATION
# ============================================

function Show-ConfigurationDialog {
    param(
        [string]$Title = "Configuration",
        [string]$Message = "Configure RawrXD settings"
    )
    
    $result = [System.Windows.Forms.MessageBox]::Show(
        $Message,
        $Title,
        [System.Windows.Forms.MessageBoxButtons]::OKCancel,
        [System.Windows.Forms.MessageBoxIcon]::Information
    )
    
    return $result -eq [System.Windows.Forms.DialogResult]::OK
}

Export-ModuleMember -Function Update-ChatBoxThreadSafe, Clear-ChatBoxThreadSafe, Initialize-BrowserControl, Show-ConfigurationDialog
