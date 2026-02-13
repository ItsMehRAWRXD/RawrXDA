# Monaco Editor Integration for RawrXD.ps1
# Replaces RichTextBox with Monaco Editor via WebView2

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Check if WebView2 Runtime is installed
function Test-WebView2Runtime {
    try {
        $webView2Path = "$env:ProgramFiles (x86)\Microsoft\EdgeWebView\Application"
        return (Test-Path $webView2Path)
    }
    catch {
        return $false
    }
}

# Create Monaco Editor HTML
$monacoHtml = @'
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <style>
        body { margin: 0; padding: 0; overflow: hidden; }
        #container { width: 100%; height: 100vh; }
    </style>
</head>
<body>
    <div id="container"></div>

    <!-- Load Monaco from CDN -->
    <script src="https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.45.0/min/vs/loader.min.js"></script>
    <script>
        require.config({ paths: { vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.45.0/min/vs' }});

        let editor;

        require(['vs/editor/editor.main'], function() {
            editor = monaco.editor.create(document.getElementById('container'), {
                value: '',
                language: 'powershell',
                theme: 'vs-dark',
                fontSize: 14,
                automaticLayout: true,
                minimap: { enabled: true },
                scrollBeyondLastLine: false,
                wordWrap: 'on',
                lineNumbers: 'on',
                folding: true,
                renderWhitespace: 'selection',
                cursorStyle: 'line',
                fontFamily: 'Consolas, "Courier New", monospace'
            });

            // Expose editor to external JavaScript
            window.monacoEditor = editor;

            // Notify PowerShell that editor is ready
            window.chrome.webview.postMessage({ type: 'ready' });

            // Listen for content changes
            editor.onDidChangeModelContent(() => {
                window.chrome.webview.postMessage({
                    type: 'textChanged',
                    content: editor.getValue()
                });
            });
        });

        // Functions exposed to PowerShell
        function getText() {
            return editor ? editor.getValue() : '';
        }

        function setText(text) {
            if (editor) {
                editor.setValue(text || '');
            }
        }

        function setLanguage(lang) {
            if (editor) {
                monaco.editor.setModelLanguage(editor.getModel(), lang);
            }
        }

        function setTheme(theme) {
            if (editor) {
                monaco.editor.setTheme(theme);
            }
        }

        function insertText(text) {
            if (editor) {
                const selection = editor.getSelection();
                editor.executeEdits('', [{
                    range: selection,
                    text: text
                }]);
            }
        }

        function getSelectedText() {
            if (editor) {
                return editor.getModel().getValueInRange(editor.getSelection());
            }
            return '';
        }
    </script>
</body>
</html>
'@

Write-Host "Monaco Editor Integration Script" -ForegroundColor Cyan
Write-Host "=================================" -ForegroundColor Cyan
Write-Host ""

if (Test-WebView2Runtime) {
    Write-Host "✓ WebView2 Runtime is installed" -ForegroundColor Green
    Write-Host ""
    Write-Host "To integrate Monaco Editor into RawrXD.ps1:" -ForegroundColor Yellow
    Write-Host "1. Replace the RichTextBox editor creation with WebView2" -ForegroundColor White
    Write-Host "2. Load the Monaco HTML into the WebView2 control" -ForegroundColor White
    Write-Host "3. Use JavaScript interop to get/set editor content" -ForegroundColor White
    Write-Host ""
    Write-Host "Benefits:" -ForegroundColor Green
    Write-Host "  ✓ Professional code editor (same as VS Code)" -ForegroundColor White
    Write-Host "  ✓ Syntax highlighting that actually works" -ForegroundColor White
    Write-Host "  ✓ IntelliSense and autocomplete" -ForegroundColor White
    Write-Host "  ✓ No rendering issues" -ForegroundColor White
    Write-Host "  ✓ Built-in minimap and folding" -ForegroundColor White
    Write-Host "  ✓ Multi-cursor editing" -ForegroundColor White
    Write-Host ""

    # Save Monaco HTML to file
    $htmlPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\monaco-editor.html"
    $monacoHtml | Out-File $htmlPath -Encoding UTF8 -Force
    Write-Host "✓ Saved Monaco Editor HTML to: $htmlPath" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next Steps:" -ForegroundColor Cyan
    Write-Host "1. Install WebView2 NuGet package:" -ForegroundColor White
    Write-Host "   Install-Package Microsoft.Web.WebView2" -ForegroundColor Gray
    Write-Host ""
    Write-Host "2. Replace editor creation in RawrXD.ps1 with:" -ForegroundColor White
    Write-Host @'
   Add-Type -Path "path\to\Microsoft.Web.WebView2.WinForms.dll"
   $script:editor = New-Object Microsoft.Web.WebView2.WinForms.WebView2
   $script:editor.Source = [System.Uri]::new("file:///$htmlPath")
'@ -ForegroundColor Gray
}
else {
    Write-Host "✗ WebView2 Runtime not found" -ForegroundColor Red
    Write-Host ""
    Write-Host "Download from: https://go.microsoft.com/fwlink/p/?LinkId=2124703" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Alternative: Use browser-based IDE (IDEre2.html)" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "Would you like to:" -ForegroundColor Cyan
Write-Host "A) Install WebView2 and integrate Monaco into RawrXD.ps1" -ForegroundColor White
Write-Host "B) Use the browser-based IDE (D:\HTML-Projects\IDEre2.html)" -ForegroundColor White
Write-Host "C) Create a standalone Monaco-based IDE in HTML" -ForegroundColor White
