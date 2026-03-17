#Requires -Version 5.1
param(
    [string]$GGUFPath = 'D:\OllamaModels\BigDaddyG-NO-REFUSE-Q4_K_M.gguf',
    [string]$Language = 'powershell'
)

# ---- Bootstrap WebView2 Dependencies ----------------------------------
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir) { $ScriptDir = $PWD.Path }
$LibsDir = Join-Path $ScriptDir "WebView2Libs"

function Bootstrap-WebView2 {
    if (-not (Test-Path $LibsDir)) {
        Write-Host "Downloading WebView2 dependencies (one-time)..." -ForegroundColor Cyan
        $nugetUrl = "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2929.43"
        $zipPath = Join-Path $ScriptDir "webview2.zip"
        
        try {
            Invoke-WebRequest -Uri $nugetUrl -OutFile $zipPath -ErrorAction Stop
            
            $tempDir = Join-Path $ScriptDir "webview2_temp"
            Expand-Archive -Path $zipPath -DestinationPath $tempDir -Force
            
            New-Item -ItemType Directory -Path $LibsDir -Force | Out-Null
            
            # Copy Managed DLLs (NetCoreApp3.0 for PS7)
            $coreSource = Join-Path $tempDir "lib\netcoreapp3.0"
            if (-not (Test-Path $coreSource)) {
                 # Fallback to net45 if core not found (older packages) but 1.0.2929 has it
                 $coreSource = Join-Path $tempDir "lib\net45"
            }
            Copy-Item (Join-Path $coreSource "Microsoft.Web.WebView2.Core.dll") $LibsDir
            Copy-Item (Join-Path $coreSource "Microsoft.Web.WebView2.WinForms.dll") $LibsDir
            
            # Copy Native Loader (win-x64)
            $nativeSource = Join-Path $tempDir "runtimes\win-x64\native\WebView2Loader.dll"
            Copy-Item $nativeSource $LibsDir
            
        }
        catch {
            Write-Error "Failed to download dependencies: $_"
            return
        }
        finally {
            if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
            if (Test-Path $tempDir) { Remove-Item $tempDir -Recurse -Force }
        }
    }
}

Bootstrap-WebView2

# Load Assemblies
try {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -Path (Join-Path $LibsDir "Microsoft.Web.WebView2.Core.dll")
    Add-Type -Path (Join-Path $LibsDir "Microsoft.Web.WebView2.WinForms.dll")
}
catch {
    Write-Error "Failed to load WebView2 assemblies. Ensure they are in '$LibsDir'."
    exit
}

# Ensure Native Loader is found
$env:Path += ";$LibsDir"

# ---- helpers ----------------------------------------------------------
function Invoke-Copilot {
    param($Prompt, $MaxTokens = 150)
    $response = Get-OpenAICompletion -Prompt $Prompt -MaxTokens $MaxTokens
    return $response.Trim()
}

# ---- Get Key ----------------------------------------------------------
try {
    $ApiKey = [System.Runtime.InteropServices.Marshal]::PtrToStringAuto(
        [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR((Get-OpenAIKey))
    )
} catch {
    $ApiKey = ""
    Write-Warning "Copilot Key not found. Inline completions will not work until you run Set-OpenAIKey."
}

# ---- Monaco + Copilot HTML string ------------------------------------
$monacoHTML = @"
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <title>Monaco + Copilot + GGUF</title>
  <style>html,body{margin:0;padding:0;height:100%}#editor{height:100%}</style>
</head>
<body>
<div id='editor'></div>
<script src='https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs/loader.js'></script>
<script>
const ggufPath = '$($GGUFPath -replace "\\", "\\\\")';
const copilotKey = '$ApiKey';

require.config({ paths: { vs: 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs' }});
require(['vs/editor/editor.main'], function () {
  const editor = monaco.editor.create(document.getElementById('editor'), {
    value: `# GGUF model embedded: \${ggufPath}\r\n# Start typing – Copilot completions appear inline!`,
    language: '$Language',
    theme: 'vs-dark',
    fontSize: 14,
    automaticLayout: true,
    minimap: { enabled: true },
    suggest: { showInlineDetails: true, showIcons: true },
    inlineSuggest: { enabled: true }
  });

  monaco.languages.registerInlineCompletionsProvider('$Language', {
    provideInlineCompletions: async function (model, position, context, token) {
      const text = model.getValue();
      const prompt = text.substring(0, model.getOffsetAt(position));
      const suffix = text.substring(model.getOffsetAt(position));
      const req = { prompt, suffix, max_tokens: 80, temperature: 0.2 };
      
      if (!copilotKey) return { items: [] };

      try {
          const resp = await fetch('https://api.githubcopilot.com/v1/engines/copilot-codex/completions', {
            method: 'POST',
            headers: {
              'Authorization': 'Bearer ' + copilotKey,
              'Content-Type': 'application/json'
            },
            body: JSON.stringify(req)
          }).then(r => r.json());
          
          if (resp.choices && resp.choices.length) {
            return { items: [{ insertText: resp.choices[0].text }] };
          }
      } catch (e) { console.error(e); }
      return { items: [] };
    },
    freeInlineCompletions: () => {}
  });

  window.getValue = () => editor.getValue();
  window.setValue = (t, l) => { editor.setValue(t); if (l) monaco.editor.setModelLanguage(editor.getModel(), l); };
});
</script>
</body>
</html>
"@

# ---- WebView2 host ----------------------------------------------------
$form          = New-Object System.Windows.Forms.Form
$form.Text     = "Monaco + Copilot + GGUF (Pure PS)"
$form.Width    = 1400
$form.Height   = 900
$form.StartPosition = 'CenterScreen'

try {
    $wv            = New-Object Microsoft.Web.WebView2.WinForms.WebView2
    $wv.Dock       = [System.Windows.Forms.DockStyle]::Fill
    $form.Controls.Add($wv)

    $wv.CoreWebView2InitializationCompleted += {
        $wv.CoreWebView2.NavigateToString($monacoHTML)
    }

    $wv.EnsureCoreWebView2Async($null) | Out-Null
}
catch {
    Write-Error "Failed to initialize WebView2. Make sure the runtime is installed and DLLs are loaded."
    Write-Error $_
}

# ---- public bridge ----------------------------------------------------
function Get-EditorContent {
    if ($wv.CoreWebView2) {
        return $wv.CoreWebView2.ExecuteScriptAsync("window.getValue()").GetAwaiter().GetResult().Trim('"').Replace('\"','"')
    }
}
function Set-EditorContent {
    param($Text,$Lang)
    $Text = $Text -replace '"','\"' -replace '`','``'
    $wv.CoreWebView2.ExecuteScriptAsync("window.setValue(`"$Text`",'$Lang')").GetAwaiter().GetResult() | Out-Null
}

# ---- show ----------------------------------------------------------------
[void]$form.ShowDialog()
