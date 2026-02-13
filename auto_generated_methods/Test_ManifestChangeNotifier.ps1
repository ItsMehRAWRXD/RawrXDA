# Test_ManifestChangeNotifier.ps1
# Starts a local HTTP listener and exercises Invoke-ManifestChangeNotifierAuto

$port = 5001
$listener = New-Object System.Net.HttpListener
$prefix = "http://localhost:$port/"
$listener.Prefixes.Add($prefix)
$listener.Start()
Write-Host "Test listener started on $prefix"

$received = $false
$job = Start-Job -ScriptBlock {
    param($listener)
    while ($listener.IsListening) {
        $ctx = $listener.GetContext()
        try {
            $req = $ctx.Request
            $reader = New-Object IO.StreamReader($req.InputStream)
            $body = $reader.ReadToEnd()
            $reader.Close()
            $resp = $ctx.Response
            $buffer = [Text.Encoding]::UTF8.GetBytes('OK')
            $resp.ContentLength64 = $buffer.Length
            $resp.OutputStream.Write($buffer,0,$buffer.Length)
            $resp.Close()
            Set-Content -Path "$env:TEMP\manifest_notifier_test_payload.json" -Value $body
            return
        } catch {
            # swallow
        }
    }
} -ArgumentList $listener

# Create temp manifest file
$manifest = Join-Path $env:TEMP 'test_manifest.json'
@{ test = 'initial' } | ConvertTo-Json | Set-Content -Path $manifest

# Start notifier in a job
$notifierJob = Start-Job -ScriptBlock {
    param($manifestPath,$endpoint)
    . 'D:/lazy init ide/auto_generated_methods/ManifestChangeNotifier_AutoFeature.ps1'
    Invoke-ManifestChangeNotifierAuto -ManifestPath $manifestPath -NotificationEndpoints @($endpoint) -MaxRetries 2 -InitialRetryDelaySeconds 1 -DebounceMilliseconds 200
} -ArgumentList $manifest, "http://localhost:$port/"

Start-Sleep -Seconds 1

# Touch manifest to trigger change
$updatedContent = @{ updated = (Get-Date -Format o) } | ConvertTo-Json
Set-Content -Path $manifest -Value $updatedContent

# Wait for payload file
for ($i=0;$i -lt 30 -and -not (Test-Path "$env:TEMP\manifest_notifier_test_payload.json");$i++) { Start-Sleep -Seconds 1 }

if (Test-Path "$env:TEMP\manifest_notifier_test_payload.json") {
    Write-Host "Notification received - test passed"
    Get-Content "$env:TEMP\manifest_notifier_test_payload.json" | Write-Host
} else {
    Write-Host "Notification not received - test failed"
}

# Cleanup
try { Stop-Job -Job $notifierJob -Force } catch {}
try { Stop-Job -Job $job -Force } catch {}
try { $listener.Stop() } catch {}
Remove-Item "$env:TEMP\manifest_notifier_test_payload.json" -ErrorAction SilentlyContinue
Remove-Item $manifest -ErrorAction SilentlyContinue
