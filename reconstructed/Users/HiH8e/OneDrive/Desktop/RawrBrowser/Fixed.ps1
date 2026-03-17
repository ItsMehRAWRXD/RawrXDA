Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# WebView2 Setup - Only load managed assemblies, not native DLLs
$wvDir = "$env:TEMP\WVLibs"
$useWebView2 = $false
$userDataDir = Join-Path $env:TEMP "RawrBrowserData"

if (-not (Test-Path $userDataDir)) {
  New-Item -ItemType Directory -Path $userDataDir -Force | Out-Null
}

Write-Host "Checking WebView2..." -ForegroundColor Cyan

if (!(Test-Path "$wvDir")) {
  Write-Host "Downloading WebView2..." -ForegroundColor Yellow
  try {
    New-Item -ItemType Directory -Path "$wvDir" -Force | Out-Null
    Invoke-WebRequest "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2" -OutFile "$wvDir\wv.zip"
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory("$wvDir\wv.zip", "$wvDir\ex", $true)
        
    # Only copy the managed WinForms assembly (net45 version)
    $winformsDll = Get-ChildItem "$wvDir\ex" -Recurse -Filter "Microsoft.Web.WebView2.WinForms.dll" | 
    Where-Object { $_.FullName -match "net45" } | 
    Select-Object -First 1
        
    if ($winformsDll) {
      Copy-Item $winformsDll.FullName -Destination $wvDir -Force
      Write-Host "✓ WebView2 downloaded" -ForegroundColor Green
    }
  }
  catch {
    Write-Host "✗ WebView2 download failed: $_" -ForegroundColor Red
    Write-Host "Using fallback WebBrowser control" -ForegroundColor Yellow
  }
}

# Try to load WebView2 WinForms assembly
if (Test-Path "$wvDir\Microsoft.Web.WebView2.WinForms.dll") {
  try {
    Add-Type -Path "$wvDir\Microsoft.Web.WebView2.WinForms.dll" -ErrorAction Stop
    $useWebView2 = $true
    Write-Host "✓ WebView2 loaded successfully" -ForegroundColor Green
  }
  catch {
    Write-Host "✗ WebView2 load failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Using fallback WebBrowser control" -ForegroundColor Yellow
    $useWebView2 = $false
  }
}

$form = New-Object System.Windows.Forms.Form
$form.Width = 1400
$form.Height = 900
$form.Text = "Rawr Browser – Byte Engine"
$form.StartPosition = 'CenterScreen'
$form.BackColor = [System.Drawing.Color]::FromArgb(25, 25, 25)

# =============================================
# Developer Console
# =============================================
$consolePanel = New-Object System.Windows.Forms.Panel
$consolePanel.Dock = 'Bottom'
$consolePanel.Height = 160
$consolePanel.BackColor = [System.Drawing.Color]::FromArgb(30, 30, 30)
$form.Controls.Add($consolePanel)

$logBox = New-Object System.Windows.Forms.RichTextBox
$logBox.Dock = 'Fill'
$logBox.ReadOnly = $true
$logBox.Font = New-Object System.Drawing.Font('Consolas', 8)
$logBox.BackColor = [System.Drawing.Color]::FromArgb(20, 20, 20)
$logBox.ForeColor = [System.Drawing.Color]::LightGray
$logBox.WordWrap = $false
$consolePanel.Controls.Add($logBox)

$consoleToolbar = New-Object System.Windows.Forms.Panel
$consoleToolbar.Dock = 'Top'
$consoleToolbar.Height = 28
$consoleToolbar.BackColor = [System.Drawing.Color]::FromArgb(45, 45, 45)
$consolePanel.Controls.Add($consoleToolbar)

$btnClear = New-Object System.Windows.Forms.Button
$btnClear.Text = 'Clear'
$btnClear.Dock = 'Right'
$btnClear.Width = 70
$consoleToolbar.Controls.Add($btnClear)

$btnExport = New-Object System.Windows.Forms.Button
$btnExport.Text = 'Export Log'
$btnExport.Dock = 'Right'
$btnExport.Width = 90
$consoleToolbar.Controls.Add($btnExport)

function Write-BrowserLog {
  param(
    [string]$Message,
    [ValidateSet('INFO', 'SUCCESS', 'WARN', 'ERROR', 'DEBUG')] [string]$Level = 'INFO'
  )
  $timestamp = (Get-Date).ToString('HH:mm:ss.fff')
  $prefix = "[$timestamp][$Level] "
  $color = switch ($Level) {
    'ERROR' { [System.Drawing.Color]::Salmon }
    'WARN' { [System.Drawing.Color]::Khaki }
    'SUCCESS' { [System.Drawing.Color]::LightGreen }
    'DEBUG' { [System.Drawing.Color]::DeepSkyBlue }
    default { [System.Drawing.Color]::LightGray }
  }
  $logBox.SelectionStart = $logBox.TextLength
  $logBox.SelectionColor = [System.Drawing.Color]::DarkGray
  $logBox.AppendText($prefix)
  $logBox.SelectionColor = $color
  $logBox.AppendText($Message + "`r`n")
  $logBox.SelectionColor = $logBox.ForeColor
  $logBox.ScrollToCaret()
}

$btnClear.Add_Click({ $logBox.Clear(); Write-BrowserLog 'Console cleared' 'INFO' })
$btnExport.Add_Click({
    try {
      $dlg = New-Object System.Windows.Forms.SaveFileDialog
      $dlg.Filter = 'Log Files (*.log)|*.log|Text Files (*.txt)|*.txt'
      $dlg.FileName = 'RawrBrowserLog_' + (Get-Date).ToString('yyyyMMdd_HHmmss') + '.log'
      if ($dlg.ShowDialog() -eq 'OK') {
        [IO.File]::WriteAllText($dlg.FileName, $logBox.Text)
        Write-BrowserLog "Log exported to $($dlg.FileName)" 'SUCCESS'
      }
    }
    catch { Write-BrowserLog "Export error: $_" 'ERROR' }
  })

Write-BrowserLog 'RawrBrowser starting...' 'INFO'
Write-BrowserLog "WebView2 available: $useWebView2" 'INFO'

if ($useWebView2) {
  Write-Host "Creating WebView2 browser..." -ForegroundColor Cyan
  Write-BrowserLog 'Creating WebView2 control...' 'INFO'
  try {
    $browser = New-Object Microsoft.Web.WebView2.WinForms.WebView2
    $browser.Dock = [System.Windows.Forms.DockStyle]::Fill
    $browser.DefaultBackgroundColor = [System.Drawing.Color]::FromArgb(25, 25, 25)

    # URL bar + navigation controls
    $navPanel = New-Object System.Windows.Forms.Panel
    $navPanel.Dock = 'Top'
    $navPanel.Height = 32
    $navPanel.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
    $form.Controls.Add($navPanel)

    $urlBox = New-Object System.Windows.Forms.TextBox
    $urlBox.Dock = 'Fill'
    $urlBox.Font = New-Object System.Drawing.Font('Consolas', 9)
    $urlBox.Text = 'https://www.youtube.com'
    $navPanel.Controls.Add($urlBox)

    $goBtn = New-Object System.Windows.Forms.Button
    $goBtn.Text = 'Go'
    $goBtn.Dock = 'Right'
    $goBtn.Width = 50
    $navPanel.Controls.Add($goBtn)

    $backBtn = New-Object System.Windows.Forms.Button
    $backBtn.Text = '<'
    $backBtn.Dock = 'Left'
    $backBtn.Width = 40
    $navPanel.Controls.Add($backBtn)

    $fwdBtn = New-Object System.Windows.Forms.Button
    $fwdBtn.Text = '>'
    $fwdBtn.Dock = 'Left'
    $fwdBtn.Width = 40
    $navPanel.Controls.Add($fwdBtn)

    $reloadBtn = New-Object System.Windows.Forms.Button
    $reloadBtn.Text = 'Reload'
    $reloadBtn.Dock = 'Right'
    $reloadBtn.Width = 70
    $navPanel.Controls.Add($reloadBtn)

    $form.Controls.Add($browser)

    $goBtn.Add_Click({
        $u = $urlBox.Text.Trim()
        if ($u -and $browser.CoreWebView2) {
          Write-BrowserLog "Navigating to $u" 'INFO'
          if ($u -notmatch '^https?://') { $u = 'https://' + $u }
          $browser.CoreWebView2.Navigate($u)
        }
      })

    $urlBox.Add_KeyDown({ if ($_.KeyCode -eq 'Enter') { $goBtn.PerformClick() } })
    $backBtn.Add_Click({ if ($browser.CoreWebView2 -and $browser.CoreWebView2.CanGoBack) { $browser.CoreWebView2.GoBack(); Write-BrowserLog 'Back' 'DEBUG' } })
    $fwdBtn.Add_Click({ if ($browser.CoreWebView2 -and $browser.CoreWebView2.CanGoForward) { $browser.CoreWebView2.GoForward(); Write-BrowserLog 'Forward' 'DEBUG' } })
    $reloadBtn.Add_Click({ if ($browser.CoreWebView2) { $browser.CoreWebView2.Reload(); Write-BrowserLog 'Reload requested' 'DEBUG' } })

    $browser.CoreWebView2InitializationCompleted.Add({
        if ($_.IsSuccess) {
          Write-BrowserLog 'CoreWebView2 initialization SUCCESS' 'SUCCESS'
        }
        else {
          Write-BrowserLog "CoreWebView2 initialization FAILED: $($_.Exception.Message)" 'ERROR'
        }
        $browser.CoreWebView2.NavigateToString(@"
<!DOCTYPE html>
<html>
<body style='font-family:consolas; background:#111; color:#0f0;'>
<h2>Rawr Byte Engine</h2>

<input type='file' id='in' multiple />
<pre id='log'></pre>

<script>
const log = msg => document.getElementById("log").textContent += msg + "\n";

log("Rawr Browser initialized with WebView2!");
log("File compression features available");

// store file into host OS
document.getElementById("in").onchange = async e => {
  for (const f of e.target.files) {
    const buf = await f.arrayBuffer();
    const bytes = Array.from(new Uint8Array(buf));

    log("Selected " + f.name + " (" + bytes.length + " bytes)");
  }
};
</script>

</body>
</html>
"@)
      })

    $browser.EnsureCoreWebView2Async() | Out-Null
    Write-Host "✓ WebView2 browser ready" -ForegroundColor Green
    Write-BrowserLog 'EnsureCoreWebView2Async invoked' 'DEBUG'

    # Additional event hooks once CoreWebView2 exists
    $browser.add_NavigationStarting({
        param($senderObj, $navStartingArgs)
        Write-BrowserLog "NavigationStarting: $($navStartingArgs.Uri)" 'DEBUG'
      })
    $browser.add_NavigationCompleted({
        param($senderObj, $navCompletedArgs)
        $lvl = if ($navCompletedArgs.IsSuccess) { 'SUCCESS' } else { 'WARN' }
        Write-BrowserLog "NavigationCompleted: Success=$($navCompletedArgs.IsSuccess)" $lvl
      })
    $browser.add_WebMessageReceived({
        param($senderObj, $webMsgArgs)
        Write-BrowserLog "WebMessage: $($webMsgArgs.WebMessageAsJson)" 'DEBUG'
      })
  }
  catch {
    Write-Host "✗ WebView2 initialization failed: $_" -ForegroundColor Red
    Write-BrowserLog "WebView2 init exception: $_" 'ERROR'
    $useWebView2 = $false
  }
}

# Fallback to old WebBrowser control if WebView2 failed
if (-not $useWebView2) {
  Write-Host "Using Internet Explorer WebBrowser fallback" -ForegroundColor Yellow
  Write-BrowserLog 'Using fallback WebBrowser control (IE mode)' 'WARN'
  $browser = New-Object System.Windows.Forms.WebBrowser
  $browser.Dock = [System.Windows.Forms.DockStyle]::Fill
  $browser.ScriptErrorsSuppressed = $true
  $form.Controls.Add($browser)
    
  $browser.DocumentText = @"
<!DOCTYPE html>
<html>
<body style='font-family:consolas; background:#111; color:#0f0;'>
<h2>Rawr Browser (Fallback Mode)</h2>
<p>WebView2 not available. Advanced features disabled.</p>
<p>For full functionality, install WebView2 Runtime from Microsoft.</p>
<p>Using Internet Explorer 11 rendering mode.</p>
</body>
</html>
"@
}

Write-Host "`n✓ Rawr Browser starting..." -ForegroundColor Cyan
$form.Add_Shown({ Write-BrowserLog 'Form shown - ready' 'SUCCESS' })
$form.ShowDialog() | Out-Null
