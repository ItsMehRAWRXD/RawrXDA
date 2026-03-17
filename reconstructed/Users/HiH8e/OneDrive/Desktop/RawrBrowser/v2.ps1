Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# WebView2 Setup with comprehensive error handling
$wvDir = "$env:TEMP\WVLibs"
$useWebView2 = $false

Write-Host "=== Rawr Browser v2.1 - Starting ===" -ForegroundColor Cyan

# Check if WebView2 Runtime is installed system-wide
try {
  $regPath = "HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
  if (Test-Path $regPath) {
    $version = (Get-ItemProperty $regPath).pv
    Write-Host "✓ WebView2 Runtime found: v$version" -ForegroundColor Green
  }
  else {
    Write-Host "⚠ WebView2 Runtime not detected in registry" -ForegroundColor Yellow
  }
}
catch {
  Write-Host "⚠ Could not check WebView2 Runtime: $($_.Exception.Message)" -ForegroundColor Yellow
}

Write-Host "Checking WebView2 SDK..." -ForegroundColor Cyan

if (!(Test-Path "$wvDir")) {
  Write-Host "Downloading WebView2 SDK..." -ForegroundColor Yellow
  try {
    New-Item -ItemType Directory -Path "$wvDir" -Force | Out-Null
    
    # Use TLS 1.2
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    
    Write-Host "  -> Downloading from NuGet..." -ForegroundColor Gray
    Invoke-WebRequest "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2" -OutFile "$wvDir\wv.zip"
    
    Write-Host "  -> Extracting package..." -ForegroundColor Gray
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory("$wvDir\wv.zip", "$wvDir\ex")
        
    $net462Path = "$wvDir\ex\lib\net462"
    if (Test-Path $net462Path) {
      Copy-Item "$net462Path\Microsoft.Web.WebView2.Core.dll" -Destination $wvDir -Force
      Copy-Item "$net462Path\Microsoft.Web.WebView2.WinForms.dll" -Destination $wvDir -Force
      
      # Copy the native loader
      $loaderPath = "$wvDir\ex\runtimes\win-x64\native\WebView2Loader.dll"
      if (Test-Path $loaderPath) {
        Copy-Item $loaderPath -Destination $wvDir -Force
        Write-Host "  -> Native loader copied" -ForegroundColor Gray
      }
      
      Write-Host "✓ WebView2 SDK downloaded successfully" -ForegroundColor Green
    }
    else {
      throw "Net462 libraries not found in package"
    }
  }
  catch {
    Write-Host "✗ WebView2 SDK download failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Will attempt to use system WebView2 or fall back to IE" -ForegroundColor Yellow
  }
}

# Try to load WebView2 assemblies
if (Test-Path "$wvDir\Microsoft.Web.WebView2.Core.dll") {
  try {
    Write-Host "Loading WebView2 assemblies..." -ForegroundColor Gray
    Add-Type -Path "$wvDir\Microsoft.Web.WebView2.Core.dll" -ErrorAction Stop
    Add-Type -Path "$wvDir\Microsoft.Web.WebView2.WinForms.dll" -ErrorAction Stop
    $useWebView2 = $true
    Write-Host "✓ WebView2 SDK loaded successfully" -ForegroundColor Green
  }
  catch {
    Write-Host "✗ WebView2 SDK load failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Will try system WebView2 or fall back to IE" -ForegroundColor Yellow
    $useWebView2 = $false
  }
}
else {
  Write-Host "WebView2 SDK not available, will try system install" -ForegroundColor Yellow
}

# Create the main form
$form = New-Object System.Windows.Forms.Form
$form.Width = 1400
$form.Height = 900
$form.Text = "🦖 Rawr Browser – Byte Engine v2.1"
$form.StartPosition = "CenterScreen"

# Add status label
$statusLabel = New-Object System.Windows.Forms.Label
$statusLabel.Text = "Initializing browser engine..."
$statusLabel.Dock = "Bottom"
$statusLabel.Height = 30
$statusLabel.BackColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$statusLabel.ForeColor = [System.Drawing.Color]::Lime
$statusLabel.Font = New-Object System.Drawing.Font("Consolas", 10)
$form.Controls.Add($statusLabel)

# Try WebView2 first (if SDK loaded or system available)
if ($useWebView2 -or (Test-Path "C:\Program Files (x86)\Microsoft\EdgeWebView\Application")) {
  Write-Host "Attempting WebView2 initialization..." -ForegroundColor Cyan
  try {
    # Try to create WebView2 control
    $browser = $null
    
    if ($useWebView2) {
      Write-Host "  -> Using downloaded SDK" -ForegroundColor Gray
      $browser = New-Object Microsoft.Web.WebView2.WinForms.WebView2 -ErrorAction Stop
    }
    else {
      Write-Host "  -> Attempting system WebView2" -ForegroundColor Gray
      # Try to load system WebView2 (requires system install)
      try {
        Add-Type -AssemblyName "Microsoft.Web.WebView2.Core"
        Add-Type -AssemblyName "Microsoft.Web.WebView2.WinForms"
        $browser = New-Object Microsoft.Web.WebView2.WinForms.WebView2 -ErrorAction Stop
        $useWebView2 = $true
        Write-Host "  -> System WebView2 loaded" -ForegroundColor Green
      }
      catch {
        throw "System WebView2 not available: $($_.Exception.Message)"
      }
    }
    
    if ($null -ne $browser) {
      Write-Host "✓ WebView2 control created successfully" -ForegroundColor Green
      
      $browser.Dock = [System.Windows.Forms.DockStyle]::Fill
      $form.Controls.Add($browser)
      $statusLabel.Text = "WebView2 control ready, starting initialization..."

      Write-Host "Setting up WebView2 events..." -ForegroundColor Gray
      
      # Create initialization flag
      $script:initializationComplete = $false
      
      # Navigation completed event
      $navigationHandler = {
        Write-Host "✓ WebView2 page navigation completed" -ForegroundColor Green
        $statusLabel.Text = "🦖 Rawr Browser Ready - Drop files to analyze!"
      }
      $browser.add_NavigationCompleted($navigationHandler)

      # Core initialization event
      $initHandler = {
        param($sender, $e)
        try {
          if ($e.IsSuccess) {
            Write-Host "✓ WebView2 core initialized successfully" -ForegroundColor Green
            $statusLabel.Text = "Loading Rawr interface..."
                  
            # Load the main interface
            $browser.CoreWebView2.NavigateToString($htmlContent)
            $script:initializationComplete = $true
          }
          else {
            Write-Host "✗ WebView2 core initialization failed" -ForegroundColor Red
            $statusLabel.Text = "WebView2 core initialization failed"
          }
        }
        catch {
          Write-Host "✗ Error in initialization handler: $($_.Exception.Message)" -ForegroundColor Red
          $statusLabel.Text = "Error in WebView2 handler"
        }
      }
      $browser.add_CoreWebView2InitializationCompleted($initHandler)
      
      # Set up user data folder
      $userDataFolder = "$env:TEMP\RawrBrowserData"
      if (!(Test-Path $userDataFolder)) {
        New-Item -ItemType Directory -Path $userDataFolder -Force | Out-Null
      }
      
      Write-Host "Starting WebView2 core initialization..." -ForegroundColor Cyan
      $statusLabel.Text = "Initializing WebView2 core..."
      
      # Start initialization with user data folder
      try {
        $initTask = $browser.EnsureCoreWebView2Async($null)
        Write-Host "  -> Initialization task started" -ForegroundColor Gray
      }
      catch {
        Write-Host "  -> Initialization start failed: $($_.Exception.Message)" -ForegroundColor Red
        throw $_
      }
      
      # HTML content for the interface
      $htmlContent = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <title>Rawr Byte Engine</title>
    <style>
        * { box-sizing: border-box; }
        body { 
            font-family: 'Consolas', 'Courier New', monospace; 
            background: linear-gradient(135deg, #0a0a0a, #1a1a1a); 
            color: #00ff00; 
            margin: 0; padding: 20px;
            line-height: 1.6;
            min-height: 100vh;
        }
        .header { text-align: center; margin-bottom: 30px; }
        .header h1 { 
            color: #ffff00; 
            text-shadow: 0 0 10px #ffff00;
            font-size: 2.5em;
            margin: 0;
        }
        .subtitle { 
            color: #00ffff; 
            font-size: 1.2em;
            margin-top: 10px;
        }
        .controls {
            background: rgba(0,0,0,0.7);
            border: 2px solid #00ff00;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
        }
        input[type='file'] { 
            background: #333; 
            color: #00ff00; 
            border: 2px solid #00ff00; 
            border-radius: 5px;
            padding: 15px; 
            width: 100%;
            font-family: 'Consolas', monospace;
            font-size: 14px;
            margin-bottom: 15px;
        }
        .button-group {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
        }
        button { 
            background: linear-gradient(135deg, #003300, #006600);
            color: #00ff00; 
            border: 2px solid #00ff00; 
            border-radius: 5px;
            padding: 12px 20px;
            font-family: 'Consolas', monospace;
            font-size: 14px;
            cursor: pointer;
            transition: all 0.3s;
        }
        button:hover {
            background: linear-gradient(135deg, #006600, #009900);
            box-shadow: 0 0 15px #00ff0050;
        }
        #log { 
            background: rgba(0,0,0,0.9);
            border: 2px solid #00ff00; 
            border-radius: 10px;
            padding: 20px; 
            height: 400px; 
            overflow-y: auto;
            white-space: pre-wrap;
            font-family: 'Consolas', monospace;
            font-size: 13px;
            line-height: 1.4;
        }
        .log-entry {
            margin: 2px 0;
            border-left: 3px solid transparent;
            padding-left: 10px;
        }
        .success { color: #00ff00; border-left-color: #00ff00; }
        .warning { color: #ffff00; border-left-color: #ffff00; }
        .error { color: #ff4444; border-left-color: #ff4444; }
        .info { color: #00ffff; border-left-color: #00ffff; }
        .timestamp { color: #888; font-size: 11px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>🦖 RAWR BYTE ENGINE</h1>
        <div class="subtitle">Advanced File Analysis & Compression Tool v2.1</div>
    </div>

    <div class="controls">
        <input type='file' id='fileInput' multiple accept='*/*' />
        <div class="button-group">
            <button onclick='clearLog()'>🗑️ Clear Log</button>
            <button onclick='exportResults()'>💾 Export Results</button>
            <button onclick='showHelp()'>❓ Help</button>
        </div>
    </div>
    
    <div id='log' class='log'></div>

    <script>
        let analysisResults = [];
        
        const log = (msg, type = 'info') => {
            const logEl = document.getElementById('log');
            const timestamp = new Date().toLocaleTimeString();
            const entry = document.createElement('div');
            entry.className = 'log-entry ' + type;
            entry.innerHTML = '<span class="timestamp">[' + timestamp + ']</span> ' + msg;
            logEl.appendChild(entry);
            logEl.scrollTop = logEl.scrollHeight;
        };

        const clearLog = () => {
            document.getElementById('log').innerHTML = '';
            analysisResults = [];
            log('Log cleared - ready for new analysis', 'info');
        };

        const exportResults = () => {
            if (analysisResults.length === 0) {
                log('No results to export', 'warning');
                return;
            }
            
            const data = JSON.stringify(analysisResults, null, 2);
            const blob = new Blob([data], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'rawr_analysis_' + new Date().toISOString().slice(0,19).replace(/:/g,'-') + '.json';
            a.click();
            URL.revokeObjectURL(url);
            log('Results exported to download folder', 'success');
        };

        const showHelp = () => {
            log('=== RAWR BYTE ENGINE HELP ===', 'info');
            log('• Select files using the file input above', 'info');
            log('• Supported: All file types up to 100MB', 'info');
            log('• Analysis includes: entropy, compression potential, byte patterns', 'info');
            log('• Use Export to save results as JSON', 'info');
            log('• Clear Log removes all entries', 'info');
        };

        function analyzeFile(file) {
            if (file.size > 100 * 1024 * 1024) {
                log('⚠️ File too large: ' + file.name + ' (max 100MB)', 'warning');
                return;
            }
            
            log('📁 Analyzing: ' + file.name + ' (' + formatBytes(file.size) + ')', 'info');
            
            const reader = new FileReader();
            reader.onload = function(e) {
                try {
                    const arrayBuffer = e.target.result;
                    const uint8Array = new Uint8Array(arrayBuffer);
                    
                    // Comprehensive analysis
                    const analysis = performAdvancedAnalysis(uint8Array, file.name);
                    
                    // Log results
                    log('✅ Analysis complete for: ' + file.name, 'success');
                    log('   📊 File size: ' + formatBytes(analysis.size), 'info');
                    log('   🔢 Entropy: ' + analysis.entropy.toFixed(3) + ' bits/byte', 'info');
                    log('   📈 Compression potential: ' + analysis.compressionPotential.toFixed(1) + '%', 'info');
                    log('   🎯 File type: ' + analysis.detectedType, 'info');
                    log('   ⚡ Null bytes: ' + analysis.nullBytes + ' (' + analysis.nullPercentage.toFixed(1) + '%)', 'info');
                    
                    if (analysis.patterns.length > 0) {
                        log('   🔍 Patterns found: ' + analysis.patterns.join(', '), 'info');
                    }
                    
                    // Store results
                    analysisResults.push(analysis);
                    
                } catch (error) {
                    log('❌ Analysis failed for ' + file.name + ': ' + error.message, 'error');
                }
            };
            
            reader.onerror = () => {
                log('❌ Failed to read file: ' + file.name, 'error');
            };
            
            reader.readAsArrayBuffer(file);
        }

        function performAdvancedAnalysis(data, filename) {
            const size = data.length;
            const freq = new Array(256).fill(0);
            let nullBytes = 0;
            let patterns = [];
            
            // Frequency analysis
            for (let i = 0; i < size; i++) {
                freq[data[i]]++;
                if (data[i] === 0) nullBytes++;
            }
            
            // Calculate entropy
            let entropy = 0;
            for (let i = 0; i < 256; i++) {
                if (freq[i] > 0) {
                    const p = freq[i] / size;
                    entropy -= p * Math.log2(p);
                }
            }
            
            // Detect file type by magic bytes
            let detectedType = 'Unknown';
            if (size >= 4) {
                const magic = Array.from(data.slice(0, 4)).map(b => b.toString(16).padStart(2, '0')).join('').toLowerCase();
                const types = {
                    '89504e47': 'PNG Image',
                    'ffd8ffe0': 'JPEG Image', 
                    '25504446': 'PDF Document',
                    '504b0304': 'ZIP Archive',
                    '52617221': 'RAR Archive',
                    '7f454c46': 'ELF Executable',
                    '4d5a9000': 'Windows Executable'
                };
                detectedType = types[magic] || 'Unknown (' + magic + ')';
            }
            
            // Pattern detection
            if (nullBytes > size * 0.3) patterns.push('High null content');
            if (freq[255] > size * 0.1) patterns.push('High 0xFF content');
            
            // Repetition detection
            let maxRun = 0, currentRun = 1, lastByte = data[0];
            for (let i = 1; i < Math.min(size, 10000); i++) {
                if (data[i] === lastByte) {
                    currentRun++;
                } else {
                    maxRun = Math.max(maxRun, currentRun);
                    currentRun = 1;
                    lastByte = data[i];
                }
            }
            if (maxRun > 100) patterns.push('Long repetitive sequences');
            
            return {
                filename,
                size,
                entropy,
                compressionPotential: Math.max(0, (8 - entropy) / 8 * 100),
                nullBytes,
                nullPercentage: nullBytes / size * 100,
                detectedType,
                patterns,
                timestamp: new Date().toISOString()
            };
        }

        function formatBytes(bytes) {
            if (bytes === 0) return '0 B';
            const k = 1024;
            const sizes = ['B', 'KB', 'MB', 'GB'];
            const i = Math.floor(Math.log(bytes) / Math.log(k));
            return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
        }

        // File input handler
        document.getElementById('fileInput').onchange = function(e) {
            const files = e.target.files;
            if (files.length === 0) return;
            
            log('=== STARTING ANALYSIS BATCH ===', 'success');
            log('Processing ' + files.length + ' file(s)...', 'info');
            
            // Process files with slight delays to prevent UI blocking
            for (let i = 0; i < files.length; i++) {
                setTimeout(() => analyzeFile(files[i]), i * 50);
            }
            
            setTimeout(() => {
                log('=== BATCH ANALYSIS COMPLETE ===', 'success');
            }, files.length * 50 + 1000);
        };

        // Welcome message
        log('🦖 RAWR BYTE ENGINE v2.1 INITIALIZED', 'success');
        log('WebView2 mode active - full functionality available', 'info');
        log('Ready for file analysis - select files above to begin', 'info');
        showHelp();
    </script>
</body>
</html>
"@
      
      # Show the form
      Write-Host "Displaying WebView2 browser window..." -ForegroundColor Green
      $form.ShowDialog() | Out-Null
    }
  }
  catch {
    Write-Host "✗ WebView2 failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Stack: $($_.ScriptStackTrace)" -ForegroundColor Gray
    $useWebView2 = $false
  }
}

# Fallback to IE WebBrowser if WebView2 fails
if (!$useWebView2) {
  Write-Host "Using IE WebBrowser fallback..." -ForegroundColor Yellow
  
  try {
    # Clear previous controls
    $form.Controls.Clear()
    
    # Add status label back
    $statusLabel.Text = "IE WebBrowser Mode - Limited Functionality"
    $form.Controls.Add($statusLabel)
    
    # Create IE WebBrowser control
    $browser = New-Object System.Windows.Forms.WebBrowser
    $browser.Dock = [System.Windows.Forms.DockStyle]::Fill
    $form.Controls.Add($browser)
    
    Write-Host "✓ IE WebBrowser control created" -ForegroundColor Green
    
    # Simple fallback HTML
    $browser.DocumentText = @"
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <title>Rawr Browser - IE Fallback</title>
</head>
<body style='font-family:Consolas,monospace; background:#111; color:#0f0; margin:20px; line-height:1.6;'>
    <h2 style='color:#ff0; text-align:center;'>🦖 Rawr Browser - IE Fallback Mode</h2>
    <div style='background:#333; border:2px solid #0f0; padding:20px; border-radius:10px; margin:20px 0;'>
        <p style='color:#ff0; font-weight:bold;'>⚠️ LIMITED FUNCTIONALITY MODE</p>
        <p>WebView2 is not available on this system.</p>
        <p>Using Internet Explorer engine with reduced features.</p>
    </div>
    
    <div style='background:#222; border:1px solid #0f0; padding:15px; border-radius:5px;'>
        <h3 style='color:#0ff;'>Basic File Info:</h3>
        <input type='file' multiple style='background:#333; color:#0f0; border:1px solid #0f0; padding:10px; width:100%;' 
               onchange='showFileInfo(this.files)' />
        <pre id='info' style='background:#000; border:1px solid #0f0; padding:10px; height:200px; overflow:auto; margin-top:10px;'>
Select files to see basic information...
        </pre>
    </div>

    <script>
        function showFileInfo(files) {
            var info = document.getElementById('info');
            if (files.length === 0) {
                info.textContent = 'No files selected.';
                return;
            }
            
            var output = 'Selected ' + files.length + ' file(s):\n\n';
            for (var i = 0; i < files.length; i++) {
                var f = files[i];
                output += '📁 ' + f.name + '\n';
                output += '   Size: ' + f.size + ' bytes\n';
                output += '   Type: ' + (f.type || 'unknown') + '\n';
                output += '   Modified: ' + f.lastModifiedDate + '\n\n';
            }
            output += 'Note: Advanced analysis requires WebView2.\n';
            info.textContent = output;
        }
    </script>
</body>
</html>
"@
    
    Write-Host "✓ Fallback interface loaded" -ForegroundColor Green
    Write-Host "Displaying fallback browser window..." -ForegroundColor Green
    $form.ShowDialog() | Out-Null
  }
  catch {
    Write-Host "✗ Fallback browser failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Cannot create any browser control!" -ForegroundColor Red
  }
}

Write-Host "=== Rawr Browser session ended ===" -ForegroundColor Cyan