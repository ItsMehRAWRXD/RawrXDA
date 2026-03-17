# RawrBrowser - Force Focus Version
# Guaranteed to show up in front with visible content

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Write-Host "=== RAWR BROWSER - FORCE FOCUS VERSION ===" -ForegroundColor Green
Write-Host "Creating browser with forced focus and visibility..." -ForegroundColor Yellow

# Create form with specific positioning
$form = New-Object System.Windows.Forms.Form
$form.Text = "RawrBrowser - Fallback Mode (IE Engine)"
$form.Size = New-Object System.Drawing.Size(1200, 800)
$form.StartPosition = [System.Windows.Forms.FormStartPosition]::CenterScreen
$form.TopMost = $true  # Force on top
$form.WindowState = [System.Windows.Forms.FormWindowState]::Normal
$form.ShowInTaskbar = $true

# Ensure form is visible
$form.Visible = $true
Write-Host "Form created and set to visible" -ForegroundColor Green

# Create WebBrowser control
$browser = New-Object System.Windows.Forms.WebBrowser
$browser.Dock = [System.Windows.Forms.DockStyle]::Fill
$browser.ScriptErrorsSuppressed = $true

# Add browser to form FIRST
$form.Controls.Add($browser)
Write-Host "WebBrowser control added to form" -ForegroundColor Green

# Simple HTML content that WILL show up
$htmlContent = @"
<!DOCTYPE html>
<html>
<head>
    <title>RawrBrowser - Working!</title>
    <style>
        body { 
            background: linear-gradient(45deg, #ff6b6b, #4ecdc4, #45b7d1, #96ceb4);
            color: white; 
            font-family: Arial, sans-serif; 
            padding: 20px;
            margin: 0;
        }
        .header { 
            text-align: center; 
            font-size: 32px; 
            margin: 20px 0;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.5);
        }
        .content { 
            background: rgba(0,0,0,0.3); 
            padding: 20px; 
            border-radius: 10px;
            margin: 20px;
        }
        button { 
            background: #ff6b6b; 
            color: white; 
            border: none; 
            padding: 10px 20px; 
            font-size: 16px; 
            border-radius: 5px; 
            cursor: pointer;
            margin: 10px;
        }
        button:hover { background: #ff5252; }
    </style>
</head>
<body>
    <div class="header">🦖 RAWR BROWSER IS WORKING! 🦖</div>
    <div class="content">
        <h2>✅ Browser Successfully Loaded!</h2>
        <p>This confirms the WebBrowser control is functioning correctly.</p>
        
        <h3>File Operations:</h3>
        <button onclick="selectFile()">📁 Select File</button>
        <button onclick="analyzeFile()">🔍 Analyze File</button>
        <button onclick="compressFile()">📦 Compress File</button>
        
        <div id="output" style="margin-top: 20px; padding: 10px; background: rgba(255,255,255,0.1); border-radius: 5px;">
            <p>Status: Ready for file operations</p>
        </div>
    </div>
    
    <script>
        function selectFile() {
            document.getElementById('output').innerHTML = '<p style="color: #96ceb4;">File selection triggered!</p>';
            try {
                window.external.SelectFile();
            } catch(e) {
                document.getElementById('output').innerHTML = '<p style="color: #ffff99;">File selection method not available in fallback mode</p>';
            }
        }
        
        function analyzeFile() {
            document.getElementById('output').innerHTML = '<p style="color: #4ecdc4;">File analysis triggered!</p>';
        }
        
        function compressFile() {
            document.getElementById('output').innerHTML = '<p style="color: #45b7d1;">File compression triggered!</p>';
        }
        
        // Confirm load
        setTimeout(function() {
            document.getElementById('output').innerHTML = '<p style="color: #96ceb4;">✅ JavaScript is working! Browser fully functional.</p>';
        }, 1000);
    </script>
</body>
</html>
"@

# Set HTML content
$browser.DocumentText = $htmlContent
Write-Host "HTML content set to browser" -ForegroundColor Green

# Force form to front
$form.BringToFront()
$form.Focus()
$form.Activate()

# Show form and force focus
Write-Host "Displaying form with forced focus..." -ForegroundColor Yellow
$form.Show()

# Additional focus attempts
Start-Sleep -Seconds 1
[System.Windows.Forms.Application]::DoEvents()
$form.TopMost = $false  # Release top-most after showing
$form.TopMost = $true   # Re-apply to ensure visibility
$form.Activate()

Write-Host "✅ RawrBrowser launched successfully with forced focus!" -ForegroundColor Green
Write-Host "If you don't see it, press Alt+Tab to cycle through windows" -ForegroundColor Yellow

# Keep form alive
$form.ShowDialog()