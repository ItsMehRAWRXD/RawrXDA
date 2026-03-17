Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing



# -------------------------------------------------------------
# GUI window
# -------------------------------------------------------------
$form = New-Object System.Windows.Forms.Form
$form.Text = "RawrCompress – GUI File Compressor"
$form.Width = 1000
$form.Height = 700

$browser = New-Object System.Windows.Forms.WebBrowser
$browser.Dock = "Fill"
$form.Controls.Add($browser)

$browser.ObjectForScripting = New-Object PSObject
$browser.ObjectForScripting | Add-Member -MemberType ScriptMethod -Name save -Value {
  $filename = $args[0]
  $bytes = $args[1]
  $path = Join-Path (Get-Location) $filename
  [IO.File]::WriteAllBytes($path, $bytes)
  return "saved"
}.\652-compressor.ps1 -Action compress -Folder "c:\Users\HiH8e\OneDrive\Desktop"
$bytes = [System.Convert]::FromBase64String($base64)
$encoding = [System.Text.Encoding]::UTF8
$js = $encoding.GetString($bytes)

$html = @"
    <!DOCTYPE html>
    <html>
    <head>
    <meta charset='utf-8'>
    <title>RawrCompress GUI</title>
    <style>
    body { font-family: Consolas; background:#111; color:#0f0; margin:0; }
    #drop { border:2px dashed #0f0; margin:20px; padding:40px; text-align:center; }
    #log { white-space: pre; margin:20px; }
    #progress { margin:20px; width:90%; }
    </style>
    </head>
    <body>

    <div id='drop'>Drag files here to compress</div>
    <progress id='progress' value='0' max='100'></progress>
    <pre id='log'></pre>

    </body>
    </html>
"@

# -------------------------------------------------------------
# Host <-> JS bridge
# -------------------------------------------------------------
$browser.DocumentCompleted.Add({
    $doc = $browser.Document
    $script = $doc.CreateElement("script")
    $script.Text = $js
    $doc.Body.AppendChild($script)
    <!DOCTYPE html>
    <html>
    <head>
    <meta charset='utf-8'>
    <title>RawrCompress GUI</title>
    <style>
    body { font-family: Consolas; background:#111; color:#0f0; margin:0; }
    #drop { border:2px dashed #0f0; margin:20px; padding:40px; text-align:center; }
    #log { white-space: pre; margin:20px; }
    #progress { margin:20px; width:90%; }
    </style>
    </head>
    <body>

    <div id='drop'>Drag files here to compress</div>
    <progress id='progress' value='0' max='100'></progress>
    <pre id='log'></pre>

    <script>
    const log = m => logBox.textContent += m + "\n";
    const logBox = document.getElementById("log");
    const progress = document.getElementById("progress");
    const drop = document.getElementById("drop");

    drop.addEventListener("dragover", e => { e.preventDefault(); drop.style.borderColor="#0f0"; });
    drop.addEventListener("dragleave", e => drop.style.borderColor="#0f0");
    drop.addEventListener("drop", async e => {
        e.preventDefault();
        drop.style.borderColor="#0f0";
        const file = e.dataTransfer.files[0];
        compressFile(file);
      });

    async function compressFile(f) {
      log("Loaded: " + f.name + " (" + f.size + " bytes)");
      const buf = await f.arrayBuffer();
      const bytes = new Uint8Array(buf);

      // simple JS compression (LZ-like)
      log("Compressing...");
      progress.value = 0;

      const compressed = [];
      let last = bytes[0];
      let count = 1;

      for (let i = 1; i < bytes.length; i++) {
        progress.value = (i / bytes.length) * 100;
        if (bytes[i] === last && count < 255) {
          count++;
        }
        else {
          compressed.push(count, last);
          last = bytes[i];
          count = 1;
        }
      }
      compressed.push(count, last);

      const outBytes = Array.from(compressed);
      log("Original: " + bytes.length + " bytes");
      log("Compressed: " + outBytes.length + " bytes");

      const outName = f.name + ".rawr";
      await rawrHost.save(outName, outBytes);
      log("Saved: " + outName);
      progress.value = 100;
    }
    </script>

    </body>
    </html>
    "@)
  })

$browser.DocumentText = @"
    <!DOCTYPE html>
    <html>
    <head>
    <meta charset='utf-8'>
    <title>RawrCompress GUI</title>
    <style>
    body { font-family: Consolas; background:#111; color:#0f0; margin:0; }
    #drop { border:2px dashed #0f0; margin:20px; padding:40px; text-align:center; }
    #log { white-space: pre; margin:20px; }
    #progress { margin:20px; width:90%; }
    </style>
    </head>
    <body>

    <div id='drop'>Drag files here to compress</div>
    <progress id='progress' value='0' max='100'></progress>
    <pre id='log'></pre>

    </body>
    </html>
    "@
$form.ShowDialog()