param(
    [string]$StartPage = (Join-Path $PSScriptRoot 'Phase4-Ultra.html')
)

# Basic WPF + WebView2 host for Phase-4 Ultra / Hyper-IDE

Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase

$webView2Loaded = $false
try {
    # Prefer a local copy of the WebView2 WPF control if it exists, otherwise
    # fall back to the assembly name (for machines with the SDK installed).
    $dllPath = Join-Path $PSScriptRoot 'Microsoft.Web.WebView2.Wpf.dll'
    if (Test-Path $dllPath) {
        Add-Type -Path $dllPath
    } else {
        Add-Type -AssemblyName 'Microsoft.Web.WebView2.Wpf'
    }
    $webView2Loaded = $true
} catch {
    Write-Warning "Microsoft.Web.WebView2.Wpf could not be loaded: $_"
}

if (-not $webView2Loaded) {
    # Graceful fallback: open the start page in the default system browser
    # instead of failing hard. Phase-4 Ultra still works fully in Chrome/Edge.
    try {
        Start-Process $StartPage
    } catch {
        [System.Windows.MessageBox]::Show(
            "WebView2 WPF control is not available, and the start page could not be opened in the system browser.",
            "Phase-4 Ultra Browser",
            [System.Windows.MessageBoxButton]::OK,
            [System.Windows.MessageBoxImage]::Error
        ) | Out-Null
    }
    return
}

# Normalise StartPage to a file:/// URL if it's a local path
if (-not ($StartPage -like 'http*')) {
    try {
        $resolved = Resolve-Path $StartPage -ErrorAction Stop
        $path = $resolved.ProviderPath -replace '\\','/'
        if (-not $path.StartsWith('file:///')) {
            $StartPage = 'file:///' + $path
        } else {
            $StartPage = $path
        }
    } catch {
        # Fall back to original string; WebView2 will show an error if invalid
    }
}

# Enable SAB + File System Access for the embedded engine
$env:WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS = '--enable-features=SharedArrayBuffer,FileSystemAccessAPI'

# Create main window
$window = New-Object System.Windows.Window
$window.Title  = 'Phase-4 Ultra Browser'
$window.Width  = 1280
$window.Height = 800

$grid = New-Object System.Windows.Controls.Grid

$rowToolbar         = New-Object System.Windows.Controls.RowDefinition
$rowToolbar.Height  = 'Auto'
$rowContent         = New-Object System.Windows.Controls.RowDefinition

$grid.RowDefinitions.Add($rowToolbar)
$grid.RowDefinitions.Add($rowContent)
$window.Content = $grid

# Toolbar: address bar + buttons
$toolbar = New-Object System.Windows.Controls.StackPanel
$toolbar.Orientation = 'Horizontal'
$toolbar.Margin      = '4'

$addressBox = New-Object System.Windows.Controls.TextBox
$addressBox.Width = 720
$addressBox.Text  = $StartPage

$goButton       = New-Object System.Windows.Controls.Button; $goButton.Content       = 'Go';             $goButton.Margin       = '4,0,0,0'
$homeButton     = New-Object System.Windows.Controls.Button; $homeButton.Content     = 'Home';           $homeButton.Margin     = '4,0,0,0'
$devToolsButton = New-Object System.Windows.Controls.Button; $devToolsButton.Content = 'DevTools';       $devToolsButton.Margin = '4,0,0,0'
$injectButton   = New-Object System.Windows.Controls.Button; $injectButton.Content   = 'Inject JS';      $injectButton.Margin   = '4,0,0,0'
$safeButton     = New-Object System.Windows.Controls.Button; $safeButton.Content     = 'Safe Mode: OFF'; $safeButton.Margin     = '4,0,0,0'
$killButton     = New-Object System.Windows.Controls.Button; $killButton.Content     = 'Kill Tab';       $killButton.Margin     = '4,0,0,0'
$restoreButton  = New-Object System.Windows.Controls.Button; $restoreButton.Content  = 'Restore Snapshot';$restoreButton.Margin  = '4,0,0,0'

$toolbar.Children.Add($addressBox)
$toolbar.Children.Add($goButton)
$toolbar.Children.Add($homeButton)
$toolbar.Children.Add($devToolsButton)
$toolbar.Children.Add($injectButton)
$toolbar.Children.Add($safeButton)
$toolbar.Children.Add($killButton)
$toolbar.Children.Add($restoreButton)

[System.Windows.Controls.Grid]::SetRow($toolbar, 0)
$grid.Children.Add($toolbar)

# WebView2 control
$wv = New-Object Microsoft.Web.WebView2.Wpf.WebView2
[System.Windows.Controls.Grid]::SetRow($wv, 1)
$grid.Children.Add($wv)

$initialized = $false
$safeModeEnabled = $false

# Ensure CoreWebView2 is created
$wv.Add_Initialized({
    try {
        $null = $wv.EnsureCoreWebView2Async()
    } catch {
        Write-Warning "EnsureCoreWebView2Async failed: $_"
    }
})

# Once CoreWebView2 is ready, configure and navigate
$wv.CoreWebView2InitializationCompleted.Add({
    param($sender, $args)
    if (-not $args.IsSuccess) {
        [System.Windows.MessageBox]::Show(
            'CoreWebView2 initialization failed: ' + $args.InitializationException.Message,
            'Phase-4 Ultra Browser',
            [System.Windows.MessageBoxButton]::OK,
            [System.Windows.MessageBoxImage]::Error
        ) | Out-Null
        return
    }

    $global:core = $wv.CoreWebView2
    $initialized  = $true

    $core.Settings.AreDevToolsEnabled   = $true
    $core.Settings.IsStatusBarEnabled   = $false
    $core.Settings.IsZoomControlEnabled = $true

    # Log messages coming from the page
    $core.AddWebMessageReceived({
        param($s,$e)
        try {
            $text = $e.TryGetWebMessageAsString()
            if (-not $text) { $text = $e.WebMessageAsJson }
            Write-Host "[page] $text"
        } catch {
            Write-Warning "WebMessageReceived error: $_"
        }
    }) | Out-Null

    # Inject a small host bridge into every document
    $bridgeScript = @"
(function(){
  if (window.hostBridge) return;
  window.hostBridge = {
    send: function(msg){
      try {
        if (window.chrome && window.chrome.webview){
          window.chrome.webview.postMessage(typeof msg === 'string' ? msg : JSON.stringify(msg));
        }
      } catch(e){ console.error('hostBridge.send error', e); }
    },
    onMessage: function(handler){
      if (!window.chrome || !window.chrome.webview) return;
      window.chrome.webview.addEventListener('message', function(ev){
        try { handler(ev.data); } catch(e){ console.error('hostBridge.onMessage error', e); }
      });
    }
  };
})();
"@
    $null = $core.AddScriptToExecuteOnDocumentCreatedAsync($bridgeScript)

    $core.Navigate($addressBox.Text)
})

# Button handlers
$goButton.Add_Click({
    if ($initialized -and $wv.CoreWebView2) {
        $wv.CoreWebView2.Navigate($addressBox.Text)
    }
})

$homeButton.Add_Click({
    if ($initialized -and $wv.CoreWebView2) {
        $wv.CoreWebView2.Navigate($StartPage)
        $addressBox.Text = $StartPage
    }
})

$devToolsButton.Add_Click({
    if ($initialized -and $wv.CoreWebView2) {
        $wv.CoreWebView2.OpenDevToolsWindow()
    }
})

$injectButton.Add_Click({
    if (-not ($initialized -and $wv.CoreWebView2)) { return }

    $inputWin = New-Object System.Windows.Window
    $inputWin.Title  = 'Inject JavaScript into page'
    $inputWin.Width  = 640
    $inputWin.Height = 400

    $dock = New-Object System.Windows.Controls.DockPanel

    $text = New-Object System.Windows.Controls.TextBox
    $text.AcceptsReturn = $true
    $text.VerticalScrollBarVisibility = 'Auto'
    $text.Text = "console.log('hello from Phase4-Browser injector');"

    $runBtn = New-Object System.Windows.Controls.Button
    $runBtn.Content = 'Run'
    $runBtn.HorizontalAlignment = 'Right'
    $runBtn.Margin = '0,4,4,4'

    $dock.Children.Add($runBtn)
    [System.Windows.Controls.DockPanel]::SetDock($runBtn,'Bottom')
    $dock.Children.Add($text)

    $inputWin.Content = $dock

    $runBtn.Add_Click({
        $script = $text.Text
        if ($script) {
            $null = $wv.CoreWebView2.ExecuteScriptAsync($script)
        }
    })

    $inputWin.Owner = $window
    [void]$inputWin.ShowDialog()
})

$safeButton.Add_Click({
    if ($initialized -and $wv.CoreWebView2) {
        $safeModeEnabled = -not $safeModeEnabled
        $state = if ($safeModeEnabled) { 'ON' } else { 'OFF' }
        $safeButton.Content = "Safe Mode: $state"
        $payload = @{ cmd = 'SAFE_MODE'; enabled = $safeModeEnabled } | ConvertTo-Json -Compress
        $wv.CoreWebView2.PostWebMessageAsJson($payload)
    }
})

$killButton.Add_Click({
    if ($initialized -and $wv.CoreWebView2) {
        try {
            $wv.CoreWebView2.Stop()
        } catch {
            Write-Warning "CoreWebView2.Stop failed: $_"
        }
        $wv.CoreWebView2.Navigate('about:blank')
    }
})

$restoreButton.Add_Click({
    if ($initialized -and $wv.CoreWebView2) {
        # The page can implement RESTORE_SNAPSHOT handling if desired
        $payload = @{ cmd = 'RESTORE_SNAPSHOT'; name = 'last-good' } | ConvertTo-Json -Compress
        $wv.CoreWebView2.PostWebMessageAsJson($payload)
    }
})

# Show the browser window
[void]$window.ShowDialog()
