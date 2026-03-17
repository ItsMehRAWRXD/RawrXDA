# PowerShell WebView2 IDE Launcher
# Provides full file system access to the IDE via PowerShell bridge

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Check for WebView2 Runtime
$webView2Runtime = Get-ItemProperty -Path "HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" -ErrorAction SilentlyContinue
if (-not $webView2Runtime) {
    Write-Host "WebView2 Runtime not found. Falling back to default browser..." -ForegroundColor Yellow
    Start-Process "msedge" "file:///$PSScriptRoot\IDEre2.html"
    exit
}

# Create form
$form = New-Object System.Windows.Forms.Form
$form.Text = "IDE - PowerShell Enhanced"
$form.Size = New-Object System.Drawing.Size(1600, 900)
$form.StartPosition = "CenterScreen"
$form.WindowState = "Maximized"

# Create WebView2
try {
    $webView = New-Object Microsoft.Web.WebView2.WinForms.WebView2
    $webView.Dock = "Fill"
    $form.Controls.Add($webView)
} catch {
    Write-Host "Failed to create WebView2 control: $_" -ForegroundColor Red
    Write-Host "Opening in default browser instead..." -ForegroundColor Yellow
    Start-Process "msedge" "file:///$PSScriptRoot\IDEre2.html"
    exit
}

# Initialize WebView2
$webView.Source = [System.Uri]::new("file:///$PSScriptRoot/IDEre2.html")

# Handle messages from JavaScript
$webView.Add_WebMessageReceived({
    param($sender, $args)
    
    $message = $args.WebMessageAsJson | ConvertFrom-Json
    
    switch ($message.type) {
        "listDirectory" {
            try {
                $path = $message.path
                $items = Get-ChildItem -Path $path -ErrorAction Stop
                
                $directories = @()
                $files = @()
                
                foreach ($item in $items) {
                    if ($item.PSIsContainer) {
                        $directories += @{
                            name = $item.Name
                            path = $item.FullName
                        }
                    } else {
                        $files += @{
                            name = $item.Name
                            path = $item.FullName
                            size = $item.Length
                        }
                    }
                }
                
                $response = @{
                    success = $true
                    requestId = $message.requestId
                    directories = $directories
                    files = $files
                } | ConvertTo-Json -Depth 10
                
                $sender.CoreWebView2.PostWebMessageAsJson($response)
            } catch {
                $response = @{
                    success = $false
                    requestId = $message.requestId
                    error = $_.Exception.Message
                } | ConvertTo-Json
                
                $sender.CoreWebView2.PostWebMessageAsJson($response)
            }
        }
        
        "readFile" {
            try {
                $path = $message.path
                $content = Get-Content -Path $path -Raw -ErrorAction Stop
                
                $response = @{
                    success = $true
                    requestId = $message.requestId
                    content = $content
                } | ConvertTo-Json -Depth 10
                
                $sender.CoreWebView2.PostWebMessageAsJson($response)
            } catch {
                $response = @{
                    success = $false
                    requestId = $message.requestId
                    error = $_.Exception.Message
                } | ConvertTo-Json
                
                $sender.CoreWebView2.PostWebMessageAsJson($response)
            }
        }
        
        "writeFile" {
            try {
                $path = $message.path
                $content = $message.content
                
                Set-Content -Path $path -Value $content -ErrorAction Stop
                
                $response = @{
                    success = $true
                    requestId = $message.requestId
                } | ConvertTo-Json
                
                $sender.CoreWebView2.PostWebMessageAsJson($response)
            } catch {
                $response = @{
                    success = $false
                    requestId = $message.requestId
                    error = $_.Exception.Message
                } | ConvertTo-Json
                
                $sender.CoreWebView2.PostWebMessageAsJson($response)
            }
        }
    }
})

Write-Host "Starting IDE with PowerShell file access..." -ForegroundColor Green
Write-Host "IDE will have full file system access via PowerShell bridge" -ForegroundColor Cyan

[void]$form.ShowDialog()
